#include "BasedEditor.h"
#include "BasedParams.h"
#include "BasedUIData.h"

namespace
{
    const char* mimeFor (const juce::String& file)
    {
        const auto ext = file.fromLastOccurrenceOf (".", false, false).toLowerCase();
        if (ext == "html") return "text/html; charset=utf-8";
        if (ext == "js")   return "text/javascript; charset=utf-8";
        if (ext == "css")  return "text/css; charset=utf-8";
        if (ext == "png")  return "image/png";
        if (ext == "jpg" || ext == "jpeg") return "image/jpeg";
        if (ext == "svg")  return "image/svg+xml";
        if (ext == "ttf")  return "font/ttf";
        if (ext == "json") return "application/json";
        return "application/octet-stream";
    }
}

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
}

BasedEditor::~BasedEditor()
{
    stopTimer();
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
    web->emitEventIfBrowserIsVisible ("status", juce::var (o));
}

// Serves the bundled UI. Every file is looked up by its file name.
std::optional<juce::WebBrowserComponent::Resource> BasedEditor::getResource (const juce::String& url)
{
    auto path = url.fromFirstOccurrenceOf (juce::WebBrowserComponent::getResourceProviderRoot(), false, false);
    path = path.upToFirstOccurrenceOf ("?", false, false).upToFirstOccurrenceOf ("#", false, false);
    if (path.isEmpty() || path == "/")
        path = "index.html";
    const auto file = path.fromLastOccurrenceOf ("/", false, false);

    for (int i = 0; i < BasedUIData::namedResourceListSize; ++i)
        if (file == BasedUIData::originalFilenames[i])
        {
            int size = 0;
            if (const auto* data = BasedUIData::getNamedResource (BasedUIData::namedResourceList[i], size))
            {
                juce::WebBrowserComponent::Resource r;
                r.data.resize ((size_t) size);
                std::memcpy (r.data.data(), data, (size_t) size);
                r.mimeType = mimeFor (file);
                return r;
            }
        }
    return std::nullopt;
}
