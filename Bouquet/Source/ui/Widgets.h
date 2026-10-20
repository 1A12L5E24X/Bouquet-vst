#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "BouquetLookAndFeel.h"

namespace bouquet::ui
{
// A rotary control: a white disc inside a thin coloured arc, its name beneath,
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

// A selectable word: a voice (with a dot that lights when chosen), a tab (underlined
// when chosen), or a button (a word in a framed glass cell).
class GlowItem : public juce::Component
{
public:
    enum class Style { text, tab, pill };

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

// A row (or grid) of choices in one frame, divided by hairlines; the chosen cell is
// tinted with the accent. A cell can be passive (shown, not clickable), which makes
// a stepper of three cells: minus, the value, plus.
class Segmented : public juce::Component
{
public:
    Segmented (juce::Colour accent, int columns);

    void setItems (const juce::StringArray&);
    void setText (int index, const juce::String&);
    void setSelected (int index);
    void setPassive (int index) { passive = index; }
    void setColumnWeights (std::vector<float> w) { weights = std::move (w); }
    int getNumItems() const { return items.size(); }

    std::function<void (int)> onSelect;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> cell (int index) const;
    int cellAt (juce::Point<float>) const;

    juce::Colour accent;
    int columns;
    juce::StringArray items;
    std::vector<float> weights;
    int selected = -1, hovered = -1, passive = -1;
};

// An on/off switch with its name beside it.
class Toggle : public juce::Component
{
public:
    explicit Toggle (const juce::String& label);

    void setOn (bool);
    bool isOn() const { return on; }
    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

    // Slides the knob towards its side; call once per frame. True when it moved.
    bool animate();

private:
    juce::String label;
    bool on = false;
    float position = 0.0f;
};

// A plain word with a small line icon in front, for actions in the header.
class HeaderButton : public juce::Component
{
public:
    enum class Icon { sprout, sparkle };
    HeaderButton (const juce::String& label, Icon);

    std::function<void()> onClick;

    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override  { repaint(); }

private:
    juce::String label;
    Icon icon;
};

// The freeze button: a small glass square with a snowflake, which tints when frozen.
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

// Ivory and ebony keys in one rounded frame; played keys warm with a coral edge.
class GlowKeyboard : public juce::MidiKeyboardComponent
{
public:
    explicit GlowKeyboard (juce::MidiKeyboardState&);

    void paint (juce::Graphics&) override;

    void drawWhiteNote (int note, juce::Graphics&, juce::Rectangle<float>, bool isDown, bool isOver,
                        juce::Colour lineColour, juce::Colour textColour) override;
    void drawBlackNote (int note, juce::Graphics&, juce::Rectangle<float>, bool isDown, bool isOver,
                        juce::Colour fillColour) override;
};

// Paints a small four-pointed star.
void drawSparkle (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour);
} // namespace bouquet::ui
