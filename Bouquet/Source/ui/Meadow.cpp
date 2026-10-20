#include "Meadow.h"
#include "../PluginProcessor.h"
#include "BouquetLookAndFeel.h"

using namespace bouquet::ui::botany;

namespace bouquet::ui
{
namespace
{
    const juce::Colour waterColour { 0xffcfeaf5 };
    const juce::Colour tonicColours[] = { juce::Colour (0xff5b9ff0), juce::Colour (0xfff2b33e),
                                          juce::Colour (0xffa58cf0), juce::Colour (0xff4f9e8a) };
    constexpr float gravity = 900.0f;
    constexpr float pourSeconds = 1.5f, pourAt = 1.15f;   // the new flower grows when the pour is nearly done

    // A conical lab flask standing on its base, opening upwards.
    juce::Path flaskShape (juce::Point<float> base, float lift)
    {
        juce::Path p;
        const float x = base.x, y = base.y - lift;
        p.startNewSubPath (x - 5.0f, y - 50.0f);
        p.lineTo (x - 5.0f, y - 34.0f);
        p.lineTo (x - 17.0f, y - 6.0f);
        p.quadraticTo (x - 19.0f, y, x - 12.0f, y);
        p.lineTo (x + 12.0f, y);
        p.quadraticTo (x + 19.0f, y, x + 17.0f, y - 6.0f);
        p.lineTo (x + 5.0f, y - 34.0f);
        p.lineTo (x + 5.0f, y - 50.0f);
        p.closeSubPath();
        return p;
    }

    // A beaker in its own frame: base centre at the origin, a spout at the top right.
    juce::Path beakerShape()
    {
        juce::Path p;
        p.startNewSubPath (-20.0f, -50.0f);
        p.lineTo (-19.0f, -4.0f);
        p.quadraticTo (-19.0f, 0.0f, -15.0f, 0.0f);
        p.lineTo (15.0f, 0.0f);
        p.quadraticTo (19.0f, 0.0f, 19.0f, -4.0f);
        p.lineTo (20.0f, -47.0f);
        p.lineTo (25.0f, -53.0f);
        p.closeSubPath();
        return p;
    }
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

    for (int i = 0; i < (int) Tonic::count; ++i)
    {
        flasks[(size_t) i].tonic = (Tonic) i;
        flasks[(size_t) i].colour = tonicColours[i];
    }
}

void Meadow::resized()
{
    drawnGround = {};
    const int bottom = getHeight() - 52;
    nameField.setBounds (24, bottom, 260, 36);
    plantButton.setBounds (292, bottom - 1, 168, 38);
    closeButton.setBounds (getWidth() - 24 - 170, bottom - 1, 170, 38);

    // The bench: the beaker, then the flasks of tonic in a row.
    const float benchY = (float) getHeight() - 10.0f;
    beakerHome = { 520.0f, benchY };
    if (! carrying && pour.plant < 0)
        beakerPos = beakerHome;
    for (size_t i = 0; i < flasks.size(); ++i)
        flasks[i].base = { 592.0f + 56.0f * (float) i, benchY };
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
    tickLab (dt);
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
    hoveredFlask = flaskAt (e.position);
    hoverBeaker = hoveredFlask < 0 && beakerAt (e.position);
    hovered = hoveredFlask < 0 && ! hoverBeaker ? plantAt (e.position) : -1;
    const bool grabbable = hoverBeaker && pour.plant < 0;
    setMouseCursor (grabbable ? juce::MouseCursor::DraggingHandCursor
                              : (hovered >= 0 || hoveredFlask >= 0) ? juce::MouseCursor::PointingHandCursor
                                                                     : juce::MouseCursor::NormalCursor);
}

void Meadow::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    if (const int f = flaskAt (e.position); f >= 0)
    {
        dose (f);
        return;
    }
    if (pour.plant < 0 && beakerAt (e.position))
    {
        carrying = true;
        grabOffset = beakerPos - e.position;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
}

void Meadow::mouseDrag (const juce::MouseEvent& e)
{
    if (! carrying)
        return;
    const auto previous = beakerPos;
    beakerPos = e.position + grabOffset;
    // It leans the way it is swung, and the water slops about.
    tilt = juce::jlimit (-0.5f, 0.5f, tilt + (beakerPos.x - previous.x) * 0.006f);
    slosh = std::min (1.0f, slosh + std::abs (beakerPos.x - previous.x) * 0.01f);
    target = plantAt (spout() + juce::Point<float> (0.0f, 40.0f));
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
        if (e.getDistanceFromDragStart() < 4)
            emptyBeaker();   // a click on the beaker tips it out
        else if (target >= 0)
            pour = { target, plants[(size_t) target].centre, plants[(size_t) target].radius, 0.0f, false };
        target = -1;
        return;
    }
    if (flaskAt (e.position) >= 0 || beakerAt (e.position))
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
int Meadow::flaskAt (juce::Point<float> pos) const
{
    for (size_t i = 0; i < flasks.size(); ++i)
        if (juce::Rectangle<float> (44.0f, 60.0f).withCentre ({ flasks[i].base.x, flasks[i].base.y - 28.0f })
                .contains (pos))
            return (int) i;
    return -1;
}

bool Meadow::beakerAt (juce::Point<float> pos) const
{
    return beakerShape().getBounds().expanded (8.0f).contains (pos.transformedBy (beakerTransform().inverted()));
}

juce::AffineTransform Meadow::beakerTransform() const
{
    return juce::AffineTransform::scale (lift).rotated (tilt).translated (beakerPos);
}

juce::Point<float> Meadow::spout() const
{
    return juce::Point<float> (25.0f, -53.0f).transformedBy (beakerTransform());
}

juce::Colour Meadow::beakerColour() const
{
    auto c = waterColour;
    for (auto t : mixture)
        c = c.interpolatedWith (tonicColours[(int) t], 0.6f);
    return c;
}

void Meadow::dose (int index)
{
    auto& flask = flasks[(size_t) index];
    if (pour.plant >= 0 || mixture.size() >= 3)
    {
        flask.shake = 1.0f;   // the beaker is full (or busy): the flask rattles
        notes.push_back ({ "the beaker is full", "drag it onto a flower, or click it to tip it out",
                           beakerHome + juce::Point<float> (0.0f, -70.0f) });
        return;
    }
    mixture.push_back (flask.tonic);
    // Never quite the same measure twice.
    level = std::min (1.0f, level + 0.12f + 0.12f * random.nextFloat());
    flask.shake = 0.6f;

    // A splash arcs from the flask's mouth into the beaker.
    const auto from = flask.base + juce::Point<float> (0.0f, -52.0f);
    const auto to = beakerPos + juce::Point<float> (random.nextFloat() * 10.0f - 5.0f, -46.0f);
    const int count = 5 + random.nextInt (4);
    for (int i = 0; i < count; ++i)
    {
        const float time = 0.38f + 0.12f * random.nextFloat();
        const juce::Point<float> vel ((to.x - from.x) / time + random.nextFloat() * 30.0f - 15.0f,
                                      (to.y - from.y - 0.5f * gravity * time * time) / time);
        drops.push_back ({ from, vel, flask.colour, 2.0f + 2.0f * random.nextFloat(), 0.0f, to.y + 4.0f });
    }
}

void Meadow::water (int plant, std::vector<Tonic> tonics)
{
    if (pour.plant >= 0 || ! juce::isPositiveAndBelow (plant, (int) plants.size()))
        return;
    tonics.resize (std::min<size_t> (tonics.size(), 3));
    mixture = std::move (tonics);
    level = shownLevel = 0.35f + 0.18f * (float) mixture.size();
    pour = { plant, plants[(size_t) plant].centre, plants[(size_t) plant].radius, 0.0f, false };
}

void Meadow::emptyBeaker()
{
    if (mixture.empty())
    {
        slosh = 1.0f;
        return;
    }
    // Poured away into the grass; it refills with plain water from the tap.
    for (int i = 0; i < 10; ++i)
        drops.push_back ({ spout(), { 20.0f + 60.0f * random.nextFloat(), -40.0f - 60.0f * random.nextFloat() },
                           beakerColour(), 2.0f + 2.0f * random.nextFloat(), 0.0f, beakerHome.y + 2.0f });
    mixture.clear();
    level = 0.35f;
    slosh = 1.0f;
}

void Meadow::finishPour()
{
    pour.done = true;
    if (! juce::isPositiveAndBelow (pour.plant, (int) plants.size()))
        return;

    const auto source = plants[(size_t) pour.plant].preset;
    const auto at = plants[(size_t) pour.plant].centre;
    const auto watering = processor.waterPreset (source, mixture);

    juce::String given = "water";
    for (auto t : mixture)
        given << " + " << tonicInfo (t).name;
    notes.push_back ({ watering.fizzed ? "it fizzed!" : watering.preset.name,
                       given + ": " + watering.note, at });
    if (watering.fizzed)
        for (int i = 0; i < 18; ++i)
        {
            const float a = random.nextFloat() * juce::MathConstants<float>::twoPi;
            const float speed = 80.0f + 160.0f * random.nextFloat();
            drops.push_back ({ at, { std::cos (a) * speed, std::sin (a) * speed - 120.0f },
                               tonicColours[random.nextInt ((int) Tonic::count)], 2.0f + 2.5f * random.nextFloat(), 0.0f, at.y + 60.0f });
        }

    mixture.clear();
    level = 0.35f;
    pendingGrowth = watering.preset.name;
    rebuild();
    for (auto& p : plants)
        if (p.preset.name == source.name)
            p.wet = 1.0f;
}

void Meadow::tickLab (float dt)
{
    // The beaker goes where it is carried, hovers over the flower it waters, or goes home.
    const bool pouring = pour.plant >= 0;
    if (! carrying)
    {
        auto home = beakerHome;
        float lean = 0.0f;
        if (pouring)
        {
            home = pour.at + juce::Point<float> (-pour.radius * 0.8f - 62.0f, -pour.radius * 0.35f + 6.0f);
            lean = pour.t < pourAt ? 1.15f : 0.3f;
        }
        beakerPos += (home - beakerPos) * (1.0f - std::exp (-dt * 9.0f));
        tilt += (lean - tilt) * (1.0f - std::exp (-dt * 6.0f));
    }
    else
    {
        tilt *= std::exp (-dt * 4.0f);
    }
    slosh *= std::exp (-dt * 2.5f);
    const float liftTarget = carrying || pouring ? 1.45f : 1.0f;
    lift += (liftTarget - lift) * (1.0f - std::exp (-dt * 8.0f));

    if (pouring)
    {
        pour.t += dt;
        // A stream from the spout once it has tipped, the beaker draining as it goes.
        if (pour.t > 0.3f && pour.t < pourAt)
        {
            level = std::max (0.05f, level - dt * 0.4f);
            for (int i = 0; i < 3; ++i)
                drops.push_back ({ spout(), { 30.0f + 30.0f * random.nextFloat(), 10.0f * random.nextFloat() },
                                   beakerColour().withMultipliedSaturation (1.3f), 3.5f + 2.5f * random.nextFloat(), 0.0f,
                                   pour.at.y + (random.nextFloat() - 0.5f) * pour.radius * 0.8f });
            if (juce::isPositiveAndBelow (pour.plant, (int) plants.size()))
                plants[(size_t) pour.plant].wet = 1.0f;
        }
        if (pour.t >= pourAt && ! pour.done)
            finishPour();
        if (pour.t >= pourSeconds)
            pour = {};
    }
    shownLevel += (level - shownLevel) * (1.0f - std::exp (-dt * 5.0f));

    for (auto& f : flasks)
    {
        f.hover += (((&f - flasks.data()) == hoveredFlask ? 1.0f : 0.0f) - f.hover) * (1.0f - std::exp (-dt / 0.1f));
        f.shake = std::max (0.0f, f.shake - dt * 2.5f);
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
            for (int i = 0; i < 3; ++i)
                splashes.push_back ({ { d.pos.x, d.landY }, { (random.nextFloat() - 0.5f) * 120.0f, -60.0f - 80.0f * random.nextFloat() },
                                      d.colour, d.size * 0.5f, 0.0f, d.landY + 30.0f, true });
            slosh = std::min (1.0f, slosh + 0.04f);
        }
    }
    drops.erase (std::remove_if (drops.begin(), drops.end(), [] (const Drop& d) { return d.life > (d.splash ? 0.5f : 3.0f); }),
                 drops.end());
    drops.insert (drops.end(), splashes.begin(), splashes.end());

    for (auto& n : notes)
        n.age += dt;
    notes.erase (std::remove_if (notes.begin(), notes.end(), [] (const Note& n) { return n.age > 4.5f; }), notes.end());
}

void Meadow::paintFlask (juce::Graphics& g, const Flask& f, float time) const
{
    const float lift = 5.0f * f.hover;
    const float rattle = std::sin (time * 60.0f) * 0.12f * f.shake;
    const auto rotate = juce::AffineTransform::rotation (rattle, f.base.x, f.base.y);
    const auto body = flaskShape (f.base, lift).createPathWithRoundedCorners (3.0f);

    if (f.hover > 0.01f)
        drawGlow (g, f.base + juce::Point<float> (0.0f, -24.0f - lift), 40.0f, f.colour.withAlpha (0.35f * f.hover));

    juce::Graphics::ScopedSaveState save (g);
    g.addTransform (rotate);
    g.setColour (juce::Colours::white.withAlpha (0.5f));
    g.fillPath (body);
    {
        // The tonic inside, its surface rocking, bubbles rising through it.
        juce::Graphics::ScopedSaveState inside (g);
        g.reduceClipRegion (body);
        const float surface = f.base.y - lift - 22.0f + std::sin (time * 2.0f + (float) f.tonic) * 1.2f;
        juce::Path liquid;
        liquid.startNewSubPath (f.base.x - 20.0f, surface + std::sin (time * 3.0f) * 1.5f);
        liquid.lineTo (f.base.x + 20.0f, surface - std::sin (time * 3.0f) * 1.5f);
        liquid.lineTo (f.base.x + 20.0f, f.base.y + 2.0f);
        liquid.lineTo (f.base.x - 20.0f, f.base.y + 2.0f);
        liquid.closeSubPath();
        g.setGradientFill (juce::ColourGradient (f.colour.brighter (0.3f), 0.0f, surface, f.colour.darker (0.2f), 0.0f, f.base.y, false));
        g.fillPath (liquid);
        for (int i = 0; i < 4; ++i)
        {
            const float t = std::fmod (time * (0.4f + 0.15f * (float) i) + (float) i * 0.37f + (float) f.tonic * 0.21f, 1.0f);
            const float x = f.base.x + std::sin ((float) i * 2.3f + time) * 8.0f;
            const float y = f.base.y - lift - 3.0f - t * 18.0f;
            g.setColour (juce::Colours::white.withAlpha (0.6f * (1.0f - t)));
            g.drawEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ x, y }), 0.8f);
        }
    }
    // Glass: a rim, a highlight down one side.
    g.setColour (colours::ink.withAlpha (0.35f));
    g.strokePath (body, juce::PathStrokeType (1.2f));
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.drawLine (f.base.x - 3.0f, f.base.y - lift - 47.0f, f.base.x - 3.0f, f.base.y - lift - 36.0f, 1.2f);
    g.setColour (colours::ink.withAlpha (0.45f));
    g.fillRoundedRectangle (juce::Rectangle<float> (14.0f, 4.0f).withCentre ({ f.base.x, f.base.y - lift - 50.0f }), 2.0f);
}

void Meadow::paintBeaker (juce::Graphics& g, float time) const
{
    const auto t = beakerTransform();
    auto glass = beakerShape();
    glass.applyTransform (t);

    if (hoverBeaker || carrying)
        drawGlow (g, juce::Point<float> (0.0f, -26.0f).transformedBy (t), 46.0f * lift,
                  beakerColour().withAlpha (0.4f));

    g.setColour (juce::Colours::white.withAlpha (0.45f));
    g.fillPath (glass);
    {
        // The water stays level as the beaker tips; it slops when swung.
        juce::Graphics::ScopedSaveState inside (g);
        g.reduceClipRegion (glass);
        const auto centre = juce::Point<float> (0.0f, -50.0f * shownLevel).transformedBy (t);
        const float wave = (1.0f + 6.0f * slosh) * std::sin (time * 5.0f);
        juce::Path liquid;
        liquid.startNewSubPath (centre.x - 60.0f, centre.y + wave);
        liquid.lineTo (centre.x + 60.0f, centre.y - wave);
        liquid.lineTo (centre.x + 60.0f, centre.y + 120.0f);
        liquid.lineTo (centre.x - 60.0f, centre.y + 120.0f);
        liquid.closeSubPath();
        const auto c = beakerColour();
        g.setGradientFill (juce::ColourGradient (c.brighter (0.2f).withAlpha (0.85f), 0.0f, centre.y, c.darker (0.15f), 0.0f, centre.y + 50.0f, false));
        g.fillPath (liquid);
        // It bubbles more the more is stirred in.
        for (int i = 0; i < 2 + 3 * (int) mixture.size(); ++i)
        {
            const float k = std::fmod (time * (0.5f + 0.13f * (float) i) + (float) i * 0.29f, 1.0f);
            const auto p = juce::Point<float> (std::sin ((float) i * 1.9f + time * 1.3f) * 13.0f, -3.0f - k * 50.0f * shownLevel).transformedBy (t);
            g.setColour (juce::Colours::white.withAlpha (0.65f * (1.0f - k)));
            g.drawEllipse (juce::Rectangle<float> (3.2f, 3.2f).withCentre (p), 0.8f);
        }
    }
    // Measuring lines, and the glass.
    g.setColour (colours::ink.withAlpha (0.25f));
    for (int i = 1; i <= 4; ++i)
    {
        const float y = -10.0f * (float) i;
        g.drawLine (juce::Line<float> (juce::Point<float> (-19.0f, y).transformedBy (t), juce::Point<float> ((i % 2) ? -12.0f : -9.0f, y).transformedBy (t)), 1.0f);
    }
    g.setColour (colours::ink.withAlpha (0.4f));
    g.strokePath (glass, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawLine (juce::Line<float> (juce::Point<float> (-15.0f, -44.0f).transformedBy (t), juce::Point<float> (-14.5f, -12.0f).transformedBy (t)), 1.4f);

    // A dot for each tonic stirred in.
    for (size_t i = 0; i < mixture.size(); ++i)
    {
        const auto p = juce::Point<float> (8.0f - 7.0f * (float) i, -8.0f).transformedBy (t);
        g.setColour (tonicColours[(int) mixture[i]]);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (p));
    }
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

void Meadow::paintLab (juce::Graphics& g, float time)
{
    for (const auto& f : flasks)
        paintFlask (g, f, time);

    // While idle, a hint above the bench.
    if (! carrying && pour.plant < 0 && hoveredFlask < 0 && ! hoverBeaker && mixture.empty())
    {
        g.setColour (colours::ink.withAlpha (0.45f));
        g.setFont (Fonts::serifItalic (14.0f));
        g.drawText ("water a flower", juce::Rectangle<float> (200.0f, 18.0f).withCentre (beakerHome + juce::Point<float> (90.0f, -70.0f)),
                    juce::Justification::centred, false);
    }

    paintBeaker (g, time);

    for (const auto& d : drops)
    {
        const float fade = d.splash ? 1.0f - d.life / 0.5f : 1.0f;
        const auto shape = juce::Rectangle<float> (d.size, d.size * (d.splash ? 1.0f : 1.4f)).withCentre (d.pos);
        g.setColour (d.colour.withAlpha (0.9f * fade));
        g.fillEllipse (shape);
        g.setColour (d.colour.darker (0.5f).withAlpha (0.5f * fade));
        g.drawEllipse (shape, 0.6f);
        g.setColour (juce::Colours::white.withAlpha (0.7f * fade));
        g.fillEllipse (juce::Rectangle<float> (d.size * 0.35f, d.size * 0.35f).withCentre (d.pos - juce::Point<float> (d.size * 0.2f, d.size * 0.3f)));
    }

    // What the bench is for, under the mouse.
    if (hoveredFlask >= 0)
    {
        const auto& f = flasks[(size_t) hoveredFlask];
        const auto& info = tonicInfo (f.tonic);
        paintLabel (g, info.name, info.effect, f.base + juce::Point<float> (0.0f, -66.0f), f.hover);
    }
    else if (hoverBeaker && ! carrying && pour.plant < 0)
    {
        juce::String title = "water";
        for (auto t : mixture)
            title << " + " << tonicInfo (t).name;
        paintLabel (g, title, mixture.empty() ? "drag onto a flower; click the flasks to stir something in"
                                              : "drag onto a flower to water it, click to tip it out",
                    beakerHome + juce::Point<float> (0.0f, -66.0f), 1.0f);
    }

    // Lab notes rise from what was just watered, and fade.
    for (const auto& n : notes)
    {
        const float alpha = juce::jlimit (0.0f, 1.0f, n.age / 0.25f) * juce::jlimit (0.0f, 1.0f, (4.5f - n.age) / 0.8f);
        paintLabel (g, n.title, n.text, n.at + juce::Point<float> (0.0f, -40.0f - 14.0f * n.age), alpha);
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
    paintLab (g, time);

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
