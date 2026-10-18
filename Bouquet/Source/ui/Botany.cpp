#include "Botany.h"
#include "BouquetLookAndFeel.h"
#include "../Presets.h"
#include "../Parameters.h"

namespace bouquet::ui::botany
{
namespace
{
    constexpr float pi = juce::MathConstants<float>::pi;
    constexpr float twoPi = juce::MathConstants<float>::twoPi;

    const juce::Colour blood    { 0xff9c1020 };
    const juce::Colour clot     { 0xff5a0910 };
    const juce::Colour necrotic { 0xff2a0d0b };
    const juce::Colour mould    { 0xffc9d2b6 };
    const juce::Colour stemGreen { 0xff5f8f45 };
    const juce::Colour leafGreen { 0xff6fa155 };

    float smooth (float x)
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        return x * x * (3.0f - 2.0f * x);
    }

    juce::Point<float> bezier (const Pose& p, float t)
    {
        const float u = 1.0f - t;
        return p.base * (u * u * u) + p.c1 * (3.0f * u * u * t) + p.c2 * (3.0f * u * t * t) + p.head * (t * t * t);
    }

    juce::Point<float> tangent (const Pose& p, float t)
    {
        const float u = 1.0f - t;
        auto d = (p.c1 - p.base) * (3.0f * u * u) + (p.c2 - p.c1) * (6.0f * u * t) + (p.head - p.c2) * (3.0f * t * t);
        const float len = std::max (0.001f, d.getDistanceFromOrigin());
        return d / len;
    }

    // Living colour giving way: first to blood, then to black.
    juce::Colour rotted (juce::Colour c, float rot)
    {
        const float bleed = smooth ((rot - 0.05f) / 0.35f);
        const float die = smooth ((rot - 0.45f) / 0.4f);
        return c.interpolatedWith (blood, bleed * 0.9f).interpolatedWith (necrotic, die);
    }

    juce::Colour rottedStem (juce::Colour c, float rot)
    {
        return c.interpolatedWith (juce::Colour (0xff4a3b1a), smooth (rot / 0.5f))
                .interpolatedWith (juce::Colour (0xff1c130b), smooth ((rot - 0.5f) / 0.4f));
    }

    // pow() of a slightly negative sine would be NaN.
    float sinPow (float x, float e) { return std::pow (std::max (0.0f, std::sin (x)), e); }

    // Half-width of a petal at s (0 base .. 1 tip), per species.
    float profile (Species sp, float s)
    {
        switch (sp)
        {
            case Species::daisy:  return sinPow (pi * juce::jlimit (0.0f, 1.0f, s * 0.92f + 0.04f), 0.55f);
            case Species::cosmos: return sinPow (pi * (0.08f + 0.84f * s), 0.45f) * (0.75f + 0.25f * s);
            case Species::poppy:  return sinPow (pi * (0.05f + 0.9f * s), 0.4f) * (0.8f + 0.35f * s);
            case Species::rose:   return sinPow (pi * (0.12f + 0.8f * s), 0.5f) * (0.85f + 0.2f * s);
            case Species::tulip:  return sinPow (pi * s, 0.75f);
            case Species::bell:
            case Species::count:  break;
        }
        return std::max (0.0f, std::sin (pi * s));
    }

    // A petal pointing up (towards -y), with crinkled edges that worsen as it rots.
    juce::Path petalPath (Species sp, float length, float width, float crinkle, float phase)
    {
        constexpr int steps = 14;
        juce::Path p;
        p.startNewSubPath (0.0f, 0.0f);
        for (int i = 1; i <= steps; ++i)
        {
            const float s = (float) i / steps;
            const float wob = crinkle * std::sin (s * 9.0f + phase) * s;
            p.lineTo (-width * 0.5f * profile (sp, s) + wob, -length * s);
        }
        if (sp == Species::cosmos)   // notched tip
        {
            p.lineTo (-width * 0.12f, -length * 0.93f);
            p.lineTo (0.0f, -length * 1.02f);
            p.lineTo (width * 0.12f, -length * 0.93f);
        }
        for (int i = steps; i >= 1; --i)
        {
            const float s = (float) i / steps;
            const float wob = crinkle * std::sin (s * 8.0f + phase * 1.7f) * s;
            p.lineTo (width * 0.5f * profile (sp, s) + wob, -length * s);
        }
        p.closeSubPath();
        return p.createPathWithRoundedCorners (width * 0.18f);
    }

    struct PetalStyle
    {
        juce::Colour base, tip;
        float alpha, gloss;
        bool veins;
    };

    void fillPetal (juce::Graphics& g, const juce::Path& local, const juce::AffineTransform& t,
                    float length, float width, const PetalStyle& st)
    {
        auto path = local;
        path.applyTransform (t);
        const auto origin = juce::Point<float> (0.0f, 0.0f).transformedBy (t);
        const auto tipPt = juce::Point<float> (0.0f, -length).transformedBy (t);

        juce::ColourGradient grad (st.base.withMultipliedAlpha (st.alpha), origin.x, origin.y,
                                   st.tip.withMultipliedAlpha (st.alpha), tipPt.x, tipPt.y, false);
        grad.addColour (0.65, st.tip.interpolatedWith (juce::Colours::white, 0.12f).withMultipliedAlpha (st.alpha));
        g.setGradientFill (grad);
        g.fillPath (path);

        if (st.veins && length > 6.0f)
        {
            g.setColour (st.base.darker (0.3f).withAlpha (0.22f * st.alpha));
            for (float side : { -0.22f, 0.0f, 0.22f })
            {
                juce::Path vein;
                vein.startNewSubPath (0.0f, -length * 0.06f);
                vein.quadraticTo (side * width * 0.6f, -length * 0.45f, side * width, -length * 0.85f);
                vein.applyTransform (t);
                g.strokePath (vein, juce::PathStrokeType (0.5f));
            }
        }

        // The edge catches a little shadow.
        g.setColour (st.base.darker (0.45f).withAlpha (0.28f * st.alpha));
        g.strokePath (path, juce::PathStrokeType (0.6f));

        // Wet sheen (rotting petals glisten).
        if (st.gloss > 0.01f)
        {
            const auto mid = juce::Point<float> (width * 0.12f, -length * 0.55f).transformedBy (t);
            g.setColour (juce::Colours::white.withAlpha (0.45f * st.gloss * st.alpha));
            g.fillEllipse (juce::Rectangle<float> (width * 0.22f, length * 0.28f).withCentre (mid));
        }
    }

    void paintDisc (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour, float rot, float alpha, uint32_t seed)
    {
        const auto base = rot > 0.0f ? colour.interpolatedWith (clot, smooth (rot / 0.5f)) : colour;
        juce::ColourGradient grad (base.brighter (0.25f).withMultipliedAlpha (alpha), c.x - r * 0.3f, c.y - r * 0.3f,
                                   base.darker (0.35f).withMultipliedAlpha (alpha), c.x + r, c.y + r, true);
        g.setGradientFill (grad);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));

        // Florets in a golden-angle spiral.
        const int n = juce::jlimit (12, 60, (int) (r * r * 0.9f));
        juce::Random rnd ((juce::int64) seed);
        for (int i = 0; i < n; ++i)
        {
            const float a = (float) i * 2.39996f;
            const float d = r * 0.88f * std::sqrt ((float) i / (float) n);
            const auto p = c + juce::Point<float> (std::cos (a), std::sin (a)) * d;
            g.setColour ((rnd.nextBool() ? base.darker (0.5f) : base.brighter (0.4f)).withMultipliedAlpha (0.7f * alpha));
            g.fillEllipse (juce::Rectangle<float> (1.1f, 1.1f).withCentre (p));
        }

        // Rot: the heart turns to wet, glistening flesh.
        if (rot > 0.2f)
        {
            const float k = smooth ((rot - 0.2f) / 0.4f);
            g.setColour (juce::Colours::white.withAlpha (0.4f * k * alpha));
            g.fillEllipse (juce::Rectangle<float> (r * 0.5f, r * 0.3f).withCentre (c + juce::Point<float> (-r * 0.25f, -r * 0.3f)));
        }
    }
}

//==============================================================================
Genome makeGenome (Species species, juce::Colour petal, float size, uint32_t seed)
{
    juce::Random r ((juce::int64) seed);
    Genome g;
    g.species = species;
    g.size = size;
    g.seed = seed;
    g.phase = r.nextFloat() * twoPi;
    g.petal = petal;
    g.petalDeep = petal.withMultipliedSaturation (1.35f).withMultipliedBrightness (0.72f);
    g.leafAt = 0.22f + 0.28f * r.nextFloat();
    g.leafSide = r.nextBool() ? 1.0f : -1.0f;
    g.leaves = 1 + r.nextInt (2);
    g.bud = r.nextFloat() < 0.3f;

    int outer = 5, inner = 0;
    float length = 1.0f, width = 0.5f;
    switch (species)
    {
        case Species::daisy:  outer = 14 + r.nextInt (6); inner = outer / 2; length = 1.15f; width = 0.24f;
                              g.centre = juce::Colour (0xfff0b43c); break;
        case Species::cosmos: outer = 8;                  length = 1.15f; width = 0.55f;
                              g.centre = juce::Colour (0xffe8a92c); break;
        case Species::poppy:  outer = 4;                  inner = 0; length = 1.05f; width = 1.0f;
                              g.centre = juce::Colour (0xff4b5a3c); break;
        case Species::rose:   outer = 5; inner = 5;       length = 0.95f; width = 0.9f;
                              g.centre = petal.darker (0.4f); break;
        case Species::tulip:  outer = 3; inner = 3;       length = 1.3f;  width = 0.6f;
                              g.centre = petal; break;
        case Species::bell:   outer = 1 + r.nextInt (2);  length = 1.25f; width = 0.85f;
                              g.centre = petal; break;
        case Species::count:  break;
    }

    auto makeRing = [&] (int count, float lengthScale, float offset)
    {
        std::vector<PetalGene> ring;
        for (int k = 0; k < count; ++k)
            ring.push_back ({ twoPi * ((float) k + offset) / (float) std::max (1, count) + (r.nextFloat() - 0.5f) * 0.16f,
                              length * lengthScale * (0.88f + 0.24f * r.nextFloat()),
                              width * (0.85f + 0.3f * r.nextFloat()),
                              0.15f + 0.6f * r.nextFloat(),
                              r.nextFloat() * 2.0f - 1.0f,
                              r.nextFloat() * 2.0f - 1.0f,
                              r.nextFloat() * twoPi });
        return ring;
    };
    g.petals = makeRing (outer, 1.0f, 0.0f);
    g.innerPetals = makeRing (inner, species == Species::rose ? 0.62f : 0.8f, 0.5f);
    return g;
}

Genome genomeForPreset (const Preset& p)
{
    const uint32_t seed = p.seed();
    juce::Random r ((juce::int64) seed);

    // Species follows the voice; the seed sometimes picks a cousin.
    const Species byInstrument[] = { Species::cosmos, Species::rose, Species::tulip, Species::daisy, Species::bell,
                                     Species::cosmos, Species::poppy, Species::rose, Species::bell, Species::daisy };
    const int instrument = juce::jlimit (0, 9, (int) p.get (ids::instrument, 1.0f));
    auto species = byInstrument[instrument];
    if (r.nextFloat() < 0.25f)
        species = (Species) r.nextInt ((int) Species::count);

    // Colour follows the harmony: warm for major, cool for minor, rose for the rest.
    const int harmony = (int) p.get (ids::harmony, 0.0f);
    const int chord = (int) p.get (ids::chord, 0.0f);
    const bool minorish = harmony == 4 || harmony == 7 || chord == 2 || chord == 6 || chord == 10 || chord == 12;
    const bool majorish = harmony == 3 || harmony == 6 || harmony == 8 || chord == 1 || chord == 5 || chord == 8 || chord == 9;
    float hue = majorish ? 0.02f + 0.12f * r.nextFloat()
              : minorish ? 0.66f + 0.14f * r.nextFloat()
                         : 0.88f + 0.1f * r.nextFloat();
    hue = std::fmod (hue - 0.05f * p.get (ids::colour, 0.0f) + 1.0f, 1.0f);
    const bool magic = p.get (ids::magic, 1.0f) > 0.5f;
    const float saturation = magic ? 0.45f + 0.35f * r.nextFloat() : 0.12f + 0.12f * r.nextFloat();   // bare sounds are pale
    const float brightness = 0.86f + 0.12f * r.nextFloat();

    const float size = 11.0f + 5.0f * p.get (ids::mix, 0.6f) + 3.0f * r.nextFloat();
    return makeGenome (species, juce::Colour::fromHSV (hue, saturation, brightness, 1.0f), size, seed);
}

//==============================================================================
void paintPlant (juce::Graphics& g, const Genome& gen, const Pose& pose)
{
    const float rot = pose.rot;
    const float fade = (rot > 0.85f ? 1.0f - (rot - 0.85f) / 0.15f : 1.0f) * pose.alpha;
    if (fade <= 0.01f || pose.grow <= 0.0f)
        return;

    const float grow = juce::jlimit (0.0f, 1.0f, pose.grow);
    const auto stemColour = rottedStem (stemGreen, rot);

    // --- Pool of blood spreading at the foot of a rotting plant.
    const float poolSize = 40.0f * smooth ((rot - 0.3f) / 0.45f);
    if (poolSize > 1.0f)
    {
        const auto c = pose.base + juce::Point<float> (0.0f, 1.0f);
        juce::ColourGradient pool (clot.withAlpha (0.9f * fade), c.x, c.y, blood.withAlpha (0.0f), c.x + poolSize * 0.5f, c.y, true);
        pool.addColour (0.7, blood.withAlpha (0.75f * fade));
        g.setGradientFill (pool);
        g.fillEllipse (juce::Rectangle<float> (poolSize, poolSize * 0.22f).withCentre (c));
        g.setColour (juce::Colours::white.withAlpha (0.25f * fade));
        g.fillEllipse (juce::Rectangle<float> (poolSize * 0.25f, poolSize * 0.04f).withCentre (c + juce::Point<float> (-poolSize * 0.12f, -poolSize * 0.03f)));
    }

    // --- The stem: tapered, shaded down one side.
    constexpr int segments = 16;
    std::array<juce::Point<float>, segments + 1> spine {};
    for (int i = 0; i <= segments; ++i)
        spine[(size_t) i] = bezier (pose, grow * (float) i / segments);

    juce::Path stem;
    for (int i = 0; i <= segments; ++i)
    {
        const float t = (float) i / segments;
        const auto n = tangent (pose, grow * t);
        const auto normal = juce::Point<float> (-n.y, n.x);
        const auto p = spine[(size_t) i] + normal * (pose.stemWidth * (1.0f - 0.45f * t) * 0.5f);
        if (i == 0) stem.startNewSubPath (p);
        else        stem.lineTo (p);
    }
    for (int i = segments; i >= 0; --i)
    {
        const float t = (float) i / segments;
        const auto n = tangent (pose, grow * t);
        const auto normal = juce::Point<float> (-n.y, n.x);
        stem.lineTo (spine[(size_t) i] - normal * (pose.stemWidth * (1.0f - 0.45f * t) * 0.5f));
    }
    stem.closeSubPath();
    g.setColour (stemColour.withMultipliedAlpha (fade));
    g.fillPath (stem);
    {
        juce::Path light;
        for (int i = 0; i <= segments; ++i)
        {
            const auto n = tangent (pose, grow * (float) i / segments);
            const auto p = spine[(size_t) i] + juce::Point<float> (-n.y, n.x) * (pose.stemWidth * 0.18f);
            if (i == 0) light.startNewSubPath (p);
            else        light.lineTo (p);
        }
        g.setColour (stemColour.brighter (0.5f).withAlpha (0.35f * fade));
        g.strokePath (light, juce::PathStrokeType (0.6f));
    }

    // --- Leaves with a midrib and veins, curling and blackening as they rot.
    for (int k = 0; k < gen.leaves; ++k)
    {
        const float at = gen.leafAt + 0.22f * (float) k;
        const float unfurl = smooth ((grow - at) / 0.25f);
        if (unfurl <= 0.01f)
            continue;
        const float side = (k % 2 == 0 ? gen.leafSide : -gen.leafSide);
        const auto anchor = bezier (pose, at);
        const auto n = tangent (pose, at);
        const float stemAngle = std::atan2 (n.x, -n.y);
        const float len = gen.size * 1.5f * unfurl * (1.0f - 0.15f * (float) k);
        const float wid = len * 0.32f * (1.0f - 0.55f * smooth (rot * 1.3f));
        const float angle = stemAngle + side * (0.85f + 0.6f * smooth (rot * 1.2f)) + std::sin (pose.time * 1.1f + gen.phase + (float) k) * 0.04f;

        juce::Path leaf;
        leaf.startNewSubPath (0.0f, 0.0f);
        leaf.cubicTo (wid, -len * 0.25f, wid * 0.8f, -len * 0.75f, 0.0f, -len);
        leaf.cubicTo (-wid * 0.6f, -len * 0.7f, -wid * 0.9f, -len * 0.25f, 0.0f, 0.0f);
        leaf.closeSubPath();
        const auto t = juce::AffineTransform::rotation (angle).translated (anchor);
        leaf.applyTransform (t);

        const auto c = rottedStem (leafGreen, rot);
        const auto base = juce::Point<float> (0.0f, 0.0f).transformedBy (t);
        const auto tip = juce::Point<float> (0.0f, -len).transformedBy (t);
        juce::ColourGradient grad (c.darker (0.25f).withMultipliedAlpha (fade), base.x, base.y,
                                   c.brighter (0.2f).withMultipliedAlpha (fade), tip.x, tip.y, false);
        g.setGradientFill (grad);
        g.fillPath (leaf);

        g.setColour (c.brighter (0.45f).withAlpha (0.5f * fade));
        g.drawLine ({ base, tip }, 0.6f);
        for (int v = 1; v <= 3; ++v)
        {
            const float s = (float) v / 4.0f;
            const auto on = base + (tip - base) * s;
            for (float dir : { -1.0f, 1.0f })
            {
                const auto out = juce::Point<float> (dir * wid * 0.55f, -len * (s + 0.12f)).transformedBy (t);
                g.setColour (c.brighter (0.3f).withAlpha (0.25f * fade));
                g.drawLine ({ on, out }, 0.4f);
            }
        }
    }

    // --- A closed bud on a side shoot.
    if (gen.bud && grow > 0.6f)
    {
        const auto at = bezier (pose, 0.68f);
        const auto budPos = at + juce::Point<float> (7.0f * -gen.leafSide, -7.0f);
        g.setColour (stemColour.withMultipliedAlpha (fade));
        g.drawLine ({ at, budPos }, 1.1f);
        const float s = gen.size * 0.32f * smooth ((grow - 0.6f) / 0.4f);
        const auto budColour = rotted (gen.petalDeep, rot);
        juce::ColourGradient grad (budColour.withMultipliedAlpha (fade), budPos.x, budPos.y,
                                   budColour.brighter (0.3f).withMultipliedAlpha (fade), budPos.x, budPos.y - s * 2.0f, false);
        g.setGradientFill (grad);
        g.fillEllipse (budPos.x - s * 0.6f, budPos.y - s * 1.8f, s * 1.2f, s * 1.9f);
        g.setColour (stemColour.withMultipliedAlpha (fade));
        juce::Path sepal;
        sepal.addTriangle (budPos.x - s * 0.6f, budPos.y, budPos.x + s * 0.6f, budPos.y, budPos.x, budPos.y - s * 0.9f);
        g.fillPath (sepal);
    }

    // --- The flower head.
    const auto tip = spine.back();
    const auto dir = tangent (pose, grow);
    const float tilt = std::atan2 (dir.x, -dir.y);
    const float bloom = juce::jlimit (0.0f, 1.0f, pose.bloom);
    const float r = gen.size * (1.0f + 0.08f * pose.glow);
    const float open = 0.25f + 0.75f * bloom;
    const float shrivel = 1.0f - 0.55f * smooth (rot * 1.4f);
    const float gloss = smooth ((rot - 0.15f) / 0.2f) * (1.0f - smooth ((rot - 0.75f) / 0.15f));
    const auto petalColour = rotted (gen.petal, rot);
    const auto deepColour = rotted (gen.petalDeep, rot);

    if (pose.glow > 0.01f)
        drawGlow (g, tip, r * 3.2f, gen.petal.withAlpha (0.35f * pose.glow * fade));

    if (grow > 0.6f)
    {
        auto drawRing = [&] (const std::vector<PetalGene>& ring, float scale, juce::Colour base, juce::Colour tipColour, float angleOffset)
        {
            for (const auto& p : ring)
            {
                float alpha = fade;
                juce::Point<float> offset;
                float spin = 0.0f;
                if (rot > p.fallAt)
                {
                    // Torn loose: drops, tumbles, leaves a smear.
                    const float fall = juce::jlimit (0.0f, 1.0f, (rot - p.fallAt) / 0.3f);
                    offset = { p.drift * 26.0f * fall, fall * fall * (pose.base.y - tip.y + 4.0f) };
                    spin = p.spin * fall * 3.0f;
                    alpha *= 1.0f - fall * fall;
                }
                if (alpha <= 0.01f)
                    continue;
                const float len = r * p.length * open * scale;
                const float wid = r * p.width * (0.55f + 0.45f * open) * shrivel * scale;
                const float droop = std::sin (p.angle + tilt) * rot * 0.8f;
                const auto path = petalPath (gen.species, len, wid, wid * (0.04f + 0.4f * rot), p.phase);
                const auto t = juce::AffineTransform::rotation (p.angle + tilt + angleOffset + droop + spin).translated (tip + offset);
                fillPetal (g, path, t, len, wid, { base, tipColour, alpha, gloss, true });
            }
        };

        // Sepals cradle cupped flowers.
        auto sepals = [&]
        {
            for (int k = 0; k < 3; ++k)
            {
                juce::Path s;
                s.addTriangle (-r * 0.16f, 0.0f, r * 0.16f, 0.0f, 0.0f, -r * 0.55f);
                s.applyTransform (juce::AffineTransform::rotation (tilt + pi + (float) (k - 1) * 0.55f).translated (tip));
                g.setColour (stemColour.withMultipliedAlpha (fade));
                g.fillPath (s);
            }
        };

        switch (gen.species)
        {
            case Species::daisy:
            case Species::cosmos:
            case Species::poppy:
            {
                drawRing (gen.petals, 1.0f, deepColour, petalColour, 0.0f);
                drawRing (gen.innerPetals, 0.85f, deepColour.darker (0.1f), petalColour, 0.0f);
                if (gen.species == Species::poppy)
                {
                    // The dark blotch, the seed pod, the ring of stamens.
                    g.setColour (necrotic.withAlpha (0.75f * fade));
                    g.fillEllipse (juce::Rectangle<float> (r * 0.9f, r * 0.9f).withCentre (tip));
                    paintDisc (g, tip, r * 0.28f, gen.centre, rot, fade, gen.seed);
                    for (int k = 0; k < 14; ++k)
                    {
                        const float a = (float) k * twoPi / 14.0f;
                        g.setColour (necrotic.withAlpha (0.8f * fade));
                        g.fillEllipse (juce::Rectangle<float> (1.4f, 1.4f).withCentre (tip + juce::Point<float> (std::cos (a), std::sin (a)) * r * 0.42f));
                    }
                }
                else
                {
                    paintDisc (g, tip, r * (gen.species == Species::daisy ? 0.3f : 0.24f) * (0.7f + 0.3f * open), gen.centre, rot, fade, gen.seed);
                }
                break;
            }

            case Species::rose:
            {
                sepals();
                drawRing (gen.petals, 1.0f, deepColour, petalColour, 0.0f);
                drawRing (gen.innerPetals, 0.75f, deepColour.darker (0.15f), petalColour.darker (0.05f), 0.0f);
                // The tight heart of the rose: overlapping curls.
                for (int k = 0; k < 4; ++k)
                {
                    const float rr = r * (0.42f - 0.08f * (float) k) * (0.6f + 0.4f * open);
                    g.setColour (deepColour.darker (0.1f * (float) k).withMultipliedAlpha (fade));
                    g.fillEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 1.6f).withCentre (tip + juce::Point<float> (rr * 0.15f * (float) (k % 2 ? 1 : -1), 0.0f)));
                    g.setColour (deepColour.darker (0.6f).withAlpha (0.35f * fade));
                    juce::Path curl;
                    curl.addCentredArc (tip.x, tip.y, rr, rr * 0.8f, (float) k, 0.3f, 3.6f, true);
                    g.strokePath (curl, juce::PathStrokeType (0.7f));
                }
                break;
            }

            case Species::tulip:
            {
                sepals();
                const float spread[] = { -0.42f, 0.42f, 0.0f };
                for (size_t k = 0; k < gen.petals.size() && k < 3; ++k)
                {
                    const auto& p = gen.petals[k];
                    const float len = r * p.length * (0.9f + 0.2f * open);
                    const float wid = r * p.width * shrivel;
                    const auto path = petalPath (Species::tulip, len, wid, wid * (0.03f + 0.4f * rot), p.phase);
                    const float a = tilt + spread[k] * (0.3f + 0.9f * open) + std::sin (spread[k] * 10.0f) * rot * 0.6f;
                    fillPetal (g, path, juce::AffineTransform::rotation (a).translated (tip), len, wid,
                               { deepColour.darker (0.15f), petalColour.darker (0.08f), fade, gloss, true });
                }
                for (size_t k = 0; k < gen.innerPetals.size() && k < 3; ++k)
                {
                    const auto& p = gen.innerPetals[k];
                    const float len = r * p.length * 1.1f * (0.9f + 0.2f * open);
                    const float wid = r * p.width * 0.9f * shrivel;
                    const auto path = petalPath (Species::tulip, len, wid, wid * (0.03f + 0.4f * rot), p.phase);
                    const float a = tilt + ((float) k - 1.0f) * 0.22f * (0.3f + 0.9f * open);
                    fillPetal (g, path, juce::AffineTransform::rotation (a).translated (tip), len, wid,
                               { deepColour, petalColour, fade, gloss, true });
                }
                break;
            }

            case Species::bell:
            {
                // Bells hang from an arched tip, mouths to the ground.
                for (size_t k = 0; k < gen.petals.size(); ++k)
                {
                    const auto& p = gen.petals[k];
                    const auto hang = tip + juce::Point<float> ((float) k * r * 1.3f * (gen.leafSide), (float) k * r * 0.4f);
                    const float h = r * 1.5f * (0.6f + 0.4f * open) * p.length / 1.25f;
                    const float w = r * (0.5f + 0.5f * open) * shrivel;
                    juce::Path bell;
                    bell.startNewSubPath (-w * 0.25f, 0.0f);
                    bell.cubicTo (-w * 0.45f, h * 0.45f, -w * 0.62f, h * 0.8f, -w, h);
                    bell.quadraticTo (-w * 0.66f, h * 0.88f, -w * 0.33f, h * 1.02f);
                    bell.quadraticTo (0.0f, h * 0.9f, w * 0.33f, h * 1.02f);
                    bell.quadraticTo (w * 0.66f, h * 0.88f, w, h);
                    bell.cubicTo (w * 0.62f, h * 0.8f, w * 0.45f, h * 0.45f, w * 0.25f, 0.0f);
                    bell.closeSubPath();
                    const float swing = std::sin (pose.time * 1.3f + gen.phase + (float) k) * 0.08f + rot * 0.5f;
                    const auto t = juce::AffineTransform::rotation (tilt * 0.3f + swing).translated (hang);
                    bell.applyTransform (t);
                    const auto top = hang;
                    juce::ColourGradient grad (deepColour.withMultipliedAlpha (fade), top.x, top.y,
                                               petalColour.brighter (0.15f).withMultipliedAlpha (fade), top.x + w, top.y + h, false);
                    g.setGradientFill (grad);
                    g.fillPath (bell);
                    g.setColour (deepColour.darker (0.4f).withAlpha (0.3f * fade));
                    g.strokePath (bell, juce::PathStrokeType (0.6f));
                    if (gloss > 0.01f)
                    {
                        g.setColour (juce::Colours::white.withAlpha (0.4f * gloss * fade));
                        g.fillEllipse (juce::Rectangle<float> (w * 0.25f, h * 0.35f).withCentre (hang + juce::Point<float> (-w * 0.3f, h * 0.45f)));
                    }
                }
                break;
            }

            case Species::count:
                break;
        }
    }

    if (rot <= 0.0f)
        return;

    // --- It bleeds: drops crawl down the stem and fall into the pool.
    juce::Random dr ((juce::int64) gen.seed);
    for (int d = 0; d < 3; ++d)
    {
        const float start = 0.1f + 0.12f * (float) d + 0.05f * dr.nextFloat();
        const float progress = smooth ((rot - start) / 0.32f);
        if (progress <= 0.0f || progress >= 1.0f)
            continue;
        const float t = grow * (1.0f - progress);
        juce::Path trail;
        const int from = segments, to = juce::jlimit (0, segments, (int) std::round (t / std::max (0.001f, grow) * segments));
        const float side = (d - 1.0f) * pose.stemWidth * 0.3f;
        for (int i = from; i >= to; --i)
        {
            const auto p = spine[(size_t) i] + juce::Point<float> (side, 0.0f);
            if (i == from) trail.startNewSubPath (p);
            else           trail.lineTo (p);
        }
        g.setColour (clot.withAlpha (0.8f * fade));
        g.strokePath (trail, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const auto drop = bezier (pose, t) + juce::Point<float> (side, 0.0f);
        g.setColour (blood.withMultipliedAlpha (fade));
        juce::Path tear;
        tear.startNewSubPath (drop.x, drop.y - 3.5f);
        tear.quadraticTo (drop.x + 2.2f, drop.y + 0.5f, drop.x, drop.y + 2.0f);
        tear.quadraticTo (drop.x - 2.2f, drop.y + 0.5f, drop.x, drop.y - 3.5f);
        g.fillPath (tear);
        g.setColour (juce::Colours::white.withAlpha (0.5f * fade));
        g.fillEllipse (drop.x - 0.9f, drop.y - 0.8f, 0.9f, 0.9f);
    }

    // --- Mould blooms on the dying head.
    const float mouldy = smooth ((rot - 0.45f) / 0.3f);
    if (mouldy > 0.01f)
    {
        for (int k = 0; k < 7; ++k)
        {
            const float a = dr.nextFloat() * twoPi, d = r * 0.9f * dr.nextFloat();
            const auto c = tip + juce::Point<float> (std::cos (a), std::sin (a)) * d;
            const float s = (1.5f + 2.2f * dr.nextFloat()) * mouldy;
            g.setColour (mould.withAlpha (0.55f * mouldy * fade));
            g.fillEllipse (juce::Rectangle<float> (s * 2.0f, s * 2.0f).withCentre (c));
            for (int f = 0; f < 4; ++f)
            {
                const float fa = (float) f * twoPi / 4.0f + a;
                g.fillEllipse (juce::Rectangle<float> (0.9f, 0.9f).withCentre (c + juce::Point<float> (std::cos (fa), std::sin (fa)) * s * 1.4f));
            }
        }
    }

    // --- Flies.
    if (rot > 0.25f && rot < 0.97f)
    {
        const float present = smooth ((rot - 0.25f) / 0.1f) * fade;
        for (int f = 0; f < 2; ++f)
        {
            const float a = pose.time * (3.1f + (float) f * 1.7f) + gen.phase * (float) (f + 1);
            const auto pos = tip + juce::Point<float> (std::cos (a) * r * 1.9f, std::sin (a * 1.3f) * r * 1.1f - r * 0.6f);
            const float flap = 0.5f + 0.5f * std::sin (pose.time * 90.0f + (float) f);
            g.setColour (juce::Colours::white.withAlpha (0.4f * flap * present));
            g.fillEllipse (pos.x - 3.2f, pos.y - 2.6f, 3.0f, 2.0f);
            g.fillEllipse (pos.x + 0.2f, pos.y - 2.6f, 3.0f, 2.0f);
            g.setColour (juce::Colour (0xff141414).withAlpha (present));
            g.fillEllipse (pos.x - 1.6f, pos.y - 1.0f, 3.2f, 2.2f);
        }
    }
}

//==============================================================================
void paintFromAbove (juce::Graphics& g, const Genome& gen, juce::Point<float> c, float radius,
                     float angle, float open, float glow, float alpha)
{
    if (alpha <= 0.01f || radius <= 0.5f)
        return;
    const float r = radius;
    const auto leafColour = juce::Colour (0xff4f7d2c);

    if (glow > 0.01f)
        drawGlow (g, c, r * 2.2f, gen.petal.withAlpha (0.4f * glow * alpha));

    // A few leaves of the plant peeking out underneath.
    juce::Random lr ((juce::int64) gen.seed);
    for (int k = 0; k < 3; ++k)
    {
        const float a = angle + (float) k * 2.1f + lr.nextFloat() * 0.6f;
        juce::Path leaf;
        leaf.addEllipse (-r * 0.22f, -r * 1.35f, r * 0.44f, r * 1.1f);
        leaf.applyTransform (juce::AffineTransform::rotation (a).translated (c));
        g.setGradientFill (juce::ColourGradient (leafColour.darker (0.3f).withAlpha (alpha), c.x, c.y,
                                                 leafColour.brighter (0.2f).withAlpha (alpha),
                                                 c.x + std::sin (a) * r * 1.3f, c.y - std::cos (a) * r * 1.3f, false));
        g.fillPath (leaf);
    }

    // A soft shadow under the head.
    g.setColour (juce::Colours::black.withAlpha (0.18f * alpha));
    g.fillEllipse (juce::Rectangle<float> (r * 1.9f, r * 1.9f).withCentre (c + juce::Point<float> (r * 0.12f, r * 0.18f)));

    const auto petal = gen.petal, deep = gen.petalDeep;
    auto ring = [&] (int count, float length, float width, float offset, juce::Colour base, juce::Colour tip, Species shape)
    {
        for (int i = 0; i < count; ++i)
        {
            const float a = angle + offset + (float) i * twoPi / (float) count;
            const auto path = petalPath (shape, length * (0.4f + 0.6f * open), width, 0.0f, (float) i);
            fillPetal (g, path, juce::AffineTransform::rotation (a).translated (c), length, width, { base, tip, alpha, 0.0f, true });
        }
    };

    switch (gen.species)
    {
        case Species::daisy:
        case Species::cosmos:
        case Species::poppy:
        {
            for (const auto& pg : gen.petals)
            {
                const auto path = petalPath (gen.species, r * pg.length * (0.4f + 0.6f * open) * 0.55f, r * pg.width * 0.55f, 0.0f, pg.phase);
                fillPetal (g, path, juce::AffineTransform::rotation (angle + pg.angle).translated (c),
                           r * pg.length * 0.55f, r * pg.width * 0.55f, { deep, petal, alpha, 0.0f, true });
            }
            if (gen.species == Species::poppy)
            {
                g.setColour (necrotic.withAlpha (0.75f * alpha));
                g.fillEllipse (juce::Rectangle<float> (r * 0.6f, r * 0.6f).withCentre (c));
                paintDisc (g, c, r * 0.2f, gen.centre, 0.0f, alpha, gen.seed);
            }
            else
            {
                paintDisc (g, c, r * (gen.species == Species::daisy ? 0.26f : 0.2f), gen.centre, 0.0f, alpha, gen.seed);
            }
            break;
        }
        case Species::rose:
        {
            ring (6, r * 0.95f, r * 0.85f, 0.0f, deep, petal, Species::rose);
            ring (5, r * 0.7f, r * 0.7f, 0.5f, deep.darker (0.1f), petal, Species::rose);
            for (int k = 0; k < 4; ++k)
            {
                const float rr = r * (0.42f - 0.09f * (float) k);
                g.setColour (deep.darker (0.12f * (float) k).withMultipliedAlpha (alpha));
                g.fillEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 1.8f).withCentre (c));
                g.setColour (deep.darker (0.6f).withAlpha (0.35f * alpha));
                juce::Path curl;
                curl.addCentredArc (c.x, c.y, rr, rr * 0.9f, angle + (float) k, 0.3f, 3.8f, true);
                g.strokePath (curl, juce::PathStrokeType (0.8f));
            }
            break;
        }
        case Species::tulip:
        case Species::bell:
        {
            // A cup seen from above: lobes around a deep, shadowed throat.
            const int lobes = gen.species == Species::tulip ? 6 : 5;
            ring (lobes, r * 0.85f, r * 0.75f, 0.0f, deep, petal.brighter (0.1f), Species::tulip);
            juce::ColourGradient throat (deep.darker (0.6f).withAlpha (alpha), c.x, c.y,
                                         deep.withAlpha (0.0f), c.x + r * 0.55f, c.y, true);
            g.setGradientFill (throat);
            g.fillEllipse (juce::Rectangle<float> (r * 1.1f, r * 1.1f).withCentre (c));
            g.setColour (juce::Colour (0xffe8d27a).withAlpha (0.8f * alpha));
            for (int k = 0; k < 3; ++k)
                g.fillEllipse (juce::Rectangle<float> (r * 0.08f, r * 0.08f)
                                   .withCentre (c + juce::Point<float> (std::cos (angle + (float) k * 2.1f), std::sin (angle + (float) k * 2.1f)) * r * 0.1f));
            break;
        }
        case Species::count:
            break;
    }
}

//==============================================================================
void Grass::generate (float width, float maxHeight, uint32_t seed, float density)
{
    blades.clear();
    juce::Random r ((juce::int64) seed);
    const int count = (int) (width / 3.0f * density);
    for (int i = 0; i < count; ++i)
    {
        Blade b;
        b.front = r.nextFloat() < 0.45f;
        b.x = r.nextFloat() * width;
        b.height = maxHeight * (b.front ? 0.18f + 0.3f * r.nextFloat() : 0.3f + 0.55f * r.nextFloat());
        b.width = 1.4f + 2.0f * r.nextFloat();
        b.lean = (r.nextFloat() - 0.5f) * 0.6f;
        b.phase = r.nextFloat() * twoPi;
        b.seedHead = ! b.front && r.nextFloat() < 0.05f;

        const bool dry = r.nextFloat() < 0.07f;
        const float hue = dry ? 0.12f + 0.03f * r.nextFloat() : 0.22f + 0.08f * r.nextFloat();
        const float sat = dry ? 0.35f : 0.45f + 0.25f * r.nextFloat();
        float bright = 0.42f + 0.3f * r.nextFloat();
        if (! b.front)
            bright += 0.12f;   // the back rows are hazier and lighter
        b.colour = juce::Colour::fromHSV (hue, sat * (b.front ? 1.0f : 0.8f), std::min (1.0f, bright), 1.0f);
        blades.push_back (b);
    }
    std::sort (blades.begin(), blades.end(), [] (const Blade& a, const Blade& b) { return a.height > b.height; });
}

void Grass::paint (juce::Graphics& g, juce::Point<float> origin, float time, float breath,
                   const std::vector<Ripple>& ripples, bool front, const std::function<float (float)>& coverage) const
{
    const float wind = 0.6f + 0.4f * std::sin (time * 0.37f) * std::sin (time * 0.23f + 1.0f);
    for (const auto& b : blades)
    {
        if (b.front != front)
            continue;
        const float grown = coverage ? coverage (b.x) : 1.0f;
        if (grown < 0.03f)
            continue;

        // Breathing in: blades rise and straighten; the wind holds still for a moment.
        const float h = b.height * (1.0f + 0.2f * breath) * grown;
        float dx = std::sin (time * 1.6f + b.x * 0.015f + b.phase) * h * 0.12f * wind * (1.0f - 0.5f * breath);
        for (const auto& rp : ripples)
        {
            const float dist = std::abs (b.x - rp.x);
            dx += rp.strength * std::exp (-rp.age * 1.3f) * std::sin (dist * 0.045f - rp.age * 7.0f)
                * std::exp (-dist / 260.0f) * h * 0.3f;
        }
        const float lean = b.lean * (1.0f - 0.6f * breath);

        const float x = origin.x + b.x, y = origin.y;
        const juce::Point<float> tip (x + dx + lean * h * 0.45f, y - h);
        juce::Path blade;
        blade.startNewSubPath (x - b.width * 0.5f, y);
        blade.quadraticTo (x + dx * 0.25f + lean * h * 0.15f - b.width * 0.3f, y - h * 0.55f, tip.x, tip.y);
        blade.quadraticTo (x + dx * 0.25f + lean * h * 0.15f + b.width * 0.3f, y - h * 0.5f, x + b.width * 0.5f, y);
        blade.closeSubPath();
        // Shaded from the dark, damp base to a sunlit tip, with a pale midrib.
        const auto colour = b.colour.brighter (0.15f * breath);
        g.setGradientFill (juce::ColourGradient (colour.darker (0.55f), x, y, colour.brighter (0.25f), tip.x, tip.y, false));
        g.fillPath (blade);
        if (b.width > 2.2f)
        {
            juce::Path rib;
            rib.startNewSubPath (x, y);
            rib.quadraticTo (x + dx * 0.25f + lean * h * 0.15f, y - h * 0.52f, tip.x, tip.y);
            g.setColour (colour.brighter (0.5f).withAlpha (0.35f));
            g.strokePath (rib, juce::PathStrokeType (0.5f));
        }

        if (b.seedHead)
        {
            g.setColour (juce::Colour (0xffc8b27a));
            g.fillEllipse (tip.x - 1.4f, tip.y - 5.0f, 2.8f, 7.0f);
        }
    }
}
} // namespace bouquet::ui::botany
