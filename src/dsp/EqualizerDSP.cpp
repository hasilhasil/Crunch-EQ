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
        linearPhaseDirty_ = true;
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

    juce::dsp::FFT fft (juce::roundToInt (std::log2 ((float) fftSize)));
    std::vector<std::complex<float>> H ((size_t) fftSize, {});
    std::vector<std::complex<float>> h ((size_t) fftSize, {});

    for (int k = 0; k <= fftSize / 2; ++k)
    {
        const float f = (float) k * sampleRate_ / (float) fftSize;

        float mag = 1.0f;
        if (f > 1.0f && f < sampleRate_ * 0.49f)
            mag = magnitudeLinearAt (f);

        const double theta = juce::MathConstants<double>::pi * k * (irLength - 1) / fftSize;
        H[(size_t) k] = std::complex<float> (
            mag * (float) std::cos (theta), -mag * (float) std::sin (theta));
    }

    for (int k = fftSize / 2 + 1; k < fftSize; ++k)
        H[(size_t) k] = std::conj (H[(size_t) (fftSize - k)]);

    fft.perform (H.data(), h.data(), true);

    std::vector<float> ir ((size_t) irLength);
    for (int n = 0; n < irLength; ++n)
        ir[(size_t) n] = h[(size_t) n].real();

    linearPhase_.setImpulseResponse (ir.data(), irLength);
    linearPhaseDirty_ = false;
}

void EqualizerDSP::process (juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (phaseMode_ == Param::PhaseMode::LinearPhase)
    {
        if (linearPhaseDirty_)
            rebuildLinearPhase();

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
}
