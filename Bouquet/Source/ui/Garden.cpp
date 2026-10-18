#include "Garden.h"
#include "../PluginProcessor.h"
#include "BouquetLookAndFeel.h"

namespace bouquet::ui
{
namespace
{
    constexpr float twoPiF = juce::MathConstants<float>::twoPi;
    const juce::Colour withered { 0xff8b7350 };
    const juce::Colour stemGreen { 0xff6f9f5c };

    float smooth (float x)
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return x * x * (3.0f - 2.0f * x);
    }

    float easeOut (float x)
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return 1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x);
    }

    juce::Point<float> bezier (juce::Point<float> a, juce::Point<float> b, juce::Point<float> c, juce::Point<float> d, float t)
    {
        const float u = 1.0f - t;
        return a * (u * u * u) + b * (3.0f * u * u * t) + c * (3.0f * u * t * t) + d * (t * t * t);
    }

    juce::Point<float> rotateAround (juce::Point<float> p, juce::Point<float> pivot, float angle)
    {
        const auto d = p - pivot;
        return pivot + juce::Point<float> (d.x * std::cos (angle) - d.y * std::sin (angle),
                                           d.x * std::sin (angle) + d.y * std::cos (angle));
    }

    // Colour slowly giving up: towards brown, greyer, a little darker.
    juce::Colour decay (juce::Colour c, float rot)
    {
        const float k = smooth (rot * 1.2f);
        return c.interpolatedWith (withered, k * 0.85f).withMultipliedSaturation (1.0f - 0.5f * k);
    }

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

    const float width = (float) getWidth(), maxHeight = (float) getHeight() - 20.0f;
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
        f.size = 10.5f + 6.0f * b.velocity + 3.5f * r.nextFloat();
        f.phase = r.nextFloat() * twoPiF;
        f.leafAt = 0.25f + 0.3f * r.nextFloat();
        f.leafSide = r.nextBool() ? 1.0f : -1.0f;
        f.bud = r.nextFloat() < 0.3f;
        f.species = r.nextFloat() < 0.6f ? dominant : (Species) r.nextInt ((int) Species::count);

        auto c = palette[(size_t) ((i + r.nextInt (3)) % (int) palette.size())];
        c = juce::Colour::fromHSV (std::fmod (c.getHue() + hueShift + 1.0f, 1.0f), c.getSaturation(),
                                   c.getBrightness() * (0.95f + 0.05f * r.nextFloat()), 1.0f);
        f.petal = c;
        f.petalDeep = c.withMultipliedSaturation (1.3f).withMultipliedBrightness (0.82f);
        f.centre = f.species == Species::poppy ? juce::Colour (0xff3a2f3c) : juce::Colour (0xfff2c14e);

        int petals = 5;
        float length = 1.0f, petalWidth = 0.5f;
        switch (f.species)
        {
            case Species::daisy:  petals = 12 + r.nextInt (5); length = 1.15f; petalWidth = 0.26f; break;
            case Species::tulip:  petals = 3;                  length = 1.25f; petalWidth = 0.55f; break;
            case Species::poppy:  petals = 4 + r.nextInt (2);  length = 1.0f;  petalWidth = 0.95f; break;
            case Species::cosmos: petals = 8;                  length = 1.15f; petalWidth = 0.5f;  break;
            case Species::bell:   petals = 1;                  length = 1.2f;  petalWidth = 0.8f;  break;
            case Species::rose:   petals = 5;                  length = 0.95f; petalWidth = 0.75f; break;
            case Species::count:  break;
        }
        for (int k = 0; k < petals; ++k)
        {
            Petal p;
            p.angle = twoPiF * (float) k / (float) petals + (r.nextFloat() - 0.5f) * 0.18f;
            p.length = length * (0.88f + 0.24f * r.nextFloat());
            p.width = petalWidth * (0.85f + 0.3f * r.nextFloat());
            p.fallAt = 0.2f + 0.6f * r.nextFloat();
            p.drift = r.nextFloat() * 2.0f - 1.0f;
            p.spin = r.nextFloat() * 2.0f - 1.0f;
            f.petals.push_back (p);
        }
        b.flowers.push_back (std::move (f));
    }
}

//==============================================================================
void Garden::paint (juce::Graphics& g)
{
    const float time = (float) clock();
    const float sway = processor.engine.breath.load();
    for (const auto& b : bouquets)
        paintBunch (g, b, time, sway);
}

void Garden::paintBunch (juce::Graphics& g, const Bunch& b, float time, float sway) const
{
    using namespace colours;
    const float baseY = (float) getHeight() - 3.0f;
    const juce::Point<float> base (b.x, baseY), tie (b.x, baseY - 16.0f);
    const float grow = easeOut (b.age / b.growSeconds);
    const float bloom = smooth ((b.age - 0.55f * b.growSeconds) / (0.9f * b.growSeconds));
    const float rot = b.rot;
    const float fade = rot > 0.8f ? 1.0f - (rot - 0.8f) / 0.2f : 1.0f;
    const float inkAlpha = 0.5f * fade;
    const auto stemColour = decay (stemGreen, rot).withMultipliedAlpha (fade);

    for (const auto& f : b.flowers)
    {
        // Where the head wants to be, swaying, then bowing over as it rots.
        const float lean = f.spread >= 0.0f ? 1.0f : -1.0f;
        const float breeze = (std::sin (time * 0.9f + f.phase) * 3.0f + sway * 2.5f) * (1.0f - rot);
        juce::Point<float> head (b.x + f.spread * 2.1f + breeze, baseY - f.height);
        const float droop = std::pow (rot, 1.3f) * 1.25f * lean;
        head = rotateAround (head, tie, droop);

        const juce::Point<float> c1 (b.x + f.spread * 0.2f, baseY - 6.0f);
        const auto c2 = rotateAround ({ b.x + f.spread * 1.4f + breeze * 0.4f, baseY - f.height * 0.55f }, tie, droop * 0.6f);
        const juce::Point<float> start (b.x + f.spread * 0.1f, baseY);

        // The stem, drawn upward as it grows, with a faint hand tremor.
        juce::Path stem;
        constexpr int segments = 18;
        const int drawn = std::max (1, (int) std::ceil (grow * segments));
        for (int s = 0; s <= drawn; ++s)
        {
            const float t = std::min (grow, (float) s / segments);
            auto p = bezier (start, c1, c2, head, t);
            p.x += std::sin (t * 23.0f + f.phase) * 0.45f;
            if (s == 0) stem.startNewSubPath (p);
            else        stem.lineTo (p);
        }
        g.setColour (stemColour);
        g.strokePath (stem, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const auto tip = bezier (start, c1, c2, head, grow);

        // A leaf unfurling partway up.
        const float unfurl = smooth ((grow - f.leafAt) / 0.25f);
        if (unfurl > 0.01f)
        {
            const auto at = bezier (start, c1, c2, head, f.leafAt);
            const float len = 14.0f * unfurl, wid = 5.0f * unfurl;
            const float angle = -0.9f * f.leafSide + droop * 0.8f + f.leafSide * rot * 0.9f;
            juce::Path leaf;
            leaf.startNewSubPath (0.0f, 0.0f);
            leaf.quadraticTo (wid * f.leafSide, -len * 0.5f, 0.0f, -len);
            leaf.quadraticTo (-wid * 0.4f * f.leafSide, -len * 0.5f, 0.0f, 0.0f);
            leaf.closeSubPath();
            leaf.applyTransform (juce::AffineTransform::rotation (angle).translated (at));
            g.setColour (decay (juce::Colour (0xff8cc47a), rot).withMultipliedAlpha (0.85f * fade));
            g.fillPath (leaf);
            g.setColour (ink.withAlpha (inkAlpha * 0.7f));
            g.strokePath (leaf, juce::PathStrokeType (0.9f));
        }

        // A closed bud on a little side shoot.
        if (f.bud && grow > 0.6f)
        {
            const auto at = bezier (start, c1, c2, head, 0.7f);
            const juce::Point<float> budPos = at + juce::Point<float> (8.0f * -f.leafSide, -6.0f);
            g.setColour (stemColour);
            g.drawLine ({ at, budPos }, 1.1f);
            const float s = 3.5f * smooth ((grow - 0.6f) / 0.4f);
            g.setColour (decay (f.petalDeep, rot).withMultipliedAlpha (fade));
            g.fillEllipse (budPos.x - s * 0.7f, budPos.y - s * 1.6f, s * 1.4f, s * 2.0f);
            g.setColour (ink.withAlpha (inkAlpha * 0.7f));
            g.drawEllipse (budPos.x - s * 0.7f, budPos.y - s * 1.6f, s * 1.4f, s * 2.0f, 0.8f);
        }

        if (grow > 0.65f)
        {
            const auto dir = tip - bezier (start, c1, c2, head, std::max (0.0f, grow - 0.05f));
            const float tilt = std::atan2 (dir.x, -dir.y);
            paintHead (g, f, tip, tilt, std::max (bloom, 0.12f * smooth ((grow - 0.65f) / 0.35f)), rot);
        }
    }

    // A ribbon tying the stems together.
    if (grow > 0.25f)
    {
        const float s = smooth ((grow - 0.25f) / 0.4f);
        const auto ribbon = decay (b.ribbon, rot * 0.7f).withMultipliedAlpha (fade);
        juce::Path bow;
        bow.addEllipse (-9.0f * s, -3.5f * s, 9.0f * s, 7.0f * s);
        bow.addEllipse (0.0f, -3.5f * s, 9.0f * s, 7.0f * s);
        bow.startNewSubPath (-1.0f, 1.0f);
        bow.lineTo (-6.0f * s, 9.0f * s);
        bow.startNewSubPath (1.0f, 1.0f);
        bow.lineTo (5.0f * s, 10.0f * s);
        bow.applyTransform (juce::AffineTransform::rotation (std::sin (time * 0.7f + (float) b.seed) * 0.05f).translated (tie));
        g.setColour (ribbon);
        g.strokePath (bow, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (ink.withAlpha (inkAlpha * 0.5f));
        g.strokePath (bow, juce::PathStrokeType (0.6f));
        g.setColour (ribbon.darker (0.2f));
        g.fillEllipse (juce::Rectangle<float> (3.4f, 3.4f).withCentre (tie));
    }
}

void Garden::paintHead (juce::Graphics& g, const Flower& f, juce::Point<float> at, float tilt, float bloom, float rot) const
{
    using namespace colours;
    const float fade = rot > 0.8f ? 1.0f - (rot - 0.8f) / 0.2f : 1.0f;
    const auto petal = decay (f.petal, rot);
    const auto deep = decay (f.petalDeep, rot);
    const float r = f.size;
    const float open = 0.28f + 0.72f * bloom;
    const float slump = rot * 0.7f;   // petals sag as they rot
    const float level = std::min (1.0f, processor.outputMeter.load() * 1.5f);
    const float pulse = 1.0f + 0.04f * level * (1.0f - rot);

    auto drawPetal = [&] (const juce::Path& shape, const Petal& p, float baseAngle, juce::Point<float> origin)
    {
        float alpha = fade;
        juce::Point<float> offset;
        float extraSpin = 0.0f;
        if (rot > p.fallAt)
        {
            // This petal has let go: it drifts down and fades.
            const float fall = juce::jlimit (0.0f, 1.0f, (rot - p.fallAt) / 0.3f);
            offset = { p.drift * 22.0f * fall, fall * fall * 55.0f };
            extraSpin = p.spin * fall * 2.5f;
            alpha *= 1.0f - fall;
        }
        if (alpha <= 0.01f)
            return;

        const auto t = juce::AffineTransform::rotation (baseAngle + extraSpin).translated (origin + offset);
        auto path = shape;
        path.applyTransform (t);

        const auto bounds = path.getBounds();
        juce::ColourGradient grad (deep.withMultipliedAlpha (0.85f * alpha), (origin + offset).x, (origin + offset).y,
                                   petal.withMultipliedAlpha (0.8f * alpha), bounds.getCentreX() + (bounds.getCentreX() - origin.x),
                                   bounds.getCentreY() + (bounds.getCentreY() - origin.y), true);
        g.setGradientFill (grad);
        g.fillPath (path);
        g.setColour (ink.withAlpha (0.5f * alpha));
        g.strokePath (path, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    switch (f.species)
    {
        case Species::daisy:
        case Species::cosmos:
        case Species::poppy:
        case Species::rose:
        {
            for (const auto& p : f.petals)
            {
                const float len = r * p.length * open * pulse;
                const float wid = r * p.width * (0.5f + 0.5f * open);
                juce::Path shape;
                if (f.species == Species::daisy)
                {
                    shape.addEllipse (-wid * 0.5f, -len, wid, len);
                }
                else if (f.species == Species::cosmos)
                {
                    shape.startNewSubPath (0.0f, 0.0f);
                    shape.quadraticTo (-wid, -len * 0.55f, -wid * 0.55f, -len);
                    shape.lineTo (-wid * 0.2f, -len * 0.88f);
                    shape.lineTo (0.0f, -len * 1.02f);
                    shape.lineTo (wid * 0.2f, -len * 0.88f);
                    shape.lineTo (wid * 0.55f, -len);
                    shape.quadraticTo (wid, -len * 0.55f, 0.0f, 0.0f);
                    shape.closeSubPath();
                }
                else if (f.species == Species::poppy)
                {
                    shape.addEllipse (-wid * 0.5f, -len * 1.05f, wid, len * 1.05f);
                }
                else // rose: soft heart-shaped petals
                {
                    shape.startNewSubPath (0.0f, 0.0f);
                    shape.cubicTo (-wid, -len * 0.45f, -wid * 0.8f, -len * 1.05f, 0.0f, -len * 0.82f);
                    shape.cubicTo (wid * 0.8f, -len * 1.05f, wid, -len * 0.45f, 0.0f, 0.0f);
                    shape.closeSubPath();
                }
                // Sagging: petals rotate towards the ground as the flower rots.
                const float a = p.angle + tilt;
                const float sag = std::sin (a) * slump;
                drawPetal (shape, p, a + sag, at);
            }

            // The heart of the flower.
            const float cr = r * (f.species == Species::poppy ? 0.32f : 0.26f) * (0.6f + 0.4f * open);
            const auto centre = decay (f.centre, rot).withMultipliedAlpha (fade);
            g.setColour (centre);
            g.fillEllipse (juce::Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (at));
            g.setColour (ink.withAlpha (0.55f * fade));
            g.drawEllipse (juce::Rectangle<float> (cr * 2.0f, cr * 2.0f).withCentre (at), 0.9f);
            g.setColour ((f.species == Species::poppy ? butter : ink).withAlpha (0.45f * fade));
            for (int k = 0; k < 6; ++k)
            {
                const float a = (float) k * twoPiF / 6.0f + tilt;
                g.fillEllipse (juce::Rectangle<float> (1.3f, 1.3f).withCentre (at + juce::Point<float> (std::cos (a), std::sin (a)) * cr * 0.55f));
            }
            break;
        }

        case Species::tulip:
        {
            // A cup of three petals that opens from closed to a loose crown.
            const float spreadAngles[] = { -0.38f, 0.38f, 0.0f };
            for (size_t k = 0; k < f.petals.size() && k < 3; ++k)
            {
                const auto& p = f.petals[k];
                const float len = r * p.length * (0.85f + 0.25f * open) * pulse;
                const float wid = r * p.width;
                juce::Path shape;
                shape.startNewSubPath (0.0f, 0.0f);
                shape.cubicTo (-wid, -len * 0.25f, -wid * 0.75f, -len * 0.95f, 0.0f, -len * 1.05f);
                shape.cubicTo (wid * 0.75f, -len * 0.95f, wid, -len * 0.25f, 0.0f, 0.0f);
                shape.closeSubPath();
                const float a = tilt + spreadAngles[k] * (0.35f + 0.9f * open) + slump * (k == 0 ? -0.6f : k == 1 ? 0.6f : 0.0f);
                drawPetal (shape, p, a, at);
            }
            break;
        }

        case Species::bell:
        {
            // A bell hanging from the tip, mouth to the ground, swinging gently.
            const auto& p = f.petals.front();
            const float h = r * 1.5f * (0.6f + 0.4f * open) * pulse;
            const float w = r * (0.5f + 0.5f * open);
            juce::Path shape;
            shape.startNewSubPath (-w * 0.25f, 0.0f);
            shape.cubicTo (-w * 0.45f, h * 0.45f, -w * 0.6f, h * 0.8f, -w, h);
            shape.quadraticTo (-w * 0.66f, h * 0.9f, -w * 0.33f, h);
            shape.quadraticTo (0.0f, h * 0.9f, w * 0.33f, h);
            shape.quadraticTo (w * 0.66f, h * 0.9f, w, h);
            shape.cubicTo (w * 0.6f, h * 0.8f, w * 0.45f, h * 0.45f, w * 0.25f, 0.0f);
            shape.closeSubPath();
            drawPetal (shape, p, tilt * 0.4f + std::sin ((float) clock() * 1.3f + f.phase) * 0.08f + slump, at);
            break;
        }

        case Species::count:
            break;
    }
}
} // namespace bouquet::ui
