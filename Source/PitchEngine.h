#pragma once

#include <array>
#include <atomic>
#include <vector>

// Reggirizer's pitch-correction engine.
//
// 1. Detects the pitch of the incoming voice (YIN algorithm on a decimated copy).
// 2. Picks the nearest note in the chosen key and scale (with hysteresis so it
//    doesn't flicker between notes).
// 3. Glides toward that note at the retune speed. 0 ms is an instant snap;
//    negative speeds (down to -3) also look ahead along the pitch movement to
//    cancel the detector's lag, for the hardest possible lock.
// 4. Shifts the audio by the needed ratio with a crossfading two-head delay line.
//
// Everything here is allocation-free once prepare() has run, so it is safe on
// the audio thread.
class PitchEngine
{
public:
    static constexpr int maxChannels = 2;

    struct Settings
    {
        int key = 0;              // 0 = C ... 11 = B
        int scale = 0;            // index into Scales::all
        float speedMs = 20.0f;    // retune speed, -3..400 ms (negative = look-ahead lock)
        float amount = 1.0f;      // 0..1 how much of the correction to apply
        int stabilizer = 1;       // 0 None, 1 Short, 2 Mid, 3 Long: resistance to note flips
        float sustainMs = 80.0f;  // how long the locked note survives breaths / dropouts
    };

    void prepare (double sampleRate, int numChannels);
    void reset();
    void setSettings (const Settings& s) { settings = s; }

    // Processes up to maxChannels channels in place. Returns wet (shifted) audio;
    // the processor mixes it with dry.
    void process (float* const* channels, int numChannels, int numSamples);

    // Fixed latency of the shifted signal at rest, so dry can be aligned to it.
    int getLatencySamples() const { return restDelay; }

    // Read by the UI (any thread).
    std::atomic<float> detectedHz { 0.0f };
    std::atomic<float> detectedMidi { -1.0f };
    std::atomic<int> targetMidi { -1 };
    std::atomic<float> correctionCents { 0.0f };

private:
    void analyseHop();
    float runYin (float& confidence);
    int chooseTarget (float midi, float hopSeconds);
    void clearTarget();
    float readHermite (const std::vector<float>& buf, float delay) const;

    Settings settings;
    double sr = 44100.0;
    int channels = 2;

    // --- analysis (decimated) ---
    int decim = 2;
    int decimCount = 0;
    float decimAccum = 0.0f;
    float lpState = 0.0f;
    double analysisRate = 22050.0;
    static constexpr int yinWindow = 512;
    static constexpr int analysisLen = 1024;
    static constexpr int hop = 128;
    std::array<float, analysisLen> analysisBuf {};
    std::array<float, analysisLen> ordered {};
    std::array<float, analysisLen / 2 + 1> yinBuf {};
    int analysisWrite = 0;
    int hopCount = 0;
    int minLag = 20, maxLag = 400;

    int currentTarget = -1;
    int pendingTarget = -1;    // candidate note the stabilizer is waiting on
    float pendingTime = 0.0f;  // seconds the candidate has been the closest note
    float unvoicedFor = 0.0f;  // seconds since the last confident pitch
    float lastMidi = -1.0f;    // previous detected pitch, for the look-ahead slope
    float midiSlope = 0.0f;    // semitones per second, smoothed

    // --- correction smoothing ---
    float desiredCorrection = 0.0f; // semitones, updated per hop
    float smoothedCorrection = 0.0f; // semitones, per sample
    float ratio = 1.0f;

    // --- shifter ---
    static constexpr int delaySize = 16384; // power of two
    std::array<std::vector<float>, maxChannels> delayBufs;
    int writePos = 0;
    float windowLen = 1000.0f; // samples
    float phase = 0.0f;        // 0..1
    // Per-head delay offsets, re-chosen whenever a head is silent so both heads
    // stay a whole number of voice periods apart (keeps them in phase: no warble).
    float headOffset[2] { 0.0f, 0.0f };
    float periodSamples = 0.0f; // 0 when unvoiced
    int restDelay = 0;
    static constexpr float minDelay = 4.0f;
};
