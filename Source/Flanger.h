#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

// Multi-voice stereo flanger.
//
// Each voice is a very short delay (fractions of a millisecond up to 10 ms)
// whose length is swept by an LFO. Mixed back with the dry signal this makes a
// moving comb filter. With several voices, their LFOs are staggered and they're
// panned across the stereo field ("spread"), so the notches smear into a wide,
// liquid sheen instead of one sharp jet sweep. Feedback (positive or negative)
// sharpens the notches. The wet path is high-passed so the low end stays solid.
class Flanger
{
public:
    static constexpr int maxVoices = 8;

    struct Settings
    {
        bool on = false;
        int voices = 4;           // 1..8
        float depth = 0.25f;      // 0..1  how far the delay sweeps
        float rateHz = 0.2f;      // LFO speed
        float delayMs = 2.5f;     // centre delay
        float spread = 0.8f;      // 0..1  stereo LFO offset + voice panning
        float feedback = 0.15f;   // -0.95..0.95
        float wet = 0.3f;         // 0..1  added on top of the full dry signal
    };

    void prepare (double sampleRate, int numChannels)
    {
        sr = sampleRate;
        channels = std::clamp (numChannels, 1, 2);
        for (auto& b : bufs)
            b.assign ((size_t) size, 0.0f);
        // One-pole high-pass on the wet signal at ~150 Hz.
        hpCoef = std::exp (-2.0f * 3.14159265f * 150.0f / (float) sr);
        smoothCoef = 1.0f - std::exp (-1.0f / (0.03f * (float) sr)); // 30 ms
        reset();
    }

    void reset()
    {
        for (auto& b : bufs)
            std::fill (b.begin(), b.end(), 0.0f);
        pos = 0;
        lfo = 0.0;
        fbState = { 0.0f, 0.0f };
        hpX = { 0.0f, 0.0f };
        hpY = { 0.0f, 0.0f };
        cur = target;
        cur.wet = target.on ? target.wet : 0.0f;
        cleared = true;
    }

    void setSettings (const Settings& s) { target = s; }

    // LFO position 0..1, for the UI's sweep display (any thread).
    std::atomic<float> lfoDisplay { 0.0f };

    // In place on 1 or 2 channels.
    void process (float* const* chans, int numChannels, int numSamples)
    {
        numChannels = std::min (numChannels, channels);
        const float wetTarget = target.on ? target.wet : 0.0f;

        // Fully off and faded out: skip the work, and clear once so stale audio
        // doesn't leak in when it's switched back on.
        if (wetTarget <= 0.0f && cur.wet < 1.0e-5f)
        {
            if (! cleared)
                reset();
            return;
        }
        cleared = false;

        const int voices = std::clamp (target.voices, 1, maxVoices);
        const float fb = std::clamp (target.feedback, -0.95f, 0.95f);
        constexpr double twoPi = 6.283185307179586;
        constexpr int mask = size - 1;

        // Per-voice pan (balance law: centre = full level on both sides).
        std::array<std::array<float, maxVoices>, 2> gain {};
        float gainSum[2] = { 0.0f, 0.0f };
        for (int v = 0; v < voices; ++v)
        {
            const float pan = voices > 1 ? target.spread * (2.0f * (float) v / (float) (voices - 1) - 1.0f) : 0.0f;
            gain[0][(size_t) v] = std::min (1.0f, 1.0f - pan);
            gain[1][(size_t) v] = std::min (1.0f, 1.0f + pan);
            gainSum[0] += gain[0][(size_t) v];
            gainSum[1] += gain[1][(size_t) v];
        }

        for (int n = 0; n < numSamples; ++n)
        {
            cur.depth   += smoothCoef * (target.depth - cur.depth);
            cur.delayMs += smoothCoef * (target.delayMs - cur.delayMs);
            cur.spread  += smoothCoef * (target.spread - cur.spread);
            cur.wet     += smoothCoef * (wetTarget - cur.wet);

            const float centre = cur.delayMs * 0.001f * (float) sr;

            for (int c = 0; c < numChannels; ++c)
            {
                auto& buf = bufs[(size_t) c];
                const float dry = chans[c][n];
                buf[(size_t) pos] = dry + fb * fbState[(size_t) c];

                // Right channel's LFOs run up to a quarter cycle ahead.
                const double chanOffset = c == 1 ? 0.25 * cur.spread : 0.0;

                float sum = 0.0f;
                for (int v = 0; v < voices; ++v)
                {
                    const double ph = lfo + (double) v / (double) voices + chanOffset;
                    const float sweep = (float) std::sin (twoPi * ph);
                    const float d = std::max (0.5f, centre * (1.0f + 0.95f * cur.depth * sweep));
                    sum += gain[(size_t) (numChannels > 1 ? c : 0)][(size_t) v] * read (buf, d);
                }

                const float g = numChannels > 1 ? gainSum[c] : gainSum[0];
                const float wetSig = g > 0.0f ? sum / g : 0.0f;
                fbState[(size_t) c] = wetSig;

                // High-pass the wet signal before it's mixed in.
                const float hp = hpCoef * (hpY[(size_t) c] + wetSig - hpX[(size_t) c]);
                hpX[(size_t) c] = wetSig;
                hpY[(size_t) c] = hp;

                chans[c][n] = dry + cur.wet * hp;
            }

            pos = (pos + 1) & mask;
            lfo += (double) target.rateHz / sr;
            if (lfo >= 1.0)
                lfo -= 1.0;
        }
        lfoDisplay = (float) lfo;
    }

private:
    float read (const std::vector<float>& buf, float delay) const
    {
        const float rp = (float) pos - delay;
        const float fl = std::floor (rp);
        const float t = rp - fl;
        const int i = (int) fl;
        constexpr int mask = size - 1;
        const float a = buf[(size_t) (i & mask)];
        const float b = buf[(size_t) ((i + 1) & mask)];
        return a + t * (b - a);
    }

    static constexpr int size = 4096; // ~21 ms at 192 kHz, room for 10 ms + full sweep
    std::array<std::vector<float>, 2> bufs;
    int pos = 0, channels = 2;
    double sr = 44100.0, lfo = 0.0;
    float hpCoef = 0.98f, smoothCoef = 0.001f;
    std::array<float, 2> fbState {}, hpX {}, hpY {};
    Settings target, cur;
    bool cleared = true;
};
