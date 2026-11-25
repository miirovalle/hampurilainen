#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace loopdeck::ui
{

/** Shared dark theme for every control in the editor. */
class Theme final : public juce::LookAndFeel_V4
{
public:
    Theme();

    static Theme& get();

    juce::Colour background { 0xff0d1015 };
    juce::Colour panel { 0xff161a21 };
    juce::Colour panelAlt { 0xff1c222b };
    juce::Colour panelDeep { 0xff11151b };
    juce::Colour outline { 0xff2b3340 };
    juce::Colour text { 0xffe8edf4 };
    juce::Colour dimText { 0xff8a97a9 };
    juce::Colour accent { 0xff35d6a4 };
    juce::Colour accentAlt { 0xffffb057 };
    juce::Colour warning { 0xffff5f5f };
    juce::Colour slotHighlight { 0xff57c8ff };

    static juce::Font font (float height, bool bold = false);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosition, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};

} // namespace loopdeck::ui
