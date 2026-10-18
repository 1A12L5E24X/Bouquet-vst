#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Parameters.h"
#include "Spells.h"

class MurmurProcessor : public juce::AudioProcessor,
                        private juce::Timer,
                        private juce::AsyncUpdater
{
public:
    explicit MurmurProcessor (bool isEffect = ! JucePlugin_IsSynth);
    ~MurmurProcessor() override;

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

    const bool isEffect;
    juce::AudioProcessorValueTreeState apvts;
    juce::MidiKeyboardState keyboardState;
    murmur::GranularEngine engine;

    std::atomic<float> sourceMeter { 0.0f }, outputMeter { 0.0f };
    std::atomic<int> soundingVoices { 0 };

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

    murmur::SoundLab soundLab;
    juce::AudioBuffer<float> scratch;
    juce::SmoothedValue<float> sourceGain, outputGain;
    float sourceEnv = 0.0f, outputEnv = 0.0f;

    std::vector<MorphTarget> morphTargets;
    double morphStartMs = 0.0, morphLengthMs = 1.0;
    juce::Random random;
    std::atomic<int> pendingProgram { -1 };
    std::atomic<int> currentSpell { 0 };   // mirrors the "spell" state property for any thread

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MurmurProcessor)
};
