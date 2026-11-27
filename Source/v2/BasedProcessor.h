#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../PitchEngine.h"
#include "BluPill.h"
#include "GrapeCrusher.h"
#include "TrapCity.h"

// 07BASED v2: four effects on a reorderable bus.
//   t2 Reggirizer (pitch correction)   mx Blu Pill (time + volume patterns)
//   hr Grapecrusher (bitcrusher)       td Trap City (distortion + BLAST)
class BasedProcessor : public juce::AudioProcessor,
                       private juce::AudioProcessorValueTreeState::Listener,
                       private juce::AsyncUpdater
{
public:
    BasedProcessor();
    ~BasedProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // ---- things the UI sets that aren't automatable parameters (message thread) ----
    // Bus order as the UI's channel ids, e.g. { "t2", "mx", "hr", "td" }.
    void setOrder (const juce::StringArray& ids);
    juce::StringArray getOrder() const;
    // Blu Pill slots the user has edited: JSON { time: { "16": [ {x,y,k}, ... ] }, vol: { ... } }.
    void setBluEdits (const juce::String& json);
    juce::String getBluEdits() const;
    void setTcPreset (int act, int track);
    juce::String getTcPreset() const;
    void blast() { pendingShots.fetch_add (1); }

    // ---- live readouts for the UI (any thread) ----
    PitchEngine pitch;
    std::atomic<float> barPhase { -1.0f }; // -1 when Blu Pill isn't running

    int getRzLatency() const { return rzLatency; }

private:
    void parameterChanged (const juce::String&, float) override;
    void handleAsyncUpdate() override; // rebuilds the Blu Pill envelopes off the audio thread
    void rebuildEnvelopes();

    // Order packed as 4 two-bit fx indices (0 = t2, 1 = mx, 2 = hr, 3 = td).
    std::atomic<uint32_t> order { 0b11100100 };

    BluPill::Engine blu;
    GrapeCrusher crusher;
    TrapCity trap;
    Gunshot gunshot;
    std::atomic<int> pendingShots { 0 };

    // Envelopes built on the message thread, picked up by the audio thread.
    juce::SpinLock envLock;
    BluPill::Envelope nextTime, nextVol;
    bool envDirty = false;

    // Reggirizer adds latency; when it's switched off the signal goes through
    // a matching delay so the plugin's reported latency never changes.
    int rzLatency = 0;
    juce::AudioBuffer<float> rzBypass;
    int rzBypassPos = 0;

    double sr = 44100.0;
    int maxBlock = 512;
    double freePpq = 0.0; // Blu Pill's own clock when HOLD runs it with the transport stopped
    std::vector<float> phaseBuf;
    std::array<std::atomic<float>*, 12> notePtrs {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasedProcessor)
};
