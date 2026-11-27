#pragma once

#include "Biquad.h"
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <cmath>
#include <random>

// Trap City: an 808-friendly distortion.
//
// in -> 808 low-shelf boost -> grit (signal-following noise + bias)
//    -> drive -> [2x oversampled] shaper (Tape / Tube / Clip / Fuzz / Blown)
//    -> DC block -> tone low-pass -> mix with dry -> out
class TrapCity
{
public:
    enum Mode { Tape, Tube, Clip, Fuzz, Blown };

    struct Settings
    {
        float boom808 = .65f, grit = .4f, drive = .7f, tone = .5f, mix = .85f, out = .5f;
        int mode = Clip;
    };

    static float gainFor (float v) { return std::pow (10.0f, (v - 0.5f) * 48.0f / 20.0f); } // -24 .. +24 dB

    // The waveshaper on its own, so the tests (and the UI's scope) agree with the DSP.
    static float shape (float x, int mode, float tone, float& crackle)
    {
        switch (mode)
        {
            case Tape:  return std::tanh (x * 0.8f) / std::tanh (0.8f);
            case Tube:  return std::tanh (x + 0.3f) - std::tanh (0.3f);
            case Fuzz:  return std::copysign (1.0f - std::exp (-std::abs (x) * 2.0f), x);
            case Blown: return std::clamp (std::sin (x * 1.2f) * 1.4f + crackle, -1.0f, 1.0f);
            default:    return std::clamp (x, -1.0f, 1.0f) * (0.6f + tone * 0.4f);
        }
    }

    void prepare (double sampleRate, int maxBlock)
    {
        sr = sampleRate;
        oversampler.initProcessing ((size_t) maxBlock);
        oversampler.reset();
        for (auto& c : ch)
            c = {};
        dryBuf.setSize (2, maxBlock);
        latency = (int) std::lround (oversampler.getLatencyInSamples());
        ring.setSize (2, std::max (1, latency));
        ring.clear();
        ringPos = 0;
    }

    // The linear-phase oversampling filters delay the wet signal by this much; the
    // dry signal (and the switched-off path) is delayed to match.
    int getLatencySamples() const { return latency; }

    void setSettings (const Settings& s)
    {
        set = s;
        const double toneHz = 1000.0 * std::pow (20.0, (double) s.tone);  // 1 kHz .. 20 kHz
        for (auto& c : ch)
        {
            c.boom.setLowShelf (sr, 90.0, s.boom808 * 12.0);
            c.toneLp.setLowPass (sr, toneHz);
            c.dc.setHighPass (sr, 3.0); // low enough not to make clipped 808s overshoot
        }
    }

    // enabled == false: just the matching delay, so switching it on/off never shifts timing.
    void process (float* const* data, int numChannels, int numSamples, bool enabled = true)
    {
        numChannels = std::min (numChannels, 2);
        if (dryBuf.getNumSamples() < numSamples)
            dryBuf.setSize (2, numSamples, false, false, true);

        // Delayed copy of the input: the dry signal, aligned with the wet one.
        const int len = ring.getNumSamples();
        for (int c = 0; c < numChannels; ++c)
        {
            auto* r = ring.getWritePointer (c);
            auto* d = dryBuf.getWritePointer (c);
            for (int n = 0, pos = ringPos; n < numSamples; ++n, pos = (pos + 1) % len)
            {
                d[n] = latency > 0 ? r[pos] : data[c][n];
                r[pos] = data[c][n];
            }
        }
        ringPos = (ringPos + numSamples) % len;
        if (! enabled)
        {
            for (int c = 0; c < numChannels; ++c)
                std::copy (dryBuf.getReadPointer (c), dryBuf.getReadPointer (c) + numSamples, data[c]);
            return;
        }

        const float driveGain = std::pow (10.0f, set.drive * 36.0f / 20.0f);     // 0 .. +36 dB
        for (int c = 0; c < numChannels; ++c)
        {
            auto& st = ch[(size_t) c];
            float* x = data[c];
            for (int n = 0; n < numSamples; ++n)
            {
                float s = st.boom.process (x[n]);
                s += set.grit * (std::abs (s) * (uni (rng) - 0.5f) * 0.5f + 0.05f * s * s);
                x[n] = s * driveGain;
            }
        }

        juce::dsp::AudioBlock<float> block (data, (size_t) numChannels, (size_t) numSamples);
        auto up = oversampler.processSamplesUp (block);
        for (size_t c = 0; c < up.getNumChannels(); ++c)
        {
            float* u = up.getChannelPointer (c);
            for (size_t n = 0; n < up.getNumSamples(); ++n)
            {
                float crackle = 0.0f;
                if (set.mode == Blown && uni (rng) < 0.0015f * (0.2f + set.grit))
                    crackle = (uni (rng) - 0.5f) * 0.6f;
                u[n] = shape (u[n], set.mode, set.tone, crackle);
            }
        }
        oversampler.processSamplesDown (block);

        const float outGain = gainFor (set.out);
        for (int c = 0; c < numChannels; ++c)
        {
            auto& st = ch[(size_t) c];
            float* x = data[c];
            const float* dry = dryBuf.getReadPointer (c);
            for (int n = 0; n < numSamples; ++n)
            {
                const float wet = st.toneLp.process (st.dc.process (x[n]));
                x[n] = (dry[n] + (wet - dry[n]) * set.mix) * outGain;
            }
        }
    }

private:
    struct Channel { Biquad boom, toneLp, dc; };

    Settings set;
    double sr = 44100.0;
    std::array<Channel, 2> ch;
    juce::AudioBuffer<float> dryBuf, ring;
    int ringPos = 0, latency = 0;
    juce::dsp::Oversampling<float> oversampler { 2, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true };
    std::minstd_rand rng { 11 };
    std::uniform_real_distribution<float> uni { 0.0f, 1.0f };
};

// The BLAST button: a synthesised gunshot (noise crack + low thump + room tail)
// mixed onto the plugin's output. Up to four can ring at once.
class Gunshot
{
public:
    void prepare (double sampleRate) { sr = sampleRate; for (auto& v : voices) v.active = false; }

    // Audio thread. Call with the number of shots requested since the last block.
    void trigger()
    {
        for (auto& v : voices)
            if (! v.active)
            {
                v = {};
                v.active = true;
                return;
            }
        voices[0] = {}; // all busy: restart the oldest slot
        voices[0].active = true;
    }

    void process (float* const* data, int numChannels, int numSamples)
    {
        const float dt = (float) (1.0 / sr);
        for (auto& v : voices)
        {
            if (! v.active)
                continue;
            for (int n = 0; n < numSamples; ++n)
            {
                const float t = v.t;
                v.t += dt;
                if (t > 1.6f) { v.active = false; break; }

                // Crack: white noise through a low-pass that sweeps 9 kHz -> 600 Hz.
                const float cut = 9000.0f * std::pow (600.0f / 9000.0f, std::min (1.0f, t / 0.3f));
                const float a = 1.0f - std::exp (-2.0f * 3.14159265f * cut * dt);
                const float crackEnv = 1.3f * std::exp (-t * 14.0f);
                // Thump: a sine falling 160 Hz -> 38 Hz.
                const float f = 160.0f * std::pow (38.0f / 160.0f, std::min (1.0f, t / 0.2f));
                v.ph += f * dt;
                const float thump = std::sin (6.2831853f * v.ph) * std::exp (-t * 16.0f);
                // Tail: darker noise decaying over ~1.5 s, a different draw per side.
                const float tailEnv = 0.18f * std::pow (std::max (0.0f, 1.0f - t / 1.6f), 3.0f);

                for (int c = 0; c < std::min (numChannels, 2); ++c)
                {
                    v.lp[c] += a * ((uni (rng) * 2.0f - 1.0f) - v.lp[c]);
                    v.tail[c] += 0.08f * ((uni (rng) * 2.0f - 1.0f) - v.tail[c]);
                    data[c][n] += 0.5f * (v.lp[c] * crackEnv + thump * 0.9f + v.tail[c] * tailEnv * 4.0f);
                }
            }
        }
    }

private:
    struct Voice { bool active = false; float t = 0.0f, ph = 0.0f; float lp[2] { 0, 0 }, tail[2] { 0, 0 }; };
    double sr = 44100.0;
    std::array<Voice, 4> voices;
    std::minstd_rand rng { 23 };
    std::uniform_real_distribution<float> uni { 0.0f, 1.0f };
};
