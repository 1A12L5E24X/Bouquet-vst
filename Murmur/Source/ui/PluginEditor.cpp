#include "PluginEditor.h"

namespace murmur::ui
{
namespace
{
    constexpr int pad = 22;
    const juce::Rectangle<int> soundArea { 24, 84, 404, 556 };
    const juce::Rectangle<int> magicArea { 444, 84, 692, 556 };
    const juce::Rectangle<int> keysArea  { 24, 652, 1112, 68 };

    struct AuroraBlob
    {
        float x, y, radius, speed, phase;
        juce::Colour colour;
        bool warm;
    };

    const AuroraBlob blobs[] = {
        { 0.12f, 0.30f, 0.42f, 0.11f, 0.0f, colours::coral,  true },
        { 0.28f, 0.85f, 0.38f, 0.08f, 1.7f, colours::rose,   true },
        { 0.05f, 0.95f, 0.30f, 0.13f, 3.1f, colours::amber,  true },
        { 0.62f, 0.20f, 0.45f, 0.07f, 0.9f, colours::violet, false },
        { 0.90f, 0.70f, 0.42f, 0.09f, 2.4f, colours::indigo, false },
        { 0.78f, 1.00f, 0.32f, 0.12f, 4.2f, colours::aqua,   false },
        { 0.48f, 0.55f, 0.28f, 0.05f, 5.0f, colours::lavender, false },
    };
}

Surface::Surface (MurmurProcessor& p)
    : processor (p),
      state (p.apvts),
      tone    (state, ids::labTone, "tone", colours::coral, colours::peach),
      attack  (state, ids::attack, "attack", colours::coral, colours::peach),
      release (state, ids::release, "release", colours::coral, colours::peach),
      strum   (state, ids::strum, "strum", colours::rose, colours::amber),
      level   (state, ids::sourceLevel, "level", colours::coral, colours::amber),
      aura    (state, p.engine),
      mix     (state, ids::mix, "mix", colours::violet, colours::aqua),
      space   (state, ids::space, "space", colours::indigo, colours::lavender),
      life    (state, ids::life, "life", colours::violet, colours::rose),
      freeze  (state),
      output  (state, ids::output, "output", colours::lavender, colours::ink),
      keyboard (p.keyboardState)
{
    setOpaque (true);

    // --- Sound Lab
    const auto names = instrumentNames();
    for (int i = 0; i < names.size(); ++i)
    {
        auto* item = instruments.add (new GlowItem (names[i], GlowItem::Style::text, colours::coral, colours::peach));
        item->setShowDot (true);
        item->setFont (Fonts::serif (19.0f));
        item->onClick = [this, i] { setParam (ids::instrument, (float) i); };
        addAndMakeVisible (item);
    }

    const auto& shapes = chordShapes();
    for (int i = 0; i < (int) shapes.size(); ++i)
    {
        auto* item = chords.add (new GlowItem (shapes[(size_t) i].name, GlowItem::Style::pill, colours::rose, colours::amber));
        item->setFont (Fonts::serif (14.5f));
        item->onClick = [this, i] { setParam (ids::chord, (float) i); };
        addAndMakeVisible (item);
    }

    const juce::StringArray voicingNames { "close", "open", "wide" };
    for (int i = 0; i < voicingNames.size(); ++i)
    {
        auto* item = voicings.add (new GlowItem (voicingNames[i], GlowItem::Style::pill, colours::coral, colours::peach));
        item->setFont (Fonts::serif (14.5f));
        item->onClick = [this, i] { setParam (ids::voicing, (float) i); };
        addAndMakeVisible (item);
    }

    octaveDown.onClick = [this] { setParam (ids::octave, std::max (-2.0f, param (ids::octave) - 1.0f)); };
    octaveUp.onClick   = [this] { setParam (ids::octave, std::min (2.0f, param (ids::octave) + 1.0f)); };
    for (auto* c : std::initializer_list<juce::Component*> { &octaveDown, &octaveUp, &tone, &attack, &release, &strum, &level })
        addAndMakeVisible (c);

    // --- Magic Lab
    const auto cats = spellCategories();
    for (int i = 0; i < cats.size(); ++i)
    {
        auto* item = categories.add (new GlowItem (cats[i], GlowItem::Style::text, colours::violet, colours::aqua));
        item->setFont (Fonts::serifItalic (17.0f));
        item->onClick = [this, i] { showCategory (i); };
        addAndMakeVisible (item);
    }

    for (int i = 0; i < 8; ++i)
    {
        auto* item = spells.add (new GlowItem ({}, GlowItem::Style::pill, colours::violet, colours::aqua));
        item->setFont (Fonts::serif (15.5f));
        item->onClick = [this, i]
        {
            if (i < (int) visibleSpells.size())
                processor.castSpell (visibleSpells[(size_t) i]);
        };
        addAndMakeVisible (item);
    }

    for (auto* c : std::initializer_list<juce::Component*> { &aura, &mix, &space, &life, &freeze, &mutate, &output, &harmony })
        addAndMakeVisible (c);

    for (auto& h : harmonySets())
        harmony.addItem (h.name, harmony.getNumItems() + 1);
    harmonyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, ids::harmony, harmony);

    struct InnerDef { const juce::String& id; const char* label; bool bipolar; };
    const InnerDef innerDefs[] = {
        { ids::density, "density", false }, { ids::size, "size", false },      { ids::time, "reach", false },
        { ids::spray, "spray", false },     { ids::pitch, "pitch", true },     { ids::harmonyAmount, "chord", false },
        { ids::detune, "detune", false },   { ids::reverse, "reverse", false }, { ids::chaos, "chaos", false },
        { ids::shape, "edge", false },      { ids::width, "width", false },    { ids::feedback, "echo", false },
        { ids::colour, "colour", true },    { ids::decay, "decay", false },
    };
    for (size_t i = 0; i < std::size (innerDefs); ++i)
    {
        const float k = (float) i / (float) (std::size (innerDefs) - 1);
        auto* knob = inner.add (new GlowKnob (state, innerDefs[i].id, innerDefs[i].label,
                                              colours::violet.interpolatedWith (colours::indigo, k),
                                              colours::aqua.interpolatedWith (colours::lavender, k), innerDefs[i].bipolar));
        addAndMakeVisible (knob);
    }

    mutate.setFont (Fonts::serifItalic (16.0f));
    mutate.onClick = [this] { processor.mutate(); };

    addAndMakeVisible (keyboard);

    // A fixed field of film grain (signed brightness offsets), tiled over the light.
    {
        juce::Random r (7);
        for (auto& n : grainNoise)
            n = (juce::int8) juce::roundToInt ((r.nextFloat() + r.nextFloat() - 1.0f) * 6.0f);
    }
    aurora = juce::Image (juce::Image::RGB, designWidth / 4, designHeight / 4, false);

    // Open on the category of the current spell.
    const int current = processor.getCurrentSpell();
    showCategory (juce::isPositiveAndBelow (current, (int) spellBook().size()) ? spellBook()[(size_t) current].category : 0);
    syncSelections();
}

void Surface::resized()
{
    // Sound Lab
    const int lx = soundArea.getX() + pad, lw = soundArea.getWidth() - 2 * pad;
    for (int i = 0; i < instruments.size(); ++i)
        instruments[i]->setBounds (lx + (i % 2) * (lw / 2), 162 + (i / 2) * 34, lw / 2, 34);

    const int chordW = lw / 7;
    for (int i = 0; i < chords.size(); ++i)
        chords[i]->setBounds (lx + (i % 7) * chordW, 388 + (i / 7) * 32, chordW, 32);

    for (int i = 0; i < voicings.size(); ++i)
        voicings[i]->setBounds (lx + i * 60, 478, 60, 32);
    octaveDown.setBounds (lx + 228, 478, 34, 32);
    octaveUp.setBounds (lx + 326, 478, 34, 32);

    int kx = lx - 6;
    for (auto* k : { &tone, &attack, &release, &strum, &level })
    {
        k->setBounds (kx, 528, 74, 86);
        kx += 74;
    }

    // Magic Lab
    const int mx = magicArea.getX() + pad, mw = magicArea.getWidth() - 2 * pad;
    for (int i = 0; i < categories.size(); ++i)
        categories[i]->setBounds (mx + mw - (categories.size() - i) * 76, 100, 76, 32);

    const int spellW = mw / 4;
    for (int i = 0; i < spells.size(); ++i)
        spells[i]->setBounds (mx + (i % 4) * spellW, 146 + (i / 4) * 38, spellW, 38);

    aura.setBounds (mx, 258, 474, 236);

    const int cx = mx + 474 + 12, cw = mw - 474 - 12;
    mix.setBounds (cx + (cw - 96) / 2, 252, 96, 104);
    space.setBounds (cx + cw / 2 - 76, 356, 76, 78);
    life.setBounds (cx + cw / 2, 356, 76, 78);
    freeze.setBounds (cx, 436, 64, 62);
    harmony.setBounds (cx + 70, 456, cw - 70, 28);

    const int innerW = (mw + 8) / inner.size();
    for (int i = 0; i < inner.size(); ++i)
        inner[i]->setBounds (mx - 4 + i * innerW, 530, innerW, 66);

    // Header
    output.setBounds (designWidth - 24 - 56, 18, 56, 60);
    mutate.setBounds (designWidth - 24 - 56 - 116, 30, 108, 34);

    keyboard.setBounds (keysArea);
    keyboard.setKeyWidth ((float) keysArea.getWidth() / 42.0f);

    soundPanel = soundArea.toFloat();
    magicPanel = magicArea.toFloat();
    spellLineArea = { mx, 224, mw, 28 };
    renderBackground();
}

//==============================================================================
float Surface::param (const juce::String& id) const
{
    return state.getRawParameterValue (id)->load();
}

void Surface::setParam (const juce::String& id, float value)
{
    auto* p = state.getParameter (id);
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (value));
    p->endChangeGesture();
}

void Surface::showCategory (int c)
{
    category = c;
    visibleSpells.clear();
    const auto& book = spellBook();
    for (int i = 0; i < (int) book.size(); ++i)
        if (book[(size_t) i].category == c)
            visibleSpells.push_back (i);

    for (int i = 0; i < spells.size(); ++i)
    {
        const bool used = i < (int) visibleSpells.size();
        spells[i]->setVisible (used);
        spells[i]->setText (used ? book[(size_t) visibleSpells[(size_t) i]].name : "");
    }
    syncSelections();
}

void Surface::syncSelections()
{
    const int instrument = (int) param (ids::instrument);
    for (int i = 0; i < instruments.size(); ++i)
        instruments[i]->setSelected (i == instrument);

    const int chord = (int) param (ids::chord);
    for (int i = 0; i < chords.size(); ++i)
        chords[i]->setSelected (i == chord);

    const int voicing = (int) param (ids::voicing);
    for (int i = 0; i < voicings.size(); ++i)
        voicings[i]->setSelected (i == voicing);

    for (int i = 0; i < categories.size(); ++i)
        categories[i]->setSelected (i == category);

    const int spell = processor.getCurrentSpell();
    for (int i = 0; i < spells.size(); ++i)
        spells[i]->setSelected (i < (int) visibleSpells.size() && visibleSpells[(size_t) i] == spell);
}

void Surface::frame (float dt)
{
    // Parameter changes that alter painted text redraw everything once.
    const int instrument = (int) param (ids::instrument), octave = (int) param (ids::octave);
    const int spell = processor.getCurrentSpell();
    bool fullRepaint = instrument != shownInstrument || octave != shownOctave || spell != shownSpell;
    shownInstrument = instrument;
    shownOctave = octave;
    shownSpell = spell;

    // The aurora drifts slowly; refresh it a few times a second.
    if (background.isNull() || clock() - backgroundTime > 0.2)
    {
        backgroundTime = clock();
        renderBackground();
        fullRepaint = true;
    }

    syncSelections();
    aura.tick (dt);

    const float target = std::min (1.0f, processor.outputMeter.load() * 1.6f);
    heartLevel += (target - heartLevel) * (target > heartLevel ? 0.5f : 0.08f);

    if (fullRepaint)
    {
        for (auto* group : { &instruments, &chords, &voicings, &categories, &spells })
            for (auto* item : *group)
                item->animate();
        for (auto* item : { &octaveDown, &octaveUp, &mutate })
            item->animate();
        repaint();
        return;
    }

    for (auto* group : { &instruments, &chords, &voicings, &categories, &spells })
        for (auto* item : *group)
            if (item->animate())
                item->repaint();
    for (auto* item : { &octaveDown, &octaveUp, &mutate })
        if (item->animate())
            item->repaint();
    for (auto* knob : { &tone, &attack, &release, &strum, &level, &mix, &space, &life, &output })
        knob->refreshIfTouched();
    for (auto* knob : inner)
        knob->refreshIfTouched();

    aura.repaint();
    freeze.repaint();
    repaint (heartArea);
    repaint (spellLineArea.withWidth (30));
    repaint (mutate.getBounds());
}

//==============================================================================
// The background moves slowly, so it is rendered into a cached image a few times
// a second (aurora, film grain and glass), and simply copied on every frame.
void Surface::renderBackground()
{
    if (background.isNull())
        background = juce::Image (juce::Image::RGB, designWidth, designHeight, false);
    juce::Graphics g (background);

    const float t = (float) clock();
    const float warm = 0.8f + 0.7f * std::min (1.0f, processor.sourceMeter.load() * 2.0f);
    const float cool = 0.8f + 0.9f * std::min (1.0f, processor.engine.wetLevel.load() * 2.0f);
    const float breath = processor.engine.breath.load();

    // The aurora is painted small and stretched: it is all blur anyway, and it's cheap.
    {
        juce::Graphics ag (aurora);
        const float w = (float) aurora.getWidth(), h = (float) aurora.getHeight();
        juce::ColourGradient base (colours::night.brighter (0.05f), 0, 0, colours::nightDeep, 0, h, false);
        ag.setGradientFill (base);
        ag.fillAll();

        for (const auto& b : blobs)
        {
            const float cx = (b.x + 0.08f * std::sin (t * b.speed + b.phase) + 0.02f * breath) * w;
            const float cy = (b.y + 0.06f * std::cos (t * b.speed * 0.8f + b.phase * 1.3f)) * h;
            const float r = b.radius * w * (1.0f + 0.06f * std::sin (t * 0.3f + b.phase));
            const float alpha = 0.22f * (b.warm ? warm : cool);
            juce::ColourGradient grad (b.colour.withAlpha (std::min (0.6f, alpha)), cx, cy,
                                       b.colour.withAlpha (0.0f), cx + r, cy, true);
            ag.setGradientFill (grad);
            ag.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        }

        // Vignette.
        juce::ColourGradient vignette (juce::Colours::transparentBlack, w * 0.5f, h * 0.45f,
                                       colours::nightDeep.withAlpha (0.75f), 0.0f, 0.0f, true);
        ag.setGradientFill (vignette);
        ag.fillAll();
    }

    // Stretch the aurora to full size (bilinear) and sprinkle film grain in the same pass.
    {
        const juce::Image::BitmapData src (aurora, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dst (background, juce::Image::BitmapData::writeOnly);
        const int srcW = aurora.getWidth(), srcH = aurora.getHeight();
        const float sx = (float) (srcW - 1) / (float) (designWidth - 1);
        const float sy = (float) (srcH - 1) / (float) (designHeight - 1);

        if ((int) columns.size() != designWidth)
        {
            columns.resize ((size_t) designWidth);
            for (int x = 0; x < designWidth; ++x)
            {
                const float fx = (float) x * sx;
                const int x0 = std::min ((int) fx, srcW - 2);
                columns[(size_t) x] = { x0 * src.pixelStride, (int) ((fx - (float) x0) * 256.0f) };
            }
        }

        for (int y = 0; y < designHeight; ++y)
        {
            const float fy = (float) y * sy;
            const int y0 = std::min ((int) fy, srcH - 2);
            const int ty = (int) ((fy - (float) y0) * 256.0f);
            const juce::uint8* row0 = src.getLinePointer (y0);
            const juce::uint8* row1 = src.getLinePointer (y0 + 1);
            juce::uint8* out = dst.getLinePointer (y);
            const juce::int8* noise = grainNoise.data() + (size_t) ((y & 255) * 256);
            const int stride = src.pixelStride;

            for (int x = 0; x < designWidth; ++x, out += dst.pixelStride)
            {
                const auto& col = columns[(size_t) x];
                const juce::uint8* a = row0 + col.offset;
                const juce::uint8* b = row1 + col.offset;
                const int n = noise[x & 255];
                for (int c = 0; c < 3; ++c)
                {
                    const int top = a[c] * 256 + (a[c + stride] - a[c]) * col.t;
                    const int bottom = b[c] * 256 + (b[c + stride] - b[c]) * col.t;
                    const int v = ((top * 256 + (bottom - top) * ty) >> 16) + n;
                    out[c] = (juce::uint8) juce::jlimit (0, 255, v);
                }
            }
        }
    }

    paintPanel (g, soundPanel, colours::coral);
    paintPanel (g, magicPanel, colours::violet);
    aura.paintBackdrop (g);

    // Glass behind the keys.
    g.setColour (colours::nightDeep.withAlpha (0.35f));
    g.fillRoundedRectangle (keysArea.toFloat().expanded (4.0f), 14.0f);
    g.setColour (colours::ink.withAlpha (0.06f));
    g.drawRoundedRectangle (keysArea.toFloat().expanded (4.0f), 14.0f, 1.0f);

    // A divider of light under the header.
    juce::ColourGradient line (colours::coral.withAlpha (0.0f), 24.0f, 0.0f, colours::violet.withAlpha (0.0f), (float) designWidth - 24.0f, 0.0f, false);
    line.addColour (0.3, colours::coral.withAlpha (0.25f));
    line.addColour (0.7, colours::violet.withAlpha (0.25f));
    g.setGradientFill (line);
    g.fillRect (24.0f, 78.0f, (float) designWidth - 48.0f, 0.8f);
}

void Surface::paintPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour glow)
{
    const float radius = 26.0f;
    g.setColour (colours::nightDeep.withAlpha (0.28f));
    g.fillRoundedRectangle (r, radius);
    g.setColour (colours::ink.withAlpha (0.022f));
    g.fillRoundedRectangle (r, radius);

    // Glass edge: brighter along the top, as if lit from above.
    juce::ColourGradient edge (colours::ink.withAlpha (0.16f), r.getX(), r.getY(),
                               colours::ink.withAlpha (0.04f), r.getX(), r.getBottom(), false);
    edge.addColour (0.5, glow.withAlpha (0.07f));
    g.setGradientFill (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

void Surface::paintSectionLabel (juce::Graphics& g, const juce::String& text, int x, int y)
{
    g.setFont (Fonts::sansMedium (9.5f, 0.2f));
    g.setColour (colours::ink.withAlpha (0.32f));
    g.drawText (text.toUpperCase(), x, y, 300, 14, juce::Justification::centredLeft, false);
}

void Surface::paint (juce::Graphics& g)
{
    using namespace colours;
    g.drawImageAt (background, 0, 0);

    // Most frames repaint a few small regions; skip any text that lies outside them.
    auto text = [&g] (const juce::String& str, const juce::Font& font, juce::Colour colour, juce::Rectangle<int> area,
                      juce::Justification just = juce::Justification::centredLeft)
    {
        if (! g.clipRegionIntersects (area))
            return;
        g.setFont (font);
        g.setColour (colour);
        g.drawText (str, area, just, true);
    };
    auto label = [&text] (const juce::String& str, int x, int y)
    {
        text (str.toUpperCase(), Fonts::sansMedium (9.5f, 0.2f), ink.withAlpha (0.32f), { x, y, 300, 14 });
    };

    // --- Header: a heart that beats with the sound, and the name.
    const float t = (float) clock();
    if (g.clipRegionIntersects (heartArea))
    {
        const juce::Point<float> heart (44.0f, 47.0f);
        const auto heartColour = coral.interpolatedWith (violet, 0.5f + 0.5f * std::sin (t * 0.4f));
        drawGlow (g, heart, 22.0f + 34.0f * heartLevel, heartColour.withAlpha (0.55f + 0.3f * heartLevel));
        g.setColour (ink);
        g.fillEllipse (juce::Rectangle<float> (7.0f + 3.0f * heartLevel, 7.0f + 3.0f * heartLevel).withCentre (heart));
    }

    const auto wordmark = Fonts::serif (40.0f);
    text ("murmur", wordmark, ink, { 66, 22, 200, 48 });
    const int subtitleX = 66 + (int) juce::GlyphArrangement::getStringWidth (wordmark, "murmur") + 16;
    text (processor.isEffect ? "a granular effect that sings back" : "a granular instrument that sings back",
          Fonts::serifItalic (17.0f), ink.withAlpha (0.45f), { subtitleX, 35, 360, 30 });

    // --- Sound Lab
    const int lx = soundArea.getX() + pad, lw = soundArea.getWidth() - 2 * pad;
    text ("sound lab", Fonts::serifItalic (30.0f), ink, { lx, 98, 200, 38 });
    text ("choose a voice", Fonts::serifItalic (15.0f), ink.withAlpha (0.4f), { lx, 102, lw, 34 }, juce::Justification::centredRight);
    label ("voice", lx, 146);

    const int instrument = (int) param (ids::instrument);
    auto description = instrumentDescriptions()[instrument];
    if (instrument == 0 && ! processor.isEffect && processor.getMainBusNumInputChannels() == 0)
        description = "route audio into murmur to hear it";
    text (description, Fonts::serifItalic (15.5f), peach.withAlpha (0.75f), { lx, 334, lw, 24 });

    label ("chord", lx, 372);
    label ("voicing", lx, 462);
    label ("octave", lx + 228, 462);
    const int octave = (int) param (ids::octave);
    text (octave > 0 ? "+" + juce::String (octave) : juce::String (octave), Fonts::serif (19.0f), ink.withAlpha (0.85f),
          { lx + 262, 478, 64, 32 }, juce::Justification::centred);
    label ("shape", lx, 516);

    // --- Magic Lab
    const int mx = magicArea.getX() + pad, mw = magicArea.getWidth() - 2 * pad;
    text ("magic lab", Fonts::serifItalic (30.0f), ink, { mx, 98, 200, 38 });

    // The spell's line of poetry.
    const int spell = processor.getCurrentSpell();
    if (juce::isPositiveAndBelow (spell, (int) spellBook().size()) && g.clipRegionIntersects (spellLineArea))
    {
        const auto& s = spellBook()[(size_t) spell];
        drawSparkle (g, { (float) mx + 6.0f, 238.0f }, 5.5f, lavender.withAlpha (0.6f + 0.3f * std::sin (t * 2.0f)));
        text (juce::String (s.name) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x80\x94  ")) + s.description,
              Fonts::serifItalic (16.5f), lavender.withAlpha (0.85f), { mx + 20, 226, mw - 20, 24 });
    }

    label ("harmony", harmony.getX(), 440);
    label ("inner workings", mx, 512);

    const auto m = mutate.getBounds().toFloat();
    if (g.clipRegionIntersects (mutate.getBounds()))
        drawSparkle (g, { m.getX() + 20.0f, m.getCentreY() }, 5.0f, lavender.withAlpha (0.7f + 0.3f * std::sin (t * 1.3f)));
}
} // namespace murmur::ui

//==============================================================================
MurmurEditor::MurmurEditor (MurmurProcessor& p)
    : AudioProcessorEditor (p), surface (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (surface);
    surface.setBounds (0, 0, murmur::ui::Surface::designWidth, murmur::ui::Surface::designHeight);

    setResizable (true, true);
    const double ratio = (double) murmur::ui::Surface::designWidth / murmur::ui::Surface::designHeight;
    getConstrainer()->setFixedAspectRatio (ratio);
    setResizeLimits (murmur::ui::Surface::designWidth / 2, murmur::ui::Surface::designHeight / 2,
                     murmur::ui::Surface::designWidth * 2, murmur::ui::Surface::designHeight * 2);
    setSize (murmur::ui::Surface::designWidth, murmur::ui::Surface::designHeight);

    lastFrame = murmur::ui::clock();
    startTimerHz (30);
}

MurmurEditor::~MurmurEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MurmurEditor::resized()
{
    const float scale = (float) getWidth() / (float) murmur::ui::Surface::designWidth;
    surface.setTransform (juce::AffineTransform::scale (scale));
}

void MurmurEditor::timerCallback()
{
    const double now = murmur::ui::clock();
    const auto dt = (float) juce::jlimit (0.0, 0.1, now - lastFrame);
    lastFrame = now;
    surface.frame (dt);
}
