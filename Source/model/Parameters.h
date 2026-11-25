#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "engine/MidiEngine.h"

namespace loopdeck::params
{

inline juce::String slotId (int slot, juce::StringRef suffix)
{
    return "s" + juce::String (slot + 1) + "_" + suffix;
}

//===== global parameter ids =====
inline constexpr const char* master = "master";
inline constexpr const char* thru = "thru";
inline constexpr const char* thruChannel = "thruCh";
inline constexpr const char* inputChannel = "inCh";
inline constexpr const char* clockSource = "clock";
inline constexpr const char* internalBpm = "bpm";
inline constexpr const char* followMidiTransport = "midiTransport";
inline constexpr const char* stopNotes = "stopNotes";
inline constexpr const char* triggerMode = "trigMode";
inline constexpr const char* triggerNote = "trigNote";

//===== per slot suffixes =====
inline constexpr const char* slotEnabled = "on";
inline constexpr const char* slotMute = "mute";
inline constexpr const char* slotSolo = "solo";
inline constexpr const char* slotChannel = "ch";
inline constexpr const char* slotTranspose = "trans";
inline constexpr const char* slotLength = "len";
inline constexpr const char* slotDirection = "dir";
inline constexpr const char* slotMap = "map";
inline constexpr const char* slotFixedVelocity = "fixedVel";
inline constexpr const char* slotSpeed = "speed";
inline constexpr const char* slotGate = "gate";
inline constexpr const char* slotVelocity = "vel";
inline constexpr const char* slotSwing = "swing";
inline constexpr const char* slotTrim = "trim";

juce::StringArray getChannelChoices();          // 0 -> from file, 1..16
juce::StringArray getInputChannelChoices();     // 0 -> all, 1..16
juce::StringArray getLengthChoices();
juce::StringArray getSpeedChoices();
juce::StringArray getDirectionChoices();
juce::StringArray getClockChoices();
juce::StringArray getTriggerModeChoices();

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

} // namespace loopdeck::params
