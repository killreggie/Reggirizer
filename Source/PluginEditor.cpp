#include "PluginEditor.h"
#include "Scales.h"
#include <complex>

namespace
{
    juce::String heb (const char* utf8) { return juce::String::fromUTF8 (utf8); }

    // Mac fonts, with Windows equivalents (Arial has Hebrew letters; Segoe UI
    // is the closest clean sans to Avenir Next that every Windows PC has).
   #if JUCE_WINDOWS
    constexpr const char* hebrewFace = "Arial";
    constexpr const char* uiFace = "Segoe UI";
   #else
    constexpr const char* hebrewFace = "Arial Hebrew";
    constexpr const char* uiFace = "Avenir Next";
   #endif

    juce::Font hebrewFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (hebrewFace, height, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font uiFont (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (uiFace, height, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Magen David: two interlocking equilateral triangles.
    juce::Path magenDavid (juce::Point<float> centre, float radius)
    {
        juce::Path p;
        for (int t = 0; t < 2; ++t)
        {
            const float offset = t == 0 ? -juce::MathConstants<float>::halfPi
                                        : juce::MathConstants<float>::halfPi;
            for (int i = 0; i < 3; ++i)
            {
                const float a = offset + (float) i * juce::MathConstants<float>::twoPi / 3.0f;
                const auto pt = centre + juce::Point<float> (std::cos (a), std::sin (a)) * radius;
                if (i == 0) p.startNewSubPath (pt);
                else        p.lineTo (pt);
            }
            p.closeSubPath();
        }
        return p;
    }

    void strokeMagenDavid (juce::Graphics& g, juce::Point<float> c, float r, float thickness, juce::Colour col)
    {
        g.setColour (col);
        g.strokePath (magenDavid (c, r), juce::PathStrokeType (thickness, juce::PathStrokeType::mitered));
    }

    // Grey crowned crane (Uganda's national bird), drawn from simple shapes in a
    // unit box and scaled into area. Faces left, standing on one leg.
    void drawCrane (juce::Graphics& g, juce::Rectangle<float> area, float alpha = 1.0f)
    {
        const auto t = juce::AffineTransform::scale (area.getWidth(), area.getHeight()).translated (area.getX(), area.getY());
        auto P = [&] (float x, float y) { return juce::Point<float> (x, y).transformedBy (t); };
        const float u = area.getWidth();

        // Legs
        g.setColour (juce::Colour (0xff3c3c3c).withMultipliedAlpha (alpha));
        g.drawLine ({ P (0.54f, 0.66f), P (0.53f, 0.94f) }, u * 0.022f);
        juce::Path raised;
        raised.startNewSubPath (P (0.60f, 0.66f));
        raised.lineTo (P (0.70f, 0.76f));
        raised.lineTo (P (0.61f, 0.82f));
        g.strokePath (raised, juce::PathStrokeType (u * 0.02f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Tail
        juce::Path tail;
        tail.startNewSubPath (P (0.70f, 0.52f));
        tail.lineTo (P (0.86f, 0.66f));
        tail.lineTo (P (0.70f, 0.64f));
        tail.closeSubPath();
        g.setColour (juce::Colour (0xff2b2b2b).withMultipliedAlpha (alpha));
        g.fillPath (tail);

        // Body and wing
        juce::Path body;
        body.addEllipse (0.36f, 0.43f, 0.40f, 0.24f);
        body.applyTransform (juce::AffineTransform::rotation (0.25f, 0.56f, 0.55f).followedBy (t));
        g.setColour (Uganda::grey.withMultipliedAlpha (alpha));
        g.fillPath (body);
        juce::Path wing;
        wing.addEllipse (0.46f, 0.45f, 0.28f, 0.14f);
        wing.applyTransform (juce::AffineTransform::rotation (0.35f, 0.6f, 0.52f).followedBy (t));
        g.setColour (juce::Colours::white.withMultipliedAlpha (0.85f * alpha));
        g.fillPath (wing);

        // Neck
        juce::Path neck;
        neck.startNewSubPath (P (0.42f, 0.50f));
        neck.quadraticTo (P (0.30f, 0.38f), P (0.33f, 0.24f));
        g.setColour (Uganda::grey.withMultipliedAlpha (alpha));
        g.strokePath (neck, juce::PathStrokeType (u * 0.06f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Crown: a fan of golden bristles
        const auto crownBase = P (0.335f, 0.17f);
        g.setColour (juce::Colour (0xffe8b923).withMultipliedAlpha (alpha));
        for (int i = 0; i < 9; ++i)
        {
            const float a = juce::degreesToRadians (-160.0f + 15.0f * (float) i);
            const auto tip = crownBase + juce::Point<float> (std::cos (a), std::sin (a)) * (u * 0.09f);
            g.drawLine ({ crownBase, tip }, u * 0.012f);
            g.fillEllipse (tip.x - u * 0.012f, tip.y - u * 0.012f, u * 0.024f, u * 0.024f);
        }

        // Head, white cheek, red wattle, beak
        const auto head = P (0.32f, 0.22f);
        g.setColour (juce::Colour (0xff1a1a1a).withMultipliedAlpha (alpha));
        g.fillEllipse (head.x - u * 0.05f, head.y - u * 0.045f, u * 0.10f, u * 0.09f);
        g.setColour (juce::Colours::white.withMultipliedAlpha (alpha));
        g.fillEllipse (head.x - u * 0.005f, head.y - u * 0.03f, u * 0.04f, u * 0.035f);
        g.setColour (Uganda::red.withMultipliedAlpha (alpha));
        g.fillEllipse (head.x - u * 0.035f, head.y + u * 0.035f, u * 0.03f, u * 0.035f);
        juce::Path beak;
        beak.addTriangle (P (0.285f, 0.205f), P (0.18f, 0.245f), P (0.285f, 0.24f));
        g.setColour (juce::Colour (0xff3c3c3c).withMultipliedAlpha (alpha));
        g.fillPath (beak);
    }

    juce::String noteName (int midi)
    {
        if (midi < 0)
            return "-";
        return juce::String (Scales::keyNames[(size_t) (midi % 12)]) + juce::String (midi / 12 - 1);
    }
}

//==============================================================================
ReggirizerLookAndFeel::ReggirizerLookAndFeel()
{
    setColour (juce::ComboBox::textColourId, Theme::blue);
    setColour (juce::ComboBox::arrowColourId, Theme::blue);
    setColour (juce::PopupMenu::backgroundColourId, Theme::white);
    setColour (juce::PopupMenu::textColourId, Theme::navy);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Theme::blue);
    setColour (juce::PopupMenu::highlightedTextColourId, Theme::white);
    setColour (juce::Slider::textBoxTextColourId, Theme::white);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, Theme::white);
}

void ReggirizerLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                              float startAngle, float endAngle, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (6.0f);

    if (slider.getProperties()["uganda"])
    {
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto c = bounds.getCentre();
        const float angle = startAngle + pos * (endAngle - startAngle);
        const float arcR = radius - 4.0f;

        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour (Uganda::white.withAlpha (0.12f));
        g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path value;
        value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setGradientFill (juce::ColourGradient (Uganda::yellow, c.x - radius, c.y, Uganda::red, c.x + radius, c.y, false));
        g.strokePath (value, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float capR = radius * 0.68f;
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillEllipse (c.x - capR, c.y - capR + 3.0f, capR * 2.0f, capR * 2.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2e2e2e), c.x, c.y - capR, juce::Colour (0xff0b0b0b), c.x, c.y + capR, false));
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
        g.setColour (Uganda::yellow);
        g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.5f);

        // Centre: the flag's white disc with a red dot.
        const float discR = capR * 0.32f;
        g.setColour (Uganda::white);
        g.fillEllipse (c.x - discR, c.y - discR, discR * 2.0f, discR * 2.0f);
        g.setColour (Uganda::red);
        g.fillEllipse (c.x - discR * 0.4f, c.y - discR * 0.4f, discR * 0.8f, discR * 0.8f);

        const auto dir = juce::Point<float> (std::sin (angle), -std::cos (angle));
        g.setColour (Uganda::yellow);
        g.drawLine ({ c + dir * (capR * 0.5f), c + dir * (capR - 3.0f) }, 3.0f);
        return;
    }
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = radius - 4.0f;

    // Track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (Theme::white.withAlpha (0.15f));
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Value arc
    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setGradientFill (juce::ColourGradient (Theme::white, c.x - radius, c.y, Theme::gold, c.x + radius, c.y, false));
    g.strokePath (value, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Cap
    const float capR = radius * 0.68f;
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (c.x - capR, c.y - capR + 3.0f, capR * 2.0f, capR * 2.0f);
    g.setGradientFill (juce::ColourGradient (Theme::white, c.x, c.y - capR, juce::Colour (0xffd9e1f5), c.x, c.y + capR, false));
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (Theme::gold);
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.5f);

    strokeMagenDavid (g, c, capR * 0.42f, 1.6f, Theme::blue.withAlpha (0.85f));

    // Pointer
    const auto tip = c + juce::Point<float> (std::sin (angle), -std::cos (angle)) * (capR - 3.0f);
    const auto base = c + juce::Point<float> (std::sin (angle), -std::cos (angle)) * (capR * 0.62f);
    g.setColour (Theme::blue);
    g.drawLine ({ base, tip }, 3.0f);
}

void ReggirizerLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<int> (0, 0, w, h).toFloat().reduced (1.0f);

    if (box.getProperties()["uganda"])
    {
        g.setColour (Uganda::black);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Uganda::yellow.withAlpha (box.hasKeyboardFocus (true) ? 1.0f : 0.7f));
        g.drawRoundedRectangle (r, 6.0f, 1.5f);
        juce::Path arrow;
        const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
        g.setColour (Uganda::yellow);
        g.fillPath (arrow);
        return;
    }
    g.setColour (Theme::white);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.hasKeyboardFocus (true) ? Theme::gold : Theme::blue.withAlpha (0.6f));
    g.drawRoundedRectangle (r, 6.0f, 1.5f);

    juce::Path arrow;
    const float ax = (float) w - 18.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 5.0f, ay - 3.0f, ax + 5.0f, ay - 3.0f, ax, ay + 4.0f);
    g.setColour (Theme::blue);
    g.fillPath (arrow);
}

juce::Font ReggirizerLookAndFeel::getComboBoxFont (juce::ComboBox&) { return uiFont (16.0f, true); }

void ReggirizerLookAndFeel::drawComboBoxTextWhenNothingSelected (juce::Graphics& g, juce::ComboBox& box, juce::Label& label)
{
    g.setColour (box.findColour (juce::ComboBox::textColourId).withMultipliedAlpha (0.6f));
    g.setFont (uiFont (16.0f, true));
    g.drawFittedText (box.getTextWhenNothingSelected(), label.getBounds().reduced (2, 1), label.getJustificationType(), 1);
}
juce::Font ReggirizerLookAndFeel::getPopupMenuFont() { return uiFont (16.0f); }

juce::Label* ReggirizerLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (uiFont (14.0f, true));
    l->setColour (juce::Label::textColourId, Theme::white);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return l;
}

//==============================================================================
PitchDisplay::PitchDisplay (PitchEngine& e) : engine (e)
{
    detectedHistory.fill (-1.0f);
    targetHistory.fill (-1.0f);
    startTimerHz (30);
}

void PitchDisplay::timerCallback()
{
    detectedHistory[(size_t) historyPos] = engine.detectedMidi.load();
    targetHistory[(size_t) historyPos] = (float) engine.targetMidi.load();
    historyPos = (historyPos + 1) % historyLen;
    shownCents += 0.35f * (engine.correctionCents.load() - shownCents);
    repaint();
}

void PitchDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();

    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Theme::white.withAlpha (0.35f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    auto inner = r.reduced (14.0f);
    auto left = inner.removeFromLeft (150.0f);
    inner.removeFromLeft (12.0f);

    // Big note readout
    const int target = engine.targetMidi.load();
    const float hz = engine.detectedHz.load();

    g.setColour (Theme::softWhite);
    g.setFont (uiFont (12.0f, true));
    g.drawText ("TUNING TO", left.removeFromTop (16.0f), juce::Justification::centredLeft);

    g.setColour (target >= 0 ? Theme::white : Theme::white.withAlpha (0.3f));
    g.setFont (uiFont (54.0f, true));
    g.drawText (noteName (target), left.removeFromTop (62.0f), juce::Justification::centredLeft);

    g.setColour (Theme::softWhite);
    g.setFont (uiFont (13.0f));
    g.drawText (hz > 0.0f ? juce::String (hz, 1) + " Hz" : juce::String ("listening..."),
                left.removeFromTop (18.0f), juce::Justification::centredLeft);

    // Correction meter (-100..+100 cents)
    left.removeFromTop (10.0f);
    auto meter = left.removeFromTop (12.0f);
    g.setColour (Theme::white.withAlpha (0.12f));
    g.fillRoundedRectangle (meter, 6.0f);
    const float centreX = meter.getCentreX();
    const float norm = juce::jlimit (-1.0f, 1.0f, shownCents / 100.0f);
    auto fill = norm >= 0 ? juce::Rectangle<float> (centreX, meter.getY(), norm * meter.getWidth() * 0.5f, meter.getHeight())
                          : juce::Rectangle<float> (centreX + norm * meter.getWidth() * 0.5f, meter.getY(), -norm * meter.getWidth() * 0.5f, meter.getHeight());
    g.setColour (Theme::gold);
    g.fillRoundedRectangle (fill, 4.0f);
    g.setColour (Theme::white);
    g.fillRect (centreX - 1.0f, meter.getY() - 3.0f, 2.0f, meter.getHeight() + 6.0f);

    g.setColour (Theme::softWhite);
    g.setFont (uiFont (12.0f));
    g.drawText ((shownCents >= 0 ? "+" : "") + juce::String ((int) std::round (shownCents)) + " cents",
                left.removeFromTop (20.0f), juce::Justification::centredLeft);

    // Pitch trace
    auto trace = inner;
    float centreMidi = 60.0f;
    for (int i = 0; i < historyLen; ++i)
    {
        const float t = targetHistory[(size_t) ((historyPos - 1 - i + historyLen) % historyLen)];
        if (t >= 0.0f) { centreMidi = t; break; }
    }
    const float span = 6.0f; // semitones each side
    auto yFor = [&] (float midi) { return juce::jmap (midi, centreMidi + span, centreMidi - span, trace.getY(), trace.getBottom()); };

    for (int s = -(int) span; s <= (int) span; ++s)
    {
        const float y = yFor (centreMidi + (float) s);
        g.setColour (Theme::white.withAlpha (s == 0 ? 0.25f : 0.07f));
        g.drawHorizontalLine ((int) y, trace.getX(), trace.getRight());
    }

    auto drawHistory = [&] (const std::array<float, historyLen>& h, juce::Colour col, float thickness)
    {
        juce::Path p;
        bool drawing = false;
        for (int i = 0; i < historyLen; ++i)
        {
            const float v = h[(size_t) ((historyPos + i) % historyLen)];
            const float x = trace.getX() + trace.getWidth() * (float) i / (float) (historyLen - 1);
            if (v < 0.0f || std::abs (v - centreMidi) > span) { drawing = false; continue; }
            const float y = yFor (v);
            if (! drawing) { p.startNewSubPath (x, y); drawing = true; }
            else           p.lineTo (x, y);
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    drawHistory (targetHistory, Theme::gold.withAlpha (0.9f), 2.0f);
    drawHistory (detectedHistory, Theme::white, 2.0f);

    g.setFont (uiFont (11.0f));
    g.setColour (Theme::white);
    g.drawText ("your voice", trace.removeFromBottom (14.0f).removeFromRight (150.0f), juce::Justification::centredRight);
    g.setColour (Theme::gold);
    g.drawText ("target note", trace.removeFromBottom (14.0f).removeFromRight (150.0f), juce::Justification::centredRight);
}

//==============================================================================
ArtworkSlot::ArtworkSlot (ReggirizerProcessor& p) : proc (p), state (p.apvts.state)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    state.addListener (this);
    reload();
}

ArtworkSlot::~ArtworkSlot()
{
    state.removeListener (this);
}

void ArtworkSlot::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id)
{
    if (id == ReggirizerProcessor::artworkId)
        triggerAsyncUpdate(); // may arrive off the message thread when a host restores state
}

void ArtworkSlot::reload()
{
    image = proc.getArtwork();
    repaint();
}

void ArtworkSlot::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    constexpr float corner = 8.0f;

    if (image.isValid())
    {
        juce::Path clip;
        clip.addRoundedRectangle (r, corner);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (clip);
            g.drawImage (image, r, juce::RectanglePlacement::fillDestination);
        }
        g.setColour (Theme::gold);
        g.drawRoundedRectangle (r, corner, 2.0f);

        if (isMouseOver())
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillPath (clip);
            g.setColour (Theme::white);
            g.setFont (uiFont (10.0f, true));
            g.drawText ("EDIT", r, juce::Justification::centred);
        }
        return;
    }

    // Empty placeholder: dashed outline with a plus sign.
    g.setColour (accentFill.withAlpha (isMouseOver() ? 0.12f : 0.05f));
    g.fillRoundedRectangle (r, corner);

    juce::Path outline;
    outline.addRoundedRectangle (r, corner);
    const float dashes[] = { 4.0f, 3.0f };
    juce::Path dashed;
    juce::PathStrokeType (1.5f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (accent.withAlpha (0.8f));
    g.fillPath (dashed);

    const auto c = r.getCentre().translated (0.0f, -5.0f);
    g.fillRect (c.x - 7.0f, c.y - 1.25f, 14.0f, 2.5f);
    g.fillRect (c.x - 1.25f, c.y - 7.0f, 2.5f, 14.0f);
    g.setFont (uiFont (8.5f, true));
    g.drawText ("IMAGE", r.withTop (r.getBottom() - 14.0f).translated (0.0f, -2.0f), juce::Justification::centred);
}

void ArtworkSlot::mouseUp (const juce::MouseEvent& e)
{
    if (! getLocalBounds().contains (e.getPosition()))
        return;

    if (! image.isValid())
    {
        chooseImage();
        return;
    }

    juce::PopupMenu menu;
    menu.addItem (1, "Change image...");
    menu.addItem (2, "Remove image");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<ArtworkSlot> (this)] (int result)
                        {
                            if (safe == nullptr)
                                return;
                            if (result == 1)
                                safe->chooseImage();
                            else if (result == 2)
                                safe->proc.setArtwork ({});
                        });
}

void ArtworkSlot::chooseImage()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose an image for Reggirizer",
                                                   juce::File::getSpecialLocation (juce::File::userPicturesDirectory),
                                                   "*.png;*.jpg;*.jpeg;*.gif");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe = juce::Component::SafePointer<ArtworkSlot> (this)] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr)
                                  return;

                              const auto file = fc.getResult();
                              if (file == juce::File())
                                  return; // cancelled

                              const auto loaded = juce::ImageFileFormat::loadFrom (file);
                              if (! loaded.isValid())
                              {
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                                          "Reggirizer",
                                                                          "That file couldn't be opened as an image.");
                                  return;
                              }

                              safe->proc.setArtwork (loaded);
                          });
}

//==============================================================================
KeyboardHeatmap::KeyboardHeatmap (PitchEngine& e, juce::AudioProcessorValueTreeState& s) : engine (e), apvts (s)
{
    startTimerHz (30);
}

void KeyboardHeatmap::timerCallback()
{
    for (auto& h : inHeat)  h *= 0.93f;
    for (auto& h : outHeat) h *= 0.93f;

    auto addHeat = [] (std::array<float, numNotes>& heat, float midi)
    {
        const float pos = midi - (float) lowNote;
        const int lo = (int) std::floor (pos);
        const float frac = pos - (float) lo;
        if (lo >= 0 && lo < numNotes)         heat[(size_t) lo]       = juce::jmin (1.0f, heat[(size_t) lo] + 0.14f * (1.0f - frac));
        if (lo + 1 >= 0 && lo + 1 < numNotes) heat[(size_t) (lo + 1)] = juce::jmin (1.0f, heat[(size_t) (lo + 1)] + 0.14f * frac);
    };

    const float in = engine.detectedMidi.load();
    detectedNote = in > 0.0f ? (int) std::round (in) : -1;
    if (in > 0.0f)
    {
        addHeat (inHeat, in);
        addHeat (outHeat, in + engine.correctionCents.load() * 0.01f);
    }
    repaint();
}

namespace
{
    bool isBlackKey (int midi)
    {
        const int pc = midi % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    int whiteIndex (int midi, int lowNote) // how many white keys from lowNote (a C) to midi
    {
        static constexpr int whitesBefore[12] = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };
        const int rel = midi - lowNote;
        return (rel / 12) * 7 + whitesBefore[rel % 12];
    }
}

float KeyboardHeatmap::keyCentreX (int midi, juce::Rectangle<float> area) const
{
    const float whiteW = area.getWidth() / (float) (whiteIndex (highNote, lowNote) + 1);
    const int wi = whiteIndex (midi, lowNote);
    return area.getX() + (isBlackKey (midi) ? (float) wi * whiteW : ((float) wi + 0.5f) * whiteW);
}

void KeyboardHeatmap::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillRoundedRectangle (r, 10.0f);

    auto inner = r.reduced (10.0f, 6.0f);
    auto legend = inner.removeFromLeft (64.0f);
    inner.removeFromLeft (6.0f);

    g.setFont (uiFont (10.0f, true));
    g.setColour (Theme::white);
    g.fillRoundedRectangle (legend.getX(), legend.getY() + 4.0f, 10.0f, 6.0f, 2.0f);
    g.drawText ("INPUT", legend.withTrimmedLeft (14.0f).withHeight (14.0f), juce::Justification::centredLeft);
    g.setColour (Theme::gold);
    g.fillRoundedRectangle (legend.getX(), legend.getY() + 18.0f, 10.0f, 6.0f, 2.0f);
    g.drawText ("OUTPUT", legend.withTrimmedLeft (14.0f).withY (legend.getY() + 14.0f).withHeight (14.0f), juce::Justification::centredLeft);

    const int key = (int) apvts.getRawParameterValue ("key")->load();
    const int scaleIdx = juce::jlimit (0, (int) Scales::all.size() - 1, (int) apvts.getRawParameterValue ("scale")->load());
    const auto& scale = Scales::all[(size_t) scaleIdx];
    auto inScale = [&] (int midi) { return scale.notes[(size_t) (((midi - key) % 12 + 12) % 12)]; };

    auto heatArea = inner.removeFromTop (inner.getHeight() * 0.36f);
    auto keys = inner.withTrimmedTop (2.0f);
    const float whiteW = keys.getWidth() / (float) (whiteIndex (highNote, lowNote) + 1);

    // Heat bars: input on the top half, output underneath.
    for (int i = 0; i < numNotes; ++i)
    {
        const float x = keyCentreX (lowNote + i, keys);
        const float bw = whiteW * 0.7f;
        const auto top = heatArea.withHeight (heatArea.getHeight() * 0.5f).withX (x - bw * 0.5f).withWidth (bw);
        const auto bottom = top.translated (0.0f, top.getHeight());
        if (inHeat[(size_t) i] > 0.01f)
        {
            g.setColour (Theme::white.withAlpha (inHeat[(size_t) i]));
            g.fillRoundedRectangle (top.reduced (0.0f, 1.0f), 2.0f);
        }
        if (outHeat[(size_t) i] > 0.01f)
        {
            g.setColour (Theme::gold.withAlpha (outHeat[(size_t) i]));
            g.fillRoundedRectangle (bottom.reduced (0.0f, 1.0f), 2.0f);
        }
    }

    // White keys
    for (int m = lowNote; m <= highNote; ++m)
    {
        if (isBlackKey (m))
            continue;
        const auto kr = juce::Rectangle<float> (keys.getX() + (float) whiteIndex (m, lowNote) * whiteW, keys.getY(), whiteW, keys.getHeight()).reduced (0.6f, 0.0f);
        g.setColour (inScale (m) ? Theme::white : Theme::white.withAlpha (0.45f));
        g.fillRoundedRectangle (kr, 2.5f);
    }

    // Black keys
    const float blackW = whiteW * 0.6f, blackH = keys.getHeight() * 0.6f;
    for (int m = lowNote; m <= highNote; ++m)
    {
        if (! isBlackKey (m))
            continue;
        const float cx = keyCentreX (m, keys);
        const auto kr = juce::Rectangle<float> (cx - blackW * 0.5f, keys.getY(), blackW, blackH);
        g.setColour (inScale (m) ? Theme::navy : Theme::deepBlue.withAlpha (0.85f));
        g.fillRoundedRectangle (kr, 2.0f);
    }

    // Scale marks, root mark and the note being sung.
    for (int m = lowNote; m <= highNote; ++m)
    {
        const float cx = keyCentreX (m, keys);
        const float y = isBlackKey (m) ? keys.getY() + blackH - 7.0f : keys.getBottom() - 7.0f;
        if (inScale (m))
        {
            const bool root = (m - key) % 12 == 0;
            const float d = root ? 6.0f : 3.5f;
            g.setColour (isBlackKey (m) ? Theme::gold : (root ? Theme::gold : Theme::blue.withAlpha (0.6f)));
            g.fillEllipse (cx - d * 0.5f, y - d * 0.5f, d, d);
        }
        if (m == detectedNote)
        {
            g.setColour (Theme::blue);
            g.drawEllipse (cx - 6.0f, y - 6.0f, 12.0f, 12.0f, 2.0f);
        }
    }
}

//==============================================================================
namespace
{
    // Nun, Gimel, Hei, Shin
    const char* const dreidelLetters[] = { "\xd7\xa0", "\xd7\x92", "\xd7\x94", "\xd7\xa9" };
    const char* const dreidelNames[] = { "NUN \xc2\xb7 NATURAL", "GIMEL \xc2\xb7 HARD TUNE", "HEI \xc2\xb7 BALANCED", "SHIN \xc2\xb7 WILD CARD" };
}

static void drawDreidel3D (juce::Graphics&, juce::Point<float> tip, float scale, float angle, float tilt);

Dreidel::Dreidel()
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    startTimerHz (30);
}

void Dreidel::timerCallback()
{
    if (! isShowing())
        return;
    // Turns slowly until it has a result to show, inviting a click.
    if (letter < 0 || isMouseOver())
        angle += 0.03f;
    repaint();
}

void Dreidel::mouseUp (const juce::MouseEvent& e)
{
    if (getLocalBounds().contains (e.getPosition()) && onClick)
        onClick();
}

void Dreidel::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float scale = r.getHeight() / 4.7f;
    drawDreidel3D (g, { r.getCentreX(), r.getBottom() - 3.0f }, scale, angle, 0.0f);
}

//==============================================================================
namespace
{
    const char* const dreidelTitles[] = { "NUN", "GIMEL", "HEI", "SHIN" };
    const char* const dreidelBlurbs[] = {
        "Natural \xc2\xb7 gentle, transparent correction",
        "Hard tune \xc2\xb7 instant robotic snap with doubles",
        "Balanced \xc2\xb7 modern pop tuning",
        "Wild card \xc2\xb7 anything goes"
    };

    // Dreidel geometry, in units (scaled to pixels when drawn).
    constexpr float dA = 1.0f;       // half-width of the body
    constexpr float dTip = -1.5f;    // y of the point
    constexpr float dBottom = -0.25f;
    constexpr float dTop = 1.55f;
    constexpr float dHandleTop = 2.6f;
    constexpr float dHandleR = 0.22f;
    constexpr float elevation = 0.38f; // camera looks down ~22 degrees

    struct Vec3
    {
        float x, y, z;
        Vec3 normalised() const { const float l = std::sqrt (x * x + y * y + z * z); return { x / l, y / l, z / l }; }
        float operator* (const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    };

    float smoothstep (float e0, float e1, float x)
    {
        const float v = juce::jlimit (0.0f, 1.0f, (x - e0) / (e1 - e0));
        return v * v * (3.0f - 2.0f * v);
    }
}

DreidelPopup::DreidelPopup()
{
    for (auto* b : { &spinAgain, &done })
    {
        b->setColour (juce::TextButton::buttonColourId, Theme::white.withAlpha (0.1f));
        b->setColour (juce::TextButton::textColourOffId, Theme::white);
        addAndMakeVisible (b);
    }
    spinAgain.setColour (juce::TextButton::buttonColourId, Theme::gold);
    spinAgain.setColour (juce::TextButton::textColourOffId, Theme::deepBlue);
    spinAgain.onClick = [this] { spin(); };
    done.onClick = [this] { close(); };
}

juce::Rectangle<float> DreidelPopup::cardBounds() const
{
    return getLocalBounds().toFloat().withSizeKeepingCentre (580.0f, 500.0f);
}

juce::Rectangle<float> DreidelPopup::stageBounds() const
{
    auto c = cardBounds();
    return { c.getX() + 20.0f, c.getY() + 64.0f, c.getWidth() - 40.0f, 300.0f };
}

juce::Rectangle<float> DreidelPopup::closeBounds() const
{
    auto c = cardBounds();
    return { c.getRight() - 46.0f, c.getY() + 16.0f, 28.0f, 28.0f };
}

void DreidelPopup::resized()
{
    auto c = cardBounds();
    spinAgain.setBounds (juce::Rectangle<float> (c.getCentreX() - 170.0f, c.getBottom() - 54.0f, 160.0f, 34.0f).toNearestInt());
    done.setBounds (juce::Rectangle<float> (c.getCentreX() + 10.0f, c.getBottom() - 54.0f, 160.0f, 34.0f).toNearestInt());
}

void DreidelPopup::open()
{
    setVisible (true);
    toFront (false);
    lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
    startTimerHz (60);
    spin();
}

void DreidelPopup::close()
{
    if (spinning)
        return; // let it land first
    stopTimer();
    particles.clear();
    setVisible (false);
}

void DreidelPopup::spin()
{
    if (spinning)
        return;

    target = rng.nextInt (4);
    const float twoPi = juce::MathConstants<float>::twoPi;
    angle = std::fmod (angle, twoPi);
    angleStart = angle;

    // Face k faces the camera when angle == -k * 90deg; stop a little past
    // square so you can still see it's a 3D block.
    const float finalAngle = -(float) target * juce::MathConstants<float>::halfPi + 0.32f;
    const float turns = (float) (11 + rng.nextInt (5));
    angleEnd = finalAngle + twoPi * std::ceil ((angleStart + turns * twoPi - finalAngle) / twoPi);

    duration = 4.6f + rng.nextFloat() * 0.9f;
    t = 0.0f;
    precession = rng.nextFloat() * twoPi;
    result = -1;
    spinning = true;
    spinAgain.setEnabled (false);
    done.setEnabled (false);
}

void DreidelPopup::land()
{
    spinning = false;
    result = target;
    landGlow = 1.0f;
    bounce = 1.0f;
    spinAgain.setEnabled (true);
    done.setEnabled (true);

    // Burst of sparks from the top of the dreidel.
    const auto stage = stageBounds();
    const juce::Point<float> origin (stage.getCentreX(), stage.getBottom() - 200.0f);
    const juce::Colour palette[] = { Theme::gold, Theme::gold, Theme::white, juce::Colour (0xff8fb4ff), juce::Colour (0xffffe58a) };
    for (int i = 0; i < 110; ++i)
    {
        const float a = juce::MathConstants<float>::twoPi * rng.nextFloat();
        const float speed = 120.0f + 380.0f * std::pow (rng.nextFloat(), 0.6f);
        Particle p;
        p.pos = origin + juce::Point<float> (rng.nextFloat() - 0.5f, rng.nextFloat() - 0.5f) * 40.0f;
        p.vel = { std::cos (a) * speed, std::sin (a) * speed - 160.0f };
        p.maxLife = p.life = 0.9f + rng.nextFloat() * 1.1f;
        p.size = 2.0f + rng.nextFloat() * 4.0f;
        p.spin = (rng.nextFloat() - 0.5f) * 10.0f;
        p.colour = palette[rng.nextInt ((int) std::size (palette))];
        p.sparkle = rng.nextFloat() < 0.35f;
        particles.push_back (p);
    }

    if (onLanded)
        onLanded (result);
}

void DreidelPopup::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.0, 0.05, now - lastTime);
    lastTime = now;

    const float previous = angle;
    if (spinning)
    {
        t += dt;
        const float u = juce::jmin (1.0f, t / duration);
        const float ease = 1.0f - std::pow (1.0f - u, 4.0f); // fast start, long glide down
        angle = angleStart + (angleEnd - angleStart) * ease;

        // Like a real top: steady when fast, wobbling wider as it slows, then
        // it settles upright for the reveal.
        const float grow = 0.025f + 0.27f * u * u * u;
        tiltAmp = grow * (1.0f - smoothstep (0.86f, 1.0f, u));
        precession += dt * (3.5f + 6.5f * u);

        if (u >= 1.0f)
            land();
    }
    else
    {
        tiltAmp *= std::exp (-dt * 6.0f);
    }
    angularSpeed = dt > 0.0f ? std::abs (angle - previous) / dt : 0.0f;

    landGlow = juce::jmax (0.0f, landGlow - dt * 0.55f);
    bounce = juce::jmax (0.0f, bounce - dt * 3.5f);
    rays += dt * 0.35f;

    for (auto& p : particles)
    {
        p.vel.y += 520.0f * dt;
        p.vel *= 1.0f - 0.9f * dt;
        p.pos += p.vel * dt;
        p.life -= dt;
    }
    particles.erase (std::remove_if (particles.begin(), particles.end(), [] (const Particle& p) { return p.life <= 0.0f; }),
                     particles.end());

    repaint();
}

void DreidelPopup::mouseUp (const juce::MouseEvent& e)
{
    const auto pos = e.position;
    if (closeBounds().expanded (4.0f).contains (pos) || ! cardBounds().contains (pos))
        close();
    else if (stageBounds().contains (pos))
        spin();
}

static const std::array<juce::Path, 4>& dreidelLetterPaths()
{
    // The four letters as paths filling a unit box, so they can be mapped onto
    // any face with an affine transform. Built once.
    static const std::array<juce::Path, 4> paths = []
    {
        std::array<juce::Path, 4> out;
        for (int i = 0; i < 4; ++i)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (hebrewFont (100.0f, true), heb (dreidelLetters[i]), 0.0f, 0.0f);
            juce::Path p;
            ga.createPath (p);
            const auto b = p.getBounds();
            const float k = juce::jmin (0.62f / b.getWidth(), 0.62f / b.getHeight());
            p.applyTransform (juce::AffineTransform::translation (-b.getCentreX(), -b.getCentreY()).scaled (k).translated (0.5f, 0.5f));
            out[(size_t) i] = p;
        }
        return out;
    }();
    return paths;
}

void DreidelPopup::drawDreidel (juce::Graphics& g, juce::Point<float> tip, float s, float ang, float tilt) const
{
    drawDreidel3D (g, tip, s, ang, tilt);
}

static void drawDreidel3D (juce::Graphics& g, juce::Point<float> tip, float s, float ang, float tilt)
{
    const auto& letterPaths = dreidelLetterPaths();
    const float ca = std::cos (ang), sa = std::sin (ang);
    const float ce = std::cos (elevation), se = std::sin (elevation);

    // 3D -> screen: spin about the vertical axis, then a camera tilted down.
    auto proj = [&] (float x, float y, float z)
    {
        const float xr = x * ca + z * sa;
        const float zr = -x * sa + z * ca;
        return juce::Point<float> (tip.x + s * xr, tip.y - s * (y - dTip) * ce + s * zr * se);
    };

    // Light from the upper left, in camera space.
    const Vec3 light = Vec3 { -0.45f, 0.55f, 0.7f }.normalised();

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (juce::AffineTransform::rotation (tilt, tip.x, tip.y));

    const juce::Colour faceBase (0xff1d4fd0), edge = Theme::gold.withAlpha (0.9f);
    const float halfPi = juce::MathConstants<float>::halfPi;

    // Point: four triangles.
    for (int k = 0; k < 4; ++k)
    {
        const float alpha = (float) k * halfPi;
        const float c = std::cos (alpha + ang), sn = std::sin (alpha + ang);
        const float hp = dBottom - dTip;
        if (hp * c * ce - dA * se <= 0.0f)
            continue;
        const float dx = std::sin (alpha), dz = std::cos (alpha), tx = std::cos (alpha), tz = -std::sin (alpha);
        juce::Path tri;
        tri.addTriangle (proj (dx * dA - tx * dA, dBottom, dz * dA - tz * dA),
                         proj (dx * dA + tx * dA, dBottom, dz * dA + tz * dA),
                         proj (0.0f, dTip, 0.0f));
        const auto n = Vec3 { sn * hp, -dA, c * hp }.normalised();
        const float shade = 0.35f + 0.55f * juce::jmax (0.0f, n * light);
        g.setColour (faceBase.withMultipliedBrightness (shade * 0.85f));
        g.fillPath (tri);
        g.setColour (edge.withAlpha (0.6f));
        g.strokePath (tri, juce::PathStrokeType (1.2f));
    }

    // Body: the four lettered sides (only the ones facing us).
    for (int k = 0; k < 4; ++k)
    {
        const float alpha = (float) k * halfPi;
        const float c = std::cos (alpha + ang), sn = std::sin (alpha + ang);
        if (c <= 0.0f)
            continue;
        const float dx = std::sin (alpha), dz = std::cos (alpha), tx = std::cos (alpha), tz = -std::sin (alpha);
        const auto tl = proj (dx * dA - tx * dA, dTop, dz * dA - tz * dA);
        const auto tr = proj (dx * dA + tx * dA, dTop, dz * dA + tz * dA);
        const auto br = proj (dx * dA + tx * dA, dBottom, dz * dA + tz * dA);
        const auto bl = proj (dx * dA - tx * dA, dBottom, dz * dA - tz * dA);

        juce::Path face;
        face.startNewSubPath (tl);
        face.lineTo (tr);
        face.lineTo (br);
        face.lineTo (bl);
        face.closeSubPath();

        const float shade = 0.4f + 0.62f * juce::jmax (0.0f, sn * light.x + c * light.z);
        g.setGradientFill (juce::ColourGradient (faceBase.withMultipliedBrightness (shade * 1.15f), tl,
                                                 faceBase.withMultipliedBrightness (shade * 0.8f), br, false));
        g.fillPath (face);

        // Letter, mapped onto the face; slightly sunk in with a dark offset.
        const auto toFace = juce::AffineTransform::fromTargetPoints (0.0f, 0.0f, tl.x, tl.y,
                                                                     1.0f, 0.0f, tr.x, tr.y,
                                                                     0.0f, 1.0f, bl.x, bl.y);
        auto letter = letterPaths[(size_t) k];
        letter.applyTransform (toFace);
        g.setColour (juce::Colours::black.withAlpha (0.3f * c));
        g.fillPath (letter, juce::AffineTransform::translation (1.5f, 1.5f));
        g.setColour (Theme::gold.withMultipliedBrightness (0.75f + 0.35f * shade).withAlpha (juce::jmin (1.0f, 0.4f + c)));
        g.fillPath (letter);

        g.setColour (edge);
        g.strokePath (face, juce::PathStrokeType (s > 30.0f ? 1.5f : 0.8f));
    }

    // Top face.
    {
        juce::Path top;
        top.startNewSubPath (proj (-dA, dTop, -dA));
        top.lineTo (proj (dA, dTop, -dA));
        top.lineTo (proj (dA, dTop, dA));
        top.lineTo (proj (-dA, dTop, dA));
        top.closeSubPath();
        g.setColour (faceBase.withMultipliedBrightness (1.25f));
        g.fillPath (top);
        g.setColour (edge);
        g.strokePath (top, juce::PathStrokeType (1.5f));
    }

    // Handle: a gold cylinder (looks the same from every angle).
    {
        const auto base = proj (0.0f, dTop, 0.0f);
        const auto topC = proj (0.0f, dHandleTop, 0.0f);
        const float r = dHandleR * s, ry = r * se;
        juce::Path stem;
        stem.addRectangle (base.x - r, topC.y, 2.0f * r, base.y - topC.y);
        stem.addCentredArc (base.x, base.y, r, ry, 0.0f, juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi * 3.0f, true);
        juce::ColourGradient metal (Theme::gold.darker (0.6f), base.x - r, 0.0f, Theme::gold.darker (0.5f), base.x + r, 0.0f, false);
        metal.addColour (0.35, Theme::gold.brighter (0.5f));
        metal.addColour (0.55, Theme::gold);
        g.setGradientFill (metal);
        g.fillPath (stem);
        g.fillEllipse (base.x - r, base.y - ry, 2.0f * r, 2.0f * ry);
        g.setColour (Theme::gold.brighter (0.6f));
        g.fillEllipse (topC.x - r, topC.y - ry, 2.0f * r, 2.0f * ry);
        g.setColour (Theme::gold.darker (0.4f));
        g.drawEllipse (topC.x - r, topC.y - ry, 2.0f * r, 2.0f * ry, 1.0f);
    }
}

void DreidelPopup::paint (juce::Graphics& g)
{
    paintCard (g);

    // Result line under the stage.
    const auto card = cardBounds();
    const auto area = juce::Rectangle<float> (card.getX() + 30.0f, stageBounds().getBottom() + 10.0f, card.getWidth() - 60.0f, 58.0f);
    if (spinning || result < 0)
    {
        g.setColour (Theme::white.withAlpha (0.6f));
        g.setFont (uiFont (15.0f, true).withExtraKerningFactor (0.3f));
        g.drawText (spinning ? "SPINNING..." : "CLICK THE DREIDEL TO SPIN", area, juce::Justification::centred);
    }
    else
    {
        const float reveal = 1.0f - landGlow * landGlow; // fades/slides in as the glow settles
        const auto a = area.translated (0.0f, 8.0f * (1.0f - reveal));
        g.setColour (Theme::gold.withAlpha (juce::jmin (1.0f, 0.3f + reveal)));
        g.setFont (hebrewFont (54.0f, true));
        g.drawText (heb (dreidelLetters[result]), a.withWidth (70.0f), juce::Justification::centred);
        g.setColour (Theme::white.withAlpha (juce::jmin (1.0f, 0.3f + reveal)));
        g.setFont (uiFont (22.0f, true).withExtraKerningFactor (0.2f));
        g.drawText (dreidelTitles[result], a.withTrimmedLeft (84.0f).withHeight (30.0f), juce::Justification::centredLeft);
        g.setColour (Theme::softWhite.withAlpha (juce::jmin (1.0f, 0.2f + reveal)));
        g.setFont (uiFont (14.0f));
        g.drawText (heb (dreidelBlurbs[result]) + heb ("  \xe2\x80\x94  applied"), a.withTrimmedLeft (84.0f).withTrimmedTop (30.0f),
                    juce::Justification::centredLeft);
    }
}

void DreidelPopup::paintCard (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.62f));

    const auto card = cardBounds();
    juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 40, { 0, 12 }).drawForRectangle (g, card.toNearestInt());
    g.setGradientFill (juce::ColourGradient (Theme::navy, 0.0f, card.getY(), juce::Colour (0xff030b26), 0.0f, card.getBottom(), false));
    g.fillRoundedRectangle (card, 18.0f);
    g.setColour (Theme::gold);
    g.drawRoundedRectangle (card.reduced (0.75f), 18.0f, 1.5f);

    // Title
    g.setColour (Theme::white);
    g.setFont (uiFont (20.0f, true).withExtraKerningFactor (0.22f));
    g.drawText ("SPIN THE DREIDEL", juce::Rectangle<float> (card.getX() + 28.0f, card.getY() + 16.0f, 300.0f, 30.0f), juce::Justification::centredLeft);
    g.setColour (Theme::gold);
    g.setFont (hebrewFont (26.0f, true));
    g.drawText (heb ("\xd7\xa1\xd7\x91\xd7\x99\xd7\x91\xd7\x95\xd7\x9f"),
                juce::Rectangle<float> (card.getRight() - 210.0f, card.getY() + 14.0f, 150.0f, 32.0f), juce::Justification::centredRight);

    // Close button
    const auto cb = closeBounds();
    g.setColour (Theme::white.withAlpha (isMouseOver (true) ? 0.15f : 0.08f));
    g.fillEllipse (cb);
    g.setColour (Theme::white);
    g.drawLine (cb.getX() + 9.0f, cb.getY() + 9.0f, cb.getRight() - 9.0f, cb.getBottom() - 9.0f, 2.0f);
    g.drawLine (cb.getRight() - 9.0f, cb.getY() + 9.0f, cb.getX() + 9.0f, cb.getBottom() - 9.0f, 2.0f);

    // Stage
    const auto stage = stageBounds();
    juce::Graphics::ScopedSaveState stageClip (g);
    juce::Path clip;
    clip.addRoundedRectangle (stage, 14.0f);
    g.reduceClipRegion (clip);
    g.setGradientFill (juce::ColourGradient (Theme::blue.withAlpha (0.35f), stage.getCentreX(), stage.getY(),
                                             juce::Colours::black.withAlpha (0.4f), stage.getCentreX(), stage.getBottom(), false));
    g.fillRect (stage);

    const float floorY = stage.getBottom() - 36.0f;
    const juce::Point<float> glowCentre (stage.getCentreX(), floorY - 140.0f);

    // Light rays: brightest on landing, then a slow gentle turn.
    const float rayAlpha = 0.05f + 0.22f * landGlow + (result >= 0 ? 0.04f : 0.0f);
    for (int i = 0; i < 16; ++i)
    {
        const float a0 = rays + (float) i * juce::MathConstants<float>::twoPi / 16.0f;
        juce::Path ray;
        ray.startNewSubPath (glowCentre);
        ray.lineTo (glowCentre + juce::Point<float> (std::cos (a0 - 0.07f), std::sin (a0 - 0.07f)) * 460.0f);
        ray.lineTo (glowCentre + juce::Point<float> (std::cos (a0 + 0.07f), std::sin (a0 + 0.07f)) * 460.0f);
        ray.closeSubPath();
        g.setGradientFill (juce::ColourGradient (Theme::gold.withAlpha (rayAlpha), glowCentre,
                                                 Theme::gold.withAlpha (0.0f), glowCentre.translated (0.0f, 300.0f), true));
        g.fillPath (ray);
    }
    g.setGradientFill (juce::ColourGradient (Theme::gold.withAlpha (0.14f + 0.4f * landGlow), glowCentre,
                                             Theme::gold.withAlpha (0.0f), glowCentre.translated (0.0f, 190.0f + 60.0f * landGlow), true));
    g.fillRect (stage);

    // Floor
    g.setGradientFill (juce::ColourGradient (Theme::white.withAlpha (0.09f), stage.getCentreX(), floorY,
                                             Theme::white.withAlpha (0.0f), stage.getCentreX() + 230.0f, floorY, true));
    g.fillEllipse (stage.getCentreX() - 230.0f, floorY - 30.0f, 460.0f, 60.0f);

    // Motion: wobble, drift and squash-free bounce.
    const float tilt = tiltAmp * std::sin (precession);
    const float drift = 18.0f * std::sin (precession * 0.5f) * (tiltAmp / 0.3f);
    const float pulse = std::sin (bounce * juce::MathConstants<float>::pi) * bounce;
    const float scale = 52.0f * (1.0f + 0.08f * pulse);
    const juce::Point<float> tip (stage.getCentreX() + drift, floorY - 10.0f * pulse);

    // Shadow: stretches toward the lean.
    const float lean = std::sin (tilt) * 200.0f;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (tip.x - 34.0f - std::abs (lean) * 0.3f + lean * 0.35f, floorY - 6.0f, 68.0f + std::abs (lean) * 0.6f, 14.0f);

    // Motion blur: faint copies trailing behind when it's spinning fast.
    if (angularSpeed > 6.0f)
    {
        const float trail = juce::jmin (0.5f, angularSpeed * 0.006f);
        for (int k = 3; k >= 1; --k)
        {
            g.beginTransparencyLayer (juce::jmin (0.35f, angularSpeed / 140.0f) / (float) k);
            drawDreidel (g, tip, scale, angle - trail * (float) k, tilt);
            g.endTransparencyLayer();
        }
    }
    drawDreidel (g, tip, scale, angle, tilt);

    // Speed arcs swirling around the body.
    if (angularSpeed > 4.0f)
    {
        const float strength = juce::jmin (1.0f, angularSpeed / 70.0f);
        const auto mid = tip.translated (0.0f, -scale * 2.2f);
        for (int i = 0; i < 3; ++i)
        {
            juce::Path arc;
            const float rx = scale * (1.55f + 0.18f * (float) i), ry = rx * 0.32f;
            const float start = angle * 0.9f + (float) i * 2.1f;
            arc.addCentredArc (mid.x, mid.y + (float) (i - 1) * scale * 0.55f, rx, ry, tilt, start, start + 1.4f, true);
            g.setColour (Theme::white.withAlpha (0.35f * strength));
            g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // Sparks
    for (const auto& p : particles)
    {
        const float a = juce::jlimit (0.0f, 1.0f, p.life / p.maxLife);
        g.setColour (p.colour.withAlpha (a));
        if (p.sparkle)
        {
            // Four-point twinkle
            const float r = p.size * 2.2f;
            juce::Path star;
            for (int i = 0; i < 8; ++i)
            {
                const float ang = p.spin * (p.maxLife - p.life) + (float) i * juce::MathConstants<float>::pi / 4.0f;
                const float rad = (i % 2 == 0) ? r : r * 0.28f;
                const auto pt = p.pos + juce::Point<float> (std::cos (ang), std::sin (ang)) * rad;
                if (i == 0) star.startNewSubPath (pt); else star.lineTo (pt);
            }
            star.closeSubPath();
            g.fillPath (star);
        }
        else
        {
            g.fillEllipse (p.pos.x - p.size * 0.5f, p.pos.y - p.size * 0.5f, p.size, p.size);
        }
    }
}

//==============================================================================
FlangerDisplay::FlangerDisplay (ReggirizerProcessor& p) : proc (p)
{
    startTimerHz (30);
}

void FlangerDisplay::paint (juce::Graphics& g)
{
    auto param = [this] (const char* id) { return proc.apvts.getRawParameterValue (id)->load(); };
    const bool on = param ("flgOn") > 0.5f;
    const int voices = juce::jlimit (1, Flanger::maxVoices, (int) param ("flgVoices"));
    const float depth = param ("flgDepth") * 0.01f;
    const float rate = param ("flgRate");
    const float delayMs = param ("flgDelay");
    const float spread = param ("flgSpread") * 0.01f;
    const float fb = juce::jlimit (-0.95f, 0.95f, param ("flgFeedback") * 0.01f);
    const float wet = param ("flgWet") * 0.01f;
    const float phase = proc.getFlangerPhase();

    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Uganda::yellow.withAlpha (0.25f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    auto inner = r.reduced (16.0f, 12.0f);
    auto titleRow = inner.removeFromTop (18.0f);
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.15f));
    g.setColour (Uganda::yellow);
    g.drawText ("COMB RESPONSE", titleRow, juce::Justification::centredLeft);
    g.setColour (Uganda::white.withAlpha (0.6f));
    g.drawText (juce::String (voices) + (voices == 1 ? " voice   " : " voices   ") + juce::String (rate, 2) + " Hz   "
                    + juce::String (delayMs, 2) + " ms   fb " + juce::String (juce::roundToInt (fb * 100.0f)) + "%",
                titleRow, juce::Justification::centredRight);

    auto sweep = inner.removeFromBottom (34.0f);
    inner.removeFromBottom (10.0f);
    auto freqLabels = inner.removeFromBottom (14.0f);
    auto dbLabels = inner.removeFromLeft (30.0f);
    auto graph = inner;

    // Voice delays right now (left channel), and their pan gains, as in Flanger::process.
    std::array<float, Flanger::maxVoices> delaySec {}, gain {};
    float gainSum = 0.0f;
    const float centre = delayMs * 0.001f;
    for (int v = 0; v < voices; ++v)
    {
        const float sweepPos = std::sin (juce::MathConstants<float>::twoPi * (phase + (float) v / (float) voices));
        delaySec[(size_t) v] = juce::jmax (1.0e-5f, centre * (1.0f + 0.95f * depth * sweepPos));
        const float pan = voices > 1 ? spread * (2.0f * (float) v / (float) (voices - 1) - 1.0f) : 0.0f;
        gain[(size_t) v] = juce::jmin (1.0f, 1.0f - pan);
        gainSum += gain[(size_t) v];
    }

    constexpr float topDb = 18.0f, bottomDb = -30.0f;
    auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (bottomDb, topDb, db), topDb, bottomDb, graph.getY(), graph.getBottom()); };
    auto xFor = [&] (float hz) { return graph.getX() + graph.getWidth() * std::log10 (hz / 20.0f) / 3.0f; };

    // Grid
    g.setFont (uiFont (10.0f, true));
    for (float hz : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xFor (hz);
        const bool major = hz == 100.0f || hz == 1000.0f || hz == 10000.0f;
        g.setColour (Uganda::white.withAlpha (major ? 0.14f : 0.06f));
        g.drawVerticalLine ((int) x, graph.getY(), graph.getBottom());
        if (major)
        {
            g.setColour (Uganda::white.withAlpha (0.5f));
            g.drawText (hz >= 1000.0f ? juce::String ((int) (hz / 1000.0f)) + "k" : juce::String ((int) hz),
                        juce::Rectangle<float> (x - 20.0f, freqLabels.getY(), 40.0f, freqLabels.getHeight()), juce::Justification::centred);
        }
    }
    for (float db : { 12.0f, 0.0f, -12.0f, -24.0f })
    {
        const float y = yFor (db);
        g.setColour (Uganda::white.withAlpha (db == 0.0f ? 0.3f : 0.08f));
        g.drawHorizontalLine ((int) y, graph.getX(), graph.getRight());
        g.setColour (Uganda::white.withAlpha (0.5f));
        g.drawText ((db > 0 ? "+" : "") + juce::String ((int) db), dbLabels.withY (y - 7.0f).withHeight (14.0f), juce::Justification::centredLeft);
    }

    // Response: dry + wet * highpass * A / (1 - fb * A), where A is the
    // gain-weighted sum of each voice's delay. This is exactly the flanger's
    // signal flow, so the curve shows the real notches moving.
    juce::Path curve, fill;
    const int steps = juce::jmax (2, (int) graph.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float hz = 20.0f * std::pow (1000.0f, (float) i / (float) steps);
        float db = 0.0f;
        if (on)
        {
            const float w = juce::MathConstants<float>::twoPi * hz;
            std::complex<float> a (0.0f, 0.0f);
            for (int v = 0; v < voices; ++v)
                a += gain[(size_t) v] * std::polar (1.0f, -w * delaySec[(size_t) v]);
            a /= gainSum;
            const float k = hz / 150.0f;
            const std::complex<float> hp (k * k / (1.0f + k * k), k / (1.0f + k * k));
            const auto h = 1.0f + wet * hp * a / (1.0f - fb * a);
            db = 20.0f * std::log10 (juce::jmax (1.0e-4f, std::abs (h)));
        }
        const float x = graph.getX() + graph.getWidth() * (float) i / (float) steps;
        const float y = yFor (db);
        if (i == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, graph.getBottom()); }
        else        curve.lineTo (x, y);
        fill.lineTo (x, y);
    }
    fill.lineTo (graph.getRight(), graph.getBottom());
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (Uganda::red.withAlpha (on ? 0.45f : 0.1f), 0.0f, graph.getY(),
                                             Uganda::red.withAlpha (0.0f), 0.0f, graph.getBottom(), false));
    g.fillPath (fill);
    g.setColour (on ? Uganda::yellow : Uganda::white.withAlpha (0.3f));
    g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (! on)
    {
        g.setColour (Uganda::white.withAlpha (0.45f));
        g.setFont (uiFont (14.0f, true).withExtraKerningFactor (0.3f));
        g.drawText ("BYPASSED", graph, juce::Justification::centred);
    }

    // Voice sweep strip: where each voice's delay sits in its range.
    g.setColour (Uganda::white.withAlpha (0.05f));
    g.fillRoundedRectangle (sweep, 6.0f);
    auto label = sweep.removeFromLeft (64.0f);
    g.setColour (Uganda::white.withAlpha (0.6f));
    g.setFont (uiFont (10.0f, true).withExtraKerningFactor (0.15f));
    g.drawText ("VOICES", label, juce::Justification::centred);

    auto lane = sweep.reduced (12.0f, 6.0f);
    g.setColour (Uganda::white.withAlpha (0.15f));
    g.drawHorizontalLine ((int) lane.getCentreY(), lane.getX(), lane.getRight());
    const float lo = centre * (1.0f - 0.95f * depth), hi = centre * (1.0f + 0.95f * depth);
    for (int v = 0; v < voices; ++v)
    {
        const float norm = hi > lo ? (delaySec[(size_t) v] - lo) / (hi - lo) : 0.5f;
        const float x = lane.getX() + lane.getWidth() * norm;
        const float y = voices > 1 ? lane.getY() + lane.getHeight() * (float) v / (float) (voices - 1) : lane.getCentreY();
        g.setColour ((v % 2 == 0 ? Uganda::yellow : Uganda::red).withAlpha (on ? 1.0f : 0.35f));
        g.fillEllipse (x - 4.5f, y - 4.5f, 9.0f, 9.0f);
    }
}

//==============================================================================
ReggirizerEditor::ReggirizerEditor (ReggirizerProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p.engine), heatmap (p.engine, p.apvts), artwork (p), flangerDisplay (p)
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (display);
    addAndMakeVisible (heatmap);
    addAndMakeVisible (artwork);

    for (int i = 0; i < (int) Scales::keyNames.size(); ++i)
        keyBox.addItem (Scales::keyNames[(size_t) i], i + 1);
    for (int i = 0; i < (int) Scales::all.size(); ++i)
        scaleBox.addItem (Scales::all[(size_t) i].englishName, i + 1);
    stabilizerBox.addItemList ({ "None", "Short", "Mid", "Long" }, 1);

    for (auto* b : { &keyBox, &scaleBox, &stabilizerBox, &groupBox })
        addAndMakeVisible (b);

    keyAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "key", keyBox);
    scaleAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "scale", scaleBox);
    stabilizerAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (p.apvts, "stabilizer", stabilizerBox);

    // Groups aren't a host parameter (automating them would be odd), so wire by hand.
    groupBox.addItem ("Off", 1);
    for (int g = 1; g <= ReggirizerProcessor::numGroups; ++g)
        groupBox.addItem (juce::String::charToString ((juce::juce_wchar) ('A' + g - 1)), g + 1);
    groupBox.setSelectedId (proc.getGroup() + 1, juce::dontSendNotification);
    groupBox.onChange = [this] { proc.setGroup (groupBox.getSelectedId() - 1); };

    groupCount.setFont (uiFont (11.0f, true));
    groupCount.setColour (juce::Label::textColourId, Theme::gold);
    groupCount.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (groupCount);
    groupPoller.onTick = [this]
    {
        const int g = proc.getGroup();
        if (groupBox.getSelectedId() != g + 1)
            groupBox.setSelectedId (g + 1, juce::dontSendNotification); // e.g. a preset was loaded
        const int n = proc.countGroupMembers();
        groupCount.setText (g == 0 ? juce::String() : juce::String (n) + " linked", juce::dontSendNotification);
    };
    groupPoller.onTick();
    groupPoller.startTimerHz (2);

    scaleHebrew.setFont (hebrewFont (20.0f, true));
    scaleHebrew.setColour (juce::Label::textColourId, Theme::gold);
    scaleHebrew.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (scaleHebrew);

    auto updateHebrew = [this]
    {
        const int idx = juce::jlimit (0, (int) Scales::all.size() - 1, scaleBox.getSelectedItemIndex());
        scaleHebrew.setText (heb (Scales::all[(size_t) idx].hebrewName), juce::dontSendNotification);
    };
    scaleBox.onChange = [this, updateHebrew]
    {
        updateHebrew();
        repaint();
    };
    updateHebrew();

    toneButton.setClickingTogglesState (true);
    toneButton.setColour (juce::TextButton::buttonColourId, Theme::white.withAlpha (0.1f));
    toneButton.setColour (juce::TextButton::buttonOnColourId, Theme::gold);
    toneButton.setColour (juce::TextButton::textColourOffId, Theme::white);
    toneButton.setColour (juce::TextButton::textColourOnId, Theme::deepBlue);
    addAndMakeVisible (toneButton);
    toneAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "tone", toneButton);

    setUpKnob (speed,   "speed",   "RETUNE SPEED", "\xd7\x9e\xd7\x94\xd7\x99\xd7\xa8\xd7\x95\xd7\xaa");
    setUpKnob (amount,  "amount",  "AMOUNT",       "\xd7\x9b\xd7\x9e\xd7\x95\xd7\xaa");
    setUpKnob (sustain, "sustain", "SUSTAIN",      "\xd7\x94\xd7\x97\xd7\x96\xd7\xa7\xd7\x94");
    setUpKnob (doubler, "doubler", "DOUBLER",      "\xd7\x94\xd7\x9b\xd7\xa4\xd7\x9c\xd7\x94");
    setUpKnob (width,   "width",   "WIDTH",        "\xd7\xa8\xd7\x95\xd7\x97\xd7\x91");
    setUpKnob (mix,     "mix",     "MIX",          "\xd7\x9e\xd7\x99\xd7\xa7\xd7\xa1");
    setUpKnob (output,  "output",  "OUTPUT",       "\xd7\x99\xd7\xa6\xd7\x99\xd7\x90\xd7\x94");

    // REGGURRECTION page captions: English + Swahili (an official language of Uganda).
    setUpKnob (fVoices,   "flgVoices",   "VOICES",   "SAUTI");
    setUpKnob (fDepth,    "flgDepth",    "DEPTH",    "KINA");
    setUpKnob (fRate,     "flgRate",     "RATE",     "KASI");
    setUpKnob (fDelay,    "flgDelay",    "DELAY",    "KUCHELEWA");
    setUpKnob (fSpread,   "flgSpread",   "SPREAD",   "KUENEA");
    setUpKnob (fFeedback, "flgFeedback", "FEEDBACK", "MREJESHO");
    setUpKnob (fWet,      "flgWet",      "WET",      "KIWANGO");
    for (auto* k : flangerKnobs)
        k->slider.getProperties().set ("uganda", true);
    addChildComponent (flangerDisplay);

    // Page tabs
    for (auto* t : { &tuningTab, &flangerTab })
    {
        t->setRadioGroupId (1001);
        t->setClickingTogglesState (true);
        t->setColour (juce::TextButton::buttonColourId, Theme::white.withAlpha (0.08f));
        t->setColour (juce::TextButton::buttonOnColourId, Theme::gold);
        t->setColour (juce::TextButton::textColourOffId, Theme::white);
        t->setColour (juce::TextButton::textColourOnId, Theme::deepBlue);
        addAndMakeVisible (t);
    }
    flangerTab.setComponentID ("flangerTab");
    tuningTab.setConnectedEdges (juce::Button::ConnectedOnRight);
    flangerTab.setConnectedEdges (juce::Button::ConnectedOnLeft);
    tuningTab.onClick = [this] { if (tuningTab.getToggleState()) showPage (0); };
    flangerTab.onClick = [this] { if (flangerTab.getToggleState()) showPage (1); };

    flangerOn.setClickingTogglesState (true);
    flangerOn.setColour (juce::TextButton::buttonColourId, Theme::white.withAlpha (0.1f));
    flangerOn.setColour (juce::TextButton::buttonOnColourId, Uganda::yellow);
    flangerOn.setColour (juce::TextButton::textColourOffId, Uganda::white);
    flangerOn.setColour (juce::TextButton::textColourOnId, Uganda::black);
    flangerOn.onStateChange = [this] { flangerOn.setButtonText (flangerOn.getToggleState() ? "FLANGER ON" : "FLANGER OFF"); };
    addChildComponent (flangerOn);
    flangerOnAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "flgOn", flangerOn);
    flangerOn.setButtonText (flangerOn.getToggleState() ? "FLANGER ON" : "FLANGER OFF");

    flangerPreset.setTextWhenNothingSelected ("Presets");
    flangerPreset.getProperties().set ("uganda", true);
    flangerPreset.setColour (juce::ComboBox::textColourId, Uganda::yellow);
    flangerPreset.addItemList ({ "Wide Vocal", "Liquid Sheen", "Metallic Jet", "Hollow Tube", "Subtle Sweep" }, 1);
    flangerPreset.onChange = [this] { applyFlangerPreset (flangerPreset.getSelectedItemIndex()); };
    addChildComponent (flangerPreset);

    addAndMakeVisible (dreidel);
    dreidel.onClick = [this] { dreidelPopup.open(); };
    dreidelPopup.onLanded = [this] (int letter)
    {
        randomizeTuning (letter);
        dreidel.setLetter (letter);
    };
    addChildComponent (dreidelPopup);
    dreidelResult.setFont (uiFont (12.0f, true));
    dreidelResult.setColour (juce::Label::textColourId, Theme::gold);
    dreidelResult.setJustificationType (juce::Justification::centredRight);
    dreidelResult.setText (heb ("SPIN THE DREIDEL FOR A RANDOM TUNE  \xe2\x86\x92"), juce::dontSendNotification);
    addAndMakeVisible (dreidelResult);

    showPage (0);
    setSize (820, 560);
}

ReggirizerEditor::~ReggirizerEditor()
{
    groupPoller.stopTimer();
    setLookAndFeel (nullptr);
}

void ReggirizerEditor::showPage (int page)
{
    currentPage = page;
    (page == 0 ? tuningTab : flangerTab).setToggleState (true, juce::dontSendNotification);
    for (auto* k : knobs)
        k->slider.setVisible (page == 0);
    for (auto* k : flangerKnobs)
        k->slider.setVisible (page == 1);
    flangerOn.setVisible (page == 1);
    flangerPreset.setVisible (page == 1);
    flangerDisplay.setVisible (page == 1);

    // The tuning page's own panels.
    for (juce::Component* c : { (juce::Component*) &display, (juce::Component*) &heatmap, (juce::Component*) &keyBox,
                                (juce::Component*) &scaleBox, (juce::Component*) &scaleHebrew, (juce::Component*) &stabilizerBox,
                                (juce::Component*) &groupBox, (juce::Component*) &groupCount, (juce::Component*) &toneButton,
                                (juce::Component*) &dreidel, (juce::Component*) &dreidelResult })
        c->setVisible (page == 0);

    artwork.setAccent (page == 0 ? Theme::blue : Uganda::yellow, page == 0 ? Theme::blue : Uganda::yellow);

    // Tabs take the colours of the page they're on.
    for (auto* t : { &tuningTab, &flangerTab })
    {
        t->setColour (juce::TextButton::buttonOnColourId, page == 0 ? Theme::gold : Uganda::yellow);
        t->setColour (juce::TextButton::textColourOnId, page == 0 ? Theme::deepBlue : Uganda::black);
    }
    repaint();
}

void ReggirizerEditor::applyFlangerPreset (int index)
{
    // voices, depth %, rate Hz, delay ms, spread %, feedback %, wet %
    struct Preset { float voices, depth, rate, delay, spread, feedback, wet; };
    constexpr Preset presets[] = {
        { 6, 18, 0.15f, 2.5f,  85,  10, 25 }, // Wide Vocal: backing vocals, ad-libs, layers
        { 8, 28, 0.12f, 4.0f, 100,  20, 40 }, // Liquid Sheen: pads, keys, chords
        { 2, 55, 0.25f, 2.0f,  50,  65, 45 }, // Metallic Jet: leads, risers
        { 1, 40, 0.30f, 1.5f,  30, -55, 35 }, // Hollow Tube: negative feedback, rubbery
        { 3, 15, 0.08f, 3.0f,  60,   5, 20 }, // Subtle Sweep: slow movement under a lead
    };
    if (index < 0 || index >= (int) std::size (presets))
        return;

    const auto& pr = presets[index];
    auto set = [this] (const char* id, float value)
    {
        auto* param = proc.apvts.getParameter (id);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    };
    set ("flgVoices", pr.voices);
    set ("flgDepth", pr.depth);
    set ("flgRate", pr.rate);
    set ("flgDelay", pr.delay);
    set ("flgSpread", pr.spread);
    set ("flgFeedback", pr.feedback);
    set ("flgWet", pr.wet);
    set ("flgOn", 1.0f); // picking a preset turns the flanger on
}

void ReggirizerEditor::randomizeTuning (int letter)
{
    // Each letter is a style. Key, scale, mix and output are never touched, so
    // a spin can't knock the song out of key or change its level.
    juce::Random rng;
    auto range = [&rng] (float lo, float hi) { return lo + rng.nextFloat() * (hi - lo); };
    auto set = [this] (const char* id, float value)
    {
        auto* param = proc.apvts.getParameter (id);
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (value));
        param->endChangeGesture();
    };

    switch (letter)
    {
        case 0: // Nun: natural, gentle correction
            set ("speed", range (40.0f, 120.0f));
            set ("amount", range (60.0f, 90.0f));
            set ("stabilizer", (float) (2 + rng.nextInt (2)));
            set ("sustain", range (100.0f, 250.0f));
            set ("doubler", range (0.0f, 15.0f));
            set ("width", range (40.0f, 70.0f));
            break;
        case 1: // Gimel: everything, the hard robotic snap
            set ("speed", range (-3.0f, 0.0f));
            set ("amount", 100.0f);
            set ("stabilizer", (float) rng.nextInt (2));
            set ("sustain", range (30.0f, 80.0f));
            set ("doubler", range (30.0f, 70.0f));
            set ("width", range (70.0f, 100.0f));
            break;
        case 2: // Hei: half way, modern pop tuning
            set ("speed", range (8.0f, 35.0f));
            set ("amount", range (85.0f, 100.0f));
            set ("stabilizer", (float) (1 + rng.nextInt (2)));
            set ("sustain", range (60.0f, 150.0f));
            set ("doubler", range (10.0f, 40.0f));
            set ("width", range (50.0f, 90.0f));
            break;
        default: // Shin: anything goes
            set ("speed", range (-3.0f, 200.0f));
            set ("amount", range (40.0f, 100.0f));
            set ("stabilizer", (float) rng.nextInt (4));
            set ("sustain", range (0.0f, 400.0f));
            set ("doubler", range (0.0f, 100.0f));
            set ("width", range (0.0f, 100.0f));
            break;
    }

    dreidelResult.setText (heb (dreidelNames[juce::jlimit (0, 3, letter)]), juce::dontSendNotification);
}

void ReggirizerEditor::setUpKnob (Knob& k, const juce::String& paramId, const juce::String& english, const char* hebrewUtf8)
{
    k.english = english;
    k.hebrew = heb (hebrewUtf8);
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
    k.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    k.slider.setColour (juce::Slider::textBoxTextColourId, Theme::white);
    addAndMakeVisible (k.slider);
    k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, paramId, k.slider);
}

void ReggirizerEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    if (currentPage == 1)
    {
        paintUganda (g);
        return;
    }

    // Body: deep blue gradient with a faint Magen David watermark.
    g.setGradientFill (juce::ColourGradient (Theme::blue, 0.0f, 80.0f, Theme::deepBlue, 0.0f, bounds.getBottom(), false));
    g.fillAll();
    strokeMagenDavid (g, { bounds.getCentreX(), 310.0f }, 240.0f, 10.0f, Theme::white.withAlpha (0.035f));

    // Header: white band with two blue tallit stripes.
    auto header = bounds.withHeight (84.0f);
    g.setColour (Theme::white);
    g.fillRect (header);
    g.setColour (Theme::blue);
    g.fillRect (header.withY (8.0f).withHeight (6.0f));
    g.fillRect (header.withY (header.getBottom() - 14.0f).withHeight (6.0f));
    g.setColour (Theme::gold);
    g.fillRect (header.withY (header.getBottom() - 2.0f).withHeight (2.0f));

    strokeMagenDavid (g, { 50.0f, 42.0f }, 20.0f, 3.0f, Theme::blue);

    g.setColour (Theme::blue);
    g.setFont (uiFont (34.0f, true).withExtraKerningFactor (0.12f));
    g.drawText ("REGGIRIZER", juce::Rectangle<float> (84.0f, 18.0f, 360.0f, 40.0f), juce::Justification::centredLeft);
    g.setColour (Theme::navy.withAlpha (0.6f));
    g.setFont (uiFont (12.0f, true).withExtraKerningFactor (0.2f));
    g.drawText (currentPage == 0 ? juce::String ("VOCAL PITCH CORRECTION") : heb ("REGGURRECTION  \xc2\xb7  MULTI-VOICE FLANGER"),
                juce::Rectangle<float> (86.0f, 52.0f, 420.0f, 16.0f), juce::Justification::centredLeft);

    g.setColour (Theme::blue);
    g.setFont (hebrewFont (32.0f, true));
    g.drawText (heb ("\xd7\xa8\xd7\x92\xd7\xb3\xd7\x99\xd7\xa8\xd7\x99\xd7\x96\xd7\xa8"),
                juce::Rectangle<float> (bounds.getRight() - 320.0f, 18.0f, 230.0f, 44.0f), juce::Justification::centredRight);

    // Left panel labels
    g.setColour (Theme::softWhite);
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.15f));
    g.drawText ("KEY", juce::Rectangle<float> (24.0f, 98.0f, 200.0f, 14.0f), juce::Justification::centredLeft);
    g.drawText ("SCALE", juce::Rectangle<float> (24.0f, 152.0f, 200.0f, 14.0f), juce::Justification::centredLeft);
    g.drawText ("STABILIZER", juce::Rectangle<float> (24.0f, 238.0f, 110.0f, 14.0f), juce::Justification::centredLeft);
    g.drawText ("GROUP", juce::Rectangle<float> (140.0f, 238.0f, 60.0f, 14.0f), juce::Justification::centredLeft);

    // Knob captions (English + Hebrew) for whichever page is showing
    for (auto* k : currentPage == 0 ? knobs : flangerKnobs)
    {
        const auto kb = k->slider.getBounds().toFloat();
        g.setColour (Theme::white);
        g.setFont (uiFont (12.0f, true).withExtraKerningFactor (0.1f));
        g.drawText (k->english, kb.withY (kb.getBottom() + 1.0f).withHeight (16.0f).expanded (8.0f, 0.0f), juce::Justification::centred);
        g.setColour (Theme::gold);
        g.setFont (hebrewFont (16.0f, true));
        g.drawText (k->hebrew, kb.withY (kb.getBottom() + 17.0f).withHeight (20.0f), juce::Justification::centred);
    }
}

void ReggirizerEditor::paintUganda (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Body: charcoal to black, with a faint crane watermark.
    g.setGradientFill (juce::ColourGradient (Uganda::charcoal, 0.0f, 80.0f, juce::Colours::black, 0.0f, bounds.getBottom(), false));
    g.fillAll();
    drawCrane (g, { bounds.getRight() - 250.0f, 330.0f, 230.0f, 230.0f }, 0.06f);

    // Header: the flag's six stripes along the top, black band, yellow rule.
    auto header = bounds.withHeight (84.0f);
    g.setColour (Uganda::black);
    g.fillRect (header);
    const juce::Colour stripes[] = { Uganda::black, Uganda::yellow, Uganda::red, Uganda::black, Uganda::yellow, Uganda::red };
    for (int i = 0; i < 6; ++i)
    {
        g.setColour (stripes[i]);
        g.fillRect (0.0f, 2.0f * (float) i * 1.5f, bounds.getWidth(), 3.0f);
    }
    g.setColour (Uganda::yellow);
    g.fillRect (header.withY (header.getBottom() - 2.0f).withHeight (2.0f));

    // Emblem: white disc with the crane, as on the flag.
    const juce::Point<float> disc (50.0f, 48.0f);
    g.setColour (Uganda::white);
    g.fillEllipse (disc.x - 27.0f, disc.y - 27.0f, 54.0f, 54.0f);
    drawCrane (g, { disc.x - 23.0f, disc.y - 25.0f, 46.0f, 48.0f });

    g.setColour (Uganda::yellow);
    g.setFont (uiFont (32.0f, true).withExtraKerningFactor (0.1f));
    g.drawText ("REGGURRECTION", juce::Rectangle<float> (88.0f, 22.0f, 420.0f, 38.0f), juce::Justification::centredLeft);
    g.setColour (Uganda::white.withAlpha (0.7f));
    g.setFont (uiFont (12.0f, true).withExtraKerningFactor (0.2f));
    g.drawText (heb ("MULTI-VOICE FLANGER  \xc2\xb7  FLANGA YA SAUTI NYINGI"),
                juce::Rectangle<float> (90.0f, 56.0f, 480.0f, 16.0f), juce::Justification::centredLeft);

    g.setColour (Uganda::white.withAlpha (0.45f));
    g.setFont (uiFont (11.0f, true).withExtraKerningFactor (0.2f));
    g.drawText ("BY REGGIRIZER", juce::Rectangle<float> (bounds.getRight() - 260.0f, 30.0f, 170.0f, 20.0f), juce::Justification::centredRight);

    // Captions: English + Swahili
    for (auto* k : flangerKnobs)
    {
        const auto kb = k->slider.getBounds().toFloat();
        g.setColour (Uganda::white);
        g.setFont (uiFont (12.0f, true).withExtraKerningFactor (0.1f));
        g.drawText (k->english, kb.withY (kb.getBottom() + 1.0f).withHeight (16.0f).expanded (10.0f, 0.0f), juce::Justification::centred);
        g.setColour (Uganda::yellow);
        g.setFont (uiFont (12.0f, true).withExtraKerningFactor (0.12f));
        g.drawText (k->hebrew, kb.withY (kb.getBottom() + 17.0f).withHeight (18.0f).expanded (10.0f, 0.0f), juce::Justification::centred);
    }
}

void ReggirizerEditor::resized()
{
    keyBox.setBounds (24, 114, 220, 32);
    scaleBox.setBounds (24, 168, 220, 32);
    scaleHebrew.setBounds (24, 202, 220, 30);
    stabilizerBox.setBounds (24, 254, 108, 30);
    groupBox.setBounds (140, 254, 104, 30);
    groupCount.setBounds (184, 238, 60, 14);

    display.setBounds (268, 98, getWidth() - 268 - 24, 186);
    artwork.setBounds (getWidth() - 24 - 52, 16, 52, 52); // sits between the header's tallit stripes

    heatmap.setBounds (24, 296, getWidth() - 48, 64);
    toneButton.setBounds (34, 296 + 64 - 26, 64, 20);

    tuningTab.setBounds (24, 372, 110, 24);
    flangerTab.setBounds (134, 372, 150, 24);
    flangerPreset.setBounds (getWidth() - 24 - 160 - 10 - 120, 370, 160, 28);
    flangerOn.setBounds (getWidth() - 24 - 120, 372, 120, 24);
    dreidel.setBounds (getWidth() - 24 - 46, 355, 46, 53);
    dreidelResult.setBounds (300, 372, getWidth() - 24 - 46 - 8 - 300, 24);
    dreidelPopup.setBounds (getLocalBounds());

    const int knobW = 100, knobH = 100, top = 404;
    const int gap = (getWidth() - (int) knobs.size() * knobW) / ((int) knobs.size() + 1);
    {
        int x = gap;
        for (auto* k : knobs)
        {
            k->slider.setBounds (x, top, knobW, knobH);
            x += knobW + gap;
        }
    }

    // REGGURRECTION page: a different plugin's layout. Big response display on
    // top; Depth and Rate as large centre knobs, the rest smaller around them.
    flangerDisplay.setBounds (24, 98, getWidth() - 48, 260);
    {
        const std::array<Knob*, 7> order { &fVoices, &fDelay, &fDepth, &fRate, &fSpread, &fFeedback, &fWet };
        const int small = 88, big = 112;
        const float fgap = (float) (getWidth() - 5 * small - 2 * big) / 8.0f;
        float x = fgap;
        for (auto* k : order)
        {
            const bool isBig = k == &fDepth || k == &fRate;
            const int size = isBig ? big : small;
            const int y = isBig ? 404 : 404 + (big - small) / 2;
            k->slider.setBounds (juce::roundToInt (x), y, size, size);
            x += (float) size + fgap;
        }
    }
}
