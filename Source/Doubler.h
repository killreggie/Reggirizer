#pragma once

#include <cmath>
#include <vector>

// Two extra copies of the tuned voice, each slightly late and slowly drifting in
// pitch (a few cents) so they sound like separate takes, panned apart by Width.
class Doubler
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        buf.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        pos = 0;
        lfo1 = 0.0;
        lfo2 = 0.37; // start the two voices out of step
        level = targetLevel;
        width = targetWidth;
    }

    void setLevel (float l) { targetLevel = l; } // 0..1
    void setWidth (float w) { targetWidth = w; } // 0..1

    // Adds the doubles to out. in is the mono tuned voice. outR may be null (mono bus).
    void process (const float* in, float* outL, float* outR, int numSamples)
    {
        constexpr double twoPi = 6.283185307179586;
        const float smooth = 1.0f - std::exp (-1.0f / (0.03f * (float) sr)); // 30 ms

        for (int n = 0; n < numSamples; ++n)
        {
            buf[(size_t) pos] = in[n];

            level += smooth * (targetLevel - level);
            width += smooth * (targetWidth - width);

            if (level > 1.0e-4f)
            {
                // Voice 1: ~17 ms late, Voice 2: ~24 ms late, each swaying about 1 ms.
                const float v1 = read (0.017 * sr + 0.0009 * sr * std::sin (twoPi * lfo1));
                const float v2 = read (0.024 * sr + 0.0010 * sr * std::sin (twoPi * lfo2));

                const float g = 0.7f * level;
                if (outR == nullptr)
                {
                    outL[n] += g * 0.7071f * (v1 + v2);
                }
                else
                {
                    // Equal-power pan: voice 1 goes left, voice 2 right, by width.
                    const float a1 = (1.0f - width) * 0.7853982f;          // pi/4 = centre
                    const float a2 = (1.0f + width) * 0.7853982f;
                    outL[n] += g * (std::cos (a1) * v1 + std::cos (a2) * v2);
                    outR[n] += g * (std::sin (a1) * v1 + std::sin (a2) * v2);
                }
            }

            lfo1 += 0.83 / sr;
            lfo2 += 0.71 / sr;
            if (lfo1 >= 1.0) lfo1 -= 1.0;
            if (lfo2 >= 1.0) lfo2 -= 1.0;
            pos = (pos + 1) & (size - 1);
        }
    }

private:
    float read (double delay) const
    {
        const double rp = (double) pos - delay;
        const double fl = std::floor (rp);
        const float t = (float) (rp - fl);
        const int i = (int) fl;
        const float a = buf[(size_t) (i & (size - 1))];
        const float b = buf[(size_t) ((i + 1) & (size - 1))];
        return a + t * (b - a);
    }

    static constexpr int size = 8192; // > 26 ms at 192 kHz
    std::vector<float> buf;
    int pos = 0;
    double sr = 44100.0, lfo1 = 0.0, lfo2 = 0.0;
    float level = 0.0f, targetLevel = 0.0f, width = 0.7f, targetWidth = 0.7f;
};
