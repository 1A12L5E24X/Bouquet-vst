#include "Meadow.h"
#include "../PluginProcessor.h"
#include "BouquetLookAndFeel.h"

using namespace bouquet::ui::botany;

namespace bouquet::ui
{

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
        const float target = (int) i == hovered ? 1.0f : 0.0f;
        p.hover += (target - p.hover) * (1.0f - std::exp (-dt / 0.12f));
    }
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
    hovered = plantAt (e.position);
    setMouseCursor (hovered >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
}

void Meadow::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
}

void Meadow::mouseUp (const juce::MouseEvent& e)
{
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
        close();
        return true;
    }
    return false;
}

//==============================================================================
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
        const float glow = std::max (p.hover, p.preset.name == current ? 0.55f : 0.0f);

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
