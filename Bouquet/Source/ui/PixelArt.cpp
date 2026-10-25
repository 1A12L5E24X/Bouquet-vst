#include "PixelArt.h"

namespace bouquet::ui::pixel
{
namespace
{
    // Bouquet's palette: taken from the painted and the photoreal garden (tools/ui/make_palette.py),
    // with the interface colours kept exactly so the type and the accents stay true.
    constexpr juce::uint32 palette[] = {
        0xfffbf7f0, 0xffefe7da, 0xffffffff, 0xff2b2631, 0xffe8735a, 0xfff4a77c, 0xffe4789b, 0xfff2c14e,
        0xff6fa8f5, 0xffa58cf0, 0xff5cc9a7, 0xff8a6fe6, 0xff6f86f0, 0xff4fb8c9, 0xff282321, 0xff392f2a,
        0xff383d61, 0xff4d4533, 0xff4d6036, 0xff864432, 0xff595842, 0xff585191, 0xff5c6a44, 0xff6d5f5f,
        0xff65734b, 0xff5e8942, 0xffca4f5e, 0xff7f777b, 0xffa46d4e, 0xff76895a, 0xff67974a, 0xff8372d1,
        0xffbf54ea, 0xff749c57, 0xff849265, 0xff6ea64e, 0xff988b90, 0xff8ea16e, 0xff74b552, 0xff82b161,
        0xffcf9e4d, 0xffaaa2a4, 0xffcd84ec, 0xffa0b180, 0xff91bd6f, 0xff9bc478, 0xffadbe8c, 0xffabb6c9,
        0xffb8b6b7, 0xffbacaa5, 0xffb9c2d5, 0xffe6c279, 0xffe2bab4, 0xffc7c8c2, 0xffbccde1, 0xffc8ced8,
        0xffd3d6cc, 0xffcdd5e4, 0xffe3d9b3, 0xffe5d8cf, 0xffd7dfe8, 0xffdeebd5, 0xffe8ebee, 0xfff9f1e2,
    };

    // A 4x4 ordered dither: gradients break into a fixed, even pattern that doesn't crawl.
    constexpr int bayer[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
    constexpr int ditherStrength = 12;   // in 0-255 levels, peak to peak

    // The nearest palette colour for every colour (at 6 bits a channel).
    struct Lookup
    {
        std::array<juce::uint8, 64 * 64 * 64> nearest {};

        Lookup()
        {
            for (int r = 0; r < 64; ++r)
                for (int g = 0; g < 64; ++g)
                    for (int b = 0; b < 64; ++b)
                    {
                        const int cr = r * 4 + 2, cg = g * 4 + 2, cb = b * 4 + 2;
                        int best = 0, bestDistance = std::numeric_limits<int>::max();
                        for (int i = 0; i < (int) std::size (palette); ++i)
                        {
                            const juce::Colour p (palette[i]);
                            // A weighted distance that follows the eye ("redmean").
                            const int mean = (cr + p.getRed()) / 2;
                            const int dr = cr - p.getRed(), dg = cg - p.getGreen(), db = cb - p.getBlue();
                            const int d = (((512 + mean) * dr * dr) >> 8) + 4 * dg * dg + (((767 - mean) * db * db) >> 8);
                            if (d < bestDistance)
                            {
                                bestDistance = d;
                                best = i;
                            }
                        }
                        nearest[(size_t) ((r << 12) | (g << 6) | b)] = (juce::uint8) best;
                    }
        }
    };

    const Lookup& lookup()
    {
        static const Lookup l;
        return l;
    }

    // The ordinary software renderer, except that text lands on whole pixels, so the
    // pixel fonts stay crisp instead of smearing across two columns.
    class Renderer : public juce::LowLevelGraphicsSoftwareRenderer
    {
    public:
        using LowLevelGraphicsSoftwareRenderer::LowLevelGraphicsSoftwareRenderer;

        void drawGlyphs (juce::Span<const juce::uint16> glyphs, juce::Span<const juce::Point<float>> positions,
                         const juce::AffineTransform& t) override
        {
            const auto toDevice = stack->transform.getTransformWith (t);
            const auto toUser = toDevice.inverted();
            std::vector<juce::Point<float>> snapped;
            snapped.reserve (positions.size());
            for (const auto& p : positions)
            {
                const auto d = p.transformedBy (toDevice);
                snapped.push_back (juce::Point<float> (std::round (d.x), std::round (d.y)).transformedBy (toUser));
            }
            LowLevelGraphicsSoftwareRenderer::drawGlyphs (glyphs, snapped, t);
        }
    };
}

void quantise (juce::Image& image, juce::Rectangle<int> area)
{
    area = area.getIntersection (image.getBounds());
    if (area.isEmpty())
        return;

    const auto& nearest = lookup().nearest;
    juce::Image::BitmapData data (image, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
                                  juce::Image::BitmapData::readWrite);
    for (int y = 0; y < area.getHeight(); ++y)
    {
        const auto* row = bayer[(area.getY() + y) & 3];
        for (int x = 0; x < area.getWidth(); ++x)
        {
            auto* px = reinterpret_cast<juce::PixelARGB*> (data.getPixelPointer (x, y));
            const int offset = ((row[(area.getX() + x) & 3] * 2 - 15) * ditherStrength) / 32;
            const int r = juce::jlimit (0, 255, (int) px->getRed() + offset);
            const int g = juce::jlimit (0, 255, (int) px->getGreen() + offset);
            const int b = juce::jlimit (0, 255, (int) px->getBlue() + offset);
            const juce::Colour c (palette[nearest[(size_t) (((r >> 2) << 12) | ((g >> 2) << 6) | (b >> 2))]]);
            px->setARGB (255, c.getRed(), c.getGreen(), c.getBlue());
        }
    }
}

//==============================================================================
bool Canvas::invalidateAll()
{
    dirty = owner.getLocalBounds();
    return true;
}

bool Canvas::invalidate (const juce::Rectangle<int>& area)
{
    dirty.add (area.getIntersection (owner.getLocalBounds()));
    return true;
}

void Canvas::render()
{
    // The changed areas, out to whole art pixels.
    juce::RectangleList<int> area;
    for (const auto& r : dirty)
    {
        const int x0 = r.getX() / size, y0 = r.getY() / size;
        const int x1 = (r.getRight() + size - 1) / size, y1 = (r.getBottom() + size - 1) / size;
        area.add ({ x0, y0, x1 - x0, y1 - y0 });
    }
    dirty.clear();
    area.clipTo (image.getBounds());
    if (area.isEmpty())
        return;

    {
        Renderer context (image, {}, area);
        juce::Graphics g (context);
        g.addTransform (juce::AffineTransform::scale (1.0f / (float) size));
        owner.paintEntireComponent (g, true);
    }
    for (const auto& r : area)
        quantise (image, r);
}

void Canvas::paint (juce::Graphics& g)
{
    const int w = (owner.getWidth() + size - 1) / size, h = (owner.getHeight() + size - 1) / size;
    if (w <= 0 || h <= 0)
        return;
    if (image.getWidth() != w || image.getHeight() != h)
    {
        image = juce::Image (juce::Image::ARGB, w, h, true, juce::SoftwareImageType());
        dirty = owner.getLocalBounds();
    }
    if (! dirty.isEmpty())
        render();

    // Blown up with hard edges: every art pixel a square.
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.setOpacity (1.0f);
    g.drawImageTransformed (image, juce::AffineTransform::scale ((float) size));
}

//==============================================================================
void ZoomConstrainer::checkBounds (juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previous,
                                   const juce::Rectangle<int>& limits, bool top, bool left, bool bottom, bool right)
{
    ComponentBoundsConstrainer::checkBounds (bounds, previous, limits, top, left, bottom, right);
    const float zoom = juce::jlimit (1.0f, 4.0f, std::round ((float) bounds.getWidth() * 2.0f / (float) width)) * 0.5f;
    const int w = juce::roundToInt ((float) width * zoom), h = juce::roundToInt ((float) height * zoom);
    bounds = { left ? bounds.getRight() - w : bounds.getX(), top ? bounds.getBottom() - h : bounds.getY(), w, h };
}
} // namespace bouquet::ui::pixel
