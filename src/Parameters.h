#pragma once

#include <JuceHeader.h>

namespace Param
{
    constexpr int kMaxBands = 24;

    constexpr float kFreqMin     = 20.0f;
    constexpr float kFreqMax     = 20000.0f;
    constexpr float kFreqDefault = 1000.0f;
    constexpr float kGainMin     = -30.0f;
    constexpr float kGainMax     = 30.0f;
    constexpr float kGainDefault = 0.0f;
    constexpr float kQMin        = 0.025f;
    constexpr float kQMax        = 40.0f;
    constexpr float kQDefault    = 1.0f;
    constexpr float kIoGainMin   = -36.0f;
    constexpr float kIoGainMax   = 36.0f;

    enum class FilterType
    {
        Bell,
        LowShelf,
        HighShelf,
        LowCut,
        HighCut,
        Notch,
        BandPass,
        TiltShelf,
        numTypes
    };

    enum class PhaseMode
    {
        ZeroLatency,
        NaturalPhase,
        LinearPhase
    };

    enum class ColorMode
    {
        Off,
        Warm,
        Cold
    };

    inline const juce::String inputGain      = "inputGain";
    inline const juce::String outputGain     = "outputGain";
    inline const juce::String phaseMode      = "phaseMode";
    inline const juce::String linearQuality  = "linearQuality";
    inline const juce::String gainScale      = "gainScale";
    inline const juce::String analyzerMode   = "analyzerMode";
    inline const juce::String analyzerTilt   = "analyzerTilt";
    inline const juce::String autoGain       = "autoGain";
    inline const juce::String colorMode      = "colorMode";
    inline const juce::String colorAmount    = "colorAmount";
    inline const juce::String colorPosition  = "colorPosition";

    inline juce::String bandEnabled (int i) { return "band" + juce::String(i) + "Enabled"; }
    inline juce::String bandType    (int i) { return "band" + juce::String(i) + "Type"; }
    inline juce::String bandFreq    (int i) { return "band" + juce::String(i) + "Freq"; }
    inline juce::String bandGain    (int i) { return "band" + juce::String(i) + "Gain"; }
    inline juce::String bandQ       (int i) { return "band" + juce::String(i) + "Q"; }
    inline juce::String bandSlope   (int i) { return "band" + juce::String(i) + "Slope"; }

    juce::StringArray getFilterTypeNames();
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    int slopeDbToIndex (int dbPerOct);
    int slopeIndexToDb (int index);
}
