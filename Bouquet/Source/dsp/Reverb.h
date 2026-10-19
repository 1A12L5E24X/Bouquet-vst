#pragma once

#include <array>
#include <vector>
#include "DspUtils.h"

namespace bouquet
{
// An 8-line feedback delay network with slow delay modulation and damping,
// preceded by a short allpass diffuser. Tuned to be soft, wide and dark.
class Reverb
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        const float scale = sampleRate / 48000.0f;

        static constexpr float lineMs[numLines] = { 41.7f, 47.3f, 53.9f, 61.1f, 67.3f, 73.9f, 81.1f, 89.3f };
        for (size_t i = 0; i < numLines; ++i)
        {
            auto& l = lines[i];
            l.length = lineMs[i] * 0.001f * sampleRate;
            l.buffer.assign ((size_t) nextPow2 ((int) (l.length + 64.0f * scale) + 4), 0.0f);
            l.mask = (int) l.buffer.size() - 1;
            l.pos = 0;
            l.damp.reset();
            l.lfoPhase = (float) i / (float) numLines;
            l.lfoInc = (0.07f + 0.05f * (float) i) / sampleRate;
            l.modDepth = 12.0f * scale;
        }

        static constexpr int diffuserSamples[numDiffusers] = { 142, 107, 379, 277 };
        for (size_t ch = 0; ch < 2; ++ch)
            for (size_t i = 0; i < numDiffusers; ++i)
            {
                auto& d = diffusers[ch][i];
                const int len = (int) ((float) diffuserSamples[i] * scale * (ch == 0 ? 1.0f : 1.13f));
                d.buffer.assign ((size_t) nextPow2 (len + 2), 0.0f);
                d.mask = (int) d.buffer.size() - 1;
                d.length = len;
                d.pos = 0;
            }

        setDecay (4.0f, 6000.0f);
    }

    void setDecay (float t60Seconds, float dampingHz)
    {
        for (auto& l : lines)
        {
            l.gain = std::pow (10.0f, -3.0f * l.length / (std::max (0.1f, t60Seconds) * sr));
            l.damp.setCutoff (sr, dampingHz);
        }
    }

    void process (float inL, float inR, float& outL, float& outR)
    {
        inL = diffuse (0, inL);
        inR = diffuse (1, inR);

        std::array<float, numLines> x {};
        for (size_t i = 0; i < numLines; ++i)
            x[i] = lines[i].read();

        // Fast Walsh-Hadamard mixing (energy preserving after scaling).
        for (size_t h = 1; h < numLines; h <<= 1)
            for (size_t i = 0; i < numLines; i += h << 1)
                for (size_t j = i; j < i + h; ++j)
                {
                    const float a = x[j], b = x[j + h];
                    x[j] = a + b;
                    x[j + h] = a - b;
                }

        constexpr float norm = 0.35355339f; // 1 / sqrt(8)
        float l = 0.0f, r = 0.0f;
        for (size_t i = 0; i < numLines; ++i)
        {
            auto& line = lines[i];
            const float y = x[i] * norm;
            const float in = (i & 1) ? inR : inL;
            line.write (line.damp.lp (y) * line.gain + in * 0.5f);
            if (i & 1) r += y * ((i & 2) ? -1.0f : 1.0f);
            else       l += y * ((i & 2) ? -1.0f : 1.0f);
        }
        outL = l * 0.5f;
        outR = r * 0.5f;
    }

    void reset()
    {
        for (auto& l : lines) { std::fill (l.buffer.begin(), l.buffer.end(), 0.0f); l.damp.reset(); }
        for (auto& ch : diffusers)
            for (auto& d : ch)
                std::fill (d.buffer.begin(), d.buffer.end(), 0.0f);
    }

private:
    static constexpr size_t numLines = 8;
    static constexpr size_t numDiffusers = 4;

    static int nextPow2 (int n) { int p = 1; while (p < n) p <<= 1; return p; }

    struct Line
    {
        float read()
        {
            lfoPhase += lfoInc;
            if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
            const float d = length + modDepth * (0.5f + 0.5f * std::sin (twoPi * lfoPhase));
            const float rp = (float) pos - d;
            const int i0 = (int) std::floor (rp);
            const float frac = rp - (float) i0;
            const float a = buffer[(size_t) (i0 & mask)];
            const float b = buffer[(size_t) ((i0 + 1) & mask)];
            return a + (b - a) * frac;
        }
        void write (float v) { buffer[(size_t) pos] = v; pos = (pos + 1) & mask; }

        std::vector<float> buffer;
        int mask = 0, pos = 0;
        float length = 1000.0f, gain = 0.5f, lfoPhase = 0.0f, lfoInc = 0.0f, modDepth = 8.0f;
        OnePole damp;
    };

    struct Allpass
    {
        std::vector<float> buffer;
        int mask = 0, pos = 0, length = 100;
    };

    float diffuse (size_t ch, float x)
    {
        constexpr float g = 0.6f;
        for (auto& d : diffusers[ch])
        {
            const float delayed = d.buffer[(size_t) ((d.pos - d.length) & d.mask)];
            const float v = x + g * delayed;
            d.buffer[(size_t) d.pos] = v;
            d.pos = (d.pos + 1) & d.mask;
            x = delayed - g * v;
        }
        return x;
    }

    float sr = 48000.0f;
    std::array<Line, numLines> lines;
    std::array<std::array<Allpass, numDiffusers>, 2> diffusers;
};
} // namespace bouquet
