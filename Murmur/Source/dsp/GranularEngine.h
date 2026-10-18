#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>
#include <vector>
#include "DspUtils.h"
#include "Reverb.h"

namespace murmur
{
// Interval sets that grains may be transposed to ("harmony").
struct HarmonySet
{
    const char* name;
    std::vector<int> intervals;
};

const std::vector<HarmonySet>& harmonySets();

// Fully resolved engine settings for one block (after macros and modulation).
struct GrainSettings
{
    float density = 12.0f;     // grains per second
    float sizeSec = 0.2f;
    float timeSec = 0.3f;      // how far back in the memory grains are taken from
    float spray = 0.2f;        // 0..1, random extra reach (up to 2 s)
    float pitch = 0.0f;        // semitones
    int harmony = 0;
    float harmonyAmount = 0.0f;
    float detuneCents = 6.0f;
    float reverse = 0.0f;      // probability
    float chaos = 0.5f;        // 0 = clockwork, 1 = random timing
    float shape = 0.0f;        // 0 = soft window, 1 = sharp edges
    float width = 0.6f;
    float feedback = 0.0f;
    float tone = 0.0f;         // -1 dark .. +1 bright
    float space = 0.3f;
    float decaySec = 4.0f;
    float life = 0.3f;
    bool freeze = false;
    float mix = 0.6f;
};

// What the UI needs to draw one grain.
struct GrainEvent
{
    float time01;     // 0 = now, 1 = the far end of memory
    float pitch;      // semitones relative to the source
    float pan;
    float duration;   // seconds
    float amp;
    bool reverse;
    bool harmonic;    // transposed to a chord tone
};

class GranularEngine
{
public:
    static constexpr int maxGrains = 96;
    static constexpr float memorySeconds = 8.0f;
    static constexpr int memoryBins = 160;

    void prepare (double sampleRate);
    void reset();

    // Granulates src into out (dry/wet mixed). Pointers may alias src == out.
    void process (const float* srcL, const float* srcR, float* outL, float* outR, int numSamples, const GrainSettings&);

    // UI thread: pop grains spawned since last call.
    int popEvents (GrainEvent* dest, int maxEvents);

    // UI thread: snapshot of recent input loudness, newest first.
    void readMemory (std::array<float, memoryBins>& dest) const;

    std::atomic<float> wetLevel { 0.0f };
    std::atomic<float> breath { 0.0f };    // slow organic modulation, -1..1
    std::atomic<int> activeGrains { 0 };

private:
    struct Grain
    {
        double pos, rate;
        int length, age;
        float invLength, fade, gainL, gainR, mono;
    };

    void spawnGrain (const GrainSettings&, float gainComp);
    float window (float t, float fade) const;

    double sr = 48000.0;
    std::vector<float> bufL, bufR;
    int bufMask = 0, writePos = 0;
    int recorded = 0;

    std::array<Grain, maxGrains> grains {};
    int numGrains = 0;
    double samplesToNextGrain = 0.0;

    std::array<float, 1025> riseTable {};
    Random rng { 0xC0FFEEu };
    Wander lifePosition, lifePitch, lifeDensity, lifePan, lifeTone;
    float lifePitchNow = 0.0f, lifePosNow = 0.0f, lifePanNow = 0.0f;

    Svf lowL, lowR, highL, highR;
    OnePole dcL, dcR;
    Reverb reverb;
    float lastDecay = -1.0f, lastTone = 99.0f;
    Ramp mixRamp, feedbackRamp, spaceRamp;
    float fbL = 0.0f, fbR = 0.0f;

    // Visualisation
    juce::AbstractFifo eventFifo { 512 };
    std::array<GrainEvent, 512> events {};
    std::array<std::atomic<float>, memoryBins> memory {};
    std::atomic<int> memoryHead { 0 };
    int memoryCounter = 0, samplesPerBin = 2400;
    float memoryPeak = 0.0f, wetEnvelope = 0.0f;
};
} // namespace murmur
