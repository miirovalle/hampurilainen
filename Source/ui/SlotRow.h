#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

#include "PluginProcessor.h"
#include "Widgets.h"

namespace loopdeck::ui
{

/** One editable loop slot. */
class SlotRow final : public juce::Component
{
public:
    SlotRow (LoopDeckAudioProcessor& processorToUse, int slotIndex);
    ~SlotRow() override;

    void refreshLabels();
    void setPhase (float newPhase, bool isRunning);
    void setSelected (bool shouldBeSelected);

    void resized() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    std::function<void (int)> onSelected;

private:
    LoopDeckAudioProcessor& processor;
    const int slot;
    bool selected = false;

    IconButton power { IconButton::Kind::power, "Enable slot" };
    IconButton mute { IconButton::Kind::mute, "Mute slot" };
    IconButton solo { IconButton::Kind::solo, "Solo slot" };
    IconButton clear { IconButton::Kind::close, "Clear slot" };

    juce::Label nameLabel;
    juce::ComboBox channelBox, mapBox, lengthBox, speedBox, directionBox;

    Knob transpose { "TRANS" };
    Knob gate { "GATE" };
    Knob velocity { "VELOCITY" };
    Knob swing { "SWING" };
    Knob trim { "START" };
    Knob fixedVelocity { "FIX VEL" };

    PhaseBar phaseBar;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> powerAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> channelAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mapAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lengthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> speedAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> directionAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SlotRow)
};

} // namespace loopdeck::ui
