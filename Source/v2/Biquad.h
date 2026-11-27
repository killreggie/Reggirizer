#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

// Small RBJ-cookbook biquad (transposed direct form II). Coefficients are set
// once per block; process() runs per sample.
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
    float z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0.0f; }

    float process (float x)
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void setLowPass (double sr, double hz, double q = 0.7071) { design (0, sr, hz, q, 0.0); }
    void setHighPass (double sr, double hz, double q = 0.7071) { design (1, sr, hz, q, 0.0); }
    void setLowShelf (double sr, double hz, double gainDb) { design (2, sr, hz, 0.7071, gainDb); }
    void setHighShelf (double sr, double hz, double gainDb) { design (3, sr, hz, 0.7071, gainDb); }

private:
    void design (int type, double sr, double hz, double q, double gainDb)
    {
        hz = std::clamp (hz, 1.0, sr * 0.49);
        const double w = 2.0 * std::numbers::pi * hz / sr, cw = std::cos (w), sw = std::sin (w);
        double nb0, nb1, nb2, na0, na1, na2;

        if (type <= 1)
        {
            const double alpha = sw / (2.0 * q);
            if (type == 0) { nb0 = (1 - cw) / 2; nb1 = 1 - cw; nb2 = (1 - cw) / 2; }
            else           { nb0 = (1 + cw) / 2; nb1 = -(1 + cw); nb2 = (1 + cw) / 2; }
            na0 = 1 + alpha; na1 = -2 * cw; na2 = 1 - alpha;
        }
        else
        {
            const double A = std::pow (10.0, gainDb / 40.0), alpha = sw / 2.0 * std::sqrt (2.0), sa = 2.0 * std::sqrt (A) * alpha;
            if (type == 2)
            {
                nb0 = A * ((A + 1) - (A - 1) * cw + sa); nb1 = 2 * A * ((A - 1) - (A + 1) * cw); nb2 = A * ((A + 1) - (A - 1) * cw - sa);
                na0 = (A + 1) + (A - 1) * cw + sa;       na1 = -2 * ((A - 1) + (A + 1) * cw);    na2 = (A + 1) + (A - 1) * cw - sa;
            }
            else
            {
                nb0 = A * ((A + 1) + (A - 1) * cw + sa); nb1 = -2 * A * ((A - 1) + (A + 1) * cw); nb2 = A * ((A + 1) + (A - 1) * cw - sa);
                na0 = (A + 1) - (A - 1) * cw + sa;        na1 = 2 * ((A - 1) - (A + 1) * cw);      na2 = (A + 1) - (A - 1) * cw - sa;
            }
        }

        b0 = (float) (nb0 / na0); b1 = (float) (nb1 / na0); b2 = (float) (nb2 / na0);
        a1 = (float) (na1 / na0); a2 = (float) (na2 / na0);
    }
};
