#pragma once

#include "Biquad.h"
#include <array>
#include <cmath>
#include <random>

// Grapecrusher: a bitcrusher (bit depth + sample-rate reduction) with tone
// shaping. Every knob arrives as 0..1, exactly as the UI shows it.
//
// in -> DC filter -> anti-alias low-pass -> sample & hold (rate, jitter)
//    -> quantiser (bits, dither, noise, mode) -> smooth -> low cut -> high cut
//    -> tilt -> mix with dry -> out -> soft clip
class GrapeCrusher
{
public:
    enum Mode { Round, Trunc, Dither, MuLaw };

    struct Settings
    {
        float bits = .55f, dither = .25f, noise = .1f;      // bit depth section
        float rate = .6f, jitter = .2f, smooth = .3f;       // sample-rate section
        float lowCut = .15f, highCut = .7f, tilt = .5f;     // tone
        float in = .5f, mix = .8f, out = .5f;               // output
        int mode = Round;
        bool antiAlias = true, dcFilter = false, softClip = true;
    };

    // Knob -> real values, shared with the UI's readout and the tests.
    static int bitsFor (float v) { return 1 + (int) std::lround ((1.0f - v) * 11.0f); }    // 12 .. 1 bits
    static float holdFor (float v) { return 1.0f + v * 14.0f; }                             // 1 .. 15 samples
    static float gainFor (float v) { return std::pow (10.0f, (v - 0.5f) * 48.0f / 20.0f); } // -24 .. +24 dB

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        for (auto& c : ch)
            c = {};
        lastHold = -1.0f;
    }

    void setSettings (const Settings& s)
    {
        set = s;
        const float hold = holdFor (s.rate);
        if (std::abs (hold - lastHold) > 1e-6f || s.antiAlias != lastAA)
        {
            for (auto& c : ch)
                c.aa.setLowPass (sr, sr / (2.0 * hold) * 0.9, 0.7071);
            lastHold = hold;
            lastAA = s.antiAlias;
        }
        const double smoothHz = 20000.0 * std::pow (1000.0 / 20000.0, (double) s.smooth);
        const double lowHz = 20.0 * std::pow (50.0, (double) s.lowCut);                   // 20 Hz .. 1 kHz
        const double highHz = 1000.0 * std::pow (20.0, (double) s.highCut);               // 1 kHz .. 20 kHz
        const double tiltDb = (s.tilt - 0.5) * 12.0;                                       // -6 .. +6 dB each side
        for (auto& c : ch)
        {
            c.smooth.setLowPass (sr, smoothHz);
            c.lowCut.setHighPass (sr, lowHz);
            c.highCut.setLowPass (sr, highHz);
            c.tiltLo.setLowShelf (sr, 1000.0, -tiltDb);
            c.tiltHi.setHighShelf (sr, 1000.0, tiltDb);
            c.dc.setHighPass (sr, 10.0);
        }
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const int bits = bitsFor (set.bits);
        const float q = std::pow (2.0f, (float) bits) * 0.5f;    // steps per unit
        const float hold = holdFor (set.rate);
        const float dith = set.dither + (set.mode == Dither ? 0.6f : 0.0f);
        const float inGain = gainFor (set.in), outGain = gainFor (set.out);

        for (int c = 0; c < std::min (numChannels, 2); ++c)
        {
            auto& st = ch[(size_t) c];
            float* x = data[c];
            for (int n = 0; n < numSamples; ++n)
            {
                const float dry = x[n] * inGain;
                float s = dry;
                if (set.dcFilter) s = st.dc.process (s);
                if (set.antiAlias && hold > 1.01f) s = st.aa.process (s);

                // Sample & hold: grab a new value every `hold` samples (jittered).
                st.phase += 1.0f;
                if (st.phase >= st.nextHold)
                {
                    st.phase -= st.nextHold;
                    st.held = quantise (s, q, dith);
                    st.nextHold = std::max (1.0f, hold * (1.0f + (uni (rng) - 0.5f) * set.jitter * 1.6f));
                }

                float y = st.held + (uni (rng) - 0.5f) * set.noise * 0.08f;
                y = st.smooth.process (y);
                y = st.lowCut.process (y);
                y = st.highCut.process (y);
                y = st.tiltHi.process (st.tiltLo.process (y));

                y = (dry + (y - dry) * set.mix) * outGain;
                if (set.softClip)
                    y = std::tanh (y);
                x[n] = y;
            }
        }
    }

private:
    float quantise (float s, float q, float dith)
    {
        if (set.mode == MuLaw)
        {
            const float a = std::min (1.0f, std::abs (s));
            const float c = std::log1p (255.0f * a) / std::log (256.0f);
            const float r = std::round (c * q) / q;
            return std::copysign ((std::pow (256.0f, r) - 1.0f) / 255.0f, s);
        }
        // TPDF dither scaled to one step, then round or truncate to the grid.
        s += (uni (rng) - uni (rng)) * dith / q;
        return (set.mode == Trunc ? std::floor (s * q) : std::round (s * q)) / q;
    }

    struct Channel
    {
        Biquad dc, aa, smooth, lowCut, highCut, tiltLo, tiltHi;
        float phase = 0.0f, nextHold = 1.0f, held = 0.0f;
    };

    Settings set;
    double sr = 44100.0;
    float lastHold = -1.0f;
    bool lastAA = true;
    std::array<Channel, 2> ch;
    std::minstd_rand rng { 7 };
    std::uniform_real_distribution<float> uni { 0.0f, 1.0f };
};
