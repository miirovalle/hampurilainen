#include "Panels.h"

#include "Theme.h"

#include <algorithm>
#include <iterator>

namespace loopdeck::ui
{

//==============================================================================
RoutingPanel::RoutingPanel (LoopDeckAudioProcessor& processorToUse) : processor (processorToUse)
{
    auto& state = processor.apvts;

    addAndMakeVisible (clockHeader);
    addAndMakeVisible (inputHeader);
    addAndMakeVisible (triggerHeader);

    fillComboBox (clockBox, params::getClockChoices());
    fillComboBox (inputChannelBox, params::getInputChannelChoices());
    fillComboBox (thruChannelBox, params::getInputChannelChoices());
    fillComboBox (triggerModeBox, params::getTriggerModeChoices());

    for (auto* box : { &clockBox, &inputChannelBox, &thruChannelBox, &triggerModeBox })
    {
        styleComboBox (*box);
        addAndMakeVisible (box);
    }

    clockBox.setTooltip ("Where the playback timing comes from");
    inputChannelBox.setTooltip ("Only react to this incoming channel");
    thruChannelBox.setTooltip ("Channel used when forwarding incoming notes");
    triggerModeBox.setTooltip ("Play slots from pads instead of the transport");

    clockAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::clockSource, clockBox);
    inputChannelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::inputChannel, inputChannelBox);
    thruChannelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::thruChannel, thruChannelBox);
    triggerModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        state, params::triggerMode, triggerModeBox);

    for (auto* toggle : { &followTransport, &stopNotes, &thruToggle })
        addAndMakeVisible (toggle);

    followTransportAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::followMidiTransport, followTransport);
    stopNotesAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::stopNotes, stopNotes);
    thruAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::thru, thruToggle);

    bpm.attach (state, params::internalBpm);
    triggerNote.attach (state, params::triggerNote);
    addAndMakeVisible (bpm);
    addAndMakeVisible (triggerNote);

    inputCaption.setText ("Input channel", juce::dontSendNotification);
    thruCaption.setText ("Thru channel", juce::dontSendNotification);

    for (auto* caption : { &inputCaption, &thruCaption, &inLedCaption, &outLedCaption, &hintLabel })
    {
        caption->setFont (Theme::font (10.5f));
        caption->setColour (juce::Label::textColourId, Theme::get().dimText);
        caption->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (caption);
    }

    inLedCaption.setText ("IN", juce::dontSendNotification);
    outLedCaption.setText ("OUT", juce::dontSendNotification);
    inLedCaption.setJustificationType (juce::Justification::centred);
    outLedCaption.setJustificationType (juce::Justification::centred);
    inLedCaption.setFont (Theme::font (9.0f, true));
    outLedCaption.setFont (Theme::font (9.0f, true));

    statusLabel.setFont (Theme::font (10.5f, true));
    statusLabel.setColour (juce::Label::textColourId, Theme::get().accent);
    statusLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (statusLabel);

    hintLabel.setText ("Base note = slot 1, next three notes = slots 2-4", juce::dontSendNotification);
    hintLabel.setJustificationType (juce::Justification::centredLeft);

    addAndMakeVisible (inputLed);
    addAndMakeVisible (outputLed);

    playButton.onClick = [this] { processor.togglePlayStop(); };
    panicButton.onClick = [this] { processor.panic(); };
    playButton.setTooltip ("Start or stop the internal clock");
    panicButton.setTooltip ("Send note offs and all notes off");
    addAndMakeVisible (playButton);
    addAndMakeVisible (panicButton);
}

RoutingPanel::~RoutingPanel() = default;

void RoutingPanel::updateIndicators()
{
    auto& engine = processor.getEngine();

    inputLed.setLevel (processor.getInputActivity());
    outputLed.setLevel (engine.getActivity());

    juce::String status;

    if (processor.isHostTransportAvailable())
        status << "Host transport";
    else if (engine.isManualTransportRunning())
        status << engine.getStatusText() << " running";
    else
        status << engine.getStatusText() << " stopped";

    if (engine.isManualTransportRunning() && processor.isHostTransportAvailable())
        status << "  (internal play)";

    statusLabel.setText (status, juce::dontSendNotification);

    const auto manualRunning = engine.isManualTransportRunning();
    playButton.setToggleState (manualRunning, juce::dontSendNotification);
    playButton.setTooltip (manualRunning ? "Stop the internal clock" : "Start the internal clock");
}

void RoutingPanel::layoutClock (juce::Rectangle<int> area)
{
    clockHeader.setBounds (area.removeFromTop (16));
    area.removeFromTop (6);

    clockBox.setBounds (area.removeFromTop (26));
    area.removeFromTop (8);

    auto knobRow = area.removeFromTop (juce::jmin (58, area.getHeight()));
    bpm.setBounds (knobRow.removeFromLeft (58));
    knobRow.removeFromLeft (8);

    auto toggleArea = knobRow;
    followTransport.setBounds (toggleArea.removeFromTop (22));
    toggleArea.removeFromTop (4);
    stopNotes.setBounds (toggleArea.removeFromTop (22));

    area.removeFromTop (8);

    auto buttonRow = area.removeFromTop (26);
    playButton.setBounds (buttonRow.removeFromLeft (30));
    buttonRow.removeFromLeft (4);
    panicButton.setBounds (buttonRow.removeFromLeft (30));
    buttonRow.removeFromLeft (8);
    statusLabel.setBounds (buttonRow);
}

void RoutingPanel::layoutInput (juce::Rectangle<int> area)
{
    inputHeader.setBounds (area.removeFromTop (16));
    area.removeFromTop (8);

    auto firstRow = area.removeFromTop (26);
    inputCaption.setBounds (firstRow.removeFromLeft (74));
    inputChannelBox.setBounds (firstRow);

    area.removeFromTop (8);
    thruToggle.setBounds (area.removeFromTop (22));
    area.removeFromTop (6);

    auto secondRow = area.removeFromTop (26);
    thruCaption.setBounds (secondRow.removeFromLeft (74));
    thruChannelBox.setBounds (secondRow);

    area.removeFromTop (10);

    auto ledRow = area.removeFromTop (26);
    inputLed.setBounds (ledRow.removeFromLeft (22));
    ledRow.removeFromLeft (4);
    inLedCaption.setBounds (ledRow.removeFromLeft (26));
    ledRow.removeFromLeft (12);
    outputLed.setBounds (ledRow.removeFromLeft (22));
    ledRow.removeFromLeft (4);
    outLedCaption.setBounds (ledRow.removeFromLeft (30));
}

void RoutingPanel::layoutTrigger (juce::Rectangle<int> area)
{
    triggerHeader.setBounds (area.removeFromTop (16));
    area.removeFromTop (6);

    triggerModeBox.setBounds (area.removeFromTop (26));
    area.removeFromTop (8);

    auto knobRow = area.removeFromTop (juce::jmin (62, area.getHeight()));
    triggerNote.setBounds (knobRow.removeFromLeft (62));
    knobRow.removeFromLeft (8);
    hintLabel.setBounds (knobRow.removeFromTop (46));
}

void RoutingPanel::resized()
{
    auto area = getLocalBounds().reduced (10);

    const auto gap = 14;
    const auto columnWidth = (area.getWidth() - gap * 2) / 3;

    auto clockArea = area.removeFromLeft (columnWidth);
    area.removeFromLeft (gap);
    auto inputArea = area.removeFromLeft (columnWidth);
    area.removeFromLeft (gap);

    layoutClock (clockArea);
    layoutInput (inputArea);
    layoutTrigger (area);
}

void RoutingPanel::paint (juce::Graphics& g)
{
    auto& theme = Theme::get();
    g.setColour (theme.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (theme.outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);

    // column separators
    const auto gap = 14;
    const auto area = getLocalBounds().reduced (10);
    const auto columnWidth = (area.getWidth() - gap * 2) / 3;

    for (int i = 1; i < 3; ++i)
    {
        const auto x = (float) (area.getX() + i * (columnWidth + gap) - gap / 2);
        g.setColour (theme.outline.withAlpha (0.7f));
        g.drawVerticalLine ((int) x, (float) area.getY() + 2.0f, (float) area.getBottom() - 2.0f);
    }
}

//==============================================================================
class NoteMapRow final : public juce::Component
{
public:
    NoteMapRow (NoteMapPanel& ownerToUse, juce::Colour dimColour) : owner (ownerToUse), dim (dimColour)
    {
        juce::StringArray choices;

        for (int note = 0; note < 128; ++note)
            choices.add (juce::String (note) + "  " + getNoteName (note));

        fillComboBox (outBox, choices);
        outBox.setTooltip ("Output note");
        outBox.onChange = [this]
        {
            if (row >= 0)
                owner.setMapping (row, outBox.getSelectedId() - 1);
        };

        addAndMakeVisible (outBox);
    }

    void setRow (int newRow, int outputNote)
    {
        row = newRow;
        outBox.setSelectedId (outputNote + 1, juce::dontSendNotification);
        outBox.setColour (juce::ComboBox::textColourId,
                          outputNote == row ? dim : Theme::get().text);
        repaint();
    }

    void resized() override
    {
        outBox.setBounds (getLocalBounds().removeFromRight (132).reduced (0, 3));
    }

private:
    NoteMapPanel& owner;
    juce::Colour dim;
    juce::ComboBox outBox;
    int row = -1;
};

//==============================================================================
NoteMapPanel::NoteMapPanel (LoopDeckAudioProcessor& processorToUse) : processor (processorToUse)
{
    addAndMakeVisible (header);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        auto& button = slotButtons[slot];
        button.setButtonText (juce::String (slot + 1));
        button.setClickingTogglesState (true);
        button.setRadioGroupId (0x10ce);
        button.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        button.onClick = [this, slot] { selectSlot (slot); };
        button.setTooltip ("Edit slot " + juce::String (slot + 1) + " note map");
        addAndMakeVisible (button);
    }

    fillComboBox (presetBox, getDrumMapPresetNames());
    presetBox.setTooltip ("Note map preset for the selected slot");
    addAndMakeVisible (presetBox);

    resetButton.setTooltip ("Reset every note to its own number");
    resetButton.onClick = [this]
    {
        processor.resetDrumMap (currentSlot);
        list.updateContent();
        list.repaint();
    };
    addAndMakeVisible (resetButton);

    copyButton.setTooltip ("Copy this map to the other three slots");
    copyButton.onClick = [this]
    {
        for (int slot = 0; slot < numSlots; ++slot)
            if (slot != currentSlot)
                processor.copyDrumMap (currentSlot, slot);

        list.updateContent();
        list.repaint();
    };
    addAndMakeVisible (copyButton);

    hintLabel.setText ("Tap a value to remap one note. Editing switches the slot to Custom.",
                       juce::dontSendNotification);
    hintLabel.setFont (Theme::font (10.0f));
    hintLabel.setColour (juce::Label::textColourId, Theme::get().dimText);
    hintLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (hintLabel);

    list.setRowHeight (24);
    list.setColour (juce::ListBox::backgroundColourId, Theme::get().panelDeep);
    addAndMakeVisible (list);

    selectSlot (processor.getSelectedSlot());
}

NoteMapPanel::~NoteMapPanel() = default;

void NoteMapPanel::selectSlot (int slot)
{
    currentSlot = juce::jlimit (0, numSlots - 1, slot);
    processor.setSelectedSlot (currentSlot);

    slotButtons[currentSlot].setToggleState (true, juce::dontSendNotification);

    presetAttachment.reset();
    presetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, params::slotId (currentSlot, params::slotMap), presetBox);

    header.setDetail ("Slot " + juce::String (currentSlot + 1));
    list.updateContent();
    list.repaint();
}

void NoteMapPanel::setMapping (int inputNote, int outputNote)
{
    processor.setCustomNoteMapping (currentSlot, inputNote, outputNote);
    list.repaint();
}

void NoteMapPanel::refresh()
{
    if (processor.getSelectedSlot() != currentSlot)
        selectSlot (processor.getSelectedSlot());

    for (int slot = 0; slot < numSlots; ++slot)
        slotButtons[slot].setToggleState (slot == currentSlot, juce::dontSendNotification);

    needsRefresh = true;
    list.updateContent();
    list.repaint();
}

void NoteMapPanel::updateIndicators()
{
    if (needsRefresh)
        list.repaint();
}

int NoteMapPanel::getNumRows()
{
    return 128;
}

juce::Component* NoteMapPanel::refreshComponentForRow (int rowNumber, bool, juce::Component* existingComponentToUpdate)
{
    auto* row = dynamic_cast<NoteMapRow*> (existingComponentToUpdate);

    if (row == nullptr)
    {
        row = new NoteMapRow (*this, Theme::get().dimText);
        existingComponentToUpdate = row;
    }

    const auto map = processor.getEffectiveDrumMap (currentSlot);
    row->setRow (rowNumber, (int) map[(size_t) rowNumber]);
    return existingComponentToUpdate;
}

void NoteMapPanel::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    auto& theme = Theme::get();

    if (! juce::isPositiveAndBelow (rowNumber, 128))
        return;

    const auto map = processor.getEffectiveDrumMap (currentSlot);
    const auto output = (int) map[(size_t) rowNumber];
    const auto bounds = juce::Rectangle<int> (0, 0, width, height);

    if (rowIsSelected)
    {
        g.setColour (theme.accent.withAlpha (0.16f));
        g.fillRect (bounds);
    }
    else if ((rowNumber % 2) == 1)
    {
        g.setColour (theme.panelAlt.withAlpha (0.3f));
        g.fillRect (bounds);
    }

    auto textArea = bounds.reduced (8, 0);
    textArea.removeFromRight (140);

    const auto description = getDrumNoteDescription (rowNumber);
    const auto changed = output != rowNumber;

    g.setColour (changed ? theme.accent : theme.text.withAlpha (0.85f));
    g.setFont (Theme::font (11.0f, changed));
    g.drawText (juce::String (rowNumber) + "   " + getNoteName (rowNumber),
                textArea.removeFromLeft (92), juce::Justification::centredLeft, false);

    g.setColour (theme.dimText);
    g.setFont (Theme::font (10.5f));
    g.drawText (description, textArea, juce::Justification::centredLeft, true);
}

void NoteMapPanel::resized()
{
    auto area = getLocalBounds().reduced (10);

    auto topRow = area.removeFromTop (26);
    header.setBounds (topRow.removeFromLeft (juce::jmax (100, topRow.getWidth() - 420)));
    topRow.removeFromLeft (6);

    auto buttons = topRow.removeFromLeft (numSlots * 34);
    for (auto& button : slotButtons)
        button.setBounds (buttons.removeFromLeft (34));

    topRow.removeFromLeft (8);
    resetButton.setBounds (topRow.removeFromLeft (64));
    topRow.removeFromLeft (4);
    copyButton.setBounds (topRow.removeFromLeft (84));
    topRow.removeFromLeft (8);
    presetBox.setBounds (topRow.removeFromLeft (juce::jmin (150, juce::jmax (90, topRow.getWidth()))));

    area.removeFromTop (4);
    hintLabel.setBounds (area.removeFromTop (16));
    area.removeFromTop (4);

    list.setBounds (area);

    if (! scrolledOnce)
    {
        scrolledOnce = true;
        list.scrollToEnsureRowIsOnscreen (36);
    }
}

void NoteMapPanel::paint (juce::Graphics& g)
{
    auto& theme = Theme::get();
    g.setColour (theme.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (theme.outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
}

//==============================================================================
InfoPanel::InfoPanel (LoopDeckAudioProcessor& processorToUse) : processor (processorToUse)
{
    setInterceptsMouseClicks (false, false);
}

void InfoPanel::paint (juce::Graphics& g)
{
    auto& theme = Theme::get();

    g.setColour (theme.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (theme.outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);

    auto area = getLocalBounds().reduced (14);

    g.setColour (theme.text);
    g.setFont (Theme::font (16.0f, true));
    g.drawText (juce::String (productName) + "  " + pluginVersion, area.removeFromTop (22),
                juce::Justification::centredLeft, false);

    area.removeFromTop (4);

    g.setColour (theme.dimText);
    g.setFont (Theme::font (11.0f));
    g.drawText ("AUv3 MIDI processor. Plays the embedded loop library and sends MIDI to any synth or drum machine.",
                area.removeFromTop (30), juce::Justification::topLeft, true);

    const auto loopCount = processor.getLibrary().getNumLoops();
    const auto folderCount = processor.getLibrary().getFolders().size();
    const auto settings = processor.getEngine().getSettings();
    const auto slotCount = (int) std::count_if (settings.slots.begin(), settings.slots.end(),
                                                [] (const SlotSettings& slot) { return slot.enabled; });

    juce::StringArray lines;
    lines.add (juce::String (loopCount) + " loops from " + juce::String (folderCount) + " folders");
    lines.add (juce::String (slotCount) + " of 4 slots active");
    lines.add ("Clock: " + processor.getEngine().getStatusText());
    lines.add (processor.isHostTransportAvailable() ? "Host transport detected" : "No host transport - use play");

    g.setColour (theme.accent.withAlpha (0.9f));
    g.setFont (Theme::font (11.0f, true));
    auto detailArea = area.removeFromTop (18 * lines.size());
    for (const auto& line : lines)
        g.drawText (line, detailArea.removeFromTop (18), juce::Justification::centredLeft, true);

    area.removeFromTop (6);
    g.setColour (theme.dimText);
    g.setFont (Theme::font (10.5f));
    g.drawFittedText ("Routing: each slot sends on its own MIDI channel, with its own transpose, drum note map, "
                      "gate, swing and start offset. Turn on Pads in the Routing tab to start slots from notes.",
                      area, juce::Justification::topLeft, 4);
}

} // namespace loopdeck::ui
