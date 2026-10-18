#include "PluginProcessor.h"
#include "ui/PluginEditor.h"

using namespace murmur;

namespace
{
    // Above 0.7 the signal is bent softly towards 1.0 instead of clipping.
    float gentleLimit (float x)
    {
        const float a = std::abs (x);
        if (a <= 0.7f)
            return x;
        return std::copysign (0.7f + 0.3f * std::tanh ((a - 0.7f) / 0.3f), x);
    }
}

juce::AudioProcessor::BusesProperties MurmurProcessor::makeBuses (bool isEffect)
{
    // The instrument also offers an (optional, off by default) input so hosts that
    // route audio into instruments can feed "live input".
    return BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), isEffect)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

MurmurProcessor::MurmurProcessor (bool effect)
    : AudioProcessor (makeBuses (effect)),
      isEffect (effect),
      apvts (*this, nullptr, "MURMUR", createParameterLayout (effect))
{
    apvts.state.setProperty ("spell", 0, nullptr);
}

MurmurProcessor::~MurmurProcessor()
{
    cancelPendingUpdate();
    stopTimer();
}

bool MurmurProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    if (isEffect)
        return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();

    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MurmurProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    soundLab.prepare (sampleRate);
    engine.prepare (sampleRate);
    scratch.setSize (6, std::max (samplesPerBlock, 512), false, true, false);

    sourceGain.reset (sampleRate, 0.05);
    outputGain.reset (sampleRate, 0.05);
    sourceGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ids::sourceLevel)->load()));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ids::output)->load()));
}

void MurmurProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n == 0)
        return;

    if (scratch.getNumSamples() < n)
        scratch.setSize (6, n, false, true, true);

    float* inL  = scratch.getWritePointer (0);
    float* inR  = scratch.getWritePointer (1);
    float* srcL = scratch.getWritePointer (2);
    float* srcR = scratch.getWritePointer (3);
    float* outL = scratch.getWritePointer (4);
    float* outR = scratch.getWritePointer (5);

    // Capture the live input before the buffer is reused for output.
    const int numIn = getMainBusNumInputChannels();
    if (numIn > 0)
    {
        std::copy_n (buffer.getReadPointer (0), n, inL);
        std::copy_n (buffer.getReadPointer (numIn > 1 ? 1 : 0), n, inR);
    }
    else
    {
        std::fill_n (inL, n, 0.0f);
        std::fill_n (inR, n, 0.0f);
    }

    keyboardState.processNextMidiBuffer (midi, 0, n, true);

    const auto lab = readSoundLab (apvts);
    soundLab.setSettings (lab);
    soundLab.process (midi, srcL, srcR, n);
    soundingVoices.store (soundLab.activeVoiceCount());

    const bool live = lab.instrument == Instrument::liveInput;
    sourceGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ids::sourceLevel)->load()));
    float srcPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = sourceGain.getNextValue();
        srcL[i] = (srcL[i] + (live ? inL[i] : 0.0f)) * g;
        srcR[i] = (srcR[i] + (live ? inR[i] : 0.0f)) * g;
        srcPeak = std::max (srcPeak, std::max (std::abs (srcL[i]), std::abs (srcR[i])));
    }

    engine.process (srcL, srcR, outL, outR, n, readGrains (apvts));

    outputGain.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ids::output)->load()));
    float outPeak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float g = outputGain.getNextValue();
        outL[i] = gentleLimit (outL[i] * g);
        outR[i] = gentleLimit (outR[i] * g);
        outPeak = std::max (outPeak, std::max (std::abs (outL[i]), std::abs (outR[i])));
    }

    sourceEnv = std::max (srcPeak, sourceEnv * 0.85f);
    outputEnv = std::max (outPeak, outputEnv * 0.85f);
    sourceMeter.store (sourceEnv);
    outputMeter.store (outputEnv);

    const int numOut = getMainBusNumOutputChannels();
    if (numOut == 1)
    {
        auto* d = buffer.getWritePointer (0);
        for (int i = 0; i < n; ++i)
            d[i] = 0.5f * (outL[i] + outR[i]);
    }
    else if (numOut >= 2)
    {
        std::copy_n (outL, n, buffer.getWritePointer (0));
        std::copy_n (outR, n, buffer.getWritePointer (1));
    }
    for (int ch = std::max (numOut, 2); ch < buffer.getNumChannels(); ++ch)
        buffer.clear (ch, 0, n);

    midi.clear();
}

juce::AudioProcessorEditor* MurmurProcessor::createEditor()
{
    return new MurmurEditor (*this);
}

//==============================================================================
int MurmurProcessor::getNumPrograms()   { return (int) spellBook().size(); }
int MurmurProcessor::getCurrentProgram() { return getCurrentSpell(); }

void MurmurProcessor::setCurrentProgram (int index)
{
    // Hosts often re-send the current program after restoring state; don't clobber it.
    if (! juce::isPositiveAndBelow (index, getNumPrograms()) || index == getCurrentSpell())
        return;

    // Hosts may call this from the audio thread; the glide runs on the message thread.
    pendingProgram.store (index);
    triggerAsyncUpdate();
}

void MurmurProcessor::handleAsyncUpdate()
{
    const int index = pendingProgram.exchange (-1);
    if (index >= 0)
        castSpell (index);
}

const juce::String MurmurProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        return spellBook()[(size_t) index].name;
    return {};
}

int MurmurProcessor::getCurrentSpell() const
{
    return currentSpell.load();
}

void MurmurProcessor::castSpell (int index, double glideSeconds)
{
    const auto& book = spellBook();
    if (! juce::isPositiveAndBelow (index, (int) book.size()))
        return;

    apvts.state.setProperty ("spell", index, nullptr);
    currentSpell.store (index);
    const auto& v = book[(size_t) index].values;

    startMorph ({
        { ids::density, v.density }, { ids::size, v.sizeMs }, { ids::time, v.timeSec }, { ids::spray, v.spray },
        { ids::pitch, v.pitch }, { ids::harmony, (float) v.harmony }, { ids::harmonyAmount, v.harmonyAmount },
        { ids::detune, v.detune }, { ids::reverse, v.reverse }, { ids::chaos, v.chaos }, { ids::shape, v.shape },
        { ids::width, v.width }, { ids::feedback, v.feedback }, { ids::colour, v.colour }, { ids::space, v.space },
        { ids::decay, v.decay }, { ids::life, v.life }, { ids::freeze, v.freeze ? 1.0f : 0.0f }, { ids::mix, v.mix },
        { ids::auraX, 0.5f }, { ids::auraY, 0.5f },
    }, glideSeconds);
}

void MurmurProcessor::mutate()
{
    std::vector<std::pair<juce::String, float>> targets;
    for (auto* id : { &ids::density, &ids::size, &ids::time, &ids::spray, &ids::detune, &ids::reverse,
                      &ids::chaos, &ids::width, &ids::feedback, &ids::colour, &ids::space, &ids::life })
    {
        auto* p = apvts.getParameter (*id);
        const float norm = juce::jlimit (0.0f, 1.0f, p->getValue() + (random.nextFloat() * 2.0f - 1.0f) * 0.18f);
        targets.push_back ({ *id, p->convertFrom0to1 (norm) });
    }
    if (random.nextFloat() < 0.5f)
        targets.push_back ({ ids::harmony, (float) random.nextInt ((int) harmonySets().size()) });
    targets.push_back ({ ids::harmonyAmount, random.nextFloat() });
    startMorph (targets, 1.0);
}

void MurmurProcessor::startMorph (const std::vector<std::pair<juce::String, float>>& targets, double seconds)
{
    for (auto& t : morphTargets)
        t.param->endChangeGesture();
    morphTargets.clear();

    for (auto& [id, value] : targets)
    {
        auto* p = apvts.getParameter (id);
        const float to = p->convertTo0to1 (value);

        // Switches and choices land immediately; continuous controls glide.
        if (dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr || dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (to);
            p->endChangeGesture();
            continue;
        }

        p->beginChangeGesture();
        morphTargets.push_back ({ p, p->getValue(), to });
    }

    morphStartMs = juce::Time::getMillisecondCounterHiRes();
    morphLengthMs = seconds * 1000.0;
    if (seconds > 0.0)
        startTimerHz (60);
    timerCallback();
}

void MurmurProcessor::timerCallback()
{
    const double elapsed = juce::Time::getMillisecondCounterHiRes() - morphStartMs;
    const double t = morphLengthMs <= 0.0 ? 1.0 : juce::jlimit (0.0, 1.0, elapsed / morphLengthMs);
    const float ease = (float) (t * t * (3.0 - 2.0 * t));

    for (auto& m : morphTargets)
        m.param->setValueNotifyingHost (m.from + (m.to - m.from) * ease);

    if (t >= 1.0)
    {
        for (auto& m : morphTargets)
            m.param->endChangeGesture();
        morphTargets.clear();
        stopTimer();
    }
}

//==============================================================================
void MurmurProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void MurmurProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    currentSpell.store (juce::jlimit (0, (int) spellBook().size() - 1, (int) apvts.state.getProperty ("spell", 0)));

    // replaceState skips parameters whose cached (snapped) value already matches, which
    // can leave a switch or choice sitting between steps. Re-assert every value exactly.
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (apvts.getRawParameterValue (ranged->getParameterID())->load()));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MurmurProcessor();
}
