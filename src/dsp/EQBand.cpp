#include "EQBand.h"

void EQBand::prepare (float sampleRate)
{
    sampleRate_ = sampleRate;
    smoothFactor_ = 1.0f - std::exp (-1.0f / (sampleRate * 0.005f));

    curType_ = targetType_; curFreq_ = targetFreq_; curGain_ = targetGain_;
    curQ_ = targetQ_; curSlope_ = targetSlope_; enabled_ = targetEnabled_;
    coeffType_ = curType_; coeffFreq_ = curFreq_; coeffGain_ = curGain_;
    coeffQ_ = curQ_; coeffSlope_ = curSlope_;
    update();
}

void EQBand::setSampleRate (float sampleRate)
{
    if (sampleRate == sampleRate_)
        return;
    sampleRate_ = sampleRate;
    smoothFactor_ = 1.0f - std::exp (-1.0f / (sampleRate * 0.005f));
    update();
}

void EQBand::setEnabled (bool enabled) { targetEnabled_ = enabled; }
void EQBand::setType (Param::FilterType type) { targetType_ = type; }
void EQBand::setFrequency (float freq) { targetFreq_ = freq; }
void EQBand::setGain (float gain) { targetGain_ = gain; }
void EQBand::setQ (float q) { targetQ_ = q; }
void EQBand::setSlope (int dbPerOct) { targetSlope_ = dbPerOct; }

void EQBand::setPhaseMode (Param::PhaseMode mode)
{
    if (mode == phaseMode_)
        return;
    phaseMode_ = mode;
    update();
}

static BiquadCoefficients designRbjSection (Param::FilterType type, float freq, float gain, float q, float sr)
{
    using FT = Param::FilterType;
    switch (type)
    {
        case FT::Bell:      return BiquadFilter::designPeak (freq, q, gain, sr);
        case FT::LowShelf:  return BiquadFilter::designLowShelf (freq, q, gain, sr);
        case FT::HighShelf: return BiquadFilter::designHighShelf (freq, q, gain, sr);
        case FT::LowCut:    return BiquadFilter::designLowCut (freq, q, sr);
        case FT::HighCut:   return BiquadFilter::designHighCut (freq, q, sr);
        case FT::Notch:     return BiquadFilter::designNotch (freq, q, sr);
        case FT::BandPass:  return BiquadFilter::designBandPass (freq, q, sr);
        default:            return BiquadFilter::designPeak (freq, q, gain, sr);
    }
}

std::vector<BiquadCoefficients> designRbjSections (Param::FilterType type, float freq, float gain,
                                                   float q, int slope, float sr)
{
    using FT = Param::FilterType;
    std::vector<BiquadCoefficients> coeffs;

    if (type == FT::LowCut || type == FT::HighCut)
    {
        const int order = juce::jlimit (1, 8, slope / 6);
        const int numBiquads = order / 2;
        const bool needsFirstOrder = (order % 2) != 0;

        for (int j = 0; j < numBiquads; ++j)
            coeffs.push_back (designRbjSection (type, freq, gain, q, sr));

        if (needsFirstOrder)
        {
            if (type == FT::LowCut)
                coeffs.push_back (BiquadFilter::designFirstOrderLowCut (freq, sr));
            else
                coeffs.push_back (BiquadFilter::designFirstOrderHighCut (freq, sr));
        }
        return coeffs;
    }

    if (type == FT::TiltShelf)
    {
        const float halfGain = gain * 0.5f;
        coeffs.push_back (BiquadFilter::designLowShelf (freq, 0.707f, -halfGain, sr));
        coeffs.push_back (BiquadFilter::designHighShelf (freq, 0.707f, halfGain, sr));
        return coeffs;
    }

    coeffs.push_back (designRbjSection (type, freq, gain, q, sr));
    return coeffs;
}

static juce::dsp::IIR::Coefficients<float>::Ptr designAnalogSection (Param::FilterType type,
                                                                     float freq, float gain, float q, float sr)
{
    using FT = Param::FilterType;
    const auto linearGain = juce::Decibels::decibelsToGain (gain);
    switch (type)
    {
        case FT::Bell:      return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, freq, q, linearGain);
        case FT::LowShelf:  return juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, freq, q, linearGain);
        case FT::HighShelf: return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, freq, q, linearGain);
        case FT::LowCut:    return juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, freq, q);
        case FT::HighCut:   return juce::dsp::IIR::Coefficients<float>::makeLowPass (sr, freq, q);
        case FT::Notch:     return juce::dsp::IIR::Coefficients<float>::makeNotch (sr, freq, q);
        case FT::BandPass:  return juce::dsp::IIR::Coefficients<float>::makeBandPass (sr, freq, q);
        default:            return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, freq, q, linearGain);
    }
}

static int sectionCount (Param::FilterType type, int slope)
{
    using FT = Param::FilterType;
    if (type == FT::LowCut || type == FT::HighCut)
    {
        const int order = juce::jlimit (1, 8, slope / 6);
        return (order / 2) + ((order % 2) != 0 ? 1 : 0);
    }
    if (type == FT::TiltShelf)
        return 2;
    return 1;
}

static juce::dsp::IIR::Coefficients<float>::Ptr makeAnalogCoeff (Param::FilterType type, float freq,
                                                                 float gain, float q, int slope,
                                                                 float sr, int sectionIndex)
{
    using FT = Param::FilterType;
    const bool isCut = (type == FT::LowCut || type == FT::HighCut);
    const int order = isCut ? juce::jlimit (1, 8, slope / 6) : 0;
    const int numBiquads = isCut ? (order / 2) : 1;

    if (type == FT::TiltShelf)
    {
        const float halfGain = gain * 0.5f;
        if (sectionIndex == 0)
            return juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, freq, 0.707f, juce::Decibels::decibelsToGain (-halfGain));
        return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, freq, 0.707f, juce::Decibels::decibelsToGain (halfGain));
    }

    if (isCut && sectionIndex == numBiquads)
    {
        if (type == FT::LowCut)
            return juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sr, freq);
        return juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (sr, freq);
    }

    return designAnalogSection (type, freq, gain, q, sr);
}

void EQBand::update()
{
    const auto rbjCoeffs = designRbjSections (curType_, curFreq_, curGain_, curQ_, curSlope_, sampleRate_);
    const size_t n = rbjCoeffs.size();

    if (rbjSectionsL_.size() != n)
    {
        rbjSectionsL_.clear();
        rbjSectionsR_.clear();
        for (size_t i = 0; i < n; ++i)
        {
            BiquadFilter f;
            f.setCoefficients (rbjCoeffs[i]);
            rbjSectionsL_.push_back (f);
            rbjSectionsR_.push_back (f);
        }
    }
    else
    {
        for (size_t i = 0; i < n; ++i)
        {
            rbjSectionsL_[i].setCoefficients (rbjCoeffs[i]);
            rbjSectionsR_[i].setCoefficients (rbjCoeffs[i]);
        }
    }

    if (phaseMode_ == Param::PhaseMode::NaturalPhase)
    {
        const int total = sectionCount (curType_, curSlope_);
        if ((int) analogSectionsL_.size() != total)
        {
            analogSectionsL_.clear();
            analogSectionsR_.clear();
            for (int s = 0; s < total; ++s)
            {
                juce::dsp::IIR::Filter<float> fl;
                fl.coefficients = makeAnalogCoeff (curType_, curFreq_, curGain_, curQ_, curSlope_, sampleRate_, s);
                analogSectionsL_.push_back (std::move (fl));

                juce::dsp::IIR::Filter<float> fr;
                fr.coefficients = makeAnalogCoeff (curType_, curFreq_, curGain_, curQ_, curSlope_, sampleRate_, s);
                analogSectionsR_.push_back (std::move (fr));
            }
        }
        else
        {
            for (int s = 0; s < total; ++s)
            {
                analogSectionsL_[s].coefficients = makeAnalogCoeff (curType_, curFreq_, curGain_, curQ_, curSlope_, sampleRate_, s);
                analogSectionsR_[s].coefficients = makeAnalogCoeff (curType_, curFreq_, curGain_, curQ_, curSlope_, sampleRate_, s);
            }
        }
    }
    else
    {
        analogSectionsL_.clear();
        analogSectionsR_.clear();
    }
}

void EQBand::processSample (float& left, float& right) noexcept
{
    if (enabled_ != targetEnabled_)
    {
        enabled_ = targetEnabled_;
        if (enabled_)
        {
            curType_ = targetType_; curFreq_ = targetFreq_; curGain_ = targetGain_;
            curQ_ = targetQ_; curSlope_ = targetSlope_;
        }
        update();
        coeffType_ = curType_; coeffFreq_ = curFreq_; coeffGain_ = curGain_;
        coeffQ_ = curQ_; coeffSlope_ = curSlope_;
    }

    if (!enabled_)
        return;

    const bool needSmooth =
        curType_ != targetType_ ||
        curSlope_ != targetSlope_ ||
        std::abs (curFreq_ - targetFreq_) > 0.01f ||
        std::abs (curGain_ - targetGain_) > 0.001f ||
        std::abs (curQ_ - targetQ_) > 0.0001f;

    if (needSmooth)
    {
        curType_ = targetType_;
        curSlope_ = targetSlope_;
        curFreq_ += (targetFreq_ - curFreq_) * smoothFactor_;
        curGain_ += (targetGain_ - curGain_) * smoothFactor_;
        curQ_    += (targetQ_ - curQ_) * smoothFactor_;

        if (curType_ != coeffType_ ||
            curSlope_ != coeffSlope_ ||
            std::abs (curFreq_ - coeffFreq_) > 0.5f ||
            std::abs (curGain_ - coeffGain_) > 0.01f ||
            std::abs (curQ_ - coeffQ_) > 0.0005f)
        {
            coeffType_ = curType_; coeffSlope_ = curSlope_;
            coeffFreq_ = curFreq_; coeffGain_ = curGain_; coeffQ_ = curQ_;
            update();
        }
    }

    if (phaseMode_ == Param::PhaseMode::NaturalPhase)
    {
        for (size_t i = 0; i < analogSectionsL_.size(); ++i)
        {
            left  = analogSectionsL_[i].processSample (left);
            right = analogSectionsR_[i].processSample (right);
        }
    }
    else
    {
        for (size_t i = 0; i < rbjSectionsL_.size(); ++i)
        {
            left  = rbjSectionsL_[i].processSample (left);
            right = rbjSectionsR_[i].processSample (right);
        }
    }
}

float EQBand::magnitudeAt (float freq, float sampleRate) const
{
    if (!targetEnabled_)
        return 1.0f;
    return magnitudeAt (targetType_, targetFreq_, targetGain_, targetQ_, targetSlope_, sampleRate, freq);
}

float EQBand::magnitudeAt (Param::FilterType type, float freq, float gain, float q, int slope,
                           float sr, float atFreq)
{
    float mag = 1.0f;
    for (const auto& c : designRbjSections (type, freq, gain, q, slope, sr))
    {
        BiquadFilter f;
        f.setCoefficients (c);
        mag *= std::abs (f.responseAt (atFreq, sr));
    }
    return mag;
}
