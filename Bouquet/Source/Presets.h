#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

namespace bouquet
{
// A preset is the whole instrument: Sound Lab voice and chord, forest, and the Magic
// Lab (or its absence). Each one is drawn as its own flower in the preset meadow.
struct Preset
{
    juce::String name;
    std::vector<std::pair<juce::String, float>> values;   // parameter id -> plain value
    bool factory = true;
    juce::File file;                                       // user presets only

    float get (const juce::String& id, float fallback = 0.0f) const;

    // Identity of the sound: the same settings always grow the same flower.
    uint32_t seed() const;
};

class PresetLibrary
{
public:
    static const std::vector<Preset>& factory();
    static juce::File defaultUserFolder();
    juce::File userFolder() const { return folder != juce::File() ? folder : defaultUserFolder(); }
    void setUserFolder (const juce::File& f) { folder = f; rescan(); }   // for tests

    // Message thread only.
    void rescan();
    const std::vector<Preset>& user() const { return userPresets; }
    bool save (const Preset&);
    bool remove (const Preset&);
    const Preset* find (const juce::String& name) const;

    // A name nobody has used yet, like "amber hollow".
    juce::String inventName (uint32_t seed) const;

private:
    std::vector<Preset> userPresets;
    juce::File folder;
};

// Captures every parameter except the output level.
Preset capturePreset (const juce::AudioProcessorValueTreeState&, const juce::String& name);

// How a flower looks once it has been tended in the preset garden. These are not sound
// parameters (the plugin ignores them when it loads a preset): they are saved with the
// preset so the flower keeps the shape the tools gave it, and show up in the meadow.
namespace traits
{
inline const juce::String size   = "tend.size";     // -1 small .. +1 big
inline const juce::String glow   = "tend.glow";     // 0 .. 1, a halo and drifting lights
inline const juce::String glitch = "tend.glitch";   // 0 .. 1, a flower that stutters and splits
} // namespace traits

// Watering: pouring water on a preset grows a richer version of it (fuller harmony,
// wider, more air and motion), and each tonic stirred into the water pushes it further
// its own way. No two waterings come out quite the same.
enum class Tonic { rain, honey, moon, storm, count };

struct TonicInfo
{
    const char* name;        // "rain water"
    const char* effect;      // what it does to a sound
    const char* adjective;   // what it does to a name: "rain-fed"
};
const TonicInfo& tonicInfo (Tonic);

struct Watering
{
    Preset preset;           // the new, richer preset (not yet saved)
    bool fizzed = false;     // something unexpected happened in the mix
    juce::String note;       // a line for the lab notebook
};

// The garden's other tools. Each makes a new preset out of the flower it is used on,
// and shapes the flower itself along with the sound.
//   soil     weirder: the harmony bends, the grains drift, run backwards, go somewhere odd
//   prune    smaller: shorter, closer, drier, tighter
//   feed     bigger: longer, wider, deeper, more room
//   lantern  glowy: shimmering harmony, bright, airy, a tail of light
//   crickets glitchy: tiny grains, stutters, skips and scatter
enum class Tending { soil, prune, feed, lantern, crickets, count };

struct TendingInfo
{
    const char* tool;        // "garden soil"
    const char* hint;        // what it does to a sound, for its label
    const char* adjective;   // what it does to a name: "loamy"
};
const TendingInfo& tendingInfo (Tending);

// The soils are not all alike (loam, peat, ash, clay), so the same tool still surprises.
Watering tendPreset (const juce::AudioProcessorValueTreeState&, const Preset& source, Tending,
                     const PresetLibrary& names, juce::Random&);

// tonics: what was stirred into the water, in order (repeats are stronger doses).
// names: the library, to keep the new name unique.
Watering waterPreset (const juce::AudioProcessorValueTreeState&, const Preset& source, const std::vector<Tonic>& tonics,
                      const PresetLibrary& names, juce::Random&);
} // namespace bouquet
