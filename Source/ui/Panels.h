#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "Widgets.h"

namespace loopdeck::ui
{

/** Clock source, MIDI input filtering, thru and pad triggering. */
class RoutingPanel final : public juce::Component
{
public:
    explicit RoutingPanel (LoopDeckAudioProcessor& processorToUse);
    ~RoutingPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    void updateIndicators();

private:
    void layoutClock (juce::Rectangle<int> area);
    void layoutInput (juce::Rectangle<int> area);
    void layoutTrigger (juce::Rectangle<int> area);

    LoopDeckAudioProcessor& processor;

    SectionHeader clockHeader { "Clock & transport" };
    SectionHeader inputHeader { "Input & thru" };
    SectionHeader triggerHeader { "Pad trigger" };

    juce::ComboBox clockBox, inputChannelBox, thruChannelBox, triggerModeBox;
    juce::Label inputCaption, thruCaption, statusLabel, hintLabel;

    juce::ToggleButton followTransport { "MIDI start / stop" };
    juce::ToggleButton stopNotes { "Notes off on stop" };
    juce::ToggleButton thruToggle { "MIDI thru" };

    Knob bpm { "TEMPO" };
    Knob triggerNote { "BASE NOTE" };

    IconButton playButton { IconButton::Kind::play, "Play / stop the internal clock" };
    IconButton panicButton { IconButton::Kind::panic, "Panic: all notes off" };

    Led inputLed { juce::Colour (0xff35d6a4) };
    Led outputLed { juce::Colour (0xffffb057) };
    juce::Label inLedCaption, outLedCaption;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> clockAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inputChannelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> thruChannelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> triggerModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followTransportAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stopNotesAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> thruAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoutingPanel)
};

//==============================================================================
/** Editable note remapping table for the selected slot. */
class NoteMapPanel final : public juce::Component,
                           private juce::ListBoxModel
{
public:
    explicit NoteMapPanel (LoopDeckAudioProcessor& processorToUse);
    ~NoteMapPanel() override;

    void resized() override;
    void paint (juce::Graphics&) override;

    void refresh();
    void updateIndicators();

private:
    friend class NoteMapRow;

    int getNumRows() override;
    juce::Component* refreshComponentForRow (int rowNumber, bool isRowSelected,
                                             juce::Component* existingComponentToUpdate) override;
    void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;

    void setMapping (int inputNote, int outputNote);
    void selectSlot (int slot);

    LoopDeckAudioProcessor& processor;

    SectionHeader header { "Note map" };
    juce::TextButton slotButtons[numSlots];
    juce::TextButton resetButton { "Reset" };
    juce::TextButton copyButton { "Copy to all" };
    juce::ComboBox presetBox;
    juce::Label hintLabel;
    juce::ListBox list { "notes", this };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> presetAttachment;
    int currentSlot = 0;
    bool needsRefresh = true;
    bool scrolledOnce = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NoteMapPanel)
};

//==============================================================================
/** Short help text and library statistics. */
class InfoPanel final : public juce::Component
{
public:
    explicit InfoPanel (LoopDeckAudioProcessor& processorToUse);

    void paint (juce::Graphics&) override;

private:
    LoopDeckAudioProcessor& processor;
};

} // namespace loopdeck::ui
