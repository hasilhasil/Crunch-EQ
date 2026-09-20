#include "PluginEditor.h"

ProQ3CloneAudioProcessorEditor::ProQ3CloneAudioProcessorEditor (ProQ3CloneAudioProcessor& p)
    : juce::AudioProcessorEditor (p),
      processor_ (p),
      display_ (p),
      panel_ (p)
{
    setLookAndFeel (&lookAndFeel_);
    lookAndFeel_.setTheme (processor_.getTheme());

    addAndMakeVisible (display_);
    addAndMakeVisible (panel_);

    const int savedW = processor_.getEditorWidth();
    const int savedH = processor_.getEditorHeight();
    setSize (savedW > 0 ? savedW : 920, savedH > 0 ? savedH : 620);
    setResizable (true, true);
    setResizeLimits (700, 500, 1840, 1240);

    sendLookAndFeelChange();

    for (auto* param : processor_.getParameters())
    {
        if (auto* idp = dynamic_cast<juce::RangedAudioParameter*> (param))
            processor_.apvts.addParameterListener (idp->paramID, this);
    }

    display_.onBandSelected = [this] (int band) { panel_.setSelectedBand (band); };

    // Theme changes must repaint every component: JUCE buttons/combos do not
    // reliably repaint on lookAndFeelChanged(), so panels would keep stale
    // (e.g. black-on-cream-theme) colours.
    display_.onThemeChanged = [this]
    {
        sendLookAndFeelChange();
        display_.repaint();
        panel_.repaint();
        repaint();
    };
}

ProQ3CloneAudioProcessorEditor::~ProQ3CloneAudioProcessorEditor()
{
    for (auto* param : processor_.getParameters())
    {
        if (auto* idp = dynamic_cast<juce::RangedAudioParameter*> (param))
            processor_.apvts.removeParameterListener (idp->paramID, this);
    }

    setLookAndFeel (nullptr);
}

void ProQ3CloneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (CrunchPalette::background (lookAndFeel_.isLightTheme()));
}

void ProQ3CloneAudioProcessorEditor::resized()
{
    processor_.setEditorSize (getWidth(), getHeight());

    auto area = getLocalBounds();
    panel_.setBounds (area.removeFromBottom (280));
    display_.setBounds (area);
}

void ProQ3CloneAudioProcessorEditor::parameterChanged (const juce::String&, float)
{
    // Let the display know its cached response curve is stale instead of
    // recomputing it on every repaint.
    display_.parametersChanged();
    display_.repaint();
    panel_.repaint();
}
