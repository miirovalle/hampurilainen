#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "ui/BrowserPanel.h"
#include "ui/Panels.h"
#include "ui/SlotRow.h"
#include "ui/Widgets.h"

namespace loopdeck
{

class LoopDeckAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit LoopDeckAudioProcessorEditor (LoopDeckAudioProcessor&);
    ~LoopDeckAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectSlot (int slot);
    void setActiveTab (int tabIndex);

    LoopDeckAudioProcessor& processor;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label transportLabel;
    juce::Label versionLabel;

    ui::Led inputLed { juce::Colour (0xff35d6a4) };
    ui::Led outputLed { juce::Colour (0xffffb057) };

    ui::BrowserPanel browser;
    std::array<std::unique_ptr<ui::SlotRow>, numSlots> rows;

    juce::TextButton tabButtons[3];
    ui::RoutingPanel routing;
    ui::NoteMapPanel noteMap;
    ui::InfoPanel info;

    int activeTab = 0;
    int lastRevision = -1;

    std::unique_ptr<juce::TooltipWindow> tooltip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoopDeckAudioProcessorEditor)
};

} // namespace loopdeck
