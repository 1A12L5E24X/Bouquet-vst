#include "SampleBank.h"
#include "../Assets.h"

namespace bouquet
{
namespace
{
    const char* manifestName (Instrument i)
    {
        switch (i)
        {
            case Instrument::piano:   return "piano";
            case Instrument::rhodes:  return "rhodes";
            case Instrument::marimba: return "marimba";
            case Instrument::harp:    return "harp";
            case Instrument::violin:  return "violin";
            case Instrument::cello:   return "cello";
            case Instrument::flute:   return "flute";
            case Instrument::glass:                     // glass and choir are synthesised
            case Instrument::choir:
            case Instrument::liveInput:
            case Instrument::count:   break;
        }
        return nullptr;
    }
}

const SampledInstrument::Region* SampledInstrument::find (float note, float velocity) const
{
    const int key = juce::roundToInt (note);
    const int vel = juce::jlimit (1, 127, juce::roundToInt (velocity * 127.0f));
    const Region* best = nullptr;
    int bestDistance = 1 << 30;
    for (auto& r : regions)
    {
        // Prefer a region covering the key and velocity, else the nearest recorded note.
        const int keyDistance = key < r.lo ? r.lo - key : (key > r.hi ? key - r.hi : 0);
        const int velDistance = vel < r.lovel ? r.lovel - vel : (vel > r.hivel ? vel - r.hivel : 0);
        const int d = keyDistance * 1000 + velDistance;
        if (d < bestDistance)
        {
            bestDistance = d;
            best = &r;
        }
    }
    return best;
}

//==============================================================================
SampleBank::SampleBank() : juce::Thread ("Bouquet samples")
{
    folder = assets::find ("Samples", "samples.json");
    if (folder.isDirectory())
    {
        manifest = juce::JSON::parse (folder.getChildFile ("samples.json"));
        startThread (juce::Thread::Priority::low);
    }
}

SampleBank::~SampleBank()
{
    stopThread (4000);
}

const SampledInstrument* SampleBank::get (Instrument i)
{
    const auto index = (size_t) i;
    if (index >= ready.size() || manifestName (i) == nullptr)
        return nullptr;
    if (auto* s = ready[index].load (std::memory_order_acquire))
        return s;
    wanted.fetch_or (1u << index);
    return nullptr;
}

const SampledInstrument* SampleBank::load (Instrument i)
{
    const auto index = (size_t) i;
    if (index >= ready.size() || manifestName (i) == nullptr || ! available())
        return nullptr;
    const juce::ScopedLock sl (loadLock);
    if (owned[index] == nullptr)
    {
        owned[index] = read (i);
        ready[index].store (owned[index].get(), std::memory_order_release);
    }
    return owned[index].get();
}

void SampleBank::run()
{
    while (! threadShouldExit())
    {
        const auto mask = wanted.exchange (0);
        for (size_t i = 0; i < ready.size(); ++i)
            if ((mask >> i) & 1u)
                load ((Instrument) i);
        wait (40);
    }
}

std::unique_ptr<SampledInstrument> SampleBank::read (Instrument i) const
{
    auto inst = std::make_unique<SampledInstrument>();
    const auto info = manifest["instruments"][manifestName (i)];
    if (! info.isObject())
        return inst;

    inst->sustained = (bool) info["sustained"];
    inst->sampleRate = (double) manifest.getProperty ("sampleRate", 44100.0);

    juce::OggVorbisAudioFormat ogg;
    if (auto* regions = info["regions"].getArray())
    {
        for (auto& r : *regions)
        {
            std::unique_ptr<juce::AudioFormatReader> reader (
                ogg.createReaderFor (folder.getChildFile (r["file"].toString()).createInputStream().release(), true));
            if (reader == nullptr)
                continue;

            SampledInstrument::Region region;
            region.key = (int) r["key"];
            region.lo = (int) r["lo"];
            region.hi = (int) r["hi"];
            region.lovel = (int) r["lovel"];
            region.hivel = (int) r["hivel"];
            region.loopStart = r.hasProperty ("loopStart") ? (int) r["loopStart"] : -1;
            region.loopEnd = r.hasProperty ("loopEnd") ? (int) r["loopEnd"] : -1;
            region.channels = juce::jlimit (1, 2, (int) reader->numChannels);
            region.length = (int) reader->lengthInSamples;

            juce::AudioBuffer<float> buffer (region.channels, region.length);
            reader->read (&buffer, 0, region.length, 0, true, region.channels > 1);
            region.data.resize ((size_t) (region.length * region.channels));
            for (int s = 0; s < region.length; ++s)
                for (int c = 0; c < region.channels; ++c)
                    region.data[(size_t) (s * region.channels + c)]
                        = (int16_t) juce::jlimit (-32767, 32767, juce::roundToInt (buffer.getSample (c, s) * 32767.0f));
            if (region.loopEnd > region.length)
                region.loopEnd = region.length;
            inst->regions.push_back (std::move (region));
        }
    }
    return inst;
}
} // namespace bouquet
