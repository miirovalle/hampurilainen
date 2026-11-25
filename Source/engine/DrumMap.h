#pragma once

#include <juce_core/juce_core.h>

#include <array>

namespace loopdeck
{

/** A MIDI note remapping table: map[incomingNote] = outgoingNote.

    The loops in this library use the General MIDI percussion map, while
    hardware drum machines and samplers often use their own layout, so each
    slot can carry its own table. The table is only used when the slot's map
    preset is "Custom" or one of the built-in presets.
*/
using DrumMap = std::array<juce::uint8, 128>;

enum class DrumMapPreset
{
    asRecorded = 0,
    gmStandard,
    tr808,
    tr909,
    mpc,
    custom
};

inline constexpr int numDrumMapPresets = 6;

juce::StringArray getDrumMapPresetNames();
juce::String getDrumMapPresetName (int presetIndex);

DrumMap makeIdentityDrumMap();
DrumMap makeDrumMapPreset (int presetIndex);

/** Applies the preset to a table and returns it. "Custom" returns `current`. */
DrumMap makeDrumMapPreset (int presetIndex, const DrumMap& current);

/** Text shown next to a note number, e.g. "36  C1  (Kick)". */
juce::String getNoteName (int noteNumber);
juce::String getDrumNoteDescription (int noteNumber);

} // namespace loopdeck
