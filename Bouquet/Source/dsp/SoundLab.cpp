#include "SoundLab.h"
#include "SampleBank.h"

namespace bouquet
{
const std::vector<ChordShape>& chordShapes()
{
    static const std::vector<ChordShape> shapes {
        { "single", { 0 } },
        { "maj",    { 0, 4, 7 } },
        { "min",    { 0, 3, 7 } },
        { "sus2",   { 0, 2, 7 } },
        { "sus4",   { 0, 5, 7 } },
        { "maj7",   { 0, 4, 7, 11 } },
        { "min7",   { 0, 3, 7, 10 } },
        { "7",      { 0, 4, 7, 10 } },
        { "add9",   { 0, 4, 7, 14 } },
        { "maj9",   { 0, 4, 7, 11, 14 } },
        { "min9",   { 0, 3, 7, 10, 14 } },
        { "6/9",    { 0, 4, 9, 14 } },
        { "dim",    { 0, 3, 6, 9 } },
        { "fifths", { 0, 7, 12, 19 } },
    };
    return shapes;
}

namespace
{
    float t60ToMul (float t60Sec, float sampleRate)
    {
        return std::exp (-6.9078f / std::max (1.0f, t60Sec * sampleRate));
    }

    struct Vowel { float f[5]; float gainDb[5]; float bw[5]; };

    // Sung vowels u, o, a, e (formant frequency, level and bandwidth).
    const Vowel vowels[] = {
        { { 350, 600, 2700, 2900, 3300 }, { 0, -20, -17, -14, -26 }, { 40, 60, 100, 120, 120 } },
        { { 450, 800, 2830, 3800, 4950 }, { 0, -11, -22, -22, -50 }, { 70, 80, 100, 130, 135 } },
        { { 800, 1150, 2900, 3900, 4950 }, { 0, -6, -32, -20, -50 }, { 80, 90, 120, 130, 140 } },
        { { 350, 2000, 2800, 3600, 4950 }, { 0, -20, -15, -40, -56 }, { 60, 100, 120, 150, 200 } },
    };
}

//==============================================================================
void InstrumentVoice::prepare (float sampleRate)
{
    sr = sampleRate;
    int size = 1;
    while (size < (int) (sampleRate / 15.0f) + 8)
        size <<= 1;
    line.assign ((size_t) size, 0.0f);
    lineMask = size - 1;
    active = false;
}

void InstrumentVoice::addPartial (float hz, float a, float t60, int delaySamples)
{
    if (numPartials >= maxPartials || hz >= sr * 0.45f || hz <= 0.0f)
        return;

    const float w = twoPi * hz / sr;
    // Start at a random phase so stacked partials don't all peak together.
    const float ph = rng.uni() * twoPi;
    partials[(size_t) numPartials++] = { std::cos (ph), std::sin (ph), std::cos (w), std::sin (w), a, t60ToMul (t60, sr), delaySamples };
}

void InstrumentVoice::start (Instrument m, float midiNote, float vel, const SoundLabSettings& s, uint32_t seed,
                             const SampledInstrument* sampled)
{
    model = m;
    note = midiNote;
    freq = midiToHz (midiNote);
    velocity = vel;
    tone = s.tone;
    releaseSec = std::max (0.01f, s.release);
    rng = Random (seed);
    samplesPlayed = 0;
    numPartials = 0;
    numFormants = 0;
    strikeNoise = 0.0f;
    phase = rng.uni();
    phase2 = 0.0f;
    vibPhase = rng.uni();
    vibDepth = 0.0f;
    sustained = false;
    keyDown = true;
    active = true;

    const float pan = std::clamp ((midiNote - 60.0f) / 48.0f, -0.5f, 0.5f) * 0.6f + rng.bi() * 0.15f;
    gainL = std::cos ((pan + 1.0f) * pi * 0.25f);
    gainR = std::sin ((pan + 1.0f) * pi * 0.25f);

    float naturalAttack = 0.002f;
    percussive = true;
    toneFilter.reset();
    toneFilter.setCutoff (sr, 1500.0f * std::exp2 (tone * 3.0f));
    drift.prepare (sr, 0.7f, seed ^ 0xA5A5A5u);

    pressure = pressureNow = 0.0f;
    toneApplied = 0.0f;
    toneBase = 1500.0f * std::exp2 (tone * 3.0f);
    numHeads = 0;
    if (sampled != nullptr && model != Instrument::liveInput)
    {
        const auto layers = sampled->findLayers (midiNote, vel);
        if (layers.a != nullptr && layers.a->length > 0)
        {
            // Every note a little different, like a player never hitting a key the same way twice.
            const float detune = rng.bi() * 3.0f / 1200.0f;
            const float level = 0.55f * (0.3f + 0.7f * vel) * (1.0f + rng.bi() * 0.09f);
            auto add = [&] (const SampledInstrument::Region* r, float gain)
            {
                if (r == nullptr || r->length <= 0 || gain <= 0.001f)
                    return;
                auto& h = heads[(size_t) numHeads++];
                h.region = r;
                h.step = std::exp2 ((midiNote - (float) r->key) / 12.0f + detune) * sampled->sampleRate / sr;
                h.pos = sampled->sustained ? (double) rng.uni() * 0.0015 * sampled->sampleRate : 0.0;
                h.gain = gain * level;
                h.done = false;
            };
            // Equal-power cross-fade between the soft and loud recordings.
            add (layers.a, std::cos (layers.mix * pi * 0.5f));
            add (layers.b, std::sin (layers.mix * pi * 0.5f));

            percussive = ! sampled->sustained;
            // Tone opens up to the full recording.
            toneBase = std::min (sr * 0.45f, 2000.0f * std::exp2 (tone * 3.5f));
            toneFilter.setCutoff (sr, toneBase);
            toneApplied = toneBase;
            env.start (sr, 0.002f + std::max (0.0f, s.attack));
            return;
        }
    }

    switch (model)
    {
        case Instrument::piano:
        case Instrument::marimba:
        case Instrument::glass:
        case Instrument::glockenspiel:
        case Instrument::bells:
        case Instrument::kalimba:
        case Instrument::chimes:
            startModal (s);
            break;

        case Instrument::bowl:
            startModal (s);
            naturalAttack = 0.14f;      // a mallet circling the rim: the tone swells in
            break;

        case Instrument::drops:
            startDrops();
            percussive = false;         // the ripples come after the first drop has gone quiet
            break;

        case Instrument::birdsong:
            startBird();
            naturalAttack = 0.02f;
            percussive = false;
            break;

        case Instrument::rhodes:
        {
            const float bright = 0.3f + 0.7f * tone;
            modIndex = 0.3f + 2.6f * vel * bright;
            modDecay = t60ToMul (0.9f, sr);
            amp = 0.30f * (0.4f + 0.6f * vel);
            ampDecay = t60ToMul (std::clamp (6.0f * std::pow (261.0f / freq, 0.5f), 1.5f, 10.0f), sr);
            tineAmp = freq * 7.0f < sr * 0.45f ? 0.05f * vel * bright : 0.0f;
            tineDecay = t60ToMul (0.12f, sr);
            phase = 0.0f;
            break;
        }

        case Instrument::harp:
        {
            const float bright = std::clamp (0.25f + 0.55f * tone + 0.3f * vel, 0.0f, 1.0f);
            loopBlend = 0.5f - 0.4f * bright;
            const float period = sr / freq;
            const float target = period - loopBlend;
            delayInt = std::clamp ((int) (target - 0.1f), 2, lineMask - 2);
            const float frac = target - (float) delayInt;
            apCoeff = (1.0f - frac) / (1.0f + frac);
            const float t60 = std::clamp (7.0f * std::pow (220.0f / freq, 0.6f), 0.6f, 14.0f);
            loopGain = std::pow (10.0f, -3.0f / (t60 * freq));
            apX1 = apY1 = prevRead = 0.0f;

            // Excite the string with band-limited noise; pluck position shapes the tone.
            std::fill (line.begin(), line.end(), 0.0f);
            OnePole exciteLp;
            exciteLp.setCutoff (sr, 800.0f + 9000.0f * bright);
            const int pluck = std::max (1, (int) ((float) delayInt * 0.17f));
            float mean = 0.0f;
            for (int i = 0; i < delayInt; ++i)
            {
                line[(size_t) i] = exciteLp.lp (rng.bi());
                mean += line[(size_t) i];
            }
            mean /= (float) delayInt;
            for (int i = delayInt - 1; i >= 0; --i)
            {
                const float back = i >= pluck ? line[(size_t) (i - pluck)] : 0.0f;
                line[(size_t) i] = (line[(size_t) i] - back - mean) * 0.6f * (0.35f + 0.65f * vel);
            }
            writePos = delayInt;
            break;
        }

        case Instrument::violin:
            startBowed (s, false);
            naturalAttack = 0.07f;
            percussive = false;
            break;

        case Instrument::cello:
        case Instrument::horn:          // until its recording is loaded, a mellow bowed tone
            startBowed (s, true);
            naturalAttack = model == Instrument::horn ? 0.12f : 0.1f;
            percussive = false;
            break;

        case Instrument::flute:
        case Instrument::clarinet:
            vibRate = 4.8f + rng.uni() * 0.6f;
            vibTarget = 0.12f;
            amp = 0.32f * (0.6f + 0.4f * vel);
            breathFilter.reset();
            breathFilter.setup (sr, freq, 7.0f);
            noiseFilter.reset();
            noiseFilter.setup (sr, 3000.0f, 0.5f);
            strikeNoise = 0.6f * vel;
            strikeDecay = t60ToMul (0.08f, sr);
            naturalAttack = 0.05f;
            percussive = false;
            break;

        case Instrument::choir:
            startChoir (s);
            naturalAttack = 0.18f;
            percussive = false;
            break;

        case Instrument::liveInput:
        case Instrument::count:
            active = false;
            return;
    }

    env.start (sr, naturalAttack + std::max (0.0f, s.attack));
}

void InstrumentVoice::startModal (const SoundLabSettings&)
{
    const float bright = std::clamp (0.2f + 0.55f * tone + 0.35f * velocity, 0.0f, 1.1f);
    float total = 0.0f;
    auto add = [&] (float hz, float a, float t60) { addPartial (hz, a, t60); total += a; };

    strikeFilter.reset();

    if (model == Instrument::piano)
    {
        const float b = std::clamp (0.00012f * std::pow (freq / 130.0f, 1.3f), 0.00004f, 0.004f);
        const float rolloff = 2.4f - 1.4f * (bright / 1.1f);
        const float t0 = std::clamp (11.0f * std::pow (130.0f / freq, 0.55f), 0.8f, 18.0f);
        for (int n = 1; n <= 16; ++n)
        {
            const float fn = (float) n * freq * std::sqrt (1.0f + b * (float) (n * n));
            const float strike = 0.3f + 0.7f * std::abs (std::sin (pi * (float) n * 0.118f));
            const float a = std::pow ((float) n, -rolloff) * strike;
            const float t60 = t0 / (1.0f + 0.02f * (float) (n * n) * std::sqrt (freq / 261.0f));
            add (fn, a, t60);
            if (n <= 5) // detuned unison string: slow beating and a longer aftersound
                add (fn * 1.00035f, a * 0.55f, t60 * 1.7f);
        }
        strikeNoise = 0.05f * velocity * bright;
        strikeDecay = t60ToMul (0.02f, sr);
        strikeFilter.setup (sr, std::min (freq * 5.0f, 7000.0f), 0.7f);
    }
    else if (model == Instrument::marimba)
    {
        const float b = 0.35f + 0.8f * bright;
        const float t = std::clamp (1.6f * std::pow (220.0f / freq, 0.6f), 0.25f, 3.0f);
        add (freq,          1.0f,          t);
        add (freq * 1.002f, 0.35f,         t * 1.6f);   // resonator tube
        add (freq * 3.93f,  0.32f * b,     t * 0.3f);
        add (freq * 9.24f,  0.12f * b * b, t * 0.12f);
        add (freq * 15.6f,  0.04f * b * b, t * 0.06f);
        strikeNoise = 0.10f * velocity * b;
        strikeDecay = t60ToMul (0.012f, sr);
        strikeFilter.setup (sr, std::min (freq * 3.0f, 6000.0f), 0.8f);
    }
    else if (model == Instrument::kalimba)
    {
        // A steel tine on a wooden box: a bright pluck, a pure ring and a woody knock.
        const float t = std::clamp (2.4f * std::pow (261.0f / freq, 0.45f), 0.5f, 4.5f);
        add (freq,          1.0f,                 t);
        add (freq * 1.0015f, 0.3f,                t * 1.4f);
        add (freq * 5.4f,   0.3f * bright,        t * 0.16f);
        add (freq * 13.1f,  0.08f * bright * bright, t * 0.05f);
        strikeNoise = 0.08f * velocity * bright;
        strikeDecay = t60ToMul (0.02f, sr);
        strikeFilter.setup (sr, std::clamp (freq * 2.0f, 500.0f, 4500.0f), 0.9f);
    }
    else if (model == Instrument::glockenspiel)
    {
        const float ratios[] = { 1.0f, 2.76f, 5.4f };
        const float amps[]   = { 1.0f, 0.4f, 0.16f };
        const float t = std::clamp (4.0f * std::pow (1046.0f / freq, 0.3f), 1.0f, 7.0f);
        for (int i = 0; i < 3; ++i)
            add (freq * ratios[i], amps[i] * std::pow (std::max (bright, 0.05f), (float) i * 0.6f), t / (1.0f + 1.5f * (float) i));
        strikeNoise = 0.06f * velocity * bright;
        strikeDecay = t60ToMul (0.005f, sr);
        strikeFilter.setup (sr, std::min (freq * 3.0f, 9000.0f), 1.0f);
    }
    else if (model == Instrument::bells)
    {
        // A tubular bell: its partials stand near 2 : 3 : 4, ringing for a long time.
        const float ratios[] = { 1.0f, 2.0f, 2.99f, 4.13f, 5.63f };
        const float amps[]   = { 1.0f, 0.55f, 0.5f, 0.3f, 0.18f };
        const float t = std::clamp (10.0f * std::pow (262.0f / freq, 0.3f), 2.0f, 14.0f);
        for (int i = 0; i < 5; ++i)
        {
            const float a = amps[i] * std::pow (std::max (bright, 0.05f), (float) i * 0.4f);
            add (freq * ratios[i], a, t / (1.0f + 0.5f * (float) i));
            add (freq * ratios[i] * 1.0008f, a * 0.5f, t / (1.0f + 0.4f * (float) i));
        }
        strikeNoise = 0.04f * velocity;
        strikeDecay = t60ToMul (0.01f, sr);
        strikeFilter.setup (sr, std::min (freq * 4.0f, 8000.0f), 1.2f);
    }
    else if (model == Instrument::chimes)
    {
        // Metal tubes in the wind: the struck one, and a couple of its neighbours knocked
        // by the same gust a moment later.
        const float ratios[] = { 1.0f, 2.32f, 4.25f, 6.63f };
        const float amps[]   = { 1.0f, 0.55f, 0.3f, 0.15f };
        const float decays[] = { 1.0f, 0.65f, 0.45f, 0.3f };
        const float t = std::clamp (7.5f * std::pow (523.0f / freq, 0.3f), 2.0f, 12.0f);
        for (int i = 0; i < 4; ++i)
        {
            const float a = amps[i] * std::pow (std::max (bright, 0.05f), (float) i * 0.5f);
            add (freq * ratios[i], a, t * decays[i]);
            add (freq * ratios[i] * 1.0009f, a * 0.6f, t * decays[i] * 1.25f);
        }
        const float steps[] = { 2.0f, 4.0f, 7.0f, 9.0f, 12.0f, -3.0f, -5.0f };
        for (int g = 0; g < 2; ++g)
        {
            const float neighbour = freq * semitonesToRatio (steps[(size_t) (rng.uni() * 6.99f)]);
            const int delay = (int) ((0.12f + 0.5f * rng.uni() + 0.35f * (float) g) * sr);
            const float a = (0.34f - 0.1f * (float) g) * (0.6f + 0.4f * bright);
            addPartial (neighbour, a, t * 0.7f, delay);
            addPartial (neighbour * ratios[1], a * 0.4f, t * 0.35f, delay);
            addPartial (neighbour * ratios[2], a * 0.15f, t * 0.2f, delay);
        }
        strikeNoise = 0.03f * velocity;
        strikeDecay = t60ToMul (0.004f, sr);
        strikeFilter.setup (sr, std::min (freq * 5.0f, 9000.0f), 1.0f);
    }
    else if (model == Instrument::bowl)
    {
        // A bronze bowl: partials near 1 : 2.7 : 5.1 : 8.3, each a pair that slowly beats.
        const float ratios[] = { 1.0f, 2.71f, 5.08f, 8.3f };
        const float amps[]   = { 1.0f, 0.5f, 0.22f, 0.1f };
        const float t = std::clamp (11.0f * std::pow (220.0f / freq, 0.25f), 4.0f, 16.0f);
        for (int i = 0; i < 4; ++i)
        {
            const float a = amps[i] * std::pow (std::max (bright, 0.05f), (float) i * 0.45f);
            const float f = freq * ratios[i];
            add (f, a, t / (1.0f + 0.6f * (float) i));
            add (f + (0.35f + 0.9f * rng.uni()) * (1.0f + (float) i), a * 0.9f, t / (1.0f + 0.5f * (float) i));
        }
        strikeNoise = 0.0f;
    }
    else // glass
    {
        const float ratios[] = { 1.0f, 2.756f, 5.404f, 8.933f, 13.34f };
        const float amps[]   = { 1.0f, 0.55f, 0.32f, 0.16f, 0.08f };
        const float decays[] = { 1.0f, 0.7f, 0.45f, 0.3f, 0.2f };
        const float t = std::clamp (7.0f * std::pow (440.0f / freq, 0.35f), 1.5f, 12.0f);
        for (int i = 0; i < 5; ++i)
        {
            const float a = amps[i] * std::pow (std::max (bright, 0.05f), (float) i * 0.5f);
            add (freq * ratios[i], a, t * decays[i]);
            add (freq * ratios[i] * 1.0012f, a * 0.7f, t * decays[i] * 1.2f);
        }
        strikeNoise = 0.02f * velocity;
        strikeDecay = t60ToMul (0.006f, sr);
        strikeFilter.setup (sr, std::min (freq * 6.0f, 9000.0f), 1.0f);
    }

    // Normalise so every note has a similar peak level; velocity sets the final loudness.
    const float scale = (0.55f / std::max (total, 0.001f)) * (0.3f + 0.7f * velocity);
    for (int i = 0; i < numPartials; ++i)
        partials[(size_t) i].amp *= scale;
}

void InstrumentVoice::startBowed (const SoundLabSettings&, bool isCello)
{
    // Body resonances: (Hz, Q, gain)
    static const float violinBody[6][3] = { { 280, 8, 1.0f }, { 460, 7, 0.9f }, { 1000, 4, 0.5f }, { 2400, 2, 0.9f }, { 3400, 3, 0.45f }, { 5200, 2, 0.2f } };
    static const float celloBody[6][3]  = { { 180, 6, 1.0f }, { 270, 6, 0.8f }, { 480, 5, 0.6f }, { 900, 4, 0.4f }, { 1600, 3, 0.45f }, { 2600, 3, 0.25f } };
    const auto& body = isCello ? celloBody : violinBody;

    numFormants = 6;
    for (int i = 0; i < numFormants; ++i)
    {
        formants[(size_t) i].reset();
        formants[(size_t) i].setup (sr, body[i][0], body[i][1]);
        formantGains[(size_t) i] = body[i][2];
    }
    directGain = 0.15f;
    noiseFilter.reset();
    noiseFilter.setup (sr, isCello ? 1800.0f : 2800.0f, 0.8f);
    sourceFilter.reset();
    sourceFilter.setCutoff (sr, 1200.0f * std::exp2 (tone * 3.5f));
    vibRate = (isCello ? 4.9f : 5.6f) + rng.uni() * 0.5f;
    vibTarget = isCello ? 0.12f : 0.16f;
    amp = (isCello ? 0.45f : 0.33f) * (0.55f + 0.45f * velocity);
}

void InstrumentVoice::startChoir (const SoundLabSettings&)
{
    const float pos = std::clamp (tone, 0.0f, 1.0f) * 3.0f;
    const int i0 = std::min ((int) pos, 2);
    const float t = pos - (float) i0;
    const auto& a = vowels[i0];
    const auto& b = vowels[i0 + 1];

    numFormants = 5;
    for (int i = 0; i < numFormants; ++i)
    {
        const float f = a.f[i] + (b.f[i] - a.f[i]) * t;
        const float bw = a.bw[i] + (b.bw[i] - a.bw[i]) * t;
        const float g = std::pow (10.0f, (a.gainDb[i] + (b.gainDb[i] - a.gainDb[i]) * t) / 20.0f);
        formants[(size_t) i].reset();
        formants[(size_t) i].setup (sr, f, f / bw);
        formantGains[(size_t) i] = g;
    }
    directGain = 0.0f;
    sourceFilter.reset();
    sourceFilter.setCutoff (sr, 4000.0f);
    noiseFilter.reset();
    noiseFilter.setup (sr, 2500.0f, 0.6f);

    const float detunes[] = { -7.0f, 0.5f, 6.0f };
    for (size_t i = 0; i < 3; ++i)
    {
        choirPhases[i] = rng.uni();
        choirDetune[i] = (detunes[i] + rng.bi() * 2.0f) / 100.0f;
        choirVibPhase[i] = rng.uni();
    }
    vibRate = 5.0f;
    vibTarget = 0.1f;
    amp = 1.3f * (0.6f + 0.4f * velocity);
}

void InstrumentVoice::startDrops()
{
    // A bubble's pitch climbs as it shrinks, then the ripples it sends out follow.
    const float ratios[] = { 1.0f, 1.5f, 1.25f, 2.0f, 0.75f };
    const float level = 0.5f * (0.3f + 0.7f * velocity);
    const float t60 = std::clamp (0.2f * std::pow (261.0f / freq, 0.4f), 0.06f, 0.4f);
    for (size_t i = 0; i < droplets.size(); ++i)
    {
        auto& d = droplets[i];
        d = {};
        d.delay = i == 0 ? 0 : (int) ((0.09f + 0.17f * (float) i + 0.12f * rng.uni()) * sr);
        d.hz = freq * (i == 0 ? 1.0f : ratios[(size_t) (rng.uni() * 4.99f)]) / 1.25f;
        d.amp = level * (i == 0 ? 1.0f : 0.5f - 0.1f * (float) i);
        d.decay = t60ToMul (t60 * (i == 0 ? 1.0f : 0.7f), sr);
        d.phase = rng.uni();
    }
}

void InstrumentVoice::startBird()
{
    // A bird sings high: fold the note up into a whistler's range, keeping its pitch class.
    float home = freq;
    while (home < 900.0f)  home *= 2.0f;
    while (home > 3800.0f) home *= 0.5f;
    bird = {};
    bird.home = home;
    bird.at = bird.length = 1;
    amp = 0.2f * (0.55f + 0.45f * velocity);
}

void InstrumentVoice::nextChirp()
{
    const float ratios[] = { 0.84f, 0.94f, 1.0f, 1.12f, 1.26f, 1.5f, 1.68f };
    auto pick = [this, &ratios] { return ratios[(size_t) (rng.uni() * 6.99f)]; };
    const bool fast = bird.trill > 0.0f;
    bird.length = (int) ((fast ? 0.022f + 0.02f * rng.uni() : 0.05f + 0.09f * rng.uni()) * sr);
    bird.at = 0;
    bird.f0 = bird.home * pick();
    bird.f1 = bird.home * pick();
}

void InstrumentVoice::release()
{
    keyDown = false;
    sustained = false;
    if (env.stage != Envelope::Stage::idle && env.stage != Envelope::Stage::release)
        env.startRelease (sr, releaseSec);
}

//==============================================================================
float InstrumentVoice::renderModal()
{
    float out = 0.0f;
    for (int i = 0; i < numPartials; ++i)
    {
        auto& p = partials[(size_t) i];
        if (p.delay > 0)   // not struck yet
        {
            --p.delay;
            continue;
        }
        out += p.im * p.amp;
        const float re = p.re * p.cr - p.im * p.ci;
        p.im = p.re * p.ci + p.im * p.cr;
        p.re = re;
        p.amp *= p.mul;
    }

    if (strikeNoise > 1.0e-5f)
    {
        out += strikeFilter.bp (rng.bi()) * strikeNoise;
        strikeNoise *= strikeDecay;
    }
    return out;
}

float InstrumentVoice::renderRhodes()
{
    const float carrier = std::sin (twoPi * phase + modIndex * std::sin (twoPi * phase));
    float out = carrier * amp;
    if (tineAmp > 1.0e-5f)
    {
        out += std::sin (twoPi * phase2) * tineAmp;
        tineAmp *= tineDecay;
        phase2 += freq * 7.0f / sr;
        phase2 -= std::floor (phase2);
    }
    phase += freq / sr;
    phase -= std::floor (phase);
    modIndex *= modDecay;
    amp *= ampDecay;
    return out;
}

float InstrumentVoice::renderHarp()
{
    const float read = line[(size_t) ((writePos - delayInt) & lineMask)];
    const float damped = (1.0f - loopBlend) * read + loopBlend * prevRead;
    prevRead = read;
    const float y = apCoeff * damped + apX1 - apCoeff * apY1;
    apX1 = damped;
    apY1 = y;
    const float out = y * loopGain;
    line[(size_t) writePos] = out;
    writePos = (writePos + 1) & lineMask;
    return out;
}

float InstrumentVoice::renderBowed (float hz)
{
    vibDepth += (vibTarget - vibDepth) * (2.0f / sr);
    vibPhase += vibRate / sr;
    vibPhase -= std::floor (vibPhase);
    const float cents = vibDepth * std::sin (twoPi * vibPhase) + drift.advance (1) * 0.04f;
    const float dt = hz * semitonesToRatio (cents) / sr;

    phase += dt;
    phase -= std::floor (phase);
    float src = 2.0f * phase - 1.0f - polyBlep (phase, dt);
    src = sourceFilter.lp (src);

    // Bow noise, louder at the stick-slip moment of each period.
    const float noise = noiseFilter.bp (rng.bi()) * noiseFilter.k * (phase < 0.25f ? 0.35f : 0.12f) * (0.4f + 0.6f * tone);
    const float excitation = src + noise;

    float out = excitation * directGain;
    for (int i = 0; i < numFormants; ++i)
    {
        auto& f = formants[(size_t) i];
        out += f.bp (excitation) * f.k * formantGains[(size_t) i];
    }
    return out * amp;
}

float InstrumentVoice::renderFlute (float hz)
{
    vibDepth += (vibTarget - vibDepth) * (1.5f / sr);
    vibPhase += vibRate / sr;
    vibPhase -= std::floor (vibPhase);
    const float vib = std::sin (twoPi * vibPhase);
    phase += hz * semitonesToRatio (vibDepth * vib) / sr;
    phase -= std::floor (phase);

    const float bright = 0.3f + 0.7f * tone;
    const float body = std::sin (twoPi * phase)
                     + bright * (0.22f * std::sin (2.0f * twoPi * phase) + 0.08f * std::sin (3.0f * twoPi * phase));
    const float n = rng.bi();
    float breath = breathFilter.bp (n) * breathFilter.k * 0.25f + noiseFilter.bp (n) * noiseFilter.k * 0.05f * bright;
    if (strikeNoise > 1.0e-4f)
    {
        breath += noiseFilter.band * noiseFilter.k * strikeNoise * 0.3f;
        strikeNoise *= strikeDecay;
    }
    return (body * 0.6f + breath) * (1.0f + 0.03f * vib) * amp;
}

float InstrumentVoice::renderDrops()
{
    float out = 0.0f;
    const float liftMul = std::exp (-38.0f / sr);
    for (auto& d : droplets)
    {
        if (d.delay > 0)
        {
            --d.delay;
            continue;
        }
        if (d.amp < 1.0e-6f)
            continue;
        d.lift *= liftMul;
        d.phase += d.hz * (1.0f + 0.55f * (1.0f - d.lift)) / sr;
        d.phase -= std::floor (d.phase);
        d.age += 1.0f / sr;
        const float attack = std::min (1.0f, d.age / 0.002f);
        out += (std::sin (twoPi * d.phase) + 0.12f * std::sin (2.0f * twoPi * d.phase)) * d.amp * attack;
        d.amp *= d.decay;
    }
    return out;
}

float InstrumentVoice::renderBird()
{
    if (bird.gapLeft > 0)
    {
        --bird.gapLeft;
        return 0.0f;
    }
    if (bird.at >= bird.length)   // between chirps: carry on with the phrase, or rest, then begin a new one
    {
        if (bird.chirpsLeft <= 0)
        {
            if (bird.rest > 0)
            {
                --bird.rest;
                return 0.0f;
            }
            bird.trill = rng.chance (0.35f) ? 0.03f : 0.0f;
            bird.chirpsLeft = bird.trill > 0.0f ? 7 + (int) (rng.uni() * 7.0f) : 3 + (int) (rng.uni() * 4.0f);
        }
        nextChirp();
    }

    const float t = (float) bird.at / (float) bird.length;
    const float glide = t * t * (3.0f - 2.0f * t);
    bird.trillPhase += 38.0f / sr;
    bird.trillPhase -= std::floor (bird.trillPhase);
    const float hz = (bird.f0 + (bird.f1 - bird.f0) * glide) * (1.0f + bird.trill * std::sin (twoPi * bird.trillPhase));
    phase += hz / sr;
    phase -= std::floor (phase);
    const float swell = std::sin (pi * t);
    const float out = (std::sin (twoPi * phase) + 0.2f * std::sin (2.0f * twoPi * phase)) * swell * swell * amp;

    if (++bird.at >= bird.length)
    {
        --bird.chirpsLeft;
        bird.gapLeft = (int) ((bird.trill > 0.0f ? 0.015f + 0.02f * rng.uni() : 0.03f + 0.08f * rng.uni()) * sr);
        if (bird.chirpsLeft <= 0)
            bird.rest = (int) ((0.6f + 1.8f * rng.uni()) * sr);
    }
    return out;
}

float InstrumentVoice::renderChoir (float hz)
{
    vibDepth += (vibTarget - vibDepth) * (1.0f / sr);
    float src = 0.0f;
    for (size_t v = 0; v < 3; ++v)
    {
        choirVibPhase[v] += (vibRate + (float) v * 0.37f) / sr;
        choirVibPhase[v] -= std::floor (choirVibPhase[v]);
        const float st = choirDetune[v] + vibDepth * std::sin (twoPi * choirVibPhase[v]);
        const float dt = hz * semitonesToRatio (st) / sr;
        auto& ph = choirPhases[v];
        ph += dt;
        ph -= std::floor (ph);
        src += 2.0f * ph - 1.0f - polyBlep (ph, dt);
    }
    src = sourceFilter.lp (src * 0.4f) + noiseFilter.bp (rng.bi()) * 0.04f;

    float out = 0.0f;
    for (int i = 0; i < numFormants; ++i)
    {
        auto& f = formants[(size_t) i];
        out += f.bp (src) * f.k * formantGains[(size_t) i];
    }
    return out * amp;
}

void InstrumentVoice::renderSampled (float* left, float* right, int numSamples, float bendRatio, float& peak)
{
    constexpr float scale = 1.0f / 32767.0f;
    const float swell = 1.0f + 0.6f * pressureNow;   // pressing harder swells the note

    for (int i = 0; i < numSamples; ++i)
    {
        float l = 0.0f, rr = 0.0f;
        bool stereo = false, sounding = false;
        for (int k = 0; k < numHeads; ++k)
        {
            auto& h = heads[(size_t) k];
            if (h.done)
                continue;
            const auto& r = *static_cast<const SampledInstrument::Region*> (h.region);
            const bool loops = r.loopEnd > r.loopStart && r.loopStart >= 0;
            const double end = loops ? (double) r.loopEnd : (double) (r.length - 2);
            if (h.pos >= end)
            {
                if (! loops) { h.done = true; continue; }
                h.pos -= (double) (r.loopEnd - r.loopStart);
            }
            const int ch = r.channels;
            const int16_t* data = r.data.data();
            const int i0 = (int) h.pos;
            const float t = (float) (h.pos - (double) i0);
            const int i1 = loops && i0 + 1 >= r.loopEnd ? r.loopStart : std::min (i0 + 1, r.length - 1);
            const float a0 = (float) data[(size_t) (i0 * ch)] * scale, a1 = (float) data[(size_t) (i1 * ch)] * scale;
            const float sl = a0 + (a1 - a0) * t;
            float sr2 = sl;
            if (ch > 1)
            {
                const float b0 = (float) data[(size_t) (i0 * ch + 1)] * scale, b1 = (float) data[(size_t) (i1 * ch + 1)] * scale;
                sr2 = b0 + (b1 - b0) * t;
                stereo = true;
            }
            l += sl * h.gain;
            rr += sr2 * h.gain;
            h.pos += h.step * bendRatio;
            sounding = true;
        }
        if (! sounding)
        {
            env.stage = Envelope::Stage::idle;
            break;
        }

        const float e = env.next() * swell;
        // One tone filter on the mid signal keeps stereo recordings' width.
        const float mid = 0.5f * (l + rr);
        const float side = 0.5f * (l - rr);
        const float m = toneFilter.lp (mid) * e;
        if (stereo)
        {
            left[i] += m + side * e * 0.8f;
            right[i] += m - side * e * 0.8f;
        }
        else
        {
            left[i] += m * gainL;
            right[i] += m * gainR;
        }
        peak = std::max (peak, std::abs (m));
        if (env.stage == Envelope::Stage::idle)
            break;
    }
}

void InstrumentVoice::render (float* left, float* right, int numSamples, float pitchBendSemis)
{
    if (! active)
        return;

    const float hz = freq * semitonesToRatio (pitchBendSemis);
    float peak = 0.0f;

    // Key pressure: glide towards it, opening the tone filter as it grows.
    pressureNow += (pressure - pressureNow) * std::min (1.0f, (float) numSamples / (0.04f * sr));
    const float toneTarget = std::min (sr * 0.45f, toneBase * std::exp2 (2.2f * pressureNow));
    if (std::abs (toneTarget - toneApplied) > toneApplied * 0.01f)
    {
        toneApplied = toneTarget;
        toneFilter.setCutoff (sr, toneApplied);
    }

    if (numHeads > 0)
    {
        renderSampled (left, right, numSamples, semitonesToRatio (pitchBendSemis), peak);
        samplesPlayed += numSamples;
        const bool decayedAway = percussive && samplesPlayed > (int) (sr * 0.05f) && peak < 3.0e-5f;
        if (env.stage == Envelope::Stage::idle || decayedAway)
            active = false;
        return;
    }

    auto run = [&] (auto&& generate)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float e = env.next() * (1.0f + 0.6f * pressureNow);
            const float s = toneFilter.lp (generate()) * e;
            left[i] += s * gainL;
            right[i] += s * gainR;
            peak = std::max (peak, std::abs (s));
            if (env.stage == Envelope::Stage::idle)
                break;
        }
    };

    switch (model)
    {
        case Instrument::piano:
        case Instrument::marimba:
        case Instrument::glass:
        case Instrument::glockenspiel:
        case Instrument::bells:
        case Instrument::kalimba:
        case Instrument::chimes:
        case Instrument::bowl:    run ([this] { return renderModal(); }); break;
        case Instrument::rhodes:  run ([this] { return renderRhodes(); }); break;
        case Instrument::harp:    run ([this] { return renderHarp(); }); break;
        case Instrument::violin:
        case Instrument::horn:
        case Instrument::cello:   run ([this, hz] { return renderBowed (hz); }); break;
        case Instrument::flute:
        case Instrument::clarinet: run ([this, hz] { return renderFlute (hz); }); break;
        case Instrument::choir:   run ([this, hz] { return renderChoir (hz); }); break;
        case Instrument::drops:   run ([this] { return renderDrops(); }); break;
        case Instrument::birdsong: run ([this] { return renderBird(); }); break;
        case Instrument::liveInput:
        case Instrument::count:   break;
    }

    // Keep the rotating-phasor oscillators on the unit circle.
    for (int i = 0; i < numPartials; ++i)
    {
        auto& p = partials[(size_t) i];
        const float mag = std::sqrt (p.re * p.re + p.im * p.im);
        if (mag > 0.0f) { p.re /= mag; p.im /= mag; }
    }

    samplesPlayed += numSamples;
    const bool decayedAway = percussive && samplesPlayed > (int) (sr * 0.05f) && peak < 3.0e-5f;
    if (env.stage == Envelope::Stage::idle || decayedAway)
        active = false;
}

//==============================================================================
void SoundLab::prepare (double sampleRate)
{
    sr = (float) sampleRate;
    for (auto& v : voices)
        v.prepare (sr);
    reset();
}

void SoundLab::reset()
{
    for (auto& v : voices)
        v.kill();
    numPending = 0;
    sustainPedal = false;
    pitchBend = 0.0f;
}

int SoundLab::activeVoiceCount() const
{
    int n = 0;
    for (auto& v : voices)
        n += v.active ? 1 : 0;
    return n;
}

void SoundLab::process (const juce::MidiBuffer& midi, float* left, float* right, int numSamples)
{
    std::fill (left, left + numSamples, 0.0f);
    std::fill (right, right + numSamples, 0.0f);

    constexpr int subBlock = 32;
    auto it = midi.cbegin();

    for (int pos = 0; pos < numSamples; pos += subBlock)
    {
        const int len = std::min (subBlock, numSamples - pos);

        for (; it != midi.cend() && (*it).samplePosition < pos + len; ++it)
            handleMidi ((*it).getMessage());

        // Strummed chord notes waiting for their moment.
        for (int i = 0; i < numPending;)
        {
            auto& p = pending[(size_t) i];
            p.samplesLeft -= len;
            if (p.samplesLeft <= 0)
            {
                startVoice (p.key, p.note, p.velocity);
                pending[(size_t) i] = pending[(size_t) --numPending];
            }
            else
            {
                ++i;
            }
        }

        for (auto& v : voices)
            v.render (left + pos, right + pos, len, pitchBend);
    }
}

void SoundLab::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn())
    {
        noteOn (m.getNoteNumber(), m.getFloatVelocity());
    }
    else if (m.isNoteOff())
    {
        noteOff (m.getNoteNumber());
    }
    else if (m.isSustainPedalOn())
    {
        sustainPedal = true;
    }
    else if (m.isSustainPedalOff())
    {
        sustainPedal = false;
        for (auto& v : voices)
            if (v.active && v.sustained)
                releaseVoice (v);
    }
    else if (m.isAftertouch())
    {
        setPressureForKey (m.getNoteNumber(), (float) m.getAfterTouchValue() / 127.0f);
    }
    else if (m.isChannelPressure())
    {
        const float p = (float) m.getChannelPressureValue() / 127.0f;
        for (auto& v : voices)
            if (v.active && v.keyDown)
                v.setPressure (p);
    }
    else if (m.isPitchWheel())
    {
        pitchBend = 2.0f * ((float) m.getPitchWheelValue() - 8192.0f) / 8192.0f;
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        numPending = 0;
        for (auto& v : voices)
        {
            if (v.active && (v.keyDown || v.sustained))
                pushNoteEvent (v.getNote(), 0.0f, false);
            if (m.isAllSoundOff())
                v.kill();
            else if (v.active)
                v.release();
        }
    }
}

void SoundLab::noteOn (int key, float velocity)
{
    if (settings.instrument == Instrument::liveInput)
        return;

    const auto& shapes = chordShapes();
    const auto& shape = shapes[(size_t) std::clamp (settings.chord, 0, (int) shapes.size() - 1)].intervals;

    std::array<int, 8> notes {};
    const int count = std::min ((int) shape.size(), (int) notes.size());
    for (int i = 0; i < count; ++i)
    {
        int interval = shape[(size_t) i];
        if (settings.voicing == 1 && i == 1 && count >= 3)
            interval += 12;                       // open: lift the third
        else if (settings.voicing == 2 && (i % 2) == 1)
            interval += 12;                       // wide: every other tone up an octave
        notes[(size_t) i] = interval;
    }
    std::sort (notes.begin(), notes.begin() + count);

    const float base = (float) (key + 12 * settings.octave);
    const int strumSamples = (int) (settings.strumMs * 0.001f * sr);

    for (int i = 0; i < count; ++i)
    {
        const float n = base + (float) notes[(size_t) i];
        if (n < 12.0f || n > 120.0f)
            continue;

        const float vel = velocity * (i == 0 ? 1.0f : 0.82f);
        const int delay = i * strumSamples + (strumSamples > 0 ? (int) (rng.uni() * 0.004f * sr) : 0);

        if (delay <= 0)
            startVoice (key, n, vel);
        else if (numPending < (int) pending.size())
            pending[(size_t) numPending++] = { key, n, vel, delay };
    }
}

void SoundLab::noteOff (int key)
{
    for (int i = 0; i < numPending;)
    {
        if (pending[(size_t) i].key == key)
            pending[(size_t) i] = pending[(size_t) --numPending];
        else
            ++i;
    }
    releaseVoicesForKey (key);
}

void SoundLab::releaseVoicesForKey (int key)
{
    for (auto& v : voices)
    {
        if (v.active && v.keyDown && v.sourceKey == key)
        {
            if (sustainPedal)
            {
                v.keyDown = false;
                v.sustained = true;
            }
            else
            {
                releaseVoice (v);
            }
        }
    }
}

void SoundLab::setPressureForKey (int key, float p)
{
    for (auto& v : voices)
        if (v.active && v.keyDown && v.sourceKey == key)
            v.setPressure (p);
}

float SoundLab::maxPressure() const
{
    float p = 0.0f;
    for (auto& v : voices)
        if (v.active)
            p = std::max (p, v.getPressure());
    return p;
}

SoundLab::HeldRange SoundLab::heldRange() const
{
    HeldRange r;
    for (auto& v : voices)
    {
        if (! v.active || ! (v.keyDown || v.sustained))
            continue;
        const float n = v.getNote();
        r.lowest = r.any ? std::min (r.lowest, n) : n;
        r.highest = r.any ? std::max (r.highest, n) : n;
        r.any = true;
    }
    return r;
}

void SoundLab::releaseVoice (InstrumentVoice& v)
{
    pushNoteEvent (v.getNote(), 0.0f, false);
    v.release();
}

void SoundLab::pushNoteEvent (float note, float velocity, bool on)
{
    const auto scope = noteFifo.write (1);
    if (scope.blockSize1 > 0)
        noteEvents[(size_t) scope.startIndex1] = { note, velocity, settings.chord, (int) settings.instrument, on };
}

int SoundLab::popNoteEvents (NoteEvent* dest, int maxEvents)
{
    const auto scope = noteFifo.read (std::min (maxEvents, noteFifo.getNumReady()));
    int n = 0;
    for (int i = 0; i < scope.blockSize1; ++i) dest[n++] = noteEvents[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[n++] = noteEvents[(size_t) (scope.startIndex2 + i)];
    return n;
}

void SoundLab::startVoice (int key, float note, float velocity)
{
    InstrumentVoice* chosen = nullptr;

    for (auto& v : voices)
        if (! v.active) { chosen = &v; break; }

    // Steal the oldest released voice, then the oldest voice of all.
    if (chosen == nullptr)
        for (auto& v : voices)
            if (! v.keyDown && (chosen == nullptr || v.startedAt < chosen->startedAt))
                chosen = &v;

    if (chosen == nullptr)
        for (auto& v : voices)
            if (chosen == nullptr || v.startedAt < chosen->startedAt)
                chosen = &v;

    // A stolen voice that was still held counts as let go.
    if (chosen->active && (chosen->keyDown || chosen->sustained))
        pushNoteEvent (chosen->getNote(), 0.0f, false);
    pushNoteEvent (note, velocity, true);

    seedCounter = seedCounter * 1664525u + 1013904223u;
    chosen->start (settings.instrument, note, velocity, settings, seedCounter, samples);
    chosen->sourceKey = key;
    chosen->startedAt = ++voiceCounter;
}
} // namespace bouquet
