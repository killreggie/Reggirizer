// 07BASED v2 engine tests. Build with -DBASED_BUILD_TESTS=ON and run BasedTest.
// Exits non-zero if anything fails.

#include "../Source/v2/BasedProcessor.h"
#include "../Source/v2/BasedUIFiles.h"
#include <juce_events/juce_events.h>
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok) ++failures;
}

static float rms (const std::vector<float>& v, size_t from = 0)
{
    double s = 0;
    for (size_t i = from; i < v.size(); ++i) s += (double) v[i] * v[i];
    return (float) std::sqrt (s / std::max<size_t> (1, v.size() - from));
}

static std::vector<float> sine (int n, double sr, double hz, float amp)
{
    std::vector<float> v ((size_t) n);
    for (int i = 0; i < n; ++i) v[(size_t) i] = amp * (float) std::sin (2.0 * 3.14159265358979 * hz * i / sr);
    return v;
}

static void testPresets()
{
    // Reference values computed by the UI's own JavaScript generator.
    const auto t = BluPill::preset (false, 16);
    check (t.size() == 8, "Blu Pill time preset 16 has 8 points, same as the UI");
    check (std::abs (t[1].x - 0.112038f) < 1e-5f && std::abs (t[1].y - 0.057924f) < 1e-5f, "time preset 16 point 1 matches the UI");
    check (std::abs (t.back().x - 1.0f) < 1e-6f && std::abs (t.back().y - 0.703971f) < 1e-5f, "time preset 16 last point matches the UI");
    const auto v = BluPill::preset (true, 8);
    check (v.size() == 10, "Blu Pill volume preset 8 has 10 points, same as the UI");
    check (std::abs (v[2].x - 0.336861f) < 1e-5f && std::abs (v[3].x - 0.314053f) < 1e-5f, "volume preset 8 points match the UI");
    check (std::abs (v[8].y - 0.496165f) < 1e-5f, "volume preset 8 step level matches the UI");
}

static void testGrapeCrusher()
{
    const double sr = 48000;
    GrapeCrusher gc;
    gc.prepare (sr);
    GrapeCrusher::Settings s;
    s.rate = 0; s.jitter = 0; s.dither = 0; s.noise = 0; s.smooth = 0; s.lowCut = 0; s.highCut = 1; s.tilt = .5f;
    s.mix = 1; s.in = .5f; s.out = .5f; s.softClip = false; s.antiAlias = false; s.mode = GrapeCrusher::Round;

    auto run = [&] (float bitsKnob)
    {
        s.bits = bitsKnob;
        gc.prepare (sr);
        gc.setSettings (s);
        auto x = sine (24000, sr, 220, 0.4f);
        float* p[1] { x.data() };
        gc.process (p, 1, (int) x.size());
        return x;
    };
    const auto clean = run (0.0f); // 12 bits
    const auto ref = sine (24000, sr, 220, 0.4f);
    std::vector<float> diff (ref.size());
    for (size_t i = 0; i < ref.size(); ++i) diff[i] = clean[i] - ref[i];
    std::printf ("      12-bit: in rms %.4f out rms %.4f diff rms %.4f\n", rms (ref, 4800), rms (clean, 4800), rms (diff, 4800));
    // The tone filters shift phase a little, so compare level rather than sample-by-sample.
    check (std::abs (rms (clean, 4800) / rms (ref, 4800) - 1.0f) < 0.02f, "Grapecrusher at 12 bits keeps the input level");
    const auto crushed = run (1.0f); // 1 bit: a 0.4 sine rounds to zero
    check (rms (crushed, 4800) < 0.02f, "Grapecrusher at 1 bit rounds a quiet sine away");
    check (GrapeCrusher::bitsFor (0.55f) == 6, "bits knob at 0.55 = 6 bits, same as the UI readout");

    // Silence in, silence out: no hiss even with dither + noise turned up, in every mode.
    for (int mode = 0; mode < 4; ++mode)
    {
        GrapeCrusher g2;
        g2.prepare (sr);
        GrapeCrusher::Settings loud;
        loud.dither = 1; loud.noise = 1; loud.mode = mode;
        g2.setSettings (loud);
        std::vector<float> quiet (48000, 0.0f);
        float* qp[1] { quiet.data() };
        g2.process (qp, 1, (int) quiet.size());
        float peak = 0;
        for (float f : quiet) peak = std::max (peak, std::abs (f));
        if (peak > 1e-6f) { std::printf ("      mode %d silence peak %g\n", mode, peak); }
        check (peak < 1e-6f, "Grapecrusher adds no static to silence (dither + noise maxed)");
    }
}

static void testTrapCity()
{
    const double sr = 48000;
    TrapCity tc;
    tc.prepare (sr, 512);
    TrapCity::Settings s;
    s.drive = 1; s.mix = 1; s.out = .5f; s.tone = 1; s.mode = TrapCity::Clip; s.grit = 0;
    tc.setSettings (s);
    auto x = sine (48000, sr, 55, 0.8f);
    bool finite = true;
    float peak = 0;
    for (size_t at = 0; at < x.size(); at += 512)
    {
        float* p[1] { x.data() + at };
        tc.process (p, 1, (int) std::min<size_t> (512, x.size() - at));
    }
    float startPeak = 0;
    for (size_t i = 0; i < x.size(); ++i)
    {
        finite = finite && std::isfinite (x[i]);
        (i < 12000 ? startPeak : peak) = std::max (i < 12000 ? startPeak : peak, std::abs (x[i]));
    }
    std::printf ("      first 0.25 s peak %.4f\n", startPeak);
    check (finite, "Trap City output is finite at full drive");
    std::printf ("      trap peak %.4f\n", peak);
    check (peak < 1.3f && peak > 0.3f, "Trap City clip mode is loud but bounded");

    s.mix = 0;
    tc.setSettings (s);
    auto y = sine (2048, sr, 440, 0.5f);
    const auto ref = y;
    float* p[1] { y.data() };
    tc.process (p, 1, 512);
    // At mix 0 the output is the input, delayed by exactly the reported latency.
    const int L = tc.getLatencySamples();
    float maxErr = 0;
    for (int i = L; i < 512; ++i) maxErr = std::max (maxErr, std::abs (y[(size_t) i] - ref[(size_t) (i - L)]));
    check (maxErr < 1e-5f, "Trap City at mix 0 passes the dry signal untouched (latency-aligned)");
}

static void testBluPill()
{
    const double sr = 48000, bar = sr * 2.0; // 120 BPM, 4/4
    BluPill::Engine e;
    e.prepare (sr, 6.0);
    e.setSmoothing (0, 0, 0);
    std::vector<float> ph (4096);

    // Flat envelopes: output == input.
    auto x = sine (4096, sr, 330, 0.5f);
    const auto ref = x;
    for (size_t i = 0; i < ph.size(); ++i) ph[i] = (float) (i / bar);
    float* p[1] { x.data() };
    e.process (p, 1, 4096, ph.data(), bar, true);
    float maxErr = 0;
    for (size_t i = 0; i < x.size(); ++i) maxErr = std::max (maxErr, std::abs (x[i] - ref[i]));
    check (maxErr < 1e-6f, "Blu Pill with flat envelopes is transparent");

    // Following the diagonal = tape stopped: a rising ramp in, a held value out.
    e.prepare (sr, 6.0);
    e.setSmoothing (0, 0, 0);
    e.setEnvelopes ({ { 0, 0, 0 }, { 1, 1, 0 } }, { { 0, 0, 0 }, { 1, 0, 0 } });
    std::vector<float> ramp (4096);
    for (size_t i = 0; i < ramp.size(); ++i) ramp[i] = (float) i / 4096.0f;
    float* q[1] { ramp.data() };
    e.process (q, 1, 4096, ph.data(), bar, true);
    check (std::abs (ramp[4000] - ramp[2000]) < 0.01f, "Blu Pill time stop holds the sound still");

    // Volume all the way down: silence.
    e.prepare (sr, 6.0);
    e.setSmoothing (0, 0, 0);
    e.setEnvelopes ({ { 0, 0, 0 }, { 1, 0, 0 } }, { { 0, 1, 0 }, { 1, 1, 0 } });
    auto z = sine (4096, sr, 330, 0.5f);
    float* r[1] { z.data() };
    e.process (r, 1, 4096, ph.data(), bar, true);
    check (rms (z, 2048) < 1e-3f, "Blu Pill volume at the bottom is silent");
}

static void testProcessor()
{
    {
        // A freshly loaded 07BASED must pass audio through untouched (all effects off),
        // just delayed by the reported latency.
        BasedProcessor fresh;
        fresh.setPlayConfigDetails (2, 2, 48000, 512);
        fresh.prepareToPlay (48000, 512);
        const int L = fresh.getLatencySamples();
        const auto in = sine (48000, 48000, 440, 0.5f);
        juce::AudioBuffer<float> b (2, 512);
        juce::MidiBuffer m;
        std::vector<float> out;
        for (size_t at = 0; at + 512 <= in.size(); at += 512)
        {
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < 512; ++i)
                    b.setSample (c, i, in[at + (size_t) i]);
            fresh.processBlock (b, m);
            for (int i = 0; i < 512; ++i)
                out.push_back (b.getSample (0, i));
        }
        float maxErr = 0;
        for (size_t i = (size_t) L; i < out.size(); ++i)
            maxErr = std::max (maxErr, std::abs (out[i] - in[i - (size_t) L]));
        std::printf ("      fresh plugin max difference from input: %g (latency %d)\n", maxErr, L);
        check (maxErr < 1e-5f, "a fresh 07BASED (everything off) passes audio through untouched");
    }

    BasedProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000, 512);
    proc.prepareToPlay (48000, 512);
    check (proc.getLatencySamples() > proc.getRzLatency() && proc.getRzLatency() > 0, "07BASED reports Reggirizer + Trap City latency");

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (3);
    for (auto* id : { "on_rz", "on_bp", "on_gc", "on_tc" })
        proc.apvts.getParameter (id)->setValueNotifyingHost (1.0f);
    bool finite = true;
    proc.blast();
    for (int b = 0; b < 200; ++b)
    {
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                buf.setSample (c, i, (rng.nextFloat() * 2 - 1) * 0.3f);
        proc.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < 512; ++i)
                finite = finite && std::isfinite (buf.getSample (c, i));
    }
    check (finite, "the full bus (all four effects + BLAST) outputs finite audio");

    // Odd host block sizes larger than promised still work.
    juce::AudioBuffer<float> big (2, 1500);
    big.clear();
    proc.processBlock (big, midi);
    check (true, "a 1500-sample block (bigger than prepared) doesn't crash");

    proc.setOrder ({ "hr", "td", "t2", "mx" });
    proc.setBluEdits (R"({"time":{"16":[{"x":0,"y":0,"k":0},{"x":1,"y":1,"k":0}]},"vol":{}})");
    proc.setTcPreset (1, 3);
    juce::MemoryBlock state;
    proc.getStateInformation (state);

    BasedProcessor other;
    other.setStateInformation (state.getData(), (int) state.getSize());
    check (other.getOrder().joinIntoString (",") == "hr,td,t2,mx", "bus order survives saving and loading");
    check (other.getTcPreset() == "1,3", "Trap City track preset survives saving and loading");
    check (other.getBluEdits().contains ("\"16\""), "edited Blu Pill slots survive saving and loading");
}

// Every file the UI page asks for must be served from the plugin, using the
// request format JUCE really uses (a path like "/fonts/x.ttf").
static void testUiFiles()
{
    check (BasedUIFiles::fileFor ("/") == "index.html", "web view root serves index.html");
    check (BasedUIFiles::fileFor ("/fonts/LilitaOne.ttf") == "LilitaOne.ttf", "path-style request maps to the file name");
    check (BasedUIFiles::fileFor ("juce://juce.backend/bridge.js?x=1") == "bridge.js", "full-URL request maps to the file name");

    int size = 0;
    const char* html = BasedUIFiles::find ("index.html", size);
    check (html != nullptr && size > 1000, "index.html is bundled");
    if (html == nullptr) return;
    const juce::String page (juce::CharPointer_UTF8 (html), (size_t) size);
    const char* js = BasedUIFiles::find ("bridge.js", size);
    const juce::String bridge = js ? juce::String (juce::CharPointer_UTF8 (js), (size_t) size) : juce::String();

    // Collect url(...), src="..." and import "..." references that point at local files.
    juce::StringArray refs { "/juce/index.js", "/juce/check_native_interop.js" };
    auto scan = [&refs] (const juce::String& text, const juce::String& open, juce::juce_wchar close)
    {
        for (int i = text.indexOf (open); i >= 0; i = text.indexOf (i + 1, open))
        {
            auto ref = text.substring (i + open.length()).upToFirstOccurrenceOf (juce::String::charToString (close), false, false).trimCharactersAtStart ("\"'").trimCharactersAtEnd ("\"'");
            if (ref.isNotEmpty() && ! ref.startsWith ("#") && ! ref.startsWith ("data:") && ! ref.contains ("://") && ! ref.contains ("$") && ref.containsChar ('.'))
                refs.addIfNotAlreadyThere ("/" + (ref.startsWith ("./") ? ref.substring (2) : ref));
        }
    };
    scan (page, "url(", ')');
    scan (page, "src=\"", '"');
    scan (bridge, "from \"", '"');
    bool all = true;
    for (auto& r : refs)
        if (BasedUIFiles::find (BasedUIFiles::fileFor (r), size) == nullptr)
        {
            std::printf ("      missing from the plugin: %s\n", r.toRawUTF8());
            all = false;
        }
    std::printf ("      %d local files referenced by the UI\n", refs.size());
    check (all && refs.size() >= 12, "every file the UI loads (fonts, images, bridge, JUCE glue) is bundled and found");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testUiFiles();
    testPresets();
    testGrapeCrusher();
    testTrapCity();
    testBluPill();
    testProcessor();
    std::printf ("\n%s (%d failed)\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
