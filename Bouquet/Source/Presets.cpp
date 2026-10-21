#include "Presets.h"
#include "Parameters.h"
#include "Spells.h"

namespace bouquet
{
namespace
{
    struct FactoryDef
    {
        const char* name;
        int instrument, chord, voicing;
        float strum, tone, attack, release;
        int octave;
        int spell;          // -1: Magic Lab off, the bare instrument in the forest
        float forest, wildlife;
        float level = 0.0f;   // dB trim so every preset arrives at a similar loudness
    };

    // Spell indices follow spellBook(); instruments and chords follow their lists.
    const FactoryDef factoryDefs[] = {
        //  name               inst chord voic strum tone  atk   rel   oct spell forest wild
        { "first light", 1, 5, 1, 30, 0.55f, 0.0f, 1.2f, 0, 0, 0.5f, 0.35f, 5.7f },
        { "morning glass", 4, 8, 2, 60, 0.7f, 0.0f, 2.0f, 1, 16, 0.45f, 0.4f, 6.5f },
        { "slow river", 7, 6, 0, 0, 0.4f, 0.35f, 2.5f, 0, 11, 0.6f, 0.3f, -5.4f },
        { "violet hour", 6, 10, 1, 70, 0.5f, 0.15f, 1.8f, 0, 6, 0.55f, 0.3f, -3.1f },
        { "honey", 2, 9, 0, 20, 0.35f, 0.0f, 1.0f, 0, 0, 0.4f, 0.2f, 2.4f },
        { "pollen choir", 9, 3, 1, 40, 0.6f, 0.3f, 2.2f, 0, 17, 0.6f, 0.4f, 4.5f },
        { "moth light", 8, 0, 0, 0, 0.45f, 0.05f, 1.5f, 1, 27, 0.65f, 0.5f, -1.6f },
        { "wind orchard", 5, 11, 2, 120, 0.6f, 0.0f, 2.5f, 0, 36, 0.55f, 0.6f, 6.2f },
        { "rain on wood", 3, 1, 1, 45, 0.55f, 0.0f, 0.8f, 1, 5, 0.5f, 0.45f, 9.0f },
        { "cathedral pines", 9, 2, 1, 0, 0.4f, 0.4f, 3.0f, 0, 25, 0.7f, 0.25f, -9.5f },
        { "ghost piano", 1, 0, 0, 0, 0.45f, 0.0f, 1.5f, 0, 9, 0.5f, 0.3f, 8.7f },
        { "silk", 6, 5, 1, 50, 0.6f, 0.2f, 2.0f, 0, 24, 0.5f, 0.3f, -3.2f },
        { "deep moss", 7, 0, 0, 0, 0.3f, 0.25f, 2.5f, -1, 29, 0.7f, 0.35f, 2.9f },
        { "tape garden", 2, 6, 0, 25, 0.4f, 0.0f, 1.2f, 0, 10, 0.45f, 0.25f, 1.7f },
        { "lantern", 4, 4, 1, 80, 0.65f, 0.0f, 3.0f, 0, 3, 0.5f, 0.4f, 4.3f },
        { "dandelion", 5, 8, 1, 90, 0.7f, 0.0f, 2.0f, 1, 17, 0.45f, 0.5f, 4.6f },
        { "orbiting", 8, 9, 1, 60, 0.55f, 0.1f, 2.0f, 0, 33, 0.55f, 0.35f, -3.0f },
        { "nocturne", 1, 10, 1, 35, 0.35f, 0.0f, 2.5f, 0, 28, 0.6f, 0.3f, -5.4f },
        { "heartbeat", 3, 0, 0, 0, 0.5f, 0.0f, 0.6f, 0, 34, 0.4f, 0.2f, 9.0f },
        { "breeze", 8, 3, 1, 40, 0.6f, 0.1f, 2.0f, 0, 32, 0.6f, 0.6f, -2.6f },
        { "starlings", 6, 7, 2, 30, 0.55f, 0.1f, 1.5f, 0, 18, 0.55f, 0.45f, 0.1f },
        { "chaos bloom", 9, 12, 2, 60, 0.5f, 0.2f, 2.5f, 0, 39, 0.5f, 0.3f, -3.4f },
        { "clearing", 1, 5, 1, 25, 0.5f, 0.0f, 1.5f, 0, -1, 0.65f, 0.45f, 4.4f },
        { "bare cello", 7, 0, 0, 0, 0.45f, 0.2f, 1.8f, 0, -1, 0.6f, 0.35f, -1.3f },
    };

    void add (Preset& p, const juce::String& id, float v) { p.values.push_back ({ id, v }); }

    Preset build (const FactoryDef& d)
    {
        Preset p;
        p.name = d.name;
        p.factory = true;
        add (p, ids::instrument, (float) d.instrument);
        add (p, ids::chord, (float) d.chord);
        add (p, ids::voicing, (float) d.voicing);
        add (p, ids::strum, d.strum);
        add (p, ids::labTone, d.tone);
        add (p, ids::attack, d.attack);
        add (p, ids::release, d.release);
        add (p, ids::octave, (float) d.octave);
        add (p, ids::sourceLevel, d.level);
        add (p, ids::forest, d.forest);
        add (p, ids::wildlife, d.wildlife);
        add (p, ids::auraX, 0.5f);
        add (p, ids::auraY, 0.5f);
        add (p, ids::magic, d.spell >= 0 ? 1.0f : 0.0f);

        const auto& v = spellBook()[(size_t) std::max (0, d.spell)].values;
        add (p, ids::density, v.density);       add (p, ids::size, v.sizeMs);
        add (p, ids::time, v.timeSec);          add (p, ids::spray, v.spray);
        add (p, ids::pitch, v.pitch);           add (p, ids::harmony, (float) v.harmony);
        add (p, ids::harmonyAmount, v.harmonyAmount);
        add (p, ids::detune, v.detune);         add (p, ids::reverse, v.reverse);
        add (p, ids::chaos, v.chaos);           add (p, ids::shape, v.shape);
        add (p, ids::width, v.width);           add (p, ids::feedback, v.feedback);
        add (p, ids::colour, v.colour);         add (p, ids::space, v.space);
        add (p, ids::decay, v.decay);           add (p, ids::life, v.life);
        add (p, ids::freeze, v.freeze ? 1.0f : 0.0f);
        add (p, ids::mix, v.mix);
        return p;
    }
}

float Preset::get (const juce::String& id, float fallback) const
{
    for (auto& [key, value] : values)
        if (key == id)
            return value;
    return fallback;
}

uint32_t Preset::seed() const
{
    auto sorted = values;
    std::sort (sorted.begin(), sorted.end(), [] (auto& a, auto& b) { return a.first < b.first; });

    uint32_t h = 2166136261u;
    auto mix = [&h] (uint32_t v) { for (int i = 0; i < 4; ++i) { h ^= (v >> (i * 8)) & 0xffu; h *= 16777619u; } };
    for (auto& [id, value] : sorted)
    {
        mix ((uint32_t) id.hashCode());
        mix ((uint32_t) juce::roundToInt (value * 1000.0f));
    }
    mix ((uint32_t) name.hashCode());
    return h;
}

const std::vector<Preset>& PresetLibrary::factory()
{
    static const std::vector<Preset> presets = []
    {
        std::vector<Preset> list;
        for (auto& d : factoryDefs)
            list.push_back (build (d));
        return list;
    }();
    return presets;
}

juce::File PresetLibrary::defaultUserFolder()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support/Bouquet/Presets");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Bouquet/Presets");
   #endif
}

void PresetLibrary::rescan()
{
    userPresets.clear();
    auto files = userFolder().findChildFiles (juce::File::findFiles, false, "*.bouquet");
    files.sort();
    for (auto& f : files)
    {
        auto xml = juce::parseXML (f);
        if (xml == nullptr || ! xml->hasTagName ("BouquetPreset"))
            continue;
        Preset p;
        p.name = xml->getStringAttribute ("name", f.getFileNameWithoutExtension());
        p.factory = false;
        p.file = f;
        for (auto* param : xml->getChildWithTagNameIterator ("PARAM"))
            p.values.push_back ({ param->getStringAttribute ("id"), (float) param->getDoubleAttribute ("value") });
        userPresets.push_back (std::move (p));
    }
}

bool PresetLibrary::save (const Preset& preset)
{
    const auto dir = userFolder();
    if (! dir.createDirectory())
        return false;

    juce::XmlElement xml ("BouquetPreset");
    xml.setAttribute ("name", preset.name);
    xml.setAttribute ("version", 1);
    for (auto& [id, value] : preset.values)
    {
        auto* param = xml.createNewChildElement ("PARAM");
        param->setAttribute ("id", id);
        param->setAttribute ("value", value);
    }

    const auto file = dir.getNonexistentChildFile (juce::File::createLegalFileName (preset.name), ".bouquet", false);
    const bool ok = xml.writeTo (file);
    rescan();
    return ok;
}

bool PresetLibrary::remove (const Preset& preset)
{
    if (preset.factory || ! preset.file.existsAsFile())
        return false;
    const bool ok = preset.file.deleteFile();
    rescan();
    return ok;
}

const Preset* PresetLibrary::find (const juce::String& name) const
{
    for (auto& p : factory())
        if (p.name == name) return &p;
    for (auto& p : userPresets)
        if (p.name == name) return &p;
    return nullptr;
}

juce::String PresetLibrary::inventName (uint32_t seed) const
{
    static const char* first[] = { "amber", "quiet", "wild", "pale", "velvet", "morning", "dusk", "silver", "mossy",
                                   "honey", "salt", "lantern", "river", "sleeping", "tender", "ember" };
    static const char* second[] = { "fern", "hollow", "meadow", "thistle", "clover", "orchard", "willow", "sorrel",
                                    "bramble", "lily", "heather", "foxglove", "yarrow", "aster", "nettle", "briar" };
    juce::Random r ((juce::int64) seed);
    for (int attempt = 0; attempt < 64; ++attempt)
    {
        juce::String name = juce::String (first[r.nextInt (16)]) + " " + second[r.nextInt (16)];
        if (attempt >= 16)
            name << " " << juce::String (attempt);
        if (find (name) == nullptr)
            return name;
    }
    return "seedling " + juce::String ((int) (seed % 10000));
}

Preset capturePreset (const juce::AudioProcessorValueTreeState& state, const juce::String& name)
{
    Preset p;
    p.name = name;
    p.factory = false;
    for (auto* param : state.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (ranged->getParameterID() != ids::output)
                p.values.push_back ({ ranged->getParameterID(), ranged->convertFrom0to1 (ranged->getValue()) });
    return p;
}

const TonicInfo& tonicInfo (Tonic t)
{
    static const TonicInfo infos[] = {
        { "rain water", "more air, room and forest", "rain-fed" },
        { "honey", "thicker, warmer, more voices", "honeyed" },
        { "moonwater", "shimmering echoes and harmony", "moonlit" },
        { "storm water", "wild, scattered, unpredictable", "storm-struck" },
    };
    return infos[juce::jlimit (0, (int) Tonic::count - 1, (int) t)];
}

Watering waterPreset (const juce::AudioProcessorValueTreeState& state, const Preset& source, const std::vector<Tonic>& tonics,
                      const PresetLibrary& names, juce::Random& r)
{
    Watering w;
    w.preset = source;
    w.preset.factory = false;
    w.preset.file = juce::File();
    auto& values = w.preset.values;

    auto find = [&values] (const juce::String& id) -> float*
    {
        for (auto& [key, value] : values)
            if (key == id)
                return &value;
        return nullptr;
    };
    auto plain = [&] (const juce::String& id) -> float
    {
        if (auto* v = find (id))
            return *v;
        auto* p = state.getParameter (id);
        return p != nullptr ? p->convertFrom0to1 (p->getDefaultValue()) : 0.0f;
    };
    auto set = [&] (const juce::String& id, float v)
    {
        if (auto* existing = find (id)) *existing = v;
        else                            values.push_back ({ id, v });
    };
    // Pushes a parameter along its own range, by a little more or less than asked.
    auto nudge = [&] (const juce::String& id, float amount)
    {
        auto* p = state.getParameter (id);
        if (p == nullptr)
            return;
        const float norm = p->convertTo0to1 (plain (id)) + amount * (0.6f + 0.8f * r.nextFloat());
        set (id, p->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm)));
    };

    // Plain water: everything a little fuller.
    if (plain (ids::magic) < 0.5f)
    {
        set (ids::magic, 1.0f);
        set (ids::mix, std::max (0.0f, 0.25f + 0.1f * r.nextFloat()));
        w.note << "woke the Magic Lab; ";
    }
    for (auto& [id, amount] : std::initializer_list<std::pair<const juce::String*, float>> {
             { &ids::harmonyAmount, 0.12f }, { &ids::detune, 0.05f }, { &ids::width, 0.1f }, { &ids::density, 0.06f },
             { &ids::space, 0.06f }, { &ids::decay, 0.05f }, { &ids::life, 0.06f }, { &ids::mix, 0.05f }, { &ids::wildlife, 0.04f } })
        nudge (*id, amount);
    if (plain (ids::voicing) < 0.5f && r.nextFloat() < 0.5f)
        set (ids::voicing, 1.0f);

    // Each tonic pushes its own way; a second dose of the same one less than the first.
    int doses[(int) Tonic::count] = {};
    for (auto t : tonics)
    {
        const float dose = 1.0f / (float) (1 + doses[(int) t]++);
        switch (t)
        {
            case Tonic::rain:
                nudge (ids::space, 0.15f * dose);  nudge (ids::decay, 0.12f * dose);
                nudge (ids::width, 0.12f * dose);  nudge (ids::wildlife, 0.08f * dose);
                nudge (ids::forest, 0.06f * dose);
                break;
            case Tonic::honey:
                nudge (ids::density, 0.12f * dose);  nudge (ids::size, 0.1f * dose);
                nudge (ids::harmonyAmount, 0.12f * dose);  nudge (ids::labTone, -0.06f * dose);
                nudge (ids::attack, 0.03f * dose);
                break;
            case Tonic::moon:
                nudge (ids::feedback, 0.12f * dose);  nudge (ids::detune, 0.1f * dose);
                nudge (ids::harmonyAmount, 0.1f * dose);  nudge (ids::time, 0.08f * dose);
                if (juce::roundToInt (plain (ids::harmony)) == 0)
                    set (ids::harmony, r.nextBool() ? 1.0f : 11.0f);   // octaves or shimmer
                break;
            case Tonic::storm:
                nudge (ids::chaos, 0.15f * dose);  nudge (ids::spray, 0.15f * dose);
                nudge (ids::reverse, 0.12f * dose);  nudge (ids::life, 0.1f * dose);
                break;
            case Tonic::count:
                break;
        }
    }

    // Sometimes the mix fizzes and something unexpected grows; storms make it likelier.
    if (r.nextFloat() < 0.18f + 0.08f * (float) doses[(int) Tonic::storm])
    {
        w.fizzed = true;
        const juce::String* wild[] = { &ids::pitch, &ids::colour, &ids::shape, &ids::reverse, &ids::size, &ids::time,
                                       &ids::feedback, &ids::chaos, &ids::detune };
        const int which = r.nextInt (4);
        if (which == 0)
        {
            set (ids::harmony, (float) r.nextInt (14));
            w.note << "it fizzed, and the harmony changed; ";
        }
        else if (which == 1)
        {
            set (ids::pitch, plain (ids::pitch) + (r.nextBool() ? 12.0f : -12.0f) * (r.nextFloat() < 0.3f ? 0.5833f : 1.0f));
            set (ids::pitch, juce::jlimit (-24.0f, 24.0f, plain (ids::pitch)));
            w.note << "it fizzed, and the grains jumped register; ";
        }
        else
        {
            for (int i = 0; i < 2; ++i)
                nudge (*wild[r.nextInt ((int) std::size (wild))], (r.nextFloat() * 2.0f - 1.0f) * 0.3f);
            w.note << "it fizzed over; ";
        }
    }

    // A fuller sound is a louder one: trim it back to sit where it was.
    set (ids::sourceLevel, juce::jlimit (-36.0f, 12.0f, plain (ids::sourceLevel) - 1.0f - 0.6f * (float) tonics.size()));

    // The name: the flower it came from, and what it was given.
    auto base = source.name.upToFirstOccurrenceOf (",", false, false).trim();
    juce::StringArray given;
    for (int t = 0; t < (int) Tonic::count; ++t)
        if (doses[t] > 0)
            given.add (tonicInfo ((Tonic) t).adjective);
    if (given.isEmpty())
        given.add ("watered");
    auto name = base + ", " + given.joinIntoString (" & ");
    static const char* numerals[] = { " ii", " iii", " iv", " v", " vi", " vii", " viii", " ix", " x" };
    for (int i = 0; names.find (name) != nullptr; ++i)
        name = base + ", " + given.joinIntoString (" & ") + (i < 9 ? juce::String (numerals[i]) : " " + juce::String (i + 2));
    w.preset.name = name;
    w.note = (w.note.isEmpty() ? juce::String ("grew richer") : w.note.dropLastCharacters (2));
    return w;
}
} // namespace bouquet
