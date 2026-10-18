#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace murmur::ui
{
// The palette: ivory ink on a deep night, warm light on the left, cool light on the right.
namespace colours
{
    const juce::Colour night     { 0xff0c0a12 };
    const juce::Colour nightDeep { 0xff07060b };
    const juce::Colour ink       { 0xfff3ede4 };
    const juce::Colour coral     { 0xffe8836b };
    const juce::Colour peach     { 0xfff2b48a };
    const juce::Colour rose      { 0xffe77fa0 };
    const juce::Colour amber     { 0xfff5c26b };
    const juce::Colour violet    { 0xff9b7bff };
    const juce::Colour lavender  { 0xffc3a8ff };
    const juce::Colour indigo    { 0xff5b6cff };
    const juce::Colour aqua      { 0xff6fe3d6 };
}

struct Fonts
{
    static juce::Font serif (float height);         // Newsreader Light
    static juce::Font serifItalic (float height);   // Newsreader Light Italic
    static juce::Font serifItalicRegular (float height);
    static juce::Font sans (float height, float tracking = 0.0f);
    static juce::Font sansMedium (float height, float tracking = 0.0f);
};

// Seconds since start-up; every animation in the UI reads this one clock.
double clock();

// A soft round pool of light.
void drawGlow (juce::Graphics&, juce::Point<float> centre, float radius, juce::Colour);

class MurmurLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MurmurLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};
} // namespace murmur::ui
