#include "SpectrumAnalyzer.h"

void SpectrumAnalyzer::prepare (double sampleRate, int fftSize)
{
    sampleRate_ = sampleRate;
    fftSize_    = juce::nextPowerOfTwo (fftSize);

    const int order = juce::roundToInt (std::log2 ((float) fftSize_));
    fft_ = std::make_unique<juce::dsp::FFT> (order);

    window_.assign ((size_t) fftSize_, 0.0f);
    for (int n = 0; n < fftSize_; ++n)
        window_[(size_t) n] = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * n / (fftSize_ - 1)));

    fftBuf_.assign ((size_t) fftSize_, {});
    fftTmp_.assign ((size_t) fftSize_, {});

    binLevelsDb_.assign ((size_t) (fftSize_ / 2 + 1), -140.0f);
    prevLevelsDb_.assign ((size_t) (fftSize_ / 2 + 1), -140.0f);

    // Frame rate is expressed in TIME and matched to the display: one frame per
    // UI tick. The display / meters refresh ~60 times a second, so producing
    // more than that would only create bursts that get dropped (the UI picks
    // the newest slot), and producing fewer would leave ticks without new data.
    // Either way the rate must not depend on how much audio the host hands us
    // per block, which is why the engine drives this instead of the UI tick.
    hop_ = juce::jmax (1, (int) std::lround (sampleRate_ / 60.0));

    // force the plot lookup to be rebuilt for the new rate / FFT size
    lutPoints_ = -1;

    reset();
}

void SpectrumAnalyzer::reset()
{
    fifo_.clear();
    hasFrame_ = false;
    std::fill (binLevelsDb_.begin(), binLevelsDb_.end(), -140.0f);
    std::fill (prevLevelsDb_.begin(), prevLevelsDb_.end(), -140.0f);
    frameCounter_ = 0;
    lastFrameDeltaDb_ = 0.0f;
}

void SpectrumAnalyzer::setTilt (float dbPerOct)
{
    tilt_ = dbPerOct;
}

void SpectrumAnalyzer::pushSamples (const float* mono, int numSamples)
{
    if (mono == nullptr || numSamples <= 0 || fft_ == nullptr)
        return;

    // Buffer only - the engine decides when frames are actually produced.
    fifo_.insert (fifo_.end(), mono, mono + numSamples);

    // The analysed window is the oldest one in the buffer, so every extra
    // buffered sample is extra latency. The buffer must however be able to hold
    // a whole host block (FL Studio hands plugins 1024-4096 samples at a time):
    // if it cannot, the surplus is thrown away and the frame rate collapses to
    // the block rate. One block is the smallest cap that still yields one frame
    // per display tick.
    const int limit = fftSize_ + 4096;
    if ((int) fifo_.size() > limit)
        fifo_.erase (fifo_.begin(), fifo_.end() - limit);
}

bool SpectrumAnalyzer::processPendingFrame()
{
    // A full window is available: analyse it and advance the buffer.
    if (fft_ == nullptr || (int) fifo_.size() < fftSize_)
        return false;

    processFrame (fifo_.data());

    // One hop (one display tick of audio) per frame keeps the window in step
    // with real time. The surplus above the target is shed over a few frames so
    // the buffer converges to the target instead of sitting at the cap:
    //   * a host that delivers large blocks NEEDS the surplus - it is spread
    //     over the frames until the next block arrives; shedding it in one go
    //     would starve the analyser between blocks (a visibly lower frame rate)
    //   * a surplus that is not replenished (the backlog the ring hands over
    //     when the editor opens, a stalled producer) is pure latency
    // The target sits one hop above the window: enough headroom that the chunked
    // delivery of a host block never leaves the buffer short of a full window
    // (which would drop frames), while staying as small as possible - the window
    // is the OLDEST one in the buffer, so every extra buffered sample is extra
    // visible latency.
    const int target  = fftSize_ + hop_;
    const int surplus = (int) fifo_.size() - target;
    const int advance = hop_ + (surplus > 0 ? surplus / 4 : 0);

    fifo_.erase (fifo_.begin(), fifo_.begin() + juce::jmin (advance, (int) fifo_.size()));
    return true;
}

void SpectrumAnalyzer::processFrame (const float* data)
{
    for (int n = 0; n < fftSize_; ++n)
        fftBuf_[(size_t) n] = std::complex<float> (data[n] * window_[(size_t) n], 0.0f);

    fft_->perform (fftBuf_.data(), fftTmp_.data(), false);

    const float norm = 4.0f / (float) fftSize_;
    const float refFreq = 1000.0f;

    float maxDelta = 0.0f;

    for (int k = 0; k <= fftSize_ / 2; ++k)
    {
        const float mag = std::abs (fftTmp_[(size_t) k]) * norm + 1.0e-9f;
        float db = 20.0f * std::log10 (mag);

        const float freq = (float) k * (float) sampleRate_ / (float) fftSize_;
        if (freq > 1.0f)
            db += tilt_ * std::log2 (freq / refFreq);

        // Per-frame temporal smoothing. Kept light (0.35): the log-band energy
        // averaging in getLevels() already removes the per-bin ripple, and every
        // extra smoothing here is extra visible latency (0.5 cost ~33 ms).
        const float alpha = 0.35f;
        float& current = binLevelsDb_[(size_t) k];
        const float prev = hasFrame_ ? prevLevelsDb_[(size_t) k] : db;
        current = alpha * prev + (1.0f - alpha) * db;
        prevLevelsDb_[(size_t) k] = current;

        maxDelta = juce::jmax (maxDelta, std::abs (current - prev));
    }

    hasFrame_ = true;
    ++frameCounter_;
    lastFrameDeltaDb_ = maxDelta;
}

void SpectrumAnalyzer::rebuildLookup (int numPoints)
{
    const double fMin = 10.0;
    const double fMax = juce::jmin (22000.0, sampleRate_ * 0.5);
    const double binHz = sampleRate_ / (double) fftSize_;

    lutK0_.resize ((size_t) numPoints);
    lutK1_.resize ((size_t) numPoints);
    lutFreq_.resize ((size_t) numPoints);
    lutFrac_.resize ((size_t) numPoints);
    lutAvg_.resize ((size_t) numPoints);

    // One output point covers one log-pixel, i.e. the band [freq/step, freq*step].
    // Two regimes:
    //   * low frequencies, where one FFT bin spans SEVERAL pixels: interpolate
    //     between the neighbouring bins, otherwise every pixel in a bin would
    //     repeat the same value and the curve would look like square blocks.
    //   * high frequencies, where the pixel spans several bins: energy-average
    //     the covered bins, which removes the per-bin ripple.
    const double step = std::pow (fMax / fMin, 1.0 / (double) juce::jmax (1, numPoints - 1));

    for (int i = 0; i < numPoints; ++i)
    {
        const double t = (double) i / (double) juce::jmax (1, numPoints - 1);
        const double freq = fMin * std::pow (fMax / fMin, t);

        const double lo = (freq / step) / binHz;
        const double hi = (freq * step) / binHz;

        if (hi - lo < 1.0)
        {
            const double pos = freq / binHz;
            const int k = juce::jlimit (0, fftSize_ / 2, (int) std::floor (pos));

            lutK0_[(size_t) i]   = k;
            lutK1_[(size_t) i]   = juce::jlimit (0, fftSize_ / 2, k + 1);
            lutFrac_[(size_t) i] = (float) (pos - std::floor (pos));
            lutAvg_[(size_t) i]  = 0;
        }
        else
        {
            const int k0 = juce::jlimit (0, fftSize_ / 2, (int) std::floor (lo));
            int k1 = juce::jlimit (0, fftSize_ / 2, (int) std::ceil (hi));
            if (k1 <= k0)
                k1 = juce::jmin (fftSize_ / 2, k0 + 1);

            lutK0_[(size_t) i]   = k0;
            lutK1_[(size_t) i]   = k1;
            lutFrac_[(size_t) i] = 0.0f;
            lutAvg_[(size_t) i]  = 1;
        }

        lutFreq_[(size_t) i] = (float) freq;
    }

    lutPoints_ = numPoints;
    lutSampleRate_ = sampleRate_;
    lutFftSize_ = fftSize_;
}

void SpectrumAnalyzer::getLevels (juce::Array<float>& freqs, juce::Array<float>& levelsDb, int numPoints)
{
    freqs.clearQuick();
    levelsDb.clearQuick();

    if (numPoints <= 0 || fft_ == nullptr || binLevelsDb_.empty())
        return;

    if (numPoints != lutPoints_ || sampleRate_ != lutSampleRate_ || fftSize_ != lutFftSize_)
        rebuildLookup (numPoints);

    freqs.ensureStorageAllocated (numPoints);
    levelsDb.ensureStorageAllocated (numPoints);

    for (int i = 0; i < numPoints; ++i)
    {
        const size_t si = (size_t) i;
        const int k0 = lutK0_[si];
        const int k1 = lutK1_[si];

        float db;

        if (lutAvg_[si] != 0)
        {
            // Energy (RMS) average over the bins the pixel covers.
            double energy = 0.0;
            int count = 0;
            for (int k = k0; k <= k1; ++k)
            {
                energy += std::pow (10.0, (double) binLevelsDb_[(size_t) k] / 10.0);
                ++count;
            }
            db = (float) (10.0 * std::log10 (juce::jmax (1.0e-20, energy / (double) juce::jmax (1, count))));
        }
        else
        {
            // Smooth interpolation between the two neighbouring bins.
            const float a = binLevelsDb_[(size_t) k0];
            const float b = binLevelsDb_[(size_t) k1];
            db = a * (1.0f - lutFrac_[si]) + b * lutFrac_[si];
        }

        freqs.add (lutFreq_[si]);
        levelsDb.add (db);
    }
}
