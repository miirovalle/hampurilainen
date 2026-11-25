#include "Parameters.h"

namespace loopdeck::params
{

juce::StringArray getChannelChoices()
{
    juce::StringArray choices { "From file" };

    for (int i = 1; i <= 16; ++i)
        choices.add (juce::String (i));

    return choices;
}

juce::StringArray getInputChannelChoices()
{
    juce::StringArray choices { "All" };

    for (int i = 1; i <= 16; ++i)
        choices.add (juce::String (i));

    return choices;
}

juce::StringArray getLengthChoices()
{
    return { "Loop", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" };
}

juce::StringArray getSpeedChoices()
{
    return { "1/4x", "1/2x", "1x", "2x", "4x" };
}

juce::StringArray getDirectionChoices()
{
    return { "Forward", "Reverse", "Ping-Pong" };
}

juce::StringArray getClockChoices()
{
    return { "Host", "Internal", "MIDI Clock" };
}

juce::StringArray getTriggerModeChoices()
{
    return { "Off", "Pads: toggle", "Pads: one-shot" };
}

namespace
{
    using ParameterLayout = juce::AudioProcessorValueTreeState::ParameterLayout;

    void addBool (ParameterLayout& layout, const juce::String& id, const juce::String& name, bool defaultValue)
    {
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, defaultValue));
    }

    void addChoice (ParameterLayout& layout, const juce::String& id, const juce::String& name,
                    const juce::StringArray& choices, int defaultIndex)
    {
        layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name,
                                                                 choices, defaultIndex));
    }

    void addFloat (ParameterLayout& layout, const juce::String& id, const juce::String& name,
                   juce::NormalisableRange<float> range, float defaultValue, const juce::String& suffix,
                   int decimals = 0)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, range, defaultValue,
            juce::AudioParameterFloatAttributes().withLabel (suffix)
                .withStringFromValueFunction ([decimals, suffix] (float value, int)
                                              {
                                                  return juce::String (value, decimals) + suffix;
                                              })));
    }

    void addInt (ParameterLayout& layout, const juce::String& id, const juce::String& name,
                 int minValue, int maxValue, int defaultValue,
                 std::function<juce::String (int, int)> textFunction = {})
    {
        auto attributes = juce::AudioParameterIntAttributes();

        if (textFunction != nullptr)
            attributes = attributes.withStringFromValueFunction (std::move (textFunction));

        layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name,
                                                               minValue, maxValue, defaultValue, attributes));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    ParameterLayout layout;

    addBool (layout, master, "Master", true);
    addBool (layout, thru, "MIDI Thru", false);
    addChoice (layout, thruChannel, "Thru channel", getInputChannelChoices(), 0);
    addChoice (layout, inputChannel, "Input channel", getInputChannelChoices(), 0);
    addChoice (layout, clockSource, "Clock", getClockChoices(), 0);
    addFloat (layout, internalBpm, "Tempo", { 40.0f, 300.0f, 0.1f }, 120.0f, " BPM", 1);
    addBool (layout, followMidiTransport, "Follow MIDI transport", true);
    addBool (layout, stopNotes, "All notes off on stop", true);
    addChoice (layout, triggerMode, "Pad trigger", getTriggerModeChoices(), 0);
    addInt (layout, triggerNote, "Trigger base note", 0, 120, 36,
            [] (int value, int) { return juce::MidiMessage::getMidiNoteName (value, true, true, 3); });

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto prefix = juce::String ("Slot ") + juce::String (slot + 1) + " ";

        addBool (layout, slotId (slot, slotEnabled), prefix + "enabled", slot == 0);
        addBool (layout, slotId (slot, slotMute), prefix + "mute", false);
        addBool (layout, slotId (slot, slotSolo), prefix + "solo", false);
        addChoice (layout, slotId (slot, slotChannel), prefix + "channel", getChannelChoices(), slot == 0 ? 10 : 0);
        addInt (layout, slotId (slot, slotTranspose), prefix + "transpose", -48, 48, 0,
                [] (int value, int) { return juce::String (value > 0 ? "+" : "") + juce::String (value); });
        addChoice (layout, slotId (slot, slotLength), prefix + "length", getLengthChoices(), 0);
        addChoice (layout, slotId (slot, slotDirection), prefix + "direction", getDirectionChoices(), 0);
        addChoice (layout, slotId (slot, slotMap), prefix + "note map", getDrumMapPresetNames(),
                   (int) DrumMapPreset::gmStandard);
        addInt (layout, slotId (slot, slotFixedVelocity), prefix + "fixed velocity", -1, 127, -1,
                [] (int value, int) { return value <= 0 ? juce::String ("Off") : juce::String (value); });
        addChoice (layout, slotId (slot, slotSpeed), prefix + "speed", getSpeedChoices(), 2);
        addFloat (layout, slotId (slot, slotGate), prefix + "gate", { 5.0f, 400.0f, 1.0f }, 100.0f, "%");
        addFloat (layout, slotId (slot, slotVelocity), prefix + "velocity", { 0.0f, 200.0f, 1.0f }, 100.0f, "%");
        addFloat (layout, slotId (slot, slotSwing), prefix + "swing", { 0.0f, 75.0f, 1.0f }, 0.0f, "%");
        addFloat (layout, slotId (slot, slotTrim), prefix + "start", { 0.0f, 100.0f, 0.1f }, 0.0f, "%", 1);
    }

    return layout;
}

} // namespace loopdeck::params
