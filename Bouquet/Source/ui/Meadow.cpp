#include "Meadow.h"
#include "../PluginProcessor.h"
#include "BouquetLookAndFeel.h"

using namespace bouquet::ui::botany;

namespace bouquet::ui
{
namespace
{
    const juce::Colour waterColour { 0xffcfe6ee };
    constexpr float gravity = 900.0f;
    // Pouring: carried over the flower, tipped, then set upright again. The new flower
    // grows when the pour is nearly done.
    constexpr float pourSeconds = 2.0f, tipFrom = 0.35f, pourAt = 1.35f;
    constexpr float pourTilt = 72.0f;
    constexpr float frameSize = 112.0f;   // a bucket pose's frame on screen, standing in the grass
}

Meadow::Meadow (BouquetProcessor& p) : processor (p)
{
    setWantsKeyboardFocus (true);
    setVisible (false);

    plantButton.setFont (Fonts::serifItalic (17.0f));
    plantButton.onClick = [this] { plant(); };
    closeButton.setFont (Fonts::serifItalic (17.0f));
    closeButton.onClick = [this] { close(); };
    addAndMakeVisible (plantButton);
    addAndMakeVisible (closeButton);

    nameField.setFont (Fonts::serifItalic (17.0f));
    nameField.setJustification (juce::Justification::centredLeft);
    nameField.setColour (juce::TextEditor::backgroundColourId, colours::frost.withAlpha (0.7f));
    nameField.setColour (juce::TextEditor::outlineColourId, colours::ink.withAlpha (0.12f));
    nameField.setColour (juce::TextEditor::focusedOutlineColourId, colours::mint.withAlpha (0.8f));
    nameField.setColour (juce::TextEditor::textColourId, colours::ink);
    nameField.setColour (juce::TextEditor::highlightColourId, colours::butter.withAlpha (0.4f));
    nameField.setColour (juce::CaretComponent::caretColourId, colours::ink);
    nameField.setIndents (14, 6);
    nameField.onReturnKey = [this] { plant(); };
    addAndMakeVisible (nameField);

}

void Meadow::resized()
{
    drawnGround = {};
    const int bottom = getHeight() - 52;
    nameField.setBounds (24, bottom, 260, 36);
    plantButton.setBounds (292, bottom - 1, 168, 38);
    closeButton.setBounds (getWidth() - 24 - 170, bottom - 1, 170, 38);

    // The bucket stands in the grass beside the planting bar.
    bucketHome = { 540.0f, (float) getHeight() - 8.0f };
    if (! carrying && pour.plant < 0)
        bucketPos = bucketHome;
    rebuild();
}

void Meadow::open()
{
    processor.presets.rescan();
    rebuild();
    for (size_t i = 0; i < plants.size(); ++i)
    {
        plants[i].appear = 0.0f;
        plants[i].delay = 0.03f * (float) i;
    }
    nameField.setText ({}, false);
    nameField.setTextToShowWhenEmpty (processor.presets.inventName (capturePreset (processor.apvts, {}).seed()),
                                      colours::ink.withAlpha (0.35f));
    opening = true;
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
}

void Meadow::close()
{
    opening = false;
    hovered = -1;
}

void Meadow::rebuild()
{
    std::vector<Preset> all (PresetLibrary::factory().begin(), PresetLibrary::factory().end());
    for (auto& p : processor.presets.user())
        all.push_back (p);

    std::vector<Plant> previous;
    previous.swap (plants);

    // Scatter the flowers over the lawn: a jittered grid, so they never crowd.
    const int count = (int) all.size();
    const auto area = getLocalBounds().toFloat().withTrimmedBottom (80.0f).reduced (60.0f, 50.0f);
    const int cols = std::max (1, (int) std::ceil (std::sqrt ((float) count * area.getWidth() / std::max (1.0f, area.getHeight()))));
    const int rows = std::max (1, (count + cols - 1) / cols);
    const float cw = area.getWidth() / (float) cols, ch = area.getHeight() / (float) rows;
    for (int i = 0; i < count; ++i)
    {
        Plant pl;
        pl.preset = all[(size_t) i];
        pl.genome = genomeForPreset (pl.preset);
        juce::Random r ((juce::int64) pl.genome.seed);
        const int row = i / cols, col = i % cols;
        const float stagger = (row % 2) ? 0.25f : -0.25f;
        pl.centre = { area.getX() + ((float) col + 0.5f + stagger + (r.nextFloat() - 0.5f) * 0.4f) * cw,
                      area.getY() + ((float) row + 0.5f + (r.nextFloat() - 0.5f) * 0.35f) * ch };
        pl.centre.x = juce::jlimit (area.getX(), area.getRight(), pl.centre.x);
        pl.radius = std::min (cw, ch) * (0.3f + 0.08f * r.nextFloat());
        pl.angle = r.nextFloat() * juce::MathConstants<float>::twoPi;
        pl.variant = (int) (pl.genome.seed % real::FlowerSprites::numVariants);
        pl.appear = 1.0f;

        // Keep the growth state of plants that were already there.
        for (auto& old : previous)
            if (old.preset.name == pl.preset.name)
            {
                pl.appear = old.appear;
                pl.delay = old.delay;
                pl.wet = old.wet;
            }
        if (pl.preset.name == pendingGrowth)
        {
            pl.appear = 0.0f;
            pl.delay = 0.2f;
        }
        plants.push_back (std::move (pl));
    }
    pendingGrowth = {};
}

void Meadow::plant()
{
    auto name = nameField.getText().trim();
    if (name.isEmpty())
        name = nameField.getTextToShowWhenEmpty();
    if (processor.presets.find (name) != nullptr)
        name = processor.presets.inventName ((uint32_t) name.hashCode() + 1u);

    processor.plantPreset (name);
    pendingGrowth = name;
    rebuild();
    nameField.setText ({}, false);
    nameField.setTextToShowWhenEmpty (processor.presets.inventName ((uint32_t) juce::Time::getMillisecondCounter()),
                                      colours::ink.withAlpha (0.35f));
}

//==============================================================================
void Meadow::tick (float dt)
{
    visibility += ((opening ? 1.0f : 0.0f) - visibility) * (1.0f - std::exp (-dt / 0.18f));
    if (! opening && visibility < 0.01f)
    {
        visibility = 0.0f;
        if (isVisible())
            setVisible (false);
        return;
    }
    setAlpha (visibility);

    for (size_t i = 0; i < plants.size(); ++i)
    {
        auto& p = plants[i];
        if (p.delay > 0.0f) p.delay -= dt;
        else                p.appear = std::min (1.0f, p.appear + dt / 1.1f);
        const float lit = (int) i == hovered ? 1.0f : 0.0f;
        p.hover += (lit - p.hover) * (1.0f - std::exp (-dt / 0.12f));
        p.wet = std::max (0.0f, p.wet - dt / 4.0f);
    }
    tickBucket (dt);
    repaint();
}

float Meadow::radiusOf (const Plant& p) const
{
    // Grows in with a little overshoot; a hovered flower lifts towards you.
    const float g = p.appear;
    const float grow = g <= 0.0f ? 0.0f : 1.0f - std::pow (1.0f - g, 3.0f) + 0.08f * std::sin (g * juce::MathConstants<float>::pi);
    return p.radius * grow * (1.0f + 0.12f * p.hover);
}

int Meadow::plantAt (juce::Point<float> pos) const
{
    int best = -1;
    float bestDistance = 1.0e9f;
    for (size_t i = 0; i < plants.size(); ++i)
    {
        const auto& p = plants[i];
        const float d = p.centre.getDistanceFrom (pos);
        if (d < p.radius * 1.1f && d < bestDistance)
        {
            bestDistance = d;
            best = (int) i;
        }
    }
    return best;
}

void Meadow::mouseMove (const juce::MouseEvent& e)
{
    hoverBucket = pour.plant < 0 && bucketAt (e.position);
    hovered = hoverBucket ? -1 : plantAt (e.position);
    setMouseCursor (hoverBucket ? juce::MouseCursor::DraggingHandCursor
                                : hovered >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void Meadow::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && pour.plant < 0 && bucketAt (e.position))
    {
        carrying = true;
        grabOffset = bucketPos - e.position;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
}

void Meadow::mouseDrag (const juce::MouseEvent& e)
{
    if (! carrying)
        return;
    const auto previous = bucketPos;
    bucketPos = e.position + grabOffset;
    // It swings the way it is carried.
    sway = juce::jlimit (-0.35f, 0.35f, sway - (bucketPos.x - previous.x) * 0.004f);
    target = plantAt (lipFor (bucketPos) + juce::Point<float> (0.0f, 50.0f));
    hovered = target;
}

void Meadow::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
}

void Meadow::mouseUp (const juce::MouseEvent& e)
{
    if (carrying)
    {
        carrying = false;
        if (target >= 0)
            water (target);
        target = -1;
        return;
    }
    if (bucketAt (e.position))
        return;

    const int index = plantAt (e.position);
    if (index < 0 || ! e.mouseWasClicked())
        return;

    const auto preset = plants[(size_t) index].preset;
    if (e.mods.isPopupMenu())
    {
        if (preset.factory)
            return;
        juce::PopupMenu menu;
        menu.addItem (1, "uproot " + preset.name);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this, preset] (int result)
                            {
                                if (result == 1)
                                {
                                    processor.presets.remove (preset);
                                    rebuild();
                                }
                            });
        return;
    }

    processor.applyPreset (preset);
    juce::Timer::callAfterDelay (450, [safe = juce::Component::SafePointer<Meadow> (this)]
    {
        if (safe != nullptr)
            safe->close();
    });
}

bool Meadow::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (carrying)
        {
            carrying = false;
            target = -1;
            return true;
        }
        close();
        return true;
    }
    return false;
}

//==============================================================================
float Meadow::bucketScale() const
{
    return frameSize * lift / 256.0f;
}

juce::Point<float> Meadow::lipFor (juce::Point<float> base) const
{
    const auto& poses = sprites->bucket();
    if (poses.empty())
        return base + juce::Point<float> (24.0f, -40.0f) * lift;
    return base + (poses.front().lip - poses.front().base) * (frameSize * lift / (float) poses.front().image.getWidth());
}

juce::Point<float> Meadow::lip() const
{
    return pour.plant >= 0 ? pourLip : lipFor (bucketPos);
}

bool Meadow::bucketAt (juce::Point<float> pos) const
{
    const float s = frameSize * lift;
    return juce::Rectangle<float> (s * 0.62f, s * 0.75f).withCentre (bucketPos + juce::Point<float> (0.0f, -s * 0.33f)).contains (pos);
}

void Meadow::water (int plant)
{
    if (pour.plant >= 0 || ! juce::isPositiveAndBelow (plant, (int) plants.size()))
        return;
    pourLip = lipFor (bucketPos);
    pour = { plant, plants[(size_t) plant].centre, plants[(size_t) plant].radius, 0.0f, false };
}

void Meadow::finishPour()
{
    pour.done = true;
    if (! juce::isPositiveAndBelow (pour.plant, (int) plants.size()))
        return;

    // What the water brings out of the flower is a surprise: plain, or one or two tonics.
    std::vector<Tonic> tonics;
    const float roll = random.nextFloat();
    for (int i = 0, n = roll < 0.3f ? 0 : roll < 0.75f ? 1 : 2; i < n; ++i)
        tonics.push_back ((Tonic) random.nextInt ((int) Tonic::count));

    const auto source = plants[(size_t) pour.plant].preset;
    const auto at = plants[(size_t) pour.plant].centre;
    const auto watering = processor.waterPreset (source, tonics);
    notes.push_back ({ watering.preset.name, watering.note, at });
    if (watering.fizzed)
    {
        const juce::Colour sparks[] = { colours::butter, colours::lilac, colours::mint, waterColour };
        for (int i = 0; i < 18; ++i)
        {
            const float a = random.nextFloat() * juce::MathConstants<float>::twoPi;
            const float speed = 80.0f + 160.0f * random.nextFloat();
            drops.push_back ({ at, { std::cos (a) * speed, std::sin (a) * speed - 120.0f },
                               sparks[random.nextInt (4)], 2.0f + 2.5f * random.nextFloat(), 0.0f, at.y + 60.0f });
        }
    }

    pendingGrowth = watering.preset.name;
    rebuild();
    for (auto& p : plants)
        if (p.preset.name == source.name)
            p.wet = 1.0f;
}

void Meadow::tickBucket (float dt)
{
    const auto ease = [dt] (float speed) { return 1.0f - std::exp (-dt * speed); };
    const bool pouring = pour.plant >= 0;
    lift += ((carrying || pouring ? 1.5f : 1.0f) - lift) * ease (8.0f);
    hover += ((hoverBucket || carrying ? 1.0f : 0.0f) - hover) * ease (10.0f);
    sway *= std::exp (-dt * 3.0f);

    if (pouring)
    {
        // Held over the flower by its lip, tipped forward, then set upright again.
        pour.t += dt;
        const auto over = pour.at + juce::Point<float> (-pour.radius * 0.25f - 10.0f, -pour.radius * 0.9f - 30.0f);
        pourLip += (over - pourLip) * ease (7.0f);
        const float tipT = juce::jlimit (0.0f, 1.0f, (pour.t - tipFrom) / 0.45f);
        const float backT = juce::jlimit (0.0f, 1.0f, (pour.t - pourAt) / 0.5f);
        const float tipping = tipT * tipT * (3.0f - 2.0f * tipT) * (1.0f - backT * backT * (3.0f - 2.0f * backT));
        tilt = pourTilt * tipping;

        // Water spills over the lip once it is tipped far enough.
        if (tilt > 20.0f && pour.t < pourAt + 0.15f)
        {
            const int count = 3 + (int) (tilt / 12.0f);
            for (int i = 0; i < count; ++i)
                drops.push_back ({ pourLip + juce::Point<float> (random.nextFloat() * 6.0f - 3.0f, 0.0f),
                                   { 25.0f + 45.0f * random.nextFloat(), 10.0f + 20.0f * random.nextFloat() },
                                   waterColour, (3.0f + 3.0f * random.nextFloat()) * lift * 0.8f, 0.0f,
                                   pour.at.y + (random.nextFloat() - 0.5f) * pour.radius * 0.7f });
            if (juce::isPositiveAndBelow (pour.plant, (int) plants.size()) && ! pour.done)
                plants[(size_t) pour.plant].wet = 1.0f;
        }
        if (pour.t >= pourAt && ! pour.done)
            finishPour();
        if (pour.t >= pourSeconds)
        {
            // Upright again: carry on from where it stands, back to the grass.
            bucketPos = pourLip - (lipFor ({}) - juce::Point<float>());
            pour = {};
            tilt = 0.0f;
        }
    }
    else if (! carrying)
    {
        bucketPos += (bucketHome - bucketPos) * ease (7.0f);
    }

    // Drops fall until they land, then burst into a few smaller ones that fade.
    std::vector<Drop> splashes;
    for (auto& d : drops)
    {
        d.life += dt;
        d.vel.y += gravity * dt;
        d.pos += d.vel * dt;
        if (! d.splash && d.vel.y > 0.0f && d.pos.y >= d.landY)
        {
            d.life = 10.0f;
            for (int i = 0; i < 2; ++i)
                splashes.push_back ({ { d.pos.x, d.landY }, { (random.nextFloat() - 0.5f) * 120.0f, -50.0f - 80.0f * random.nextFloat() },
                                      d.colour, d.size * 0.5f, 0.0f, d.landY + 30.0f, true });
        }
    }
    drops.erase (std::remove_if (drops.begin(), drops.end(), [] (const Drop& d) { return d.life > (d.splash ? 0.5f : 3.0f); }),
                 drops.end());
    drops.insert (drops.end(), splashes.begin(), splashes.end());

    for (auto& n : notes)
        n.age += dt;
    notes.erase (std::remove_if (notes.begin(), notes.end(), [] (const Note& n) { return n.age > 4.5f; }), notes.end());
}

void Meadow::paintBucket (juce::Graphics& g, float time) const
{
    const auto& poses = sprites->bucket();
    const bool pouring = pour.plant >= 0;
    const float s = frameSize * lift;

    // Its shadow on the grass, softer and further off when it is lifted.
    if (! pouring)
    {
        const float up = (lift - 1.0f) * 2.0f;
        g.setColour (juce::Colours::black.withAlpha (0.28f - 0.12f * up));
        g.fillEllipse (juce::Rectangle<float> (s * 0.5f, s * 0.16f).withCentre (bucketPos + juce::Point<float> (s * 0.06f + up * 10.0f, -s * 0.03f + up * 14.0f)));
    }
    if (hover > 0.01f)
        drawGlow (g, bucketPos + juce::Point<float> (0.0f, -s * 0.35f), s * 0.6f, waterColour.withAlpha (0.45f * hover));

    if (poses.empty())
    {
        // Without the rendered pail: a plain grey one.
        juce::Path pail;
        const auto b = pouring ? pourLip + juce::Point<float> (-s * 0.25f, s * 0.4f) : bucketPos;
        pail.addQuadrilateral (b.x - s * 0.2f, b.y, b.x + s * 0.2f, b.y, b.x + s * 0.26f, b.y - s * 0.5f, b.x - s * 0.26f, b.y - s * 0.5f);
        g.setColour (juce::Colour (0xffa9b0b4));
        g.fillPath (pail);
        g.setColour (juce::Colour (0xff6b7378));
        g.fillEllipse (b.x - s * 0.26f, b.y - s * 0.56f, s * 0.52f, s * 0.12f);
        return;
    }

    // The two rendered poses either side of the tilt, the nearer drawn over the other.
    size_t a = 0;
    while (a + 1 < poses.size() && poses[a + 1].tilt <= tilt)
        ++a;
    const size_t b = std::min (a + 1, poses.size() - 1);
    const float span = poses[b].tilt - poses[a].tilt;
    const float f = span > 0.0f ? juce::jlimit (0.0f, 1.0f, (tilt - poses[a].tilt) / span) : 0.0f;

    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    auto draw = [&] (const real::FlowerSprites::BucketPose& pose, float alpha)
    {
        if (alpha <= 0.01f)
            return;
        const float scale = s / (float) pose.image.getWidth();
        const auto anchor = pouring ? pose.lip : pose.base;
        const auto at = pouring ? pourLip : bucketPos;
        // A little bob as it is carried.
        const float bob = carrying ? std::sin (time * 9.0f) * 1.5f : 0.0f;
        g.setOpacity (alpha);
        g.drawImageTransformed (pose.image, juce::AffineTransform::translation (-anchor)
                                                .scaled (scale)
                                                .rotated (pouring ? 0.0f : sway)
                                                .translated (at + juce::Point<float> (0.0f, bob)));
    };
    draw (poses[a], 1.0f);
    if (b != a)
        draw (poses[b], f);
    g.setOpacity (1.0f);
}

void Meadow::paintLabel (juce::Graphics& g, const juce::String& title, const juce::String& text,
                         juce::Point<float> above, float alpha) const
{
    if (alpha <= 0.01f)
        return;
    const auto titleFont = Fonts::serifItalic (18.0f);
    const auto textFont = Fonts::sansMedium (11.0f, 0.02f);
    const float w = std::max (juce::GlyphArrangement::getStringWidth (titleFont, title),
                              juce::GlyphArrangement::getStringWidth (textFont, text)) + 28.0f;
    const float h = text.isEmpty() ? 32.0f : 48.0f;
    auto box = juce::Rectangle<float> (w, h).withCentre ({ above.x, above.y - h * 0.5f });
    box = box.constrainedWithin (getLocalBounds().toFloat().reduced (8.0f));
    g.setColour (colours::frost.withAlpha (0.9f * alpha));
    g.fillRoundedRectangle (box, 14.0f);
    g.setColour (colours::ink.withAlpha (0.1f * alpha));
    g.drawRoundedRectangle (box, 14.0f, 1.0f);
    g.setColour (colours::ink.withAlpha (alpha));
    g.setFont (titleFont);
    g.drawText (title, box.withHeight (32.0f), juce::Justification::centred, false);
    if (text.isNotEmpty())
    {
        g.setColour (colours::ink.withAlpha (0.6f * alpha));
        g.setFont (textFont);
        g.drawText (text, box.withTrimmedTop (27.0f).withHeight (16.0f), juce::Justification::centred, false);
    }
}


void Meadow::paintGround (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const bool photoreal = processor.photorealFlowers() && sprites->available();
    if (photoreal)
    {
        const auto lawn = sprites->ground();
        if (lawn.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.setOpacity (1.0f);
            g.drawImage (lawn, bounds, juce::RectanglePlacement::fillDestination);
            // Bright midday: a warm wash of sunlight and soft light falling off at the edges.
            g.setColour (juce::Colour (0xfffff6dc).withAlpha (0.16f));
            g.fillAll();
            drawGlow (g, { bounds.getWidth() * 0.78f, bounds.getHeight() * 0.2f }, bounds.getWidth() * 0.6f,
                      juce::Colour (0xfffff2c4).withAlpha (0.28f));
            return;
        }
    }

    // The illustrated lawn: thousands of short strokes of grass seen from above.
    if (! drawnGround.isValid() || drawnGround.getWidth() != getWidth() || drawnGround.getHeight() != getHeight())
    {
        drawnGround = juce::Image (juce::Image::RGB, std::max (1, getWidth()), std::max (1, getHeight()), true);
        juce::Graphics lg (drawnGround);
        lg.setGradientFill (juce::ColourGradient (juce::Colour (0xffa9cf86), 0.0f, 0.0f, juce::Colour (0xff86b565),
                                                  bounds.getWidth(), bounds.getHeight(), false));
        lg.fillAll();
        juce::Random r (0xa11);
        const int blades = (int) (bounds.getWidth() * bounds.getHeight() / 26.0f);
        for (int i = 0; i < blades; ++i)
        {
            const juce::Point<float> p (r.nextFloat() * bounds.getWidth(), r.nextFloat() * bounds.getHeight());
            const float a = r.nextFloat() * juce::MathConstants<float>::twoPi;
            const float len = 4.0f + 7.0f * r.nextFloat();
            const auto colour = juce::Colour::fromHSV (0.24f + 0.07f * r.nextFloat(), 0.45f + 0.25f * r.nextFloat(),
                                                       0.45f + 0.35f * r.nextFloat(), 0.8f);
            lg.setColour (colour);
            lg.drawLine (p.x, p.y, p.x + std::cos (a) * len, p.y + std::sin (a) * len, 1.0f + r.nextFloat());
        }
    }
    g.drawImageAt (drawnGround, 0, 0);
}

void Meadow::paint (juce::Graphics& g)
{
    const float time = (float) clock();
    paintGround (g);

    const bool photoreal = processor.photorealFlowers() && sprites->available();
    const auto current = processor.getCurrentPresetName();
    for (size_t i = 0; i < plants.size(); ++i)
    {
        const auto& p = plants[i];
        const float radius = radiusOf (p);
        if (radius <= 0.5f)
            continue;
        // Nodding in the breeze, turning a little.
        const float angle = p.angle + std::sin (time * 0.6f + p.genome.phase) * 0.06f + p.hover * 0.15f;
        const auto centre = p.centre + juce::Point<float> (std::sin (time * 0.8f + p.genome.phase), std::cos (time * 0.7f + p.genome.phase))
                                           * (radius * 0.03f);
        const float glow = std::max ({ p.hover, p.preset.name == current ? 0.55f : 0.0f, p.wet * 0.8f });

        const auto image = photoreal ? sprites->top (p.genome.species, p.variant, real::FlowerSprites::nearestColour (p.genome.petal))
                                     : juce::Image();
        if (image.isValid())
        {
            if (glow > 0.01f)
                drawGlow (g, centre, radius * 2.0f, p.genome.petal.withAlpha (0.45f * glow));
            // The sprite's flower spans about 80% of its frame.
            const float scale = radius * 2.0f / ((float) image.getWidth() * 0.8f);
            // Shadow cast by the high sun.
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.fillEllipse (juce::Rectangle<float> (radius * 1.7f, radius * 1.7f).withCentre (centre + juce::Point<float> (radius * 0.14f, radius * 0.2f)));
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.setOpacity (1.0f);   // images take the current colour's opacity
            g.drawImageTransformed (image, juce::AffineTransform::translation (-(float) image.getWidth() * 0.5f, -(float) image.getHeight() * 0.5f)
                                               .rotated (angle)
                                               .scaled (scale)
                                               .translated (centre));
        }
        else
        {
            paintFromAbove (g, p.genome, centre, radius, angle, juce::jlimit (0.0f, 1.0f, p.appear * 1.4f - 0.2f), glow);
        }
    }

    // A frosted strip for planting and leaving.
    g.setColour (colours::frost.withAlpha (0.72f));
    g.fillRect (getLocalBounds().removeFromBottom (72));
    paintBucket (g, time);

    for (const auto& d : drops)
    {
        const float fade = d.splash ? 1.0f - d.life / 0.5f : 1.0f;
        const auto shape = juce::Rectangle<float> (d.size, d.size * (d.splash ? 1.0f : 1.5f)).withCentre (d.pos);
        g.setColour (d.colour.withAlpha (0.75f * fade));
        g.fillEllipse (shape);
        g.setColour (juce::Colours::white.withAlpha (0.8f * fade));
        g.fillEllipse (juce::Rectangle<float> (d.size * 0.35f, d.size * 0.35f).withCentre (d.pos - juce::Point<float> (d.size * 0.15f, d.size * 0.3f)));
    }

    if (hoverBucket && ! carrying)
        paintLabel (g, "a bucket of water", "drag it onto a flower: every watering grows something different",
                    bucketPos + juce::Point<float> (0.0f, -frameSize * lift * 0.8f), hover);

    // What just grew, rising from the flower that was watered, and fading.
    for (const auto& n : notes)
    {
        const float alpha = juce::jlimit (0.0f, 1.0f, n.age / 0.25f) * juce::jlimit (0.0f, 1.0f, (4.5f - n.age) / 0.8f);
        paintLabel (g, n.title, n.text, n.at + juce::Point<float> (0.0f, -40.0f - 14.0f * n.age), alpha);
    }

    // The name of the flower under the mouse, and only that one.
    if (juce::isPositiveAndBelow (hovered, (int) plants.size()))
    {
        const auto& p = plants[(size_t) hovered];
        const auto font = Fonts::serifItalic (20.0f);
        const float tw = juce::GlyphArrangement::getStringWidth (font, p.preset.name) + 28.0f;
        auto label = juce::Rectangle<float> (tw, 32.0f).withCentre ({ p.centre.x, p.centre.y - radiusOf (p) - 26.0f });
        label = label.constrainedWithin (getLocalBounds().toFloat().reduced (8.0f));
        g.setColour (colours::frost.withAlpha (0.88f * p.hover));
        g.fillRoundedRectangle (label, 16.0f);
        g.setColour (colours::ink.withAlpha (0.1f * p.hover));
        g.drawRoundedRectangle (label, 16.0f, 1.0f);
        g.setFont (font);
        g.setColour (colours::ink.withAlpha (p.hover));
        g.drawText (p.preset.name, label, juce::Justification::centred, false);
    }
}
} // namespace bouquet::ui
