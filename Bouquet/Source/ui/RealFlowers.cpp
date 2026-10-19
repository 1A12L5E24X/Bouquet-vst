#include "RealFlowers.h"
#include "BouquetLookAndFeel.h"
#include "../Assets.h"

namespace bouquet::ui::real
{
namespace
{
    const char* speciesNames[] = { "daisy", "tulip", "poppy", "cosmos", "bell", "rose",
                                   "lily", "iris", "dahlia", "peony", "foxglove", "allium" };
    const char* colourNames[] = { "white", "butter", "apricot", "coral", "crimson", "pink", "lilac", "blue" };
    const juce::Colour colourValues[] = {
        juce::Colour (0xfff2efe6), juce::Colour (0xfff5c542), juce::Colour (0xfff7944a), juce::Colour (0xffe8473a),
        juce::Colour (0xff8e0a24), juce::Colour (0xffee8fae), juce::Colour (0xffa98be0), juce::Colour (0xff5c7fdb),
    };
    constexpr int numColours = 8;
    constexpr int atlasColumns = 6;
    constexpr size_t maxAtlases = 36;     // ~6 MB each decoded
    constexpr int blendSteps = 6;

    juce::Point<float> pointFrom (const juce::var& v)
    {
        if (auto* a = v.getArray(); a != nullptr && a->size() >= 2)
            return { (float) (double) (*a)[0], (float) (double) (*a)[1] };
        return {};
    }

    juce::Point<float> alongPolyline (const std::vector<juce::Point<float>>& pts, float t)
    {
        if (pts.size() < 2)
            return pts.empty() ? juce::Point<float>() : pts.front();
        const float x = juce::jlimit (0.0f, 1.0f, t) * (float) (pts.size() - 1);
        const auto i = std::min ((size_t) x, pts.size() - 2);
        return pts[i] + (pts[i + 1] - pts[i]) * (x - (float) i);
    }

    // Premultiplied cross-fade of two equally sized ARGB images: a*(1-t) + b*t.
    juce::Image crossFade (const juce::Image& a, const juce::Image& b, float t)
    {
        juce::Image out (juce::Image::ARGB, a.getWidth(), a.getHeight(), false, juce::SoftwareImageType());
        const juce::Image::BitmapData da (a, juce::Image::BitmapData::readOnly);
        const juce::Image::BitmapData db (b, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dout (out, juce::Image::BitmapData::writeOnly);
        const int wb = juce::roundToInt (t * 256.0f), wa = 256 - wb;
        for (int y = 0; y < out.getHeight(); ++y)
        {
            auto* pa = da.getLinePointer (y);
            auto* pb = db.getLinePointer (y);
            auto* po = dout.getLinePointer (y);
            for (int x = 0; x < out.getWidth() * 4; ++x)
                po[x] = (juce::uint8) ((pa[x] * wa + pb[x] * wb) >> 8);
        }
        return out;
    }
}

//==============================================================================
class FlowerSprites::Loader : public juce::ThreadPoolJob
{
public:
    Loader (FlowerSprites& o, int k, juce::File c, juce::File a)
        : juce::ThreadPoolJob ("flower atlas"), owner (o), key (k), colour (std::move (c)), alpha (std::move (a)) {}

    JobStatus runJob() override
    {
        // Scaled while decoding, to a whole grid of equally sized frames.
        const int rows = owner.frameRows;
        auto image = FlowerSprites::loadWithAlpha (colour, alpha, FlowerSprites::scaled (owner.frameWidth) * atlasColumns,
                                                   FlowerSprites::scaled (owner.frameHeight) * rows);
        const std::lock_guard<std::mutex> l (owner.lock);
        owner.requested.erase (key);
        if (image.isValid())
        {
            // Keep memory bounded: forget the least recently used atlas.
            while (owner.atlases.size() >= maxAtlases)
            {
                auto oldest = owner.atlases.begin();
                for (auto it = owner.atlases.begin(); it != owner.atlases.end(); ++it)
                    if (it->second.lastUsed < oldest->second.lastUsed)
                        oldest = it;
                owner.blends.erase (oldest->first);
                owner.atlases.erase (oldest);
            }
            owner.atlases[key] = { image, ++owner.useCounter };
        }
        return jobHasFinished;
    }

private:
    FlowerSprites& owner;
    int key;
    juce::File colour, alpha;
};

//==============================================================================
FlowerSprites::FlowerSprites() : folder (assets::find ("Flowers", "daisy0.json"))
{
    if (! folder.isDirectory())
        return;

    for (int s = 0; s < (int) botany::Species::count; ++s)
    {
        for (int v = 0; v < numVariants; ++v)
        {
            const auto json = juce::JSON::parse (folder.getChildFile (juce::String (speciesNames[s]) + juce::String (v) + ".json"));
            if (! json.isObject())
                continue;
            Info info;
            info.width = (int) json["width"];
            info.height = (int) json["height"];
            bloomCount = (int) json["bloomFrames"];
            rotCount = (int) json["rotFrames"];
            if (auto* frames = json["frames"].getArray())
                for (auto& f : *frames)
                {
                    std::vector<juce::Point<float>> stem;
                    if (auto* pts = f["stem"].getArray())
                        for (auto& p : *pts)
                            stem.push_back (pointFrom (p));
                    info.stems.push_back (std::move (stem));
                    info.heads.push_back (pointFrom (f["head"]));
                }
            frameWidth = info.width;
            frameHeight = info.height;
            frameRows = ((int) info.stems.size() + atlasColumns - 1) / atlasColumns;
            if (info.width > 0 && ! info.stems.empty())
                infos[infoKey ((botany::Species) s, v)] = std::move (info);
        }
    }

    grassSheet = juce::ImageFileFormat::loadFrom (folder.getChildFile ("grass.png"));
    const auto grassInfo = juce::JSON::parse (folder.getChildFile ("grass.json"));
    grassTufts = (int) grassInfo.getProperty ("count", 0);
    grassGroundY = (float) (double) grassInfo.getProperty ("ground", 0.0);
    extrasSheet = juce::ImageFileFormat::loadFrom (folder.getChildFile ("extras.png"));
    extrasSize = (int) juce::JSON::parse (folder.getChildFile ("extras.json")).getProperty ("size", 64);

    pool = std::make_unique<juce::ThreadPool> (juce::ThreadPoolOptions().withThreadName ("Bouquet flowers").withNumberOfThreads (2));
}

FlowerSprites::~FlowerSprites()
{
    if (pool != nullptr)
        pool->removeAllJobs (true, 4000);
}

juce::Image FlowerSprites::loadWithAlpha (const juce::File& colourFile, const juce::File& alphaFile, int width, int height)
{
    // Both decoded to ARGB so their bytes can be read directly (blue, green, red, alpha).
    const auto colour = juce::ImageFileFormat::loadFrom (colourFile).convertedToFormat (juce::Image::ARGB);
    auto alpha = juce::ImageFileFormat::loadFrom (alphaFile);
    if (! colour.isValid() || ! alpha.isValid() || colour.getBounds() != alpha.getBounds())
        return {};
    const bool alphaInAlpha = alpha.getFormat() == juce::Image::SingleChannel;
    alpha = alpha.convertedToFormat (juce::Image::ARGB);

    const int sw = colour.getWidth(), sh = colour.getHeight();
    if (width <= 0 || height <= 0) { width = sw; height = sh; }

    juce::Image out (juce::Image::ARGB, width, height, false, juce::SoftwareImageType());
    const juce::Image::BitmapData dc (colour, juce::Image::BitmapData::readOnly);
    const juce::Image::BitmapData da (alpha, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dout (out, juce::Image::BitmapData::writeOnly);
    const int alphaByte = alphaInAlpha ? 3 : 2;   // a greyscale mask arrives as grey RGB
    const float fx = (float) sw / (float) width, fy = (float) sh / (float) height;

    // One pass: scale (bilinear, centre-aligned), attach the mask, premultiply.
    for (int y = 0; y < height; ++y)
    {
        const float sy = juce::jlimit (0.0f, (float) (sh - 1), ((float) y + 0.5f) * fy - 0.5f);
        const int y0 = (int) sy, y1 = std::min (y0 + 1, sh - 1);
        const float ty = sy - (float) y0;
        const juce::uint8* c0 = dc.getLinePointer (y0);
        const juce::uint8* c1 = dc.getLinePointer (y1);
        const juce::uint8* a0 = da.getLinePointer (y0);
        const juce::uint8* a1 = da.getLinePointer (y1);
        juce::uint8* o = dout.getLinePointer (y);
        for (int x = 0; x < width; ++x)
        {
            const float sx = juce::jlimit (0.0f, (float) (sw - 1), ((float) x + 0.5f) * fx - 0.5f);
            const int x0 = (int) sx, x1 = std::min (x0 + 1, sw - 1);
            const float tx = sx - (float) x0;
            auto lerp = [&] (const juce::uint8* r0, const juce::uint8* r1, int stride, int channel)
            {
                const float top = r0[x0 * stride + channel] + (r0[x1 * stride + channel] - r0[x0 * stride + channel]) * tx;
                const float bottom = r1[x0 * stride + channel] + (r1[x1 * stride + channel] - r1[x0 * stride + channel]) * tx;
                return top + (bottom - top) * ty;
            };
            const float a = lerp (a0, a1, da.pixelStride, alphaByte) / 255.0f;
            auto* px = o + x * dout.pixelStride;
            for (int ch = 0; ch < 3; ++ch)
                px[ch] = (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (lerp (c0, c1, dc.pixelStride, ch) * a));
            px[3] = (juce::uint8) juce::roundToInt (a * 255.0f);
        }
    }
    return out;
}

int FlowerSprites::nearestColour (juce::Colour c)
{
    int best = 0;
    float bestDistance = 1.0e9f;
    for (int i = 0; i < numColours; ++i)
    {
        const auto& w = colourValues[i];
        const float dh = std::min (std::abs (w.getHue() - c.getHue()), 1.0f - std::abs (w.getHue() - c.getHue()));
        // Pale colours are told apart by brightness and saturation, vivid ones by hue.
        const float sat = std::min (w.getSaturation(), c.getSaturation());
        const float d = dh * dh * 6.0f * sat + std::pow (w.getSaturation() - c.getSaturation(), 2.0f) * 1.5f
                      + std::pow (w.getBrightness() - c.getBrightness(), 2.0f);
        if (d < bestDistance) { bestDistance = d; best = i; }
    }
    return best;
}

juce::Colour FlowerSprites::colourway (int index)
{
    return colourValues[juce::jlimit (0, numColours - 1, index)];
}

float FlowerSprites::framePosition (float bloom, float rot) const
{
    if (rot <= 0.0f)
        return juce::jlimit (0.0f, (float) (bloomCount - 1), bloom * (float) (bloomCount - 1));
    return juce::jlimit ((float) (bloomCount - 1), (float) (bloomCount + rotCount - 1), (float) (bloomCount - 1) + rot * (float) rotCount);
}

juce::Image FlowerSprites::atlas (int key, const juce::String& baseName, bool wait)
{
    {
        const std::lock_guard<std::mutex> l (lock);
        if (auto it = atlases.find (key); it != atlases.end())
        {
            it->second.lastUsed = ++useCounter;
            return it->second.image;
        }
        if (requested.count (key) == 0 && pool != nullptr && ! wait)
        {
            requested[key] = baseName;
            pool->addJob (new Loader (*this, key, folder.getChildFile (baseName + ".jpg"),
                                      folder.getChildFile (baseName + ".alpha.png")), true);
        }
    }
    if (! wait)
        return {};
    Loader (*this, key, folder.getChildFile (baseName + ".jpg"), folder.getChildFile (baseName + ".alpha.png")).runJob();
    const std::lock_guard<std::mutex> l (lock);
    auto it = atlases.find (key);
    return it != atlases.end() ? it->second.image : juce::Image();
}

juce::Image FlowerSprites::frameImage (int, const Info& info, const juce::Image& sheet, int frame) const
{
    const int w = scaled (info.width), h = scaled (info.height);
    return sheet.getClippedImage ({ (frame % atlasColumns) * w, (frame / atlasColumns) * h, w, h });
}

FlowerSprites::Frame FlowerSprites::get (botany::Species s, int variant, int colour, float position)
{
    Frame f;
    variant = juce::jlimit (0, numVariants - 1, variant);
    auto it = infos.find (infoKey (s, variant));
    if (it == infos.end())
        it = infos.find (infoKey (s, 0));
    if (it == infos.end())
        return f;
    const auto& info = it->second;
    const int last = (int) info.stems.size() - 1;
    position = juce::jlimit (0.0f, (float) last, position);
    const int a = std::min ((int) position, last), b = std::min (a + 1, last);
    const int step = juce::roundToInt ((position - (float) a) * (float) blendSteps);
    const int nearest = step >= blendSteps ? b : a;

    f.stem = info.stems[(size_t) nearest];
    f.head = info.heads[(size_t) nearest];
    f.base = f.stem.empty() ? juce::Point<float> ((float) info.width * 0.5f, (float) info.height) : f.stem.front();

    colour = juce::jlimit (0, numColours - 1, colour);
    const int key = atlasKey (s, variant, colour);
    const auto sheet = atlas (key, juce::String (speciesNames[(int) s]) + juce::String (variant) + "_" + colourNames[colour],
                              waitForLoads || ! juce::MessageManager::getInstanceWithoutCreating());
    if (! sheet.isValid())
        return f;

    f.imageScale = atlasScale;
    if (step == 0 || step >= blendSteps || a == b)
    {
        f.image = frameImage (key, info, sheet, nearest);
        return f;
    }

    // Between two frames: cross-fade, and keep the result while it is still current.
    const std::lock_guard<std::mutex> l (lock);
    auto& blend = blends[key];
    if (blend.a != a || blend.b != b || blend.step != step || ! blend.image.isValid())
    {
        blend.image = crossFade (frameImage (key, info, sheet, a), frameImage (key, info, sheet, b), (float) step / (float) blendSteps);
        blend.a = a;
        blend.b = b;
        blend.step = step;
    }
    f.image = blend.image;
    return f;
}

void FlowerSprites::prefetch (botany::Species s, int variant, int colour)
{
    variant = juce::jlimit (0, numVariants - 1, variant);
    colour = juce::jlimit (0, numColours - 1, colour);
    if (infos.count (infoKey (s, variant)) != 0)
        atlas (atlasKey (s, variant, colour), juce::String (speciesNames[(int) s]) + juce::String (variant) + "_" + colourNames[colour], false);
}

juce::Image FlowerSprites::top (botany::Species s, int variant, int colour)
{
    variant = juce::jlimit (0, numVariants - 1, variant);
    const int key = infoKey (s, variant);
    auto it = tops.find (key);
    if (it == tops.end())
    {
        const auto base = juce::String (speciesNames[(int) s]) + juce::String (variant) + "_top";
        it = tops.emplace (key, loadWithAlpha (folder.getChildFile (base + ".jpg"), folder.getChildFile (base + ".alpha.png"))).first;
    }
    if (! it->second.isValid())
        return {};
    const int size = it->second.getHeight();
    return it->second.getClippedImage ({ juce::jlimit (0, numColours - 1, colour) * size, 0, size, size });
}

juce::Image FlowerSprites::grass (int index) const
{
    if (! grassSheet.isValid() || grassTufts <= 0)
        return {};
    const int w = grassSheet.getWidth() / grassTufts;
    return grassSheet.getClippedImage ({ (((index % grassTufts) + grassTufts) % grassTufts) * w, 0, w, grassSheet.getHeight() });
}

juce::Image FlowerSprites::fly (int wingPhase) const
{
    if (! extrasSheet.isValid())
        return {};
    return extrasSheet.getClippedImage ({ (((wingPhase % 4) + 4) % 4) * extrasSize, 0, extrasSize, extrasSize });
}

juce::Image FlowerSprites::drop() const
{
    if (! extrasSheet.isValid())
        return {};
    return extrasSheet.getClippedImage ({ 4 * extrasSize, 0, extrasSize, extrasSize });
}

juce::Image FlowerSprites::sky()
{
    if (! skyImage.isValid() && folder.isDirectory())
        skyImage = juce::ImageFileFormat::loadFrom (folder.getChildFile ("sky.jpg"));
    return skyImage;
}

juce::Image FlowerSprites::ground()
{
    if (! groundImage.isValid() && folder.isDirectory())
        groundImage = juce::ImageFileFormat::loadFrom (folder.getChildFile ("ground.jpg"));
    return groundImage;
}

//==============================================================================
void paintFlower (juce::Graphics& g, FlowerSprites& sprites, botany::Species species, int variant, int colour,
                  juce::Point<float> base, float height, float lean, float grow, float bloom, float rot,
                  float time, float glow, float alpha)
{
    const float fade = (rot > 0.88f ? 1.0f - (rot - 0.88f) / 0.12f : 1.0f) * alpha;
    if (fade <= 0.01f || grow <= 0.0f)
        return;

    const auto frame = sprites.get (species, variant, colour, sprites.framePosition (grow < 1.0f ? 0.0f : bloom, rot));
    const auto open = sprites.get (species, variant, colour, (float) (sprites.bloomFrames() - 1));
    if (! frame.image.isValid())
        return;

    // Scale from the fully open plant so the size stays steady as it droops.
    const float spriteHeight = std::max (1.0f, open.base.y - open.head.y);
    const float scale = height / spriteHeight;
    const auto t = juce::AffineTransform::translation (-frame.base.x, -frame.base.y)
                       .scaled (scale)
                       .rotated (lean)
                       .translated (base);

    juce::Graphics::ScopedSaveState save (g);

    // Growing: the plant is revealed from the ground up.
    if (grow < 1.0f)
    {
        const float visibleTop = frame.base.y - (frame.base.y + 4.0f) * grow;
        juce::Path reveal;
        reveal.addRectangle (juce::Rectangle<float> (0.0f, visibleTop, (float) frame.image.getWidth() / frame.imageScale,
                                                     (float) frame.image.getHeight() / frame.imageScale - visibleTop));
        reveal.applyTransform (t);
        g.reduceClipRegion (reveal);
    }

    const auto head = frame.head.transformedBy (t);
    if (glow > 0.01f)
        drawGlow (g, head, height * 0.45f, FlowerSprites::colourway (colour).withAlpha (0.35f * glow * fade));

    g.setOpacity (fade);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageTransformed (frame.image, juce::AffineTransform::scale (1.0f / frame.imageScale).followedBy (t));
    g.setOpacity (1.0f);

    if (rot <= 0.15f)
        return;

    // A fresh drop of blood keeps sliding down the stem.
    const auto drop = sprites.drop();
    if (rot < 0.7f && frame.stem.size() >= 2 && drop.isValid())
    {
        std::vector<juce::Point<float>> stem;
        for (auto& p : frame.stem)
            stem.push_back (p.transformedBy (t));
        const float s = 1.0f - std::fmod (time * 0.22f + (float) colour * 0.13f, 1.0f);
        const auto at = alongPolyline (stem, s);
        const float size = juce::jlimit (4.0f, 9.0f, height * 0.05f);
        g.setOpacity (fade);
        g.drawImage (drop, juce::Rectangle<float> (size, size).withCentre (at));
        g.setOpacity (1.0f);
    }

    // Flies drawn to it.
    if (rot > 0.25f && rot < 0.97f)
    {
        const float present = juce::jlimit (0.0f, 1.0f, (rot - 0.25f) / 0.1f) * fade;
        const float r = height * 0.2f;
        const float size = juce::jlimit (10.0f, 22.0f, height * 0.11f);
        for (int f = 0; f < 2; ++f)
        {
            const float a = time * (3.1f + (float) f * 1.7f) + (float) colour + (float) f * 2.0f;
            const auto pos = head + juce::Point<float> (std::cos (a) * r * 1.3f, std::sin (a * 1.3f) * r * 0.8f - r * 0.3f);
            const auto sprite = sprites.fly ((int) (time * 60.0f) + f * 2);
            if (! sprite.isValid())
                continue;
            // Face the way it is flying.
            const float heading = std::atan2 (std::cos (a * 1.3f) * 1.04f, -std::sin (a) * 1.3f) + juce::MathConstants<float>::halfPi;
            g.setOpacity (present);
            g.drawImageTransformed (sprite, juce::AffineTransform::translation (-(float) sprite.getWidth() * 0.5f, -(float) sprite.getHeight() * 0.5f)
                                                .scaled (size / (float) sprite.getWidth())
                                                .rotated (heading)
                                                .translated (pos));
            g.setOpacity (1.0f);
        }
    }
}
} // namespace bouquet::ui::real
