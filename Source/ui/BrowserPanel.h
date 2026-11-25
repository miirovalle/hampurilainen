#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "PluginProcessor.h"
#include "Widgets.h"

namespace loopdeck::ui
{

/** Searchable list of every embedded loop. Tapping a row loads it into the
    currently selected slot.
*/
class BrowserPanel final : public juce::Component,
                           private juce::ListBoxModel,
                           private juce::ComboBox::Listener
{
public:
    explicit BrowserPanel (LoopDeckAudioProcessor& processorToUse);

    void resized() override;
    void paint (juce::Graphics&) override;

    void refreshResults();
    void refreshSelection();

private:
    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics&, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void comboBoxChanged (juce::ComboBox*) override;

    void updateResults();
    void loadRow (int row);

    LoopDeckAudioProcessor& processor;

    juce::TextEditor search;
    juce::ComboBox folderBox;
    juce::ListBox list { "loops", this };
    SectionHeader header { "Library" };

    std::vector<int> results;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrowserPanel)
};

} // namespace loopdeck::ui
