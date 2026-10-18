#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Raw parameter values shared by all voices; read once per block.
struct VoiceParams
{
    int waveform = 1;          // 0 sine, 1 saw, 2 square, 3 triangle
    juce::ADSR::Parameters ampEnv { 0.01f, 0.2f, 0.8f, 0.3f };
    float cutoffHz = 2000.0f;
    float resonance = 0.7f;
    float filterEnvAmount = 0.0f; // octaves of cutoff modulation by the amp envelope
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }

    void prepare (double sampleRate, int maxBlockSize);
    void setParams (const VoiceParams& p);

    void startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int newValue) override;
    void controllerMoved (int, int) override {}
    using juce::SynthesiserVoice::renderNextBlock;
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;

private:
    float nextOscSample();
    void updateFrequency();

    VoiceParams params;
    juce::ADSR env;
    juce::dsp::StateVariableTPTFilter<float> filter;

    int currentNote = 60;
    float pitchBendSemitones = 0.0f;
    float level = 0.0f;
    double phase = 0.0, phaseInc = 0.0;
    float triState = 0.0f;
};
