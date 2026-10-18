#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <atomic>
#include <memory>
#include <vector>
#include "SoundLab.h"

namespace bouquet
{
// Recordings of real instruments (tools/samples): one region per recorded note, mapped
// over a key and velocity range, sustained notes with a seamless loop.
struct SampledInstrument
{
    struct Region
    {
        int key = 60, lo = 0, hi = 127, lovel = 0, hivel = 127;
        int loopStart = -1, loopEnd = -1;   // in source samples; -1 when the note just decays
        int length = 0, channels = 1;
        std::vector<int16_t> data;          // interleaved
    };

    const Region* find (float note, float velocity) const;

    std::vector<Region> regions;
    double sampleRate = 44100.0;
    bool sustained = false;
};

// Loads the sampled instruments on a background thread the first time each is asked
// for, and keeps them for every plugin instance (share it with a SharedResourcePointer).
class SampleBank : private juce::Thread
{
public:
    SampleBank();
    ~SampleBank() override;

    bool available() const { return folder.isDirectory(); }

    // Real-time safe: the instrument if it is loaded, otherwise nullptr (and it gets loaded).
    const SampledInstrument* get (Instrument);

    // Loads it now, on this thread (offline rendering and tests).
    const SampledInstrument* load (Instrument);

private:
    void run() override;
    std::unique_ptr<SampledInstrument> read (Instrument) const;

    juce::File folder;
    juce::var manifest;
    juce::CriticalSection loadLock;
    std::array<std::unique_ptr<SampledInstrument>, (size_t) Instrument::count> owned;
    std::array<std::atomic<const SampledInstrument*>, (size_t) Instrument::count> ready {};
    std::atomic<uint32_t> wanted { 0 };
};
} // namespace bouquet
