#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Scales.h"

// Shared by every Reggirizer instance in this process (same plugin binary).
struct GroupHub
{
    juce::CriticalSection lock;
    juce::Array<ReggirizerProcessor*> members;
};

namespace
{
    // Settings that follow the group. Mix and Output stay per track.
    const char* const syncedIds[] = { "key", "scale", "speed", "amount", "stabilizer", "sustain", "doubler", "width" };
    constexpr int numSynced = (int) std::size (syncedIds);

    const juce::Identifier groupId { "group" };
}

ReggirizerProcessor::ReggirizerProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Reggirizer", createLayout())
{
    static_assert (numSynced <= 8, "dirty[] is too small");

    for (auto* id : syncedIds)
        apvts.addParameterListener (id, this);

    const juce::ScopedLock sl (hub->lock);
    hub->members.add (this);
}

ReggirizerProcessor::~ReggirizerProcessor()
{
    cancelPendingUpdate();
    for (auto* id : syncedIds)
        apvts.removeParameterListener (id, this);

    const juce::ScopedLock sl (hub->lock);
    hub->members.removeFirstMatchingValue (this);
}

juce::AudioProcessorValueTreeState::ParameterLayout ReggirizerProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    StringArray keys;
    for (auto* k : Scales::keyNames)
        keys.add (k);

    StringArray scales;
    for (auto& s : Scales::all)
        scales.add (s.englishName);

    auto percent = [] { return AudioParameterFloatAttributes().withLabel ("%"); };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keys, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scales, 3));

    // -3..0 = hard lock with look-ahead, 0 = instant, above = glide time.
    NormalisableRange<float> speedRange (-3.0f, 400.0f, 0.1f);
    speedRange.setSkewForCentre (40.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Retune Speed", speedRange, 15.0f,
                                                       AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, percent()));

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "stabilizer", 1 }, "Note Stabilizer",
                                                        StringArray { "None", "Short", "Mid", "Long" }, 1));

    NormalisableRange<float> sustainRange (0.0f, 500.0f, 1.0f);
    sustainRange.setSkewForCentre (100.0f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sustain", 1 }, "Sustain", sustainRange, 80.0f,
                                                       AudioParameterFloatAttributes().withLabel ("ms")));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "doubler", 1 }, "Doubler",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f, percent()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Doubler Width",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 70.0f, percent()));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, percent()));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                                                       NormalisableRange<float> (-18.0f, 12.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "tone", 1 }, "Reference Tone", false));

    // Flanger (per track, not shared by groups).
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "flgOn", 1 }, "Flanger On", false));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { "flgVoices", 1 }, "Flanger Voices", 1, Flanger::maxVoices, 4));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "flgDepth", 1 }, "Flanger Depth",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 25.0f, percent()));
    NormalisableRange<float> rateRange (0.02f, 8.0f, 0.01f);
    rateRange.setSkewForCentre (0.5f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "flgRate", 1 }, "Flanger Rate", rateRange, 0.2f,
                                                       AudioParameterFloatAttributes().withLabel ("Hz")));
    NormalisableRange<float> delayRange (0.5f, 10.0f, 0.01f);
    delayRange.setSkewForCentre (2.5f);
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "flgDelay", 1 }, "Flanger Delay", delayRange, 2.5f,
                                                       AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "flgSpread", 1 }, "Flanger Spread",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f, percent()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "flgFeedback", 1 }, "Flanger Feedback",
                                                       NormalisableRange<float> (-95.0f, 95.0f, 0.1f), 15.0f, percent()));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "flgWet", 1 }, "Flanger Wet",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 30.0f, percent()));
    return layout;
}

bool ReggirizerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void ReggirizerProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const int numCh = juce::jmax (1, getTotalNumOutputChannels());
    engine.prepare (sampleRate, numCh);

    dryBuffer.setSize (numCh, samplesPerBlock);
    monoBuffer.assign ((size_t) samplesPerBlock, 0.0f);
    dryDelay.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) numCh });
    dryDelay.setDelay ((float) engine.getLatencySamples());
    dryDelay.reset();

    doubler.setLevel (apvts.getRawParameterValue ("doubler")->load() * 0.01f);
    doubler.setWidth (apvts.getRawParameterValue ("width")->load() * 0.01f);
    doubler.prepare (sampleRate);

    flanger.setSettings (readFlangerSettings());
    flanger.prepare (sampleRate, numCh);

    mixSmoothed.reset (sampleRate, 0.05);
    gainSmoothed.reset (sampleRate, 0.05);
    toneGain.reset (sampleRate, 0.02);
    mixSmoothed.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load() * 0.01f);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));
    toneGain.setCurrentAndTargetValue (0.0f);
    tonePhase = 0.0;

    setLatencySamples (engine.getLatencySamples());
}

void ReggirizerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    auto param = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    PitchEngine::Settings s;
    s.key = (int) param ("key");
    s.scale = (int) param ("scale");
    s.speedMs = param ("speed");
    s.amount = param ("amount") * 0.01f;
    s.stabilizer = (int) param ("stabilizer");
    s.sustainMs = param ("sustain");
    engine.setSettings (s);

    doubler.setLevel (param ("doubler") * 0.01f);
    doubler.setWidth (param ("width") * 0.01f);
    mixSmoothed.setTargetValue (param ("mix") * 0.01f);
    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (param ("output")));
    toneGain.setTargetValue (param ("tone") > 0.5f ? 0.1f : 0.0f); // about -20 dBFS
    flanger.setSettings (readFlangerSettings());

    if (dryBuffer.getNumChannels() < numCh || dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize (numCh, numSamples, false, false, true);
    if ((int) monoBuffer.size() < numSamples)
        monoBuffer.resize ((size_t) numSamples); // only if the host breaks its promised block size

    // Keep a delay-aligned dry copy for the mix knob.
    for (int c = 0; c < numCh; ++c)
    {
        auto* dry = dryBuffer.getWritePointer (c);
        const auto* in = buffer.getReadPointer (c);
        for (int n = 0; n < numSamples; ++n)
        {
            dryDelay.pushSample (c, in[n]);
            dry[n] = dryDelay.popSample (c);
        }
    }

    engine.process (buffer.getArrayOfWritePointers(), numCh, numSamples);

    // Doubler feeds on the tuned voice and adds two panned copies to it.
    for (int n = 0; n < numSamples; ++n)
    {
        float m = 0.0f;
        for (int c = 0; c < numCh; ++c)
            m += buffer.getSample (c, n);
        monoBuffer[(size_t) n] = m / (float) numCh;
    }
    doubler.process (monoBuffer.data(), buffer.getWritePointer (0),
                     numCh > 1 ? buffer.getWritePointer (1) : nullptr, numSamples);

    // Reference tone: the key's root around C3..B3, for finding the key by ear.
    const double toneHz = 440.0 * std::pow (2.0, (48 + s.key - 69) / 12.0);
    const double toneInc = juce::MathConstants<double>::twoPi * toneHz / currentSampleRate;

    for (int n = 0; n < numSamples; ++n)
    {
        const float mix = mixSmoothed.getNextValue();
        const float gain = gainSmoothed.getNextValue();
        for (int c = 0; c < numCh; ++c)
        {
            auto* out = buffer.getWritePointer (c);
            out[n] = (out[n] * mix + dryBuffer.getSample (c, n) * (1.0f - mix)) * gain;
        }
    }

    // Flanger goes on the finished vocal (tuned + doubles + dry blend).
    flanger.process (buffer.getArrayOfWritePointers(), numCh, numSamples);

    for (int n = 0; n < numSamples; ++n)
    {
        const float tg = toneGain.getNextValue();
        if (tg <= 0.0f && ! toneGain.isSmoothing())
            continue;

        const float tone = tg * (float) std::sin (tonePhase);
        tonePhase += toneInc;
        if (tonePhase > juce::MathConstants<double>::twoPi)
            tonePhase -= juce::MathConstants<double>::twoPi;

        for (int c = 0; c < numCh; ++c)
            buffer.getWritePointer (c)[n] += tone;
    }
}

Flanger::Settings ReggirizerProcessor::readFlangerSettings() const
{
    auto param = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    Flanger::Settings f;
    f.on = param ("flgOn") > 0.5f;
    f.voices = (int) param ("flgVoices");
    f.depth = param ("flgDepth") * 0.01f;
    f.rateHz = param ("flgRate");
    f.delayMs = param ("flgDelay");
    f.spread = param ("flgSpread") * 0.01f;
    f.feedback = param ("flgFeedback") * 0.01f;
    f.wet = param ("flgWet") * 0.01f;
    return f;
}

juce::AudioProcessorEditor* ReggirizerProcessor::createEditor()
{
    return new ReggirizerEditor (*this);
}

void ReggirizerProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (groupId, group.load(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ReggirizerProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            group = juce::jlimit (0, numGroups, (int) tree.getProperty (groupId, 0));
            apvts.replaceState (tree);
        }
}

//==============================================================================
// Groups

void ReggirizerProcessor::parameterChanged (const juce::String& id, float)
{
    // Can arrive on any thread (automation): just mark it and sync on the message thread.
    if (applyingFromGroup || group.load() == 0)
        return;

    for (int i = 0; i < numSynced; ++i)
        if (id == syncedIds[i])
        {
            dirty[(size_t) i] = true;
            triggerAsyncUpdate();
            return;
        }
}

void ReggirizerProcessor::handleAsyncUpdate()
{
    const int g = group.load();
    if (g == 0)
        return;

    const juce::ScopedLock sl (hub->lock);
    for (int i = 0; i < numSynced; ++i)
    {
        if (! dirty[(size_t) i].exchange (false))
            continue;

        const float v = apvts.getParameter (syncedIds[i])->getValue();
        for (auto* other : hub->members)
            if (other != this && other->getGroup() == g)
                other->applyFromGroup (i, v);
    }
}

void ReggirizerProcessor::applyFromGroup (int paramIndex, float normalisedValue)
{
    auto* p = apvts.getParameter (syncedIds[paramIndex]);
    if (juce::approximatelyEqual (p->getValue(), normalisedValue))
        return;

    const juce::ScopedValueSetter<bool> guard (applyingFromGroup, true);
    p->beginChangeGesture();
    p->setValueNotifyingHost (normalisedValue);
    p->endChangeGesture();
}

void ReggirizerProcessor::setGroup (int g)
{
    group = juce::jlimit (0, numGroups, g);
    if (group.load() != 0)
        pullSettingsFromGroup();
}

void ReggirizerProcessor::pullSettingsFromGroup()
{
    const juce::ScopedLock sl (hub->lock);
    for (auto* other : hub->members)
        if (other != this && other->getGroup() == group.load())
        {
            for (int i = 0; i < numSynced; ++i)
                applyFromGroup (i, other->apvts.getParameter (syncedIds[i])->getValue());
            return;
        }
}

int ReggirizerProcessor::countGroupMembers() const
{
    const int g = group.load();
    if (g == 0)
        return 1;

    const juce::ScopedLock sl (hub->lock);
    int count = 0;
    for (auto* m : hub->members)
        count += m->getGroup() == g ? 1 : 0;
    return count;
}

//==============================================================================
juce::Image ReggirizerProcessor::getArtwork() const
{
    const auto encoded = apvts.state.getProperty (artworkId).toString();
    if (encoded.isEmpty())
        return {};

    juce::MemoryBlock data;
    if (! data.fromBase64Encoding (encoded))
        return {};

    return juce::ImageFileFormat::loadFrom (data.getData(), data.getSize());
}

void ReggirizerProcessor::setArtwork (const juce::Image& image)
{
    if (! image.isValid())
    {
        apvts.state.removeProperty (artworkId, nullptr);
        return;
    }

    // Keep the saved state small: cap the longest side at 512 px.
    auto scaled = image;
    constexpr int maxSide = 512;
    const int longest = juce::jmax (image.getWidth(), image.getHeight());
    if (longest > maxSide)
    {
        const float k = (float) maxSide / (float) longest;
        scaled = image.rescaled (juce::jmax (1, juce::roundToInt ((float) image.getWidth() * k)),
                                 juce::jmax (1, juce::roundToInt ((float) image.getHeight() * k)),
                                 juce::Graphics::highResamplingQuality);
    }

    juce::MemoryOutputStream png;
    if (juce::PNGImageFormat().writeImageToStream (scaled, png))
        apvts.state.setProperty (artworkId, png.getMemoryBlock().toBase64Encoding(), nullptr);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ReggirizerProcessor();
}
