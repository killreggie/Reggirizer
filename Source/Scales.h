#pragma once

#include <array>

// Scales Reggirizer can tune to. Besides the usual major/minor it includes the
// four main prayer modes (shteygers) of Ashkenazi cantorial and klezmer music.
// Intervals are semitones above the key note.
namespace Scales
{
    struct Scale
    {
        const char* englishName;
        const char* hebrewName; // UTF-8, may be empty
        std::array<bool, 12> notes;
    };

    constexpr std::array<bool, 12> mask (std::initializer_list<int> steps)
    {
        std::array<bool, 12> m {};
        for (int s : steps)
            m[(size_t) s] = true;
        return m;
    }

    inline const std::array<Scale, 7> all {{
        { "Chromatic",              "",                  mask ({ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }) },
        { "Major",                  "",                  mask ({ 0, 2, 4, 5, 7, 9, 11 }) },
        { "Minor",                  "",                  mask ({ 0, 2, 3, 5, 7, 8, 10 }) },
        // Freygish: b2 and major 3rd, the classic "Jewish" sound.
        { "Ahava Rabbah (Freygish)", "\xd7\x90\xd7\x94\xd7\x91\xd7\x94 \xd7\xa8\xd7\x91\xd7\x94",           mask ({ 0, 1, 4, 5, 7, 8, 10 }) },
        // Ukrainian Dorian: raised 4th.
        { "Mi Sheberach",           "\xd7\x9e\xd7\x99 \xd7\xa9\xd7\x91\xd7\xa8\xd7\x9a",                   mask ({ 0, 2, 3, 6, 7, 9, 10 }) },
        // Natural-minor based prayer mode.
        { "Magen Avot",             "\xd7\x9e\xd7\x92\xd7\x9f \xd7\x90\xd7\x91\xd7\x95\xd7\xaa",           mask ({ 0, 2, 3, 5, 7, 8, 10 }) },
        // Major with a flat 7th (Mixolydian-like).
        { "Adonai Malach",          "\xd7\x94\xd7\xb3 \xd7\x9e\xd7\x9c\xd7\x9a",                           mask ({ 0, 2, 4, 5, 7, 9, 10 }) },
    }};

    inline const std::array<const char*, 12> keyNames {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
}
