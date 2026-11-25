#include "Widgets.h"

#include "Theme.h"

namespace loopdeck::ui
{

IconButton::IconButton (Kind kindToUse, const juce::String& tooltip)
    : juce::Button (tooltip), kind (kindToUse)
{
    setTooltip (tooltip);
    setButtonText (tooltip);
}

juce::Colour IconButton::getActiveColour() const
{
    switch (kind)
    {
        case Kind::power:
        case Kind::play:
            return Theme::get().accent;

        case Kind::mute:
            return Theme::get().warning;

        case Kind::solo:
        case Kind::panic:
            return Theme::get().accentAlt;

        case Kind::close:
        case Kind::stop:
            break;
    }

    return Theme::get().text;
}

void IconButton::paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    auto& theme = Theme::get();
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    const auto active = getToggleState() || shouldDrawButtonAsDown;

    auto fill = active ? getActiveColour().withAlpha (kind == Kind::close ? 0.22f : 0.9f) : theme.panelAlt;

    if (shouldDrawButtonAsHighlighted && ! active)
        fill = fill.brighter (0.08f);

    g.setColour (fill);
    g.fillRoundedRectangle (bounds, 4.0f);

    g.setColour (active ? getActiveColour() : theme.outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    const auto colour = active ? theme.background : theme.text.withAlpha (0.9f);
    drawGlyph (g, bounds.reduced (juce::jmax (3.0f, bounds.getWidth() * 0.2f)), colour);
}

void IconButton::drawGlyph (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    g.setColour (colour);

    const auto centre = area.getCentre();
    const auto size = juce::jmin (area.getWidth(), area.getHeight());

    switch (kind)
    {
        case Kind::power:
        {
            const auto radius = size * 0.42f;
            juce::Path arc;
            arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                               juce::degreesToRadians (-50.0f), juce::degreesToRadians (230.0f), true);
            g.strokePath (arc, juce::PathStrokeType (juce::jmax (1.4f, size * 0.13f),
                                                    juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawLine (centre.x, centre.y - radius * 1.15f, centre.x, centre.y - radius * 0.1f,
                        juce::jmax (1.4f, size * 0.13f));
            break;
        }

        case Kind::play:
        {
            juce::Path triangle;
            triangle.addTriangle (centre.x - size * 0.28f, centre.y - size * 0.36f,
                                  centre.x - size * 0.28f, centre.y + size * 0.36f,
                                  centre.x + size * 0.36f, centre.y);
            g.fillPath (triangle);
            break;
        }

        case Kind::stop:
            g.fillRoundedRectangle (juce::Rectangle<float> (size * 0.62f, size * 0.62f).withCentre (centre), 1.5f);
            break;

        case Kind::close:
            g.drawLine (centre.x - size * 0.3f, centre.y - size * 0.3f,
                        centre.x + size * 0.3f, centre.y + size * 0.3f, 1.6f);
            g.drawLine (centre.x + size * 0.3f, centre.y - size * 0.3f,
                        centre.x - size * 0.3f, centre.y + size * 0.3f, 1.6f);
            break;

        case Kind::mute:
        case Kind::solo:
        case Kind::panic:
        {
            g.setFont (Theme::font (juce::jmax (9.0f, size * 0.95f), true));
            g.drawText (kind == Kind::mute ? "M" : (kind == Kind::solo ? "S" : "!"),
                        area, juce::Justification::centred, false);
            break;
        }
    }
}

//==============================================================================
Knob::Knob (const juce::String& captionText)
{
    caption.setText (captionText, juce::dontSendNotification);
    caption.setFont (Theme::font (10.0f, true));
    caption.setJustificationType (juce::Justification::centred);
    caption.setColour (juce::Label::textColourId, Theme::get().dimText);
    caption.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (caption);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 13);
    slider.setMouseDragSensitivity (180);
    slider.setColour (juce::Slider::textBoxTextColourId, Theme::get().text);
    addAndMakeVisible (slider);
}

void Knob::attach (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId)
{
    if (auto* parameter = state.getParameter (parameterId))
    {
        slider.setDoubleClickReturnValue (true, parameter->convertFrom0to1 (parameter->getDefaultValue()));
        slider.setValue (parameter->convertFrom0to1 (parameter->getValue()), juce::dontSendNotification);
    }

    sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, parameterId, slider);
}

void Knob::setCaption (const juce::String& newCaption)
{
    caption.setText (newCaption, juce::dontSendNotification);
}

void Knob::resized()
{
    auto area = getLocalBounds();
    caption.setBounds (area.removeFromTop (11));
    slider.setBounds (area);
}

//==============================================================================
void PhaseBar::setPhase (float newPhase, bool isRunning)
{
    if (! juce::approximatelyEqual (newPhase, phase) || isRunning != running)
    {
        phase = newPhase;
        running = isRunning;
        repaint();
    }
}

void PhaseBar::paint (juce::Graphics& g)
{
    auto& theme = Theme::get();
    const auto bounds = getLocalBounds().toFloat();
    const auto trackHeight = juce::jmin (6.0f, bounds.getHeight());

    auto track = bounds.withSizeKeepingCentre (bounds.getWidth(), trackHeight);
    g.setColour (theme.panelDeep);
    g.fillRoundedRectangle (track, trackHeight * 0.5f);

    if (running)
    {
        auto fill = track.withWidth (juce::jmax (2.0f, track.getWidth() * juce::jlimit (0.0f, 1.0f, phase)));
        g.setColour (theme.accent.withAlpha (0.85f));
        g.fillRoundedRectangle (fill, trackHeight * 0.5f);
    }

    g.setColour (theme.outline);
    g.drawRoundedRectangle (track.reduced (0.5f), trackHeight * 0.5f, 1.0f);
}

//==============================================================================
Led::Led (juce::Colour colourToUse) : colour (colourToUse)
{
    setInterceptsMouseClicks (false, false);
}

void Led::setLevel (float newLevel)
{
    const auto clamped = juce::jlimit (0.0f, 1.0f, newLevel);

    if (! juce::approximatelyEqual (clamped, level))
    {
        level = clamped;
        repaint();
    }
}

void Led::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto dot = bounds.withSizeKeepingCentre (juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.62f,
                                                   juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.62f);

    g.setColour (Theme::get().panelDeep);
    g.fillEllipse (bounds);

    auto on = colour.withAlpha (0.18f + 0.82f * level);
    g.setColour (on);
    g.fillEllipse (dot);

    if (level > 0.05f)
    {
        g.setColour (colour.withAlpha (0.18f * level));
        g.drawEllipse (bounds.reduced (0.5f), 2.5f);
    }
}

//==============================================================================
SectionHeader::SectionHeader (const juce::String& text) : title (text)
{
    setInterceptsMouseClicks (false, false);
}

void SectionHeader::setDetail (const juce::String& newDetail)
{
    if (newDetail != detail)
    {
        detail = newDetail;
        repaint();
    }
}

void SectionHeader::paint (juce::Graphics& g)
{
    auto& theme = Theme::get();
    auto area = getLocalBounds();

    g.setColour (theme.dimText);
    g.setFont (Theme::font (10.0f, true));
    g.drawText (title.toUpperCase(), area.removeFromLeft (juce::jmax (0, area.getWidth() - 90)),
                juce::Justification::centredLeft, true);

    if (detail.isNotEmpty())
    {
        g.setColour (theme.accent.withAlpha (0.9f));
        g.setFont (Theme::font (10.0f));
        g.drawText (detail, area, juce::Justification::centredRight, true);
    }
}

//==============================================================================
void fillComboBox (juce::ComboBox& box, const juce::StringArray& items)
{
    box.clear (juce::dontSendNotification);

    for (int i = 0; i < items.size(); ++i)
        box.addItem (items[i], i + 1);

    box.setJustificationType (juce::Justification::centredLeft);
}

void styleComboBox (juce::ComboBox& box)
{
    box.setJustificationType (juce::Justification::centredLeft);
}

} // namespace loopdeck::ui
