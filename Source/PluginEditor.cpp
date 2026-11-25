#include "PluginEditor.h"

#include "ui/Theme.h"

namespace loopdeck
{

LoopDeckAudioProcessorEditor::LoopDeckAudioProcessorEditor (LoopDeckAudioProcessor& processorToUse)
    : AudioProcessorEditor (&processorToUse),
      processor (processorToUse),
      browser (processorToUse),
      routing (processorToUse),
      noteMap (processorToUse),
      info (processorToUse)
{
    setLookAndFeel (&ui::Theme::get());

    addAndMakeVisible (titleLabel);
    titleLabel.setText ("LOOPDECK", juce::dontSendNotification);
    titleLabel.setFont (ui::Theme::font (21.0f, true));
    titleLabel.setColour (juce::Label::textColourId, ui::Theme::get().text);
    titleLabel.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (subtitleLabel);
    subtitleLabel.setText ("AUv3 MIDI LOOP PLAYER", juce::dontSendNotification);
    subtitleLabel.setFont (ui::Theme::font (9.5f, true));
    subtitleLabel.setColour (juce::Label::textColourId, ui::Theme::get().accent.withAlpha (0.85f));
    subtitleLabel.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (transportLabel);
    transportLabel.setFont (ui::Theme::font (11.0f, true));
    transportLabel.setColour (juce::Label::textColourId, ui::Theme::get().dimText);
    transportLabel.setJustificationType (juce::Justification::centredRight);
    transportLabel.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (versionLabel);
    versionLabel.setText ("v" + juce::String (pluginVersion), juce::dontSendNotification);
    versionLabel.setFont (ui::Theme::font (10.0f));
    versionLabel.setColour (juce::Label::textColourId, ui::Theme::get().dimText);
    versionLabel.setJustificationType (juce::Justification::centredRight);
    versionLabel.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (inputLed);
    addAndMakeVisible (outputLed);

    addAndMakeVisible (browser);
    addAndMakeVisible (routing);
    addAndMakeVisible (noteMap);
    addAndMakeVisible (info);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        auto row = std::make_unique<ui::SlotRow> (processor, slot);
        row->onSelected = [this] (int index) { selectSlot (index); };
        addAndMakeVisible (*row);
        rows[(size_t) slot] = std::move (row);
    }

    for (int i = 0; i < 3; ++i)
    {
        auto& button = tabButtons[i];
        button.setButtonText (i == 0 ? "ROUTING" : (i == 1 ? "NOTE MAP" : "INFO"));
        button.setClickingTogglesState (true);
        button.setRadioGroupId (0x7ab1);
        button.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        button.onClick = [this, i] { setActiveTab (i); };
        addAndMakeVisible (button);
    }

    tooltip = std::make_unique<juce::TooltipWindow> (this, 700);

    selectSlot (processor.getSelectedSlot());
    setActiveTab (0);

    setResizable (true, true);
    setResizeLimits (940, 620, 1800, 1400);
    setSize (1140, 760);

    startTimerHz (30);
}

LoopDeckAudioProcessorEditor::~LoopDeckAudioProcessorEditor()
{
    stopTimer();
    tooltip.reset();
    setLookAndFeel (nullptr);
}

void LoopDeckAudioProcessorEditor::selectSlot (int slot)
{
    processor.setSelectedSlot (slot);

    for (int i = 0; i < numSlots; ++i)
        rows[(size_t) i]->setSelected (i == slot);

    browser.refreshSelection();
    noteMap.refresh();
}

void LoopDeckAudioProcessorEditor::setActiveTab (int tabIndex)
{
    activeTab = juce::jlimit (0, 2, tabIndex);

    for (int i = 0; i < 3; ++i)
        tabButtons[i].setToggleState (i == activeTab, juce::dontSendNotification);

    routing.setVisible (activeTab == 0);
    noteMap.setVisible (activeTab == 1);
    info.setVisible (activeTab == 2);
}

void LoopDeckAudioProcessorEditor::timerCallback()
{
    auto& engine = processor.getEngine();
    const auto revision = processor.uiRevision.load();

    if (revision != lastRevision)
    {
        lastRevision = revision;

        for (auto& row : rows)
            row->refreshLabels();

        browser.refreshSelection();
        noteMap.refresh();
        repaint();
    }

    for (int slot = 0; slot < numSlots; ++slot)
        rows[(size_t) slot]->setPhase (engine.getSlotPhase (slot), engine.isSlotRunning (slot));

    inputLed.setLevel (processor.getInputActivity());
    outputLed.setLevel (processor.getOutputActivity());

    if (activeTab == 0)
        routing.updateIndicators();
    else if (activeTab == 1)
        noteMap.updateIndicators();

    juce::String status;

    if (processor.isHostTransportAvailable())
        status << "HOST TRANSPORT";
    else
        status << engine.getStatusText();

    if (engine.isManualTransportRunning() && ! processor.isHostTransportAvailable())
        status << "  -  RUNNING";
    else if (! engine.isManualTransportRunning() && ! processor.isHostTransportAvailable())
        status << "  -  STOPPED";

    transportLabel.setText (status, juce::dontSendNotification);
}

void LoopDeckAudioProcessorEditor::paint (juce::Graphics& g)
{
    auto& theme = ui::Theme::get();

    g.fillAll (theme.background);

    juce::ColourGradient gradient (theme.panelDeep.brighter (0.04f), 0.0f, 0.0f,
                                   theme.background, 0.0f, (float) getHeight() * 0.9f, false);
    g.setGradientFill (gradient);
    g.fillAll();

    // header separator
    g.setColour (theme.outline.withAlpha (0.8f));
    g.drawHorizontalLine (getHeight() > 0 ? 45 : 0, 10.0f, (float) getWidth() - 10.0f);

    // accent line above the tab bar
    if (! tabButtons[0].getBounds().isEmpty())
    {
        const auto top = (float) tabButtons[0].getY() - 4.0f;
        g.setColour (theme.outline.withAlpha (0.55f));
        g.drawHorizontalLine ((int) top, (float) tabButtons[0].getX(), (float) getWidth() - 10.0f);
    }
}

void LoopDeckAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    auto header = area.removeFromTop (46);
    header.reduce (12, 6);

    auto titleArea = header.removeFromLeft (juce::jmax (150, header.getWidth() / 3));
    titleLabel.setBounds (titleArea.removeFromTop (24));
    subtitleLabel.setBounds (titleArea);

    auto rightSide = header.removeFromRight (juce::jmin (340, header.getWidth() / 2));
    versionLabel.setBounds (rightSide.removeFromRight (52));
    outputLed.setBounds (rightSide.removeFromRight (18).reduced (2, 6));
    rightSide.removeFromRight (8);
    inputLed.setBounds (rightSide.removeFromRight (18).reduced (2, 6));
    rightSide.removeFromRight (8);
    transportLabel.setBounds (rightSide);

    area.reduce (10, 8);

    const auto gap = 10;
    auto browserArea = area.removeFromLeft (juce::jlimit (250, 360, juce::roundToInt (area.getWidth() * 0.3f)));
    area.removeFromLeft (gap);
    browser.setBounds (browserArea);

    auto rightArea = area;

    const auto rowGap = 6;
    const auto wantedSlots = numSlots * 94 + (numSlots - 1) * rowGap;
    auto slotArea = rightArea.removeFromTop (juce::jlimit (4 * 70 + 3 * rowGap,
                                                           wantedSlots,
                                                           juce::roundToInt (rightArea.getHeight() * 0.57f)));

    const auto rowHeight = juce::jmax (66, (slotArea.getHeight() - (numSlots - 1) * rowGap) / numSlots);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        rows[(size_t) slot]->setBounds (slotArea.removeFromTop (rowHeight));
        slotArea.removeFromTop (rowGap);
    }

    rightArea.removeFromTop (gap);

    auto tabRow = rightArea.removeFromTop (28);
    auto tabArea = tabRow.removeFromRight (juce::jmin (330, tabRow.getWidth()));

    for (auto& button : tabButtons)
        button.setBounds (tabArea.removeFromLeft (tabArea.getWidth() / 3));

    rightArea.removeFromTop (4);

    routing.setBounds (rightArea);
    noteMap.setBounds (rightArea);
    info.setBounds (rightArea);
}

} // namespace loopdeck
