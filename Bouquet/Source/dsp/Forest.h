#pragma once

#include <array>
#include <vector>
#include "DspUtils.h"
#include "Reverb.h"

namespace bouquet
{
// Places the Sound Lab instruments in a forest clearing, as if recorded there with a
// spaced pair of microphones: sparse early reflections off tree trunks (no walls, so
// no flutter), a soft open-air tail, high frequencies fading with distance, and a
// living ambience of wind, leaves, a brook, distant birds and insects that breathes in
// while you play and settles when you stop. The notes you play bring weather: low ones
// start rain (and, when it pours, far-off thunder); high ones let in the light (more birds
// and dew glittering in the sun).
class Forest
{
public:
    void prepare (float sampleRate)
    {
        sr = sampleRate;
        int size = 1;
        while (size < (int) (sampleRate * 0.15f))
            size <<= 1;
        for (auto& b : erBuffer)
            b.assign ((size_t) size, 0.0f);
        erMask = size - 1;
        erPos = 0;

        // Tree trunks at scattered distances; each ear hears a different set.
        Random r (0x7eee5u);
        for (size_t ch = 0; ch < 2; ++ch)
            for (auto& tap : taps[ch])
            {
                const float ms = 9.0f + r.uni() * 85.0f;
                tap.delay = (int) (ms * 0.001f * sampleRate);
                tap.gain = std::exp (-ms / 38.0f) * (0.45f + 0.55f * r.uni()) * (r.chance (0.5f) ? 1.0f : -1.0f);
            }

        tail.prepare (sampleRate);
        tail.setDecay (1.7f, 2600.0f);
        for (size_t ch = 0; ch < 2; ++ch)
        {
            erDamp[ch].setCutoff (sampleRate, 3800.0f);
            air[ch].setCutoff (sampleRate, 16000.0f);
            lowCut[ch].setCutoff (sampleRate, 35.0f);
            windLow[ch].setCutoff (sampleRate, 420.0f);
            windBody[ch].setup (sampleRate, 700.0f, 0.6f);
            leaves[ch].setup (sampleRate, 5200.0f, 0.5f);
            birdTone[ch].setCutoff (sampleRate, 6500.0f);
            rainBed[ch].setup (sampleRate, 3600.0f, 0.5f);
            rainHiss[ch].setCutoff (sampleRate, 6500.0f);
            brookBand[ch].setup (sampleRate, 900.0f, 2.0f);
            brookWander[ch].prepare (sampleRate / 64.0f, 0.9f + 0.3f * (float) ch, 307u + (uint32_t) ch);
            thunderLow[ch].setCutoff (sampleRate, 120.0f);
        }
        rainNow = shineNow = thunder = thunderTarget = 0.0f;
        rainDrips = {};
        twinkles = {};
        gust.prepare (sampleRate, 0.07f, 101u);
        gust2.prepare (sampleRate, 0.11f, 203u);
        activity = ambience = 0.0f;
        nextBird = (int) (sampleRate * 1.2f);
        birds = {};
        reset();
    }

    void reset()
    {
        for (auto& b : erBuffer)
            std::fill (b.begin(), b.end(), 0.0f);
        tail.reset();
    }

    // forest: 0 = close and dry .. 1 = deep in the trees.  wildlife: ambience level.
    // rain, shine: the weather the notes bring, 0..1 (see Weather.h).
    void process (float* left, float* right, int numSamples, float forestAmount, float wildlife,
                  float rain = 0.0f, float shine = 0.0f)
    {
        float* io[2] = { left, right };
        const float airHz = 18000.0f - 9500.0f * forestAmount;
        air[0].setCutoff (sr, airHz);
        air[1].setCutoff (sr, airHz);

        // The forest "breathes in" while instruments sound and settles a few seconds after.
        const float attack = 1.0f - std::exp (-1.0f / (0.8f * sr));
        const float release = 1.0f - std::exp (-1.0f / (4.0f * sr));
        // Weather comes in over a second or two and clears more slowly.
        const float weatherUp = 1.0f - std::exp (-1.0f / (1.2f * sr));
        const float weatherDown = 1.0f - std::exp (-1.0f / (3.5f * sr));

        for (int i = 0; i < numSamples; ++i)
        {
            rainNow += (rain - rainNow) * (rain > rainNow ? weatherUp : weatherDown);
            shineNow += (shine - shineNow) * (shine > shineNow ? weatherUp : weatherDown);
            const float inL = left[i], inR = right[i];
            const float level = std::max (std::abs (inL), std::abs (inR));
            activity += (level - activity) * (level > activity ? 0.01f : 0.0005f);
            const float target = wildlife * std::min (1.0f, activity * 12.0f);
            ambience += (target - ambience) * (target > ambience ? attack : release);

            float ambL = 0.0f, ambR = 0.0f;
            if (ambience > 1.0e-4f)
                renderAmbience (ambL, ambR);

            erBuffer[0][(size_t) erPos] = inL;
            erBuffer[1][(size_t) erPos] = inR;

            float er[2] = { 0.0f, 0.0f };
            for (size_t ch = 0; ch < 2; ++ch)
            {
                for (const auto& tap : taps[ch])
                    er[ch] += erBuffer[ch][(size_t) ((erPos - tap.delay) & erMask)] * tap.gain;
                er[ch] = erDamp[ch].lp (er[ch]);
            }
            erPos = (erPos + 1) & erMask;

            // The tail hears the instrument through the trees, and the distant birds.
            float tl = 0.0f, tr = 0.0f;
            tail.process (er[0] * 0.6f + ambL * 0.5f, er[1] * 0.6f + ambR * 0.5f, tl, tr);

            const float dry = 1.0f - 0.3f * forestAmount;
            const float ambOut[2] = { ambL, ambR };
            const float tails[2] = { tl, tr };
            for (size_t ch = 0; ch < 2; ++ch)
            {
                const float x = ch == 0 ? inL : inR;
                float y = air[ch].lp (x) * dry
                        + forestAmount * (er[ch] * 0.55f + tails[ch] * 0.5f)
                        + ambOut[ch] * ambience;
                io[ch][i] = lowCut[ch].hp (y);
            }
        }
    }

private:
    struct Tap { int delay; float gain; };

    // A single pattering drop of rain, or a point of light on dew: a short decaying sine.
    struct Ping { float phase = 0.0f, hz = 1000.0f, amp = 0.0f, decay = 0.99f, pan = 0.0f; };

    struct Bird
    {
        bool active = false;
        int notesLeft = 0, sample = 0, length = 0, gap = 0;
        float phase = 0.0f, f0 = 3000.0f, f1 = 3500.0f, vibrato = 0.0f, vibPhase = 0.0f, pan = 0.0f, gain = 0.0f;
    };

    void startBird (Bird& b)
    {
        b.active = true;
        b.notesLeft = 2 + (int) (rng.uni() * 5.0f);
        b.pan = rng.bi() * 0.8f;
        b.gain = (0.018f + 0.02f * rng.uni()) * (1.0f + 0.8f * shineNow);
        b.f0 = 2200.0f + rng.uni() * 2800.0f;
        newNote (b);
    }

    void newNote (Bird& b)
    {
        b.sample = 0;
        b.length = (int) ((0.04f + rng.uni() * 0.1f) * sr);
        b.gap = (int) ((0.03f + rng.uni() * 0.07f) * sr);
        b.f0 *= 0.9f + rng.uni() * 0.25f;
        b.f1 = b.f0 * (0.7f + rng.uni() * 0.65f);
        b.vibrato = rng.uni() < 0.4f ? 25.0f + rng.uni() * 20.0f : 0.0f;
    }

    void renderAmbience (float& outL, float& outR)
    {
        // Wind: low, slowly gusting rushes of air, different in each ear.
        const float g = 0.55f + 0.45f * gust.advance (1) + 0.2f * gust2.advance (1);
        float wind[2];
        for (size_t ch = 0; ch < 2; ++ch)
            wind[ch] = windBody[ch].bp (windLow[ch].lp (rng.bi())) * 0.09f * std::max (0.1f, g);

        // Leaves: crisp rustles riding on the gusts.
        if (rng.uni() < 0.00006f * (0.5f + g))
            rustle = 0.6f + 0.4f * rng.uni();
        rustle *= 0.99985f;
        float leaf[2];
        for (size_t ch = 0; ch < 2; ++ch)
            leaf[ch] = leaves[ch].bp (rng.bi()) * rustle * 0.025f * (0.6f + 0.4f * g);

        // Birds, somewhere off between the trees.
        if (--nextBird <= 0)
        {
            for (auto& b : birds)
                if (! b.active) { startBird (b); break; }
            nextBird = (int) ((1.5f + rng.uni() * 5.0f) * sr / (1.0f + 2.5f * shineNow));
        }
        float bird[2] = { 0.0f, 0.0f };
        for (auto& b : birds)
        {
            if (! b.active)
                continue;
            if (b.sample < b.length)
            {
                const float t = (float) b.sample / (float) b.length;
                const float env = std::sin (pi * t);
                b.vibPhase += b.vibrato / sr;
                const float hz = (b.f0 + (b.f1 - b.f0) * t) * (1.0f + 0.04f * std::sin (twoPi * b.vibPhase));
                b.phase += hz / sr;
                b.phase -= std::floor (b.phase);
                const float s = std::sin (twoPi * b.phase) * env * env * b.gain;
                bird[0] += s * (0.5f - 0.5f * b.pan);
                bird[1] += s * (0.5f + 0.5f * b.pan);
            }
            if (++b.sample >= b.length + b.gap)
            {
                if (--b.notesLeft > 0) newNote (b);
                else b.active = false;
            }
        }

        // Insects: a faint pulsing shimmer, high up.
        cricketPhase += 4600.0f / sr;
        cricketPhase -= std::floor (cricketPhase);
        pulsePhase += 31.0f / sr;
        pulsePhase -= std::floor (pulsePhase);
        chirpPhase += 1.7f / sr;
        chirpPhase -= std::floor (chirpPhase);
        const float cricket = std::sin (twoPi * cricketPhase) * (pulsePhase < 0.4f ? 1.0f : 0.0f)
                            * (chirpPhase < 0.35f ? 1.0f : 0.0f) * 0.004f;

        // A brook somewhere down the slope: water talking over stones.
        if ((++brookTick & 63) == 0)
            for (size_t ch = 0; ch < 2; ++ch)
            {
                brookHz[ch] = 650.0f + 450.0f * brookWander[ch].advance (1) + 250.0f * (float) ch;
                brookBand[ch].setup (sr, std::max (200.0f, brookHz[ch]), 2.0f);
                brookGain[ch] = 0.6f + 0.4f * (ch == 0 ? gust2.value : -gust2.value);
            }
        float brook[2];
        for (size_t ch = 0; ch < 2; ++ch)
            brook[ch] = brookBand[ch].bp (rng.bi()) * brookBand[ch].k * 0.07f * brookGain[ch];

        // Rain: a hush of water on leaves, single drops pattering through it, and, when it
        // pours, thunder rolling in from far away.
        float rainOut[2] = { 0.0f, 0.0f };
        if (rainNow > 1.0e-3f)
        {
            for (size_t ch = 0; ch < 2; ++ch)
            {
                const float n = rng.bi();
                rainOut[ch] = (rainBed[ch].bp (n) * rainBed[ch].k * 0.22f + rainHiss[ch].hp (n) * 0.07f) * rainNow;
            }
            if (rng.uni() < rainNow * 70.0f / sr)
                for (auto& d : rainDrips)
                    if (d.amp < 1.0e-4f)
                    {
                        d.phase = 0.0f;
                        d.hz = 900.0f + 2400.0f * rng.uni();
                        d.amp = (0.03f + 0.05f * rng.uni()) * (0.5f + 0.5f * rainNow);
                        d.decay = std::exp (-6.9078f / ((0.025f + 0.05f * rng.uni()) * sr));
                        d.pan = rng.bi();
                        break;
                    }
            for (auto& d : rainDrips)
            {
                if (d.amp < 1.0e-5f)
                    continue;
                d.phase += d.hz / sr;
                d.phase -= std::floor (d.phase);
                const float s = std::sin (twoPi * d.phase) * d.amp;
                rainOut[0] += s * (0.5f - 0.5f * d.pan);
                rainOut[1] += s * (0.5f + 0.5f * d.pan);
                d.amp *= d.decay;
            }
            if (rainNow > 0.6f && rng.uni() < 1.0f / (22.0f * sr))
                thunderTarget = 0.6f + 0.4f * rng.uni();
        }
        thunder += (thunderTarget - thunder) * 0.00025f;
        thunderTarget *= 0.999985f;
        float rumble[2] = { 0.0f, 0.0f };
        if (thunder > 1.0e-3f)
            for (size_t ch = 0; ch < 2; ++ch)
                rumble[ch] = thunderLow[ch].lp (rng.bi()) * thunder * 5.0f;

        // Light: dew glittering as the sun reaches it, a few glassy points at a time.
        float glitter[2] = { 0.0f, 0.0f };
        if (shineNow > 1.0e-3f)
        {
            if (rng.uni() < shineNow * 9.0f / sr)
                for (auto& t : twinkles)
                    if (t.amp < 1.0e-4f)
                    {
                        t.phase = 0.0f;
                        t.hz = 3200.0f + 5200.0f * rng.uni();
                        t.amp = 0.006f + 0.01f * rng.uni();
                        t.decay = std::exp (-6.9078f / ((0.04f + 0.12f * rng.uni()) * sr));
                        t.pan = rng.bi();
                        break;
                    }
            for (auto& t : twinkles)
            {
                if (t.amp < 1.0e-5f)
                    continue;
                t.phase += t.hz / sr;
                t.phase -= std::floor (t.phase);
                const float s = (std::sin (twoPi * t.phase) + 0.4f * std::sin (2.76f * twoPi * t.phase)) * t.amp;
                glitter[0] += s * (0.5f - 0.5f * t.pan);
                glitter[1] += s * (0.5f + 0.5f * t.pan);
                t.amp *= t.decay;
            }
        }

        outL = wind[0] + leaf[0] + birdTone[0].lp (bird[0]) + cricket * 0.7f + brook[0] + rainOut[0] + rumble[0] + glitter[0];
        outR = wind[1] + leaf[1] + birdTone[1].lp (bird[1]) + cricket + brook[1] + rainOut[1] + rumble[1] + glitter[1];
    }

    float sr = 48000.0f;
    std::array<std::vector<float>, 2> erBuffer;
    int erMask = 0, erPos = 0;
    std::array<std::array<Tap, 10>, 2> taps {};
    std::array<OnePole, 2> erDamp, air, lowCut, windLow, birdTone;
    std::array<Svf, 2> windBody, leaves, rainBed, brookBand;
    std::array<OnePole, 2> rainHiss, thunderLow;
    std::array<Wander, 2> brookWander;
    std::array<float, 2> brookHz { 900.0f, 900.0f }, brookGain { 1.0f, 1.0f };
    std::array<Ping, 10> rainDrips;
    std::array<Ping, 6> twinkles;
    float rainNow = 0.0f, shineNow = 0.0f, thunder = 0.0f, thunderTarget = 0.0f;
    int brookTick = 0;
    Reverb tail;

    Random rng { 0xf0e57u };
    Wander gust, gust2;
    float activity = 0.0f, ambience = 0.0f, rustle = 0.0f;
    float cricketPhase = 0.0f, pulsePhase = 0.0f, chirpPhase = 0.0f;
    int nextBird = 48000;
    std::array<Bird, 3> birds {};
};
} // namespace bouquet
