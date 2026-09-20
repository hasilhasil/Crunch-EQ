#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <vector>

#include "SpectrumAnalyzer.h"

// Fixed-rate spectrum analysis engine.
//
// Hosts hand audio to plugins in blocks whose size THEY control (FL Studio's
// mixer commonly uses 1024-4096 samples). Driving the FFT from the UI tick
// therefore means the displayed spectrum only refreshes as often as the host
// produces audio: 12-47 fps in a large-buffer session, while the meters (which
// only need peak values) still refresh on every tick. That mismatch is exactly
// what "the spectrum stutters but the meters are smooth" looks like.
//
// This engine removes the coupling:
//   * the audio thread only writes into lock-free ring buffers - no locks, no
//     allocation, and it can never block
//   * a background thread wakes up at a fixed rate, feeds the analyser from the
//     ring and publishes at most ONE frame per display tick (see
//     kFrameIntervalMs) through a lock-free triple buffer
//   * the UI thread never runs an FFT; it picks up the newest published points
//     and shapes them for display (attack/release + peak hold, see DisplayView)
class SpectrumEngine : public juce::Thread
{
public:
    static constexpr int kDisplayPoints = 512;

    // One published frame per display tick. Emitting faster only creates bursts
    // the UI cannot use (it reads the newest slot), emitting slower leaves
    // ticks without new data - both show up as stutter.
    static constexpr juce::uint32 kFrameIntervalMs = 16;

    SpectrumEngine();
    ~SpectrumEngine() override;

    // called from the processor (prepareToPlay) / UI thread
    void setSampleRate (double rate)      noexcept { sampleRate_.store (rate); }
    void setTilt (float dbPerOct)         noexcept { tilt_.store (dbPerOct); }
    void setMode (int analyzerModeIndex)  noexcept { mode_.store (analyzerModeIndex); }
    void setUIActive (bool isActive)      noexcept { uiActive_.store (isActive); }

    // audio thread: mono pre/post signals, same length.
    void pushSamples (const float* pre, const float* post, int numSamples) noexcept;

    // UI thread: newest published display points. Returns true when they are new.
    bool getLatest (std::array<float, kDisplayPoints>& pre,
                    std::array<float, kDisplayPoints>& post);

    // UI thread: log-spaced frequencies of the published points.
    bool getFrequencies (std::vector<float>& freqs);

private:
    void run() override;
    void analyseTick();
    static void drain (juce::AbstractFifo& fifo, std::vector<float>& ring, SpectrumAnalyzer& analyzer);

    std::atomic<double> sampleRate_ { 48000.0 };
    std::atomic<float>  tilt_      { 4.5f };
    std::atomic<int>    mode_      { 3 };   // Param::analyzerMode index
    std::atomic<bool>   uiActive_  { false };

    juce::AbstractFifo fifoPre_  { 1 << 16 };
    juce::AbstractFifo fifoPost_ { 1 << 16 };
    std::vector<float> ringPre_, ringPost_;

    SpectrumAnalyzer analyzerPre_, analyzerPost_;
    double currentRate_ = 0.0;
    juce::uint32 lastFrameMs_ = 0;

    struct Snapshot { std::array<float, kDisplayPoints> pre {}, post {}; };
    std::array<Snapshot, 3> slots_;
    std::atomic<int> publishedSlot_ { -1 };
    int writeSlot_ = 0;
    int uiSlot_    = -1;

    std::vector<float> freqs_;
    juce::Array<float> scratchFreqs_, scratchPre_, scratchPost_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumEngine)
};
