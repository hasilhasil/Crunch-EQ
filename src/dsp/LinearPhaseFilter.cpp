#include "LinearPhaseFilter.h"

void LinearPhaseFilter::prepare (double sampleRate, int numChannels, int fftSize)
{
    sampleRate_  = sampleRate;
    numChannels_ = juce::jmax (1, numChannels);
    fftSize_     = juce::nextPowerOfTwo (fftSize);
    blockSize_   = fftSize_ / 2;

    const int order = juce::roundToInt (std::log2 ((float) fftSize_));
    fft_ = std::make_unique<juce::dsp::FFT> (order);

    fftIn_.assign  (fftSize_, {});
    fftOut_.assign (fftSize_, {});
    irSpectrum_.assign (fftSize_, {});

    inputFifo_.assign  (numChannels_, {});
    outputFifo_.assign (numChannels_, {});
    overlap_.assign    (numChannels_, std::vector<float> ((size_t) blockSize_, 0.0f));

    irLength_ = 0;
    latency_  = 0;
    reset();
}

void LinearPhaseFilter::setImpulseResponse (const float* ir, int length)
{
    if (fft_ == nullptr)
        return;

    irLength_ = juce::jmin (length, blockSize_ + 1);

    std::fill (fftIn_.begin(), fftIn_.end(), std::complex<float> {});
    for (int i = 0; i < irLength_; ++i)
        fftIn_[(size_t) i] = std::complex<float> (ir[i], 0.0f);

    fft_->perform (fftIn_.data(), irSpectrum_.data(), false);

    latency_ = blockSize_ + (irLength_ - 1) / 2;

    // Deliberately NO reset() here: clearing the FIFOs on every IR change
    // would drop the in-flight audio and emit a block of silence, which is
    // exactly what dragging a band node used to do (the IR is rebuilt on
    // every parameter change). Callers that need a flush - prepare(), or
    // entering linear-phase mode - call reset() explicitly instead.
}

void LinearPhaseFilter::reset()
{
    for (auto& fifo : inputFifo_)  fifo.clear();
    for (auto& fifo : outputFifo_) fifo.clear();
    for (auto& ov : overlap_)      std::fill (ov.begin(), ov.end(), 0.0f);
}

void LinearPhaseFilter::process (const float* const* input, float* const* output, int numChannels, int numSamples)
{
    if (fft_ == nullptr || irLength_ == 0)
    {
        for (int ch = 0; ch < numChannels; ++ch)
            juce::FloatVectorOperations::clear (output[ch], numSamples);
        return;
    }

    const int chans = juce::jmin (numChannels, numChannels_);
    for (int ch = 0; ch < chans; ++ch)
        processOneChannel (ch, input[ch], output[ch], numSamples);
}

void LinearPhaseFilter::processOneChannel (int channel, const float* input, float* output, int numSamples)
{
    auto& inFifo  = inputFifo_[(size_t) channel];
    auto& outFifo = outputFifo_[(size_t) channel];
    auto& ov      = overlap_[(size_t) channel];

    inFifo.insert (inFifo.end(), input, input + numSamples);

    while ((int) inFifo.size() >= blockSize_)
    {
        for (int i = 0; i < blockSize_; ++i)
            fftIn_[(size_t) i] = std::complex<float> (inFifo[(size_t) i], 0.0f);
        for (int i = blockSize_; i < fftSize_; ++i)
            fftIn_[(size_t) i] = std::complex<float> {};

        inFifo.erase (inFifo.begin(), inFifo.begin() + blockSize_);

        fft_->perform (fftIn_.data(), fftOut_.data(), false);

        for (int i = 0; i < fftSize_; ++i)
            fftOut_[(size_t) i] *= irSpectrum_[(size_t) i];

        fft_->perform (fftOut_.data(), fftIn_.data(), true);

        for (int i = 0; i < blockSize_; ++i)
            outFifo.push_back (fftIn_[(size_t) i].real() + ov[(size_t) i]);

        for (int i = 0; i < blockSize_; ++i)
        {
            if (i < irLength_ - 1)
                ov[(size_t) i] = fftIn_[(size_t) (blockSize_ + i)].real();
            else
                ov[(size_t) i] = 0.0f;
        }
    }

    const int have = (int) outFifo.size();
    if (have >= numSamples)
    {
        std::copy (outFifo.begin(), outFifo.begin() + numSamples, output);
        outFifo.erase (outFifo.begin(), outFifo.begin() + numSamples);
    }
    else
    {
        for (int i = 0; i < have; ++i)
            output[i] = outFifo[(size_t) i];
        for (int i = have; i < numSamples; ++i)
            output[i] = 0.0f;
        outFifo.clear();
    }
}
