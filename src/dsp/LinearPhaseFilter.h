#pragma once

#include <JuceHeader.h>

#include <complex>
#include <vector>

class LinearPhaseFilter
{
public:
    LinearPhaseFilter() = default;

    void prepare (double sampleRate, int numChannels, int fftSize);
    void setImpulseResponse (const float* ir, int length);
    void reset();
    void process (const float* const* input, float* const* output, int numChannels, int numSamples);

    int getLatencySamples() const noexcept { return latency_; }
    int getFftSize() const noexcept        { return fftSize_; }

    // True once an impulse response has been installed (and the spectrum is
    // therefore usable). A prepared-but-empty filter would output silence, so
    // callers can use this to force an immediate rebuild.
    bool hasImpulseResponse() const noexcept { return irLength_ > 0 && fft_ != nullptr; }

private:
    void processOneChannel (int channel, const float* input, float* output, int numSamples);

    double sampleRate_   = 48000.0;
    int    fftSize_      = 4096;
    int    blockSize_    = 2048;
    int    irLength_     = 0;
    int    latency_      = 0;
    int    numChannels_  = 2;

    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<std::complex<float>> irSpectrum_;

    std::vector<std::vector<float>> inputFifo_;
    std::vector<std::vector<float>> outputFifo_;
    std::vector<std::vector<float>> overlap_;

    std::vector<std::complex<float>> fftIn_;
    std::vector<std::complex<float>> fftOut_;
};
