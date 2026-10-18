#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/SoundLab.h"
#include "dsp/GranularEngine.h"

namespace bouquet::ids
{
// Sound Lab
inline const juce::String instrument  = "instrument";
inline const juce::String chord       = "chord";
inline const juce::String voicing     = "voicing";
inline const juce::String strum       = "strum";
inline const juce::String labTone     = "labTone";
inline const juce::String attack      = "attack";
inline const juce::String release     = "release";
inline const juce::String octave      = "octave";
inline const juce::String sourceLevel = "sourceLevel";

// Magic Lab
inline const juce::String density   = "density";
inline const juce::String size      = "size";
inline const juce::String time      = "time";
inline const juce::String spray     = "spray";
inline const juce::String pitch     = "pitch";
inline const juce::String harmony   = "harmony";
inline const juce::String harmonyAmount = "harmonyAmount";
inline const juce::String detune    = "detune";
inline const juce::String reverse   = "reverse";
inline const juce::String chaos     = "chaos";
inline const juce::String shape     = "shape";
inline const juce::String width     = "width";
inline const juce::String feedback  = "feedback";
inline const juce::String colour    = "colour";
inline const juce::String space     = "space";
inline const juce::String decay     = "decay";
inline const juce::String life      = "life";
inline const juce::String freeze    = "freeze";
inline const juce::String mix       = "mix";
inline const juce::String auraX     = "auraX";
inline const juce::String auraY     = "auraY";
inline const juce::String output    = "output";
} // namespace bouquet::ids

namespace bouquet
{
juce::StringArray instrumentNames();
juce::StringArray instrumentDescriptions();

// The effect build opens on "live input"; the instrument build on the piano.
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout (bool isEffect);

// Reads the parameter tree into DSP settings. Aura macros are applied here.
SoundLabSettings readSoundLab (const juce::AudioProcessorValueTreeState&);
GrainSettings readGrains (const juce::AudioProcessorValueTreeState&);
} // namespace bouquet
