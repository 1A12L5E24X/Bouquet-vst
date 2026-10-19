#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace bouquet
{
constexpr float pi    = 3.14159265358979f;
constexpr float twoPi = 6.28318530717959f;

inline float midiToHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
inline float semitonesToRatio (float st) { return std::exp2 (st / 12.0f); }

// Small, fast, deterministic per-instance random source (xorshift32).
struct Random
{
    explicit Random (uint32_t seed = 0x9E3779B9u) : state (seed ? seed : 1u) {}

    uint32_t next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return state;
    }

    float uni()     { return (float) (next() >> 8) * (1.0f / 16777216.0f); } // [0, 1)
    float bi()      { return uni() * 2.0f - 1.0f; }                           // [-1, 1)
    bool chance (float p) { return uni() < p; }

    uint32_t state;
};

// Topology-preserving-transform state variable filter (Cytomic / Simper).
struct Svf
{
    void setup (float sampleRate, float cutoffHz, float q)
    {
        cutoffHz = std::clamp (cutoffHz, 10.0f, sampleRate * 0.49f);
        g = std::tan (pi * cutoffHz / sampleRate);
        k = 1.0f / std::max (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() { ic1 = ic2 = 0.0f; }

    // Processes one sample and stores low/band/high outputs.
    void tick (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        low = v2;
        band = v1;
        high = x - k * v1 - v2;
    }

    float lp (float x) { tick (x); return low; }
    float bp (float x) { tick (x); return band; }
    float hp (float x) { tick (x); return high; }

    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    float ic1 = 0, ic2 = 0;
    float low = 0, band = 0, high = 0;
};

struct OnePole
{
    void setCutoff (float sampleRate, float hz) { a = 1.0f - std::exp (-twoPi * hz / sampleRate); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0.0f; }

    float a = 1.0f, z = 0.0f;
};

// Linear ramp towards a target over a fixed number of samples.
struct Ramp
{
    void reset (float sampleRate, float seconds) { steps = std::max (1, (int) (sampleRate * seconds)); }
    void setTarget (float t)
    {
        if (! (t < target || t > target)) return;
        target = t;
        remaining = steps;
        inc = (target - value) / (float) steps;
    }
    void snap (float v) { value = target = v; remaining = 0; }
    float next()
    {
        if (remaining > 0) { value += inc; if (--remaining == 0) value = target; }
        return value;
    }

    float value = 0, target = 0, inc = 0;
    int steps = 1, remaining = 0;
};

// Smoothly wandering random value in [-1, 1]: the "life" behind the sound.
struct Wander
{
    void prepare (float sampleRate, float rateHz, uint32_t seed)
    {
        rng = Random (seed);
        inc = rateHz / sampleRate;
        a = rng.bi(); b = rng.bi();
    }

    // Advance by n samples (called per block) and return the current value.
    float advance (int n)
    {
        phase += inc * (float) n;
        while (phase >= 1.0f) { phase -= 1.0f; a = b; b = rng.bi(); }
        const float t = phase * phase * (3.0f - 2.0f * phase);
        value = a + (b - a) * t;
        return value;
    }

    Random rng;
    float phase = 0, inc = 0, a = 0, b = 0, value = 0;
};

inline float softClip (float x) { return std::tanh (x); }

// 4-point, 3rd-order Hermite interpolation.
inline float hermite (float xm1, float x0, float x1, float x2, float t)
{
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float bNeg = w + a;
    return ((((a * t) - bNeg) * t + c) * t + x0);
}

// PolyBLEP residual for band-limited discontinuities.
inline float polyBlep (float t, float dt)
{
    if (t < dt)        { t /= dt;          return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
} // namespace bouquet
