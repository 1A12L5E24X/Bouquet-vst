#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "BouquetLookAndFeel.h"

namespace bouquet::ui
{
// A minimal rotary control: a thin arc of light, its name beneath,
// which turns into the value while you touch it.
class GlowKnob : public juce::Component
{
public:
    GlowKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& label,
              juce::Colour a, juce::Colour b, bool bipolar = false);

    void paint (juce::Graphics&) override;
    void resized() override;

    // Repaints the label while the knob is touched (it shows the value then).
    void refreshIfTouched();

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };

private:
    juce::String label;
    juce::RangedAudioParameter* param;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    bool wasTouched = false;
};

// A selectable word or pill that glows when chosen and warms when hovered.
class GlowItem : public juce::Component
{
public:
    enum class Style { text, pill };

    GlowItem (const juce::String& text, Style, juce::Colour a, juce::Colour b);

    void setText (const juce::String& t) { text = t; }
    void setSelected (bool s) { selected = s; }
    bool isSelected() const { return selected; }
    void setFont (const juce::Font& f) { font = f; }
    void setShowDot (bool d) { showDot = d; }

    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    // Eases hover and selection light towards their targets; call once per frame.
    // Returns true when the item looks different and needs repainting.
    bool animate();

private:
    juce::String text;
    Style style;
    juce::Colour accentA, accentB;
    juce::Font font;
    bool selected = false, showDot = false;
    float hoverLight = 0.0f, selectLight = 0.0f;
};

// A glass orb that freezes time.
class FreezeOrb : public juce::Component
{
public:
    explicit FreezeOrb (juce::AudioProcessorValueTreeState&);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::RangedAudioParameter* param;
    juce::ParameterAttachment attachment;
    bool on = false;
};

// Dark glass keys that bloom with light when played.
class GlowKeyboard : public juce::MidiKeyboardComponent
{
public:
    explicit GlowKeyboard (juce::MidiKeyboardState&);

    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float>, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float>, bool isDown, bool isOver,
                        juce::Colour fillColour) override;
};

// Paints a small four-pointed star.
void drawSparkle (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour);
} // namespace bouquet::ui
