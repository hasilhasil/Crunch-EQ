#include "EqualizerDSP.h"

int EqualizerDSP::fftSizeForQuality (int qualityIndex)
{
    static const int sizes[] = { 2048, 4096, 8192 };
    return sizes[juce::jlimit (0, 2, qualityIndex)];
}

void EqualizerDSP::prepare (juce::dsp::ProcessSpec spec)
{
    sampleRate_  = (float) spec.sampleRate;
    numChannels_ = (int) spec.numChannels;

    for (auto& band : bands_)
        band.prepare (sampleRate_);

    linearPhase_.prepare (spec.sampleRate, numChannels_, fftSizeForQuality (linearQuality_));
    linearPhaseDirty_ = true;
}

void EqualizerDSP::reset()
{
    for (auto& band : bands_)
        band.prepare (sampleRate_);
    linearPhase_.reset();
}

void EqualizerDSP::setSampleRate (float sampleRate)
{
    if (sampleRate == sampleRate_)
        return;
    sampleRate_ = sampleRate;
    for (auto& band : bands_)
        band.setSampleRate (sampleRate_);
    linearPhaseDirty_ = true;
}

void EqualizerDSP::updateBand (int index, Param::FilterType type, float freq, float gain, float q,
                               int slope, bool enabled)
{
    auto& band = bands_[(size_t) juce::jlimit (0, Param::kMaxBands - 1, index)];
    band.setType (type);
    band.setFrequency (freq);
    band.setGain (gain);
    band.setQ (q);
    band.setSlope (slope);
    band.setEnabled (enabled);
    band.setPhaseMode (phaseMode_);
    linearPhaseDirty_ = true;
}

void EqualizerDSP::setPhaseMode (Param::PhaseMode mode)
{
    if (mode == phaseMode_)
        return;

    phaseMode_ = mode;
    for (auto& band : bands_)
        band.setPhaseMode (mode);

    if (mode == Param::PhaseMode::LinearPhase)
    {
        // Flush audio still buffered from a previous linear-phase session so
        // re-entering the mode does not replay stale samples. This is the ONLY
        // place the FIFOs are cleared on a mode change - flushing on every IR
        // update instead is what made dragging a node stutter.
        linearPhase_.reset();
        linearPhaseDirty_ = true;
    }
}

void EqualizerDSP::setLinearQuality (int qualityIndex)
{
    qualityIndex = juce::jlimit (0, 2, qualityIndex);
    if (qualityIndex == linearQuality_)
        return;

    linearQuality_ = qualityIndex;
    linearPhase_.prepare (sampleRate_, numChannels_, fftSizeForQuality (linearQuality_));
    linearPhaseDirty_ = true;
}

float EqualizerDSP::magnitudeLinearAt (float freq) const
{
    float mag = 1.0f;
    for (const auto& band : bands_)
        mag *= band.magnitudeAt (freq, sampleRate_);
    return juce::jlimit (1.0e-4f, 1.0e4f, mag);
}

float EqualizerDSP::getMagnitudeDbAt (float freq) const
{
    return juce::Decibels::gainToDecibels (magnitudeLinearAt (freq));
}

int EqualizerDSP::getLatencySamples() const
{
    if (phaseMode_ != Param::PhaseMode::LinearPhase)
        return 0;

    const int fftSize  = fftSizeForQuality (linearQuality_);
    const int blockSize = fftSize / 2;
    const int irLength  = fftSize / 2 - 1;
    return blockSize + (irLength - 1) / 2;
}

void EqualizerDSP::rebuildLinearPhase()
{
    const int fftSize  = fftSizeForQuality (linearQuality_);
    const int irLength = fftSize / 2 - 1;   // odd length -> integer group delay

    // Reuse the FFT object across rebuilds: constructing one computes its
    // twiddle tables, which is far too expensive to redo per parameter tick.
    const int order = juce::roundToInt (std::log2 ((float) fftSize));
    if (rebuildFft_ == nullptr || rebuildFftOrder_ != order)
    {
        rebuildFft_ = std::make_unique<juce::dsp::FFT> (order);
        rebuildFftOrder_ = order;
    }

    // Scratch is retained across calls (capacity stabilises after the first
    // rebuild), so a drag performs no allocation on the audio thread.
    rebuildSpectrum_.assign ((size_t) fftSize, {});
    rebuildTime_.resize ((size_t) fftSize);
    rebuildIr_.resize ((size_t) irLength);

    for (int k = 0; k <= fftSize / 2; ++k)
    {
        const float f = (float) k * sampleRate_ / (float) fftSize;

        float mag = 1.0f;
        if (f > 1.0f && f < sampleRate_ * 0.49f)
            mag = magnitudeLinearAt (f);

        const double theta = juce::MathConstants<double>::pi * k * (irLength - 1) / fftSize;
        rebuildSpectrum_[(size_t) k] = std::complex<float> (
            mag * (float) std::cos (theta), -mag * (float) std::sin (theta));
    }

    for (int k = fftSize / 2 + 1; k < fftSize; ++k)
        rebuildSpectrum_[(size_t) k] = std::conj (rebuildSpectrum_[(size_t) (fftSize - k)]);

    rebuildFft_->perform (rebuildSpectrum_.data(), rebuildTime_.data(), true);

    for (int n = 0; n < irLength; ++n)
        rebuildIr_[(size_t) n] = rebuildTime_[(size_t) n].real();

    linearPhase_.setImpulseResponse (rebuildIr_.data(), irLength);
    linearPhaseDirty_ = false;
}

void EqualizerDSP::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (phaseMode_ == Param::PhaseMode::LinearPhase)
    {
        // A drag dirties the IR on every parameter tick; rebuilding that often
        // would stall the audio thread, so the impulse response is refreshed
        // at a bounded rate instead. A filter without an IR yet (fresh
        // prepare / quality switch - its spectrum is empty and would output
        // silence) always rebuilds immediately.
        if (linearPhaseDirty_
            && (rebuildCountdown_ <= 0 || ! linearPhase_.hasImpulseResponse()))
        {
            rebuildLinearPhase();
            rebuildCountdown_ = kRebuildIntervalSamples;
        }
        else
        {
            rebuildCountdown_ = juce::jmax (0, rebuildCountdown_ - numSamples);
        }

        linearPhase_.process (buffer.getArrayOfReadPointers(),
                              buffer.getArrayOfWritePointers(),
                              numChannels, numSamples);
        return;
    }

    float* left  = buffer.getWritePointer (0);
    float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int s = 0; s < numSamples; ++s)
    {
        float l = left[s];
        float r = right != nullptr ? right[s] : l;

        for (auto& band : bands_)
            band.processSample (l, r);

        left[s] = l;
        if (right != nullptr)
            right[s] = r;
    }

    // Safety net: with marginally-stable extreme coefficients (e.g. Q = 40
    // cascaded cuts) a runaway state is conceivable in principle. If the
    // output ever leaves the range of sane audio (or goes non-finite), reset
    // every band's filter state and zero the whole block, so the plugin
    // degrades to a brief dropout and recovers instead of emitting noise
    // forever. 1e12 (~240 dBFS) is far beyond anything a real signal or a
    // legitimate filter chain can produce, but catches the runaway long
    // before it overflows float. Costs one comparison scan per block.
    constexpr float kInsaneLevel = 1.0e12f;

    bool sane = true;
    for (int ch = 0; ch < numChannels && sane; ++ch)
    {
        const float* p = buffer.getReadPointer (ch);
        for (int s = 0; s < numSamples; ++s)
        {
            const float v = p[s];
            if (! (std::abs (v) < kInsaneLevel))   // also catches NaN
            {
                sane = false;
                break;
            }
        }
    }

    if (! sane)
    {
        buffer.clear();
        for (auto& band : bands_)
            band.resetFilterState();
    }
}
