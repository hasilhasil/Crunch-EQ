#pragma once

#include <JuceHeader.h>

#include <array>
#include <vector>

#include "dsp/EqualizerDSP.h"
#include "dsp/ColorProcessor.h"
#include "dsp/SpectrumEngine.h"
#include "Parameters.h"

class ProQ3CloneAudioProcessor : public juce::AudioProcessor
{
public:
    ProQ3CloneAudioProcessor();
    ~ProQ3CloneAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    // Spectrum analysis engine (audio thread feeds it lock-free, its own thread
    // runs the FFT at a fixed frame rate). The editor consumes the published
    // points; it must never run an FFT itself.
    SpectrumEngine& getSpectrumEngine() noexcept { return spectrumEngine_; }

    int  getSelectedBand() const noexcept { return selectedBand_.load(); }
    void setSelectedBand (int index);

    float getInputPeak() const noexcept  { return inputPeak_.load(); }
    float getOutputPeak() const noexcept { return outputPeak_.load(); }

    int getTheme() const noexcept { return theme_.load(); }
    void setTheme (int theme) { theme_.store (juce::jlimit (0, 2, theme)); }
    juce::Colour getThemeAccent() const;
    bool isLightTheme() const noexcept { return theme_.load() == 2; }

    // True while the editor window is being dragged or resized. Hosts that
    // move the plugin editor as its own top-level window (FL Studio) send a
    // window-position change per mouse sample, each of which invalidates the
    // whole window; combined with the display's own 60 Hz repaints that
    // saturates the message loop and shows up as stutter and smearing while
    // dragging. The display suspends its animation repaints while this is set.
    bool isWindowDragging() const noexcept { return windowDragging_.load(); }
    void setWindowDragging (bool dragging) noexcept { windowDragging_.store (dragging); }

    int getEditorWidth() const noexcept  { return editorWidth_.load(); }
    int getEditorHeight() const noexcept { return editorHeight_.load(); }
    void setEditorSize (int w, int h)    { editorWidth_.store (w); editorHeight_.store (h); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void updateParameters();

    EqualizerDSP eq_;
    ColorProcessor color_;

    // Display analysis: the audio thread only writes into the engine's rings.
    SpectrumEngine spectrumEngine_;
    std::vector<float> analysisScratchPre_, analysisScratchPost_;

    bool latencyDirty_ = true;

    // Display-time alignment for the analyser and meters. The linear-phase
    // engine delays the post tap (and the output meter) by
    // getLatencySamples(), so the pre tap and the input meter are pushed
    // through this matching delay line - otherwise the two spectrum layers
    // and the two meters drift apart by the plugin latency (32/64/128 ms for
    // Low/Medium/Maximum at 48 kHz) in Linear Phase mode. Zero-latency and
    // natural-phase modes report 0 samples, where the line is a passthrough.
    static constexpr int kMaxDisplayDelay = 8192;   // > max quality latency (6143)

    struct DisplayDelay
    {
        void prepare (int maxSamples)
        {
            buf_.assign ((size_t) jmax (1, maxSamples), 0.0f);
            setLength (0);
        }

        void setLength (int samples)
        {
            len_ = jlimit (0, (int) buf_.size() - 1, jmax (0, samples));
            std::fill (buf_.begin(), buf_.end(), 0.0f);
            write_ = 0;
        }

        float process (float x) noexcept
        {
            if (len_ == 0)
                return x;

            const int size = (int) buf_.size();
            buf_[(size_t) write_] = x;
            int read = write_ - len_;
            if (read < 0)
                read += size;
            const float y = buf_[(size_t) read];
            if (++write_ >= size)
                write_ = 0;
            return y;
        }

        std::vector<float> buf_;
        int write_ = 0;
        int len_   = 0;
    };

    DisplayDelay preDelay_;

    std::atomic<int> selectedBand_ { 0 };
    std::atomic<float> inputPeak_ { 0.0f };
    std::atomic<float> outputPeak_ { 0.0f };
    std::atomic<int> theme_ { 0 };
    std::atomic<bool> windowDragging_ { false };
    std::atomic<int> editorWidth_ { 0 };
    std::atomic<int> editorHeight_ { 0 };

    std::array<std::atomic<float>*, Param::kMaxBands> rawEnabled_ {};
    std::array<std::atomic<float>*, Param::kMaxBands> rawType_ {};
    std::array<std::atomic<float>*, Param::kMaxBands> rawFreq_ {};
    std::array<std::atomic<float>*, Param::kMaxBands> rawGain_ {};
    std::array<std::atomic<float>*, Param::kMaxBands> rawQ_ {};
    std::array<std::atomic<float>*, Param::kMaxBands> rawSlope_ {};

    std::atomic<float>* rawPhaseMode_ = nullptr;
    std::atomic<float>* rawLinearQuality_ = nullptr;
    std::atomic<float>* rawInputGain_ = nullptr;
    std::atomic<float>* rawOutputGain_ = nullptr;
    std::atomic<float>* rawColorMode_ = nullptr;
    std::atomic<float>* rawColorAmount_ = nullptr;
    std::atomic<float>* rawColorPosition_ = nullptr;

    struct BandCache { float type = 0.0f, freq = 1000.0f, gain = 0.0f, q = 1.0f, slope = 1.0f, enabled = 0.0f; };
    std::array<BandCache, Param::kMaxBands> bandCache_ {};
    float cachedPhase_ = -1.0f, cachedQuality_ = -1.0f;
    float cachedInGain_ = 0.0f, cachedOutGain_ = 0.0f;
    float cachedColorMode_ = -1.0f, cachedColorAmount_ = -1.0f;
    bool cacheValid_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProQ3CloneAudioProcessor)
};
