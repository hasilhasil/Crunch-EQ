#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "ui/DisplayView.h"
#include "ui/BandControlPanel.h"
#include "ui/LookAndFeel.h"

class ProQ3CloneAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::AudioProcessorValueTreeState::Listener,
                                       public juce::Timer
{
public:
    explicit ProQ3CloneAudioProcessorEditor (ProQ3CloneAudioProcessor&);
    ~ProQ3CloneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void moved() override;
    void timerCallback() override;
    void parentHierarchyChanged() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

private:
    // JUCE 8 gives HWND peers the Direct2D backend by default. Under Direct2D
    // every drawImageAt() of a software Image converts it to a native bitmap
    // and re-uploads the whole thing to the GPU (NativeImageType::convert has
    // no cache), so the cached grid image cost a full CPU->GPU copy per
    // repaint - more than the rest of the frame combined, and growing with
    // the window size. The software renderer blits the same image directly,
    // which makes the frame cost predictable CPU work.
    void forceSoftwareRenderer();

    // Marks the start of a user drag/resize loop and arms the timer that ends
    // it (see moved() / timerCallback()).
    void noteWindowMove();

    // Timestamp of the last window move/resize event, used to detect when the
    // user's drag loop has finished (see moved() / timerCallback()).
    juce::uint32 lastMoveMs_ = 0;

    ProQ3CloneAudioProcessor& processor_;

    CrunchLookAndFeel lookAndFeel_;
    DisplayView display_;
    BandControlPanel panel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProQ3CloneAudioProcessorEditor)
};
