#pragma once

#include <JuceHeader.h>

#include <vector>

#include "BiquadFilter.h"
#include "../Parameters.h"

std::vector<BiquadCoefficients> designRbjSections (Param::FilterType type, float freq, float gain,
                                                   float q, int slope, float sr);

class EQBand
{
public:
    void prepare (float sampleRate);
    void setSampleRate (float sampleRate);
    void setEnabled (bool enabled);
    void setType (Param::FilterType type);
    void setFrequency (float freq);
    void setGain (float gain);
    void setQ (float q);
    void setSlope (int dbPerOct);
    void setPhaseMode (Param::PhaseMode mode);

    void processSample (float& left, float& right) noexcept;

    float magnitudeAt (float freq, float sampleRate) const;
    static float magnitudeAt (Param::FilterType type, float freq, float gain, float q, int slope,
                              float sr, float atFreq);

    bool isEnabled() const noexcept               { return targetEnabled_; }
    Param::FilterType getType() const noexcept    { return targetType_; }
    float getFrequency() const noexcept           { return targetFreq_; }
    float getGain() const noexcept                { return targetGain_; }
    float getQ() const noexcept                   { return targetQ_; }

private:
    void update();

    // target values set by the UI / processor
    Param::FilterType targetType_ = Param::FilterType::Bell;
    float targetFreq_ = Param::kFreqDefault;
    float targetGain_ = Param::kGainDefault;
    float targetQ_    = Param::kQDefault;
    int   targetSlope_ = 12;
    bool  targetEnabled_ = false;

    // smoothed values actually driving the filter
    Param::FilterType curType_ = Param::FilterType::Bell;
    float curFreq_ = Param::kFreqDefault;
    float curGain_ = Param::kGainDefault;
    float curQ_    = Param::kQDefault;
    int   curSlope_ = 12;
    bool  enabled_ = false;

    // values the coefficients were last built from
    Param::FilterType coeffType_ = Param::FilterType::Bell;
    float coeffFreq_ = Param::kFreqDefault;
    float coeffGain_ = Param::kGainDefault;
    float coeffQ_    = Param::kQDefault;
    int   coeffSlope_ = 12;

    float sampleRate_ = 48000.0f;
    float smoothFactor_ = 0.02f;
    Param::PhaseMode phaseMode_ = Param::PhaseMode::ZeroLatency;

    std::vector<BiquadFilter> rbjSectionsL_;
    std::vector<BiquadFilter> rbjSectionsR_;
    std::vector<juce::dsp::IIR::Filter<float>> analogSectionsL_;
    std::vector<juce::dsp::IIR::Filter<float>> analogSectionsR_;
};
