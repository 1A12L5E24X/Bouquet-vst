#include "PluginEditor.h"

namespace bouquet::ui
{
namespace
{
    constexpr int pad = 22;
    const juce::Rectangle<int> soundArea { 24, 84, 404, 556 };
    const juce::Rectangle<int> magicArea { 444, 84, 692, 556 };
    const juce::Rectangle<int> gardenArea { 24, 640, 1112, 142 };
    const juce::Rectangle<int> keysArea  { 24, 782, 1112, 66 };

    // A soft shadow made of a few widening, faint layers (much cheaper than a blur).
    void softShadow (juce::Graphics& g, juce::Rectangle<float> r, float radius, juce::Colour colour, float spread, float dropY)
    {
        constexpr int layers = 7;
        for (int i = layers; i >= 1; --i)
        {
            const float k = (float) i / (float) layers;
            g.setColour (colour.withMultipliedAlpha (1.0f / (float) layers));
            g.fillRoundedRectangle (r.expanded (spread * k).translated (0.0f, dropY * k), radius + spread * k);
        }
    }

    struct AuroraBlob
    {
        float x, y, radius, speed, phase;
        juce::Colour colour;
        bool warm;
    };

    // Sunlight: warm on the left, sky on the right, a pale sun up in the corner.
    const AuroraBlob blobs[] = {
        { 0.10f, 0.08f, 0.55f, 0.05f, 0.0f, colours::butter, true },
        { 0.18f, 0.55f, 0.40f, 0.11f, 1.7f, colours::peach,  true },
        { 0.08f, 0.95f, 0.32f, 0.13f, 3.1f, colours::rose,   true },
        { 0.62f, 0.18f, 0.42f, 0.07f, 0.9f, colours::sky,    false },
        { 0.92f, 0.60f, 0.40f, 0.09f, 2.4f, colours::lilac,  false },
        { 0.75f, 1.00f, 0.32f, 0.12f, 4.2f, colours::mint,   false },
        { 0.45f, 0.50f, 0.26f, 0.05f, 5.0f, colours::butter, false },
    };
}

Surface::Surface (BouquetProcessor& p)
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
      output  (state, ids::output, "output", colours::lilac, colours::sky),
      keyboard (p.keyboardState),
      garden (p, [this] (float note)
      {
          return keyboard.getRectangleForKey (juce::jlimit (36, 107, juce::roundToInt (note))).getCentreX();
      })
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
    addAndMakeVisible (garden);

    // A fixed field of film grain (signed brightness offsets), tiled over the light.
    {
        juce::Random r (7);
        for (auto& n : grainNoise)
            n = (juce::int8) juce::roundToInt ((r.nextFloat() + r.nextFloat() - 1.0f) * 3.0f);
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
    garden.setBounds (gardenArea);
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

    // The aurora drifts slowly; refresh it a few times a second (and when the
    // window moves to a screen with a different pixel density).
    if (background.isNull() || clock() - backgroundTime > 0.2 || pixelScale() != backgroundScale)
    {
        backgroundTime = clock();
        renderBackground();
        fullRepaint = true;
    }

    syncSelections();
    aura.tick (dt);
    const bool gardenWasAlive = garden.isAlive();
    garden.tick (dt);

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
    if (garden.isAlive() || gardenWasAlive)
        garden.repaint();
    repaint (heartArea);
    repaint (spellLineArea.withWidth (30));
    repaint (mutate.getBounds());
}

int Surface::pixelScale() const
{
    // The editor's own zoom times the display's pixel density (2 on Retina screens).
    float s = juce::Component::getApproximateScaleFactorForComponent (this);
    if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        s *= (float) display->scale;
    return juce::jlimit (1, 3, (int) std::ceil (s - 0.05f));
}

//==============================================================================
// The background moves slowly, so it is rendered into a cached image a few times
// a second (aurora, film grain and glass), and simply copied on every frame.
void Surface::renderBackground()
{
    // Rendered at the screen's pixel density so it stays sharp on Retina displays.
    const int scale = pixelScale();
    const int pixelW = designWidth * scale, pixelH = designHeight * scale;
    if (background.getWidth() != pixelW)
    {
        background = juce::Image (juce::Image::RGB, pixelW, pixelH, false);
        columns.clear();
    }
    backgroundScale = scale;
    juce::Graphics g (background);

    const float t = (float) clock();
    const float warm = 0.8f + 0.7f * std::min (1.0f, processor.sourceMeter.load() * 2.0f);
    const float cool = 0.8f + 0.9f * std::min (1.0f, processor.engine.wetLevel.load() * 2.0f);
    const float breath = processor.engine.breath.load();

    // The aurora is painted small and stretched: it is all blur anyway, and it's cheap.
    {
        juce::Graphics ag (aurora);
        const float w = (float) aurora.getWidth(), h = (float) aurora.getHeight();
        juce::ColourGradient base (colours::paper.brighter (0.02f), 0, 0, colours::paperDeep, 0, h, false);
        ag.setGradientFill (base);
        ag.fillAll();

        for (const auto& b : blobs)
        {
            const float cx = (b.x + 0.08f * std::sin (t * b.speed + b.phase) + 0.02f * breath) * w;
            const float cy = (b.y + 0.06f * std::cos (t * b.speed * 0.8f + b.phase * 1.3f)) * h;
            const float r = b.radius * w * (1.0f + 0.06f * std::sin (t * 0.3f + b.phase));
            const float alpha = 0.3f * (b.warm ? warm : cool);
            juce::ColourGradient grad (b.colour.withAlpha (std::min (0.6f, alpha)), cx, cy,
                                       b.colour.withAlpha (0.0f), cx + r, cy, true);
            ag.setGradientFill (grad);
            ag.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        }

        // Daylight: the centre washed brighter, the edges a touch warmer.
        juce::ColourGradient wash (colours::frost.withAlpha (0.35f), w * 0.5f, h * 0.4f,
                                   colours::paperDeep.withAlpha (0.0f), 0.0f, 0.0f, true);
        ag.setGradientFill (wash);
        ag.fillAll();
    }

    // Stretch the aurora to full size (bilinear) and sprinkle film grain in the same pass.
    {
        const juce::Image::BitmapData src (aurora, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dst (background, juce::Image::BitmapData::writeOnly);
        const int srcW = aurora.getWidth(), srcH = aurora.getHeight();
        const float sx = (float) (srcW - 1) / (float) (pixelW - 1);
        const float sy = (float) (srcH - 1) / (float) (pixelH - 1);

        if ((int) columns.size() != pixelW)
        {
            columns.resize ((size_t) pixelW);
            for (int x = 0; x < pixelW; ++x)
            {
                const float fx = (float) x * sx;
                const int x0 = std::min ((int) fx, srcW - 2);
                columns[(size_t) x] = { x0 * src.pixelStride, (int) ((fx - (float) x0) * 256.0f) };
            }
        }

        for (int y = 0; y < pixelH; ++y)
        {
            const float fy = (float) y * sy;
            const int y0 = std::min ((int) fy, srcH - 2);
            const int ty = (int) ((fy - (float) y0) * 256.0f);
            const juce::uint8* row0 = src.getLinePointer (y0);
            const juce::uint8* row1 = src.getLinePointer (y0 + 1);
            juce::uint8* out = dst.getLinePointer (y);
            const juce::int8* noise = grainNoise.data() + (size_t) ((y & 255) * 256);
            const int stride = src.pixelStride;

            for (int x = 0; x < pixelW; ++x, out += dst.pixelStride)
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

    g.addTransform (juce::AffineTransform::scale ((float) scale));
    paintPanel (g, soundPanel, colours::coral);
    paintPanel (g, magicPanel, colours::violet);
    aura.paintBackdrop (g);

    // Glass behind the keys.
    {
        juce::Path keysGlass;
        keysGlass.addRoundedRectangle (keysArea.toFloat().expanded (4.0f), 14.0f);
        softShadow (g, keysArea.toFloat().expanded (4.0f), 14.0f, colours::ink.withAlpha (0.06f), 12.0f, 5.0f);
        g.setColour (colours::frost.withAlpha (0.6f));
        g.fillPath (keysGlass);
        g.setColour (colours::ink.withAlpha (0.07f));
        g.strokePath (keysGlass, juce::PathStrokeType (1.0f));
    }

    // The soil line the bouquets grow from.
    juce::ColourGradient soil (colours::coral.withAlpha (0.0f), (float) gardenArea.getX(), 0.0f,
                               colours::sky.withAlpha (0.0f), (float) gardenArea.getRight(), 0.0f, false);
    soil.addColour (0.2, colours::mint.withAlpha (0.35f));
    soil.addColour (0.8, colours::mint.withAlpha (0.35f));
    g.setGradientFill (soil);
    g.fillRect ((float) gardenArea.getX(), (float) gardenArea.getBottom() - 1.5f, (float) gardenArea.getWidth(), 1.0f);

    // A divider of light under the header.
    juce::ColourGradient line (colours::coral.withAlpha (0.0f), 24.0f, 0.0f, colours::violet.withAlpha (0.0f), (float) designWidth - 24.0f, 0.0f, false);
    line.addColour (0.3, colours::coral.withAlpha (0.25f));
    line.addColour (0.7, colours::violet.withAlpha (0.25f));
    g.setGradientFill (line);
    g.fillRect (24.0f, 78.0f, (float) designWidth - 48.0f, 0.8f);
}

void Surface::paintPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour glow)
{
    // Frosted glass resting in the sun: a soft shadow below, a bright rim on top.
    const float radius = 26.0f;
    juce::Path panel;
    panel.addRoundedRectangle (r, radius);
    softShadow (g, r, radius, glow.withAlpha (0.10f), 26.0f, 0.0f);
    softShadow (g, r, radius, colours::ink.withAlpha (0.07f), 18.0f, 7.0f);

    g.setColour (colours::frost.withAlpha (0.58f));
    g.fillPath (panel);

    juce::ColourGradient edge (colours::frost, r.getX(), r.getY(),
                               colours::ink.withAlpha (0.07f), r.getX(), r.getBottom(), false);
    edge.addColour (0.5, glow.withAlpha (0.12f));
    g.setGradientFill (edge);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.2f);
}

void Surface::paintSectionLabel (juce::Graphics& g, const juce::String& text, int x, int y)
{
    g.setFont (Fonts::sansMedium (9.5f, 0.2f));
    g.setColour (colours::ink.withAlpha (0.42f));
    g.drawText (text.toUpperCase(), x, y, 300, 14, juce::Justification::centredLeft, false);
}

void Surface::paint (juce::Graphics& g)
{
    using namespace colours;
    g.drawImage (background, getLocalBounds().toFloat());

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
        text (str.toUpperCase(), Fonts::sansMedium (9.5f, 0.2f), ink.withAlpha (0.42f), { x, y, 300, 14 });
    };

    // --- Header: a heart that beats with the sound, and the name.
    const float t = (float) clock();
    if (g.clipRegionIntersects (heartArea))
    {
        const juce::Point<float> heart (44.0f, 47.0f);
        // A little sun.
        const auto heartColour = coral.interpolatedWith (butter, 0.5f + 0.5f * std::sin (t * 0.4f));
        drawGlow (g, heart, 22.0f + 34.0f * heartLevel, heartColour.withAlpha (0.6f + 0.3f * heartLevel));
        const auto core = juce::Rectangle<float> (9.0f + 3.0f * heartLevel, 9.0f + 3.0f * heartLevel).withCentre (heart);
        g.setColour (frost);
        g.fillEllipse (core);
        g.setColour (heartColour.darker (0.1f));
        g.drawEllipse (core, 1.4f);
    }

    const auto wordmark = Fonts::serif (40.0f);
    text ("bouquet", wordmark, ink, { 66, 22, 200, 48 });
    const int subtitleX = 66 + (int) juce::GlyphArrangement::getStringWidth (wordmark, "bouquet") + 16;
    text (processor.isEffect ? "a granular effect in bloom" : "a granular instrument in bloom",
          Fonts::serifItalic (17.0f), ink.withAlpha (0.45f), { subtitleX, 35, 360, 30 });

    // --- Sound Lab
    const int lx = soundArea.getX() + pad, lw = soundArea.getWidth() - 2 * pad;
    text ("sound lab", Fonts::serifItalic (30.0f), ink, { lx, 98, 200, 38 });
    text ("choose a voice", Fonts::serifItalic (15.0f), ink.withAlpha (0.4f), { lx, 102, lw, 34 }, juce::Justification::centredRight);
    label ("voice", lx, 146);

    const int instrument = (int) param (ids::instrument);
    auto description = instrumentDescriptions()[instrument];
    if (instrument == 0 && ! processor.isEffect && processor.getMainBusNumInputChannels() == 0)
        description = "route audio into bouquet to hear it";
    text (description, Fonts::serifItalic (15.5f), coral.darker (0.15f), { lx, 334, lw, 24 });

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
        drawSparkle (g, { (float) mx + 6.0f, 238.0f }, 5.5f, butter.withAlpha (0.7f + 0.3f * std::sin (t * 2.0f)));
        text (juce::String (s.name) + juce::String (juce::CharPointer_UTF8 ("  \xe2\x80\x94  ")) + s.description,
              Fonts::serifItalic (16.5f), violet.darker (0.2f), { mx + 20, 226, mw - 20, 24 });
    }

    label ("harmony", harmony.getX(), 440);
    label ("inner workings", mx, 512);

    const auto m = mutate.getBounds().toFloat();
    if (g.clipRegionIntersects (mutate.getBounds()))
        drawSparkle (g, { m.getX() + 20.0f, m.getCentreY() }, 5.0f, butter.withAlpha (0.8f + 0.2f * std::sin (t * 1.3f)));
}
} // namespace bouquet::ui

//==============================================================================
BouquetEditor::BouquetEditor (BouquetProcessor& p)
    : AudioProcessorEditor (p), surface (p)
{
    setLookAndFeel (&lookAndFeel);
    addAndMakeVisible (surface);
    surface.setBounds (0, 0, bouquet::ui::Surface::designWidth, bouquet::ui::Surface::designHeight);

    setResizable (true, true);
    const double ratio = (double) bouquet::ui::Surface::designWidth / bouquet::ui::Surface::designHeight;
    getConstrainer()->setFixedAspectRatio (ratio);
    setResizeLimits (bouquet::ui::Surface::designWidth / 2, bouquet::ui::Surface::designHeight / 2,
                     bouquet::ui::Surface::designWidth * 2, bouquet::ui::Surface::designHeight * 2);
    setSize (bouquet::ui::Surface::designWidth, bouquet::ui::Surface::designHeight);

    lastFrame = bouquet::ui::clock();
    startTimerHz (30);
}

BouquetEditor::~BouquetEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BouquetEditor::resized()
{
    const float scale = (float) getWidth() / (float) bouquet::ui::Surface::designWidth;
    surface.setTransform (juce::AffineTransform::scale (scale));
}

void BouquetEditor::timerCallback()
{
    const double now = bouquet::ui::clock();
    const auto dt = (float) juce::jlimit (0.0, 0.1, now - lastFrame);
    lastFrame = now;
    surface.frame (dt);
}
