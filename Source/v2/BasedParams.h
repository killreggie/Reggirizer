#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>

// Every 07BASED parameter. Knob parameters use the same 0..1 (or real) range the
// UI dial shows, so the web UI's normalised value maps straight onto them.
namespace BasedParams
{
    struct Knob { const char* id; const char* name; float min, max, def; };

    // The UI calls knobs "<channel>:<key>"; the parameter id is the channel's
    // prefix + the key with spaces removed (hr:low cut -> gc_lowcut).
    inline const std::array<Knob, 25> knobs {{
        // Reggirizer (t2)
        { "rz_speed",    "Reggirizer Retune Speed (ms)", -3.0f, 400.0f, -3.0f },
        { "rz_amount",   "Reggirizer Amount",             0.0f, 100.0f, 100.0f },
        { "rz_sustain",  "Reggirizer Sustain (ms)",       0.0f, 500.0f, 80.0f },
        { "rz_humanize", "Reggirizer Humanize",           0.0f, 100.0f, 40.0f },
        // Blu Pill (mx)
        { "bp_att", "Blu Pill Attack",  0.0f, 1.0f, .3f },
        { "bp_rel", "Blu Pill Release", 0.0f, 1.0f, .55f },
        { "bp_tns", "Blu Pill Tension", 0.0f, 1.0f, .5f },
        // Grapecrusher (hr)
        { "gc_bits",    "Grapecrusher Bits",      0.0f, 1.0f, .55f },
        { "gc_dither",  "Grapecrusher Dither",    0.0f, 1.0f, 0.0f },
        { "gc_noise",   "Grapecrusher Noise",     0.0f, 1.0f, 0.0f },
        { "gc_rate",    "Grapecrusher Rate",      0.0f, 1.0f, .6f },
        { "gc_jitter",  "Grapecrusher Jitter",    0.0f, 1.0f, .2f },
        { "gc_smooth",  "Grapecrusher Smooth",    0.0f, 1.0f, .3f },
        { "gc_lowcut",  "Grapecrusher Low Cut",   0.0f, 1.0f, .15f },
        { "gc_highcut", "Grapecrusher High Cut",  0.0f, 1.0f, .7f },
        { "gc_tilt",    "Grapecrusher Tilt",      0.0f, 1.0f, .5f },
        { "gc_in",      "Grapecrusher In",        0.0f, 1.0f, .5f },
        { "gc_mix",     "Grapecrusher Mix",       0.0f, 1.0f, .8f },
        { "gc_out",     "Grapecrusher Out",       0.0f, 1.0f, .5f },
        // Trap City (td)
        { "tc_808",   "Trap City 808",   0.0f, 1.0f, .65f },
        { "tc_grit",  "Trap City Grit",  0.0f, 1.0f, .4f },
        { "tc_drive", "Trap City Drive", 0.0f, 1.0f, .7f },
        { "tc_tone",  "Trap City Tone",  0.0f, 1.0f, .5f },
        { "tc_mix",   "Trap City Mix",   0.0f, 1.0f, .85f },
        { "tc_out",   "Trap City Out",   0.0f, 1.0f, .5f },
    }};

    struct Choice { const char* id; const char* name; std::initializer_list<const char*> items; int def; };
    inline const std::array<Choice, 5> choices {{
        { "rz_key",   "Reggirizer Key",   { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }, 4 },
        { "rz_scale", "Reggirizer Scale", { "Note", "Major", "Minor", "Chromatic" }, 2 },
        { "rz_stab",  "Reggirizer Note Stabilizer", { "None", "Short", "Mid", "Long" }, 1 },
        { "gc_mode",  "Grapecrusher Mode", { "Round", "Trunc", "Dither", "Mu-law" }, 0 },
        { "tc_mode",  "Trap City Mode", { "Tape", "Tube", "Clip", "Fuzz", "Blown" }, 2 },
    }};

    struct Toggle { const char* id; const char* name; bool def; };
    inline const std::array<Toggle, 20> toggles {{
        // Every effect starts switched off: a fresh 07BASED passes audio through untouched.
        { "on_rz", "Reggirizer On", false }, { "on_bp", "Blu Pill On", false },
        { "on_gc", "Grapecrusher On", false }, { "on_tc", "Trap City On", false },
        { "gc_aa", "Grapecrusher Anti Alias", true }, { "gc_dc", "Grapecrusher DC Filter", false },
        { "gc_soft", "Grapecrusher Soft Clip Out", true }, { "bp_hold", "Blu Pill Hold", false },
        // Reggirizer keyboard: which notes it tunes to (default E minor)
        { "rz_n0", "Note C", true }, { "rz_n1", "Note C#", false }, { "rz_n2", "Note D", true },
        { "rz_n3", "Note D#", false }, { "rz_n4", "Note E", true }, { "rz_n5", "Note F", false },
        { "rz_n6", "Note F#", true }, { "rz_n7", "Note G", true }, { "rz_n8", "Note G#", false },
        { "rz_n9", "Note A", true }, { "rz_n10", "Note A#", false }, { "rz_n11", "Note B", true },
    }};

    struct Int { const char* id; const char* name; int min, max, def; };
    inline const std::array<Int, 2> ints {{
        { "bp_tslot", "Blu Pill Time Preset",   0, 31, 16 },
        { "bp_vslot", "Blu Pill Volume Preset", 0, 31, 8 },
    }};

    // Effect ids in the UI's bus (the TV channel names) and their on/off parameter.
    inline const std::array<const char*, 4> fxIds { "t2", "mx", "hr", "td" };
    inline const std::array<const char*, 4> fxOnIds { "on_rz", "on_bp", "on_gc", "on_tc" };
}
