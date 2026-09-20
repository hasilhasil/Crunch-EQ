#include "DisplayView.h"
#include "LookAndFeel.h"

// |H(e^{jw})|^2 of one biquad section for precomputed cos/sin at w.
// Sharing the trig across every section of every band is what keeps the curve
// cheap: the naive version ran 4 transcendentals per section per point plus a
// heap allocation per (band, point) pair.
double DisplayView::sectionMagnitudeSquared (const ResponseSection& s, double cw, double sw) noexcept
{
    const double c2 = 2.0 * cw * cw - 1.0;   // cos (2w)
    const double s2 = 2.0 * sw * cw;         // sin (2w)

    const double numRe = s.b0 + s.b1 * cw + s.b2 * c2;
    const double numIm = -(s.b1 * sw + s.b2 * s2);
    const double denRe = 1.0 + s.a1 * cw + s.a2 * c2;
    const double denIm = -(s.a1 * sw + s.a2 * s2);

    const double num = numRe * numRe + numIm * numIm;
    const double den = denRe * denRe + denIm * denIm;
    return den > 0.0 ? num / den : 1.0;
}

DisplayView::DisplayView (ProQ3CloneAudioProcessor& processor)
    : processor_ (processor),
      spectrumEngine_ (processor.getSpectrumEngine())
{
    startTimerHz (60);
    spectrumEngine_.setUIActive (true);

    addAndMakeVisible (settingsButton_);
    settingsButton_.setButtonText (juce::CharPointer_UTF8 ("\xe2\x9a\x99"));
    settingsButton_.setTooltip ("Settings");
    settingsButton_.onClick = [this] { showSettingsMenu(); };
}

DisplayView::~DisplayView()
{
    spectrumEngine_.setUIActive (false);
    stopTimer();
}

void DisplayView::resized()
{
    settingsButton_.setBounds (4, 4, 24, 24);
}

float DisplayView::gainScaleDb() const
{
    const int idx = (int) processor_.apvts.getRawParameterValue (Param::gainScale)->load();
    static const float scales[] = { 3.0f, 6.0f, 12.0f, 30.0f };
    return scales[juce::jlimit (0, 3, idx)];
}

int DisplayView::plotWidth() const
{
    return juce::jmax (1, getWidth() - kMeterWidth);
}

juce::Colour DisplayView::accentColour() const { return processor_.getThemeAccent(); }
juce::Colour DisplayView::bgColour() const     { return CrunchPalette::background (processor_.isLightTheme()); }
juce::Colour DisplayView::gridColour() const   { return CrunchPalette::grid (processor_.isLightTheme()); }
juce::Colour DisplayView::textColour() const   { return CrunchPalette::textDim (processor_.isLightTheme()); }

float DisplayView::freqToX (float freq) const
{
    const float t = std::log (freq / kFreqMin) / std::log (kFreqMax / kFreqMin);
    return juce::jlimit (0.0f, 1.0f, t) * (float) plotWidth();
}

float DisplayView::xToFreq (float x) const
{
    const float t = juce::jlimit (0.0f, 1.0f, x / (float) plotWidth());
    return kFreqMin * std::pow (kFreqMax / kFreqMin, t);
}

float DisplayView::dbToY (float db) const
{
    const float half = gainScaleDb();
    const float frac = juce::jlimit (0.0f, 1.0f, (db + half) / (2.0f * half));
    const float usable = (float) getHeight() - kPlotPadTop - kPlotPadBottom;
    return kPlotPadTop + (1.0f - frac) * usable;
}

float DisplayView::yToDb (float y) const
{
    const float half = gainScaleDb();
    const float usable = (float) getHeight() - kPlotPadTop - kPlotPadBottom;
    const float frac = 1.0f - juce::jlimit (0.0f, 1.0f, (y - kPlotPadTop) / usable);
    return (frac * 2.0f - 1.0f) * half;
}

float DisplayView::spectrumDbToY (float db) const
{
    const float minDb = -80.0f, maxDb = 6.0f;
    const float frac = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
    return (1.0f - frac) * (float) getHeight();
}

float DisplayView::xToQ (float x) const
{
    const float frac = juce::jlimit (0.0f, 1.0f, x / (float) plotWidth());
    return Param::kQMin * std::pow (Param::kQMax / Param::kQMin, frac);
}

void DisplayView::updateDisplayTargets()
{
    // Pass the display preferences on to the engine; it owns the FFT entirely.
    spectrumEngine_.setMode ((int) processor_.apvts.getRawParameterValue (Param::analyzerMode)->load());
    spectrumEngine_.setTilt (processor_.apvts.getRawParameterValue (Param::analyzerTilt)->load());

    std::array<float, kPoints> pre, post;
    if (! spectrumEngine_.getLatest (pre, post))
    {
        spectrumHasFrame_ = false;   // nothing new: the picture would not change
        return;
    }

    if (spectrumFreqs_.empty())
    {
        std::vector<float> freqs;
        if (spectrumEngine_.getFrequencies (freqs))
            spectrumFreqs_ = freqs;
    }

    if (spectrumFreqs_.size() != (size_t) kPoints)
        return;

    // The published points are drawn as they are (no extra shaping on the UI
    // side): the engine already paces them to one frame per display tick and
    // the analyser applies its own temporal smoothing per frame.
    spectrumPre_  = pre;
    spectrumPost_ = post;
    spectrumValid_ = true;
    spectrumHasFrame_ = true;
}

void DisplayView::timerCallback()
{
    updateDisplayTargets();

    const float inDb  = juce::Decibels::gainToDecibels (processor_.getInputPeak() + 1.0e-9f);
    const float outDb = juce::Decibels::gainToDecibels (processor_.getOutputPeak() + 1.0e-9f);
    meterInDb_  = (inDb  > meterInDb_)  ? inDb  : juce::jmax (meterInDb_  - 1.0f, -60.0f);
    meterOutDb_ = (outDb > meterOutDb_) ? outDb : juce::jmax (meterOutDb_ - 1.0f, -60.0f);

    meterInDbLabel_  = meterInDb_;
    meterOutDbLabel_ = meterOutDb_;

    // Repaint when the picture can actually differ: a new engine frame (or the
    // shaped curve still moving towards it), or a moving meter.
    const bool metersMoved = std::abs (meterInDb_  - lastMeterIn_)  > 0.01f
                          || std::abs (meterOutDb_ - lastMeterOut_) > 0.01f;

    lastMeterIn_  = meterInDb_;
    lastMeterOut_ = meterOutDb_;

    if (spectrumHasFrame_ || metersMoved)
        repaint();
}

void DisplayView::setFloatParam (const juce::String& id, float value)
{
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (processor_.apvts.getParameter (id)))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void DisplayView::setChoiceParam (const juce::String& id, int index)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor_.apvts.getParameter (id)))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
}

void DisplayView::showSettingsMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Theme: Blue");
    menu.addItem (2, "Theme: Red");
    menu.addItem (3, "Theme: Cream");

    menu.showMenuAsync (juce::PopupMenu::Options(), [this] (int r)
    {
        if (r >= 1 && r <= 3)
        {
            processor_.setTheme (r - 1);
            if (auto* laf = dynamic_cast<CrunchLookAndFeel*> (&getLookAndFeel()))
                laf->setTheme (r - 1);
            if (onThemeChanged)
                onThemeChanged();
            repaint();
        }
    });
}

int DisplayView::hitTestBand (juce::Point<float> pos) const
{
    float bestDist = 18.0f;
    int   best = -1;

    for (int i = 0; i < Param::kMaxBands; ++i)
    {
        const float enabled = processor_.apvts.getRawParameterValue (Param::bandEnabled (i))->load();
        if (enabled < 0.5f)
            continue;

        const float freq = processor_.apvts.getRawParameterValue (Param::bandFreq (i))->load();
        const float gain = processor_.apvts.getRawParameterValue (Param::bandGain (i))->load();

        const juce::Point<float> node (freqToX (freq), dbToY (gain));
        const float dist = pos.getDistanceFrom (node);
        if (dist <= bestDist)
        {
            bestDist = dist;
            best = i;
        }
    }

    return best;
}

void DisplayView::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isRightButtonDown())
    {
        const int band = hitTestBand (e.position);
        if (band >= 0)
            removeBand (band);
        return;
    }

    const int band = hitTestBand (e.position);
    if (band >= 0)
    {
        dragBand_ = band;
        dragging_ = true;
        dragQ_ = e.mods.isCommandDown() || e.mods.isCtrlDown();
        dragStartPos_ = e.position;
        processor_.setSelectedBand (band);
        if (onBandSelected)
            onBandSelected (band);
        repaint();
    }
    else
    {
        dragging_ = false;
        dragBand_ = -1;
        processor_.setSelectedBand (-1);
        if (onBandSelected)
            onBandSelected (-1);
        repaint();
    }
}

void DisplayView::mouseDrag (const juce::MouseEvent& e)
{
    if (!dragging_ || dragBand_ < 0)
        return;

    if (dragStartPos_.getDistanceFrom (e.position) < 3.0f)
        return;

    if (dragQ_)
    {
        const float newQ = xToQ (e.position.x);
        setFloatParam (Param::bandQ (dragBand_), newQ);
    }
    else
    {
        const float newFreq = xToFreq (e.position.x);
        const float newGain = yToDb (e.position.y);
        setFloatParam (Param::bandFreq (dragBand_), newFreq);
        setFloatParam (Param::bandGain (dragBand_), newGain);
    }
}

void DisplayView::mouseUp (const juce::MouseEvent&)
{
    dragging_ = false;
    dragBand_ = -1;
    dragQ_ = false;
}

void DisplayView::addBandAt (juce::Point<float> pos)
{
    int target = -1;
    for (int i = 0; i < Param::kMaxBands; ++i)
    {
        const float enabled = processor_.apvts.getRawParameterValue (Param::bandEnabled (i))->load();
        if (enabled < 0.5f)
        {
            target = i;
            break;
        }
    }
    if (target < 0)
        return;

    const float freq = xToFreq (pos.x);
    const float t = pos.x / (float) plotWidth();

    setFloatParam (Param::bandFreq (target), freq);
    setFloatParam (Param::bandQ (target), 1.0f);

    if (t < 0.08f)
    {
        setChoiceParam (Param::bandType (target), (int) Param::FilterType::LowCut);
        setFloatParam (Param::bandGain (target), 0.0f);
    }
    else if (t > 0.92f)
    {
        setChoiceParam (Param::bandType (target), (int) Param::FilterType::HighCut);
        setFloatParam (Param::bandGain (target), 0.0f);
    }
    else
    {
        setChoiceParam (Param::bandType (target), (int) Param::FilterType::Bell);
        setFloatParam (Param::bandGain (target), yToDb (pos.y));
    }

    if (auto* p = dynamic_cast<juce::AudioParameterBool*> (processor_.apvts.getParameter (Param::bandEnabled (target))))
        p->setValueNotifyingHost (1.0f);

    processor_.setSelectedBand (target);
    if (onBandSelected)
        onBandSelected (target);
}

void DisplayView::removeBand (int index)
{
    if (auto* p = dynamic_cast<juce::AudioParameterBool*> (processor_.apvts.getParameter (Param::bandEnabled (index))))
        p->setValueNotifyingHost (0.0f);

    int newSel = -1;
    for (int i = 0; i < Param::kMaxBands; ++i)
    {
        const float enabled = processor_.apvts.getRawParameterValue (Param::bandEnabled (i))->load();
        if (enabled >= 0.5f)
        {
            newSel = i;
            break;
        }
    }

    processor_.setSelectedBand (newSel);
    if (onBandSelected)
        onBandSelected (newSel);
}

void DisplayView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.getNumberOfClicks() != 2)
        return;

    const int band = hitTestBand (e.position);
    if (band < 0)
        addBandAt (e.position);
}

void DisplayView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int band = hitTestBand (e.position);
    if (band < 0)
        return;

    const float currentQ = processor_.apvts.getRawParameterValue (Param::bandQ (band))->load();
    const float factor = wheel.deltaY > 0.0f ? 1.1f : (1.0f / 1.1f);
    setFloatParam (Param::bandQ (band), juce::jlimit (Param::kQMin, Param::kQMax, currentQ * factor));
}

void DisplayView::drawGrid (juce::Graphics& g)
{
    g.setColour (bgColour());
    g.fillAll();

    static const float decadeFreqs[] = { 10.0f, 20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
                                         1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };

    g.setColour (gridColour());
    for (float f : decadeFreqs)
    {
        const float x = freqToX (f);
        g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
    }

    const float scale = gainScaleDb();
    float dbStep = scale / 2.0f;
    if (scale >= 12.0f) dbStep = scale / 4.0f;

    g.setColour (gridColour());
    for (float db = dbStep; db <= scale + 0.01f; db += dbStep)
    {
        g.drawHorizontalLine ((int) dbToY (db), 0.0f, (float) plotWidth());
        g.drawHorizontalLine ((int) dbToY (-db), 0.0f, (float) plotWidth());
    }
    g.setColour (CrunchPalette::gridZero (processor_.isLightTheme()));
    g.drawHorizontalLine ((int) dbToY (0.0f), 0.0f, (float) plotWidth());

    g.setFont (CrunchLookAndFeel::uiFont (10.5f, 500));
    g.setColour (textColour());
    for (float f : decadeFreqs)
    {
        juce::String label;
        if (f >= 1000.0f)
            label = juce::String ((int) (f / 1000.0f)) + "k";
        else
            label = juce::String ((int) f);

        const int labelW = 34;
        int labelX = (int) freqToX (f) + 3;
        if (f >= 10000.0f)
            labelX = (int) freqToX (f) - labelW - 3;
        g.drawText (label, labelX, getHeight() - 16, labelW, 14, juce::Justification::left, false);
    }
}

void DisplayView::drawDbScale (juce::Graphics& g)
{
    const float scale = gainScaleDb();
    float dbStep = scale / 2.0f;
    if (scale >= 12.0f) dbStep = scale / 4.0f;

    g.setFont (CrunchLookAndFeel::uiFont (10.5f, 500));
    g.setColour (textColour());

    auto drawLabel = [&] (float db)
    {
        const int y = (int) dbToY (db);
        juce::String label = (db > 0.0f ? "+" : "") + juce::String (db, 0) + " dB";
        g.drawText (label, 4, y - 8, 52, 16, juce::Justification::left, false);
    };

    for (float db = -scale; db <= scale + 0.01f; db += dbStep)
        drawLabel (db);
}

void DisplayView::drawSpectrum (juce::Graphics& g)
{
    if (spectrumFreqs_.size() != (size_t) kPoints || ! spectrumValid_)
        return;

    // Paths are built straight from the published analyser points: the previous
    // version's spectrum look (no extra shaping, no peak-hold line).
    const auto makePath = [&] (const std::array<float, kPoints>& db) -> juce::Path
    {
        juce::Path p;
        bool started = false;
        for (int i = 0; i < kPoints; ++i)
        {
            const float x = freqToX (spectrumFreqs_[(size_t) i]);
            const float y = spectrumDbToY (juce::jlimit (-140.0f, 60.0f, db[(size_t) i]));
            if (!started) { p.startNewSubPath (x, y); started = true; }
            else p.lineTo (x, y);
        }
        return p;
    };

    const auto fillUnder = [&] (juce::Graphics& gr, const juce::Path& p, juce::Colour top, juce::Colour bottom)
    {
        juce::Path filled (p);
        filled.lineTo (freqToX (spectrumFreqs_.back()), (float) getHeight());
        filled.lineTo (freqToX (spectrumFreqs_.front()), (float) getHeight());
        filled.closeSubPath();
        gr.setGradientFill (juce::ColourGradient (top, 0.0f, 0.0f, bottom, 0.0f, (float) getHeight(), false));
        gr.fillPath (filled);
    };

    const int mode = (int) processor_.apvts.getRawParameterValue (Param::analyzerMode)->load();
    const bool light = processor_.isLightTheme();

    // Layer order (bottom -> top):
    //   1. grey  : the ORIGINAL DRY signal (input, before EQ + colour)
    //   2. blue  : the FINAL OUTPUT (after EQ + colour), filled with the accent
    //   3. white : a stroke curve on top of the blue output layer
    // Drawing the dry layer first means EQ cuts show grey sticking out beyond
    // the blue, and boosts show blue beyond the grey.
    const auto dry = CrunchPalette::inputFill (light);

    if (mode == 1 || mode == 3)
    {
        fillUnder (g, makePath (spectrumPre_), dry.withAlpha (0.42f), dry.withAlpha (0.08f));
        g.setColour (dry.withAlpha (0.55f));
        g.strokePath (makePath (spectrumPre_), juce::PathStrokeType (1.0f));
    }

    if (mode == 2 || mode == 3)
    {
        fillUnder (g, makePath (spectrumPost_), accentColour().withAlpha (0.65f), accentColour().withAlpha (0.10f));

        // white outline of the output layer
        g.setColour (juce::Colours::white.withAlpha (0.95f));
        g.strokePath (makePath (spectrumPost_), juce::PathStrokeType (2.0f));
    }
}

void DisplayView::updateResponseCurve()
{
    const float sr = processor_.getSampleRate() > 0.0 ? (float) processor_.getSampleRate() : 48000.0f;

    // Only rebuild when a parameter (or the sample rate) actually changed: the
    // curve is a pure function of the band settings, so a static instance does
    // no work at all from frame to frame.
    if (! responseDirty_ && sr == responseSampleRate_)
        return;

    responseDirty_ = false;
    responseSampleRate_ = sr;

    constexpr int points = 320;

    if ((int) responseFreqs_.size() != points)
    {
        responseFreqs_.resize ((size_t) points);
        for (int i = 0; i < points; ++i)
        {
            const float t = (float) i / (float) (points - 1);
            responseFreqs_[(size_t) i] = kFreqMin * std::pow (kFreqMax / kFreqMin, t);
        }
    }

    // 1) Design each enabled band's sections once (not once per point).
    responseSections_.clear();
    for (int b = 0; b < Param::kMaxBands; ++b)
    {
        const float enabled = processor_.apvts.getRawParameterValue (Param::bandEnabled (b))->load();
        if (enabled < 0.5f)
            continue;

        const float type  = processor_.apvts.getRawParameterValue (Param::bandType (b))->load();
        const float freq  = processor_.apvts.getRawParameterValue (Param::bandFreq (b))->load();
        const float gain  = processor_.apvts.getRawParameterValue (Param::bandGain (b))->load();
        const float q     = processor_.apvts.getRawParameterValue (Param::bandQ (b))->load();
        const float slope = processor_.apvts.getRawParameterValue (Param::bandSlope (b))->load();

        for (const auto& c : designRbjSections (static_cast<Param::FilterType> ((int) type),
                                                freq, gain, q,
                                                Param::slopeIndexToDb ((int) slope), sr))
            responseSections_.push_back ({ c.b0, c.b1, c.b2, c.a1, c.a2 });
    }

    // 2) Evaluate the combined magnitude at every plot point. cos/sin are
    //    computed once per point and shared by all sections; the product of
    //    |H| becomes a sum of dB, which also removes the per-point sqrt.
    responseDb_.resize ((size_t) points);

    const size_t numSections = responseSections_.size();
    for (int i = 0; i < points; ++i)
    {
        const double w = juce::MathConstants<double>::twoPi * (double) responseFreqs_[(size_t) i] / (double) sr;
        const double cw = std::cos (w);
        const double sw = std::sin (w);

        double db = 0.0;
        for (size_t s = 0; s < numSections; ++s)
            db += 10.0 * std::log10 (juce::jmax (1.0e-12, sectionMagnitudeSquared (responseSections_[s], cw, sw)));

        responseDb_[(size_t) i] = (float) db;
    }
}

void DisplayView::drawResponseCurve (juce::Graphics& g)
{
    if (responseFreqs_.empty())
        return;

    juce::Path p;
    bool started = false;
    for (size_t i = 0; i < responseFreqs_.size(); ++i)
    {
        const float x = freqToX (responseFreqs_[i]);
        const float y = dbToY (responseDb_[i]);
        if (!started) { p.startNewSubPath (x, y); started = true; }
        else p.lineTo (x, y);
    }

    g.setColour (accentColour());
    g.strokePath (p, juce::PathStrokeType (2.0f));
}

void DisplayView::drawBandNodes (juce::Graphics& g)
{
    const int selected = processor_.getSelectedBand();

    for (int i = 0; i < Param::kMaxBands; ++i)
    {
        const float enabled = processor_.apvts.getRawParameterValue (Param::bandEnabled (i))->load();
        if (enabled < 0.5f)
            continue;

        const float freq = processor_.apvts.getRawParameterValue (Param::bandFreq (i))->load();
        const float gain = processor_.apvts.getRawParameterValue (Param::bandGain (i))->load();
        const float type = processor_.apvts.getRawParameterValue (Param::bandType (i))->load();

        const float x = freqToX (freq);
        const float y = dbToY (gain);
        const bool isCut = ((int) type == (int) Param::FilterType::LowCut || (int) type == (int) Param::FilterType::HighCut);

        if (isCut)
        {
            g.setColour (accentColour().withAlpha (0.4f));
            g.drawVerticalLine ((int) x, 0.0f, (float) getHeight());
        }

        const bool isSel = (i == selected);
        const float radius = isSel ? 9.0f : 7.0f;

        if (isSel)
        {
            g.setColour (CrunchPalette::text (processor_.isLightTheme()));
            g.drawEllipse (x - radius - 2.0f, y - radius - 2.0f, (radius + 2.0f) * 2.0f, (radius + 2.0f) * 2.0f, 1.5f);
        }

        g.setColour (isSel ? accentColour().brighter (0.35f) : accentColour());
        g.fillEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour (CrunchPalette::background (processor_.isLightTheme()));
        g.fillEllipse (x - 3.0f, y - 3.0f, 6.0f, 6.0f);
    }
}

void DisplayView::drawMeters (juce::Graphics& g)
{
    const int x0 = plotWidth();
    const int areaW = getWidth() - x0;
    const int barW = (areaW - 6) / 2;
    const int bar1x = x0 + 2;
    const int bar2x = x0 + 4 + barW;

    const float minDb = -60.0f, maxDb = 0.0f;

    const auto meterY = [&] (float db)
    {
        const float frac = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
        return (1.0f - frac) * (float) getHeight();
    };

    const auto drawMeter = [&] (int x, int w, float db, const juce::String& label, const juce::String& dbText, juce::Colour col)
    {
        g.setColour (CrunchPalette::meterBg (processor_.isLightTheme()));
        g.fillRect (x, 0, w, getHeight());

        g.setColour (gridColour());
        for (float dbTick = -60.0f; dbTick <= 0.0f; dbTick += 20.0f)
            g.drawHorizontalLine ((int) meterY (dbTick), (float) x, (float) (x + w));

        const float y = meterY (juce::jlimit (minDb, maxDb, db));
        g.setColour (col.withAlpha (0.35f));
        g.fillRect (x, (int) y, w, getHeight() - (int) y);
        g.setColour (col);
        g.fillRect (x, (int) y, w, 2);

        g.setFont (CrunchLookAndFeel::uiFont (11.0f, 500));
        g.setColour (textColour());
        g.drawText (label, x, 3, w, 13, juce::Justification::centred, false);

        g.setFont (CrunchLookAndFeel::uiFont (10.0f, 500));
        g.setColour (textColour());
        g.drawText (dbText, x, getHeight() - 16, w, 13, juce::Justification::centred, false);
    };

    const bool light = processor_.isLightTheme();
    drawMeter (bar1x, barW, meterInDb_,  "IN",  juce::String (meterInDbLabel_, 1),  CrunchPalette::inputFill (light));
    drawMeter (bar2x, barW, meterOutDb_, "OUT", juce::String (meterOutDbLabel_, 1), CrunchPalette::outputLevel (light));
}

void DisplayView::paint (juce::Graphics& g)
{
    refreshGridCache();
    g.drawImageAt (gridImage_, 0, 0);

    drawSpectrum (g);
    updateResponseCurve();
    drawResponseCurve (g);
    drawBandNodes (g);
    drawMeters (g);
}

// The background, grid lines and axis labels only change when the component is
// resized, the theme changes or the dB scale changes, so they are rendered once
// into an image and blitted instead of being redrawn (with text layout) on
// every animation frame.
void DisplayView::refreshGridCache()
{
    const float scale = gainScaleDb();
    const bool  light = processor_.isLightTheme();

    if (gridImage_.isValid()
        && gridCacheW_ == getWidth() && gridCacheH_ == getHeight()
        && gridCacheGainScale_ == scale && gridCacheLight_ == light)
        return;

    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    gridImage_ = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
    {
        juce::Graphics gi (gridImage_);
        drawGrid (gi);
        drawDbScale (gi);
    }

    gridCacheW_ = getWidth();
    gridCacheH_ = getHeight();
    gridCacheGainScale_ = scale;
    gridCacheLight_ = light;
}
