#include "BouquetLookAndFeel.h"
#include "BinaryData.h"

namespace bouquet::ui
{
namespace
{
    juce::Typeface::Ptr load (const void* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    struct Typefaces
    {
        juce::Typeface::Ptr serifLight       = load (BouquetAssets::NewsreaderLight_ttf, BouquetAssets::NewsreaderLight_ttfSize);
        juce::Typeface::Ptr serifLightItalic = load (BouquetAssets::NewsreaderLightItalic_ttf, BouquetAssets::NewsreaderLightItalic_ttfSize);
        juce::Typeface::Ptr serifItalic      = load (BouquetAssets::NewsreaderItalic_ttf, BouquetAssets::NewsreaderItalic_ttfSize);
        juce::Typeface::Ptr sansRegular      = load (BouquetAssets::InterRegular_ttf, BouquetAssets::InterRegular_ttfSize);
        juce::Typeface::Ptr sansMedium       = load (BouquetAssets::InterMedium_ttf, BouquetAssets::InterMedium_ttfSize);
    };

    const Typefaces& typefaces()
    {
        static const Typefaces t;
        return t;
    }

    juce::Font make (const juce::Typeface::Ptr& tf, float height, float tracking = 0.0f)
    {
        return juce::Font (juce::FontOptions (tf).withHeight (height).withKerningFactor (tracking));
    }
}

juce::Font Fonts::serif (float h)               { return make (typefaces().serifLight, h); }
juce::Font Fonts::serifItalic (float h)         { return make (typefaces().serifLightItalic, h); }
juce::Font Fonts::serifItalicRegular (float h)  { return make (typefaces().serifItalic, h); }
juce::Font Fonts::sans (float h, float t)       { return make (typefaces().sansRegular, h, t); }
juce::Font Fonts::sansMedium (float h, float t) { return make (typefaces().sansMedium, h, t); }

static std::atomic<double> clockOverride { -1.0 };

double clock()
{
    static const double start = juce::Time::getMillisecondCounterHiRes();
    if (const double fixed = clockOverride.load(); fixed >= 0.0)
        return fixed;
    return (juce::Time::getMillisecondCounterHiRes() - start) * 0.001;
}

void setClockOverride (double seconds)
{
    clockOverride.store (seconds);
}

void drawGlow (juce::Graphics& g, juce::Point<float> c, float radius, juce::Colour colour)
{
    if (radius < 0.5f || colour.getAlpha() == 0)
        return;

    // A radial gradient shaped like light falling off: bright core, long soft tail.
    juce::ColourGradient grad (colour, c.x, c.y, colour.withAlpha (0.0f), c.x + radius, c.y, true);
    grad.addColour (0.25, colour.withMultipliedAlpha (0.52f));
    grad.addColour (0.5, colour.withMultipliedAlpha (0.2f));
    grad.addColour (0.75, colour.withMultipliedAlpha (0.045f));
    g.setGradientFill (grad);
    g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
}

void drawGlow (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    if (area.isEmpty() || colour.getAlpha() == 0)
        return;

    // A unit radial gradient, stretched to the ellipse inside area.
    juce::ColourGradient grad (colour, 0.0f, 0.0f, colour.withAlpha (0.0f), 1.0f, 0.0f, true);
    grad.addColour (0.3, colour.withMultipliedAlpha (0.55f));
    grad.addColour (0.65, colour.withMultipliedAlpha (0.15f));
    const auto c = area.getCentre();
    g.setFillType (juce::FillType (grad).transformed (juce::AffineTransform::scale (area.getWidth() * 0.5f, area.getHeight() * 0.5f)
                                                                 .translated (c.x, c.y)));
    g.fillEllipse (area);
}

//==============================================================================
BouquetLookAndFeel::BouquetLookAndFeel()
{
    using namespace colours;
    setColour (juce::PopupMenu::backgroundColourId, paper);
    setColour (juce::PopupMenu::textColourId, ink.withAlpha (0.8f));
    setColour (juce::PopupMenu::highlightedBackgroundColourId, lilac.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, ink);
    setColour (juce::ComboBox::textColourId, ink.withAlpha (0.85f));
    setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ComboBox::outlineColourId, ink.withAlpha (0.12f));
    setColour (juce::ComboBox::arrowColourId, ink.withAlpha (0.5f));
    setColour (juce::Slider::rotarySliderFillColourId, violet);
    setColour (juce::Slider::thumbColourId, aqua);
}

juce::Typeface::Ptr BouquetLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return typefaces().sansRegular;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

void BouquetLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float pos,
                                          float startAngle, float endAngle, juce::Slider& s)
{
    using namespace colours;
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (1.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float hover = (s.isMouseOverOrDragging() ? 1.0f : 0.0f);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const auto a = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto b = s.findColour (juce::Slider::thumbColourId);

    const float valueAngle = startAngle + pos * (endAngle - startAngle);
    const float originAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    // A soft white disc, lit from above, resting a little above the glass.
    const float discR = radius * 0.72f;
    const auto disc = juce::Rectangle<float> (discR * 2.0f, discR * 2.0f).withCentre (c);
    g.setColour (ink.withAlpha (0.12f));
    g.fillEllipse (disc.translated (0.0f, 1.6f).expanded (0.6f));
    juce::ColourGradient face (juce::Colours::white, c.x, c.y - discR * 0.25f,
                               juce::Colour (0xffefebe6), c.x, c.y + discR, true);
    face.addColour (0.55, hover > 0.0f ? juce::Colours::white : juce::Colour (0xfffbf9f7));
    g.setGradientFill (face);
    g.fillEllipse (disc);
    g.setColour (ink.withAlpha (0.08f));
    g.drawEllipse (disc, 0.5f);

    auto arc = [&] (float from, float to, float r)
    {
        juce::Path p;
        p.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);
        return p;
    };

    // The value: a thin coloured arc around the disc, with a dot at its end.
    const float r = radius - 1.5f;
    const float stroke = radius > 30.0f ? 2.6f : 2.0f;
    const juce::PathStrokeType line (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    g.setColour (ink.withAlpha (0.09f));
    g.strokePath (arc (startAngle, endAngle, r), line);

    if (std::abs (valueAngle - originAngle) > 0.001f)
    {
        g.setGradientFill (juce::ColourGradient (a, c.x - radius, c.y + radius, b, c.x + radius, c.y - radius, false));
        g.strokePath (arc (std::min (originAngle, valueAngle), std::max (originAngle, valueAngle), r), line);
    }

    const juce::Point<float> dot (c.x + r * std::sin (valueAngle), c.y - r * std::cos (valueAngle));
    const float dotR = radius > 30.0f ? 3.6f : 2.8f;
    g.setColour (frost);
    g.fillEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (dot));
    g.setColour (b);
    g.drawEllipse (juce::Rectangle<float> (dotR * 2.0f, dotR * 2.0f).withCentre (dot), 1.4f);
}

void BouquetLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (colours::frost.withAlpha (box.isMouseOver (true) ? 0.6f : 0.4f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (colours::ink.withAlpha (0.2f));
    g.drawRoundedRectangle (r, 10.0f, 1.0f);

    juce::Path chevron;
    const float cx = r.getRight() - 12.0f, cy = r.getCentreY();
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (colours::ink.withAlpha (0.5f));
    g.strokePath (chevron, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font BouquetLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::sans (13.0f); }

void BouquetLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 0, box.getWidth() - 26, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font BouquetLookAndFeel::getPopupMenuFont() { return Fonts::sans (13.0f); }

void BouquetLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (colours::ink.withAlpha (0.08f));
    g.drawRect (0, 0, width, height);
}
} // namespace bouquet::ui
