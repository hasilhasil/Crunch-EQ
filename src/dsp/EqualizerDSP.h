#pragma once

#include <JuceHeader.h>

#include <array>

#include "EQBand.h"
#include "LinearPhaseFilter.h"
#include "../Parameters.h"

class EqualizerDSP
{
public:
    void prepare (juce::dsp::ProcessSpec spec);
    void reset();
    void process (juce::AudioBuffer<float>& buffer);

    void setSampleRate (float sampleRate);
    void updateBand (int index, Param::FilterType type, float freq, float gain, float q, int slope, bool enabled);
    void setPhaseMode (Param::PhaseMode mode);
    void setLinearQuality (int qualityIndex);

    float getMagnitudeDbAt (float freq) const;
    int getLatencySamples() const;

    Param::PhaseMode getPhaseMode() const noexcept { return phaseMode_; }

private:
    void rebuildLinearPhase();
    float magnitudeLinearAt (float freq) const;
    static int fftSizeForQuality (int qualityIndex);

    std::array<EQBand, Param::kMaxBands> bands_;
    LinearPhaseFilter linearPhase_;

    Param::PhaseMode phaseMode_ = Param::PhaseMode::ZeroLatency;
    int linearQuality_ = 1;
    float sampleRate_ = 48000.0f;
    int numChannels_ = 2;
    bool linearPhaseDirty_ = true;
};
