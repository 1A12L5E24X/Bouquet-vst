#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Parameters.h"
#include "Spells.h"
#include "Presets.h"
#include "dsp/Forest.h"
#include "dsp/SampleBank.h"

class BouquetProcessor : public juce::AudioProcessor,
                        private juce::Timer,
                        private juce::AsyncUpdater
{
public:
    explicit BouquetProcessor (bool isEffect = ! JucePlugin_IsSynth);
    ~BouquetProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 12.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Message thread: glide every Magic Lab control to a spell over a moment
    // (or instantly, when glideSeconds is 0).
    void castSpell (int index, double glideSeconds = 1.4);
    // Message thread: nudge the current spell somewhere new.
    void mutate();
    int getCurrentSpell() const;

    // Presets: the whole instrument (Sound Lab, forest and Magic Lab). Message thread.
    void applyPreset (const bouquet::Preset&, double glideSeconds = 1.2);
    bouquet::Preset plantPreset (const juce::String& name);   // saves the current sound as a user preset
    // Grows a richer version of a preset (see waterPreset), saves it and glides to it.
    bouquet::Watering waterPreset (const bouquet::Preset&, const std::vector<bouquet::Tonic>&);
    // Tends a flower with one of the garden's other tools (see tendPreset): saves the result and glides to it.
    bouquet::Watering tendPreset (const bouquet::Preset&, bouquet::Tending);
    juce::String getCurrentPresetName() const { return apvts.state.getProperty ("preset", "").toString(); }
    bouquet::PresetLibrary presets;

    // The garden's flowers: photoreal renders (default) or hand-drawn. Saved with the
    // plugin's state. Message thread.
    bool photorealFlowers() const { return (bool) apvts.state.getProperty ("photoreal", true); }
    void setPhotorealFlowers (bool on) { apvts.state.setProperty ("photoreal", on, nullptr); }

    // UI thread: notes started and let go since the last call (for the garden).
    int popNoteEvents (bouquet::NoteEvent* dest, int maxEvents) { return soundLab.popNoteEvents (dest, maxEvents); }

    const bool isEffect;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    bouquet::GranularEngine engine;

    std::atomic<float> sourceMeter { 0.0f }, outputMeter { 0.0f };
    std::atomic<int> soundingVoices { 0 };

    // Expression, for the garden: key pressure (aftertouch) and the mod wheel, 0..1.
    std::atomic<float> keyPressure { 0.0f }, modWheel { 0.0f };

private:
    static BusesProperties makeBuses (bool isEffect);
    void timerCallback() override;
    void handleAsyncUpdate() override;
    void startMorph (const std::vector<std::pair<juce::String, float>>& targets, double seconds);

    struct MorphTarget
    {
        juce::RangedAudioParameter* param;
        float from, to;
    };

    bouquet::SoundLab soundLab;
    juce::SharedResourcePointer<bouquet::SampleBank> sampleBank;
    bouquet::Forest forest;
    bouquet::Ramp magicGain;
    juce::AudioBuffer<float> scratch;
    juce::SmoothedValue<float> sourceGain, outputGain;
    float sourceEnv = 0.0f, outputEnv = 0.0f;

    std::vector<MorphTarget> morphTargets;
    double morphStartMs = 0.0, morphLengthMs = 1.0;
    juce::Random random;
    std::atomic<int> pendingProgram { -1 };
    std::atomic<int> currentSpell { 0 };
    std::atomic<int> currentFactory { -1 };   // factory preset index, -1 for a user preset   // mirrors the "spell" state property for any thread

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BouquetProcessor)
};
