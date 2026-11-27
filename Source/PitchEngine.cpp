#include "PitchEngine.h"
#include "Scales.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float pi = 3.14159265358979f;

    float frac (float x) { return x - std::floor (x); }
}

void PitchEngine::prepare (double sampleRate, int numChannels)
{
    sr = sampleRate;
    channels = std::clamp (numChannels, 1, maxChannels);

    decim = std::max (1, (int) std::round (sr / 22050.0));
    analysisRate = sr / decim;
    minLag = std::max (2, (int) (analysisRate / 1200.0));            // up to ~1.2 kHz
    maxLag = std::min (analysisLen - yinWindow - 2, (int) (analysisRate / 60.0)); // down to ~60 Hz

    windowLen = (float) std::round (sr * 0.028); // 28 ms grains
    restDelay = (int) std::round (minDelay + windowLen * 0.5f);

    for (auto& b : delayBufs)
        b.assign ((size_t) delaySize, 0.0f);

    reset();
}

void PitchEngine::reset()
{
    for (auto& b : delayBufs)
        std::fill (b.begin(), b.end(), 0.0f);

    analysisBuf.fill (0.0f);
    analysisWrite = hopCount = decimCount = 0;
    decimAccum = lpState = 0.0f;
    writePos = 0;
    phase = 0.0f;
    headOffset[0] = headOffset[1] = 0.0f;
    periodSamples = 0.0f;
    ratio = 1.0f;
    desiredCorrection = smoothedCorrection = 0.0f;
    clearTarget();

    detectedHz = 0.0f;
    detectedMidi = -1.0f;
    targetMidi = -1;
    correctionCents = 0.0f;
}

float PitchEngine::readHermite (const std::vector<float>& buf, float delay) const
{
    const float readPos = (float) writePos - delay;
    const float fl = std::floor (readPos);
    const float t = readPos - fl;
    const int i = (int) fl;
    constexpr int mask = delaySize - 1;

    const float y0 = buf[(size_t) ((i - 1) & mask)];
    const float y1 = buf[(size_t) (i & mask)];
    const float y2 = buf[(size_t) ((i + 1) & mask)];
    const float y3 = buf[(size_t) ((i + 2) & mask)];

    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}

float PitchEngine::runYin (float& confidence)
{
    confidence = 0.0f;

    // Unwrap the ring buffer into chronological order.
    for (int i = 0; i < analysisLen; ++i)
        ordered[(size_t) i] = analysisBuf[(size_t) ((analysisWrite + i) % analysisLen)];

    // Analyse the most recent samples.
    const float* x = ordered.data() + (analysisLen - yinWindow - maxLag);

    float energy = 0.0f;
    for (int j = 0; j < yinWindow; ++j)
        energy += x[j] * x[j];

    if (std::sqrt (energy / yinWindow) < 0.0025f) // about -52 dBFS: treat as silence
        return 0.0f;

    // Difference function + cumulative mean normalisation.
    yinBuf[0] = 1.0f;
    float running = 0.0f;
    for (int tau = 1; tau <= maxLag; ++tau)
    {
        float d = 0.0f;
        for (int j = 0; j < yinWindow; ++j)
        {
            const float diff = x[j] - x[j + tau];
            d += diff * diff;
        }
        running += d;
        yinBuf[(size_t) tau] = running > 0.0f ? d * (float) tau / running : 1.0f;
    }

    constexpr float threshold = 0.15f;
    int best = -1;
    for (int tau = minLag; tau <= maxLag; ++tau)
    {
        if (yinBuf[(size_t) tau] < threshold)
        {
            while (tau + 1 <= maxLag && yinBuf[(size_t) (tau + 1)] < yinBuf[(size_t) tau])
                ++tau;
            best = tau;
            break;
        }
    }

    if (best < 0)
    {
        // Fall back to the global minimum but only trust it if it's decent.
        best = minLag;
        for (int tau = minLag; tau <= maxLag; ++tau)
            if (yinBuf[(size_t) tau] < yinBuf[(size_t) best])
                best = tau;

        if (yinBuf[(size_t) best] > 0.3f)
            return 0.0f;
    }

    confidence = 1.0f - yinBuf[(size_t) best];

    // Parabolic interpolation for sub-sample accuracy.
    float refined = (float) best;
    if (best > 1 && best < maxLag)
    {
        const float a = yinBuf[(size_t) (best - 1)];
        const float b = yinBuf[(size_t) best];
        const float c = yinBuf[(size_t) (best + 1)];
        const float denom = a - 2.0f * b + c;
        if (std::abs (denom) > 1e-9f)
            refined += 0.5f * (a - c) / denom;
    }

    return (float) (analysisRate / refined);
}

void PitchEngine::clearTarget()
{
    currentTarget = pendingTarget = -1;
    pendingTime = 0.0f;
    unvoicedFor = 0.0f;
    lastMidi = -1.0f;
    midiSlope = 0.0f;
    heldFor = 0.0f;
    desiredCorrection = 0.0f;
}

int PitchEngine::chooseTarget (float midi, float hopSeconds)
{
    const auto& scale = Scales::all[(size_t) std::clamp (settings.scale, 0, (int) Scales::all.size() - 1)];

    auto allowed = [&] (int note)
    {
        if (settings.useMask)
            return settings.mask[(size_t) ((note % 12 + 12) % 12)];
        const int degree = ((note - settings.key) % 12 + 12) % 12;
        return scale.notes[(size_t) degree];
    };

    int nearest = -1;
    float nearestDist = 1e9f;
    const int centre = (int) std::round (midi);
    for (int n = centre - 6; n <= centre + 6; ++n)
    {
        if (! allowed (n))
            continue;
        const float dist = std::abs (midi - (float) n);
        if (dist < nearestDist)
        {
            nearestDist = dist;
            nearest = n;
        }
    }

    auto take = [this] (int note)
    {
        pendingTarget = -1;
        pendingTime = 0.0f;
        return note;
    };

    if (currentTarget < 0 || ! allowed (currentTarget) || nearest == currentTarget)
        return take (nearest);

    // Note stabilizer: how far past the halfway point the singer must go, and
    // for how long, before the target is allowed to change note.
    struct Stab { float margin, holdSec; };
    constexpr Stab stabs[] = { { 0.05f, 0.0f }, { 0.2f, 0.03f }, { 0.35f, 0.08f }, { 0.5f, 0.16f } };
    const auto st = stabs[std::clamp (settings.stabilizer, 0, 3)];

    const float currentDist = std::abs (midi - (float) currentTarget);

    // A real leap to another note always switches straight away.
    if (currentDist >= 1.5f)
        return take (nearest);

    // Inside the hysteresis band: stay put.
    if (currentDist <= nearestDist + st.margin)
        return take (currentTarget);

    // Outside it: the new note has to stay closest for holdSec first.
    if (pendingTarget == nearest)
        pendingTime += hopSeconds;
    else
    {
        pendingTarget = nearest;
        pendingTime = hopSeconds;
    }

    if (pendingTime >= st.holdSec)
        return take (nearest);

    return currentTarget;
}

void PitchEngine::analyseHop()
{
    float confidence = 0.0f;
    const float hz = runYin (confidence);
    const float hopSeconds = (float) (hop / analysisRate);

    if (hz > 0.0f && confidence > 0.7f)
    {
        unvoicedFor = 0.0f;
        const float midi = 69.0f + 12.0f * std::log2 (hz / 440.0f);

        // Smoothed pitch slope, used by negative speeds to look ahead.
        if (lastMidi > 0.0f && std::abs (midi - lastMidi) < 1.0f)
            midiSlope += 0.5f * ((midi - lastMidi) / hopSeconds - midiSlope);
        else
            midiSlope = 0.0f;
        lastMidi = midi;

        const int previousTarget = currentTarget;
        currentTarget = chooseTarget (midi, hopSeconds);
        heldFor = (currentTarget >= 0 && currentTarget == previousTarget) ? heldFor + hopSeconds : 0.0f;

        // -3 ms looks up to 15 ms ahead, about the detector's lag behind the output.
        float predicted = midi;
        if (settings.speedMs < 0.0f)
        {
            const float lookAheadSec = std::min (-settings.speedMs, 3.0f) / 3.0f * 0.015f;
            predicted += std::clamp (midiSlope * lookAheadSec, -0.5f, 0.5f);
        }

        // No allowed note at all (every key switched off): leave the voice alone.
        float correction = currentTarget >= 0 ? (float) currentTarget - predicted : 0.0f;
        correction = std::clamp (correction, -2.0f, 2.0f) * settings.amount;
        desiredCorrection = correction;

        periodSamples = (float) (sr / hz);
        detectedHz = hz;
        detectedMidi = midi;
        targetMidi = currentTarget;
    }
    else
    {
        // No pitch (breath, consonant, silence). Keep the locked note and its
        // correction for the sustain time so the next syllable doesn't scoop in.
        unvoicedFor += hopSeconds;
        periodSamples = 0.0f;
        detectedHz = 0.0f;
        detectedMidi = -1.0f;
        lastMidi = -1.0f;
        midiSlope = 0.0f;

        if (unvoicedFor * 1000.0f >= settings.sustainMs)
        {
            clearTarget();
            targetMidi = -1;
        }
    }
}

void PitchEngine::process (float* const* chans, int numChannels, int numSamples)
{
    numChannels = std::min (numChannels, channels);
    if (numChannels <= 0)
        return;

    // Retune speed -> one-pole smoothing coefficient. At 0 ms or below the
    // correction steps straight to each new value: a jump in shift ratio only
    // changes how fast the delay heads move, so it can't click.
    // Humanize: notes held past ~120 ms ease toward a slower retune (up to +250 ms),
    // so sustained notes keep their natural movement while quick notes still snap.
    const float held = std::clamp ((heldFor - 0.12f) / 0.5f, 0.0f, 1.0f);
    const float tauSec = settings.speedMs * 0.001f + settings.humanize * held * 0.25f;
    const float smoothCoef = tauSec > 0.0f ? 1.0f - std::exp (-1.0f / (tauSec * (float) sr)) : 1.0f;

    // Gentle anti-alias low-pass before decimating for analysis.
    const float lpCoef = 1.0f - std::exp (-2.0f * pi * 4000.0f / (float) sr);

    const float invL = 1.0f / windowLen;
    constexpr int mask = delaySize - 1;

    for (int n = 0; n < numSamples; ++n)
    {
        // ---- analysis path (mono sum) ----
        float mono = 0.0f;
        for (int c = 0; c < numChannels; ++c)
            mono += chans[c][n];
        mono /= (float) numChannels;

        lpState += lpCoef * (mono - lpState);
        decimAccum += lpState;
        if (++decimCount >= decim)
        {
            analysisBuf[(size_t) analysisWrite] = decimAccum / (float) decim;
            analysisWrite = (analysisWrite + 1) % analysisLen;
            decimAccum = 0.0f;
            decimCount = 0;

            if (++hopCount >= hop)
            {
                hopCount = 0;
                analyseHop();
            }
        }

        // ---- correction smoothing ----
        smoothedCorrection += smoothCoef * (desiredCorrection - smoothedCorrection);
        ratio = std::exp2 (smoothedCorrection / 12.0f);

        // When we're not shifting, slowly park the grain phase at 0 so one head
        // carries the full signal (avoids a static comb-filter colour).
        float effectiveRatio = ratio;
        if (std::abs (ratio - 1.0f) < 0.0005f && phase > 0.0005f && phase < 0.9995f)
            effectiveRatio = phase < 0.5f ? 1.004f : 0.996f;

        // ---- shifter ----
        for (int c = 0; c < numChannels; ++c)
            delayBufs[(size_t) c][(size_t) writePos] = chans[c][n];

        const float p1 = phase;
        const float p2 = frac (phase + 0.5f);
        const float s1 = std::sin (pi * p1);
        const float s2 = std::sin (pi * p2);
        const float g1 = s1 * s1;
        const float g2 = s2 * s2;
        const float d1 = minDelay + p1 * windowLen + headOffset[0];
        const float d2 = minDelay + p2 * windowLen + headOffset[1];

        for (int c = 0; c < numChannels; ++c)
        {
            const auto& buf = delayBufs[(size_t) c];
            chans[c][n] = g1 * readHermite (buf, d1) + g2 * readHermite (buf, d2);
        }

        writePos = (writePos + 1) & mask;
        phase = frac (phase + (1.0f - effectiveRatio) * invL);

        // A head that just wrapped is at zero gain, so its offset can change
        // silently: line it up a whole number of periods from the other head.
        const float np1 = phase;
        const float np2 = frac (phase + 0.5f);
        auto realign = [this] (int head, float newBase, float otherDelay)
        {
            if (periodSamples <= 0.0f)
            {
                headOffset[head] = 0.0f;
                return;
            }
            const float diff = otherDelay - newBase;
            headOffset[head] = diff - periodSamples * std::floor (diff / periodSamples);
        };
        if (std::abs (np1 - p1) > 0.5f)
            realign (0, minDelay + np1 * windowLen, minDelay + np2 * windowLen + headOffset[1]);
        if (std::abs (np2 - p2) > 0.5f)
            realign (1, minDelay + np2 * windowLen, minDelay + np1 * windowLen + headOffset[0]);
    }

    correctionCents = smoothedCorrection * 100.0f;
}
