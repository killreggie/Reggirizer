#include "BasedProcessor.h"
#include "BasedParams.h"

#if ! BASED_NO_EDITOR
 #include "BasedEditor.h"
#endif

namespace
{
    const juce::Identifier orderId { "order" }, bluEditsId { "bluEdits" }, tcPresetId { "tcPreset" };

    uint32_t packOrder (const juce::StringArray& ids)
    {
        // Keep only known, unique ids; append any missing ones in default order.
        std::vector<int> seq;
        for (auto& id : ids)
            for (int k = 0; k < 4; ++k)
                if (id == BasedParams::fxIds[(size_t) k] && std::find (seq.begin(), seq.end(), k) == seq.end())
                    seq.push_back (k);
        for (int k = 0; k < 4; ++k)
            if (std::find (seq.begin(), seq.end(), k) == seq.end())
                seq.push_back (k);
        uint32_t packed = 0;
        for (int i = 0; i < 4; ++i)
            packed |= (uint32_t) seq[(size_t) i] << (2 * i);
        return packed;
    }

    BluPill::Envelope envelopeFrom (const juce::var& points, bool volume)
    {
        BluPill::Envelope env;
        if (auto* arr = points.getArray())
            for (auto& p : *arr)
            {
                if (env.size() >= BluPill::maxPoints)
                    break;
                const float yMax = volume ? 1.0f : BluPill::maxBarsBehind;
                env.push_back ({ juce::jlimit (0.0f, 1.0f, (float) p["x"]),
                                 juce::jlimit (0.0f, yMax, (float) p["y"]),
                                 juce::jlimit (-1.0f, 1.0f, (float) p["k"]) });
            }
        return env;
    }
}

BasedProcessor::BasedProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "BASED", createLayout())
{
    apvts.state.setProperty (orderId, "t2,mx,hr,td", nullptr);
    apvts.state.setProperty (bluEditsId, "{}", nullptr);
    apvts.state.setProperty (tcPresetId, "0,6", nullptr);
    for (size_t n = 0; n < 12; ++n)
        notePtrs[n] = apvts.getRawParameterValue ("rz_n" + juce::String ((int) n));
    apvts.addParameterListener ("bp_tslot", this);
    apvts.addParameterListener ("bp_vslot", this);
    rebuildEnvelopes();
}

BasedProcessor::~BasedProcessor()
{
    apvts.removeParameterListener ("bp_tslot", this);
    apvts.removeParameterListener ("bp_vslot", this);
    cancelPendingUpdate();
}

juce::AudioProcessorValueTreeState::ParameterLayout BasedProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& k : BasedParams::knobs)
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { k.id, 1 }, k.name, NormalisableRange<float> (k.min, k.max), k.def));
    for (auto& c : BasedParams::choices)
    {
        StringArray items;
        for (auto* i : c.items)
            items.add (i);
        layout.add (std::make_unique<AudioParameterChoice> (ParameterID { c.id, 1 }, c.name, items, c.def));
    }
    for (auto& t : BasedParams::toggles)
        layout.add (std::make_unique<AudioParameterBool> (ParameterID { t.id, 1 }, t.name, t.def));
    for (auto& i : BasedParams::ints)
        layout.add (std::make_unique<AudioParameterInt> (ParameterID { i.id, 1 }, i.name, i.min, i.max, i.def));
    return layout;
}

bool BasedProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void BasedProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (32, samplesPerBlock);

    pitch.prepare (sr, 2);
    rzLatency = pitch.getLatencySamples();
    trap.prepare (sr, maxBlock);
    setLatencySamples (rzLatency + trap.getLatencySamples()); // constant, whatever is switched on
    rzBypass.setSize (2, std::max (1, rzLatency)); // a ring of exactly the engine's latency
    rzBypass.clear();
    rzBypassPos = 0;

    blu.prepare (sr, 6.0); // a bar at 40 BPM, times two bars of look-back
    crusher.prepare (sr);
    gunshot.prepare (sr);
    phaseBuf.assign ((size_t) maxBlock, 0.0f);

    const juce::SpinLock::ScopedLockType lock (envLock);
    envDirty = true; // re-send the current envelopes to the freshly prepared engine
}

void BasedProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = std::min (buffer.getNumChannels(), 2);
    const int total = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, total);

    auto p = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    auto on = [&] (const char* id) { return p (id) > 0.5f; };

    // ---- settings for every effect ----
    PitchEngine::Settings rz;
    rz.speedMs = p ("rz_speed");
    rz.amount = p ("rz_amount") * 0.01f;
    rz.sustainMs = p ("rz_sustain");
    rz.humanize = p ("rz_humanize") * 0.01f;
    rz.stabilizer = (int) p ("rz_stab");
    rz.useMask = true;
    for (size_t n = 0; n < 12; ++n)
        rz.mask[n] = notePtrs[n]->load() > 0.5f;
    pitch.setSettings (rz);

    GrapeCrusher::Settings gc;
    gc.bits = p ("gc_bits"); gc.dither = p ("gc_dither"); gc.noise = p ("gc_noise");
    gc.rate = p ("gc_rate"); gc.jitter = p ("gc_jitter"); gc.smooth = p ("gc_smooth");
    gc.lowCut = p ("gc_lowcut"); gc.highCut = p ("gc_highcut"); gc.tilt = p ("gc_tilt");
    gc.in = p ("gc_in"); gc.mix = p ("gc_mix"); gc.out = p ("gc_out");
    gc.mode = (int) p ("gc_mode");
    gc.antiAlias = on ("gc_aa"); gc.dcFilter = on ("gc_dc"); gc.softClip = on ("gc_soft");
    crusher.setSettings (gc);

    TrapCity::Settings tc;
    tc.boom808 = p ("tc_808"); tc.grit = p ("tc_grit"); tc.drive = p ("tc_drive");
    tc.tone = p ("tc_tone"); tc.mix = p ("tc_mix"); tc.out = p ("tc_out");
    tc.mode = (int) p ("tc_mode");
    trap.setSettings (tc);

    blu.setSmoothing (p ("bp_att"), p ("bp_rel"), p ("bp_tns"));
    if (envLock.tryEnter())
    {
        if (envDirty)
        {
            blu.setEnvelopes (nextTime, nextVol);
            envDirty = false;
        }
        envLock.exit();
    }

    // ---- where are we in the bar? ----
    double bpm = 120.0, ppq = freePpq, qnPerBar = 4.0;
    bool playing = false;
    if (auto* head = getPlayHead())
        if (auto pos = head->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto sig = pos->getTimeSignature()) qnPerBar = juce::jmax (1.0, sig->numerator * 4.0 / juce::jmax (1, sig->denominator));
            playing = pos->getIsPlaying();
            if (playing)
                if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    const bool hold = on ("bp_hold");
    const bool bluActive = on ("on_bp") && (playing || hold);
    const double qnPerSample = bpm / 60.0 / sr;
    const double barSamples = sr * 60.0 / bpm * qnPerBar;

    const uint32_t ord = order.load();
    const int shots = pendingShots.exchange (0);
    for (int s = 0; s < shots; ++s)
        gunshot.trigger();

    for (int start = 0; start < total; start += maxBlock)
    {
        const int n = std::min (maxBlock, total - start);
        float* ptr[2] { buffer.getWritePointer (0, start), numCh > 1 ? buffer.getWritePointer (1, start) : nullptr };

        for (int i = 0; i < n; ++i)
        {
            const double q = ppq + (start + i) * qnPerSample;
            const double ph = std::fmod (q, qnPerBar) / qnPerBar;
            phaseBuf[(size_t) i] = (float) (ph < 0.0 ? ph + 1.0 : ph);
        }

        for (int slot = 0; slot < 4; ++slot)
        {
            switch ((ord >> (2 * slot)) & 3u)
            {
                case 0: // Reggirizer (or its latency-matched bypass)
                {
                    const bool rzOn = on ("on_rz");
                    const int len = rzBypass.getNumSamples();
                    for (int c = 0; c < numCh; ++c)
                    {
                        auto* ring = rzBypass.getWritePointer (c);
                        for (int i = 0, pos = rzBypassPos; i < n; ++i, pos = (pos + 1) % len)
                        {
                            const float in = ptr[c][i];
                            const float delayed = ring[pos];
                            ring[pos] = in;
                            if (! rzOn) ptr[c][i] = delayed;
                        }
                    }
                    rzBypassPos = (rzBypassPos + n) % len;
                    if (rzOn)
                        pitch.process (ptr, numCh, n);
                    break;
                }
                case 1: blu.process (ptr, numCh, n, phaseBuf.data(), barSamples, bluActive); break;
                case 2:
                    gcIn.push (ptr, numCh, n);
                    if (on ("on_gc")) crusher.process (ptr, numCh, n);
                    gcOut.push (ptr, numCh, n);
                    break;
                case 3:
                    tcIn.push (ptr, numCh, n);
                    trap.process (ptr, numCh, n, on ("on_tc"));
                    tcOut.push (ptr, numCh, n);
                    break;
            }
        }

        gunshot.process (ptr, numCh, n);
    }

    if (! playing && hold)
        freePpq += total * qnPerSample;
    if (bluActive && total > 0)
    {
        const double ph = std::fmod (ppq + (total - 1) * qnPerSample, qnPerBar) / qnPerBar;
        barPhase.store ((float) (ph < 0.0 ? ph + 1.0 : ph));
    }
    else
        barPhase.store (-1.0f);

    // Never hand the host a broken sample; note the output peak for the speakers.
    for (int c = 0; c < numCh; ++c)
    {
        auto* d = buffer.getWritePointer (c);
        float peak = 0.0f;
        for (int i = 0; i < total; ++i)
        {
            if (! std::isfinite (d[i]))
                d[i] = 0.0f;
            peak = std::max (peak, std::abs (d[i]));
        }
        if (peak > outPeak[(size_t) c].load (std::memory_order_relaxed))
            outPeak[(size_t) c].store (peak, std::memory_order_relaxed);
    }
    if (numCh == 1)
        outPeak[1].store (outPeak[0].load (std::memory_order_relaxed), std::memory_order_relaxed);
}

// ---------------------------------------------------------------- UI-owned state

void BasedProcessor::setOrder (const juce::StringArray& ids)
{
    order.store (packOrder (ids));
    apvts.state.setProperty (orderId, getOrder().joinIntoString (","), nullptr);
}

juce::StringArray BasedProcessor::getOrder() const
{
    juce::StringArray ids;
    const auto o = order.load();
    for (int i = 0; i < 4; ++i)
        ids.add (BasedParams::fxIds[(o >> (2 * i)) & 3u]);
    return ids;
}

void BasedProcessor::setBluEdits (const juce::String& json)
{
    apvts.state.setProperty (bluEditsId, json, nullptr);
    rebuildEnvelopes();
}

juce::String BasedProcessor::getBluEdits() const { return apvts.state.getProperty (bluEditsId, "{}").toString(); }

void BasedProcessor::setTcPreset (int act, int track)
{
    apvts.state.setProperty (tcPresetId, juce::String (act) + "," + juce::String (track), nullptr);
}

juce::String BasedProcessor::getTcPreset() const { return apvts.state.getProperty (tcPresetId, "0,6").toString(); }

void BasedProcessor::parameterChanged (const juce::String&, float) { triggerAsyncUpdate(); }
void BasedProcessor::handleAsyncUpdate() { rebuildEnvelopes(); }

void BasedProcessor::rebuildEnvelopes()
{
    const int tslot = (int) apvts.getRawParameterValue ("bp_tslot")->load();
    const int vslot = (int) apvts.getRawParameterValue ("bp_vslot")->load();
    const auto edits = juce::JSON::parse (getBluEdits());

    auto build = [&] (bool volume, int slot)
    {
        auto env = envelopeFrom (edits[volume ? "vol" : "time"][juce::Identifier (juce::String (slot))], volume);
        return env.size() >= 2 ? env : BluPill::preset (volume, slot);
    };
    auto t = build (false, tslot), v = build (true, vslot);

    const juce::SpinLock::ScopedLockType lock (envLock);
    nextTime = std::move (t);
    nextVol = std::move (v);
    envDirty = true;
}

void BasedProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void BasedProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            order.store (packOrder (juce::StringArray::fromTokens (apvts.state.getProperty (orderId, "t2,mx,hr,td").toString(), ",", "")));
            rebuildEnvelopes();
        }
}

juce::AudioProcessorEditor* BasedProcessor::createEditor()
{
#if BASED_NO_EDITOR
    return nullptr;
#else
    return new BasedEditor (*this);
#endif
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BasedProcessor();
}
