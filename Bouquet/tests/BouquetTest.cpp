// Offline tests for Bouquet: every instrument through every spell, the live-input
// path, freeze, state, and a rough CPU measurement.
#include "PluginProcessor.h"
#include "ui/Garden.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <iostream>

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
static int snapshot (const juce::File& file, bool effect, int spell, int instrument, bool rotting, float zoom)
{
    BouquetProcessor p (effect);
    p.setPlayConfigDetails (effect ? 2 : 0, 2, sr, block);
    p.prepareToPlay (sr, block);
    p.castSpell (spell, 0.0);
    set (p, ids::instrument, (float) instrument);
    set (p, ids::chord, 5.0f);
    set (p, ids::auraX, 0.62f);
    set (p, ids::auraY, 0.7f);

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

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    return png.writeImageToStream (image, out) ? 0 : 1;
}

// Renders a short chord progression through a spell into a WAV file.
static int renderDemo (const juce::File& file, int spell, int instrument)
{
    BouquetProcessor p (false);
    p.setPlayConfigDetails (0, 2, sr, block);
    p.prepareToPlay (sr, block);
    p.castSpell (spell, 0.0);
    set (p, ids::instrument, (float) instrument);
    set (p, ids::voicing, 1.0f);
    set (p, ids::strum, 40.0f);

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

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc >= 5 && juce::String (argv[1]) == "--demo")
        return renderDemo (juce::File (argv[2]), juce::String (argv[3]).getIntValue(), juce::String (argv[4]).getIntValue());

    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
        return snapshot (juce::File (argv[2]), argc > 3 && juce::String (argv[3]) == "fx",
                         argc > 4 ? juce::String (argv[4]).getIntValue() : 0,
                         argc > 5 ? juce::String (argv[5]).getIntValue() : 1,
                         argc > 6 && juce::String (argv[6]) == "rot",
                         argc > 7 ? juce::String (argv[7]).getFloatValue() : 1.0f);

    const auto names = instrumentNames();
    const auto& book = spellBook();

    // 1. Each instrument, dry, to check levels are sane and voices end.
    {
        BouquetProcessor p (false);
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

    // 5. State round trip.
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

    // 6. CPU: dense spell, four-note chords on the choir with chord mode on.
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
