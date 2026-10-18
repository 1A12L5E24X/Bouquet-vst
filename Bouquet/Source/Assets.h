#pragma once

#include <juce_core/juce_core.h>

namespace bouquet::assets
{
// Finds a folder of shipped assets ("Flowers", "Samples") that holds `marker`.
// On a Mac the installer puts them once in /Library/Application Support/Bouquet, shared
// by every plugin format; elsewhere they sit inside the bundle (or next to the app).
// Development builds fall back to the source tree.
juce::File find (const juce::String& name, const juce::String& marker);
} // namespace bouquet::assets
