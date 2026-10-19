#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>
#include "DspUtils.h"

namespace bouquet
{
struct SampledInstrument;

// The voices of the Sound Lab. Piano, rhodes, marimba, harp, violin, cello and flute
// play recordings of real instruments (SampleBank) and fall back to synthesised models
// until those are loaded; glass and choir are always synthesised.
enum class Instrument
{
    liveInput = 0,
    piano,
    rhodes,
    marimba,
    glass,
    harp,
    violin,
    cello,
    flute,
    choir,
    count
};

struct SoundLabSettings
{
    Instrument instrument = Instrument::piano;
    int chord = 0;          // index into chordShapes()
    int voicing = 0;        // 0 close, 1 open, 2 wide
    float strumMs = 0.0f;
    float tone = 0.5f;      // 0 dark .. 1 bright
    float attack = 0.0f;    // extra attack, seconds
    float release = 0.8f;   // seconds
    int octave = 0;
};

struct ChordShape
{
    const char* name;
    std::vector<int> intervals;
};

const std::vector<ChordShape>& chordShapes();

class InstrumentVoice
{
public:
    void prepare (float sampleRate);
    void start (Instrument model, float note, float velocity, const SoundLabSettings&, uint32_t seed,
                const SampledInstrument* sampled = nullptr);
    void release();
    void kill() { active = false; }

    // Key pressure (aftertouch), 0..1: brightens and swells the note.
    void setPressure (float p) { pressure = p; }
    float getPressure() const { return pressureNow; }

    // Adds this voice's output into left/right.
    void render (float* left, float* right, int numSamples, float pitchBendSemis);

    float getNote() const { return note; }

    bool active = false;
    bool keyDown = false;     // the physical key is still held
    bool sustained = false;   // released by key but held by the sustain pedal
    int sourceKey = -1;       // MIDI key that triggered this voice (chords share one key)
    uint64_t startedAt = 0;

private:
    struct Envelope
    {
        enum class Stage { idle, attack, hold, release };
        void start (float sampleRate, float attackSec)
        {
            stage = Stage::attack;
            value = 0.0f;
            attackInc = 1.0f / std::max (1.0f, attackSec * sampleRate);
        }
        void startRelease (float sampleRate, float releaseSec)
        {
            stage = Stage::release;
            releaseMul = std::exp (std::log (1.0e-4f) / std::max (1.0f, releaseSec * sampleRate));
        }
        float next()
        {
            switch (stage)
            {
                case Stage::attack:  value += attackInc; if (value >= 1.0f) { value = 1.0f; stage = Stage::hold; } break;
                case Stage::release: value *= releaseMul; if (value < 1.0e-4f) { value = 0.0f; stage = Stage::idle; } break;
                case Stage::hold:
                case Stage::idle:    break;
            }
            return value;
        }
        Stage stage = Stage::idle;
        float value = 0, attackInc = 0, releaseMul = 0;
    };

    static constexpr int maxPartials = 24;
    struct Partial { float re, im, cr, ci, amp, mul; };

    void addPartial (float hz, float amp, float t60);
    void startModal (const SoundLabSettings&);
    void startBowed (const SoundLabSettings&, bool isCello);
    void startChoir (const SoundLabSettings&);

    float renderModal();
    float renderRhodes();
    float renderHarp();
    float renderBowed (float hz);
    float renderFlute (float hz);
    float renderChoir (float hz);
    void renderSampled (float* left, float* right, int numSamples, float bendRatio, float& peak);

    Instrument model = Instrument::piano;
    float sr = 48000.0f;
    float note = 60.0f, freq = 261.6f, velocity = 1.0f, tone = 0.5f;
    float gainL = 1.0f, gainR = 1.0f;
    float releaseSec = 0.8f;
    bool percussive = true;
    int samplesPlayed = 0;
    Envelope env;
    Random rng;

    // Modal / additive state
    std::array<Partial, maxPartials> partials {};
    int numPartials = 0;
    float strikeNoise = 0.0f, strikeDecay = 0.0f;
    Svf strikeFilter;

    // Phase-based oscillators (rhodes, bowed, flute, choir)
    float phase = 0.0f, phase2 = 0.0f;
    float modIndex = 0.0f, modDecay = 0.0f, ampDecay = 1.0f, amp = 1.0f;
    float tineAmp = 0.0f, tineDecay = 0.0f;
    float vibPhase = 0.0f, vibRate = 5.5f, vibDepth = 0.0f, vibTarget = 0.0f;
    Wander drift;

    // Body / formant resonators
    static constexpr int maxFormants = 6;
    std::array<Svf, maxFormants> formants {};
    std::array<float, maxFormants> formantGains {};
    int numFormants = 0;
    float directGain = 0.0f;
    Svf noiseFilter, breathFilter;
    OnePole toneFilter, sourceFilter;
    std::array<float, 3> choirPhases {};
    std::array<float, 3> choirDetune {};
    std::array<float, 3> choirVibPhase {};

    // Karplus-Strong string
    std::vector<float> line;
    int lineMask = 0, writePos = 0;
    float loopGain = 0.99f, loopBlend = 0.5f, apCoeff = 0.0f, apX1 = 0.0f, apY1 = 0.0f, prevRead = 0.0f;
    int delayInt = 100;

    // Sample playback: up to two recorded layers cross-faded by velocity.
    struct Playhead
    {
        const void* region = nullptr;      // SampledInstrument::Region
        double pos = 0.0, step = 1.0;
        float gain = 0.0f;
        bool done = false;
    };
    std::array<Playhead, 2> heads {};
    int numHeads = 0;

    float pressure = 0.0f, pressureNow = 0.0f, toneBase = 1500.0f, toneApplied = 0.0f;
};

// A voice starting or being let go, for the UI (the garden grows bouquets from these).
struct NoteEvent
{
    float note;
    float velocity;
    int chordShape;
    int instrument;
    bool on;
};

class SoundLab
{
public:
    static constexpr int numVoices = 16;

    void prepare (double sampleRate);
    void reset();
    void setSettings (const SoundLabSettings& s) { settings = s; }

    // Recordings for the current instrument (nullptr plays the synthesised model).
    void setSamples (const SampledInstrument* s) { samples = s; }

    // Renders the voices into left/right (overwrites) using MIDI from the block.
    void process (const juce::MidiBuffer& midi, float* left, float* right, int numSamples);

    // Number of voices currently sounding (for the UI's "breathing").
    int activeVoiceCount() const;

    // The strongest key pressure on any held voice, 0..1 (for the UI).
    float maxPressure() const;

    // UI thread: pop note starts/releases since the last call.
    int popNoteEvents (NoteEvent* dest, int maxEvents);

private:
    void pushNoteEvent (float note, float velocity, bool on);
    void releaseVoice (InstrumentVoice&);

    void handleMidi (const juce::MidiMessage&);
    void noteOn (int key, float velocity);
    void noteOff (int key);
    void startVoice (int key, float note, float velocity);
    void releaseVoicesForKey (int key);
    void setPressureForKey (int key, float pressure);

    struct PendingNote { int key; float note; float velocity; int samplesLeft; };

    SoundLabSettings settings;
    const SampledInstrument* samples = nullptr;
    std::array<InstrumentVoice, numVoices> voices;
    std::array<PendingNote, 64> pending {};
    int numPending = 0;
    bool sustainPedal = false;
    float pitchBend = 0.0f;
    uint64_t voiceCounter = 0;
    uint32_t seedCounter = 1;
    float sr = 48000.0f;
    Random rng { 0x1234567u };

    juce::AbstractFifo noteFifo { 256 };
    std::array<NoteEvent, 256> noteEvents {};
};
} // namespace bouquet
