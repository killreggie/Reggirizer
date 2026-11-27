#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// Blu Pill: a time + volume pattern effect in the spirit of Gross Beat.
//
// Two envelopes run across one bar, locked to the host's beat position:
//   time   y = how far behind playback to read, in bars (0 = normal, following
//            the diagonal = stopped, steeper = slower, climbing back = catching up)
//   volume y = 0 full volume .. 1 silent
// Each envelope is a list of points {x 0..1, y, k}; k bends the segment that
// starts at that point (k > 0 eases in, k < 0 eases out), the same as the UI.
namespace BluPill
{
    struct Pt { float x, y, k; };
    using Envelope = std::vector<Pt>;

    constexpr float maxBarsBehind = 2.0f; // the time graph's floor (-2)
    constexpr size_t maxPoints = 1024;

    inline float shape (float u, float k)
    {
        return k >= 0.0f ? std::pow (u, 1.0f + k * 3.0f) : 1.0f - std::pow (1.0f - u, 1.0f - k * 3.0f);
    }

    // Same walk as the UI: the first segment whose end is at or past x.
    inline float valueAt (const Envelope& P, float x)
    {
        if (P.empty())
            return 0.0f;
        for (size_t i = 1; i < P.size(); ++i)
            if (x <= P[i].x)
            {
                const auto& a = P[i - 1];
                const auto& b = P[i];
                const float u = b.x > a.x ? (x - a.x) / (b.x - a.x) : 1.0f;
                return a.y + (b.y - a.y) * shape (u, a.k);
            }
        return P.back().y;
    }

    // The 32 built-in time and volume presets, generated exactly like the UI does
    // (same seeded random walk), so a slot sounds like it looks without the UI open.
    inline Envelope preset (bool volume, int slot)
    {
        long long seed = slot * 77 + (volume ? 5 : 1);
        auto r = [&seed] { seed = (seed * 9301 + 49297) % 233280; return (double) seed / 233280.0; };
        std::vector<std::array<double, 2>> pts { { 0.0, 0.0 } };

        if (slot == 0)
            pts.push_back ({ 1.0, 0.0 });
        else if (volume)
        {
            const int steps = std::array<int, 4> { 2, 4, 8, 16 }[(size_t) (slot % 4)];
            for (int k = 0; k < steps; ++k)
            {
                const double x0 = (double) k / steps, x1 = (double) (k + 1) / steps;
                const double lo = r() < .55 ? .1 + r() * .9 : 0.0;
                pts.push_back ({ x0 + .002, 0.0 });
                const double xa = x0 + (x1 - x0) * (.35 + r() * .4);
                pts.push_back ({ xa, 0.0 });
                const double xb = x0 + (x1 - x0) * (.4 + r() * .4);
                pts.push_back ({ xb, lo });
                pts.push_back ({ x1 - .002, lo });
            }
            pts.push_back ({ 1.0, 0.0 });
        }
        else
        {
            double x = 0.0, y = 0.0;
            while (x < 1.0)
            {
                const double w = .06 + r() * .14, k = r();
                x = std::min (1.0, x + w);
                if (k < .35) y += w;
                else if (k < .55) y += w * (.3 + r() * .5);
                else if (k < .7) y = std::max (0.0, y - w * .8);
                else if (k < .8) y += .15 + r() * .2;
                pts.push_back ({ x, y });
            }
        }

        Envelope out;
        const float yMax = volume ? 1.0f : maxBarsBehind;
        for (size_t i = 0; i < pts.size(); ++i)
            out.push_back ({ (float) pts[i][0], std::min (yMax, (float) pts[i][1]), volume ? 0.0f : (i % 2 ? .35f : -.35f) });
        return out;
    }

    // The audio engine. The caller passes the bar phase for every sample.
    class Engine
    {
    public:
        void prepare (double sampleRate, double maxBarSeconds)
        {
            sr = sampleRate;
            int len = 1;
            while (len < (int) (sampleRate * maxBarSeconds * maxBarsBehind) + 8)
                len <<= 1;
            for (auto& b : buf)
                b.assign ((size_t) len, 0.0f);
            mask = len - 1;
            write = 0;
            timeEnv.reserve (maxPoints);   // so setEnvelopes() never allocates on the audio thread
            volEnv.reserve (maxPoints);
            delay = 0.0f;
            gain = 1.0f;
        }

        void setEnvelopes (const Envelope& t, const Envelope& v) { timeEnv = t; volEnv = v; }
        void setSmoothing (float att, float rel, float tns) { attK = att; relK = rel; tnsK = tns; }

        // active == false (transport stopped, no hold): glide back to plain playback.
        void process (float* const* data, int numChannels, int numSamples, const float* phase, double barSamples, bool active)
        {
            numChannels = std::min (numChannels, 2);
            const float aDelay = coef (0.2f + tnsK * 40.0f);        // time smoothing
            const float aUp = coef (0.1f + attK * 50.0f);          // volume attack
            const float aDown = coef (0.1f + relK * 100.0f);       // volume release
            const float maxDelay = (float) (mask - 4);

            for (int n = 0; n < numSamples; ++n)
            {
                const float ph = phase[n];
                const float targetDelay = active ? std::clamp ((float) (valueAt (timeEnv, ph) * barSamples), 0.0f, maxDelay) : 0.0f;
                const float targetGain = active ? 1.0f - std::clamp (valueAt (volEnv, ph), 0.0f, 1.0f) : 1.0f;
                delay += (targetDelay - delay) * aDelay;
                gain += (targetGain - gain) * (targetGain > gain ? aUp : aDown);

                for (int c = 0; c < numChannels; ++c)
                {
                    auto& b = buf[(size_t) c];
                    b[(size_t) write] = data[c][n];
                    data[c][n] = read (b, delay) * gain;
                }
                write = (write + 1) & mask;
            }
        }

        float currentGain() const { return gain; }

    private:
        float coef (float ms) const { return 1.0f - std::exp (-1.0f / (ms * 0.001f * (float) sr)); }

        float read (const std::vector<float>& b, float d) const
        {
            if (d < 0.0005f)
                return b[(size_t) write];
            const float pos = (float) write - d;
            const int i = (int) std::floor (pos);
            const float f = pos - (float) i;
            auto at = [&] (int k) { return b[(size_t) ((i + k) & mask)]; };
            if (d < 2.0f) // too close to the write head for 4 points: stay linear
                return at (0) + (at (1) - at (0)) * f;
            const float y0 = at (-1), y1 = at (0), y2 = at (1), y3 = at (2);
            // 4-point Hermite
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * f + c2) * f + c1) * f + y1;
        }

        double sr = 44100.0;
        std::array<std::vector<float>, 2> buf;
        int mask = 0, write = 0;
        float delay = 0.0f, gain = 1.0f;
        float attK = .3f, relK = .55f, tnsK = .5f;
        Envelope timeEnv { { 0, 0, 0 }, { 1, 0, 0 } }, volEnv { { 0, 0, 0 }, { 1, 0, 0 } };
    };
}
