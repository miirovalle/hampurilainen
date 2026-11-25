#include "DrumMap.h"

#include <juce_audio_basics/juce_audio_basics.h>

namespace loopdeck
{

juce::StringArray getDrumMapPresetNames()
{
    return { "As recorded", "GM Standard", "TR-808 style", "TR-909 style", "MPC style", "Custom" };
}

juce::String getDrumMapPresetName (int presetIndex)
{
    const auto names = getDrumMapPresetNames();
    return juce::isPositiveAndBelow (presetIndex, names.size()) ? names[presetIndex] : names[0];
}

DrumMap makeIdentityDrumMap()
{
    DrumMap map {};

    for (int i = 0; i < 128; ++i)
        map[(size_t) i] = (juce::uint8) i;

    return map;
}

namespace
{
    struct MapEntry
    {
        int from;
        int to;
    };

    // GM percussion uses 35-81. The hardware presets below only redefine the
    // notes where the common machines differ, everything else passes through.
    // They are starting points: every entry can be edited in the note map grid.
    void applyEntries (DrumMap& map, const MapEntry* entries, size_t numEntries)
    {
        for (size_t i = 0; i < numEntries; ++i)
        {
            const auto& entry = entries[i];

            if (juce::isPositiveAndBelow (entry.from, 128) && juce::isPositiveAndBelow (entry.to, 128))
                map[(size_t) entry.from] = (juce::uint8) entry.to;
        }
    }

    // TR-808: bass drum sits on 35, and the 808 family uses its own tom/crash
    // layout. Notes the 808 does not have fall back to the closest voice.
    constexpr MapEntry tr808Entries[] =
    {
        { 36, 35 }, { 38, 38 }, { 39, 39 }, { 37, 37 },
        { 40, 38 }, { 41, 45 }, { 43, 47 }, { 45, 45 }, { 47, 47 }, { 48, 50 }, { 50, 50 },
        { 42, 42 }, { 44, 42 }, { 46, 46 },
        { 49, 49 }, { 52, 49 }, { 55, 49 }, { 57, 49 },
        { 51, 51 }, { 53, 51 }, { 59, 51 },
        { 54, 54 }, { 56, 56 }, { 70, 70 }, { 75, 75 },
        { 60, 60 }, { 61, 61 }, { 62, 62 }, { 63, 63 }, { 64, 64 }
    };

    // TR-909 keeps the kick on 36 but has a distinct rim/clap/tom layout.
    constexpr MapEntry tr909Entries[] =
    {
        { 35, 36 }, { 37, 37 }, { 38, 38 }, { 39, 39 }, { 40, 40 },
        { 41, 43 }, { 43, 41 }, { 45, 45 }, { 47, 47 }, { 48, 48 }, { 50, 50 },
        { 42, 42 }, { 44, 42 }, { 46, 46 },
        { 49, 49 }, { 52, 49 }, { 55, 49 }, { 57, 49 },
        { 51, 51 }, { 53, 51 }, { 59, 51 },
        { 54, 54 }, { 56, 56 }, { 70, 70 }, { 75, 75 }
    };

    // MPC-style pad banks start at 36 and cover two rows of 16 pads.
    constexpr MapEntry mpcEntries[] =
    {
        { 35, 36 }, { 41, 37 }, { 43, 38 }, { 45, 39 },
        { 47, 40 }, { 48, 41 }, { 50, 42 }, { 52, 43 },
        { 53, 44 }, { 55, 45 }, { 57, 46 }, { 59, 47 },
        { 54, 48 }, { 56, 49 }, { 58, 50 }, { 60, 51 }
    };
}

DrumMap makeDrumMapPreset (int presetIndex, const DrumMap& current)
{
    if ((DrumMapPreset) presetIndex == DrumMapPreset::custom)
        return current;

    return makeDrumMapPreset (presetIndex);
}

DrumMap makeDrumMapPreset (int presetIndex)
{
    auto map = makeIdentityDrumMap();

    switch ((DrumMapPreset) presetIndex)
    {
        case DrumMapPreset::gmStandard:
        case DrumMapPreset::asRecorded:
        case DrumMapPreset::custom:
            break;

        case DrumMapPreset::tr808:
            applyEntries (map, tr808Entries, juce::numElementsInArray (tr808Entries));
            break;

        case DrumMapPreset::tr909:
            applyEntries (map, tr909Entries, juce::numElementsInArray (tr909Entries));
            break;

        case DrumMapPreset::mpc:
            applyEntries (map, mpcEntries, juce::numElementsInArray (mpcEntries));
            break;
    }

    return map;
}

juce::String getNoteName (int noteNumber)
{
    return juce::MidiMessage::getMidiNoteName (noteNumber, true, true, 3);
}

namespace
{
    struct NoteName
    {
        int note;
        const char* description;
    };

    constexpr NoteName gmNotes[] =
    {
        { 35, "Acoustic bass drum" }, { 36, "Bass drum 1" }, { 37, "Side stick" },
        { 38, "Acoustic snare" }, { 39, "Hand clap" }, { 40, "Electric snare" },
        { 41, "Low floor tom" }, { 42, "Closed hi-hat" }, { 43, "High floor tom" },
        { 44, "Pedal hi-hat" }, { 45, "Low tom" }, { 46, "Open hi-hat" },
        { 47, "Low-mid tom" }, { 48, "Hi-mid tom" }, { 49, "Crash cymbal 1" },
        { 50, "High tom" }, { 51, "Ride cymbal 1" }, { 52, "Chinese cymbal" },
        { 53, "Ride bell" }, { 54, "Tambourine" }, { 55, "Splash cymbal" },
        { 56, "Cowbell" }, { 57, "Crash cymbal 2" }, { 58, "Vibraslap" },
        { 59, "Ride cymbal 2" }, { 60, "Hi bongo" }, { 61, "Low bongo" },
        { 62, "Mute hi conga" }, { 63, "Open hi conga" }, { 64, "Low conga" },
        { 65, "High timbale" }, { 66, "Low timbale" }, { 67, "High agogo" },
        { 68, "Low agogo" }, { 69, "Cabasa" }, { 70, "Maracas" },
        { 71, "Short whistle" }, { 72, "Long whistle" }, { 73, "Short guiro" },
        { 74, "Long guiro" }, { 75, "Claves" }, { 76, "Hi wood block" },
        { 77, "Low wood block" }, { 78, "Mute cuica" }, { 79, "Open cuica" },
        { 80, "Mute triangle" }, { 81, "Open triangle" }
    };
}

juce::String getDrumNoteDescription (int noteNumber)
{
    for (const auto& note : gmNotes)
        if (note.note == noteNumber)
            return note.description;

    return {};
}

} // namespace loopdeck
