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
} // namespace bouquet
