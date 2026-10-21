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

    // The tools stand in a row on the planting strip, beside the bucket.
    for (size_t i = 0; i < tools.size(); ++i)
    {
        auto& t = tools[i];
        t.kind = (Tending) i;
        t.home = { 650.0f + 68.0f * (float) i, (float) getHeight() - 38.0f };
        if (! t.carrying && use.tool != (int) i)
            t.pos = t.home;
    }
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
        pl.bigness = pl.preset.get (traits::size);
        pl.glow = pl.preset.get (traits::glow);
        pl.glitch = pl.preset.get (traits::glitch);
        // Pruned flowers are small, fed ones big.
        pl.radius = std::min (cw, ch) * (0.3f + 0.08f * r.nextFloat()) * (1.0f + 0.4f * pl.bigness);
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
                pl.earth = old.earth;
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
        p.earth = std::max (0.0f, p.earth - dt / 12.0f);
    }
    tickBucket (dt);
    tickTools (dt);
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
    hoveredTool = pour.plant < 0 && use.tool < 0 ? toolAt (e.position) : -1;
    hoverBucket = pour.plant < 0 && hoveredTool < 0 && bucketAt (e.position);
    hovered = hoverBucket || hoveredTool >= 0 ? -1 : plantAt (e.position);
    setMouseCursor (hoverBucket || hoveredTool >= 0 ? juce::MouseCursor::DraggingHandCursor
                                : hovered >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void Meadow::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu() && pour.plant < 0 && use.tool < 0)
        if (const int i = toolAt (e.position); i >= 0)
        {
            auto& t = tools[(size_t) i];
            t.carrying = true;
            t.grab = t.pos - e.position;
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            return;
        }
    if (! e.mods.isPopupMenu() && pour.plant < 0 && bucketAt (e.position))
    {
        carrying = true;
        grabOffset = bucketPos - e.position;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }
}

void Meadow::mouseDrag (const juce::MouseEvent& e)
{
    for (auto& t : tools)
        if (t.carrying)
        {
            const auto previous = t.pos;
            t.pos = e.position + t.grab;
            t.sway = juce::jlimit (-0.35f, 0.35f, t.sway - (t.pos.x - previous.x) * 0.004f);
            hovered = plantAt (t.pos);
            return;
        }
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
    for (size_t i = 0; i < tools.size(); ++i)
        if (tools[i].carrying)
        {
            tools[i].carrying = false;
            const int target = plantAt (tools[i].pos);
            hovered = -1;
            if (target >= 0)
                useTool ((int) i, target);
            return;
        }
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
        for (auto& t : tools)
            if (t.carrying)
            {
                t.carrying = false;
                hovered = -1;
                return true;
            }
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
// The shelf of tools
int Meadow::toolAt (juce::Point<float> pos) const
{
    for (size_t i = 0; i < tools.size(); ++i)
        if (juce::Rectangle<float> (60.0f * tools[i].lift, 64.0f * tools[i].lift).withCentre (tools[i].pos).contains (pos))
            return (int) i;
    return -1;
}

void Meadow::useTool (int tool, int plant)
{
    if (pour.plant >= 0 || use.tool >= 0 || ! juce::isPositiveAndBelow (plant, (int) plants.size()))
        return;
    const auto& p = plants[(size_t) plant];
    use = {};
    use.tool = tool;
    use.plant = plant;
    use.at = p.centre;
    use.radius = p.radius;
    use.held = p.centre + juce::Point<float> (p.radius * 0.35f, -p.radius * 0.8f - 30.0f);
}

void Meadow::finishUse()
{
    use.done = true;
    if (! juce::isPositiveAndBelow (use.plant, (int) plants.size()))
        return;

    const auto tending = (Tending) use.tool;
    const auto source = plants[(size_t) use.plant].preset;
    const auto at = plants[(size_t) use.plant].centre;
    const auto tended = processor.tendPreset (source, tending);
    notes.push_back ({ tended.preset.name, tended.note, at });

    if (tended.fizzed)
    {
        const juce::Colour sparks[] = { colours::butter, colours::lilac, colours::mint, waterColour };
        for (int i = 0; i < 14; ++i)
        {
            const float a = random.nextFloat() * juce::MathConstants<float>::twoPi;
            const float speed = 80.0f + 160.0f * random.nextFloat();
            drops.push_back ({ at, { std::cos (a) * speed, std::sin (a) * speed - 120.0f },
                               sparks[random.nextInt (4)], 2.0f + 2.5f * random.nextFloat(), 0.0f, at.y + 60.0f });
        }
    }

    pendingGrowth = tended.preset.name;
    rebuild();
    for (auto& p : plants)
        if (p.preset.name == source.name)
        {
            if (tending == Tending::soil)
                p.earth = 1.0f;
            else
                p.wet = 0.6f;
        }
}

void Meadow::tickTools (float dt)
{
    const auto ease = [dt] (float speed) { return 1.0f - std::exp (-dt * speed); };
    const float groundY = (float) getHeight();

    for (size_t i = 0; i < tools.size(); ++i)
    {
        auto& t = tools[i];
        const bool busy = use.tool == (int) i;
        t.lift += ((t.carrying || busy ? 1.4f : 1.0f) - t.lift) * ease (8.0f);
        t.hover += (((int) i == hoveredTool || t.carrying ? 1.0f : 0.0f) - t.hover) * ease (10.0f);
        t.sway *= std::exp (-dt * 3.0f);
        if (busy)
            t.pos += (use.held - t.pos) * ease (7.0f);
        else if (! t.carrying)
            t.pos += (t.home - t.pos) * ease (7.0f);
    }

    if (use.tool >= 0)
    {
        use.t += dt;
        const auto kind = (Tending) use.tool;
        const float tPour = juce::jlimit (0.0f, 1.0f, (use.t - 0.25f) / 0.3f);
        const bool active = use.t > 0.55f && use.t < 1.75f;

        auto burst = [&] (float rate, auto&& make)
        {
            use.emit += rate * dt;
            for (; use.emit >= 1.0f; use.emit -= 1.0f)
                make();
        };
        const auto tip = tools[(size_t) use.tool].pos + juce::Point<float> (0.0f, 12.0f);
        const auto landing = [&] { return use.at.y + (random.nextFloat() - 0.4f) * use.radius * 0.9f; };

        if (active && kind == Tending::soil)
            burst (80.0f, [&]
            {
                const float shade = random.nextFloat();
                bits.push_back ({ tip + juce::Point<float> (random.nextFloat() * 16.0f - 8.0f, 0.0f),
                                  { random.nextFloat() * 50.0f - 20.0f, 30.0f + 60.0f * random.nextFloat() },
                                  juce::Colour (0xff4a3426).interpolatedWith (juce::Colour (0xff8a6a4a), shade * 0.7f),
                                  2.0f + 3.0f * random.nextFloat(), 1.4f, 1.4f, 0.0f, 0.0f, landing(), 0.0f, false, kind });
            });
        else if (active && kind == Tending::feed)
            burst (50.0f, [&]
            {
                bits.push_back ({ tip + juce::Point<float> (8.0f, 0.0f), { 15.0f + 25.0f * random.nextFloat(), 20.0f },
                                  juce::Colour (0xff7fe3a1), 3.0f + 2.5f * random.nextFloat(), 1.4f, 1.4f, 0.0f, 0.0f, landing(), 0.0f, false, kind });
            });
        else if (kind == Tending::prune)
        {
            // Each snip of the blades drops a few cut petals and a leaf.
            const float cycle = use.t * 11.0f;
            if (use.t > 0.5f && use.t < 1.7f && (int) cycle != (int) ((use.t - dt) * 11.0f))
            {
                const auto petal = plants[(size_t) juce::jlimit (0, (int) plants.size() - 1, use.plant)].genome.petal;
                for (int k = 0; k < 4; ++k)
                {
                    const float a = random.nextFloat() * juce::MathConstants<float>::twoPi;
                    const bool leaf = k == 3;
                    bits.push_back ({ use.at + juce::Point<float> (std::cos (a), std::sin (a)) * use.radius * 0.6f,
                                      { (random.nextFloat() - 0.5f) * 90.0f, -70.0f - 50.0f * random.nextFloat() },
                                      leaf ? juce::Colour (0xff7bae52) : petal, 5.0f + 3.0f * random.nextFloat(), 1.6f, 1.6f,
                                      random.nextFloat() * 6.0f, (random.nextFloat() - 0.5f) * 10.0f, use.at.y + use.radius * 1.3f, 0.0f, false, kind });
                }
            }
        }
        else if ((kind == Tending::lantern || kind == Tending::crickets) && use.t >= 0.55f && use.t - dt < 0.55f)
        {
            const int n = kind == Tending::lantern ? 12 : 8;
            for (int k = 0; k < n; ++k)
            {
                const float life = kind == Tending::lantern ? 3.2f : 2.6f;
                bits.push_back ({ tools[(size_t) use.tool].pos, { (random.nextFloat() - 0.5f) * 160.0f, -140.0f - 60.0f * random.nextFloat() },
                                  kind == Tending::lantern ? juce::Colour (0xfffff2a8) : juce::Colour (0xff3a2f24),
                                  kind == Tending::lantern ? 2.5f + 1.5f * random.nextFloat() : 1.0f, life, life,
                                  random.nextFloat() * 6.28f, 0.0f, use.at.y + use.radius * (0.2f + 0.6f * random.nextFloat()),
                                  random.nextFloat() * 6.28f, false, kind });
            }
        }
        juce::ignoreUnused (tPour, groundY);

        if (use.t >= 1.3f && ! use.done)
            finishUse();
        if (use.t >= 2.2f)
        {
            use = {};
        }
    }

    for (auto& b : bits)
    {
        b.life -= dt;
        switch (b.kind)
        {
            case Tending::soil:
            case Tending::feed:
                if (! b.landed)
                {
                    b.vel.y += 900.0f * dt;
                    b.pos += b.vel * dt;
                    if (b.pos.y >= b.landY)
                    {
                        b.landed = true;
                        b.life = std::min (b.life, 0.7f);
                        b.maxLife = 0.7f;
                    }
                }
                break;
            case Tending::prune:
                b.vel.y += 380.0f * dt;
                b.vel.x *= std::exp (-dt * 0.8f);
                b.pos += b.vel * dt;
                b.angle += b.spin * dt;
                break;
            case Tending::lantern:
            {
                // Out of the jar and round the flower in ever wider circles.
                const float age = 1.0f - b.life / b.maxLife;
                b.angle += (1.6f + b.aux * 0.15f) * dt;
                const float r = use.radius > 0.0f ? use.radius : 40.0f;
                const juce::Point<float> target = (use.plant >= 0 ? use.at : b.pos) + juce::Point<float> (std::cos (b.angle) * r * (0.6f + 0.8f * age),
                                                                                                         std::sin (b.angle * 1.3f) * r * 0.7f * (0.6f + 0.8f * age));
                b.pos += (target - b.pos) * (1.0f - std::exp (-dt * 3.0f));
                break;
            }
            case Tending::crickets:
                b.vel.y += 900.0f * dt;
                b.pos += b.vel * dt;
                if (b.pos.y >= b.landY && b.vel.y > 0.0f)
                {
                    b.pos.y = b.landY;
                    const bool hop = b.life > 0.6f;
                    b.vel = hop ? juce::Point<float> ((random.nextFloat() - 0.5f) * 190.0f, -150.0f - 90.0f * random.nextFloat()) : juce::Point<float>();
                    b.landed = true;
                }
                break;
            case Tending::count:
                break;
        }
    }
    bits.erase (std::remove_if (bits.begin(), bits.end(), [] (const Bit& b) { return b.life <= 0.0f; }), bits.end());
}

void Meadow::paintBits (juce::Graphics& g, float time) const
{
    for (const auto& b : bits)
    {
        const float fade = juce::jlimit (0.0f, 1.0f, b.life / std::min (0.6f, b.maxLife));
        switch (b.kind)
        {
            case Tending::soil:
                g.setColour (b.colour.withAlpha (fade));
                g.fillEllipse (juce::Rectangle<float> (b.size, b.size * (b.landed ? 0.6f : 1.0f)).withCentre (b.pos));
                break;
            case Tending::feed:
                drawGlow (g, b.pos, b.size * 3.5f, b.colour.withAlpha (0.55f * fade));
                g.setColour (juce::Colour (0xffc9ffd9).withAlpha (fade));
                g.fillEllipse (juce::Rectangle<float> (b.size, b.size * 1.4f).withCentre (b.pos));
                break;
            case Tending::prune:
            {
                juce::Graphics::ScopedSaveState save (g);
                g.addTransform (juce::AffineTransform::rotation (b.angle, b.pos.x, b.pos.y));
                g.setColour (b.colour.withAlpha (fade));
                g.fillEllipse (juce::Rectangle<float> (b.size * 1.6f, b.size).withCentre (b.pos));
                break;
            }
            case Tending::lantern:
            {
                const float blink = 0.35f + 0.65f * (0.5f + 0.5f * std::sin (time * 9.0f + b.aux * 3.0f));
                drawGlow (g, b.pos, b.size * 5.0f, b.colour.withAlpha (0.8f * blink * fade));
                g.setColour (juce::Colours::white.withAlpha (blink * fade));
                g.fillEllipse (juce::Rectangle<float> (b.size, b.size).withCentre (b.pos));
                break;
            }
            case Tending::crickets:
            {
                juce::Graphics::ScopedSaveState save (g);
                const float dir = b.vel.x < 0.0f ? -1.0f : 1.0f;
                g.addTransform (juce::AffineTransform::scale (dir, 1.0f, b.pos.x, b.pos.y));
                g.setColour (b.colour.withAlpha (fade));
                g.fillEllipse (juce::Rectangle<float> (8.0f, 4.0f).withCentre (b.pos));
                g.drawLine (b.pos.x - 3.0f, b.pos.y, b.pos.x - 6.0f, b.pos.y + 4.0f, 1.0f);
                g.drawLine (b.pos.x + 2.0f, b.pos.y, b.pos.x + 6.0f, b.pos.y + 4.0f, 1.0f);
                g.drawLine (b.pos.x + 4.0f, b.pos.y - 1.0f, b.pos.x + 9.0f, b.pos.y - 5.0f, 0.8f);
                if (b.landed && std::fmod (time * 6.0f + b.aux, 2.0f) < 1.0f)
                {
                    g.setColour (colours::ink.withAlpha (0.45f * fade));
                    for (int k = 1; k <= 2; ++k)
                    {
                        juce::Path arc;
                        arc.addCentredArc (b.pos.x + 7.0f, b.pos.y - 7.0f, 3.0f * (float) k, 3.0f * (float) k, 0.0f, 0.2f, 1.3f, true);
                        g.strokePath (arc, juce::PathStrokeType (0.9f));
                    }
                }
                break;
            }
            case Tending::count:
                break;
        }
    }
}

void Meadow::paintTools (juce::Graphics& g, float time) const
{
    for (size_t i = 0; i < tools.size(); ++i)
    {
        const auto& t = tools[i];
        const float size = 64.0f * t.lift;
        const bool busy = use.tool == (int) i;

        // Standing on the strip it casts a small shadow; lifted, a softer one further off.
        if (! busy)
        {
            const float up = (t.lift - 1.0f) * 2.0f;
            g.setColour (juce::Colours::black.withAlpha (0.18f - 0.08f * up));
            g.fillEllipse (juce::Rectangle<float> (size * 0.6f, size * 0.12f).withCentre (t.pos + juce::Point<float> (size * 0.05f + up * 8.0f, size * 0.5f + up * 12.0f)));
        }
        if (t.hover > 0.01f)
            drawGlow (g, t.pos, size * 0.8f, colours::butter.withAlpha (0.4f * t.hover));

        // Poured, scattered, opened or snipped, by how far into its use it is.
        float open = 0.0f, tilt = t.sway;
        if (busy)
        {
            const float tipT = juce::jlimit (0.0f, 1.0f, (use.t - 0.25f) / 0.3f) * (1.0f - juce::jlimit (0.0f, 1.0f, (use.t - 1.7f) / 0.4f));
            const float eased = tipT * tipT * (3.0f - 2.0f * tipT);
            if ((Tending) i == Tending::prune)
                open = eased * (0.5f + 0.5f * std::sin (use.t * 22.0f));
            else
            {
                tilt = 1.75f * eased;
                open = eased;
            }
        }
        else if (t.carrying)
            tilt += std::sin (time * 9.0f) * 0.03f;

        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::rotation (tilt).translated (t.pos));
        paintToolArt (g, t.kind, size, open, time);
    }
}

// Each tool is drawn in a 64 x 64 box centred on the origin.
void Meadow::paintToolArt (juce::Graphics& g, Tending kind, float size, float open, float time)
{
    g.addTransform (juce::AffineTransform::scale (size / 64.0f));
    const auto glass = [&g] (juce::Rectangle<float> body, float radius, juce::Colour tint)
    {
        g.setGradientFill (juce::ColourGradient (tint.withAlpha (0.9f), body.getX(), body.getY(), tint.withAlpha (0.5f), body.getRight(), body.getBottom(), false));
        g.fillRoundedRectangle (body, radius);
        g.setColour (juce::Colour (0xff5d6b66).withAlpha (0.8f));
        g.drawRoundedRectangle (body, radius, 1.6f);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.fillRoundedRectangle (juce::Rectangle<float> (3.0f, body.getHeight() * 0.55f).withPosition (body.getX() + 4.0f, body.getY() + 6.0f), 1.5f);
    };

    switch (kind)
    {
        case Tending::soil:
        {
            // A burlap sack of dark earth, tied at the neck with twine.
            juce::Path sack;
            sack.startNewSubPath (-7.0f, -22.0f);
            sack.cubicTo (-8.0f, -12.0f, -24.0f, -6.0f, -22.0f, 12.0f);
            sack.cubicTo (-21.0f, 24.0f, -16.0f, 28.0f, 0.0f, 28.0f);
            sack.cubicTo (16.0f, 28.0f, 21.0f, 24.0f, 22.0f, 12.0f);
            sack.cubicTo (24.0f, -6.0f, 8.0f, -12.0f, 7.0f, -22.0f);
            sack.closeSubPath();
            g.setGradientFill (juce::ColourGradient (juce::Colour (0xffd2ad72), -20.0f, -10.0f, juce::Colour (0xff9a7440), 22.0f, 26.0f, false));
            g.fillPath (sack);
            g.setColour (juce::Colour (0xff7a5a30).withAlpha (0.55f));
            for (int k = -2; k <= 2; ++k)
                g.drawLine ((float) k * 7.0f, -6.0f, (float) k * 8.0f, 24.0f, 0.8f);
            g.strokePath (sack, juce::PathStrokeType (1.2f));
            // Soil heaped at the mouth, and the twine.
            g.setColour (juce::Colour (0xff3d2b1d));
            g.fillEllipse (-11.0f, -30.0f, 22.0f, 12.0f);
            g.setColour (juce::Colour (0xff5b4130));
            for (int k = 0; k < 5; ++k)
                g.fillEllipse (-8.0f + (float) k * 4.0f, -29.0f + (float) (k % 2) * 3.0f, 3.0f, 2.5f);
            g.setColour (juce::Colour (0xff6b4a2b));
            g.drawLine (-8.0f, -18.0f, 8.0f, -16.0f, 2.2f);
            // A sprout stamped on it.
            g.setColour (juce::Colour (0xff4f7a35).withAlpha (0.9f));
            g.drawLine (0.0f, 16.0f, 0.0f, 4.0f, 1.8f);
            g.fillEllipse (-8.0f, 3.0f, 8.0f, 5.0f);
            g.fillEllipse (1.0f, 0.0f, 8.0f, 5.0f);
            break;
        }

        case Tending::prune:
        {
            // Secateurs: two steel blades crossing at a pin, red handles.
            const float a = 0.12f + 0.5f * open;
            for (int side = -1; side <= 1; side += 2)
            {
                juce::Graphics::ScopedSaveState save (g);
                g.addTransform (juce::AffineTransform::rotation ((float) side * a, 0.0f, 4.0f));
                juce::Path handle;
                handle.startNewSubPath (0.0f, 4.0f);
                handle.lineTo ((float) -side * 5.0f, 22.0f);
                g.setColour (juce::Colour (0xffd94f3d));
                g.strokePath (handle, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colour (0xffb23a2b));
                g.drawEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre ({ (float) -side * 8.0f, 25.0f }), 2.4f);
                juce::Path blade;
                blade.startNewSubPath (0.0f, 4.0f);
                blade.cubicTo ((float) side * 3.0f, -10.0f, (float) side * 3.0f, -22.0f, (float) side * 0.5f, -31.0f);
                blade.lineTo ((float) -side * 3.5f, -30.0f);
                blade.cubicTo ((float) -side * 3.5f, -20.0f, (float) -side * 3.0f, -8.0f, 0.0f, 4.0f);
                g.setGradientFill (juce::ColourGradient (juce::Colour (0xfff2f5f7), -4.0f, -20.0f, juce::Colour (0xff98a2ab), 4.0f, -4.0f, false));
                g.fillPath (blade);
                g.setColour (juce::Colour (0xff6b747c));
                g.strokePath (blade, juce::PathStrokeType (0.8f));
            }
            g.setColour (juce::Colour (0xff4b5259));
            g.fillEllipse (-2.5f, 1.5f, 5.0f, 5.0f);
            break;
        }

        case Tending::feed:
        {
            // A round flask of glowing green tonic with a cork and a leaf on its label.
            const auto tonic = juce::Colour (0xff62d68f);
            juce::Path flask;
            flask.addEllipse (-19.0f, -8.0f, 38.0f, 38.0f);
            flask.addRoundedRectangle (-6.0f, -28.0f, 12.0f, 24.0f, 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.28f));
            g.fillPath (flask);
            {
                juce::Graphics::ScopedSaveState save (g);
                g.reduceClipRegion (juce::Rectangle<int> (-30, -6, 60, 40));
                g.setGradientFill (juce::ColourGradient (tonic.brighter (0.3f), 0.0f, 0.0f, tonic.darker (0.3f), 0.0f, 30.0f, false));
                g.fillPath (flask);
            }
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.strokePath (flask, juce::PathStrokeType (1.4f));
            g.setColour (juce::Colours::white.withAlpha (0.6f));
            g.fillEllipse (-13.0f, 2.0f, 5.0f, 12.0f);
            g.setColour (juce::Colour (0xffb98a55));
            g.fillRoundedRectangle (-7.5f, -35.0f, 15.0f, 9.0f, 2.5f);
            g.setColour (juce::Colour (0xfffff4e0));
            g.fillRoundedRectangle (-11.0f, 8.0f, 22.0f, 14.0f, 3.0f);
            g.setColour (juce::Colour (0xff3f9f5f));
            g.fillEllipse (-5.0f, 11.0f, 10.0f, 6.0f);
            g.drawLine (-6.0f, 18.0f, 6.0f, 12.0f, 1.2f);
            drawGlow (g, { 0.0f, 10.0f }, 26.0f, tonic.withAlpha (0.25f + 0.2f * open));
            break;
        }

        case Tending::lantern:
        case Tending::crickets:
        {
            const bool lit = kind == Tending::lantern;
            const auto tint = lit ? juce::Colour (0xffe9f5ff) : juce::Colour (0xffbfe3b0);
            const juce::Rectangle<float> body (-17.0f, -17.0f, 34.0f, 46.0f);
            if (lit)
                drawGlow (g, { 0.0f, 6.0f }, 34.0f, juce::Colour (0xfffff2a8).withAlpha (0.55f * (1.0f - 0.6f * open)));
            glass (body, 7.0f, tint);
            // The lid, lifting off as it is opened.
            juce::Graphics::ScopedSaveState save (g);
            g.addTransform (juce::AffineTransform::translation (open * 8.0f, -open * 10.0f).rotated (open * 0.5f, 0.0f, -22.0f));
            g.setGradientFill (juce::ColourGradient (juce::Colour (lit ? 0xffe3c36e : 0xff7a8a60), -18.0f, -26.0f, juce::Colour (lit ? 0xffb08a3a : 0xff566340), 18.0f, -17.0f, false));
            g.fillRoundedRectangle (-18.0f, -27.0f, 36.0f, 11.0f, 3.0f);
            if (! lit)
            {
                g.setColour (juce::Colour (0xff2f3a20));
                for (int k = -2; k <= 2; ++k)
                    g.fillEllipse ((float) k * 6.0f - 1.2f, -23.0f, 2.4f, 2.4f);
            }
            g.restoreState();
            g.saveState();
            // What lives inside.
            for (int k = 0; k < (lit ? 6 : 5); ++k)
            {
                const float ph = (float) k * 1.9f;
                if (lit)
                {
                    const juce::Point<float> at (std::sin (time * 0.9f + ph) * 9.0f, 4.0f + std::cos (time * 0.7f + ph * 1.3f) * 14.0f);
                    const float blink = 0.4f + 0.6f * (0.5f + 0.5f * std::sin (time * 6.0f + ph * 2.0f));
                    drawGlow (g, at, 9.0f, juce::Colour (0xfffff2a8).withAlpha (0.9f * blink * (1.0f - open)));
                    g.setColour (juce::Colours::white.withAlpha (blink * (1.0f - open)));
                    g.fillEllipse (juce::Rectangle<float> (2.4f, 2.4f).withCentre (at));
                }
                else
                {
                    const juce::Point<float> at (-9.0f + (float) k * 4.5f, 23.0f - (float) (k % 2) * 7.0f - std::max (0.0f, std::sin (time * 3.0f + ph)) * 4.0f);
                    g.setColour (juce::Colour (0xff3a2f24).withAlpha (1.0f - open));
                    g.fillEllipse (juce::Rectangle<float> (6.0f, 3.2f).withCentre (at));
                    g.drawLine (at.x + 2.0f, at.y, at.x + 5.0f, at.y - 4.0f, 0.8f);
                }
            }
            break;
        }

        case Tending::count:
            break;
    }
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
    if (pour.plant >= 0 || use.tool >= 0 || ! juce::isPositiveAndBelow (plant, (int) plants.size()))
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
        const float using_ = use.plant == (int) i ? juce::jlimit (0.0f, 1.0f, use.t / 0.4f) * (1.0f - juce::jlimit (0.0f, 1.0f, (use.t - 1.9f) / 0.3f)) : 0.0f;
        const float lit = p.glow + (use.plant == (int) i && use.tool == (int) Tending::lantern ? using_ : 0.0f);
        const float glitch = std::max (p.glitch, use.plant == (int) i && (use.tool == (int) Tending::crickets || use.tool == (int) Tending::soil) ? using_ : 0.0f);
        const float pulse = 0.65f + 0.35f * std::sin (time * 1.6f + p.genome.phase);
        const float glow = std::max ({ p.hover, p.preset.name == current ? 0.55f : 0.0f, p.wet * 0.8f, 0.5f * std::min (1.0f, lit) * pulse });

        // Soil scattered on it darkens the ground beneath.
        if (p.earth > 0.01f)
        {
            g.setColour (juce::Colour (0xff3b2a1c).withAlpha (0.5f * std::min (1.0f, p.earth * 1.5f)));
            g.fillEllipse (juce::Rectangle<float> (radius * 2.3f, radius * 2.0f).withCentre (centre + juce::Point<float> (radius * 0.05f, radius * 0.1f)));
        }
        // A glowing flower has a halo of its own, and lights drifting around it.
        if (lit > 0.01f)
        {
            const auto warm = p.genome.petal.interpolatedWith (juce::Colour (0xfffff2c4), 0.6f);
            drawGlow (g, centre, radius * (2.2f + 0.5f * std::min (1.0f, lit)), warm.withAlpha (0.5f * std::min (1.0f, lit) * pulse));
            for (int k = 0; k < 6; ++k)
            {
                const float a = time * (0.5f + 0.13f * (float) k) + (float) k * 1.047f + p.genome.phase;
                const auto at = centre + juce::Point<float> (std::cos (a) * radius * (1.15f + 0.1f * (float) (k % 3)), std::sin (a * 1.2f) * radius * 0.95f);
                const float tw = 0.5f + 0.5f * std::sin (time * 5.0f + (float) k * 2.1f);
                drawGlow (g, at, 7.0f, juce::Colour (0xfffff2a8).withAlpha (0.8f * tw * std::min (1.0f, lit)));
            }
        }

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
            auto draw = [&] (juce::Point<float> c, bool tint)
            {
                g.drawImageTransformed (image, juce::AffineTransform::translation (-(float) image.getWidth() * 0.5f, -(float) image.getHeight() * 0.5f)
                                                   .rotated (angle)
                                                   .scaled (scale)
                                                   .translated (c), tint);
            };
            if (glitch > 0.02f)
            {
                // Glitching: the colours split apart, and slices of it skip sideways.
                juce::Random r ((juce::int64) p.genome.seed + (juce::int64) (time * 9.0f));
                const float split = radius * 0.07f * glitch * (0.4f + r.nextFloat());
                g.setColour (juce::Colour (0xff00e5ff).withAlpha (0.55f * glitch));
                draw (centre + juce::Point<float> (-split, 0.0f), true);
                g.setColour (juce::Colour (0xffff2fa8).withAlpha (0.5f * glitch));
                draw (centre + juce::Point<float> (split, split * 0.3f), true);
            }
            g.setOpacity (1.0f);   // images take the current colour's opacity
            draw (centre, false);
            if (lit > 0.01f)
            {
                g.setColour (juce::Colour (0xfffff6dc).withAlpha (0.28f * std::min (1.0f, lit) * pulse));
                draw (centre, true);
            }
            if (glitch > 0.02f)
            {
                juce::Random r ((juce::int64) p.genome.seed * 7 + (juce::int64) (time * 7.0f));
                for (int k = 0; k < 2; ++k)
                    if (r.nextFloat() < 0.6f * glitch + 0.1f)
                    {
                        const float y = centre.y + (r.nextFloat() - 0.5f) * radius * 1.7f;
                        juce::Graphics::ScopedSaveState save (g);
                        g.reduceClipRegion (juce::Rectangle<float> (radius * 2.2f, 3.0f + radius * 0.18f * r.nextFloat()).withCentre ({ centre.x, y }).toNearestInt());
                        g.setOpacity (1.0f);
                        draw (centre + juce::Point<float> ((r.nextFloat() - 0.5f) * radius * 0.7f * (0.4f + glitch), 0.0f), false);
                    }
            }
        }
        else
        {
            if (glitch > 0.02f)
            {
                const float split = radius * 0.06f * glitch;
                paintFromAbove (g, p.genome, centre + juce::Point<float> (-split, 0.0f), radius, angle, juce::jlimit (0.0f, 1.0f, p.appear * 1.4f - 0.2f), 0.0f, 0.4f * glitch);
                paintFromAbove (g, p.genome, centre + juce::Point<float> (split, 0.0f), radius, angle, juce::jlimit (0.0f, 1.0f, p.appear * 1.4f - 0.2f), 0.0f, 0.4f * glitch);
            }
            paintFromAbove (g, p.genome, centre, radius, angle, juce::jlimit (0.0f, 1.0f, p.appear * 1.4f - 0.2f), glow);
        }
    }

    // A frosted strip for planting and leaving.
    g.setColour (colours::frost.withAlpha (0.72f));
    g.fillRect (getLocalBounds().removeFromBottom (72));
    paintBucket (g, time);
    paintTools (g, time);
    paintBits (g, time);

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

    if (hoveredTool >= 0 && ! tools[(size_t) hoveredTool].carrying)
    {
        const auto& info = tendingInfo (tools[(size_t) hoveredTool].kind);
        paintLabel (g, info.tool, info.hint, tools[(size_t) hoveredTool].pos + juce::Point<float> (0.0f, -52.0f), tools[(size_t) hoveredTool].hover);
    }

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
