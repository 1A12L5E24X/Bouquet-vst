#include "PluginEditor.h"

namespace bouquet::ui
{
namespace
{
    constexpr int pad = 24;
    const juce::Rectangle<int> soundHome { 24, 84, 404, 548 };
    const juce::Rectangle<int> magicHome { 444, 84, 692, 548 };
    // The bouquets stand behind the keys, so their stems disappear behind the keyboard.
    const juce::Rectangle<int> gardenArea { 24, 630, 1112, 142 };
    const juce::Rectangle<int> keysArea  { 24, 752, 1112, 64 };
    const juce::Rectangle<int> meadowArea { 0, 760, 1160, 100 };

    juce::String capitalised (const juce::String& s)
    {
        return s.substring (0, 1).toUpperCase() + s.substring (1);
    }

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
}

Surface::Surface (BouquetProcessor& p)
    : processor (p),
      state (p.apvts),
      tone    (state, ids::labTone, "tone", colours::coral, colours::peach),
      attack  (state, ids::attack, "attack", colours::coral, colours::peach),
      release (state, ids::release, "release", colours::coral, colours::peach),
      strum   (state, ids::strum, "strum", colours::rose, colours::amber),
      forest  (state, ids::forest, "distance", juce::Colour (0xff6f9f5c), colours::mint),
      wildlife (state, ids::wildlife, "ambience", juce::Colour (0xff6f9f5c), colours::butter),
      level   (state, ids::sourceLevel, "level", colours::coral, colours::amber),
      aura    (state, p.engine),
      mix     (state, ids::mix, "mix", colours::violet, colours::aqua),
      space   (state, ids::space, "reverb", colours::indigo, colours::lavender),
      life    (state, ids::life, "drift", colours::violet, colours::rose),
      freeze  (state),
      output  (state, ids::output, "output", colours::lilac, colours::sky),
      meadowUnder (p, false),
      garden (p, [this] (float note)
      {
          return keyboard.getRectangleForKey (juce::jlimit (36, 107, juce::roundToInt (note))).getCentreX();
      }),
      keyboard (p.keyboardState),
      meadowOver (p, true),
      meadow (p)
{
    setOpaque (true);

    // --- Sound Lab
    const auto names = instrumentNames();
    for (int i = 0; i < names.size(); ++i)
    {
        auto* item = instruments.add (new GlowItem (names[i], GlowItem::Style::text, colours::coral, colours::peach));
        item->setShowDot (true);
        item->setFont (Fonts::serif (18.0f));
        item->onClick = [this, i] { setParam (ids::instrument, (float) i); };
        addAndMakeVisible (item);
    }

    juce::StringArray chordNames;
    for (const auto& shape : chordShapes())
        chordNames.add (capitalised (shape.name));
    chords.setItems (chordNames);
    chords.onSelect = [this] (int i) { setParam (ids::chord, (float) i); };

    voicings.setItems ({ "Close", "Open", "Wide" });
    voicings.onSelect = [this] (int i) { setParam (ids::voicing, (float) i); };

    octave.setItems ({ juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")), "0", "+" });
    octave.setPassive (1);
    octave.setColumnWeights ({ 1.0f, 1.2f, 1.0f });
    octave.onSelect = [this] (int i)
    {
        const float step = i == 0 ? -1.0f : 1.0f;
        setParam (ids::octave, juce::jlimit (-2.0f, 2.0f, param (ids::octave) + step));
    };
    for (auto* c : std::initializer_list<juce::Component*> { &chords, &voicings, &octave, &tone, &attack, &release, &strum, &forest, &wildlife, &level })
        addAndMakeVisible (c);

    // --- Magic Lab
    const auto cats = spellCategories();
    for (int i = 0; i < cats.size(); ++i)
    {
        auto* item = categories.add (new GlowItem (cats[i], GlowItem::Style::tab, colours::violet, colours::aqua));
        item->setFont (Fonts::serifItalic (17.0f));
        item->onClick = [this, i] { showCategory (i); };
        addAndMakeVisible (item);
    }

    spells.onSelect = [this] (int i)
    {
        if (i < (int) visibleSpells.size())
            processor.castSpell (visibleSpells[(size_t) i]);
    };
    addAndMakeVisible (spells);

    for (auto* c : std::initializer_list<juce::Component*> { &aura, &mix, &space, &life, &freeze, &mutate, &output, &harmony })
        addAndMakeVisible (c);

    for (auto& h : harmonySets())
        harmony.addItem (capitalised (h.name), harmony.getNumItems() + 1);
    harmonyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, ids::harmony, harmony);

    struct InnerDef { const juce::String& id; const char* label; bool bipolar; };
    const InnerDef innerDefs[] = {
        { ids::density, "density", false }, { ids::size, "size", false },      { ids::time, "delay", false },
        { ids::spray, "spray", false },     { ids::pitch, "pitch", true },     { ids::harmonyAmount, "harmony", false },
        { ids::detune, "detune", false },   { ids::reverse, "reverse", false }, { ids::chaos, "jitter", false },
        { ids::shape, "shape", false },      { ids::width, "width", false },    { ids::feedback, "feedback", false },
        { ids::colour, "tone", true },    { ids::decay, "decay", false },
    };
    for (size_t i = 0; i < std::size (innerDefs); ++i)
    {
        const float k = (float) i / (float) (std::size (innerDefs) - 1);
        auto* knob = inner.add (new GlowKnob (state, innerDefs[i].id, innerDefs[i].label,
                                              colours::violet.interpolatedWith (colours::indigo, k),
                                              colours::aqua.interpolatedWith (colours::lavender, k), innerDefs[i].bipolar));
        addAndMakeVisible (knob);
    }

    mutate.onClick = [this] { processor.mutate(); };
    gardenButton.onClick = [this] { meadow.open(); };
    magicSwitch.onClick = [this] { setParam (ids::magic, param (ids::magic) > 0.5f ? 0.0f : 1.0f); };
    flowerStyle.onClick = [this] { processor.setPhotorealFlowers (! processor.photorealFlowers()); renderBackground(); repaint(); };
    syncSwitch.onClick = [this] { setParam (ids::sync, param (ids::sync) > 0.5f ? 0.0f : 1.0f); };
    for (auto* c : std::initializer_list<juce::Component*> { &gardenButton, &flowerStyle, &syncSwitch, &magicSwitch })
        addAndMakeVisible (c);

    addAndMakeVisible (meadowUnder);
    addAndMakeVisible (garden);
    addAndMakeVisible (keyboard);
    addAndMakeVisible (meadowOver);
    addChildComponent (meadow);
    magicShown = param (ids::magic) > 0.5f ? 1.0f : 0.0f;

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

juce::Rectangle<int> Surface::soundArea() const
{
    // With the Magic Lab folded away the Sound Lab moves to the middle.
    const float e = magicShown * magicShown * (3.0f - 2.0f * magicShown);
    const int centred = (designWidth - soundHome.getWidth()) / 2;
    return soundHome.withX (juce::roundToInt ((float) centred + (float) (soundHome.getX() - centred) * e));
}

juce::Rectangle<int> Surface::magicArea() const
{
    const float e = magicShown * magicShown * (3.0f - 2.0f * magicShown);
    return magicHome.translated (juce::roundToInt ((1.0f - e) * 80.0f), 0);
}

std::vector<juce::Component*> Surface::magicComponents()
{
    std::vector<juce::Component*> list { &aura, &mix, &space, &life, &freeze, &harmony, &mutate, &spells };
    for (auto* c : categories) list.push_back (c);
    for (auto* c : inner)      list.push_back (c);
    return list;
}

void Surface::resized()
{
    const auto soundArea = this->soundArea();
    const auto magicArea = this->magicArea();

    // Sound Lab
    const int lx = soundArea.getX() + pad, lw = soundArea.getWidth() - 2 * pad;
    for (int i = 0; i < instruments.size(); ++i)
        instruments[i]->setBounds (lx + (i % 2) * (lw / 2), 176 + (i / 2) * 27, lw / 2, 27);

    chords.setBounds (lx, 402, lw, 60);
    voicings.setBounds (lx, 496, 200, 30);
    octave.setBounds (lx + 238, 496, 118, 30);

    int kx = lx - 10;
    for (auto* k : { &tone, &attack, &release, &strum, &forest, &wildlife, &level })
    {
        k->setBounds (kx, 542, 54, 62);
        kx += 52;
    }

    // Magic Lab
    const int mx = magicArea.getX() + pad, mw = magicArea.getWidth() - 2 * pad;
    {
        int tx = magicArea.getX() + 330;
        for (auto* tab : categories)
        {
            const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (Fonts::serifItalic (17.0f), spellCategories()[categories.indexOf (tab)]));
            tab->setBounds (tx, 108, w + 2, 30);
            tx += w + 22;
        }
    }

    spells.setBounds (mx, 156, mw, 64);
    aura.setBounds (mx, 262, 472, 232);

    const int cx = magicArea.getX() + 514;
    mix.setBounds (magicArea.getX() + 532, 256, 110, 112);
    space.setBounds (cx, 374, 66, 70);
    life.setBounds (cx + 80, 374, 66, 70);
    freeze.setBounds (cx, 460, 60, 52);
    harmony.setBounds (cx + 74, 460, 80, 34);

    const int innerW = (mw + 16) / inner.size();
    for (int i = 0; i < inner.size(); ++i)
        inner[i]->setBounds (mx - 8 + i * innerW, 542, innerW, 54);

    // Header
    gardenButton.setBounds (28, 28, 76, 30);
    flowerStyle.setBounds (116, 28, 110, 30);
    syncSwitch.setBounds (770, 28, 116, 30);
    magicSwitch.setBounds (892, 28, 108, 30);
    mutate.setBounds (1006, 28, 72, 30);
    output.setBounds (designWidth - 24 - 56, 12, 56, 62);

    garden.setBounds (gardenArea);
    keyboard.setBounds (keysArea);
    meadowUnder.setBounds (meadowArea);
    meadowOver.setBounds (meadowArea);
    meadow.setBounds (getLocalBounds());
    keyboard.setKeyWidth ((float) keysArea.getWidth() / 42.0f);

    soundPanel = soundArea.toFloat();
    magicPanel = magicArea.toFloat();
    spellLineArea = { mx, 222, mw, 26 };

    for (auto* c : magicComponents())
    {
        c->setAlpha (magicShown);
        c->setVisible (magicShown > 0.02f);
    }
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

    juce::StringArray names;
    for (int i : visibleSpells)
        names.add (capitalised (book[(size_t) i].name));
    spells.setItems (names);
    syncSelections();
}

void Surface::syncSelections()
{
    const int instrument = (int) param (ids::instrument);
    for (int i = 0; i < instruments.size(); ++i)
        instruments[i]->setSelected (i == instrument);

    chords.setSelected ((int) param (ids::chord));
    voicings.setSelected ((int) param (ids::voicing));
    const int oct = (int) param (ids::octave);
    octave.setText (1, oct > 0 ? "+" + juce::String (oct) : juce::String (oct));

    for (int i = 0; i < categories.size(); ++i)
        categories[i]->setSelected (i == category);

    const int spell = processor.getCurrentSpell();
    const auto found = std::find (visibleSpells.begin(), visibleSpells.end(), spell);
    spells.setSelected (found == visibleSpells.end() ? -1 : (int) (found - visibleSpells.begin()));

    magicSwitch.setOn (param (ids::magic) > 0.5f);
    flowerStyle.setOn (processor.photorealFlowers());
    syncSwitch.setOn (param (ids::sync) > 0.5f);
}

void Surface::frame (float dt)
{
    meadow.tick (dt);
    if (meadow.isOpen() && meadow.getAlpha() > 0.99f)
        return;   // the meadow covers everything

    // Fold the Magic Lab away (or back) when its switch changes.
    const float magicTarget = param (ids::magic) > 0.5f ? 1.0f : 0.0f;
    if (std::abs (magicTarget - magicShown) > 0.001f)
    {
        magicShown += (magicTarget - magicShown) * (1.0f - std::exp (-dt / 0.12f));
        if (std::abs (magicTarget - magicShown) < 0.01f)
            magicShown = magicTarget;
        resized();
        for (auto* toggle : { &magicSwitch, &flowerStyle, &syncSwitch })
            toggle->animate();
        repaint();
        return;
    }

    // Parameter changes that alter painted text redraw everything once.
    const int instrument = (int) param (ids::instrument);
    const int spell = processor.getCurrentSpell();
    bool fullRepaint = instrument != shownInstrument || spell != shownSpell;
    shownInstrument = instrument;
    shownSpell = spell;

    // The sky drifts slowly; refresh it a few times a second (and when the
    // window moves to a screen with a different pixel density).
    if (background.isNull() || clock() - backgroundTime > 0.4 || pixelScale() != backgroundScale)
    {
        backgroundTime = clock();
        renderBackground();
        fullRepaint = true;
    }

    syncSelections();
    if (magicShown > 0.0f)
        aura.tick (dt);
    garden.tick (dt);
    // The plants move slowly, so the garden and the grass in front of the keys are redrawn
    // together at half the frame rate (one paint for both).
    const bool plantFrame = (++frameCount & 1) == 0;
    if (meadowUnder.sway (0.0f, 0.0f, pixelScale()))
        meadowUnder.repaint();
    if (plantFrame && meadowOver.sway ((float) clock(), processor.modWheel.load(), pixelScale()))
        meadowOver.repaint();

    if (fullRepaint)
    {
        for (auto* group : { &instruments, &categories })
            for (auto* item : *group)
                item->animate();
        for (auto* toggle : { &magicSwitch, &flowerStyle, &syncSwitch })
            toggle->animate();
        repaint();
        return;
    }

    for (auto* group : { &instruments, &categories })
        for (auto* item : *group)
            if (item->animate())
                item->repaint();
    for (auto* toggle : { &magicSwitch, &flowerStyle, &syncSwitch })
        if (toggle->animate())
            toggle->repaint();
    for (auto* knob : { &tone, &attack, &release, &strum, &forest, &wildlife, &level, &mix, &space, &life, &output })
        knob->refreshIfTouched();
    for (auto* knob : inner)
        knob->refreshIfTouched();

    if (magicShown > 0.0f)
    {
        aura.repaint();
        freeze.repaint();
    }
    if (plantFrame)
        garden.repaint();
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
bool Surface::paintSky (juce::Graphics& g, int pixelW, int pixelH, float t)
{
    if (! processor.photorealFlowers() || ! sprites->available())
        return false;
    const auto sky = sprites->sky();
    if (! sky.isValid())
        return false;

    // Scaled once to the window's pixel height and laid next to its mirror image, so it
    // can scroll on forever without a seam; then only ever blitted.
    if (skyStrip.getHeight() != pixelH)
    {
        const int w = std::max (pixelW, juce::roundToInt ((float) sky.getWidth() * (float) pixelH / (float) sky.getHeight()));
        const auto scaled = sky.rescaled (w, pixelH, juce::Graphics::highResamplingQuality);
        skyStrip = juce::Image (juce::Image::RGB, w * 2, pixelH, false);
        juce::Graphics sg (skyStrip);
        sg.drawImageAt (scaled, 0, 0);
        sg.drawImageTransformed (scaled, juce::AffineTransform::scale (-1.0f, 1.0f).translated ((float) w * 2.0f, 0.0f));
    }

    // The clouds drift steadily across, two points a second.
    const int period = skyStrip.getWidth();
    const int x = (int) std::fmod ((double) t * 2.0 * (double) pixelH / (double) designHeight, (double) period);
    g.setOpacity (1.0f);
    g.drawImageAt (skyStrip, -x, 0);
    if (period - x < pixelW)
        g.drawImageAt (skyStrip, period - x, 0);

    // Bright midday haze, so the glass and the type stay light and clear.
    g.setGradientFill (juce::ColourGradient (colours::frost.withAlpha (0.06f), 0.0f, 0.0f,
                                             colours::paper.withAlpha (0.3f), 0.0f, (float) pixelH, false));
    g.fillAll();

    return true;
}

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

    // Photoreal: a real sky drifting behind the glass. Otherwise the painted aurora.
    const bool photoreal = paintSky (g, pixelW, pixelH, t);
    photorealProblem = processor.photorealFlowers() && ! photoreal ? sprites->problem() : juce::String();
    if (! photoreal)
    {
    // The aurora is painted small and stretched: it is all blur anyway, and it's cheap.
    {
        juce::Graphics ag (aurora);
        const float w = (float) aurora.getWidth(), h = (float) aurora.getHeight();
        juce::ColourGradient base (colours::paper.brighter (0.02f), 0, 0, colours::paperDeep, 0, h, false);
        ag.setGradientFill (base);
        ag.fillAll();

        // Calm daylight on paper: the sun low on the left, a cool sky light on the right,
        // both breathing a little with the sound.
        juce::ColourGradient sun (colours::butter.withAlpha (std::min (0.3f, 0.18f * warm)), w * 0.12f, h * 0.05f,
                                  colours::butter.withAlpha (0.0f), w * 0.7f, h * 0.05f, true);
        ag.setGradientFill (sun);
        ag.fillAll();
        juce::ColourGradient skyLight (colours::sky.withAlpha (std::min (0.16f, 0.08f * cool)), w * 0.95f, h * 0.3f,
                                       colours::sky.withAlpha (0.0f), w * 0.4f, h * 0.3f, true);
        ag.setGradientFill (skyLight);
        ag.fillAll();

        // The ground the keyboard rests on: a soft sage hill along the bottom.
        juce::Path hill;
        hill.startNewSubPath (0.0f, h * 0.9f);
        hill.cubicTo (w * 0.3f, h * 0.875f, w * 0.7f, h * 0.875f, w, h * 0.9f);
        hill.lineTo (w, h);
        hill.lineTo (0.0f, h);
        hill.closeSubPath();
        ag.setGradientFill (juce::ColourGradient (juce::Colour (0xffb8cfa6).withAlpha (0.55f), 0.0f, h * 0.88f,
                                                  juce::Colour (0xff97b585).withAlpha (0.7f), 0.0f, h, false));
        ag.fillPath (hill);
        juce::ignoreUnused (t, breath);
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

    }

    g.addTransform (juce::AffineTransform::scale ((float) scale));

    // The keyboard lies in a meadow (photoreal), or rests on the light (drawn).
    const auto keys = keysArea.toFloat();
    if (! photoreal)
        softShadow (g, keys, 10.0f, colours::ink.withAlpha (0.08f), 12.0f, 6.0f);

    // Glass: the light behind each panel, blurred, under a pale frost.
    paintPanel (g, soundPanel, frost (soundPanel, scale));
    if (magicShown > 0.02f)
    {
        const auto frosted = frost (magicPanel, scale);
        g.beginTransparencyLayer (magicShown);
        paintPanel (g, magicPanel, frosted);
        aura.paintBackdrop (g);
        g.endTransparencyLayer();
    }
}

juce::Image Surface::frost (juce::Rectangle<float> r, int scale) const
{
    // Shrinking the light behind the panel and stretching it back blurs it, cheaply.
    const auto area = (r * (float) scale).getSmallestIntegerContainer().getIntersection (background.getBounds());
    if (area.isEmpty())
        return {};
    const auto behind = background.getClippedImage (area).createCopy();
    return behind.rescaled (std::max (1, area.getWidth() / 16), std::max (1, area.getHeight() / 16),
                            juce::Graphics::mediumResamplingQuality);
}

void Surface::paintPanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::Image& frosted)
{
    // Frosted glass resting in the sun: a soft shadow below, a bright rim on top.
    constexpr float radius = 18.0f;
    juce::Path panel;
    panel.addRoundedRectangle (r, radius);
    softShadow (g, r, radius, colours::ink.withAlpha (0.08f), 18.0f, 8.0f);

    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (panel);
        if (frosted.isValid())
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.setOpacity (1.0f);
            g.drawImage (frosted, r);
        }
        g.setColour (juce::Colour (0xfffcf8f4).withAlpha (0.66f));
        g.fillPath (panel);
    }

    juce::ColourGradient rim (colours::frost.withAlpha (0.95f), r.getX(), r.getY(),
                              colours::ink.withAlpha (0.1f), r.getX(), r.getBottom(), false);
    rim.addColour (0.1, colours::frost.withAlpha (0.5f));
    g.setGradientFill (rim);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

bool MeadowLayer::sway (float time, float wind, int scale)
{
    if (! processor.photorealFlowers() || getWidth() <= 0)
    {
        const bool shown = bent.isValid();
        bent = {};
        return shown;
    }
    const auto full = sprites->meadow (over);
    if (! full.isValid())
        return false;

    const int w = getWidth() * scale, h = getHeight() * scale;
    if (! over && bent.isValid() && bent.getWidth() == w && bent.getHeight() == h)
        return false;   // the field under the keys stays still
    if (source.getWidth() != w || source.getHeight() != h)
    {
        source = full.rescaled (w, h, juce::Graphics::highResamplingQuality).convertedToFormat (juce::Image::ARGB);
        bent = juce::Image (juce::Image::ARGB, w, h, true);
        wave.resize ((size_t) w);
    }

    // A slow wave of wind running along the field, a quicker ripple on top, in gusts.
    const float px = (float) scale;
    const float amp = over ? (1.1f + 4.0f * wind) * (0.7f + 0.3f * std::sin (time * 0.37f)) * px : 0.0f;
    for (int x = 0; x < w; ++x)
    {
        const float lx = (float) x / px;
        wave[(size_t) x] = amp * (std::sin (lx * 0.021f - time * 0.9f) + 0.5f * std::sin (lx * 0.067f - time * 1.7f + 1.7f));
    }

    // Each row slides along the wave, the tips the most and the roots not at all.
    const float tips = over ? 0.25f : 0.1f, roots = over ? 0.8f : 0.7f;
    const juce::Image::BitmapData in (source, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData out (bent, juce::Image::BitmapData::writeOnly);
    const int stride = in.pixelStride;
    for (int y = 0; y < h; ++y)
    {
        const float k = juce::jlimit (0.0f, 1.0f, (roots - (float) y / (float) h) / (roots - tips));
        const juce::uint8* src = in.getLinePointer (y);
        juce::uint8* dst = out.getLinePointer (y);
        if (k * amp * 1.5f < 0.05f)
        {
            std::memcpy (dst, src, (size_t) (w * stride));
            continue;
        }
        for (int x = 0; x < w; ++x, dst += stride)
        {
            const float sx = (float) x - wave[(size_t) x] * k;
            const int i = (int) std::floor (sx);
            const int f = (int) ((sx - (float) i) * 256.0f);
            const juce::uint8* a = src + juce::jlimit (0, w - 1, i) * stride;
            const juce::uint8* b = src + juce::jlimit (0, w - 1, i + 1) * stride;
            for (int c = 0; c < stride; ++c)
                dst[c] = (juce::uint8) ((a[c] * (256 - f) + b[c] * f) >> 8);
        }
    }
    return true;
}

void MeadowLayer::paint (juce::Graphics& g)
{
    if (! bent.isValid())
        return;
    g.setOpacity (1.0f);
    g.drawImage (bent, getLocalBounds().toFloat());

    // The keyboard's shadow on the grass it lies in.
    if (! over)
    {
        const auto keys = keysArea.toFloat() - getPosition().toFloat();
        drawGlow (g, juce::Rectangle<float> (keys.getX() + 6.0f, keys.getBottom() - 10.0f, keys.getWidth() - 12.0f, 22.0f),
                  juce::Colour (0xff0c1806).withAlpha (0.45f));
    }
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
    auto label = [&text] (const juce::String& str, int x, int y, float alpha = 1.0f)
    {
        text (str.toUpperCase(), Fonts::sansMedium (10.0f, 0.14f), ink.withAlpha (0.5f * alpha), { x, y, 300, 14 });
    };

    const auto soundArea = this->soundArea();
    const auto magicArea = this->magicArea();

    // Photoreal is on but can't be shown: say why, rather than quietly drawing.
    if (photorealProblem.isNotEmpty())
        text ("Photoreal is unavailable: " + photorealProblem, Fonts::sans (11.0f), ink.withAlpha (0.6f),
              { 24, 732, designWidth - 48, 16 }, juce::Justification::centred);

    // --- Sound Lab
    const int lx = soundArea.getX() + pad, lw = soundArea.getWidth() - 2 * pad;
    text ("sound lab", Fonts::serifItalic (32.0f), ink, { lx, 98, 240, 40 });
    label ("voice", lx, 156);

    const int instrument = (int) param (ids::instrument);
    auto description = instrumentDescriptions()[instrument];
    if (instrument == 0 && ! processor.isEffect && processor.getMainBusNumInputChannels() == 0)
        description = "route audio into bouquet to hear it";
    text (capitalised (description), Fonts::serifItalic (15.0f), ink.withAlpha (0.72f), { lx, 342, lw, 24 });

    label ("chord", lx, 384);
    label ("voicing", lx, 478);
    label ("octave", lx + 238, 478);

    // --- Magic Lab
    if (magicShown <= 0.02f)
        return;
    const int mx = magicArea.getX() + pad;
    text ("magic lab", Fonts::serifItalic (32.0f), ink.withAlpha (magicShown), { mx, 98, 240, 40 });

    // The spell's name and its line of poetry.
    const int spell = processor.getCurrentSpell();
    if (juce::isPositiveAndBelow (spell, (int) spellBook().size()) && g.clipRegionIntersects (spellLineArea))
    {
        const auto& s = spellBook()[(size_t) spell];
        const auto font = Fonts::serifItalic (15.0f);
        const auto name = capitalised (s.name);
        const int nameW = juce::roundToInt (juce::GlyphArrangement::getStringWidth (font, name));
        text (name, font, ink.withAlpha (magicShown), spellLineArea);
        text (juce::String (juce::CharPointer_UTF8 (" \xe2\x80\x94 ")) + s.description, font, ink.withAlpha (0.72f * magicShown),
              spellLineArea.withTrimmedLeft (nameW));
    }

    label ("scale", harmony.getX() + 8, 498, magicShown);
    label ("grains", mx, 522, magicShown);
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
