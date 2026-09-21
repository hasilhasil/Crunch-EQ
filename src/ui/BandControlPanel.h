#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"
#include "LookAndFeel.h"

// Bottom panel (280 px, two zones) — same skeleton as the Crunch Compressor
// bottom panel so both plugins read as one product family:
//
//   +---------------------------------------------+---------------------------+
//   | COLOUR                         (label above)| TYPE       SLOPE          |
//   | [Color Mode][Position]                      |  (captioned selectors)    |
//   | Amount                                      |  Freq    Gain     Q  Bypass|
//   | ---------                                   |  (main knobs, accent       |
//   | PHASE          QUALITY                      |   labels, value below)    |
//   | [Zero Latency][Medium]                      |---------------------------|
//   | GAIN SCALE       ANALYZER                   |  In                 Out   |
//   | [12 dB][Pre + Post]                         |  (bottom knob strip)      |
//   +---------------------------------------------+---------------------------+
//
// Every selector carries a caption above it so its function is obvious; the
// main knob row is taller than the bottom strip so its knobs read as the
// primary controls of the selected band. The COLOUR module sits on the left,
// aligned with the selector column, with its own module label above.
class BandControlPanel : public juce::Component,
                         public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit BandControlPanel (ProQ3CloneAudioProcessor& processor);
    ~BandControlPanel() override;

    void resized() override;
    void lookAndFeelChanged() override;
    void setSelectedBand (int band);
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    void setupKnob (juce::Slider& slider, juce::Label& label, const juce::String& title, const juce::String& suffix);
    void setupCombo (juce::ComboBox& box, juce::Label& label, const juce::String& title);
    void addComboWithoutCaption (juce::ComboBox& box);
    void applyThemeColours();
    void refreshBypassState();
    void refreshQualityEnabled();

    ProQ3CloneAudioProcessor& processor_;

    juce::Slider freqSlider_, gainSlider_, qSlider_, inSlider_, outSlider_, colorAmountSlider_;
    juce::Label  freqLabel_, gainLabel_, qLabel_, inLabel_, outLabel_, colorAmountLabel_;

    juce::ComboBox typeBox_, slopeBox_, phaseBox_, qualityBox_, gainScaleBox_, analyzerBox_;
    juce::Label    typeLabel_, slopeLabel_, phaseLabel_, qualityLabel_, gainScaleLabel_, analyzerLabel_;

    // COLOUR module: covered by the "COLOUR" module label above, so its two
    // selectors get no individual captions (mirrors the compressor's STYLE).
    juce::ComboBox colorModeBox_, colorPositionBox_;
    juce::Label    colorSectionLabel_;

    juce::TextButton bypassButton_;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   freqAtt_, gainAtt_, qAtt_, inAtt_, outAtt_, colorAmountAtt_;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAtt_, slopeAtt_, phaseAtt_, qualityAtt_, gainScaleAtt_, analyzerAtt_, colorModeAtt_, colorPositionAtt_;

    int selectedBand_ = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandControlPanel)
};
