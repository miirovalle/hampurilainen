#include "BrowserPanel.h"

#include "Theme.h"

namespace loopdeck::ui
{

BrowserPanel::BrowserPanel (LoopDeckAudioProcessor& processorToUse) : processor (processorToUse)
{
    addAndMakeVisible (header);

    search.setTextToShowWhenEmpty ("Search loops", Theme::get().dimText);
    search.setFont (Theme::font (13.0f));
    search.setMultiLine (false);
    search.setReturnKeyStartsNewLine (false);
    search.setIndents (8, 5);
    search.onTextChange = [this] { updateResults(); };
    addAndMakeVisible (search);

    folderBox.setTooltip ("Filter by folder");
    styleComboBox (folderBox);
    folderBox.addItem ("All folders", 1);

    const auto folders = processor.getLibrary().getFolders();

    for (int i = 0; i < folders.size(); ++i)
        folderBox.addItem (folders[i], i + 2);

    folderBox.setSelectedId (1, juce::dontSendNotification);
    folderBox.addListener (this);
    addAndMakeVisible (folderBox);

    list.setRowHeight (44);
    list.setMultipleSelectionEnabled (false);
    list.setColour (juce::ListBox::backgroundColourId, Theme::get().panelDeep);
    addAndMakeVisible (list);

    updateResults();
}

void BrowserPanel::comboBoxChanged (juce::ComboBox*)
{
    updateResults();
}

void BrowserPanel::refreshResults()
{
    updateResults();
}

void BrowserPanel::refreshSelection()
{
    list.repaint();
}

void BrowserPanel::updateResults()
{
    const auto folderId = folderBox.getSelectedId();
    juce::String folder;

    if (folderId > 1)
        folder = folderBox.getItemText (folderId - 1);

    results = processor.getLibrary().search (search.getText(), folder);

    header.setDetail (juce::String (results.size()) + " / " + juce::String (processor.getLibrary().getNumLoops()));

    list.updateContent();
    list.repaint();
}

int BrowserPanel::getNumRows()
{
    return (int) results.size();
}

void BrowserPanel::paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    auto& theme = Theme::get();

    if (! juce::isPositiveAndBelow (rowNumber, (int) results.size()))
        return;

    const auto index = results[(size_t) rowNumber];
    const auto* info = processor.getLibrary().getInfo (index);

    if (info == nullptr)
        return;

    const auto bounds = juce::Rectangle<int> (0, 0, width, height);

    if (rowIsSelected)
    {
        g.setColour (theme.accent.withAlpha (0.16f));
        g.fillRect (bounds);
    }
    else if ((rowNumber % 2) == 1)
    {
        g.setColour (theme.panelAlt.withAlpha (0.35f));
        g.fillRect (bounds);
    }

    auto textArea = bounds.reduced (8, 4);

    // slot badges on the right
    juce::String badges;

    for (int slot = 0; slot < numSlots; ++slot)
        if (processor.getLoopIndexForSlot (slot) == index)
            badges << (juce::String (slot + 1) + " ");

    if (badges.isNotEmpty())
    {
        auto badgeArea = textArea.removeFromRight (56).toFloat().withSizeKeepingCentre (48.0f, 18.0f);
        g.setColour (theme.accent.withAlpha (0.22f));
        g.fillRoundedRectangle (badgeArea, 9.0f);
        g.setColour (theme.accent);
        g.setFont (Theme::font (10.0f, true));
        g.drawText (badges.trim(), badgeArea, juce::Justification::centred, false);
    }

    g.setColour (rowIsSelected ? theme.text : theme.text.withAlpha (0.92f));
    g.setFont (Theme::font (13.0f, true));
    g.drawFittedText (info->name, textArea.removeFromTop (19), juce::Justification::centredLeft, 1);

    juce::String detail;
    detail << info->getNumBars() << (info->getNumBars() == 1 ? " bar" : " bars");

    if (info->getChannelText().isNotEmpty())
        detail << "  -  ch " << info->getChannelText();

    if (info->numNotes > 0)
        detail << "  -  " << info->numNotes << " notes";

    if (info->folder.isNotEmpty())
        detail << "  -  " << info->folder;

    g.setColour (theme.dimText);
    g.setFont (Theme::font (10.5f));
    g.drawFittedText (detail, textArea, juce::Justification::centredLeft, 1);
}

void BrowserPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    loadRow (row);
}

void BrowserPanel::returnKeyPressed (int row)
{
    loadRow (row);
}

void BrowserPanel::loadRow (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) results.size()))
        return;

    const auto slot = processor.getSelectedSlot();
    processor.loadLoopIntoSlot (slot, results[(size_t) row]);
    list.repaint();
}

void BrowserPanel::resized()
{
    auto area = getLocalBounds();

    header.setBounds (area.removeFromTop (18));
    area.removeFromTop (4);

    auto searchRow = area.removeFromTop (28);
    search.setBounds (searchRow.removeFromLeft (juce::jmax (80, searchRow.getWidth() - 118)));
    searchRow.removeFromLeft (6);
    folderBox.setBounds (searchRow);

    area.removeFromTop (6);
    list.setBounds (area);
}

void BrowserPanel::paint (juce::Graphics& g)
{
    auto& theme = Theme::get();
    g.setColour (theme.panel);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 8.0f);
    g.setColour (theme.outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 8.0f, 1.0f);
}

} // namespace loopdeck::ui
