#include "SynthVoice.h"

namespace
{
    // Polynomial band-limited step: smooths the discontinuities of saw/square to reduce aliasing.
    float polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return (float) (t + t - t * t - 1.0);
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return (float) (t * t + t + t + 1.0);
        }
        return 0.0f;
    }
}

void SynthVoice::prepare (double sampleRate, int maxBlockSize)
{
    env.setSampleRate (sampleRate);
    filter.prepare ({ sampleRate, (juce::uint32) maxBlockSize, 1 });
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
}

void SynthVoice::setParams (const VoiceParams& p)
{
    params = p;
    env.setParameters (p.ampEnv);
    filter.setResonance (p.resonance);
}

void SynthVoice::startNote (int midiNote, float velocity, juce::SynthesiserSound*, int pitchWheel)
{
    currentNote = midiNote;
    level = velocity;
    pitchWheelMoved (pitchWheel);
    phase = 0.0;
    triState = 0.0f;
    filter.reset();
    env.reset();
    env.noteOn();
}

void SynthVoice::stopNote (float, bool allowTailOff)
{
    if (allowTailOff)
    {
        env.noteOff();
    }
    else
    {
        env.reset();
        clearCurrentNote();
    }
}

void SynthVoice::pitchWheelMoved (int newValue)
{
    pitchBendSemitones = 2.0f * ((float) newValue - 8192.0f) / 8192.0f;
    updateFrequency();
}

void SynthVoice::updateFrequency()
{
    const auto hz = 440.0 * std::pow (2.0, ((double) currentNote + pitchBendSemitones - 69.0) / 12.0);
    phaseInc = hz / getSampleRate();
}

float SynthVoice::nextOscSample()
{
    const auto t = phase;
    const auto dt = phaseInc;
    float out = 0.0f;

    switch (params.waveform)
    {
        case 0: // sine
            out = (float) std::sin (juce::MathConstants<double>::twoPi * t);
            break;

        case 1: // saw
            out = (float) (2.0 * t - 1.0) - polyBlep (t, dt);
            break;

        case 2: // square
        case 3: // triangle (leaky-integrated band-limited square)
        {
            out = t < 0.5 ? 1.0f : -1.0f;
            out += polyBlep (t, dt);
            out -= polyBlep (std::fmod (t + 0.5, 1.0), dt);

            if (params.waveform == 3)
            {
                triState = (float) (dt * 4.0) * out + (1.0f - (float) dt) * triState;
                out = triState;
            }
            break;
        }

        default: break;
    }

    phase += dt;
    if (phase >= 1.0)
        phase -= 1.0;

    return out;
}

void SynthVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    if (! isVoiceActive())
        return;

    const auto nyquistSafe = (float) getSampleRate() * 0.45f;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto envValue = env.getNextSample();

        // Only recompute cutoff every 16 samples; it's comparatively expensive.
        if ((i & 15) == 0)
        {
            const auto cutoff = params.cutoffHz * std::pow (2.0f, params.filterEnvAmount * envValue);
            filter.setCutoffFrequency (juce::jlimit (20.0f, nyquistSafe, cutoff));
        }

        const auto sample = filter.processSample (0, nextOscSample()) * envValue * level * 0.25f;

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addSample (ch, startSample + i, sample);

        if (! env.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}
