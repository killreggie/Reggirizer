// Offline checks for the flanger: stays stable at extreme feedback, makes the
// expected comb notch, and leaves the signal untouched when off.
#include "../Source/Flanger.h"
#include <cmath>
#include <cstdio>
#include <random>
#include <vector>

static double rms (const std::vector<float>& x, size_t from)
{
    double e = 0; for (size_t i = from; i < x.size(); ++i) e += x[i] * x[i];
    return std::sqrt (e / (double) (x.size() - from));
}

int main()
{
    int fails = 0;
    const double sr = 48000;

    // 1. Stability: noise through 8 voices at +/-95% feedback, full depth, fast rate.
    for (float fb : { 0.95f, -0.95f })
    {
        Flanger f; f.prepare (sr, 2);
        Flanger::Settings s; s.on = true; s.voices = 8; s.depth = 1; s.rateHz = 6; s.delayMs = 10; s.spread = 1; s.feedback = fb; s.wet = 1;
        f.setSettings (s); f.reset();
        std::mt19937 rng (1); std::uniform_real_distribution<float> u (-0.5f, 0.5f);
        std::vector<float> L ((size_t) sr * 5), R (L.size());
        for (size_t i = 0; i < L.size(); ++i) { L[i] = u (rng); R[i] = u (rng); }
        for (size_t i = 0; i < L.size(); i += 256) { float* p[2] = { L.data() + i, R.data() + i }; f.process (p, 2, (int) std::min<size_t> (256, L.size() - i)); }
        float peak = 0; bool finite = true;
        for (size_t i = 0; i < L.size(); ++i) { peak = std::max ({ peak, std::fabs (L[i]), std::fabs (R[i]) }); finite &= std::isfinite (L[i]) && std::isfinite (R[i]); }
        const bool ok = finite && peak < 6.0f;
        fails += ! ok;
        std::printf ("Stability, feedback %+.0f%%: peak %.2f %s  %s\n", fb * 100, peak, finite ? "(finite)" : "(NaN!)", ok ? "PASS" : "FAIL");
    }

    // 2. Comb notch: 1 ms fixed delay cancels a 500 Hz tone (half a period late).
    {
        Flanger f; f.prepare (sr, 1);
        Flanger::Settings s; s.on = true; s.voices = 1; s.depth = 0; s.delayMs = 1; s.spread = 0; s.feedback = 0; s.wet = 1;
        f.setSettings (s); f.reset();
        std::vector<float> x ((size_t) sr);
        for (size_t i = 0; i < x.size(); ++i) x[i] = 0.5f * (float) std::sin (2 * M_PI * 500 * (double) i / sr);
        const double in = rms (x, x.size() / 2);
        for (size_t i = 0; i < x.size(); i += 256) { float* p = x.data() + i; f.process (&p, 1, (int) std::min<size_t> (256, x.size() - i)); }
        const double db = 20 * std::log10 (rms (x, x.size() / 2) / in);
        const bool ok = db < -8;
        fails += ! ok;
        std::printf ("Comb notch at 500 Hz with 1 ms delay: %.1f dB  %s\n", db, ok ? "PASS" : "FAIL");
    }

    // 3. Off = untouched.
    {
        Flanger f; f.prepare (sr, 1);
        Flanger::Settings s; s.on = false; f.setSettings (s); f.reset();
        std::vector<float> x (48000), orig;
        for (size_t i = 0; i < x.size(); ++i) x[i] = (float) std::sin (0.01 * (double) i);
        orig = x;
        for (size_t i = 0; i < x.size(); i += 256) { float* p = x.data() + i; f.process (&p, 1, (int) std::min<size_t> (256, x.size() - i)); }
        float diff = 0; for (size_t i = 0; i < x.size(); ++i) diff = std::max (diff, std::fabs (x[i] - orig[i]));
        const bool ok = diff == 0.0f;
        fails += ! ok;
        std::printf ("Off leaves audio untouched: max diff %g  %s\n", diff, ok ? "PASS" : "FAIL");
    }
    return fails;
}
