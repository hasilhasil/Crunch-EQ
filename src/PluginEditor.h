#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "ui/DisplayView.h"
#include "ui/BandControlPanel.h"
#include "ui/LookAndFeel.h"

class ProQ3CloneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit ProQ3CloneAudioProcessorEditor (ProQ3CloneAudioProcessor&);
    ~ProQ3CloneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    ProQ3CloneAudioProcessor& processor_;

    CrunchLookAndFeel lookAndFeel_;
    DisplayView display_;
    BandControlPanel panel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProQ3CloneAudioProcessorEditor)
};
