#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

namespace Theme
{
    const juce::Colour blue      { 0xff0038b8 }; // flag / tallit blue
    const juce::Colour deepBlue  { 0xff061a4a };
    const juce::Colour navy      { 0xff0a2463 };
    const juce::Colour white     { 0xfff8f9ff };
    const juce::Colour gold      { 0xffd4af37 }; // Torah-crown gold
    const juce::Colour softWhite { 0xb3ffffff };
}

// REGGURRECTION page: Ugandan flag colours.
namespace Uganda
{
    const juce::Colour black    { 0xff111111 };
    const juce::Colour charcoal { 0xff1d1d1d };
    const juce::Colour yellow   { 0xfffcdc04 };
    const juce::Colour red      { 0xffd90000 };
    const juce::Colour white    { 0xfffafafa };
    const juce::Colour grey     { 0xff9aa0a6 };
}

class ReggirizerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ReggirizerLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void drawComboBoxTextWhenNothingSelected (juce::Graphics&, juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

// Shows the note being sung, how far it is being pulled, and a scrolling pitch trace.
class PitchDisplay : public juce::Component, private juce::Timer
{
public:
    explicit PitchDisplay (PitchEngine& e);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    PitchEngine& engine;
    static constexpr int historyLen = 180;
    std::array<float, historyLen> detectedHistory {};
    std::array<float, historyLen> targetHistory {};
    int historyPos = 0;
    float shownCents = 0.0f;
};

// Piano keyboard (C2..C6) with decaying heat bars above it: white = where the
// singer's pitch has been (input), gold = where Reggirizer put it (output).
// Notes in the current scale are marked; the root gets a bigger mark.
class KeyboardHeatmap : public juce::Component, private juce::Timer
{
public:
    KeyboardHeatmap (PitchEngine& e, juce::AudioProcessorValueTreeState& s);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float keyCentreX (int midi, juce::Rectangle<float> area) const;

    static constexpr int lowNote = 36, highNote = 84; // C2..C6
    static constexpr int numNotes = highNote - lowNote + 1;

    PitchEngine& engine;
    juce::AudioProcessorValueTreeState& apvts;
    std::array<float, numNotes> inHeat {}, outHeat {};
    int detectedNote = -1;
};

// A dreidel you can spin. It wobbles, slows down and lands on one of the four
// letters (Nun, Gimel, Hei, Shin); the editor turns the letter into a random
// tuning setting in that letter's style.
class Dreidel : public juce::Component, private juce::Timer
{
public:
    Dreidel();
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void()> onClick;
    int getLetter() const { return letter; }
    void setLetter (int l) { letter = l; angle = -(float) l * juce::MathConstants<float>::halfPi + 0.32f; repaint(); }

private:
    void timerCallback() override;
    int letter = -1;
    float angle = 0.32f;
};

// Full-window pop-up with a big 3D dreidel. Spinning: fast spin-up that eases
// down, precession wobble that grows as it slows, motion blur, a moving
// shadow, then a bounce, a burst of sparks and light rays when it lands.
class DreidelPopup : public juce::Component, private juce::Timer
{
public:
    DreidelPopup();
    void open();
    void close();
    void spin();
    bool isSpinning() const { return spinning; }
    int getResult() const { return result; }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    std::function<void (int letter)> onLanded;

private:
    void timerCallback() override;
    void land();
    void drawDreidel (juce::Graphics&, juce::Point<float> tip, float scale, float angle, float tilt) const;
    void paintCard (juce::Graphics&);
    juce::Rectangle<float> cardBounds() const;
    juce::Rectangle<float> stageBounds() const;
    juce::Rectangle<float> closeBounds() const;

    struct Particle
    {
        juce::Point<float> pos, vel;
        float life, maxLife, size, spin;
        juce::Colour colour;
        bool sparkle;
    };
    std::vector<Particle> particles;
    juce::TextButton spinAgain { "SPIN AGAIN" }, done { "DONE" };
    juce::Random rng;

    bool spinning = false;
    double lastTime = 0.0;
    float t = 0.0f, duration = 5.0f;
    float angleStart = 0.0f, angleEnd = 0.0f, angle = 0.32f, angularSpeed = 0.0f;
    float precession = 0.0f, tiltAmp = 0.0f;
    float landGlow = 0.0f, bounce = 0.0f, rays = 0.0f;
    int result = -1, target = 0;
};

// Live frequency response of the flanger (what the comb filter is doing right
// now) plus where each voice currently is in its sweep.
class FlangerDisplay : public juce::Component, private juce::Timer
{
public:
    explicit FlangerDisplay (ReggirizerProcessor& p);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override { if (isShowing()) repaint(); }
    ReggirizerProcessor& proc;
};

// Header box showing a picture the user loads from disk. Click to choose an
// image; once one is set, click for Change / Remove.
class ArtworkSlot : public juce::Component,
                    private juce::ValueTree::Listener,
                    private juce::AsyncUpdater
{
public:
    explicit ArtworkSlot (ReggirizerProcessor& p);
    ~ArtworkSlot() override;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setAccent (juce::Colour c, juce::Colour fill) { accent = c; accentFill = fill; repaint(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }

private:
    void chooseImage();
    void reload();
    void handleAsyncUpdate() override { reload(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id) override;
    void valueTreeRedirected (juce::ValueTree&) override { triggerAsyncUpdate(); }

    ReggirizerProcessor& proc;
    juce::ValueTree state;
    juce::Image image;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::Colour accent = Theme::blue, accentFill = Theme::blue;
};

class ReggirizerEditor : public juce::AudioProcessorEditor
{
public:
    explicit ReggirizerEditor (ReggirizerProcessor&);
    ~ReggirizerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::String english, hebrew;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void setUpKnob (Knob&, const juce::String& paramId, const juce::String& english, const char* hebrewUtf8);

    ReggirizerProcessor& proc;
    ReggirizerLookAndFeel lnf;

    PitchDisplay display;
    KeyboardHeatmap heatmap;
    ArtworkSlot artwork;
    juce::ComboBox keyBox, scaleBox, stabilizerBox, groupBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAttach, scaleAttach, stabilizerAttach;
    juce::Label scaleHebrew, groupCount;
    juce::TextButton toneButton { "REF TONE" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toneAttach;

    Knob speed, amount, sustain, doubler, width, mix, output;
    std::array<Knob*, 7> knobs { &speed, &amount, &sustain, &doubler, &width, &mix, &output };

    // Flanger page: the knob row switches between tuning and flanger controls.
    void showPage (int page);
    void paintUganda (juce::Graphics&);
    void applyFlangerPreset (int index);
    int currentPage = 0;
    juce::TextButton tuningTab { "TUNING" }, flangerTab { "REGGURRECTION" };
    juce::TextButton flangerOn { "FLANGER OFF" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> flangerOnAttach;
    juce::ComboBox flangerPreset;
    FlangerDisplay flangerDisplay;

    void randomizeTuning (int letter);
    Dreidel dreidel;
    juce::Label dreidelResult;
    DreidelPopup dreidelPopup; // keep last: drawn on top of everything
    Knob fVoices, fDepth, fRate, fDelay, fSpread, fFeedback, fWet;
    std::array<Knob*, 7> flangerKnobs { &fVoices, &fDepth, &fRate, &fDelay, &fSpread, &fFeedback, &fWet };

    struct GroupPoller : juce::Timer
    {
        std::function<void()> onTick;
        void timerCallback() override { onTick(); }
    } groupPoller;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReggirizerEditor)
};
