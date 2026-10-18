#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamIDs
{
    static const juce::String wave      = "wave";
    static const juce::String attack    = "attack";
    static const juce::String decay     = "decay";
    static const juce::String sustain   = "sustain";
    static const juce::String release   = "release";
    static const juce::String cutoff    = "cutoff";
    static const juce::String resonance = "resonance";
    static const juce::String envAmount = "envAmount";
    static const juce::String gain      = "gain";
}

SimpleSynthProcessor::SimpleSynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    for (int i = 0; i < numVoices; ++i)
        synth.addVoice (new SynthVoice());

    synth.addSound (new SynthSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout SimpleSynthProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto timeRange = [] (float max) { return NormalisableRange<float> (0.001f, max, 0.0f, 0.3f); };
    auto seconds = AudioParameterFloatAttributes().withLabel ("s");

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::wave, 1 }, "Waveform",
                                                         StringArray { "Sine", "Saw", "Square", "Triangle" }, 1));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::attack, 1 }, "Attack", timeRange (5.0f), 0.01f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::decay, 1 }, "Decay", timeRange (5.0f), 0.2f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::sustain, 1 }, "Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.8f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::release, 1 }, "Release", timeRange (10.0f), 0.3f, seconds));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::cutoff, 1 }, "Cutoff",
                                                        NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 2000.0f,
                                                        AudioParameterFloatAttributes().withLabel ("Hz")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::resonance, 1 }, "Resonance", NormalisableRange<float> (0.3f, 10.0f, 0.0f, 0.4f), 0.7f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::envAmount, 1 }, "Filter Env", NormalisableRange<float> (-4.0f, 4.0f), 2.0f,
                                                        AudioParameterFloatAttributes().withLabel ("oct")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::gain, 1 }, "Gain", NormalisableRange<float> (-48.0f, 6.0f), -6.0f,
                                                        AudioParameterFloatAttributes().withLabel ("dB")));
    return layout;
}

bool SimpleSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void SimpleSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);

    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            v->prepare (sampleRate, samplesPerBlock);

    outputGain.reset (sampleRate, 0.02);
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::gain)->load()));
}

VoiceParams SimpleSynthProcessor::readParams() const
{
    auto get = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };

    VoiceParams p;
    p.waveform = (int) get (ParamIDs::wave);
    p.ampEnv = { get (ParamIDs::attack), get (ParamIDs::decay), get (ParamIDs::sustain), get (ParamIDs::release) };
    p.cutoffHz = get (ParamIDs::cutoff);
    p.resonance = get (ParamIDs::resonance);
    p.filterEnvAmount = get (ParamIDs::envAmount);
    return p;
}

void SimpleSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // Merge notes played on the on-screen keyboard.
    keyboardState.processNextMidiBuffer (midi, 0, buffer.getNumSamples(), true);

    const auto params = readParams();
    for (int i = 0; i < synth.getNumVoices(); ++i)
        if (auto* v = dynamic_cast<SynthVoice*> (synth.getVoice (i)))
            v->setParams (params);

    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ParamIDs::gain)->load()));
    outputGain.applyGain (buffer, buffer.getNumSamples());
}

juce::AudioProcessorEditor* SimpleSynthProcessor::createEditor()
{
    return new SimpleSynthEditor (*this);
}

void SimpleSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SimpleSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SimpleSynthProcessor();
}
