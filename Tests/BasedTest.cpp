// 07BASED v2 engine tests. Build with -DBASED_BUILD_TESTS=ON and run BasedTest.
// Exits non-zero if anything fails.

#include "../Source/v2/BasedProcessor.h"
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
    BasedProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000, 512);
    proc.prepareToPlay (48000, 512);
    check (proc.getLatencySamples() > proc.getRzLatency() && proc.getRzLatency() > 0, "07BASED reports Reggirizer + Trap City latency");

    juce::AudioBuffer<float> buf (2, 512);
    juce::MidiBuffer midi;
    juce::Random rng (3);
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

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    testPresets();
    testGrapeCrusher();
    testTrapCity();
    testBluPill();
    testProcessor();
    std::printf ("\n%s (%d failed)\n", failures ? "SOME TESTS FAILED" : "ALL TESTS PASSED", failures);
    return failures ? 1 : 0;
}
