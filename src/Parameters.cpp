#include "Parameters.h"

namespace Param
{
    juce::StringArray getFilterTypeNames()
    {
        return juce::StringArray {
            "Bell",
            "Low Shelf",
            "High Shelf",
            "Low Cut",
            "High Cut",
            "Notch",
            "Band Pass",
            "Tilt Shelf"
        };
    }

    int slopeDbToIndex (int dbPerOct)
    {
        switch (dbPerOct)
        {
            case 6:  return 0;
            case 12: return 1;
            case 18: return 2;
            case 24: return 3;
            case 30: return 4;
            case 36: return 5;
            case 48: return 6;
        }
        return 1;
    }

    int slopeIndexToDb (int index)
    {
        static const int slopes[] = { 6, 12, 18, 24, 30, 36, 48 };
        index = juce::jlimit (0, 6, index);
        return slopes[index];
    }

    static juce::NormalisableRange<float> freqRange()
    {
        juce::NormalisableRange<float> r (kFreqMin, kFreqMax, 1.0f);
        r.setSkewForCentre (kFreqDefault);
        return r;
    }

    static juce::NormalisableRange<float> qRange()
    {
        juce::NormalisableRange<float> r (kQMin, kQMax, 0.01f);
        r.setSkewForCentre (kQDefault);
        return r;
    }

    static juce::StringArray slopeNames()
    {
        return juce::StringArray { "6 dB/oct", "12 dB/oct", "18 dB/oct", "24 dB/oct",
                                   "30 dB/oct", "36 dB/oct", "48 dB/oct" };
    }

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { inputGain, 1 }, "Input Gain",
            juce::NormalisableRange<float> (kIoGainMin, kIoGainMax, 0.1f), 0.0f, "dB"));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { outputGain, 1 }, "Output Gain",
            juce::NormalisableRange<float> (kIoGainMin, kIoGainMax, 0.1f), 0.0f, "dB"));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { phaseMode, 1 }, "Phase Mode",
            juce::StringArray { "Zero Latency", "Natural Phase", "Linear Phase" }, 0));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { linearQuality, 1 }, "Linear Phase Quality",
            juce::StringArray { "Low", "Medium", "Maximum" }, 1));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { gainScale, 1 }, "Gain Scale",
            juce::StringArray { "3 dB", "6 dB", "12 dB", "30 dB" }, 2));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { analyzerMode, 1 }, "Analyzer",
            juce::StringArray { "Off", "Pre", "Post", "Pre + Post" }, 3));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { analyzerTilt, 1 }, "Analyzer Tilt",
            juce::NormalisableRange<float> (0.0f, 4.5f, 0.1f), 4.5f, "dB/oct"));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { autoGain, 1 }, "Auto Gain", false));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { colorMode, 1 }, "Color Mode",
            juce::StringArray { "Off", "Warm", "Cold", "Clip" }, 0));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { colorAmount, 1 }, "Color Amount",
            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, "%"));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { colorPosition, 1 }, "Color Position",
            juce::StringArray { "Pre", "Post" }, 1));

        for (int i = 0; i < kMaxBands; ++i)
        {
            layout.add (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { bandEnabled (i), 1 }, "Band " + juce::String (i + 1) + " Enabled",
                false));

            layout.add (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { bandType (i), 1 }, "Band " + juce::String (i + 1) + " Type",
                getFilterTypeNames(), 0));

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { bandFreq (i), 1 }, "Band " + juce::String (i + 1) + " Freq",
                freqRange(), kFreqDefault, "Hz"));

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { bandGain (i), 1 }, "Band " + juce::String (i + 1) + " Gain",
                juce::NormalisableRange<float> (kGainMin, kGainMax, 0.01f), kGainDefault, "dB"));

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { bandQ (i), 1 }, "Band " + juce::String (i + 1) + " Q",
                qRange(), kQDefault, ""));

            layout.add (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { bandSlope (i), 1 }, "Band " + juce::String (i + 1) + " Slope",
                slopeNames(), 1));
        }

        return layout;
    }
}
