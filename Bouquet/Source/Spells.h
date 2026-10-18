#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace bouquet
{
// A spell is a complete Magic Lab setting with a name and a line of poetry.
struct SpellValues
{
    float density, sizeMs, timeSec, spray, pitch;
    int harmony;
    float harmonyAmount, detune, reverse, chaos, shape, width, feedback, colour, space, decay, life;
    bool freeze;
    float mix;
};

struct Spell
{
    const char* name;
    int category;
    const char* description;
    SpellValues values;
};

juce::StringArray spellCategories();
const std::vector<Spell>& spellBook();
} // namespace bouquet
