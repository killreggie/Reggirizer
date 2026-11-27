#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "PitchEngine.h"
#include "Doubler.h"
#include "Flanger.h"

struct GroupHub;

class ReggirizerProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener,
                            private juce::AsyncUpdater
{
public:
    ReggirizerProcessor();
    ~ReggirizerProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    PitchEngine engine;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    // User-chosen header artwork, stored in the plugin state as a base64 PNG so it
    // travels with the DAW session/preset. Message thread only.
    static inline const juce::Identifier artworkId { "artwork" };
    juce::Image getArtwork() const;
    void setArtwork (const juce::Image&); // pass an invalid Image to clear

    // Instance groups: every Reggirizer loaded in the same DAW process with the
    // same group letter shares key, scale and tuning settings. 0 = no group.
    static constexpr int numGroups = 4;
    int getGroup() const { return group.load(); }
    void setGroup (int g);          // message thread; joining copies the group's settings
    int countGroupMembers() const;  // including this one

    float getFlangerPhase() const { return flanger.lfoDisplay.load(); }

private:
    void parameterChanged (const juce::String& id, float) override;
    void handleAsyncUpdate() override;
    void applyFromGroup (int paramIndex, float normalisedValue);
    void pullSettingsFromGroup();
    Flanger::Settings readFlangerSettings() const;

    juce::AudioBuffer<float> dryBuffer;
    std::vector<float> monoBuffer;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 8192 };
    juce::SmoothedValue<float> mixSmoothed, gainSmoothed, toneGain;
    Doubler doubler;
    Flanger flanger;
    double tonePhase = 0.0;
    double currentSampleRate = 44100.0;

    juce::SharedResourcePointer<GroupHub> hub;
    std::atomic<int> group { 0 };
    std::array<std::atomic<bool>, 8> dirty {};
    bool applyingFromGroup = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReggirizerProcessor)
};
