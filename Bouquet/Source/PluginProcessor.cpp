#include "PluginProcessor.h"
#include "ui/PluginEditor.h"

using namespace bouquet;

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

juce::AudioProcessor::BusesProperties BouquetProcessor::makeBuses (bool isEffect)
{
    // The instrument also offers an (optional, off by default) input so hosts that
    // route audio into instruments can feed "live input".
    return BusesProperties()
        .withInput ("Input", juce::AudioChannelSet::stereo(), isEffect)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

BouquetProcessor::BouquetProcessor (bool effect)
    : AudioProcessor (makeBuses (effect)),
      isEffect (effect),
      apvts (*this, nullptr, "BOUQUET", createParameterLayout (effect))
{
    apvts.state.setProperty ("spell", 0, nullptr);
    apvts.state.setProperty ("preset", "", nullptr);
    presets.rescan();
}

BouquetProcessor::~BouquetProcessor()
{
    cancelPendingUpdate();
    stopTimer();
}

bool BouquetProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    const auto in = layouts.getMainInputChannelSet();
    if (isEffect)
        return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();

    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void BouquetProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    soundLab.prepare (sampleRate);
    forest.prepare ((float) sampleRate);
    engine.prepare (sampleRate);
    magicGain.reset ((float) sampleRate, 0.03f);
    magicGain.snap (apvts.getRawParameterValue (ids::magic)->load() > 0.5f ? 1.0f : 0.0f);
    scratch.setSize (6, std::max (samplesPerBlock, 512), false, true, false);

    sourceGain.reset (sampleRate, 0.05);
    outputGain.reset (sampleRate, 0.05);
    sourceGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ids::sourceLevel)->load()));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue (ids::output)->load()));
}

void BouquetProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
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

    // The instruments are heard as if recorded in a forest clearing.
    forest.process (srcL, srcR, n, apvts.getRawParameterValue (ids::forest)->load(),
                    apvts.getRawParameterValue (ids::wildlife)->load());

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

    // The Magic Lab, unless it's switched off (then it costs nothing at all).
    magicGain.setTarget (apvts.getRawParameterValue (ids::magic)->load() > 0.5f ? 1.0f : 0.0f);
    if (magicGain.value <= 0.0f && magicGain.target <= 0.0f)
    {
        std::copy_n (srcL, n, outL);
        std::copy_n (srcR, n, outR);
        engine.wetLevel.store (0.0f);
        engine.activeGrains.store (0);
    }
    else
    {
        engine.process (srcL, srcR, outL, outR, n, readGrains (apvts));
        for (int i = 0; i < n; ++i)
        {
            const float g = magicGain.next();
            outL[i] = srcL[i] + (outL[i] - srcL[i]) * g;
            outR[i] = srcR[i] + (outR[i] - srcR[i]) * g;
        }
    }

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

juce::AudioProcessorEditor* BouquetProcessor::createEditor()
{
    return new BouquetEditor (*this);
}

//==============================================================================
int BouquetProcessor::getNumPrograms()   { return (int) PresetLibrary::factory().size(); }
int BouquetProcessor::getCurrentProgram() { return std::max (0, currentFactory.load()); }

void BouquetProcessor::setCurrentProgram (int index)
{
    // Hosts often re-send the current program after restoring state; don't clobber it.
    if (! juce::isPositiveAndBelow (index, getNumPrograms()) || index == currentFactory.load())
        return;

    // Hosts may call this from the audio thread; the glide runs on the message thread.
    pendingProgram.store (index);
    triggerAsyncUpdate();
}

void BouquetProcessor::handleAsyncUpdate()
{
    const int index = pendingProgram.exchange (-1);
    if (index >= 0)
        applyPreset (PresetLibrary::factory()[(size_t) index]);
}

const juce::String BouquetProcessor::getProgramName (int index)
{
    if (juce::isPositiveAndBelow (index, getNumPrograms()))
        return PresetLibrary::factory()[(size_t) index].name;
    return {};
}

int BouquetProcessor::getCurrentSpell() const
{
    return currentSpell.load();
}

void BouquetProcessor::castSpell (int index, double glideSeconds)
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

void BouquetProcessor::applyPreset (const Preset& preset, double glideSeconds)
{
    apvts.state.setProperty ("preset", preset.name, nullptr);
    int factoryIndex = -1;
    const auto& factory = PresetLibrary::factory();
    for (size_t i = 0; i < factory.size(); ++i)
        if (factory[i].name == preset.name && preset.factory)
            factoryIndex = (int) i;
    currentFactory.store (factoryIndex);

    std::vector<std::pair<juce::String, float>> targets;
    for (auto& [id, value] : preset.values)
        if (id != ids::output)
            targets.push_back ({ id, value });
    startMorph (targets, glideSeconds);
}

Preset BouquetProcessor::plantPreset (const juce::String& name)
{
    auto preset = capturePreset (apvts, name);
    presets.save (preset);
    if (auto* saved = presets.find (name))
        preset = *saved;
    apvts.state.setProperty ("preset", preset.name, nullptr);
    currentFactory.store (-1);
    return preset;
}

void BouquetProcessor::mutate()
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

void BouquetProcessor::startMorph (const std::vector<std::pair<juce::String, float>>& targets, double seconds)
{
    for (auto& t : morphTargets)
        t.param->endChangeGesture();
    morphTargets.clear();

    for (auto& [id, value] : targets)
    {
        auto* p = apvts.getParameter (id);
        if (p == nullptr)   // a preset from a newer or older version
            continue;
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

void BouquetProcessor::timerCallback()
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
void BouquetProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void BouquetProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    currentSpell.store (juce::jlimit (0, (int) spellBook().size() - 1, (int) apvts.state.getProperty ("spell", 0)));
    currentFactory.store (-1);
    const auto& factory = PresetLibrary::factory();
    for (size_t i = 0; i < factory.size(); ++i)
        if (factory[i].name == getCurrentPresetName())
            currentFactory.store ((int) i);

    // replaceState skips parameters whose cached (snapped) value already matches, which
    // can leave a switch or choice sitting between steps. Re-assert every value exactly.
    for (auto* p : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            ranged->setValueNotifyingHost (ranged->convertTo0to1 (apvts.getRawParameterValue (ranged->getParameterID())->load()));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BouquetProcessor();
}
