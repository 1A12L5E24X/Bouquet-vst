#pragma once

#include <juce_core/juce_core.h>

namespace bouquet::assets
{
// Finds a folder of shipped assets ("Flowers", "Samples") that holds `marker`.
// On a Mac the installer puts them once in /Library/Application Support/Bouquet, shared
// by every plugin format; elsewhere they sit inside the bundle (or next to the app).
// Development builds fall back to the source tree.
juce::File find (const juce::String& name, const juce::String& marker);

// Appends a line to Bouquet's log (~/Library/Logs/Bouquet.log on a Mac), so a user can
// tell us what the plugin found on their machine.
void log (const juce::String& line);
} // namespace bouquet::assets
