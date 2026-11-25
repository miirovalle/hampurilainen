#include "Theme.h"

namespace loopdeck::ui
{

Theme& Theme::get()
{
    static Theme instance;
    return instance;
}

Theme::Theme()
{
    setColour (juce::ResizableWindow::backgroundColourId, background);

    setColour (juce::Label::textColourId, text);
    setColour (juce::TextButton::buttonColourId, panelAlt);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, background);

    setColour (juce::ComboBox::backgroundColourId, panelAlt);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::arrowColourId, dimText);
    setColour (juce::ComboBox::buttonColourId, panelAlt);

    setColour (juce::PopupMenu::backgroundColourId, panelAlt);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent);
    setColour (juce::PopupMenu::highlightedTextColourId, background);
    setColour (juce::PopupMenu::headerTextColourId, dimText);

    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, outline);
    setColour (juce::Slider::thumbColourId, accent);

    setColour (juce::TextEditor::backgroundColourId, panelDeep);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, accent);

    setColour (juce::ListBox::backgroundColourId, panelDeep);
    setColour (juce::ListBox::outlineColourId, outline);
    setColour (juce::ScrollBar::thumbColourId, outline.brighter (0.25f));
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);

    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::ToggleButton::tickDisabledColourId, outline);

    setColour (juce::TooltipWindow::backgroundColourId, panelAlt);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, outline);

    setColour (juce::AlertWindow::backgroundColourId, panel);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, outline);
}

juce::Font Theme::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions (height).withStyle (bold ? juce::String ("Bold") : juce::String()));
}

void Theme::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPosition, float rotaryStartAngle, float rotaryEndAngle,
                              juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                            .reduced (2.0f);
    const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto lineWidth = juce::jmax (2.0f, radius * 0.16f);
    const auto arcRadius = radius - lineWidth * 0.5f;

    const auto angle = rotaryStartAngle + sliderPosition * (rotaryEndAngle - rotaryStartAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (outline.withAlpha (0.85f));
    g.strokePath (track, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (sliderPosition > 0.0001f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                             rotaryStartAngle, angle, true);
        g.setColour (accent);
        g.strokePath (value, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    juce::Path pointer;
    const auto pointerLength = arcRadius * 0.62f;
    pointer.addRoundedRectangle (-lineWidth * 0.28f, -arcRadius, lineWidth * 0.56f, pointerLength, lineWidth * 0.3f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

    g.setColour (text);
    g.fillPath (pointer);
}

void Theme::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const auto corner = 4.0f;

    auto fill = backgroundColour;

    if (shouldDrawButtonAsDown)
        fill = fill.brighter (0.12f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter (0.06f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, corner);

    g.setColour (outline);
    g.drawRoundedRectangle (bounds, corner, 1.0f);
}

void Theme::drawComboBox (juce::Graphics& g, int width, int height, bool,
                          int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat().reduced (0.5f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (box.isEnabled() ? outline : outline.withAlpha (0.4f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    juce::Path arrow;
    const auto arrowArea = juce::Rectangle<float> ((float) width - 14.0f, 0.0f, 10.0f, (float) height);
    const auto cx = arrowArea.getCentreX();
    const auto cy = arrowArea.getCentreY();
    arrow.startNewSubPath (cx - 3.5f, cy - 1.5f);
    arrow.lineTo (cx, cy + 2.0f);
    arrow.lineTo (cx + 3.5f, cy - 1.5f);

    g.setColour (box.isEnabled() ? dimText : dimText.withAlpha (0.4f));
    g.strokePath (arrow, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void Theme::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (font ((float) juce::jlimit (10, 14, box.getHeight() - 12)));
    label.setColour (juce::Label::textColourId, box.findColour (juce::ComboBox::textColourId));
}

void Theme::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto boxSize = juce::jmin (16.0f, bounds.getHeight() - 2.0f);
    const auto boxBounds = juce::Rectangle<float> (bounds.getX() + 1.0f,
                                                   bounds.getCentreY() - boxSize * 0.5f,
                                                   boxSize, boxSize);

    g.setColour (button.getToggleState() ? accent : panelAlt);
    g.fillRoundedRectangle (boxBounds, 3.0f);

    g.setColour (button.getToggleState() ? accent : (highlighted ? outline.brighter (0.3f) : outline));
    g.drawRoundedRectangle (boxBounds, 3.0f, 1.2f);

    g.setColour (button.getToggleState() ? background : text);
    g.setFont (font (11.0f, true));
    g.drawText (button.getButtonText(),
                bounds.withTrimmedLeft (boxSize + 8.0f),
                juce::Justification::centredLeft, true);
}

void Theme::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));

    if (! label.isBeingEdited())
    {
        const auto alpha = label.isEnabled() ? 1.0f : 0.45f;

        g.setColour (label.findColour (juce::Label::textColourId).withMultipliedAlpha (alpha));
        g.setFont (label.getFont());
        g.drawFittedText (label.getText(), label.getLocalBounds().reduced (2, 0),
                          label.getJustificationType(), 1,
                          label.getMinimumHorizontalScale());
    }
}

} // namespace loopdeck::ui
