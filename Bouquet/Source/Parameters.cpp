#include "Parameters.h"
#include "Spells.h"

namespace bouquet
{
juce::StringArray instrumentNames()
{
    return { "live input", "piano", "rhodes", "marimba", "glass", "harp", "violin", "cello", "flute", "choir" };
}

juce::StringArray instrumentDescriptions()
{
    return {
        "whatever is playing into the track",
        "felt hammers, long strings, a quiet room",
        "warm tines, a bell inside a cloud",
        "wooden bars, round and hollow",
        "struck crystal, ringing forever",
        "plucked strings, bright and patient",
        "a bow drawn slowly across the strings",
        "low, close, the voice of the wood",
        "breath turned into a tone",
        "three voices, singing as one",
    };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool isEffect)
{
    using namespace juce;
    using Float = AudioParameterFloat;
    using Choice = AudioParameterChoice;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto range = [] (float lo, float hi, float centre)
    {
        NormalisableRange<float> r (lo, hi);
        r.setSkewForCentre (centre);
        return r;
    };
    auto attrs = [] (const char* label) { return AudioParameterFloatAttributes().withLabel (label); };
    auto pct = AudioParameterFloatAttributes()
                   .withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + "%"; });

    // Every magic-lab default comes from the first spell, so a fresh instance opens on it.
    const auto& d = spellBook().front().values;

    // --- Sound Lab
    StringArray chordNames;
    for (auto& c : chordShapes())
        chordNames.add (c.name);

    layout.add (std::make_unique<Choice> (ParameterID { ids::instrument, 1 }, "Instrument", instrumentNames(), isEffect ? 0 : 1));
    layout.add (std::make_unique<Choice> (ParameterID { ids::chord, 1 }, "Chord", chordNames, 0));
    layout.add (std::make_unique<Choice> (ParameterID { ids::voicing, 1 }, "Voicing", StringArray { "close", "open", "wide" }, 0));
    layout.add (std::make_unique<Float> (ParameterID { ids::strum, 1 }, "Strum", range (0.0f, 400.0f, 60.0f), 0.0f, attrs ("ms")));
    layout.add (std::make_unique<Float> (ParameterID { ids::labTone, 1 }, "Tone", NormalisableRange<float> (0.0f, 1.0f), 0.5f, pct));
    layout.add (std::make_unique<Float> (ParameterID { ids::attack, 1 }, "Attack", range (0.0f, 4.0f, 0.3f), 0.0f, attrs ("s")));
    layout.add (std::make_unique<Float> (ParameterID { ids::release, 1 }, "Release", range (0.02f, 10.0f, 1.0f), 0.8f, attrs ("s")));
    layout.add (std::make_unique<AudioParameterInt> (ParameterID { ids::octave, 1 }, "Octave", -2, 2, 0));
    layout.add (std::make_unique<Float> (ParameterID { ids::sourceLevel, 1 }, "Source Level", NormalisableRange<float> (-36.0f, 6.0f), 0.0f, attrs ("dB")));

    // --- Magic Lab
    StringArray harmonyNames;
    for (auto& h : harmonySets())
        harmonyNames.add (h.name);

    layout.add (std::make_unique<Float> (ParameterID { ids::density, 1 }, "Density", range (0.5f, 80.0f, 12.0f), d.density, attrs ("/s")));
    layout.add (std::make_unique<Float> (ParameterID { ids::size, 1 }, "Grain Size", range (8.0f, 2000.0f, 200.0f), d.sizeMs, attrs ("ms")));
    layout.add (std::make_unique<Float> (ParameterID { ids::time, 1 }, "Reach", range (0.0f, 6.0f, 0.6f), d.timeSec, attrs ("s")));
    layout.add (std::make_unique<Float> (ParameterID { ids::spray, 1 }, "Spray", NormalisableRange<float> (0.0f, 1.0f), d.spray, pct));
    layout.add (std::make_unique<Float> (ParameterID { ids::pitch, 1 }, "Pitch", NormalisableRange<float> (-24.0f, 24.0f, 0.01f), d.pitch, attrs ("st")));
    layout.add (std::make_unique<Choice> (ParameterID { ids::harmony, 1 }, "Harmony", harmonyNames, d.harmony));
    layout.add (std::make_unique<Float> (ParameterID { ids::harmonyAmount, 1 }, "Harmony Amount", NormalisableRange<float> (0.0f, 1.0f), d.harmonyAmount, pct));
    layout.add (std::make_unique<Float> (ParameterID { ids::detune, 1 }, "Detune", range (0.0f, 50.0f, 10.0f), d.detune, attrs ("ct")));
    layout.add (std::make_unique<Float> (ParameterID { ids::reverse, 1 }, "Reverse", NormalisableRange<float> (0.0f, 1.0f), d.reverse, pct));
    layout.add (std::make_unique<Float> (ParameterID { ids::chaos, 1 }, "Chaos", NormalisableRange<float> (0.0f, 1.0f), d.chaos, pct));
    layout.add (std::make_unique<Float> (ParameterID { ids::shape, 1 }, "Edge", NormalisableRange<float> (0.0f, 1.0f), d.shape, pct));
    layout.add (std::make_unique<Float> (ParameterID { ids::width, 1 }, "Width", NormalisableRange<float> (0.0f, 1.0f), d.width, pct));
    layout.add (std::make_unique<Float> (ParameterID { ids::feedback, 1 }, "Feedback", NormalisableRange<float> (0.0f, 0.95f), d.feedback, pct));
    layout.add (std::make_unique<Float> (ParameterID { ids::colour, 1 }, "Colour", NormalisableRange<float> (-1.0f, 1.0f), d.colour));
    layout.add (std::make_unique<Float> (ParameterID { ids::space, 1 }, "Space", NormalisableRange<float> (0.0f, 1.0f), d.space, pct));
    layout.add (std::make_unique<Float> (ParameterID { ids::decay, 1 }, "Space Decay", range (0.5f, 30.0f, 5.0f), d.decay, attrs ("s")));
    layout.add (std::make_unique<Float> (ParameterID { ids::life, 1 }, "Life", NormalisableRange<float> (0.0f, 1.0f), d.life, pct));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { ids::freeze, 1 }, "Freeze", d.freeze));
    layout.add (std::make_unique<Float> (ParameterID { ids::mix, 1 }, "Mix", NormalisableRange<float> (0.0f, 1.0f), d.mix, pct));
    layout.add (std::make_unique<Float> (ParameterID { ids::auraX, 1 }, "Aura X", NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    layout.add (std::make_unique<Float> (ParameterID { ids::auraY, 1 }, "Aura Y", NormalisableRange<float> (0.0f, 1.0f), 0.5f));
    layout.add (std::make_unique<Float> (ParameterID { ids::output, 1 }, "Output", NormalisableRange<float> (-24.0f, 6.0f), 0.0f, attrs ("dB")));

    return layout;
}

namespace
{
    float get (const juce::AudioProcessorValueTreeState& s, const juce::String& id)
    {
        return s.getRawParameterValue (id)->load();
    }
}

SoundLabSettings readSoundLab (const juce::AudioProcessorValueTreeState& s)
{
    SoundLabSettings out;
    out.instrument = (Instrument) std::clamp ((int) get (s, ids::instrument), 0, (int) Instrument::count - 1);
    out.chord = (int) get (s, ids::chord);
    out.voicing = (int) get (s, ids::voicing);
    out.strumMs = get (s, ids::strum);
    out.tone = get (s, ids::labTone);
    out.attack = get (s, ids::attack);
    out.release = get (s, ids::release);
    out.octave = (int) get (s, ids::octave);
    return out;
}

GrainSettings readGrains (const juce::AudioProcessorValueTreeState& s)
{
    GrainSettings g;
    g.density = get (s, ids::density);
    g.sizeSec = get (s, ids::size) * 0.001f;
    g.timeSec = get (s, ids::time);
    g.spray = get (s, ids::spray);
    g.pitch = get (s, ids::pitch);
    g.harmony = (int) get (s, ids::harmony);
    g.harmonyAmount = get (s, ids::harmonyAmount);
    g.detuneCents = get (s, ids::detune);
    g.reverse = get (s, ids::reverse);
    g.chaos = get (s, ids::chaos);
    g.shape = get (s, ids::shape);
    g.width = get (s, ids::width);
    g.feedback = get (s, ids::feedback);
    g.tone = get (s, ids::colour);
    g.space = get (s, ids::space);
    g.decaySec = get (s, ids::decay);
    g.life = get (s, ids::life);
    g.freeze = get (s, ids::freeze) > 0.5f;
    g.mix = get (s, ids::mix);

    // The aura pad: left = fine dust, right = long smears; down = close, up = vast.
    const float x = (get (s, ids::auraX) - 0.5f) * 2.0f;
    const float y = (get (s, ids::auraY) - 0.5f) * 2.0f;
    g.sizeSec = std::clamp (g.sizeSec * std::exp2 (2.5f * x), 0.005f, 3.0f);
    g.density = std::clamp (g.density * std::exp2 (-1.5f * x), 0.3f, 120.0f);
    g.feedback = std::clamp (g.feedback + std::max (0.0f, y) * 0.3f, 0.0f, 0.95f);
    g.space = std::clamp (g.space + 0.4f * y, 0.0f, 1.0f);
    g.detuneCents = std::clamp (g.detuneCents * std::exp2 (y), 0.0f, 80.0f);
    g.width = std::clamp (g.width + 0.3f * y, 0.0f, 1.0f);
    g.spray = std::clamp (g.spray + std::max (0.0f, y) * 0.3f, 0.0f, 1.0f);
    g.decaySec = std::clamp (g.decaySec * std::exp2 (1.2f * y), 0.3f, 40.0f);
    return g;
}
} // namespace bouquet
