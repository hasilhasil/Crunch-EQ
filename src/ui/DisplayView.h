#pragma once

#include <JuceHeader.h>

#include <array>
#include <functional>
#include <vector>

#include "../PluginProcessor.h"
#include "../dsp/EQBand.h"
#include "../dsp/SpectrumEngine.h"

class DisplayView : public juce::Component,
                    public juce::Timer
{
public:
    explicit DisplayView (ProQ3CloneAudioProcessor& processor);
    ~DisplayView() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    std::function<void (int)> onBandSelected;
    std::function<void()> onThemeChanged;

    // Called by the editor whenever any parameter change was notified: the
    // response curve is only recomputed when something actually changed, so a
    // static instance costs nothing per frame.
    void parametersChanged() { responseDirty_ = true; }

private:
    void updateDisplayTargets();
    void updateResponseCurve();
    void drawGrid (juce::Graphics& g);
    void drawDbScale (juce::Graphics& g);
    void drawSpectrum (juce::Graphics& g);
    void drawResponseCurve (juce::Graphics& g);
    void drawBandNodes (juce::Graphics& g);
    void drawMeters (juce::Graphics& g);
    void refreshGridCache();

    int hitTestBand (juce::Point<float> pos) const;
    void addBandAt (juce::Point<float> pos);
    void removeBand (int index);
    void setFloatParam (const juce::String& id, float value);
    void setChoiceParam (const juce::String& id, int index);
    void showSettingsMenu();

    juce::Colour accentColour() const;
    juce::Colour bgColour() const;
    juce::Colour gridColour() const;
    juce::Colour textColour() const;

    float freqToX (float freq) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    float spectrumDbToY (float db) const;
    float gainScaleDb() const;
    float xToQ (float x) const;
    int plotWidth() const;

    ProQ3CloneAudioProcessor& processor_;
    SpectrumEngine& spectrumEngine_;

    // --- displayed spectrum --------------------------------------------------
    // The engine publishes raw log-band points at one frame per display tick;
    // those points are drawn as they are. The analyser's own per-frame temporal
    // smoothing is what keeps the curve steady, exactly like the previous
    // version's look.
    static constexpr int kPoints = SpectrumEngine::kDisplayPoints;

    std::vector<float> spectrumFreqs_;
    std::array<float, kPoints> spectrumPre_ {}, spectrumPost_ {};
    bool  spectrumValid_ = false;
    bool  spectrumHasFrame_ = false;

    // One biquad section of the current curve, flattened over every enabled
    // band. Designing the coefficients once per parameter change (instead of
    // once per curve point) is what makes the curve affordable to draw.
    struct ResponseSection { double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; };
    static double sectionMagnitudeSquared (const ResponseSection& s, double cw, double sw) noexcept;

    std::vector<ResponseSection> responseSections_;
    std::vector<float> responseFreqs_;
    std::vector<float> responseDb_;
    bool  responseDirty_ = true;
    float responseSampleRate_ = 0.0f;

    // --- static grid cache (background, lines, axis labels) -----------------
    juce::Image gridImage_;
    int   gridCacheW_ = -1, gridCacheH_ = -1;
    float gridCacheGainScale_ = -1.0f;
    bool  gridCacheLight_ = false;

    float lastMeterIn_ = -200.0f, lastMeterOut_ = -200.0f;
    float lastPeakIn_ = -200.0f, lastPeakOut_ = -200.0f;

    juce::TextButton settingsButton_;

    int dragBand_ = -1;
    bool dragging_ = false;
    bool dragQ_ = false;
    juce::Point<float> dragStartPos_;

    float meterInDb_ = -120.0f;
    float meterOutDb_ = -120.0f;

    // Peak-hold for the meter top strokes (Pro-Q 3 style): the stroke rides the
    // peak and holds it for kPeakHoldMs before falling back, so a short peak
    // stays readable. The bar itself keeps its fast (instant up / 60 dB per
    // second down) ballistics.
    static constexpr int kPeakHoldMs = 2000;          // 1-3 s, as requested
    static constexpr float kPeakFallDbPerSec = 30.0f; // after the hold expires

    float peakInDb_  = -120.0f, peakOutDb_  = -120.0f;
    juce::uint32 peakInHoldUntilMs_ = 0, peakOutHoldUntilMs_ = 0;
    juce::uint32 lastMeterMs_ = 0;

    void updatePeakHold (float& peakDb, juce::uint32& holdUntilMs,
                         float levelDb, juce::uint32 nowMs, double dt);

    float meterInDbLabel_ = -120.0f;
    float meterOutDbLabel_ = -120.0f;

    static constexpr float kFreqMin = 10.0f;
    static constexpr float kFreqMax = 20000.0f;
    static constexpr int   kMeterWidth = 60;
    static constexpr float kPlotPadTop = 16.0f;
    static constexpr float kPlotPadBottom = 24.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DisplayView)
};
