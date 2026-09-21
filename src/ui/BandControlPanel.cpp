#include "BandControlPanel.h"

BandControlPanel::BandControlPanel (ProQ3CloneAudioProcessor& processor)
    : processor_ (processor)
{
    // --- band controls (main row) + global I/O strip ------------------------
    setupKnob (freqSlider_,  freqLabel_,  "Freq",   " Hz");
    setupKnob (gainSlider_,  gainLabel_,  "Gain",   " dB");
    setupKnob (qSlider_,     qLabel_,     "Q",      "");
    setupKnob (inSlider_,    inLabel_,    "In",     " dB");
    setupKnob (outSlider_,   outLabel_,   "Out",    " dB");
    setupKnob (colorAmountSlider_, colorAmountLabel_, "Amount", " %");

    // Combo items are taken from the parameters themselves, so the displayed
    // list and the parameter choice indices can never drift apart.
    const auto fillFromParam = [this] (juce::ComboBox& box, const juce::String& paramID)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor_.apvts.getParameter (paramID)))
            box.addItemList (p->choices, 1);
    };

    setupCombo (typeBox_,      typeLabel_,      "TYPE");
    fillFromParam (typeBox_, Param::bandType (0));
    typeBox_.setTooltip ("Filter type of the selected band");

    setupCombo (slopeBox_,     slopeLabel_,     "SLOPE");
    fillFromParam (slopeBox_, Param::bandSlope (0));
    slopeBox_.setTooltip ("Filter slope of the selected band (dB/oct)");

    setupCombo (phaseBox_,     phaseLabel_,     "PHASE");
    fillFromParam (phaseBox_, Param::phaseMode);
    phaseBox_.setTooltip ("Phase mode: zero latency, natural phase or linear phase");

    setupCombo (qualityBox_,   qualityLabel_,   "QUALITY");
    fillFromParam (qualityBox_, Param::linearQuality);
    qualityBox_.setTooltip ("Linear phase quality (CPU vs. accuracy)");

    setupCombo (gainScaleBox_, gainScaleLabel_, "GAIN SCALE");
    fillFromParam (gainScaleBox_, Param::gainScale);
    gainScaleBox_.setTooltip ("Vertical range of the frequency plot");

    setupCombo (analyzerBox_,  analyzerLabel_,  "ANALYZER");
    fillFromParam (analyzerBox_, Param::analyzerMode);
    analyzerBox_.setTooltip ("Spectrum analyzer source: off, pre, post or both");

    // --- COLOUR module: module label above, no per-selector captions --------
    addComboWithoutCaption (colorModeBox_);
    fillFromParam (colorModeBox_, Param::colorMode);
    colorModeBox_.setTooltip ("Colour module flavour");

    addComboWithoutCaption (colorPositionBox_);
    fillFromParam (colorPositionBox_, Param::colorPosition);
    colorPositionBox_.setTooltip ("Colour module position in the signal chain");

    addAndMakeVisible (colorSectionLabel_);
    colorSectionLabel_.setText ("COLOUR", juce::dontSendNotification);
    colorSectionLabel_.setJustificationType (juce::Justification::centredLeft);

    // --- band bypass pill --------------------------------------------------
    // Bypass is the INVERSE of the "band enabled" parameter: the pill shows
    // the theme's off colour while the band runs and switches to the accent
    // colour once bypassed, exactly like the compressor's Bypass pill.
    addAndMakeVisible (bypassButton_);
    bypassButton_.setButtonText ("Bypass");
    bypassButton_.setTooltip ("Bypass: the selected band is skipped entirely");
    bypassButton_.setClickingTogglesState (false);
    bypassButton_.onClick = [this]
    {
        if (selectedBand_ < 0)
            return;

        if (auto* p = processor_.apvts.getParameter (Param::bandEnabled (selectedBand_)))
        {
            const bool nowEnabled = p->getValue() < 0.5f;
            p->setValueNotifyingHost (nowEnabled ? 1.0f : 0.0f);
        }
    };

    auto& apvts = processor_.apvts;

    // --- global parameters (always attached) --------------------------------
    inAtt_  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::inputGain, inSlider_);
    outAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::outputGain, outSlider_);

    phaseAtt_     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::phaseMode, phaseBox_);
    qualityAtt_   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::linearQuality, qualityBox_);
    gainScaleAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::gainScale, gainScaleBox_);
    analyzerAtt_  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::analyzerMode, analyzerBox_);

    colorModeAtt_     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::colorMode, colorModeBox_);
    colorAmountAtt_   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>   (apvts, Param::colorAmount, colorAmountSlider_);
    colorPositionAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::colorPosition, colorPositionBox_);

    applyThemeColours();

    // QUALITY only drives the linear-phase FIR engine (FFT size / IR length),
    // so it is greyed out and its popup suppressed in Zero Latency and Natural
    // Phase mode. Listen to phaseMode so the state follows the parameter from
    // any source (UI combo, host automation, state recall).
    processor_.apvts.addParameterListener (Param::phaseMode, this);
    refreshQualityEnabled();

    // Start with the same band the display selected (or "no band" / disabled
    // state when nothing is selected yet).
    setSelectedBand (processor_.getSelectedBand());
}

BandControlPanel::~BandControlPanel()
{
    processor_.apvts.removeParameterListener (Param::phaseMode, this);

    if (selectedBand_ >= 0)
        processor_.apvts.removeParameterListener (Param::bandEnabled (selectedBand_), this);
}

void BandControlPanel::setupKnob (juce::Slider& slider, juce::Label& label,
                                  const juce::String& title, const juce::String& suffix)
{
    addAndMakeVisible (slider);
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 76, 18);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextValueSuffix (suffix);

    addAndMakeVisible (label);
    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
}

void BandControlPanel::setupCombo (juce::ComboBox& box, juce::Label& label,
                                   const juce::String& title)
{
    addAndMakeVisible (box);
    box.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (label);
    label.setText (title, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
}

void BandControlPanel::addComboWithoutCaption (juce::ComboBox& box)
{
    addAndMakeVisible (box);
    box.setJustificationType (juce::Justification::centredLeft);
}

// Label fonts/colours are theme-dependent (accent for the main knobs and the
// COLOUR module, plain text for the strip knobs, dim captions everywhere else).
// They are re-applied whenever the look and feel changes so a theme switch
// recolours them instantly.
void BandControlPanel::applyThemeColours()
{
    const bool light  = processor_.isLightTheme();
    const auto accent = CrunchLookAndFeel::accentForTheme (processor_.getTheme());
    const auto text   = CrunchPalette::text (light);
    const auto dim    = CrunchPalette::textDim (light);

    const auto styleMain = [accent] (juce::Label& l)
    {
        l.setFont (CrunchLookAndFeel::uiFont (16.5f, 600));
        l.setColour (juce::Label::textColourId, accent);
    };

    const auto styleSmall = [text] (juce::Label& l)
    {
        l.setFont (CrunchLookAndFeel::uiFont (14.5f, 600));
        l.setColour (juce::Label::textColourId, text);
    };

    const auto styleCaption = [dim] (juce::Label& l)
    {
        l.setFont (CrunchLookAndFeel::uiFont (12.5f, 600));
        l.setColour (juce::Label::textColourId, dim);
    };

    styleMain (freqLabel_);
    styleMain (gainLabel_);
    styleMain (qLabel_);

    styleSmall (inLabel_);
    styleSmall (outLabel_);
    styleSmall (colorAmountLabel_);

    styleCaption (typeLabel_);
    styleCaption (slopeLabel_);
    styleCaption (phaseLabel_);
    styleCaption (qualityLabel_);
    styleCaption (gainScaleLabel_);
    styleCaption (analyzerLabel_);

    colorSectionLabel_.setFont (CrunchLookAndFeel::uiFont (15.5f, 700));
    colorSectionLabel_.setColour (juce::Label::textColourId, accent);
}

void BandControlPanel::lookAndFeelChanged()
{
    applyThemeColours();
    repaint();
}

void BandControlPanel::refreshBypassState()
{
    if (selectedBand_ < 0)
    {
        bypassButton_.setToggleState (false, juce::dontSendNotification);
        return;
    }

    if (auto* v = processor_.apvts.getRawParameterValue (Param::bandEnabled (selectedBand_)))
        bypassButton_.setToggleState (v->load() < 0.5f, juce::dontSendNotification);
}

// QUALITY only affects the linear-phase FIR engine (FFT size / IR length), so
// in Zero Latency and Natural Phase mode it is disabled and dimmed: JUCE hides
// an open popup when a combo becomes disabled and never opens one from a
// disabled combo, and the alpha dims the box, its caption and its text the
// same way the band controls dim when no band is selected.
void BandControlPanel::refreshQualityEnabled()
{
    const bool linear = (int) processor_.apvts.getRawParameterValue (Param::phaseMode)->load()
                        == (int) Param::PhaseMode::LinearPhase;

    const float alpha = linear ? 1.0f : 0.45f;

    qualityBox_.setEnabled (linear);
    qualityBox_.setAlpha (alpha);
    qualityLabel_.setAlpha (alpha);
}

void BandControlPanel::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == Param::phaseMode)
    {
        // The host may call this from the audio thread, so bounce the widget
        // update to the message thread.
        juce::Component::SafePointer<BandControlPanel> safe (this);

        juce::MessageManager::callAsync ([safe]() mutable
        {
            if (safe != nullptr)
                safe->refreshQualityEnabled();
        });

        return;
    }

    if (parameterID != Param::bandEnabled (selectedBand_))
        return;

    // The host may call this from the audio thread, so bounce the widget
    // update to the message thread.
    juce::Component::SafePointer<BandControlPanel> safe (this);
    const bool bypassed = (newValue < 0.5f);

    juce::MessageManager::callAsync ([safe, bypassed]() mutable
    {
        if (safe != nullptr)
            safe->bypassButton_.setToggleState (bypassed, juce::dontSendNotification);
    });
}

void BandControlPanel::setSelectedBand (int band)
{
    if (selectedBand_ >= 0)
        processor_.apvts.removeParameterListener (Param::bandEnabled (selectedBand_), this);

    freqAtt_.reset();
    gainAtt_.reset();
    qAtt_.reset();
    typeAtt_.reset();
    slopeAtt_.reset();

    selectedBand_ = band;

    const bool on    = (band >= 0);
    const float alpha = on ? 1.0f : 0.45f;

    freqSlider_.setEnabled (on);
    gainSlider_.setEnabled (on);
    qSlider_.setEnabled (on);
    typeBox_.setEnabled (on);
    slopeBox_.setEnabled (on);
    bypassButton_.setEnabled (on);

    freqLabel_.setAlpha (alpha);
    gainLabel_.setAlpha (alpha);
    qLabel_.setAlpha (alpha);
    typeLabel_.setAlpha (alpha);
    slopeLabel_.setAlpha (alpha);
    typeBox_.setAlpha (alpha);
    slopeBox_.setAlpha (alpha);

    if (! on)
    {
        bypassButton_.setToggleState (false, juce::dontSendNotification);
        repaint();
        return;
    }

    auto& apvts = processor_.apvts;

    freqAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::bandFreq (selectedBand_), freqSlider_);
    gainAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::bandGain (selectedBand_), gainSlider_);
    qAtt_    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, Param::bandQ (selectedBand_), qSlider_);

    typeAtt_  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::bandType (selectedBand_), typeBox_);
    slopeAtt_ = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, Param::bandSlope (selectedBand_), slopeBox_);

    apvts.addParameterListener (Param::bandEnabled (selectedBand_), this);
    refreshBypassState();
    repaint();
}

void BandControlPanel::resized()
{
    auto area = getLocalBounds().reduced (12, 8);
    const int H = area.getHeight();
    const int W = area.getWidth();

    const auto vcentre = [] (juce::Rectangle<int> r, int h)
    {
        const int top = r.getY() + (r.getHeight() - h) / 2;
        return juce::Rectangle<int> (r.getX(), top, r.getWidth(), h);
    };

    const auto placeKnob = [] (juce::Slider& slider, juce::Label& label,
                               juce::Rectangle<int> bounds, int labelH)
    {
        auto knob = bounds;
        label.setBounds (knob.removeFromTop (labelH));
        slider.setBounds (knob);
    };

    const auto placeCaptionCombo = [&vcentre] (juce::Rectangle<int> col, int captionH, int comboH,
                                               juce::Label& label, juce::ComboBox& box)
    {
        label.setBounds (col.removeFromTop (captionH));
        box.setBounds (vcentre (col, comboH));
    };

    // --- left column: COLOUR module (top) + captioned selectors --------------
    const int leftW = juce::jlimit (190, (int) (W * 0.27f), 280);
    auto left = area.removeFromLeft (leftW);
    area.removeFromLeft (10);

    colorSectionLabel_.setBounds (left.removeFromTop (18));
    {
        auto row = left.removeFromTop (30);
        const int posW = juce::jmin (78, row.getWidth() / 3);
        colorPositionBox_.setBounds (vcentre (row.removeFromRight (posW).reduced (0, 2), 26));
        colorModeBox_.setBounds (vcentre (row.reduced (0, 2), 26));
    }

    // Amount knob (label above, value below)
    {
        auto amountRow = left.removeFromTop (104);
        colorAmountLabel_.setBounds (amountRow.removeFromTop (15));
        colorAmountSlider_.setBounds (amountRow);
    }

    left.removeFromTop (8);
    {
        const int colW = left.getWidth() / 2;
        const int selH = left.getHeight() / 2;

        auto row1 = left.removeFromTop (selH);
        placeCaptionCombo (row1.removeFromLeft (colW), 13, 26, phaseLabel_,   phaseBox_);
        placeCaptionCombo (row1,                       13, 26, qualityLabel_, qualityBox_);

        auto row2 = left;
        placeCaptionCombo (row2.removeFromLeft (colW), 13, 26, gainScaleLabel_, gainScaleBox_);
        placeCaptionCombo (row2,                       13, 26, analyzerLabel_,  analyzerBox_);
    }

    // --- right zone: main knob row + In/Out strip ---------------------------
    // The main row is taller than the bottom strip: its knobs read as the
    // primary controls of the selected band.
    const int stripH = (int) ((float) H * 0.42f);
    auto mainRow = area.removeFromTop (H - stripH);
    auto stripRow = area;

    bypassButton_.setBounds (vcentre (mainRow.removeFromRight (90).reduced (2), 32));

    // Type / slope selectors share the main row with the band knobs, so their
    // captions line up with the knob captions on the same baseline.
    const int selW = juce::jmin (124, mainRow.getWidth() / 4);
    placeCaptionCombo (mainRow.removeFromLeft (selW).reduced (2, 0), 18, 30, typeLabel_,  typeBox_);
    placeCaptionCombo (mainRow.removeFromLeft (selW).reduced (2, 0), 18, 30, slopeLabel_, slopeBox_);

    const int knobW = mainRow.getWidth() / 3;
    placeKnob (freqSlider_, freqLabel_, mainRow.removeFromLeft (knobW).reduced (2), 18);
    placeKnob (gainSlider_, gainLabel_, mainRow.removeFromLeft (knobW).reduced (2), 18);
    placeKnob (qSlider_,    qLabel_,    mainRow.reduced (2), 18);

    const int knobW2 = stripRow.getWidth() / 2;
    placeKnob (inSlider_,  inLabel_,  stripRow.removeFromLeft (knobW2).reduced (2), 16);
    placeKnob (outSlider_, outLabel_, stripRow.reduced (2), 16);
}
