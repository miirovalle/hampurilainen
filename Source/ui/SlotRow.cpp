#include "SlotRow.h"

#include "Theme.h"

namespace loopdeck::ui
{

SlotRow::SlotRow (LoopDeckAudioProcessor& processorToUse, int slotIndex)
    : processor (processorToUse), slot (slotIndex)
{
    auto& state = processor.apvts;

    for (auto* button : { &power, &mute, &solo })
    {
        button->setClickingTogglesState (true);
        addAndMakeVisible (button);
    }

    clear.onClick = [this]
    {
        processor.clearSlot (slot);
        refreshLabels();
    };

    addAndMakeVisible (clear);

    nameLabel.setFont (Theme::font (13.0f, true));
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setInterceptsMouseClicks (false, false);
    nameLabel.setColour (juce::Label::textColourId, Theme::get().text);
    addAndMakeVisible (nameLabel);

    fillComboBox (channelBox, params::getChannelChoices());
    fillComboBox (mapBox, getDrumMapPresetNames());
    fillComboBox (lengthBox, params::getLengthChoices());
    fillComboBox (speedBox, params::getSpeedChoices());
    fillComboBox (directionBox, params::getDirectionChoices());

    for (auto* box : { &channelBox, &mapBox, &lengthBox, &speedBox, &directionBox })
    {
        styleComboBox (*box);
        addAndMakeVisible (box);
    }

    channelBox.setTooltip ("Output MIDI channel");
    mapBox.setTooltip ("Drum note map");
    lengthBox.setTooltip ("Loop length");
    speedBox.setTooltip ("Playback speed");
    directionBox.setTooltip ("Playback direction");

    for (auto* knob : { &transpose, &gate, &velocity, &swing, &trim, &fixedVelocity })
        addAndMakeVisible (knob);

    addAndMakeVisible (phaseBar);

    powerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::slotId (slot, params::slotEnabled), power);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::slotId (slot, params::slotMute), mute);
    soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::slotId (slot, params::slotSolo), solo);

    channelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::slotId (slot, params::slotChannel), channelBox);
    mapAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::slotId (slot, params::slotMap), mapBox);
    lengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::slotId (slot, params::slotLength), lengthBox);
    speedAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::slotId (slot, params::slotSpeed), speedBox);
    directionAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::slotId (slot, params::slotDirection), directionBox);

    transpose.attach (state, params::slotId (slot, params::slotTranspose));
    gate.attach (state, params::slotId (slot, params::slotGate));
    velocity.attach (state, params::slotId (slot, params::slotVelocity));
    swing.attach (state, params::slotId (slot, params::slotSwing));
    trim.attach (state, params::slotId (slot, params::slotTrim));
    fixedVelocity.attach (state, params::slotId (slot, params::slotFixedVelocity));

    nameLabel.setText (processor.getLoopNameForSlot (slot), juce::dontSendNotification);
}

SlotRow::~SlotRow() = default;

void SlotRow::refreshLabels()
{
    const auto index = processor.getLoopIndexForSlot (slot);
    juce::String text;

    if (index >= 0)
    {
        if (const auto* info = processor.getLibrary().getInfo (index))
        {
            text << info->name << "   " << info->getNumBars() << (info->getNumBars() == 1 ? " bar" : " bars");

            if (info->getChannelText().isNotEmpty())
                text << "  -  ch " << info->getChannelText();
        }
        else
        {
            text = processor.getLoopNameForSlot (slot);
        }
    }
    else
    {
        text = "Tap a loop in the browser";
    }

    nameLabel.setText (juce::String (slot + 1) + "   " + text, juce::dontSendNotification);
    nameLabel.setColour (juce::Label::textColourId, index >= 0 ? Theme::get().text : Theme::get().dimText);
}

void SlotRow::setPhase (float newPhase, bool isRunning)
{
    phaseBar.setPhase (newPhase, isRunning);
}

void SlotRow::setSelected (bool shouldBeSelected)
{
    if (selected != shouldBeSelected)
    {
        selected = shouldBeSelected;
        repaint();
    }
}

void SlotRow::mouseDown (const juce::MouseEvent&)
{
    if (onSelected != nullptr)
        onSelected (slot);
}

void SlotRow::resized()
{
    auto area = getLocalBounds().reduced (8, 7);
    auto top = area.removeFromTop (28);

    power.setBounds (top.removeFromLeft (28));
    top.removeFromLeft (3);
    mute.setBounds (top.removeFromLeft (24));
    top.removeFromLeft (3);
    solo.setBounds (top.removeFromLeft (24));
    top.removeFromLeft (6);

    clear.setBounds (top.removeFromRight (24));
    top.removeFromRight (5);
    channelBox.setBounds (top.removeFromRight (58));
    top.removeFromRight (4);
    mapBox.setBounds (top.removeFromRight (92));
    top.removeFromRight (4);
    lengthBox.setBounds (top.removeFromRight (62));
    top.removeFromRight (4);
    speedBox.setBounds (top.removeFromRight (52));
    top.removeFromRight (4);
    directionBox.setBounds (top.removeFromRight (78));
    top.removeFromRight (8);

    nameLabel.setBounds (top);

    area.removeFromTop (5);

    auto knobWidth = juce::jlimit (44, 68, (area.getWidth() - 90) / 6);

    for (auto* knob : { &transpose, &gate, &velocity, &swing, &trim, &fixedVelocity })
    {
        knob->setBounds (area.removeFromLeft (knobWidth));
        area.removeFromLeft (1);
    }

    area.removeFromLeft (6);
    phaseBar.setBounds (area.withSizeKeepingCentre (juce::jmax (20, area.getWidth()), 10));
}

void SlotRow::paint (juce::Graphics& g)
{
    auto& theme = Theme::get();
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (theme.panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    if (selected)
    {
        g.setColour (theme.slotHighlight.withAlpha (0.9f));
        g.drawRoundedRectangle (bounds, 6.0f, 1.6f);
    }
    else
    {
        g.setColour (theme.outline);
        g.drawRoundedRectangle (bounds, 6.0f, 1.0f);
    }
}

} // namespace loopdeck::ui
