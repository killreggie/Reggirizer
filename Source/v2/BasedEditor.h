#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "BasedProcessor.h"

// The 07BASED window: the CRT-TV interface (HTML, bundled inside the plugin)
// running in the system web view, wired to the parameters through JUCE's
// web relays. The UI's own controls stay the source of truth for how it looks;
// the relays just keep them and the host in sync both ways.
class BasedEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit BasedEditor (BasedProcessor&);
    ~BasedEditor() override;

    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }

    static constexpr int baseWidth = 900, baseHeight = 840;

private:
    void timerCallback() override;
    std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    BasedProcessor& proc;

    std::vector<std::unique_ptr<juce::WebSliderRelay>> sliderRelays;
    std::vector<std::unique_ptr<juce::WebToggleButtonRelay>> toggleRelays;
    std::vector<std::unique_ptr<juce::WebComboBoxRelay>> comboRelays;
    std::unique_ptr<juce::WebBrowserComponent> web;
    std::vector<std::unique_ptr<juce::WebSliderParameterAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<juce::WebToggleButtonParameterAttachment>> toggleAttachments;
    std::vector<std::unique_ptr<juce::WebComboBoxParameterAttachment>> comboAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BasedEditor)
};
