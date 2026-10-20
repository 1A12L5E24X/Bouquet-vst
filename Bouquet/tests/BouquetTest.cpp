// Offline tests for Bouquet: every instrument through every spell, the live-input
// path, freeze, state, and a rough CPU measurement.
#include "PluginProcessor.h"
#include "ui/Garden.h"
#include "ui/PluginEditor.h"
#include "ui/Botany.h"
#include "ui/RealFlowers.h"
#include "Assets.h"
#include "dsp/SampleBank.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>
#include <set>

using namespace bouquet;

static int failures = 0;
static void check (bool ok, const juce::String& what)
{
    if (! ok)
    {
        std::cout << "FAIL  " << what << "\n";
        ++failures;
    }
}

struct Stats { float peak = 0.0f; double sumSq = 0.0; long n = 0; bool finite = true;
               float rms() const { return n > 0 ? (float) std::sqrt (sumSq / (double) n) : 0.0f; } };

static constexpr double sr = 48000.0;
static constexpr int block = 512;

static Stats render (BouquetProcessor& p, juce::MidiBuffer& midi, float seconds,
                     std::function<void (juce::AudioBuffer<float>&)> fillInput = {})
{
    Stats s;
    juce::AudioBuffer<float> buf (2, block);
    const int blocks = (int) (seconds * sr / block);
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        if (fillInput) fillInput (buf);
        p.processBlock (buf, midi);
        midi.clear();
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
            {
                const float v = buf.getSample (ch, i);
                s.finite &= std::isfinite (v);
                s.peak = std::max (s.peak, std::abs (v));
                s.sumSq += (double) v * v;
                ++s.n;
            }
    }
    return s;
}

static void set (BouquetProcessor& p, const juce::String& id, float value)
{
    auto* param = p.apvts.getParameter (id);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

static void chordOn (juce::MidiBuffer& m, int root) { for (int k : { 0, 7 }) m.addEvent (juce::MidiMessage::noteOn (1, root + k, 0.8f), 0); }
static void chordOff (juce::MidiBuffer& m, int root) { for (int k : { 0, 7 }) m.addEvent (juce::MidiMessage::noteOff (1, root + k), 0); }

// Renders the editor with a chord playing and writes it to a PNG.
static int snapshot (const juce::File& file, bool effect, int spell, int instrument, const juce::String& mode, float zoom)
{
    BouquetProcessor p (effect);
    p.setPlayConfigDetails (effect ? 2 : 0, 2, sr, block);
    p.prepareToPlay (sr, block);
    p.castSpell (spell, 0.0);
    set (p, ids::instrument, (float) instrument);
    set (p, ids::chord, 5.0f);
    set (p, ids::auraX, 0.62f);
    set (p, ids::auraY, 0.7f);

    const bool rotting = mode == "rot" || mode == "realrot";
    p.setPhotorealFlowers (mode.startsWith ("real"));
    if (mode == "nomagic")
        set (p, ids::magic, 0.0f);
    set (p, ids::wildlife, 0.6f);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    if (zoom > 1.0f) // exercises the high-density (Retina) background path
        editor->setSize (juce::roundToInt ((float) editor->getWidth() * zoom), juce::roundToInt ((float) editor->getHeight() * zoom));
    juce::MidiBuffer midi;
    p.keyboardState.noteOn (1, 57, 0.8f);
    juce::AudioBuffer<float> buf (2, block);
    for (int frame = 0; frame < 120; ++frame)
    {
        for (int b = 0; b < 3; ++b)
        {
            buf.clear();
            p.processBlock (buf, midi);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
    }

    // Paint cost: a full repaint (a few times a second, when the aurora moves) and a
    // typical animation frame (the aura pad, the heart and a handful of glowing items).
    juce::Image target (juce::Image::RGB, editor->getWidth(), editor->getHeight(), false);
    auto timePaint = [&] (const juce::RectangleList<int>& region)
    {
        double total = 0.0;
        for (int i = 0; i < 30; ++i)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (33);
            const auto start = juce::Time::getMillisecondCounterHiRes();
            juce::Graphics g (target);
            g.reduceClipRegion (region);
            editor->paintEntireComponent (g, true);
            total += juce::Time::getMillisecondCounterHiRes() - start;
        }
        return total / 30.0;
    };
    juce::RectangleList<int> partial;
    partial.add ({ 466, 258, 474, 236 });   // aura pad
    partial.add ({ 0, 0, 66, 94 });         // heart
    partial.add ({ 940, 436, 64, 62 });     // freeze orb
    partial.add ({ 466, 146, 162, 38 });    // a glowing spell
    partial.add ({ 46, 162, 180, 34 });     // a glowing instrument
    partial.add ({ 46, 388, 51, 32 });      // a glowing chord
    partial.add ({ 24, 640, 1112, 142 });   // the garden and its grass
    const double full = timePaint (juce::RectangleList<int> (editor->getLocalBounds()));
    const double frame = timePaint (partial);
    std::cout << "Editor paint: full " << full << " ms, animation frame " << frame << " ms\n";

    // Play a little progression so the garden shows bouquets rising, blooming and rotting.
    auto play = [&] (float seconds)
    {
        for (int f = 0; f < (int) (seconds * 60.0f); ++f)
        {
            for (int b = 0; b < 2; ++b)
            {
                buf.clear();
                p.processBlock (buf, midi);
            }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (16);
        }
    };
    p.keyboardState.noteOff (1, 57, 0.0f);
    const int roots[] = { 45, 53, 60, 67 };
    const int shapes[] = { 6, 5, 8, 4 };
    for (int i = 0; i < 4; ++i)
    {
        set (p, ids::chord, (float) shapes[i]);
        p.keyboardState.noteOn (1, roots[i], 0.85f);
        play (i < 3 ? 1.3f : 1.8f);
        if (i < 3 || rotting)
            p.keyboardState.noteOff (1, roots[i], 0.0f);
    }
    if (rotting)
        play (2.2f);
    else
        play (0.6f);   // the last bouquet opens fully
    if (mode == "meadow" || mode == "realmeadow")
    {
        if (auto* bouquetEditor = dynamic_cast<BouquetEditor*> (editor.get()))
            bouquetEditor->showPresetGarden (17);
        play (2.0f);
    }

    // For design mock-ups: the garden alone, on transparency, at twice the size.
    if (const char* gardenFile = std::getenv ("BOUQUET_GARDEN_PNG"))
    {
        std::function<juce::Component* (juce::Component&)> findGarden = [&] (juce::Component& c) -> juce::Component*
        {
            if (dynamic_cast<bouquet::ui::Garden*> (&c) != nullptr)
                return &c;
            for (auto* child : c.getChildren())
                if (auto* found = findGarden (*child))
                    return found;
            return nullptr;
        };
        if (auto* gardenView = findGarden (*editor))
        {
            juce::Image layer (juce::Image::ARGB, gardenView->getWidth() * 2, gardenView->getHeight() * 2, true);
            juce::Graphics lg (layer);
            lg.addTransform (juce::AffineTransform::scale (2.0f));
            gardenView->paintEntireComponent (lg, false);
            juce::File (gardenFile).deleteFile();
            juce::FileOutputStream gout ((juce::File (gardenFile)));
            juce::PNGImageFormat().writeImageToStream (layer, gout);
            std::cout << "Garden layer " << gardenView->getWidth() << "x" << gardenView->getHeight()
                      << " at " << gardenView->getScreenPosition().toString() << "\n";
        }
    }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    return png.writeImageToStream (image, out) ? 0 : 1;
}

// Renders a short chord progression through a spell into a WAV file.
static int renderDemo (const juce::File& file, int spell, int instrument, const juce::String& presetName = {})
{
    BouquetProcessor p (false);
    p.setNonRealtime (true);
    p.setPlayConfigDetails (0, 2, sr, block);
    p.prepareToPlay (sr, block);
    if (auto* preset = p.presets.find (presetName))
    {
        p.applyPreset (*preset, 0.0);
    }
    else
    {
        p.castSpell (spell, 0.0);
        set (p, ids::instrument, (float) instrument);
        set (p, ids::voicing, 1.0f);
        set (p, ids::strum, 40.0f);
    }

    // Am7 - Fmaj7 - Cadd9 - Gsus4, one finger per chord.
    struct Bar { int root; int chord; };
    const Bar bars[] = { { 57, 6 }, { 53, 5 }, { 48, 8 }, { 55, 4 } };
    const float barSeconds = 3.0f, tailSeconds = 5.0f;

    juce::AudioBuffer<float> out (2, (int) ((barSeconds * 4 + tailSeconds) * sr));
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    int written = 0, bar = -1;

    while (written + block <= out.getNumSamples())
    {
        const int nowBar = (int) ((float) written / (float) sr / barSeconds);
        if (nowBar != bar)
        {
            if (bar >= 0 && bar < 4) midi.addEvent (juce::MidiMessage::noteOff (1, bars[bar].root), 0);
            bar = nowBar;
            if (bar < 4)
            {
                if (presetName.isEmpty())
                    set (p, ids::chord, (float) bars[bar].chord);
                midi.addEvent (juce::MidiMessage::noteOn (1, bars[bar].root, 0.75f), 0);
            }
        }
        buf.clear();
        p.processBlock (buf, midi);
        midi.clear();
        for (int ch = 0; ch < 2; ++ch)
            out.copyFrom (ch, written, buf, ch, 0, block);
        written += block;
    }

    file.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (new juce::FileOutputStream (file), sr, 2, 24, {}, 0));
    return writer != nullptr && writer->writeFromAudioSampleBuffer (out, 0, written) ? 0 : 1;
}


// Resident memory of this process in MB (Linux and macOS; 0 elsewhere).
static double residentMB()
{
   #if JUCE_LINUX
    juce::StringArray lines;
    lines.addLines (juce::File ("/proc/self/status").loadFileAsString());
    for (auto& l : lines)
        if (l.startsWith ("VmRSS:"))
            return l.fromFirstOccurrenceOf (":", false, false).trim().getDoubleValue() / 1024.0;
   #endif
    return 0.0;
}

// Measures what the plugin costs: audio CPU per instrument and for the busiest spells,
// the editor's frame and paint times with a full garden, and memory.
static int measure()
{
    const auto names = instrumentNames();
    const auto& book = spellBook();
    std::cout << "Memory at start: " << juce::String (residentMB(), 0) << " MB\n\n";

    // Audio: a held four-note chord through the default spell, as a share of real time.
    auto cpu = [] (BouquetProcessor& p, float seconds, int root)
    {
        juce::MidiBuffer midi;
        for (int k : { 0, 4, 7, 11 })
            midi.addEvent (juce::MidiMessage::noteOn (1, root + k, 0.8f), 0);
        juce::AudioBuffer<float> buf (2, block);
        const int blocks = (int) (seconds * sr / block);
        double worst = 0.0, total = 0.0;
        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            p.processBlock (buf, midi);
            const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
            midi.clear();
            total += ms;
            worst = std::max (worst, ms);
        }
        const double blockMs = 1000.0 * block / sr;
        return std::pair { 100.0 * total / (blocks * blockMs), 100.0 * worst / blockMs };
    };

    std::cout << "Audio, 48 kHz, 512-sample blocks, a held 4-note chord (with the chord shape on top):\n";
    for (int i = 1; i < names.size(); ++i)
    {
        BouquetProcessor p (false);
        p.setNonRealtime (true);
        p.prepareToPlay (sr, block);
        set (p, ids::instrument, (float) i);
        p.setNonRealtime (false);
        cpu (p, 1.0f, 48);   // warm up
        const auto [avg, worst] = cpu (p, 10.0f, 52);
        std::cout << "  " << names[i].paddedRight (' ', 8) << "average " << juce::String (avg, 1) << "%  worst block "
                  << juce::String (worst, 1) << "%\n";
    }

    // The heaviest spells, on the piano.
    std::vector<std::pair<double, int>> spells;
    for (int sp = 0; sp < (int) book.size(); ++sp)
    {
        BouquetProcessor p (false);
        p.setNonRealtime (true);
        p.prepareToPlay (sr, block);
        set (p, ids::instrument, 1.0f);
        p.castSpell (sp, 0.0);
        p.setNonRealtime (false);
        cpu (p, 1.0f, 48);
        spells.push_back ({ cpu (p, 4.0f, 52).first, sp });
    }
    std::sort (spells.rbegin(), spells.rend());
    std::cout << "  heaviest spells on piano:";
    for (int k = 0; k < 3; ++k)
        std::cout << " " << book[(size_t) spells[(size_t) k].second].name << " " << juce::String (spells[(size_t) k].first, 1) << "%";
    std::cout << "\n  lightest spell: " << book[(size_t) spells.back().second].name << " " << juce::String (spells.back().first, 1) << "%\n";
    std::cout << "Memory after audio: " << juce::String (residentMB(), 0) << " MB\n\n";

    // The editor, photoreal, with a garden of twelve bouquets (half of them rotting).
    BouquetProcessor p (false);
    p.setPlayConfigDetails (0, 2, sr, block);
    p.prepareToPlay (sr, block);
    p.setPhotorealFlowers (true);
    std::unique_ptr<juce::AudioProcessorEditor> editor (p.createEditor());
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> buf (2, block);
    auto play = [&] (float seconds)
    {
        for (int f = 0; f < (int) (seconds * 30.0f); ++f)
        {
            for (int b = 0; b < 3; ++b)
            {
                buf.clear();
                p.processBlock (buf, midi);
            }
            juce::MessageManager::getInstance()->runDispatchLoopUntil (33);
        }
    };
    for (int i = 0; i < 12; ++i)
    {
        const int root = 40 + i * 5;
        p.keyboardState.noteOn (1, root, 0.8f);
        play (0.7f);
        if (i % 2 == 0)
            p.keyboardState.noteOff (1, root, 0.0f);
    }
    play (2.0f);
    std::cout << "Memory with the editor open and twelve bouquets: " << juce::String (residentMB(), 0) << " MB\n";

    std::function<juce::Component* (juce::Component&)> findSurface = [&] (juce::Component& c) -> juce::Component*
    {
        if (dynamic_cast<bouquet::ui::Surface*> (&c) != nullptr)
            return &c;
        for (auto* child : c.getChildren())
            if (auto* found = findSurface (*child))
                return found;
        return nullptr;
    };
    auto* surface = dynamic_cast<bouquet::ui::Surface*> (findSurface (*editor));

    const int designW = editor->getWidth(), designH = editor->getHeight();
    for (int scale : { 1, 2 })
    {
        // Sized up, the editor renders its cached layers at that density, as on a Retina screen.
        editor->setSize (designW * scale, designH * scale);
        play (1.0f);
        juce::Image target (juce::Image::RGB, editor->getWidth(), editor->getHeight(), false);
        auto paintMs = [&] (juce::Rectangle<int> area)
        {
            double total = 0.0;
            for (int i = 0; i < 20; ++i)
            {
                juce::Graphics g (target);
                g.reduceClipRegion (area * scale);
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                editor->paintEntireComponent (g, true);
                total += juce::Time::getMillisecondCounterHiRes() - t0;
            }
            return total / 20.0;
        };
        double frameMs = 0.0;
        for (int i = 0; i < 30; ++i)
        {
            const auto t0 = juce::Time::getMillisecondCounterHiRes();
            surface->frame (1.0f / 30.0f);
            frameMs += juce::Time::getMillisecondCounterHiRes() - t0;
        }
        frameMs /= 30.0;
        const double plants = paintMs ({ 0, 630, 1160, 230 });   // the garden and the grass over the keys
        const double pad = paintMs ({ 468, 262, 472, 232 });
        const double full = paintMs ({ 0, 0, designW, designH });
        std::cout << "Editor at " << scale << "x: frame update " << juce::String (frameMs, 2) << " ms; the aura pad (30 a second) "
                  << juce::String (pad, 2) << " ms; the garden and grass (15 a second) " << juce::String (plants, 2)
                  << " ms; a full repaint as the sky moves (2.5 a second) " << juce::String (full, 2) << " ms\n";
        const double perSecond = 30.0 * (frameMs + pad) + 15.0 * plants + 2.5 * full;
        std::cout << "  about " << juce::String (perSecond / 10.0, 1) << "% of one core at 30 fps\n";
    }
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc >= 2 && juce::String (argv[1]) == "--measure")
        return measure();

    if (argc >= 2 && juce::String (argv[1]) == "--where")
    {
        // Where the plugin finds its shipped assets (CI runs this after installing the .pkg).
        const auto flowers = bouquet::assets::find ("Flowers", "daisy0.json");
        const auto samples = bouquet::assets::find ("Samples", "samples.json");
        std::cout << "Flowers: " << (flowers.exists() ? flowers.getFullPathName() : juce::String ("NOT FOUND")) << "\n"
                  << "Samples: " << (samples.exists() ? samples.getFullPathName() : juce::String ("NOT FOUND")) << "\n";
        return flowers.exists() && samples.exists() ? 0 : 1;
    }

    if (argc >= 2 && juce::String (argv[1]) == "--measure-sprites")
    {
        bouquet::ui::real::FlowerSprites sprites;
        sprites.waitForLoads = true;
        juce::Image target (juce::Image::RGB, 2320, 400, true);
        for (auto quality : { juce::Graphics::mediumResamplingQuality, juce::Graphics::highResamplingQuality })
        {
            double flower = 0.0, tuft = 0.0;
            for (int i = 0; i < 40; ++i)
            {
                juce::Graphics g (target);
                g.addTransform (juce::AffineTransform::scale (2.0f));
                g.setImageResamplingQuality (quality);
                auto t0 = juce::Time::getMillisecondCounterHiRes();
                bouquet::ui::real::paintFlower (g, sprites, bouquet::ui::botany::Species::rose, 1, 3, { 200.0f + (float) i * 20.0f, 190.0f },
                                                 110.0f, 0.05f, 1.0f, 1.0f, 0.0f, 1.0f, 0.0f);
                flower += juce::Time::getMillisecondCounterHiRes() - t0;
                const auto image = sprites.grass (i % 8);
                t0 = juce::Time::getMillisecondCounterHiRes();
                g.drawImageTransformed (image, juce::AffineTransform::translation (-(float) image.getWidth() * 0.5f, -sprites.grassGround())
                                                   .sheared (-0.1f, 0.0f).scaled (0.4f).translated (300.0f + (float) i * 10.0f, 190.0f));
                tuft += juce::Time::getMillisecondCounterHiRes() - t0;
            }
            std::cout << (quality == juce::Graphics::highResamplingQuality ? "high" : "medium") << ": one flower " << juce::String (flower / 40.0, 3)
                      << " ms, one grass tuft " << juce::String (tuft / 40.0, 3) << " ms (2x)\n";
        }
        return 0;
    }

    if (argc >= 3 && juce::String (argv[1]) == "--species")
    {
        // Debug aid: every species, fresh and rotting, on one sheet.
        using namespace bouquet::ui::botany;
        juce::Image img (juce::Image::RGB, 150 * (int) Species::count, 360, true);
        juce::Graphics g (img);
        g.fillAll (juce::Colour (0xfffbf7f0));
        for (int s = 0; s < (int) Species::count; ++s)
            for (int row = 0; row < 2; ++row)
            {
                const auto genome = makeGenome ((Species) s, juce::Colour (0xffe4789b), 16.0f, 99u + (uint32_t) s);
                Pose pose;
                const float x = 75.0f + 150.0f * (float) s, y = 170.0f + 180.0f * (float) row;
                pose.base = { x, y };
                pose.c1 = { x, y - 40.0f };
                pose.c2 = { x + 10.0f, y - 90.0f };
                pose.head = { x + 15.0f, y - 130.0f };
                pose.rot = row == 0 ? 0.0f : 0.5f;
                pose.time = 1.0f;
                paintPlant (g, genome, pose);
            }
        const juce::File file { juce::String (argv[2]) };
        file.deleteFile();
        juce::FileOutputStream out { file };
        return juce::PNGImageFormat().writeImageToStream (img, out) ? 0 : 1;
    }

    if (argc >= 3 && juce::String (argv[1]) == "--sprites")
    {
        // Writes a few decoded sprites (top view, a frame, a cross-fade) for inspection.
        juce::ScopedJuceInitialiser_GUI gui;
        bouquet::ui::real::FlowerSprites sprites;
        sprites.waitForLoads = true;
        juce::Image sheet (juce::Image::ARGB, 1100, 450, true);
        juce::Graphics g (sheet);
        g.fillAll (juce::Colour (0xff6e8c46));
        g.drawImageAt (sprites.top (bouquet::ui::botany::Species::rose, 0, 5), 0, 0);
        g.drawImageAt (sprites.top (bouquet::ui::botany::Species::daisy, 0, 1), 256, 0);
        g.drawImageAt (sprites.get (bouquet::ui::botany::Species::daisy, 0, 1, 9.0f).image, 520, 0);
        g.drawImageAt (sprites.get (bouquet::ui::botany::Species::daisy, 0, 1, 15.5f).image, 810, 0);
        const auto top = sprites.top (bouquet::ui::botany::Species::rose, 0, 5);
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageTransformed (top, juce::AffineTransform::translation (-128.0f, -128.0f).rotated (0.7f).scaled (0.35f).translated (60.0f, 380.0f));
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImageTransformed (top, juce::AffineTransform::translation (-128.0f, -128.0f).rotated (0.7f).scaled (0.35f).translated (160.0f, 380.0f));
        g.drawImageTransformed (top, juce::AffineTransform::translation (-128.0f, -128.0f).scaled (0.35f).translated (260.0f, 380.0f));
        {
            const auto dir = bouquet::assets::find ("Flowers", "daisy0.json");
            for (int i = 0; i < 3; ++i)
            {
                const auto t0 = juce::Time::getMillisecondCounterHiRes();
                const auto c = juce::ImageFileFormat::loadFrom (dir.getChildFile ("rose1_pink.jpg"));
                const auto t1 = juce::Time::getMillisecondCounterHiRes();
                const auto a = juce::ImageFileFormat::loadFrom (dir.getChildFile ("rose1_pink.alpha.png"));
                const auto t2 = juce::Time::getMillisecondCounterHiRes();
                const auto img = bouquet::ui::real::FlowerSprites::loadWithAlpha (dir.getChildFile ("rose1_pink.jpg"), dir.getChildFile ("rose1_pink.alpha.png"), 1212, 1208);
                const auto t3 = juce::Time::getMillisecondCounterHiRes();
                std::cout << "jpeg " << (t1 - t0) << " ms, png " << (t2 - t1) << " ms, combined " << (t3 - t2) << " ms " << c.getFormat() << " " << a.getFormat() << "\n";
            }
        }
        // Growing, blooming, rotting, through paintFlower.
        const float blooms[] = { 0.0f, 0.3f, 0.6f, 1.0f, 1.0f, 1.0f };
        const float rots[] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.3f, 0.7f };
        for (int i = 0; i < 6; ++i)
            bouquet::ui::real::paintFlower (g, sprites, bouquet::ui::botany::Species::tulip, 1, 3, { 380.0f + (float) i * 120.0f, 440.0f },
                                            120.0f, 0.0f, 1.0f, blooms[i], rots[i], 1.0f, 0.0f);
        juce::FileOutputStream out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
        out.setPosition (0);
        out.truncate();
        juce::PNGImageFormat().writeImageToStream (sheet, out);
        return 0;
    }
    if (argc >= 4 && juce::String (argv[1]) == "--demo-preset")
        return renderDemo (juce::File (argv[2]), 0, 1, argv[3]);

    if (argc >= 5 && juce::String (argv[1]) == "--demo")
        return renderDemo (juce::File (argv[2]), juce::String (argv[3]).getIntValue(), juce::String (argv[4]).getIntValue());

    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
        return snapshot (juce::File (argv[2]), argc > 3 && juce::String (argv[3]) == "fx",
                         argc > 4 ? juce::String (argv[4]).getIntValue() : 0,
                         argc > 5 ? juce::String (argv[5]).getIntValue() : 1,
                         argc > 6 ? juce::String (argv[6]) : juce::String(),
                         argc > 7 ? juce::String (argv[7]).getFloatValue() : 1.0f);

    const auto names = instrumentNames();
    const auto& book = spellBook();

    // 1. Each instrument, dry, to check levels are sane and voices end.
    {
        BouquetProcessor p (false);
        p.setNonRealtime (true);   // the recordings load before the first note
        p.setPlayConfigDetails (0, 2, sr, block);
        p.prepareToPlay (sr, block);
        set (p, ids::mix, 0.0f);
        set (p, ids::release, 0.3f);
        juce::MidiBuffer midi;
        std::cout << "Dry instrument levels (two-note chord):\n";
        for (int inst = 1; inst < (int) Instrument::count; ++inst)
        {
            set (p, ids::instrument, (float) inst);
            chordOn (midi, 60);
            const auto on = render (p, midi, 1.5f);
            chordOff (midi, 60);
            render (p, midi, 12.0f);
            const bool ended = p.soundingVoices.load() == 0;
            std::cout << "  " << names[inst].paddedRight (' ', 10) << " peak " << juce::String (on.peak, 3)
                      << "  rms " << juce::String (on.rms(), 3) << (ended ? "" : "  (still sounding)") << "\n";
            check (on.finite, names[inst] + " finite");
            check (on.peak > 0.02f && on.peak < 1.0f, names[inst] + " level in range");
            check (ended, names[inst] + " voices end after release");
        }
    }

    // 2. Every spell with a sustained and a percussive voice.
    {
        std::cout << "Spells (piano + violin, wet):\n";
        for (int spell = 0; spell < (int) book.size(); ++spell)
        {
            BouquetProcessor p (false);
            p.setPlayConfigDetails (0, 2, sr, block);
            p.prepareToPlay (sr, block);
            p.castSpell (spell, 0.0);
            check (p.getCurrentSpell() == spell, "spell is remembered");
            juce::MidiBuffer midi;
            float peak = 0.0f, rms = 0.0f;
            bool finite = true;
            for (int inst : { (int) Instrument::piano, (int) Instrument::violin })
            {
                set (p, ids::instrument, (float) inst);
                chordOn (midi, 57);
                auto s = render (p, midi, 2.0f);
                chordOff (midi, 57);
                auto tail = render (p, midi, 2.0f);
                peak = std::max ({ peak, s.peak, tail.peak });
                rms = std::max (rms, s.rms());
                finite &= s.finite && tail.finite;
            }
            std::cout << "  " << juce::String (book[(size_t) spell].name).paddedRight (' ', 18)
                      << " peak " << juce::String (peak, 3) << "  rms " << juce::String (rms, 3) << "\n";
            check (finite, juce::String (book[(size_t) spell].name) + " finite");
            check (peak > 0.02f && peak <= 1.0f, juce::String (book[(size_t) spell].name) + " level in range");
        }
    }

    // 3. Effect build: granulates the input; freeze holds it after the input stops.
    {
        BouquetProcessor p (true);
        p.setPlayConfigDetails (2, 2, sr, block);
        p.prepareToPlay (sr, block);
        check (p.apvts.getRawParameterValue (ids::instrument)->load() < 0.5f, "fx opens on live input");
        juce::MidiBuffer midi;
        double phase = 0.0;
        auto sine = [&phase] (juce::AudioBuffer<float>& b)
        {
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const float v = 0.3f * (float) std::sin (phase);
                phase += juce::MathConstants<double>::twoPi * 220.0 / sr;
                b.setSample (0, i, v);
                b.setSample (1, i, v);
            }
        };
        p.castSpell (8, 0.0); // frozen breath
        set (p, ids::freeze, 0.0f);
        auto s = render (p, midi, 2.0f, sine);
        check (s.finite && s.peak > 0.05f, "fx granulates live input");
        set (p, ids::freeze, 1.0f);
        set (p, ids::mix, 1.0f);
        render (p, midi, 0.5f);
        auto held = render (p, midi, 3.0f);
        std::cout << "Freeze after input stops: rms " << held.rms() << "\n";
        check (held.rms() > 0.01f, "freeze keeps singing after the input stops");
    }

    // 4. The garden: note events reach the UI, and chords grow their own bouquets.
    {
        BouquetProcessor p (false);
        p.setPlayConfigDetails (0, 2, sr, block);
        p.prepareToPlay (sr, block);
        set (p, ids::instrument, (float) Instrument::piano);
        set (p, ids::chord, 6.0f); // min7: one key, four notes
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 57, 0.8f), 0);
        render (p, midi, 0.2f);
        midi.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
        render (p, midi, 0.2f);

        std::array<NoteEvent, 64> ev {};
        const int n = p.popNoteEvents (ev.data(), (int) ev.size());
        int ons = 0, offs = 0;
        for (int i = 0; i < n; ++i)
            (ev[(size_t) i].on ? ons : offs)++;
        check (ons == 4 && offs == 4, "a min7 chord sends four note starts and four releases (got "
                                       + juce::String (ons) + "/" + juce::String (offs) + ")");
        check (n > 0 && ev[0].on && juce::roundToInt (ev[0].note) == 57, "the root arrives first");

        using bouquet::ui::Garden;
        const auto am7 = Garden::seedFor ({ 57, 60, 64, 67 }, 6, 1, 0.8f);
        check (am7 == Garden::seedFor ({ 67, 64, 60, 57 }, 6, 1, 0.8f), "same chord, same bouquet");
        check (am7 != Garden::seedFor ({ 60, 64, 67, 69 }, 6, 1, 0.8f), "C6 and Am7 share notes but not a bouquet");
        check (am7 != Garden::seedFor ({ 57, 60, 64, 67 }, 6, 7, 0.8f), "a different instrument grows a different bouquet");
        check (am7 != Garden::seedFor ({ 53, 57, 60, 64 }, 5, 1, 0.8f), "a different chord grows a different bouquet");
    }

    // 5. Presets: the whole instrument, factory and planted.
    {
        BouquetProcessor p (false);
        const auto temp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("bouquet-test-presets");
        temp.deleteRecursively();
        p.presets.setUserFolder (temp);

        const auto& factory = PresetLibrary::factory();
        check (factory.size() >= 20, "at least twenty factory presets");
        std::set<uint32_t> seeds;
        for (auto& f : factory)
            seeds.insert (f.seed());
        check (seeds.size() == factory.size(), "every factory preset grows a different flower");

        const auto& nocturne = *p.presets.find ("nocturne");
        p.applyPreset (nocturne, 0.0);
        check (std::abs (p.apvts.getRawParameterValue (ids::instrument)->load() - nocturne.get (ids::instrument)) < 0.5f, "preset sets the voice");
        check (std::abs (p.apvts.getRawParameterValue (ids::decay)->load() - nocturne.get (ids::decay)) < 0.01f, "preset sets the Magic Lab");
        check (p.getCurrentPresetName() == "nocturne", "preset name remembered");

        set (p, ids::labTone, 0.91f);
        const auto planted = p.plantPreset ("test bloom");
        check (p.presets.user().size() == 1 && temp.getChildFile ("test bloom.bouquet").existsAsFile(), "planting saves a preset file");
        check (planted.seed() != nocturne.seed(), "a planted preset grows its own flower");

        set (p, ids::labTone, 0.1f);
        p.applyPreset (*p.presets.find ("test bloom"), 0.0);
        check (std::abs (p.apvts.getRawParameterValue (ids::labTone)->load() - 0.91f) < 0.01f, "planted preset loads back");

        p.presets.remove (*p.presets.find ("test bloom"));
        check (p.presets.user().empty(), "uprooting deletes it");
        temp.deleteRecursively();
    }

    // 6. Magic Lab off: the engine is bypassed entirely; the forest still sings and then falls quiet.
    {
        BouquetProcessor p (false);
        p.setPlayConfigDetails (0, 2, sr, block);
        p.prepareToPlay (sr, block);
        set (p, ids::magic, 0.0f);
        set (p, ids::instrument, (float) Instrument::violin);
        set (p, ids::forest, 1.0f);
        set (p, ids::wildlife, 1.0f);
        set (p, ids::release, 0.3f);
        juce::MidiBuffer midi;
        chordOn (midi, 60);
        const auto on = render (p, midi, 2.0f);
        check (on.finite && on.peak > 0.02f && on.peak < 1.0f, "forest instrument audible with the Magic Lab off");
        check (p.engine.activeGrains.load() == 0, "no grains while the Magic Lab is off");
        chordOff (midi, 60);
        render (p, midi, 12.0f);
        const auto after = render (p, midi, 1.0f);
        std::cout << "Forest settles after playing: rms " << after.rms() << "\n";
        check (after.rms() < 0.002f, "the forest ambience fades once you stop playing");
    }

    // 7. Rendered flowers: found, and every atlas there has all its frames.
    {
        bouquet::ui::real::FlowerSprites sprites;
        sprites.waitForLoads = true;
        const auto folder = bouquet::assets::find ("Flowers", "daisy0.json");
        const char* speciesNames[] = { "daisy", "tulip", "poppy", "cosmos", "bell", "rose",
                                       "lily", "iris", "dahlia", "peony", "foxglove", "allium" };
        const char* colourNames[] = { "white", "butter", "apricot", "coral", "crimson", "pink", "lilac", "blue" };
        if (! sprites.available())
            std::cout << "Rendered flowers not present; skipping photoreal checks\n";
        else
        {
            const float last = (float) (sprites.bloomFrames() + sprites.rotFrames() - 1);
            int atlases = 0;
            for (int s = 0; s < (int) bouquet::ui::botany::Species::count; ++s)
                for (int v = 0; v < bouquet::ui::real::FlowerSprites::numVariants; ++v)
                    for (int c = 0; c < 8; ++c)
                    {
                        const auto name = juce::String (speciesNames[s]) + juce::String (v) + "_" + colourNames[c];
                        if (! folder.getChildFile (name + ".jpg").existsAsFile())
                            continue;
                        ++atlases;
                        const auto species = (bouquet::ui::botany::Species) s;
                        auto first = sprites.get (species, v, c, 0.0f);
                        auto between = sprites.get (species, v, c, 3.5f);
                        auto end = sprites.get (species, v, c, last);
                        check (first.image.isValid() && between.image.isValid() && end.image.isValid() && ! end.stem.empty(),
                               "rendered frames for " + name);
                    }
            std::cout << "Checked " << atlases << " flower atlases\n";
            check (std::abs (sprites.framePosition (0.0f, 0.0f)) < 1e-4f && std::abs (sprites.framePosition (1.0f, 1.0f) - last) < 1e-4f, "frame positions cover bud to rot");
            check (sprites.top (bouquet::ui::botany::Species::rose, 0, 5).isValid(), "flowers seen from above");
            check (sprites.grassCount() > 0 && sprites.grass (0).isValid(), "grass tufts");
            check (sprites.fly (0).isValid() && sprites.drop().isValid(), "fly and blood-drop sprites");
            check (sprites.ground().isValid(), "the meadow ground");
        }
    }

    // 7b. Recordings: every sampled instrument loads and sounds, sustained ones loop.
    {
        juce::SharedResourcePointer<bouquet::SampleBank> bank;
        if (! bank->available())
            std::cout << "Instrument samples not present; skipping sample checks\n";
        else
            for (auto inst : { Instrument::piano, Instrument::rhodes, Instrument::marimba, Instrument::harp,
                               Instrument::violin, Instrument::cello, Instrument::flute })
            {
                const auto* s = bank->load (inst);
                check (s != nullptr && ! s->regions.empty(), "samples load for " + names[(int) inst]);
                if (s == nullptr || s->regions.empty())
                    continue;
                check (s->find (60.0f, 0.8f) != nullptr, "a sample covers middle C on " + names[(int) inst]);

                BouquetProcessor p (false);
                p.setNonRealtime (true);
                p.setPlayConfigDetails (0, 2, sr, block);
                p.prepareToPlay (sr, block);
                set (p, ids::magic, 0.0f);
                set (p, ids::forest, 0.0f);
                set (p, ids::wildlife, 0.0f);
                set (p, ids::chord, 0.0f);
                set (p, ids::instrument, (float) inst);
                juce::MidiBuffer midi;
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                render (p, midi, 0.05f);
                const auto held = render (p, midi, s->sustained ? 6.0f : 0.5f);
                check (held.finite && held.peak > 0.02f && held.peak < 1.0f, "recorded " + names[(int) inst] + " sounds");
                if (s->sustained)
                {
                    // Past the end of the recording, the loop keeps the note going.
                    const auto later = render (p, midi, 0.5f);
                    check (later.rms() > 0.005f, names[(int) inst] + " sustains through its loop");
                }
            }
    }

    // 7c. Tempo sync: grains follow the host's beat grid.
    {
        check (std::abs (bouquet::GranularEngine::syncDivision (8.0f, 120.0) - 0.25) < 1e-9, "8 grains a second at 120 bpm are sixteenths");
        check (std::abs (bouquet::GranularEngine::syncDivision (3.0f, 120.0) - 2.0 / 3.0) < 1e-9, "3 a second are quarter-note triplets");

        bouquet::GranularEngine e;
        e.prepare (sr);
        bouquet::GrainSettings g;
        g.density = 4.0f;
        g.chaos = 0.0f;
        g.sync = true;
        g.bpm = 120.0;
        g.playing = true;
        std::vector<float> l ((size_t) block, 0.1f), r ((size_t) block, 0.1f), ol ((size_t) block), orr ((size_t) block);
        std::array<bouquet::GrainEvent, 512> events {};
        int spawned = 0;
        const int blocks = (int) (3.0 * sr / block);
        for (int b = 0; b < blocks; ++b)
        {
            g.ppq = (double) b * block / sr * 2.0;   // two beats a second
            e.process (l.data(), r.data(), ol.data(), orr.data(), block, g);
            const int n = e.popEvents (events.data(), (int) events.size());
            if (b >= blocks / 3)                     // after the memory has filled
                spawned += n;
        }
        std::cout << "Synced grains in 2 s at 120 bpm, eighths: " << spawned << "\n";
        check (spawned >= 6 && spawned <= 8, "synced grains fall on eighth notes");
    }

    // 7d. Expression and dynamics: aftertouch swells a held note; velocity cross-fades layers.
    {
        juce::SharedResourcePointer<bouquet::SampleBank> bank;
        if (const auto* piano = bank->load (Instrument::piano); piano != nullptr && ! piano->regions.empty())
        {
            const auto mid = piano->findLayers (60.0f, 0.55f);
            check (mid.a != nullptr && mid.b != nullptr && mid.mix > 0.0f && mid.mix < 1.0f, "piano blends its soft and loud recordings");
            check (piano->findLayers (60.0f, 1.0f).b == nullptr, "full velocity plays the loudest recording");
        }

        BouquetProcessor p (false);
        p.setNonRealtime (true);
        p.setPlayConfigDetails (0, 2, sr, block);
        p.prepareToPlay (sr, block);
        set (p, ids::magic, 0.0f);
        set (p, ids::forest, 0.0f);
        set (p, ids::wildlife, 0.0f);
        set (p, ids::chord, 0.0f);
        set (p, ids::labTone, 0.2f);
        set (p, ids::instrument, (float) Instrument::flute);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 72, 0.6f), 0);
        render (p, midi, 1.0f);
        const auto before = render (p, midi, 1.0f);
        midi.addEvent (juce::MidiMessage::channelPressureChange (1, 127), 0);
        render (p, midi, 0.3f);
        const auto pressed = render (p, midi, 1.0f);
        std::cout << "Aftertouch: rms " << before.rms() << " -> " << pressed.rms() << ", pressure " << p.keyPressure.load() << "\n";
        check (pressed.rms() > before.rms() * 1.2f && p.keyPressure.load() > 0.9f, "pressing into a held key swells it");
    }

    // 8. State round trip.
    {
        BouquetProcessor p (false);
        p.castSpell (24, 0.0);
        set (p, ids::chord, 5.0f);
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        BouquetProcessor q (false);
        q.setStateInformation (mb.getData(), (int) mb.getSize());
        check (q.getCurrentSpell() == 24, "spell restored");
        check (std::abs (q.apvts.getRawParameterValue (ids::chord)->load() - 5.0f) < 0.5f, "chord restored");
        check (std::abs (q.apvts.getRawParameterValue (ids::feedback)->load() - book[24].values.feedback) < 1e-3f, "feedback restored");
    }

    // 9. CPU: dense spell, four-note chords on the choir with chord mode on.
    {
        BouquetProcessor p (false);
        p.setPlayConfigDetails (0, 2, sr, block);
        p.prepareToPlay (sr, block);
        p.castSpell (18, 0.0); // murmuration
        set (p, ids::instrument, (float) Instrument::choir);
        set (p, ids::chord, 9.0f);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 48, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        const auto start = juce::Time::getMillisecondCounterHiRes();
        const float seconds = 10.0f;
        render (p, midi, seconds);
        const double elapsed = (juce::Time::getMillisecondCounterHiRes() - start) * 0.001;
        std::cout << "CPU: " << seconds << " s of audio in " << elapsed << " s ("
                  << juce::String (100.0 * elapsed / seconds, 1) << "% of one core), "
                  << p.engine.activeGrains.load() << " grains, " << p.soundingVoices.load() << " voices\n";
        check (elapsed < seconds * 0.5, "renders comfortably faster than real time");
    }

    std::cout << (failures == 0 ? "All tests passed\n" : juce::String (failures) + " checks FAILED\n");
    return failures == 0 ? 0 : 1;
}
