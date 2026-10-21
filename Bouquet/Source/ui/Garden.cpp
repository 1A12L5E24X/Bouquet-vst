#include "Garden.h"
#include "../PluginProcessor.h"
#include "BouquetLookAndFeel.h"
#include "../dsp/Weather.h"

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
        juce::Colour (0xff6fc3e0), juce::Colour (0xfff4a77c), juce::Colour (0xff8a9a5b), juce::Colour (0xffc9a24a),
        juce::Colour (0xff9ad0e8), juce::Colour (0xffd4a373), juce::Colour (0xff6fb7a0), juce::Colour (0xffe6b8d4),
        juce::Colour (0xffb08968), juce::Colour (0xff7ec4d9), juce::Colour (0xfff2d06b),
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
    const float press = processor.keyPressure.load();
    for (auto& b : bouquets)
    {
        b.age += dt;
        if (b.released)
            b.rot += dt / 5.0f * (1.0f - 0.6f * wet);
        else if (b.formed)
            b.heldFor += dt;      // it keeps growing and opening for as long as the key is held

        // A bouquet starts growing once its flowers can be drawn, so it rises whole from the soil
        // rather than being revealed bit by bit.
        if (! b.formed)
        {
            if (b.age < 0.5f && ! ready (b))
                continue;
            b.formed = true;
            b.open = juce::jlimit (0.0f, 1.0f, 0.3f + 0.4f * b.velocity);
        }
        b.growthTime += dt;
        b.growth = std::min (1.0f, b.growthTime / b.growSeconds);
        mold (b, dt, press);
    }

    bouquets.erase (std::remove_if (bouquets.begin(), bouquets.end(), [] (const Bunch& b) { return b.rot >= 1.0f; }),
                    bouquets.end());
    tickWeather (dt);

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
            last.stretchSpeed -= 2.5f * e.velocity;   // each note of the strum presses it down again
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

bool Garden::ready (const Bunch& b)
{
    if (! processor.photorealFlowers())
        return true;
    for (const auto& f : b.flowers)
        if (sprites->has (f.genome.species)
            && ! sprites->ready (f.genome.species, (int) (f.genome.seed % real::FlowerSprites::numVariants),
                                 real::FlowerSprites::nearestColour (f.genome.petal)))
            return false;
    return true;
}

void Garden::mold (Bunch& b, float dt, float press)
{
    // Touch opens the flowers: a harder strike further, pressing in further still.
    const float touch = b.released ? 0.0f : press;
    const float openTarget = b.released ? 1.0f : juce::jlimit (0.0f, 1.0f, 0.4f + 0.45f * b.velocity + 0.5f * touch
                                                                         + 0.2f * (1.0f - std::exp (-b.heldFor / 2.5f)));
    b.open += (openTarget - b.open) * (1.0f - std::exp (-dt * 5.0f));

    // Squash and stretch on a soft spring, like clay: pressing in draws the bouquet up.
    const float rest = 0.18f * touch;
    const float damping = 2.0f * 0.32f * std::sqrt (b.stiffness);
    const float h = std::min (dt, 0.1f) / 4.0f;
    for (int i = 0; i < 4; ++i)
    {
        b.stretchSpeed += (-b.stiffness * (b.stretch - rest) - damping * b.stretchSpeed) * h;
        b.stretch += b.stretchSpeed * h;
    }
    b.stretch = juce::jlimit (-0.7f, 0.6f, b.stretch);
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

    const float width = (float) getWidth(), maxHeight = (float) (getHeight() - headroom) - 22.0f;
    b.ribbon = ribbonColours[(size_t) juce::jlimit (0, (int) std::size (ribbonColours) - 1, b.instrument)];

    const float attack = processor.apvts.getRawParameterValue (ids::attack)->load();
    b.stiffness = 260.0f / (1.0f + 1.5f * attack);   // a slow attack settles slowly
    b.growSeconds = juce::jlimit (0.4f, 2.4f, 0.95f - 0.45f * b.velocity + attack * 0.4f);

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
    float centre = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        Flower f;
        f.note = (float) tones[(size_t) i];
        const float pitch = juce::jlimit (0.0f, 1.0f, (f.note - 36.0f) / 60.0f);
        const float centreness = 1.0f - std::abs ((float) i - mid) / std::max (1.0f, mid + 0.5f);
        f.height = maxHeight * juce::jlimit (0.35f, 1.0f, 0.42f + 0.32f * pitch + 0.2f * centreness + 0.06f * r.nextFloat());

        // Each flower grows out of its own key, a hair after the one before it.
        f.keyX = juce::jlimit (18.0f, std::max (18.0f, width - 18.0f), noteToX (f.note) + r.nextFloat() * 6.0f - 3.0f);
        f.delay = 0.05f * (float) i;
        centre += f.keyX;

        // The higher the note, the brighter the flower: deep and rich in the bass, pale and
        // luminous at the top. The weather follows from the same pitch.
        f.bright = brightnessForNote (f.note);
        f.rain = rainForNote (f.note);
        f.light = lightForNote (f.note);

        auto c = palette[(size_t) ((i + r.nextInt (3)) % (int) palette.size())];
        c = juce::Colour::fromHSV (std::fmod (c.getHue() + hueShift + 1.0f, 1.0f),
                                   juce::jlimit (0.0f, 1.0f, c.getSaturation() * (1.12f - 0.42f * f.bright)),
                                   juce::jlimit (0.0f, 1.0f, c.getBrightness() * (0.6f + 0.5f * f.bright) * (0.95f + 0.05f * r.nextFloat())),
                                   1.0f);
        const auto species = r.nextFloat() < 0.6f ? dominant : (Species) r.nextInt ((int) Species::count);
        f.genome = makeGenome (species, c, 10.5f + 6.0f * b.velocity + 3.5f * r.nextFloat(), b.seed + (uint32_t) i * 7919u);
        if (processor.photorealFlowers())
            sprites->prefetch (species, (int) (f.genome.seed % real::FlowerSprites::numVariants), real::FlowerSprites::nearestColour (c));
        b.flowers.push_back (std::move (f));
    }

    // The bouquet is gathered around the middle of its flowers.
    b.x = juce::jlimit (18.0f, std::max (18.0f, width - 18.0f), centre / (float) std::max (1, count));
    for (auto& f : b.flowers)
        f.spread = f.keyX - b.x;
}

//==============================================================================
Garden::Placement Garden::place (const Bunch& b, const Flower& f, float time, float sway) const
{
    Placement p;
    const float baseY = (float) getHeight() - 3.0f;
    const float rot = b.rot;
    const float press = b.released ? 0.0f : processor.keyPressure.load();
    const float gust = processor.modWheel.load();
    // Squashed it grows wide, drawn up it grows thin, as clay keeps its volume.
    const float tall = 1.0f + b.stretch;
    p.wide = 1.0f / std::sqrt (tall);
    const float bend = gust * 0.3f * (1.0f - rot);

    // Growing: the plant rises from the soil, overshooting a touch and settling like soft
    // clay, its buds opening as it gets there. A key held down keeps it growing a while longer.
    const float gi = juce::jlimit (0.0f, 1.0f, (b.growthTime - f.delay) / b.growSeconds);
    const float t = gi - 1.0f;
    const float rise = 1.0f + 2.2f * t * t * t + 1.2f * t * t;
    p.risen = 0.08f + 0.92f * rise;
    const float fanned = 0.45f + 0.55f * juce::jlimit (0.0f, 1.0f, rise);
    const float budT = juce::jlimit (0.0f, 1.0f, (gi - 0.45f) / 0.55f);
    p.opening = budT * budT * (3.0f - 2.0f * budT);
    p.creep = 1.0f + 0.16f * (1.0f - std::exp (-b.heldFor / 3.5f));
    p.breeze = (std::sin (time * (0.9f + 1.6f * gust) + f.genome.phase) * 3.0f * (1.0f + 2.5f * gust) + sway * 2.5f) * (1.0f - rot);

    const bool sprite = processor.photorealFlowers() && sprites->has (f.genome.species);
    const float height = f.height * tall * p.risen * p.creep * (sprite ? 0.9f : 1.0f);

    // The stem rises from its own key and leans in towards the middle of the bouquet.
    const float headX = b.x + f.spread * 0.6f * fanned * p.wide + p.breeze + bend * height;
    p.base = { f.keyX, baseY };
    p.head = { headX, baseY - height - (4.0f * breath + 10.0f * press) * (1.0f - rot) };
    p.lean = std::atan2 (p.head.x - p.base.x, p.base.y - p.head.y);
    p.c1 = { f.keyX + (b.x - f.keyX) * 0.1f, baseY - 6.0f * p.risen };
    p.c2 = { f.keyX + (headX - f.keyX) * 0.3f + p.breeze * 0.4f + bend * height * 0.3f, baseY - height * 0.55f };

    // As it rots the head bows over from its own base.
    p.droop = std::pow (rot, 1.2f) * 1.45f * (f.spread >= 0.0f ? 1.0f : -1.0f);
    auto rotate = [&p] (juce::Point<float> q, float a)
    {
        const auto d = q - p.base;
        return p.base + juce::Point<float> (d.x * std::cos (a) - d.y * std::sin (a), d.x * std::sin (a) + d.y * std::cos (a));
    };
    p.head = rotate (p.head, p.droop);
    p.c2 = rotate (p.c2, p.droop * 0.6f);

    p.weather = juce::jlimit (0.0f, 1.0f, (gi - 0.35f) / 0.4f) * (1.0f - juce::jlimit (0.0f, 1.0f, rot * 1.25f)) * (b.released ? 0.6f : 1.0f);
    return p;
}

void Garden::tickWeather (float dt)
{
    const float time = (float) clock();
    const float gust = processor.modWheel.load();

    for (auto& b : bouquets)
    {
        if (! b.formed)
            continue;
        for (auto& f : b.flowers)
        {
            if (f.rain < 0.04f && f.light < 0.04f)
                continue;
            const auto where = place (b, f, time, 0.0f);
            if (where.weather < 0.03f)
                continue;

            // Rain gathers above the flower and falls onto its head.
            if (f.rain > 0.04f && where.head.y > 22.0f)
            {
                f.rainDue = std::min (f.rainDue + f.rain * where.weather * 80.0f * dt, 6.0f);
                for (; f.rainDue >= 1.0f && drops.size() < 500; f.rainDue -= 1.0f)
                {
                    const float top = 3.0f + 5.0f * random.nextFloat();
                    drops.push_back ({ { where.head.x + (random.nextFloat() - 0.5f) * (34.0f + 30.0f * f.rain), top },
                                       330.0f + 130.0f * random.nextFloat(), 7.0f + 9.0f * random.nextFloat(),
                                       std::max (top + 8.0f, where.head.y - 2.0f + 12.0f * random.nextFloat()),
                                       0.65f + 0.3f * f.rain });
                }
            }

            // Light: dust turns gold as it drifts through the beam.
            if (f.light > 0.04f)
            {
                f.moteDue = std::min (f.moteDue + f.light * where.weather * 8.0f * dt, 4.0f);
                for (; f.moteDue >= 1.0f && motes.size() < 160; f.moteDue -= 1.0f)
                {
                    const float along = random.nextFloat();
                    const float sunX = where.head.x + 46.0f;
                    motes.push_back ({ { sunX + (where.head.x - sunX) * along + (random.nextFloat() - 0.5f) * (8.0f + 50.0f * along),
                                         where.head.y * along },
                                       { (random.nextFloat() - 0.5f) * 8.0f, -3.0f - 9.0f * random.nextFloat() },
                                       1.2f + 1.4f * random.nextFloat(), 1.1f + 1.6f * random.nextFloat(), random.nextFloat() * 6.28f });
                }
            }
        }
    }

    for (auto& d : drops)
    {
        d.pos.y += d.speed * dt;
        d.pos.x += gust * 40.0f * dt;
        if (d.pos.y >= d.landY)
        {
            splashes.push_back ({ { d.pos.x, d.landY }, 0.0f });
            d.alpha = 0.0f;
        }
    }
    drops.erase (std::remove_if (drops.begin(), drops.end(), [] (const Drop& d) { return d.alpha <= 0.0f; }), drops.end());

    for (auto& s : splashes)
        s.age += dt;
    splashes.erase (std::remove_if (splashes.begin(), splashes.end(), [] (const Splash& s) { return s.age > 0.4f; }), splashes.end());
    if (splashes.size() > 200)
        splashes.erase (splashes.begin(), splashes.begin() + (long) (splashes.size() - 200));

    for (auto& m : motes)
    {
        m.pos += m.vel * dt;
        m.life -= dt;
    }
    motes.erase (std::remove_if (motes.begin(), motes.end(), [] (const Mote& m) { return m.life <= 0.0f; }), motes.end());
}

void Garden::paintSky (juce::Graphics& g, float time) const
{
    const juce::Colour mist (0xff8196ad), warm (0xffffe9a8);
    for (const auto& b : bouquets)
    {
        if (! b.formed)
            continue;
        for (const auto& f : b.flowers)
        {
            if (f.rain < 0.04f && f.light < 0.04f)
                continue;
            const auto where = place (b, f, time, 0.0f);
            if (where.weather < 0.03f)
                continue;

            // A small cloud of mist hangs over a flower the rain is about to fall on.
            if (f.rain > 0.04f)
            {
                const float a = f.rain * where.weather;
                for (int i = 0; i < 4; ++i)
                {
                    const float ox = ((float) i - 1.5f) * 15.0f + std::sin (time * 0.4f + (float) i * 1.7f) * 4.0f;
                    drawGlow (g, { where.head.x + ox, 8.0f + (float) (i % 2) * 5.0f }, 30.0f + 10.0f * f.rain, mist.withAlpha (0.62f * a));
                }
            }

            // A shaft of light from the sun, up and to the right, falling on a high flower.
            if (f.light > 0.04f)
            {
                const float a = f.light * where.weather;
                const float sunX = where.head.x + 46.0f;
                const float lowY = where.head.y + 22.0f;
                juce::Path shaft;
                shaft.startNewSubPath (sunX - 5.0f, -4.0f);
                shaft.lineTo (sunX + 7.0f, -4.0f);
                shaft.lineTo (where.head.x + 36.0f, lowY);
                shaft.lineTo (where.head.x - 32.0f, lowY);
                shaft.closeSubPath();
                juce::ColourGradient beam (warm.withAlpha (0.0f), sunX, 0.0f, warm.withAlpha (0.0f), where.head.x, lowY, false);
                beam.addColour (0.3, warm.withAlpha (0.22f * a));
                beam.addColour (0.8, warm.withAlpha (0.62f * a));
                g.setGradientFill (beam);
                g.fillPath (shaft);

                // A second, slimmer ray breathes beside it.
                const float breathe = 0.6f + 0.4f * std::sin (time * 0.9f + f.genome.phase);
                juce::Path ray;
                ray.startNewSubPath (sunX + 2.0f, -4.0f);
                ray.lineTo (sunX + 9.0f, -4.0f);
                ray.lineTo (where.head.x + 14.0f, lowY);
                ray.lineTo (where.head.x - 4.0f, lowY);
                ray.closeSubPath();
                juce::ColourGradient slim (warm.withAlpha (0.0f), sunX, 0.0f, warm.withAlpha (0.0f), where.head.x, lowY, false);
                slim.addColour (0.8, warm.withAlpha (0.5f * a * breathe));
                g.setGradientFill (slim);
                g.fillPath (ray);
            }
        }
    }
}

void Garden::paintWeather (juce::Graphics& g, float time) const
{
    const juce::Colour water (0xffeaf6ff);
    for (const auto& d : drops)
    {
        const float fadeIn = juce::jlimit (0.0f, 1.0f, d.pos.y / 18.0f);
        g.setColour (water.withAlpha (d.alpha * fadeIn));
        g.drawLine (d.pos.x - 1.5f, d.pos.y - d.length * 1.3f, d.pos.x, d.pos.y, 1.5f);
    }
    for (const auto& s : splashes)
    {
        const float u = s.age / 0.4f;
        const float radius = 2.0f + 9.0f * u;
        g.setColour (water.withAlpha (0.55f * (1.0f - u)));
        g.drawEllipse (juce::Rectangle<float> (radius * 2.0f, radius * 0.9f).withCentre (s.at), 0.9f);
        const float jump = std::sin (u * juce::MathConstants<float>::pi) * 6.0f;
        g.fillEllipse (juce::Rectangle<float> (1.6f, 1.6f).withCentre (s.at + juce::Point<float> (-radius * 0.5f, -jump)));
        g.fillEllipse (juce::Rectangle<float> (1.6f, 1.6f).withCentre (s.at + juce::Point<float> (radius * 0.5f, -jump * 0.8f)));
    }
    for (const auto& m : motes)
    {
        const float twinkle = 0.5f + 0.5f * std::sin (time * 9.0f + m.phase);
        const float a = 0.8f * twinkle * juce::jlimit (0.0f, 1.0f, m.life / 0.4f);
        drawGlow (g, m.pos, m.size * 3.0f, juce::Colour (0xfffff1c2).withAlpha (0.5f * a));
        g.setColour (juce::Colours::white.withAlpha (a));
        g.drawLine (m.pos.x - m.size, m.pos.y, m.pos.x + m.size, m.pos.y, 0.8f);
        g.drawLine (m.pos.x, m.pos.y - m.size, m.pos.x, m.pos.y + m.size, 0.8f);
    }
}

void Garden::paint (juce::Graphics& g)
{
    const float time = (float) clock();
    const float sway = processor.engine.breath.load();
    const juce::Point<float> ground (0.0f, (float) getHeight() - 1.0f);

    // Soil, then the grass behind the flowers, the weather behind them, the bouquets, the
    // weather in front, and the grass in front of all.
    juce::ColourGradient soil (juce::Colour (0x00000000), 0.0f, ground.y - 18.0f,
                               juce::Colour (0xff4e5e33).withAlpha (0.35f), 0.0f, ground.y, false);
    g.setGradientFill (soil);
    g.fillRect (0.0f, ground.y - 18.0f, (float) getWidth(), 19.0f);

    const bool photoreal = processor.photorealFlowers() && sprites->available() && sprites->grassCount() > 0;
    const auto coverage = [this] (float x) { return grassCoverage (x); };
    if (photoreal) paintRealGrass (g, time, false);
    else           grass.paint (g, ground, time, breath, ripples, false, coverage, processor.modWheel.load());
    paintSky (g, time);
    for (const auto& b : bouquets)
        paintBunch (g, b, time, sway);
    paintWeather (g, time);
    if (photoreal) paintRealGrass (g, time, true);
    else           grass.paint (g, ground, time, breath, ripples, true, coverage, processor.modWheel.load());
}

// A seed that has just been pressed in: a green shoot at once, before its flower is ready.
void Garden::paintSprouts (juce::Graphics& g, const Bunch& b) const
{
    const float baseY = (float) getHeight() - 3.0f;
    const float grown = juce::jlimit (0.0f, 1.0f, b.age / 0.45f);
    const float h = 3.0f + 12.0f * (1.0f - (1.0f - grown) * (1.0f - grown));
    for (const auto& f : b.flowers)
    {
        const float x = f.keyX;
        juce::Path shoot;
        shoot.startNewSubPath (x, baseY);
        shoot.quadraticTo (x - 1.5f, baseY - h * 0.5f, x + 1.5f, baseY - h);
        g.setColour (juce::Colour (0xff6f9f4a));
        g.strokePath (shoot, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff98c46a));
        g.fillEllipse (juce::Rectangle<float> (4.0f * grown, 2.4f * grown).withCentre ({ x - 2.0f, baseY - h + 1.0f }));
        g.fillEllipse (juce::Rectangle<float> (4.0f * grown, 2.4f * grown).withCentre ({ x + 4.0f, baseY - h - 0.5f }));
    }
}

void Garden::paintBunch (juce::Graphics& g, const Bunch& b, float time, float sway) const
{
    if (! b.formed)
    {
        paintSprouts (g, b);
        return;
    }

    const float baseY = (float) getHeight() - 3.0f;
    const float rot = b.rot;
    // Expression: pressing into held keys lifts and lights the bouquet.
    const float press = b.released ? 0.0f : processor.keyPressure.load();
    const juce::Colour warm (0xfffff1c2);

    for (const auto& f : b.flowers)
    {
        const auto where = place (b, f, time, sway);
        const float gi = juce::jlimit (0.0f, 1.0f, (b.growthTime - f.delay) / b.growSeconds);
        if (gi <= 0.0f)
            continue;

        // Pitch makes light: deep and dim in the bass, brightening to a glow at the top.
        const float shade = (f.bright >= 0.4f ? (f.bright - 0.4f) / 0.6f : (f.bright - 0.4f) / 0.4f) * (1.0f - rot);
        const float shine = std::max (0.0f, shade);
        const float glow = std::max (press, 0.55f * shine * shine);

        // Where light falls, the flower catches it.
        if (f.light > 0.04f && where.weather > 0.03f)
            drawGlow (g, where.head, 34.0f + 36.0f * f.light, warm.withAlpha (0.42f * f.light * where.weather));
        else if (shine > 0.3f)
            drawGlow (g, where.head, 26.0f + 22.0f * shine, warm.withAlpha (0.2f * shine));

        Pose pose;
        pose.base = where.base;
        pose.c1 = where.c1;
        pose.c2 = where.c2;
        pose.head = where.head;
        pose.grow = 1.0f;
        pose.bloom = b.open * where.opening;
        pose.rot = rot;
        pose.time = time;
        pose.stemWidth = 2.4f * where.wide;
        pose.glow = glow;
        if (processor.photorealFlowers() && sprites->has (f.genome.species))
        {
            // Rendered flowers carry their own droop; we only lean them, sway them and mold them.
            real::paintFlower (g, *sprites, f.genome.species, (int) (f.genome.seed % real::FlowerSprites::numVariants),
                               real::FlowerSprites::nearestColour (f.genome.petal),
                               where.base, f.height * where.risen * where.creep * 0.9f * (1.0f + 0.06f * press), where.lean, 1.0f,
                               pose.bloom, rot, time, glow, 1.0f, b.stretch, shade);
        }
        else
        {
            paintPlant (g, f.genome, pose);
        }

        // The brightest flowers throw sparks.
        if (shine > 0.55f && where.weather > 0.03f)
        {
            for (int k = 0; k < 2; ++k)
            {
                const float a = time * (0.8f + 0.3f * (float) k) + (float) k * juce::MathConstants<float>::pi + f.genome.phase;
                const auto at = where.head + juce::Point<float> (std::cos (a) * (14.0f + 6.0f * (float) k), std::sin (a * 1.3f) * 10.0f - 6.0f);
                const float tw = 0.5f + 0.5f * std::sin (time * 7.0f + (float) k * 2.0f + f.genome.phase);
                const float s = 2.2f + 1.8f * shine;
                g.setColour (juce::Colours::white.withAlpha (0.85f * tw * shine * where.weather));
                g.drawLine (at.x - s, at.y, at.x + s, at.y, 0.9f);
                g.drawLine (at.x, at.y - s, at.x, at.y + s, 0.9f);
            }
        }
    }

    // Twine and a ribbon holding the stems together, where they come together. Stems that
    // rise from keys far apart are not tied.
    float span = 0.0f;
    for (const auto& f : b.flowers)
        span = std::max (span, std::abs (f.spread));
    if (b.growth > 0.25f && span < 16.0f)
    {
        const float s = juce::jlimit (0.0f, 1.0f, (b.growth - 0.25f) / 0.4f);
        const float fade = rot > 0.85f ? 1.0f - (rot - 0.85f) / 0.15f : 1.0f;
        const auto ribbon = b.ribbon.interpolatedWith (juce::Colour (0xff5a0910), juce::jlimit (0.0f, 1.0f, rot * 1.2f) * 0.7f)
                                .withMultipliedAlpha (fade);
        const juce::Point<float> knot (b.x, baseY - 16.0f);
        juce::Path bow;
        bow.addEllipse (-9.0f * s, -3.5f * s, 9.0f * s, 7.0f * s);
        bow.addEllipse (0.0f, -3.5f * s, 9.0f * s, 7.0f * s);
        bow.startNewSubPath (-1.0f, 1.0f);
        bow.lineTo (-6.0f * s, 9.0f * s);
        bow.startNewSubPath (1.0f, 1.0f);
        bow.lineTo (5.0f * s, 10.0f * s + rot * 6.0f);
        bow.applyTransform (juce::AffineTransform::rotation (std::sin (time * 0.7f + (float) (b.seed % 100)) * 0.05f + rot * 0.4f).translated (knot));
        g.setColour (ribbon);
        g.strokePath (bow, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (ribbon.darker (0.35f));
        g.fillEllipse (juce::Rectangle<float> (3.4f, 3.4f).withCentre (knot));
    }
}
} // namespace bouquet::ui
