// Offline check: feed out-of-tune tones through PitchEngine and measure the output pitch.
#include "../Source/PitchEngine.h"
#include <cmath>
#include <cstdio>
#include <vector>

static double measureHz (const std::vector<float>& x, double sr, int start, int len)
{
    // Zero-crossing count with linear interpolation.
    double first = -1, last = -1; int crossings = 0;
    for (int i = start + 1; i < start + len; ++i)
        if (x[i - 1] < 0 && x[i] >= 0)
        {
            double t = (i - 1) + (-x[i - 1]) / (x[i] - x[i - 1]);
            if (first < 0) first = t; else ++crossings;
            last = t;
        }
    return crossings > 0 ? crossings * sr / (last - first) : 0;
}

static double wobbleDb = 0;
static double run (double sr, double inHz, int key, int scale, float speed, float amount = 1.0f)
{
    PitchEngine e; e.prepare (sr, 1);
    PitchEngine::Settings s; s.key = key; s.scale = scale; s.speedMs = speed; s.amount = amount; e.setSettings (s);
    const int n = (int) (sr * 4.0);
    std::vector<float> x (n);
    for (int i = 0; i < n; ++i) x[i] = 0.5f * (float) std::sin (2 * M_PI * inHz * i / sr);
    for (int i = 0; i < n; i += 512) { float* p = x.data() + i; e.process (&p, 1, std::min (512, n - i)); }
    // Volume steadiness: RMS in 20 ms windows over the steady part.
    const int w = (int) (sr * 0.02); double lo = 1e9, hi = 0;
    for (int i = (int) sr; i + w < (int) (sr * 3.9); i += w)
    { double e = 0; for (int j = 0; j < w; ++j) e += x[i + j] * x[i + j]; e = std::sqrt (e / w); lo = std::min (lo, e); hi = std::max (hi, e); }
    wobbleDb = 20 * std::log10 (hi / lo);
    return measureHz (x, sr, (int) (sr * 1.0), (int) (sr * 2.9));
}

// Wobbly note sitting near the A4/A#4 boundary: count how often the target flips.
static int targetFlips (int stabilizer)
{
    const double sr = 48000;
    PitchEngine e; e.prepare (sr, 1);
    PitchEngine::Settings s; s.speedMs = 0; s.stabilizer = stabilizer; e.setSettings (s);
    const int n = (int) (sr * 4.0), block = 256;
    std::vector<float> x (n);
    double ph = 0;
    for (int i = 0; i < n; ++i)
    {
        const double midi = 69.45 + 0.25 * std::sin (2 * M_PI * 5.5 * i / sr); // vibrato across the boundary
        ph += 2 * M_PI * 440.0 * std::pow (2.0, (midi - 69) / 12) / sr;
        x[i] = 0.5f * (float) std::sin (ph);
    }
    int flips = 0, last = -1;
    for (int i = 0; i < n; i += block)
    {
        float* p = x.data() + i; e.process (&p, 1, std::min (block, n - i));
        const int t = e.targetMidi.load();
        if (i > sr * 0.5 && t >= 0 && last >= 0 && t != last) ++flips;
        if (t >= 0) last = t;
    }
    return flips;
}

// Tone, 60 ms of silence (a breath), tone again: does the target survive the gap?
static bool targetHeldThroughGap (float sustainMs)
{
    const double sr = 48000;
    PitchEngine e; e.prepare (sr, 1);
    PitchEngine::Settings s; s.speedMs = 0; s.sustainMs = sustainMs; e.setSettings (s);
    const int n = (int) (sr * 1.0), block = 128;
    std::vector<float> x (n);
    for (int i = 0; i < n; ++i)
    {
        const bool gap = i > sr * 0.5 && i < sr * 0.56;
        x[i] = gap ? 0.0f : 0.5f * (float) std::sin (2 * M_PI * 450.0 * i / sr);
    }
    bool held = true;
    for (int i = 0; i < n; i += block)
    {
        float* p = x.data() + i; e.process (&p, 1, std::min (block, n - i));
        if (i > sr * 0.52 && i < sr * 0.56 && e.targetMidi.load() < 0) held = false;
    }
    return held;
}

int main()
{
    struct Case { double sr, in; int key, scale; float speed; double expect; const char* name; };
    const Case cases[] = {
        { 44100, 450.0,  0, 0, 0,   440.00, "450 Hz -> A4 chromatic, hard tune" },
        { 48000, 450.0,  0, 0, 20,  440.00, "450 Hz -> A4 chromatic, 20 ms" },
        { 48000, 210.0,  0, 0, 0,   207.65, "210 Hz -> G#3 chromatic" },
        { 48000, 300.0,  2, 3, 0,   293.66, "300 Hz in D Ahava Rabbah -> D4" },
        { 48000, 325.0,  2, 3, 0,   311.13, "325 Hz in D Ahava Rabbah -> Eb4 (flat 2nd)" },
        { 96000, 130.0,  0, 1, 0,   130.81, "130 Hz C major @96k -> C3" },
        { 48000, 440.0,  0, 0, 0,   440.00, "440 Hz already in tune (no change)" },
        { 48000, 450.0,  0, 0, -3,  440.00, "450 Hz -> A4, negative speed -3" },
    };
    int fails = 0;
    for (auto& c : cases)
    {
        double out = run (c.sr, c.in, c.key, c.scale, c.speed);
        double cents = 1200 * std::log2 (out / c.expect);
        bool ok = std::fabs (cents) < 5 && wobbleDb < 1.5;
        fails += ! ok;
        std::printf ("%-46s in %7.2f  out %7.2f  target %7.2f  (%+5.1f cents, volume wobble %.2f dB) %s\n", c.name, c.in, out, c.expect, cents, wobbleDb, ok ? "PASS" : "FAIL");
    }

    const int flipsNone = targetFlips (0), flipsLong = targetFlips (3);
    const bool stabOk = flipsLong < flipsNone && flipsLong <= 2;
    fails += ! stabOk;
    std::printf ("Stabilizer on wobbly note: None %d flips, Long %d flips  %s\n", flipsNone, flipsLong, stabOk ? "PASS" : "FAIL");

    const bool heldLong = targetHeldThroughGap (120), heldShort = targetHeldThroughGap (0);
    const bool susOk = heldLong && ! heldShort;
    fails += ! susOk;
    std::printf ("Sustain through 60 ms breath: 120 ms %s, 0 ms %s  %s\n", heldLong ? "held" : "dropped", heldShort ? "held" : "dropped", susOk ? "PASS" : "FAIL");
    return fails;
}
