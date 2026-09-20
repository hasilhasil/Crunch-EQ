#include "SpectrumEngine.h"

SpectrumEngine::SpectrumEngine()
    : juce::Thread ("Crunch EQ Spectrum Engine")
{
    ringPre_.resize ((size_t) fifoPre_.getTotalSize());
    ringPost_.resize ((size_t) fifoPost_.getTotalSize());
    startThread (juce::Thread::Priority::normal);
}

SpectrumEngine::~SpectrumEngine()
{
    stopThread (2000);
}

void SpectrumEngine::pushSamples (const float* pre, const float* post, int numSamples) noexcept
{
    if (pre == nullptr || post == nullptr || numSamples <= 0)
        return;

    // Single-producer side of the SPSC rings: prepareToWrite is lock free and
    // simply reports how much fits, so a stalled consumer drops samples instead
    // of ever blocking the audio thread.
    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    fifoPre_.prepareToWrite (numSamples, s1, n1, s2, n2);

    if (n1 > 0)
        juce::FloatVectorOperations::copy (ringPre_.data() + s1, pre, n1);
    if (n2 > 0)
        juce::FloatVectorOperations::copy (ringPre_.data() + s2, pre + n1, n2);

    const int written = n1 + n2;

    int t1 = 0, m1 = 0, t2 = 0, m2 = 0;
    fifoPost_.prepareToWrite (written, t1, m1, t2, m2);

    if (m1 > 0)
        juce::FloatVectorOperations::copy (ringPost_.data() + t1, post, m1);
    if (m2 > 0)
        juce::FloatVectorOperations::copy (ringPost_.data() + t2, post + m1, m2);

    fifoPost_.finishedWrite (m1 + m2);
    fifoPre_.finishedWrite (written);
}

void SpectrumEngine::drain (juce::AbstractFifo& fifo, std::vector<float>& ring, SpectrumAnalyzer& analyzer)
{
    const int ready = fifo.getNumReady();
    if (ready <= 0)
        return;

    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    fifo.prepareToRead (ready, s1, n1, s2, n2);

    if (n1 > 0)
        analyzer.pushSamples (ring.data() + s1, n1);

    if (n2 > 0)
        analyzer.pushSamples (ring.data() + s2, n2);

    fifo.finishedRead (n1 + n2);
}

void SpectrumEngine::analyseTick()
{
    const double sr = sampleRate_.load();
    if (sr <= 0.0)
        return;

    if (sr != currentRate_)
    {
        currentRate_ = sr;
        analyzerPre_.prepare (sr, 2048);
        analyzerPost_.prepare (sr, 2048);
        analyzerPre_.setTilt (tilt_.load());
        analyzerPost_.setTilt (tilt_.load());
        // stale published data must not be shown after a rate change
        publishedSlot_.store (-1);
        writeSlot_ = 0;
        uiSlot_    = -1;
        freqs_.clear();
        lastFrameMs_ = 0;
        return;
    }

    const int mode = mode_.load();
    if (mode <= 0)   // analyzer off
        return;

    const bool wantPre  = (mode == 1 || mode == 3);
    const bool wantPost = (mode == 2 || mode == 3);

    // Always buffer whatever arrived, then emit at most one frame per display
    // tick. Emitting every buffered frame at once would produce bursts the UI
    // cannot use (it only ever reads the newest published slot), which is what
    // makes a block-driven host look like low frame rate.
    if (wantPre)
        drain (fifoPre_, ringPre_, analyzerPre_);
    if (wantPost)
        drain (fifoPost_, ringPost_, analyzerPost_);

    const auto nowMs = juce::Time::getMillisecondCounter();
    if (lastFrameMs_ != 0 && nowMs - lastFrameMs_ < kFrameIntervalMs)
        return;
    lastFrameMs_ = nowMs;

    // NOTE: both analysers must be advanced unconditionally - a single
    // short-circuiting expression would starve whichever one sits on the right
    // of the || (the pre analyser produces a frame on almost every tick).
    const bool producedPre  = wantPre  && analyzerPre_.processPendingFrame();
    const bool producedPost = wantPost && analyzerPost_.processPendingFrame();

    if (! (producedPre || producedPost))
        return;

    if (analyzerPre_.getLastFrameDeltaDb()  < 0.01f
     && analyzerPost_.getLastFrameDeltaDb() < 0.01f)
        return;   // converged (silence / steady tone): nothing worth repainting

    scratchFreqs_.clearQuick();
    scratchPre_.clearQuick();
    scratchPost_.clearQuick();

    analyzerPre_.getLevels (scratchFreqs_, scratchPre_, kDisplayPoints);
    analyzerPost_.getLevels (scratchFreqs_, scratchPost_, kDisplayPoints);

    if (scratchPre_.size() != kDisplayPoints || scratchFreqs_.size() != kDisplayPoints)
        return;

    if (freqs_.size() != (size_t) kDisplayPoints)
    {
        freqs_.clear();
        freqs_.reserve ((size_t) kDisplayPoints);
        for (int i = 0; i < kDisplayPoints; ++i)
            freqs_.push_back (scratchFreqs_[(size_t) i]);
    }

    auto& slot = slots_[(size_t) writeSlot_];
    for (int i = 0; i < kDisplayPoints; ++i)
    {
        slot.pre[(size_t) i]  = scratchPre_[(size_t) i];
        slot.post[(size_t) i] = scratchPost_[(size_t) i];
    }

    publishedSlot_.store (writeSlot_, std::memory_order_release);
    writeSlot_ = (writeSlot_ + 1) % 3;
}

void SpectrumEngine::run()
{
    bool wasActive = false;

    while (! threadShouldExit())
    {
        wait (5);   // ~200 wakeups/s; well above the ~60 Hz the analysis needs

        if (threadShouldExit())
            break;

        const bool active = uiActive_.load();

        if (active && ! wasActive)
        {
            // The editor has just been opened. While nobody was watching, the
            // rings filled with audio that is now seconds old; feeding that to
            // the analyser would leave a permanent backlog (production and
            // consumption are balanced, so it would never drain again). Start
            // from the newest audio instead.
            fifoPre_.reset();
            fifoPost_.reset();
        }

        wasActive = active;

        if (! active)
            continue;   // no editor: nothing to analyse for

        analyseTick();
    }
}

bool SpectrumEngine::getLatest (std::array<float, kDisplayPoints>& pre,
                                std::array<float, kDisplayPoints>& post)
{
    const int slot = publishedSlot_.load (std::memory_order_acquire);

    if (slot < 0 || slot == uiSlot_)
        return false;

    uiSlot_ = slot;
    pre  = slots_[(size_t) slot].pre;
    post = slots_[(size_t) slot].post;
    return true;
}

bool SpectrumEngine::getFrequencies (std::vector<float>& freqs)
{
    if (freqs_.empty() || freqs.size() == freqs_.size())
        return false;

    freqs = freqs_;
    return true;
}
