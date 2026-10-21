#pragma once

#include <algorithm>

namespace bouquet
{
// Notes make the weather in the garden: low ones bring rain, high ones shine light, and the
// higher a flower's note, the brighter it blooms. All three are 0..1. The garden draws them
// and the forest sounds them, from the same numbers.
inline float rainForNote (float note)       { return std::clamp ((62.0f - note) / 26.0f, 0.0f, 1.0f); }   // from D4 down, full at D2
inline float lightForNote (float note)      { return std::clamp ((note - 70.0f) / 20.0f, 0.0f, 1.0f); }   // from A#4 up, full at A#6
inline float brightnessForNote (float note) { return std::clamp ((note - 36.0f) / 60.0f, 0.0f, 1.0f); }   // C2 dark .. C7 bright
} // namespace bouquet
