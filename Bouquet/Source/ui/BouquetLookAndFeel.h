#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bouquet::ui
{
// The palette: a bright day. Warm charcoal ink on sunlit paper, warm light on the
// left (Sound Lab), cool sky light on the right (Magic Lab).
namespace colours
{
    const juce::Colour paper     { 0xfffbf7f0 };   // warm white
    const juce::Colour paperDeep { 0xffefe7da };
    const juce::Colour frost     { 0xffffffff };   // frosted glass
    const juce::Colour ink       { 0xff2b2631 };   // warm charcoal

    const juce::Colour coral     { 0xffe8735a };
    const juce::Colour peach     { 0xfff4a77c };
    const juce::Colour rose      { 0xffe4789b };
    const juce::Colour butter    { 0xfff2c14e };
    const juce::Colour sky       { 0xff6fa8f5 };
    const juce::Colour lilac     { 0xffa58cf0 };
    const juce::Colour mint      { 0xff5cc9a7 };

    // Roles carried over from the first design.
    const juce::Colour amber     = butter;
    const juce::Colour violet    { 0xff8a6fe6 };
    const juce::Colour lavender  = lilac;
    const juce::Colour indigo    { 0xff6f86f0 };
    const juce::Colour aqua      { 0xff4fb8c9 };
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
// An elliptical pool of light that fades out exactly at the edges of area.
void drawGlow (juce::Graphics&, juce::Rectangle<float> area, juce::Colour);

class BouquetLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BouquetLookAndFeel();

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
} // namespace bouquet::ui
