#include "Garden.h"
#include "../PluginProcessor.h"
#include "BouquetLookAndFeel.h"

using namespace bouquet::ui::botany;

namespace bouquet::ui
{
namespace
{
    // Chord quality decides the bouquet's palette.
    std::vector<juce::Colour> paletteFor (const std::vector<int>& notes)
    {
        using namespace colours;
        bool third = false, minorThird = false, flatFive = false, seventh = false;
        const int root = notes.front();
        for (int n : notes)
        {
            const int i = ((n - root) % 12 + 12) % 12;
            third |= i == 4;
            minorThird |= i == 3;
            flatFive |= i == 6;
            seventh |= i == 10 || i == 11;
        }

        if (minorThird && flatFive)  return { juce::Colour (0xff9b5b8f), juce::Colour (0xffc76b98), juce::Colour (0xffb59ac9) };
        if (third && seventh)        return { butter, coral, lilac, peach };
        if (third)                   return { butter, peach, coral, juce::Colour (0xfffff4e0) };
        if (minorThird)              return { lilac, sky, rose, juce::Colour (0xff8fa3e8) };
        if (notes.size() == 1)       return { coral, butter, rose, sky, lilac };
        return { mint, sky, juce::Colour (0xfff7e8a0), juce::Colour (0xfffdfdfd) };
    }

    const juce::Colour ribbonColours[] = {
        juce::Colour (0xff6fa8f5), juce::Colour (0xffe8735a), juce::Colour (0xffa58cf0), juce::Colour (0xfff2c14e),
        juce::Colour (0xff5cc9a7), juce::Colour (0xffe4789b), juce::Colour (0xffd9895b), juce::Colour (0xff7d6bd6),
        juce::Colour (0xff6fc3e0), juce::Colour (0xfff4a77c),
    };
}

uint32_t Garden::seedFor (std::vector<int> notes, int chordShape, int instrument, float velocity)
{
    std::sort (notes.begin(), notes.end());
    uint32_t pitchClasses = 0;
    for (int n : notes)
        pitchClasses |= 1u << (((n % 12) + 12) % 12);

    // FNV-1a over everything that makes a chord itself.
    uint32_t h = 2166136261u;
    auto mix = [&h] (uint32_t v) { for (int i = 0; i < 4; ++i) { h ^= (v >> (i * 8)) & 0xffu; h *= 16777619u; } };
    mix (pitchClasses);
    mix ((uint32_t) (notes.empty() ? 0 : notes.front()));
    mix ((uint32_t) notes.size());
    mix ((uint32_t) (chordShape + 1));
    mix ((uint32_t) instrument);
    mix ((uint32_t) juce::jlimit (0, 4, (int) (velocity * 4.0f)));
    return h;
}

Garden::Garden (BouquetProcessor& p, std::function<float (float)> mapNote)
    : processor (p), noteToX (std::move (mapNote))
{
    setInterceptsMouseClicks (false, false);
}

//==============================================================================
void Garden::tick (float dt)
{
    const int n = processor.popNoteEvents (incoming.data(), (int) incoming.size());
    for (int i = 0; i < n; ++i)
    {
        if (incoming[(size_t) i].on) noteOn (incoming[(size_t) i]);
        else                         noteOff (incoming[(size_t) i]);
    }

    listenToLiveInput (dt);

    // A loud granular tail keeps the flowers alive a little longer.
    const float wet = std::min (1.0f, processor.engine.wetLevel.load() * 3.0f);
    for (auto& b : bouquets)
    {
        b.age += dt;
        if (b.released)
            b.rot += dt / 5.0f * (1.0f - 0.6f * wet);
    }

    bouquets.erase (std::remove_if (bouquets.begin(), bouquets.end(), [] (const Bunch& b) { return b.rot >= 1.0f; }),
                    bouquets.end());

    // Breathing: in while something is held or sounding, out slowly after.
    bool holding = false;
    for (auto& b : bouquets)
        holding |= ! b.released;
    const bool sounding = holding || processor.sourceMeter.load() > 0.03f;
    const float target = sounding ? 1.0f : 0.0f;
    breath += (target - breath) * (1.0f - std::exp (-dt / (sounding ? 0.7f : 1.8f)));

    // Grass under held chords stays up; everywhere else it slowly recedes.
    for (auto& b : bouquets)
        if (! b.released)
            for (int held : b.notes)
                growGrass (held, 1.0f);
    for (auto& p : patches)
    {
        p.target = std::max (0.0f, p.target - dt / 16.0f);
        const float speed = p.target > p.level ? 1.4f : 0.25f;   // springs up, withers slowly
        p.level += (p.target - p.level) * (1.0f - std::exp (-dt * speed * 2.0f));
    }
    patches.erase (std::remove_if (patches.begin(), patches.end(),
                                   [] (const Patch& p) { return p.target <= 0.0f && p.level < 0.01f; }), patches.end());

    for (auto& r : ripples)
        r.age += dt;
    ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (const Ripple& r) { return r.age > 4.0f; }), ripples.end());
}

void Garden::resized()
{
    grass.generate ((float) getWidth(), 58.0f, 0x6a55u, 2.6f);
}

float Garden::keyWidth() const
{
    return std::max (6.0f, std::abs (noteToX (61.0f) - noteToX (60.0f)) * 1.4f);
}

void Garden::growGrass (int note, float strength)
{
    // The key itself and its neighbours, less and less.
    for (int k = -2; k <= 2; ++k)
    {
        const float x = noteToX ((float) (note + k));
        const float s = strength * (k == 0 ? 1.0f : (std::abs (k) == 1 ? 0.7f : 0.4f));
        auto it = std::find_if (patches.begin(), patches.end(), [&] (const Patch& p) { return std::abs (p.x - x) < keyWidth() * 0.45f; });
        if (it == patches.end())
            patches.push_back ({ x, 0.0f, s, (uint32_t) (note + k) * 2654435761u });
        else
            it->target = std::max (it->target, s);
    }
}

float Garden::grassCoverage (float x) const
{
    const float w = keyWidth() * 0.9f;
    float c = 0.0f;
    for (auto& p : patches)
    {
        const float d = (x - p.x) / w;
        c = std::max (c, p.level * std::exp (-d * d));
    }
    return c;
}

void Garden::paintRealGrass (juce::Graphics& g, float time, bool front) const
{
    const int tufts = sprites->grassCount();
    if (tufts == 0)
        return;
    const float baseY = (float) getHeight() - 1.0f;
    const float wind = 0.6f + 0.4f * std::sin (time * 0.37f) * std::sin (time * 0.23f + 1.0f);
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);

    for (auto& p : patches)
    {
        if (p.level < 0.02f)
            continue;
        juce::Random r ((juce::int64) p.seed);
        for (int i = 0; i < 3; ++i)
        {
            const bool inFront = i == 1;
            const float x = p.x + (r.nextFloat() - 0.5f) * keyWidth() * 1.2f;
            const float height = (inFront ? 30.0f : 44.0f) * (0.8f + 0.4f * r.nextFloat()) * p.level * (1.0f + 0.15f * breath);
            const auto image = sprites->grass (r.nextInt (tufts));
            if (inFront != front || ! image.isValid())
                continue;

            // Wind and the ripples of each new chord shear the tuft.
            const float gust = processor.modWheel.load();
            float sway = std::sin (time * (1.6f + 2.0f * gust) + x * 0.015f + (float) i) * (0.12f + 0.3f * gust) * wind * (1.0f - 0.5f * breath);
            for (const auto& rp : ripples)
            {
                const float dist = std::abs (x - rp.x);
                sway += rp.strength * std::exp (-rp.age * 1.3f) * std::sin (dist * 0.045f - rp.age * 7.0f) * std::exp (-dist / 260.0f) * 0.3f;
            }
            const float scale = height / (sprites->grassGround() * 0.85f);
            const float groundY = sprites->grassGround();
            g.setOpacity (juce::jlimit (0.0f, 1.0f, p.level * 2.0f));
            g.drawImageTransformed (image, juce::AffineTransform::translation (-(float) image.getWidth() * 0.5f, -groundY)
                                               .sheared (-sway, 0.0f)
                                               .scaled (scale)
                                               .translated (x, baseY));
            g.setOpacity (1.0f);
        }
    }
}

void Garden::noteOn (const NoteEvent& e)
{
    const int note = juce::roundToInt (e.note);
    const float strumMs = processor.apvts.getRawParameterValue (ids::strum)->load();
    const float window = 0.09f + strumMs * 0.006f;

    // Notes landing together (a chord, a strum) join the newest bouquet.
    if (! bouquets.empty())
    {
        auto& last = bouquets.back();
        if (! last.released && ! last.fromInput && last.age < window)
        {
            if (std::find (last.notes.begin(), last.notes.end(), note) == last.notes.end())
                last.notes.push_back (note);
            growGrass (note, 0.6f + 0.4f * e.velocity);
            last.held.push_back (note);
            last.velocity = std::max (last.velocity, e.velocity);
            arrange (last);
            return;
        }
    }

    Bunch b;
    b.notes = { note };
    b.held = { note };
    b.chordShape = e.chordShape;
    b.instrument = e.instrument;
    b.velocity = e.velocity;
    arrange (b);
    ripples.push_back ({ b.x, 0.0f, 0.6f + 0.6f * b.velocity });
    growGrass (note, 0.6f + 0.4f * e.velocity);
    bouquets.push_back (std::move (b));

    // Keep the garden from overgrowing.
    int alive = 0;
    for (auto& x : bouquets)
        alive += x.released ? 0 : 1;
    for (auto& x : bouquets)
        if (alive > 6 && ! x.released) { releaseBunch (x); --alive; }
    while (bouquets.size() > 10)
        bouquets.erase (bouquets.begin());
}

void Garden::noteOff (const NoteEvent& e)
{
    const int note = juce::roundToInt (e.note);
    for (auto it = bouquets.rbegin(); it != bouquets.rend(); ++it)
    {
        auto found = std::find (it->held.begin(), it->held.end(), note);
        if (found == it->held.end())
            continue;
        it->held.erase (found);
        if (it->held.empty())
            releaseBunch (*it);
        return;
    }
}

void Garden::releaseBunch (Bunch& b)
{
    b.released = true;
    b.held.clear();
}

void Garden::listenToLiveInput (float dt)
{
    const bool live = processor.apvts.getRawParameterValue (ids::instrument)->load() < 0.5f;
    const float level = processor.sourceMeter.load();

    if (! live || level < 0.02f)
    {
        quietFor += dt;
        loudFor = 0.0f;
        if (quietFor > 1.5f)
            for (auto& b : bouquets)
                if (b.fromInput && ! b.released)
                    releaseBunch (b);
        return;
    }

    loudFor += dt;
    if (level > 0.06f && quietFor > 0.35f)
    {
        // A swell after silence: plant something from a pentatonic field.
        Bunch b;
        b.fromInput = true;
        const int root = 48 + random.nextInt (16);
        const int shape[] = { 0, 2, 4, 7, 9, 12 };
        const int count = 2 + random.nextInt (3);
        for (int i = 0; i < count; ++i)
            b.notes.push_back (root + shape[random.nextInt (6)]);
        std::sort (b.notes.begin(), b.notes.end());
        b.notes.erase (std::unique (b.notes.begin(), b.notes.end()), b.notes.end());
        b.chordShape = -1;
        b.velocity = juce::jlimit (0.3f, 1.0f, level * 3.0f);
        arrange (b);
        bouquets.push_back (std::move (b));
        while (bouquets.size() > 10)
            bouquets.erase (bouquets.begin());
    }
    quietFor = 0.0f;
}

//==============================================================================
void Garden::arrange (Bunch& b)
{
    std::sort (b.notes.begin(), b.notes.end());
    b.seed = seedFor (b.notes, b.chordShape, b.instrument, b.velocity);
    juce::Random r ((juce::int64) b.seed);

    const float width = (float) getWidth(), maxHeight = (float) getHeight() - 22.0f;
    b.x = juce::jlimit (60.0f, std::max (60.0f, width - 60.0f), noteToX ((float) b.notes.front()) + r.nextFloat() * 10.0f - 5.0f);
    b.ribbon = ribbonColours[(size_t) juce::jlimit (0, 9, b.instrument)];

    const float attack = processor.apvts.getRawParameterValue (ids::attack)->load();
    b.growSeconds = juce::jlimit (0.5f, 2.6f, 1.15f - 0.5f * b.velocity + attack * 0.4f);

    // Up to six flowers: spread the chord tones if there are more.
    std::vector<int> tones = b.notes;
    while (tones.size() > 6)
        tones.erase (tones.begin() + (long) tones.size() / 2);

    const auto palette = paletteFor (b.notes);
    const auto dominant = (Species) r.nextInt ((int) Species::count);
    const int count = (int) tones.size();
    const float mid = (float) (count - 1) * 0.5f;
    const float hueShift = ((float) (b.notes.front() % 12) - 6.0f) / 6.0f * 0.035f;

    b.flowers.clear();
    for (int i = 0; i < count; ++i)
    {
        Flower f;
        f.note = (float) tones[(size_t) i];
        const float pitch = juce::jlimit (0.0f, 1.0f, (f.note - 36.0f) / 60.0f);
        const float centreness = 1.0f - std::abs ((float) i - mid) / std::max (1.0f, mid + 0.5f);
        f.height = maxHeight * juce::jlimit (0.35f, 1.0f, 0.42f + 0.32f * pitch + 0.2f * centreness + 0.06f * r.nextFloat());
        f.spread = ((float) i - mid) * (17.0f + 7.0f * r.nextFloat()) + (r.nextFloat() - 0.5f) * 6.0f;

        auto c = palette[(size_t) ((i + r.nextInt (3)) % (int) palette.size())];
        c = juce::Colour::fromHSV (std::fmod (c.getHue() + hueShift + 1.0f, 1.0f), c.getSaturation(),
                                   c.getBrightness() * (0.95f + 0.05f * r.nextFloat()), 1.0f);
        const auto species = r.nextFloat() < 0.6f ? dominant : (Species) r.nextInt ((int) Species::count);
        f.genome = makeGenome (species, c, 10.5f + 6.0f * b.velocity + 3.5f * r.nextFloat(), b.seed + (uint32_t) i * 7919u);
        b.flowers.push_back (std::move (f));
    }
}

//==============================================================================
void Garden::paint (juce::Graphics& g)
{
    const float time = (float) clock();
    const float sway = processor.engine.breath.load();
    const juce::Point<float> ground (0.0f, (float) getHeight() - 1.0f);

    // Soil, then the grass behind the flowers, the bouquets, and the grass in front.
    juce::ColourGradient soil (juce::Colour (0x00000000), 0.0f, ground.y - 18.0f,
                               juce::Colour (0xff4e5e33).withAlpha (0.35f), 0.0f, ground.y, false);
    g.setGradientFill (soil);
    g.fillRect (0.0f, ground.y - 18.0f, (float) getWidth(), 19.0f);

    const bool photoreal = processor.photorealFlowers() && sprites->available() && sprites->grassCount() > 0;
    const auto coverage = [this] (float x) { return grassCoverage (x); };
    if (photoreal) paintRealGrass (g, time, false);
    else           grass.paint (g, ground, time, breath, ripples, false, coverage, processor.modWheel.load());
    for (const auto& b : bouquets)
        paintBunch (g, b, time, sway);
    if (photoreal) paintRealGrass (g, time, true);
    else           grass.paint (g, ground, time, breath, ripples, true, coverage, processor.modWheel.load());
}

void Garden::paintBunch (juce::Graphics& g, const Bunch& b, float time, float sway) const
{
    const float baseY = (float) getHeight() - 3.0f;
    const juce::Point<float> tie (b.x, baseY - 16.0f);
    const float grow = 1.0f - std::pow (1.0f - juce::jlimit (0.0f, 1.0f, b.age / b.growSeconds), 3.0f);
    const float bloomT = juce::jlimit (0.0f, 1.0f, (b.age - 0.55f * b.growSeconds) / (0.9f * b.growSeconds));
    const float bloom = bloomT * bloomT * (3.0f - 2.0f * bloomT);
    const float rot = b.rot;
    // Expression: pressing into held keys lifts and lights the bouquet; the mod wheel is wind.
    const float press = b.released ? 0.0f : processor.keyPressure.load();
    const float gust = processor.modWheel.load();

    for (const auto& f : b.flowers)
    {
        // The head sways, lifts with the breath, and bows over as it rots.
        const float lean = f.spread >= 0.0f ? 1.0f : -1.0f;
        const float breeze = (std::sin (time * (0.9f + 1.6f * gust) + f.genome.phase) * 3.0f * (1.0f + 2.5f * gust) + sway * 2.5f) * (1.0f - rot);
        juce::Point<float> head (b.x + f.spread * 2.1f + breeze, baseY - f.height - (4.0f * breath + 10.0f * press) * (1.0f - rot));
        const float droop = std::pow (rot, 1.2f) * 1.45f * lean;
        auto rotate = [&tie] (juce::Point<float> p, float a)
        {
            const auto d = p - tie;
            return tie + juce::Point<float> (d.x * std::cos (a) - d.y * std::sin (a), d.x * std::sin (a) + d.y * std::cos (a));
        };
        head = rotate (head, droop);

        Pose pose;
        pose.base = { b.x + f.spread * 0.1f, baseY };
        pose.c1 = { b.x + f.spread * 0.2f, baseY - 6.0f };
        pose.c2 = rotate ({ b.x + f.spread * 1.4f + breeze * 0.4f, baseY - f.height * 0.55f }, droop * 0.6f);
        pose.head = head;
        pose.grow = grow;
        pose.bloom = std::max (bloom, 0.12f * juce::jlimit (0.0f, 1.0f, (grow - 0.65f) / 0.35f));
        pose.rot = rot;
        pose.time = time;
        pose.stemWidth = 2.4f;
        pose.glow = press;
        if (processor.photorealFlowers() && sprites->available())
        {
            // Rendered flowers carry their own droop; we only fan them out and sway them.
            const float fan = f.spread * 0.011f + breeze * 0.008f;
            real::paintFlower (g, *sprites, f.genome.species, (int) (f.genome.seed % real::FlowerSprites::numVariants),
                               real::FlowerSprites::nearestColour (f.genome.petal),
                               pose.base, f.height * 0.9f * (1.0f + 0.06f * press), fan, grow, pose.bloom, rot, time, press);
        }
        else
        {
            paintPlant (g, f.genome, pose);
        }
    }

    // Twine and a ribbon holding the stems together.
    if (grow > 0.25f)
    {
        const float s = juce::jlimit (0.0f, 1.0f, (grow - 0.25f) / 0.4f);
        const float fade = rot > 0.85f ? 1.0f - (rot - 0.85f) / 0.15f : 1.0f;
        const auto ribbon = b.ribbon.interpolatedWith (juce::Colour (0xff5a0910), juce::jlimit (0.0f, 1.0f, rot * 1.2f) * 0.7f)
                                .withMultipliedAlpha (fade);
        juce::Path bow;
        bow.addEllipse (-9.0f * s, -3.5f * s, 9.0f * s, 7.0f * s);
        bow.addEllipse (0.0f, -3.5f * s, 9.0f * s, 7.0f * s);
        bow.startNewSubPath (-1.0f, 1.0f);
        bow.lineTo (-6.0f * s, 9.0f * s);
        bow.startNewSubPath (1.0f, 1.0f);
        bow.lineTo (5.0f * s, 10.0f * s + rot * 6.0f);
        bow.applyTransform (juce::AffineTransform::rotation (std::sin (time * 0.7f + (float) (b.seed % 100)) * 0.05f + rot * 0.4f).translated (tie));
        g.setColour (ribbon);
        g.strokePath (bow, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (ribbon.darker (0.35f));
        g.fillEllipse (juce::Rectangle<float> (3.4f, 3.4f).withCentre (tie));
    }
}
} // namespace bouquet::ui
