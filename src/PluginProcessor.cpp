#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ui/LookAndFeel.h"

ProQ3CloneAudioProcessor::ProQ3CloneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", Param::createParameterLayout())
{
    for (int i = 0; i < Param::kMaxBands; ++i)
    {
        rawEnabled_[i] = apvts.getRawParameterValue (Param::bandEnabled (i));
        rawType_[i]    = apvts.getRawParameterValue (Param::bandType (i));
        rawFreq_[i]    = apvts.getRawParameterValue (Param::bandFreq (i));
        rawGain_[i]    = apvts.getRawParameterValue (Param::bandGain (i));
        rawQ_[i]       = apvts.getRawParameterValue (Param::bandQ (i));
        rawSlope_[i]   = apvts.getRawParameterValue (Param::bandSlope (i));
    }

    rawPhaseMode_     = apvts.getRawParameterValue (Param::phaseMode);
    rawLinearQuality_ = apvts.getRawParameterValue (Param::linearQuality);
    rawInputGain_     = apvts.getRawParameterValue (Param::inputGain);
    rawOutputGain_    = apvts.getRawParameterValue (Param::outputGain);
    rawColorMode_     = apvts.getRawParameterValue (Param::colorMode);
    rawColorAmount_   = apvts.getRawParameterValue (Param::colorAmount);
    rawColorPosition_ = apvts.getRawParameterValue (Param::colorPosition);
}

ProQ3CloneAudioProcessor::~ProQ3CloneAudioProcessor() = default;

bool ProQ3CloneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    return true;
}

void ProQ3CloneAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumInputChannels();

    eq_.prepare (spec);
    preDelay_.prepare (kMaxDisplayDelay);
    color_.prepare ((float) sampleRate);
    cacheValid_ = false;
    latencyDirty_ = true;

    spectrumEngine_.setSampleRate (sampleRate);
}


void ProQ3CloneAudioProcessor::releaseResources()
{
    eq_.reset();
}

void ProQ3CloneAudioProcessor::updateParameters()
{
    const float inDb = rawInputGain_->load();
    if (!cacheValid_ || inDb != cachedInGain_) { cachedInGain_ = inDb; }

    const float outDb = rawOutputGain_->load();
    if (!cacheValid_ || outDb != cachedOutGain_) { cachedOutGain_ = outDb; }

    const float phase = rawPhaseMode_->load();
    if (!cacheValid_ || phase != cachedPhase_)
    {
        cachedPhase_ = phase;
        eq_.setPhaseMode (static_cast<Param::PhaseMode> ((int) phase));
        latencyDirty_ = true;
    }

    const float quality = rawLinearQuality_->load();
    if (!cacheValid_ || quality != cachedQuality_)
    {
        cachedQuality_ = quality;
        eq_.setLinearQuality ((int) quality);
        latencyDirty_ = true;
    }

    const float colorMode = rawColorMode_->load();
    if (!cacheValid_ || colorMode != cachedColorMode_)
    {
        cachedColorMode_ = colorMode;
        color_.setMode ((int) colorMode);
    }

    const float colorAmount = rawColorAmount_->load();
    if (!cacheValid_ || colorAmount != cachedColorAmount_)
    {
        cachedColorAmount_ = colorAmount;
        color_.setAmount (colorAmount / 100.0f);
    }

    for (int i = 0; i < Param::kMaxBands; ++i)
    {
        const float type    = rawType_[i]->load();
        const float freq    = rawFreq_[i]->load();
        const float gain    = rawGain_[i]->load();
        const float q       = rawQ_[i]->load();
        const float slope   = rawSlope_[i]->load();
        const float enabled = rawEnabled_[i]->load();

        auto& c = bandCache_[(size_t) i];
        if (!cacheValid_ || type != c.type || freq != c.freq || gain != c.gain
            || q != c.q || slope != c.slope || enabled != c.enabled)
        {
            c.type = type; c.freq = freq; c.gain = gain; c.q = q; c.slope = slope; c.enabled = enabled;
            eq_.updateBand (i,
                            static_cast<Param::FilterType> ((int) type),
                            freq, gain, q,
                            Param::slopeIndexToDb ((int) slope),
                            enabled > 0.5f);
        }
    }

    cacheValid_ = true;
}

void ProQ3CloneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    // Mono copies for the display. The scratch buffers are members so the audio
    // thread never allocates; they are written to the lock-free analysis rings.
    // Mono copies for the display. The scratch buffers are members so the audio
    // thread never allocates; they are written to the lock-free analysis rings.
    analysisScratchPre_.resize ((size_t) jmax (1, numSamples));
    analysisScratchPost_.resize ((size_t) jmax (1, numSamples));

    float inPeak = 0.0f;
    for (int s = 0; s < numSamples; ++s)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            sum += buffer.getReadPointer (ch)[s];

        // The linear-phase engine delays the post tap (and the output meter)
        // by getLatencySamples(); run the pre tap (and this peak) through the
        // same delay so the analyser layers and both meters stay time-aligned.
        // In zero-latency / natural-phase mode the line is a passthrough.
        const float pre = preDelay_.process (sum / (float) numChannels);

        analysisScratchPre_[(size_t) s] = pre;
        analysisScratchPost_[(size_t) s] = 0.0f;

        const float a = std::abs (pre);
        if (a > inPeak) inPeak = a;
    }
    inputPeak_.store (inPeak);

    updateParameters();

    if (latencyDirty_)
    {
        const int latency = eq_.getLatencySamples();
        setLatencySamples (latency);
        preDelay_.setLength (latency);   // keep the pre tap aligned with the post tap
        latencyDirty_ = false;
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (cachedInGain_));

    const bool colourPre = ((int) rawColorPosition_->load() == 0);

    const auto applyColour = [&] ()
    {
        float* left  = buffer.getWritePointer (0);
        float* right = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;
        for (int s = 0; s < numSamples; ++s)
        {
            float l = left[s];
            float r = right != nullptr ? right[s] : l;
            color_.processSample (l, r);
            left[s] = l;
            if (right != nullptr)
                right[s] = r;
        }
    };

    if (colourPre)
    {
        applyColour();
        eq_.process (buffer);
    }
    else
    {
        eq_.process (buffer);
        applyColour();
    }

    buffer.applyGain (juce::Decibels::decibelsToGain (cachedOutGain_));

    float outPeak = 0.0f;
    for (int s = 0; s < numSamples; ++s)
    {
        float sum = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float v = buffer.getReadPointer (ch)[s];
            sum += v;
            const float a = std::abs (v);
            if (a > outPeak) outPeak = a;
        }
        analysisScratchPost_[(size_t) s] = sum / (float) numChannels;
    }
    outputPeak_.store (outPeak);

    // Hand the block to the analysis engine (lock-free ring; the FFT itself
    // runs on the engine's own thread at a fixed, display-matched frame rate).
    spectrumEngine_.pushSamples (analysisScratchPre_.data(),
                                 analysisScratchPost_.data(),
                                 numSamples);
}

void ProQ3CloneAudioProcessor::setSelectedBand (int index)
{
    selectedBand_.store (index < 0 ? -1 : juce::jlimit (0, Param::kMaxBands - 1, index));
}

juce::Colour ProQ3CloneAudioProcessor::getThemeAccent() const
{
    return CrunchLookAndFeel::accentForTheme (theme_.load());
}

void ProQ3CloneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void ProQ3CloneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml.get() != nullptr && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* ProQ3CloneAudioProcessor::createEditor()
{
    return new ProQ3CloneAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProQ3CloneAudioProcessor();
}
