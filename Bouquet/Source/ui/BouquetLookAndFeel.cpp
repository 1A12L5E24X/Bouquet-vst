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

double clock()
{
    static const double start = juce::Time::getMillisecondCounterHiRes();
    return (juce::Time::getMillisecondCounterHiRes() - start) * 0.001;
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
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (3.0f);
    const float radius = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float hover = (s.isMouseOverOrDragging() ? 1.0f : 0.0f);
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const auto a = s.findColour (juce::Slider::rotarySliderFillColourId);
    const auto b = s.findColour (juce::Slider::thumbColourId);

    const float valueAngle = startAngle + pos * (endAngle - startAngle);
    const float originAngle = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;

    // The well: a small frosted disc.
    const auto well = juce::Rectangle<float> (radius * 1.35f, radius * 1.35f).withCentre (c);
    g.setColour (frost.withAlpha (0.45f + 0.25f * hover));
    g.fillEllipse (well);
    g.setColour (ink.withAlpha (0.06f));
    g.drawEllipse (well, 0.8f);

    auto arc = [&] (float from, float to, float r)
    {
        juce::Path p;
        p.addCentredArc (c.x, c.y, r, r, 0.0f, from, to, true);
        return p;
    };

    const float r = radius * 0.86f;
    g.setColour (ink.withAlpha (0.10f));
    g.strokePath (arc (startAngle, endAngle, r), juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (std::abs (valueAngle - originAngle) > 0.001f)
    {
        const auto value = arc (std::min (originAngle, valueAngle), std::max (originAngle, valueAngle), r);
        juce::ColourGradient grad (a, c.x - radius, c.y + radius, b, c.x + radius, c.y - radius, false);

        // Glow layers, then the bright line.
        for (auto [w, alpha] : { std::pair { 9.0f, 0.08f }, std::pair { 5.0f, 0.16f } })
        {
            g.setGradientFill (grad);
            g.setOpacity (alpha * (1.0f + 0.6f * hover));
            g.strokePath (value, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setOpacity (1.0f);
        g.setGradientFill (grad);
        g.strokePath (value, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const juce::Point<float> dot (c.x + r * std::sin (valueAngle), c.y - r * std::cos (valueAngle));
    drawGlow (g, dot, 9.0f + 3.0f * hover, b.withAlpha (0.45f));
    g.setColour (frost);
    g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dot));
    g.setColour (b.interpolatedWith (ink, 0.15f));
    g.drawEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dot), 1.2f);
}

void BouquetLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0, 0, (float) width, (float) height).reduced (0.5f);
    g.setColour (colours::ink.withAlpha (box.isMouseOver (true) ? 0.22f : 0.12f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

    juce::Path chevron;
    const float cx = r.getRight() - 14.0f, cy = r.getCentreY();
    chevron.startNewSubPath (cx - 3.5f, cy - 1.5f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 3.5f, cy - 1.5f);
    g.setColour (colours::ink.withAlpha (0.5f));
    g.strokePath (chevron, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font BouquetLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::serifItalic (16.0f); }

void BouquetLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (12, 0, box.getWidth() - 30, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

juce::Font BouquetLookAndFeel::getPopupMenuFont() { return Fonts::serifItalic (16.0f); }

void BouquetLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (colours::ink.withAlpha (0.08f));
    g.drawRect (0, 0, width, height);
}
} // namespace bouquet::ui
