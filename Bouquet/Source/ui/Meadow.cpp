#include "Meadow.h"
#include "../PluginProcessor.h"
#include "BouquetLookAndFeel.h"

using namespace bouquet::ui::botany;

namespace bouquet::ui
{
namespace
{
    constexpr float rowScale[] = { 0.7f, 0.85f, 1.0f };
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
    for (size_t row = 0; row < rowGrass.size(); ++row)
        rowGrass[row].generate ((float) getWidth(), 34.0f * rowScale[row] + 10.0f, 0xa11u + (uint32_t) row * 31u, 1.3f);

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

    const int count = (int) all.size();
    const int perRow = std::max (1, (count + 2) / 3);
    const float margin = 70.0f, usable = (float) getWidth() - 2.0f * margin;
    for (int i = 0; i < count; ++i)
    {
        Plant pl;
        pl.preset = all[(size_t) i];
        pl.genome = genomeForPreset (pl.preset);
        const int row = i % 3, col = i / 3;
        juce::Random r ((juce::int64) pl.genome.seed);
        pl.scale = rowScale[row] * (0.92f + 0.16f * r.nextFloat());
        pl.base = { margin + ((float) col + 0.5f + (row == 1 ? 0.33f : row == 2 ? -0.2f : 0.0f)) * usable / (float) perRow
                        + (r.nextFloat() - 0.5f) * 20.0f,
                    rowGround[(size_t) row] };
        pl.base.x = juce::jlimit (40.0f, (float) getWidth() - 40.0f, pl.base.x);
        pl.height = (95.0f + 70.0f * r.nextFloat()) * pl.scale;
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

Pose Meadow::poseOf (const Plant& p, float time) const
{
    const float sway = std::sin (time * 0.8f + p.genome.phase) * 4.0f * p.scale + p.hover * std::sin (time * 3.0f) * 2.0f;
    const float lean = std::sin (p.genome.phase * 3.0f) * 14.0f * p.scale;
    Pose pose;
    pose.base = p.base;
    pose.head = { p.base.x + lean + sway, p.base.y - p.height * (1.0f + 0.06f * p.hover) };
    pose.c1 = { p.base.x, p.base.y - p.height * 0.3f };
    pose.c2 = { p.base.x + lean * 0.6f + sway * 0.5f, p.base.y - p.height * 0.7f };
    const float g = p.appear;
    pose.grow = 1.0f - (1.0f - g) * (1.0f - g) * (1.0f - g);
    pose.bloom = juce::jlimit (0.0f, 1.0f, (g - 0.55f) / 0.45f) * (0.85f + 0.15f * p.hover);
    pose.time = time;
    pose.stemWidth = 2.8f * p.scale;
    const bool current = p.preset.name == processor.getCurrentPresetName();
    pose.glow = std::max (p.hover, current ? 0.55f : 0.0f);
    return pose;
}

juce::Point<float> Meadow::headOf (const Plant& p) const
{
    return poseOf (p, (float) clock()).head;
}

int Meadow::plantAt (juce::Point<float> pos) const
{
    int best = -1;
    float bestDistance = 1.0e9f;
    for (size_t i = 0; i < plants.size(); ++i)
    {
        const auto& p = plants[i];
        const float d = headOf (p).getDistanceFrom (pos);
        const float reach = p.genome.size * p.scale * 2.4f;
        // Front rows win over back rows when they overlap.
        const float score = d - (float) (i % 3) * 4.0f;
        if (d < reach && score < bestDistance)
        {
            bestDistance = score;
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
void Meadow::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float time = (float) clock();

    // A bright sky over rolling hills.
    juce::ColourGradient sky (juce::Colour (0xffd6e8fb), 0.0f, 0.0f, colours::paper, 0.0f, 420.0f, false);
    g.setGradientFill (sky);
    g.fillAll();
    drawGlow (g, { w * 0.82f, 90.0f }, 260.0f, colours::butter.withAlpha (0.35f));

    auto hill = [&] (float y, float amp, float phase, juce::Colour top, juce::Colour bottom)
    {
        juce::Path p;
        p.startNewSubPath (0.0f, h);
        for (float x = 0.0f; x <= w; x += 20.0f)
            p.lineTo (x, y - amp * std::sin (x / w * 3.4f + phase) - amp * 0.4f * std::sin (x / w * 7.1f + phase * 2.0f));
        p.lineTo (w, h);
        p.closeSubPath();
        g.setGradientFill (juce::ColourGradient (top, 0.0f, y - amp, bottom, 0.0f, h, false));
        g.fillPath (p);
    };
    hill (360.0f, 26.0f, 0.4f, juce::Colour (0xffcfe1b8), juce::Colour (0xffb7d39b));
    hill (420.0f, 18.0f, 2.1f, juce::Colour (0xffb5d394), juce::Colour (0xff9cc27a));
    hill (570.0f, 14.0f, 4.0f, juce::Colour (0xffa6cb86), juce::Colour (0xff8fb96e));
    hill (735.0f, 10.0f, 5.3f, juce::Colour (0xff97c07a), juce::Colour (0xff7fa860));

    const float breath = 0.0f;
    const std::vector<Ripple> none;
    for (int row = 0; row < 3; ++row)
    {
        const juce::Point<float> ground (0.0f, rowGround[(size_t) row] + 2.0f);
        rowGrass[(size_t) row].paint (g, ground, time, breath, none, false);
        for (size_t i = (size_t) row; i < plants.size(); i += 3)
            paintPlant (g, plants[i].genome, poseOf (plants[i], time));
        rowGrass[(size_t) row].paint (g, ground, time, breath, none, true);
    }

    // The name of the flower under the mouse, and only that one.
    if (juce::isPositiveAndBelow (hovered, (int) plants.size()))
    {
        const auto& p = plants[(size_t) hovered];
        const auto head = headOf (p);
        const auto font = Fonts::serifItalic (20.0f);
        const float tw = juce::GlyphArrangement::getStringWidth (font, p.preset.name) + 28.0f;
        auto label = juce::Rectangle<float> (tw, 32.0f).withCentre ({ head.x, head.y - p.genome.size * p.scale * 2.0f - 22.0f });
        label = label.constrainedWithin (getLocalBounds().toFloat().reduced (8.0f));
        g.setColour (colours::frost.withAlpha (0.82f * p.hover));
        g.fillRoundedRectangle (label, 16.0f);
        g.setColour (colours::ink.withAlpha (0.1f * p.hover));
        g.drawRoundedRectangle (label, 16.0f, 1.0f);
        g.setFont (font);
        g.setColour (colours::ink.withAlpha (p.hover));
        g.drawText (p.preset.name, label, juce::Justification::centred, false);
    }
}
} // namespace bouquet::ui
