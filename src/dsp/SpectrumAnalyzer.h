#pragma once

#include <JuceHeader.h>

#include <complex>
#include <vector>

class SpectrumAnalyzer
{
public:
    void prepare (double sampleRate, int fftSize);
    void reset();
    // Feed samples into the internal buffer. This only buffers: frame
    // production is driven by processPendingFrame() so the caller can pace it.
    void pushSamples (const float* mono, int numSamples);
    // Produce ONE frame from the buffered audio (oldest window first, window
    // advanced by hop_ samples). Returns true when a frame was produced. The
    // caller decides when this happens, which is what keeps the analysed frame
    // rate independent of the host's audio block size.
    bool processPendingFrame();
    void setTilt (float dbPerOct);
    void getLevels (juce::Array<float>& freqs, juce::Array<float>& levelsDb, int numPoints);

    // Number of FFT frames analysed so far and how much the last frame moved
    // the displayed levels. The UI uses these to skip repaints while the
    // spectrum is static (silence / steady tone) instead of redrawing 60x/s.
    juce::uint32 getFrameCounter() const noexcept { return frameCounter_; }
    float getLastFrameDeltaDb() const noexcept    { return lastFrameDeltaDb_; }

private:
    void processFrame (const float* data);
    void rebuildLookup (int numPoints);

    double sampleRate_ = 48000.0;
    int    fftSize_    = 4096;
    float  tilt_       = 4.5f;

    std::vector<float> fifo_;
    int    hop_            = 512;   // samples between frames (~60 fps: sr/60)

    std::vector<float> window_;
    std::vector<std::complex<float>> fftBuf_;
    std::vector<std::complex<float>> fftTmp_;
    std::unique_ptr<juce::dsp::FFT> fft_;

    std::vector<float> binLevelsDb_;
    std::vector<float> prevLevelsDb_;
    bool hasFrame_ = false;

    // Asymmetric (Pro-Q "Speed" style) release time constant in milliseconds.
    // The spectrum rises instantly and falls with this time constant; raise it
    // for a calmer, slower-falling display.
    static constexpr double kReleaseMs = 20.0;
    juce::uint32 lastFrameMs_ = 0;

    juce::uint32 frameCounter_ = 0;
    float lastFrameDeltaDb_ = 0.0f;

    // Log-spaced plot lookup: the frequency/position/weight of every output
    // point only depends on the sample rate and FFT size, so it is built once
    // instead of running a std::pow per point on every UI tick.
    std::vector<int>   lutK0_, lutK1_;
    std::vector<float> lutFreq_, lutFrac_;
    std::vector<uint8_t> lutAvg_;   // 0 = interpolate bins, 1 = average the covered bins
    int    lutPoints_ = -1;
    double lutSampleRate_ = 0.0;
    int    lutFftSize_ = 0;
};
