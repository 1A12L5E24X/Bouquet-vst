#include "GranularEngine.h"

namespace bouquet
{
const std::vector<HarmonySet>& harmonySets()
{
    static const std::vector<HarmonySet> sets {
        { "unison",     { 0 } },
        { "octaves",    { 0, 12, -12 } },
        { "fifths",     { 0, 7, 12, 19 } },
        { "major",      { 0, 4, 7, 12 } },
        { "minor",      { 0, 3, 7, 12 } },
        { "sus",        { 0, 5, 7, 12, 14 } },
        { "maj7",       { 0, 4, 7, 11, 12 } },
        { "min9",       { 0, 3, 7, 10, 14 } },
        { "lydian",     { 0, 4, 6, 7, 11, 14 } },
        { "pentatonic", { 0, 2, 4, 7, 9, 12 } },
        { "whole tone", { 0, 2, 4, 6, 8, 10 } },
        { "shimmer",    { 0, 12, 24 } },
        { "sub",        { 0, -12, -24 } },
        { "cluster",    { 0, 1, 2, -1 } },
    };
    return sets;
}

void GranularEngine::prepare (double sampleRate)
{
    sr = sampleRate;

    int size = 1;
    while (size < (int) (sampleRate * 16.0))
        size <<= 1;
    bufL.assign ((size_t) size, 0.0f);
    bufR.assign ((size_t) size, 0.0f);
    bufMask = size - 1;

    for (size_t i = 0; i < riseTable.size(); ++i)
        riseTable[i] = 0.5f - 0.5f * std::cos (pi * (float) i / (float) (riseTable.size() - 1));

    const auto fsr = (float) sampleRate;
    lifePosition.prepare (fsr, 0.13f, 11u);
    lifePitch.prepare (fsr, 0.21f, 23u);
    lifeDensity.prepare (fsr, 0.09f, 37u);
    lifePan.prepare (fsr, 0.31f, 41u);
    lifeTone.prepare (fsr, 0.07f, 53u);

    reverb.prepare (fsr);
    dcL.setCutoff (fsr, 30.0f);
    dcR.setCutoff (fsr, 30.0f);
    mixRamp.reset (fsr, 0.05f);
    feedbackRamp.reset (fsr, 0.1f);
    spaceRamp.reset (fsr, 0.1f);

    samplesPerBin = std::max (1, (int) (sampleRate * memorySeconds / memoryBins));
    reset();
}

void GranularEngine::reset()
{
    std::fill (bufL.begin(), bufL.end(), 0.0f);
    std::fill (bufR.begin(), bufR.end(), 0.0f);
    writePos = 0;
    recorded = 0;
    numGrains = 0;
    samplesToNextGrain = 0.0;
    fbL = fbR = 0.0f;
    lowL.reset(); lowR.reset(); highL.reset(); highR.reset();
    dcL.reset(); dcR.reset();
    reverb.reset();
    lastDecay = -1.0f;
    lastTone = 99.0f;
    mixRamp.snap (-1.0f);
    for (auto& m : memory)
        m.store (0.0f);
}

float GranularEngine::window (float t, float fade) const
{
    const float edge = std::min (t, 1.0f - t);
    if (edge >= fade)
        return 1.0f;
    const float x = std::max (0.0f, edge / fade) * (float) (riseTable.size() - 1);
    const int i = std::min ((int) x, (int) riseTable.size() - 2);
    const float frac = x - (float) i;
    return riseTable[(size_t) i] + (riseTable[(size_t) i + 1] - riseTable[(size_t) i]) * frac;
}

void GranularEngine::spawnGrain (const GrainSettings& s, float gainComp)
{
    if (numGrains >= maxGrains)
        return;

    const auto fsr = (float) sr;
    int length = std::max (32, (int) (s.sizeSec * (1.0f + 0.3f * s.chaos * rng.bi()) * fsr));

    float semis = s.pitch + lifePitchNow;
    bool harmonic = false;
    const auto& sets = harmonySets();
    const auto& set = sets[(size_t) std::clamp (s.harmony, 0, (int) sets.size() - 1)].intervals;
    if (s.harmonyAmount > 0.0f && rng.chance (s.harmonyAmount))
    {
        const int interval = set[(size_t) (rng.next() % set.size())];
        semis += (float) interval;
        harmonic = interval != 0;
    }
    semis += rng.bi() * s.detuneCents * 0.01f;

    const bool reversed = rng.chance (s.reverse);
    const double rate = (double) semitonesToRatio (semis) * (reversed ? -1.0 : 1.0);

    // Keep the read head a safe distance from the write head for the grain's whole life.
    const double margin = 64.0;
    const double bufLen = (double) (bufMask + 1);
    const double writeSpeed = s.freeze ? 0.0 : 1.0;
    double delay = (double) ((s.timeSec * (1.0f + 0.5f * lifePosNow * s.life) + s.spray * 2.0f * rng.uni()) * fsr);

    double lo = 0.0, hi = -1.0;
    for (int attempt = 0; attempt < 6; ++attempt)
    {
        const double travel = (double) length * (writeSpeed - rate);
        lo = std::max (margin, margin - travel);
        hi = std::min (bufLen - margin, bufLen - margin - travel);
        if (lo <= hi)
            break;
        length /= 2;
    }
    if (lo > hi)
        return;

    // Before the memory has filled, don't reach back into silence.
    if (! s.freeze)
        hi = std::min (hi, std::max (lo, (double) recorded - margin));
    delay = std::clamp (delay, lo, hi);

    double pos = (double) writePos - delay;
    while (pos < 0.0) pos += bufLen;

    const float pan = std::clamp (rng.bi() * s.width + lifePanNow * 0.3f * s.life, -1.0f, 1.0f);
    const float angle = (pan + 1.0f) * pi * 0.25f;
    const float amp = gainComp * (0.75f + 0.25f * rng.uni());

    auto& g = grains[(size_t) numGrains++];
    g.pos = pos;
    g.rate = rate;
    g.length = length;
    g.age = 0;
    g.invLength = 1.0f / (float) length;
    g.fade = 0.5f - 0.46f * s.shape;
    g.gainL = std::cos (angle) * 1.41421356f * amp;
    g.gainR = std::sin (angle) * 1.41421356f * amp;
    g.mono = std::abs (pan);

    const auto scope = eventFifo.write (1);
    if (scope.blockSize1 > 0)
        events[(size_t) scope.startIndex1] = { (float) (delay / (memorySeconds * sr)), semis, pan,
                                               (float) length / fsr, amp, reversed, harmonic };
}

void GranularEngine::process (const float* srcL, const float* srcR, float* outL, float* outR,
                              int numSamples, const GrainSettings& s)
{
    const auto fsr = (float) sr;

    // Slow modulation: the sound never sits still.
    lifePosNow = lifePosition.advance (numSamples);
    lifePitchNow = lifePitch.advance (numSamples) * s.life * 0.18f;
    lifePanNow = lifePan.advance (numSamples);
    const float lifeDens = lifeDensity.advance (numSamples);
    const float lifeTn = lifeTone.advance (numSamples);
    breath.store (0.6f * lifePosNow + 0.4f * lifeDens);

    const float density = std::max (0.1f, s.density * std::exp2 (lifeDens * s.life * 0.8f));
    const float meanSquareWindow = 1.0f - 1.25f * (0.5f - 0.46f * s.shape);
    const float overlap = density * s.sizeSec * meanSquareWindow;
    const float gainComp = std::clamp (1.0f / std::sqrt (std::max (overlap, 0.35f)), 0.08f, 1.6f);

    const float tone = std::clamp (s.tone + lifeTn * s.life * 0.15f, -1.0f, 1.0f);
    if (std::abs (tone - lastTone) > 0.002f)
    {
        lastTone = tone;
        const float lpHz = tone < 0.0f ? 18000.0f * std::exp2 (tone * 5.5f) : 19000.0f;
        const float hpHz = tone > 0.0f ? 25.0f * std::exp2 (tone * 5.5f) : 20.0f;
        lowL.setup (fsr, lpHz, 0.6f);  lowR.setup (fsr, lpHz, 0.6f);
        highL.setup (fsr, hpHz, 0.6f); highR.setup (fsr, hpHz, 0.6f);
    }
    if (std::abs (s.decaySec - lastDecay) > 0.01f)
    {
        lastDecay = s.decaySec;
        reverb.setDecay (s.decaySec, 4500.0f + 2500.0f * std::max (0.0f, tone));
    }

    if (mixRamp.value < 0.0f)
    {
        mixRamp.snap (s.mix);
        feedbackRamp.snap (s.feedback);
        spaceRamp.snap (s.space);
    }
    mixRamp.setTarget (s.mix);
    feedbackRamp.setTarget (s.freeze ? 0.0f : s.feedback);
    spaceRamp.setTarget (s.space);

    const double baseInterval = sr / (double) density;
    const double bufLen = (double) (bufMask + 1);
    float wetPeak = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = srcL[i], inR = srcR[i];

        // 1. Record the source (plus feedback) into memory.
        if (! s.freeze)
        {
            const float wl = inL + fbL, wr = inR + fbR;
            bufL[(size_t) writePos] = wl;
            bufR[(size_t) writePos] = wr;
            writePos = (writePos + 1) & bufMask;
            recorded = std::min (recorded + 1, bufMask);

            memoryPeak = std::max (memoryPeak, std::max (std::abs (wl), std::abs (wr)));
            if (++memoryCounter >= samplesPerBin)
            {
                const int head = (memoryHead.load (std::memory_order_relaxed) + 1) % memoryBins;
                memory[(size_t) head].store (memoryPeak, std::memory_order_relaxed);
                memoryHead.store (head, std::memory_order_release);
                memoryCounter = 0;
                memoryPeak = 0.0f;
            }
        }

        // 2. Schedule new grains.
        samplesToNextGrain -= 1.0;
        if (samplesToNextGrain <= 0.0)
        {
            spawnGrain (s, gainComp);
            const double randomGap = -std::log (std::max (1.0e-6f, rng.uni()));
            const double factor = 1.0 + (double) s.chaos * (randomGap - 1.0);
            samplesToNextGrain += baseInterval * std::clamp (factor, 0.05, 6.0);
        }

        // 3. Render active grains.
        float wl = 0.0f, wr = 0.0f;
        for (int g = 0; g < numGrains;)
        {
            auto& gr = grains[(size_t) g];
            const float w = window ((float) gr.age * gr.invLength, gr.fade);

            const double p = gr.pos;
            const int i0 = (int) p;
            const float frac = (float) (p - (double) i0);
            const auto im1 = (size_t) ((i0 - 1) & bufMask), ia = (size_t) (i0 & bufMask),
                       ib = (size_t) ((i0 + 1) & bufMask), ic = (size_t) ((i0 + 2) & bufMask);
            const float l = hermite (bufL[im1], bufL[ia], bufL[ib], bufL[ic], frac);
            const float r = hermite (bufR[im1], bufR[ia], bufR[ib], bufR[ic], frac);
            const float m = 0.5f * (l + r);

            wl += w * gr.gainL * (l + (m - l) * gr.mono);
            wr += w * gr.gainR * (r + (m - r) * gr.mono);

            gr.pos += gr.rate;
            if (gr.pos >= bufLen) gr.pos -= bufLen;
            else if (gr.pos < 0.0) gr.pos += bufLen;

            if (++gr.age >= gr.length)
                gr = grains[(size_t) --numGrains];
            else
                ++g;
        }

        // 4. Colour, feed back, add space.
        wl = highL.hp (lowL.lp (wl));
        wr = highR.hp (lowR.lp (wr));

        const float fb = feedbackRamp.next();
        fbL = softClip (dcL.hp (wl) * fb);
        fbR = softClip (dcR.hp (wr) * fb);

        const float space = spaceRamp.next();
        float rl = 0.0f, rr = 0.0f;
        reverb.process (wl, wr, rl, rr);
        const float wetL = wl * (1.0f - 0.45f * space) + rl * space * 1.3f;
        const float wetR = wr * (1.0f - 0.45f * space) + rr * space * 1.3f;
        wetPeak = std::max (wetPeak, std::max (std::abs (wetL), std::abs (wetR)));

        const float mix = mixRamp.next();
        const float dryGain = std::cos (mix * pi * 0.5f);
        const float wetGain = std::sin (mix * pi * 0.5f);
        outL[i] = inL * dryGain + wetL * wetGain;
        outR[i] = inR * dryGain + wetR * wetGain;
    }

    wetEnvelope = std::max (wetPeak, wetEnvelope * 0.85f);
    wetLevel.store (wetEnvelope);
    activeGrains.store (numGrains);
}

int GranularEngine::popEvents (GrainEvent* dest, int maxEvents)
{
    const auto scope = eventFifo.read (std::min (maxEvents, eventFifo.getNumReady()));
    int n = 0;
    for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = events[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = events[(size_t) (scope.startIndex2 + i)];
    return n;
}

void GranularEngine::readMemory (std::array<float, memoryBins>& dest) const
{
    const int head = memoryHead.load (std::memory_order_acquire);
    for (int i = 0; i < memoryBins; ++i)
        dest[(size_t) i] = memory[(size_t) ((head - i + memoryBins) % memoryBins)].load (std::memory_order_relaxed);
}
} // namespace bouquet
