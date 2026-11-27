#include "BasedEditor.h"
#include "BasedParams.h"
#include "BasedUIFiles.h"

BasedEditor::BasedEditor (BasedProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    using namespace juce;

    auto options = WebBrowserComponent::Options {}
                       .withBackend (WebBrowserComponent::Options::Backend::webview2)
                       .withWinWebView2Options (WebBrowserComponent::Options::WinWebView2 {}
                                                    .withUserDataFolder (File::getSpecialLocation (File::tempDirectory).getChildFile ("07BASED-webview"))
                                                    .withBackgroundColour (Colours::black))
                       .withNativeIntegrationEnabled()
                       .withKeepPageLoadedWhenBrowserIsHidden()
                       .withResourceProvider ([this] (const String& url) { return getResource (url); });

    // One relay per parameter, named exactly like the parameter id.
    for (auto& k : BasedParams::knobs)
        options = options.withOptionsFrom (*sliderRelays.emplace_back (std::make_unique<WebSliderRelay> (k.id)));
    for (auto& i : BasedParams::ints)
        options = options.withOptionsFrom (*sliderRelays.emplace_back (std::make_unique<WebSliderRelay> (i.id)));
    for (auto& t : BasedParams::toggles)
        options = options.withOptionsFrom (*toggleRelays.emplace_back (std::make_unique<WebToggleButtonRelay> (t.id)));
    for (auto& c : BasedParams::choices)
        options = options.withOptionsFrom (*comboRelays.emplace_back (std::make_unique<WebComboBoxRelay> (c.id)));

    // Things the UI owns that aren't host parameters.
    options = options
        .withNativeFunction ("setOrder", [this] (const Array<var>& args, auto done)
        {
            StringArray ids;
            if (auto* a = args[0].getArray())
                for (auto& v : *a)
                    ids.add (v.toString());
            proc.setOrder (ids);
            done ({});
        })
        .withNativeFunction ("saveBluEdits", [this] (const Array<var>& args, auto done) { proc.setBluEdits (args[0].toString()); done ({}); })
        .withNativeFunction ("setTcPreset", [this] (const Array<var>& args, auto done) { proc.setTcPreset ((int) args[0], (int) args[1]); done ({}); })
        .withNativeFunction ("blast", [this] (const Array<var>&, auto done) { proc.blast(); done ({}); })
        // The UI reports that it connected (and any script error) to a small log file,
        // so a broken UI can be diagnosed: ~/Library/Logs/07BASED.log or %APPDATA%\07BASED\07BASED.log
        .withNativeFunction ("uiLog", [] (const Array<var>& args, auto done) { log (args[0].toString()); done ({}); })
        .withNativeFunction ("getState", [this] (const Array<var>&, auto done)
        {
            auto* o = new DynamicObject();
            Array<var> order;
            for (auto& id : proc.getOrder())
                order.add (id);
            o->setProperty ("order", order);
            o->setProperty ("bluEdits", proc.getBluEdits());
            o->setProperty ("tcPreset", proc.getTcPreset());
            done (var (o));
        });

    web = std::make_unique<WebBrowserComponent> (options);

    auto& apvts = proc.apvts;
    size_t s = 0;
    for (auto& k : BasedParams::knobs)
        sliderAttachments.push_back (std::make_unique<WebSliderParameterAttachment> (*apvts.getParameter (k.id), *sliderRelays[s++], nullptr));
    for (auto& i : BasedParams::ints)
        sliderAttachments.push_back (std::make_unique<WebSliderParameterAttachment> (*apvts.getParameter (i.id), *sliderRelays[s++], nullptr));
    for (size_t t = 0; t < BasedParams::toggles.size(); ++t)
        toggleAttachments.push_back (std::make_unique<WebToggleButtonParameterAttachment> (*apvts.getParameter (BasedParams::toggles[t].id), *toggleRelays[t], nullptr));
    for (size_t c = 0; c < BasedParams::choices.size(); ++c)
        comboAttachments.push_back (std::make_unique<WebComboBoxParameterAttachment> (*apvts.getParameter (BasedParams::choices[c].id), *comboRelays[c], nullptr));

    addAndMakeVisible (*web);
    web->goToURL (WebBrowserComponent::getResourceProviderRoot());

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) baseWidth / baseHeight);
        c->setSizeLimits (baseWidth / 2, baseHeight / 2, baseWidth * 2, baseHeight * 2);
    }
    setSize (baseWidth, baseHeight);
    startTimerHz (30);
    for (auto* id : BasedParams::fxOnIds)
        proc.apvts.addParameterListener (id, this);
}

BasedEditor::~BasedEditor()
{
    for (auto* id : BasedParams::fxOnIds)
        proc.apvts.removeParameterListener (id, this);
    stopTimer();
}

void BasedEditor::parameterChanged (const juce::String& id, float value)
{
    log (id + (value > 0.5f ? " on" : " off"));
}

// A small diagnostics log: ~/Library/Logs/07BASED.log (Mac) or %APPDATA%\07BASED\07BASED.log (Windows).
void BasedEditor::log (const juce::String& line)
{
    using namespace juce;
   #if JUCE_MAC
    auto f = File::getSpecialLocation (File::userHomeDirectory).getChildFile ("Library/Logs/07BASED.log");
   #else
    auto f = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("07BASED/07BASED.log");
   #endif
    f.getParentDirectory().createDirectory();
    if (f.getSize() > 200000)
        f.deleteFile();
    f.appendText (Time::getCurrentTime().toString (true, true) + "  " + line + "\n");
}

void BasedEditor::resized()
{
    if (web != nullptr)
        web->setBounds (getLocalBounds());
}

// Live readouts: the singer's pitch for Reggirizer and the bar position for Blu Pill.
void BasedEditor::timerCallback()
{
    auto* o = new juce::DynamicObject();
    const float det = proc.pitch.detectedMidi.load();
    const int tgt = proc.pitch.targetMidi.load();
    o->setProperty ("det", det);
    o->setProperty ("tgt", tgt);
    o->setProperty ("cents", det >= 0.0f && tgt >= 0 ? (det - (float) tgt) * 100.0f : 0.0f);
    o->setProperty ("phase", proc.barPhase.load());
    o->setProperty ("corr", proc.pitch.correctionCents.load());

    // Output level (for Grapecrusher's speakers) and the scopes: ~21 ms of signal, 128 points.
    juce::Array<juce::var> lv;
    for (auto& pk : proc.outPeak)
        lv.add (pk.exchange (0.0f));
    o->setProperty ("lv", lv);
    auto scope = [] (const BasedProcessor::Scope& sc)
    {
        float tmp[128];
        sc.read (tmp, 128, 8);
        juce::Array<juce::var> a;
        a.ensureStorageAllocated (128);
        for (float f : tmp)
            a.add (std::round (f * 1000.0f) / 1000.0f);
        return juce::var (a);
    };
    o->setProperty ("gcIn", scope (proc.gcIn));
    o->setProperty ("gcOut", scope (proc.gcOut));
    o->setProperty ("tcIn", scope (proc.tcIn));
    o->setProperty ("tcOut", scope (proc.tcOut));
    web->emitEventIfBrowserIsVisible ("status", juce::var (o));
}

// Serves the bundled UI (see BasedUIFiles for how requests map to files).
std::optional<juce::WebBrowserComponent::Resource> BasedEditor::getResource (const juce::String& url)
{
    const auto file = BasedUIFiles::fileFor (url);
    int size = 0;
    if (const auto* data = BasedUIFiles::find (file, size))
    {
        juce::WebBrowserComponent::Resource r;
        r.data.resize ((size_t) size);
        std::memcpy (r.data.data(), data, (size_t) size);
        r.mimeType = BasedUIFiles::mimeFor (file);
        return r;
    }
    return std::nullopt;
}
