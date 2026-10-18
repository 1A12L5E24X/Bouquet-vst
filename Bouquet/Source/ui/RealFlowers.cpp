#include "RealFlowers.h"
#include "BouquetLookAndFeel.h"

namespace bouquet::ui::real
{
namespace
{
    const char* speciesNames[] = { "daisy", "tulip", "poppy", "cosmos", "bell", "rose" };
    const char* colourNames[] = { "white", "butter", "apricot", "coral", "crimson", "pink", "lilac", "blue" };
    const juce::Colour colourValues[] = {
        juce::Colour (0xfff2efe6), juce::Colour (0xfff5c542), juce::Colour (0xfff7944a), juce::Colour (0xffe8473a),
        juce::Colour (0xff8e0a24), juce::Colour (0xffee8fae), juce::Colour (0xffa98be0), juce::Colour (0xff5c7fdb),
    };
    constexpr int numColours = 8;
    constexpr int atlasColumns = 6;
    constexpr size_t maxAtlases = 18;

    int key (botany::Species s, int colour) { return (int) s * 16 + colour; }

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
}

juce::File FlowerSprites::findFolder()
{
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    const juce::File candidates[] = {
        exe.getParentDirectory().getParentDirectory().getChildFile ("Resources/Flowers"),   // any bundle: .vst3 .component .app
        exe.getParentDirectory().getChildFile ("Flowers"),                                   // a plain executable
       #ifdef BOUQUET_ASSET_DIR
        juce::File (BOUQUET_ASSET_DIR),                                                      // development and tests
       #endif
    };
    for (auto& c : candidates)
        if (c.getChildFile ("daisy.json").existsAsFile())
            return c;
    return {};
}

FlowerSprites::FlowerSprites() : folder (findFolder())
{
    if (! folder.isDirectory())
        return;

    for (int s = 0; s < (int) botany::Species::count; ++s)
    {
        const auto json = juce::JSON::parse (folder.getChildFile (juce::String (speciesNames[s]) + ".json"));
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
        if (info.width > 0 && ! info.stems.empty())
            infos[s] = std::move (info);
    }
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

int FlowerSprites::frameFor (float bloom, float rot) const
{
    if (rot <= 0.0f)
        return juce::jlimit (0, bloomCount - 1, juce::roundToInt (bloom * (float) (bloomCount - 1)));
    return juce::jlimit (bloomCount, bloomCount + rotCount - 1, bloomCount - 1 + (int) std::ceil (rot * (float) rotCount));
}

juce::Image FlowerSprites::atlas (botany::Species s, int colour)
{
    const int k = key (s, colour);
    if (auto it = atlases.find (k); it != atlases.end())
    {
        it->second.second = ++useCounter;
        return it->second.first;
    }

    // Keep memory bounded: forget the least recently used atlas.
    if (atlases.size() >= maxAtlases)
    {
        auto oldest = atlases.begin();
        for (auto it = atlases.begin(); it != atlases.end(); ++it)
            if (it->second.second < oldest->second.second)
                oldest = it;
        atlases.erase (oldest);
    }

    const auto file = folder.getChildFile (juce::String (speciesNames[(int) s]) + "_" + colourNames[colour] + ".png");
    auto image = juce::ImageFileFormat::loadFrom (file);
    atlases[k] = { image, ++useCounter };
    return image;
}

FlowerSprites::Frame FlowerSprites::get (botany::Species s, int colour, int frame)
{
    Frame f;
    auto it = infos.find ((int) s);
    if (it == infos.end())
        return f;
    const auto& info = it->second;
    frame = juce::jlimit (0, (int) info.stems.size() - 1, frame);

    const auto sheet = atlas (s, juce::jlimit (0, numColours - 1, colour));
    if (! sheet.isValid())
        return f;
    f.image = sheet.getClippedImage ({ (frame % atlasColumns) * info.width, (frame / atlasColumns) * info.height, info.width, info.height });
    f.stem = info.stems[(size_t) frame];
    f.head = info.heads[(size_t) frame];
    f.base = f.stem.empty() ? juce::Point<float> ((float) info.width * 0.5f, (float) info.height) : f.stem.front();
    return f;
}

//==============================================================================
void paintFlower (juce::Graphics& g, FlowerSprites& sprites, botany::Species species, int colour,
                  juce::Point<float> base, float height, float lean, float grow, float bloom, float rot,
                  float time, float glow, float alpha)
{
    const float fade = (rot > 0.88f ? 1.0f - (rot - 0.88f) / 0.12f : 1.0f) * alpha;
    if (fade <= 0.01f || grow <= 0.0f)
        return;

    const auto frame = sprites.get (species, colour, sprites.frameFor (grow < 1.0f ? 0.0f : bloom, rot));
    const auto open = sprites.get (species, colour, sprites.bloomFrames() - 1);
    if (! frame.image.isValid() || ! open.image.isValid())
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
        reveal.addRectangle (juce::Rectangle<float> (0.0f, visibleTop, (float) frame.image.getWidth(), (float) frame.image.getHeight() - visibleTop));
        reveal.applyTransform (t);
        g.reduceClipRegion (reveal);
    }

    const auto head = frame.head.transformedBy (t);
    if (glow > 0.01f)
        drawGlow (g, head, height * 0.45f, FlowerSprites::colourway (colour).withAlpha (0.35f * glow * fade));

    g.setOpacity (fade);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageTransformed (frame.image, t);
    g.setOpacity (1.0f);

    if (rot <= 0.15f)
        return;

    // A fresh drop of blood keeps sliding down the stem.
    if (rot < 0.7f && frame.stem.size() >= 2)
    {
        std::vector<juce::Point<float>> stem;
        for (auto& p : frame.stem)
            stem.push_back (p.transformedBy (t));
        const float s = 1.0f - std::fmod (time * 0.22f + (float) colour * 0.13f, 1.0f);
        const auto drop = alongPolyline (stem, s);
        g.setColour (juce::Colour (0xff5a0910).withMultipliedAlpha (fade));
        g.fillEllipse (drop.x - 1.6f, drop.y - 2.2f, 3.2f, 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.45f * fade));
        g.fillEllipse (drop.x - 0.8f, drop.y - 1.6f, 1.0f, 1.0f);
    }

    // Flies drawn to it.
    if (rot > 0.25f && rot < 0.97f)
    {
        const float present = juce::jlimit (0.0f, 1.0f, (rot - 0.25f) / 0.1f) * fade;
        const float r = height * 0.2f;
        for (int f = 0; f < 2; ++f)
        {
            const float a = time * (3.1f + (float) f * 1.7f) + (float) colour + (float) f * 2.0f;
            const auto pos = head + juce::Point<float> (std::cos (a) * r * 1.3f, std::sin (a * 1.3f) * r * 0.8f - r * 0.3f);
            const float flap = 0.5f + 0.5f * std::sin (time * 90.0f + (float) f);
            g.setColour (juce::Colours::white.withAlpha (0.4f * flap * present));
            g.fillEllipse (pos.x - 3.2f, pos.y - 2.6f, 3.0f, 2.0f);
            g.fillEllipse (pos.x + 0.2f, pos.y - 2.6f, 3.0f, 2.0f);
            g.setColour (juce::Colour (0xff141414).withAlpha (present));
            g.fillEllipse (pos.x - 1.6f, pos.y - 1.0f, 3.2f, 2.2f);
        }
    }
}
} // namespace bouquet::ui::real
