#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace loopdeck::ui
{

/** Small flat button used for power / mute / solo / transport actions. */
class IconButton final : public juce::Button
{
public:
    enum class Kind { power, mute, solo, close, play, stop, panic };

    IconButton (Kind kind, const juce::String& tooltip);

    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

    Kind getKind() const noexcept { return kind; }

private:
    juce::Colour getActiveColour() const;
    void drawGlyph (juce::Graphics&, juce::Rectangle<float> area, juce::Colour colour);

    Kind kind;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IconButton)
};

//==============================================================================
/** Caption + rotary slider, attached to a parameter. */
class Knob final : public juce::Component
{
public:
    explicit Knob (const juce::String& captionText);

    void attach (juce::AudioProcessorValueTreeState& state, const juce::String& parameterId);
    void setCaption (const juce::String& newCaption);

    void resized() override;

    juce::Slider slider;

private:
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Knob)
};

//==============================================================================
/** Thin strip that shows the playback position of a slot. */
class PhaseBar final : public juce::Component
{
public:
    void setPhase (float newPhase, bool isRunning);
    void paint (juce::Graphics&) override;

private:
    float phase = 0.0f;
    bool running = false;
};

//==============================================================================
/** Small round indicator. */
class Led final : public juce::Component
{
public:
    explicit Led (juce::Colour colourToUse = juce::Colour (0xff35d6a4));

    void setLevel (float newLevel);
    void paint (juce::Graphics&) override;

private:
    juce::Colour colour;
    float level = 0.0f;
};

//==============================================================================
/** Draws a heading line with an optional right hand side text. */
class SectionHeader final : public juce::Component
{
public:
    explicit SectionHeader (const juce::String& text);

    void setDetail (const juce::String& newDetail);
    void paint (juce::Graphics&) override;

private:
    juce::String title;
    juce::String detail;
};

//==============================================================================
void fillComboBox (juce::ComboBox& box, const juce::StringArray& items);
void styleComboBox (juce::ComboBox& box);

} // namespace loopdeck::ui
