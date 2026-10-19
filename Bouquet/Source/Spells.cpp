#include "Spells.h"

namespace bouquet
{
namespace
{
    // Harmony set indices (see harmonySets()).
    enum { unison, octaves, fifths, major, minor, sus, maj7, min9, lydian, pentatonic, wholeTone, shimmer, sub, cluster };
    enum { harmonyCat, timeCat, textureCat, spaceCat, motionCat };
}

juce::StringArray spellCategories()
{
    return { "harmony", "time", "texture", "space", "motion" };
}

const std::vector<Spell>& spellBook()
{
    //                    density size   time  spray pitch harmony     amt   detune rev   chaos shape width fb    colour space decay life  freeze mix
    static const std::vector<Spell> book {
        { "chord smear",      harmonyCat, "every note melts into a slow, warm chord",
                            { 14, 420,  0.35f, 0.25f, 0, major,      0.85f, 8,  0.1f, 0.6f, 0.0f, 0.8f, 0.15f, -0.1f, 0.45f, 6,  0.35f, false, 0.7f } },
        { "minor sigh",       harmonyCat, "a darker exhale, three notes falling into one",
                            { 10, 520,  0.5f,  0.3f,  0, minor,      0.8f,  10, 0.2f, 0.6f, 0.0f, 0.8f, 0.2f, -0.3f,  0.5f,  7,  0.4f,  false, 0.7f } },
        { "fifths aurora",    harmonyCat, "open fifths rising like northern light",
                            { 16, 360,  0.3f,  0.2f,  0, fifths,     0.9f,  6,  0.0f, 0.5f, 0.0f, 0.9f, 0.3f,  0.15f, 0.55f, 9,  0.35f, false, 0.7f } },
        { "lydian glow",      harmonyCat, "bright, floating, a little unreal",
                            { 12, 480,  0.4f,  0.25f, 0, lydian,     0.75f, 9,  0.1f, 0.6f, 0.0f, 0.85f, 0.25f, 0.25f, 0.55f, 8, 0.4f,  false, 0.7f } },
        { "octave ghosts",    harmonyCat, "voices an octave away, barely there",
                            { 9,  300,  0.25f, 0.15f, 0, octaves,    0.7f,  5,  0.15f, 0.5f, 0.0f, 0.7f, 0.2f, 0.0f,  0.4f,  5,  0.3f,  false, 0.6f } },
        { "pentatonic rain",  harmonyCat, "falling notes that always belong",
                            { 28, 120,  0.6f,  0.5f,  0, pentatonic, 1.0f,  4,  0.0f, 0.9f, 0.3f, 1.0f, 0.0f,  0.2f,  0.4f,  4,  0.45f, false, 0.65f } },
        { "nine moons",       harmonyCat, "a minor ninth, suspended in slow orbit",
                            { 8,  700,  0.6f,  0.35f, 0, min9,       0.85f, 12, 0.25f, 0.5f, 0.0f, 0.9f, 0.3f, -0.2f, 0.6f,  12, 0.5f,  false, 0.75f } },
        { "suspended",        harmonyCat, "never resolving, always about to",
                            { 12, 450,  0.4f,  0.25f, 0, sus,        0.8f,  7,  0.1f, 0.55f, 0.0f, 0.8f, 0.2f, 0.05f, 0.5f,  7,  0.35f, false, 0.7f } },

        { "frozen breath",    timeCat, "the moment held still, breathing",
                            { 20, 380,  0.4f,  0.35f, 0, unison,     0.0f,  10, 0.3f, 0.7f, 0.0f, 0.9f, 0.0f,  0.0f,  0.5f,  8,  0.55f, true,  0.85f } },
        { "reverse bloom",    timeCat, "sound flowers backwards into itself",
                            { 8,  650,  0.7f,  0.3f,  0, unison,     0.0f,  6,  1.0f, 0.4f, 0.0f, 0.7f, 0.15f, -0.05f, 0.45f, 6, 0.3f,  false, 0.7f } },
        { "tape memory",      timeCat, "a worn cassette remembering you",
                            { 6,  900,  1.2f,  0.4f,  0, unison,     0.0f,  22, 0.0f, 0.3f, 0.0f, 0.4f, 0.25f, -0.45f, 0.3f, 3,  0.7f,  false, 0.65f } },
        { "time smear",       timeCat, "seconds dissolve into a single hush",
                            { 18, 1200, 1.5f,  1.0f,  0, unison,     0.0f,  8,  0.3f, 0.8f, 0.0f, 0.9f, 0.3f, -0.1f,  0.6f,  10, 0.4f,  false, 0.75f } },
        { "stutter dust",     timeCat, "tiny echoes caught in a skipping groove",
                            { 22, 45,   0.12f, 0.02f, 0, unison,     0.0f,  0,  0.0f, 0.0f, 0.9f, 0.5f, 0.1f,  0.1f,  0.2f,  2,  0.1f,  false, 0.6f } },
        { "echo garden",      timeCat, "memories planted at different depths",
                            { 5,  260,  1.8f,  1.0f,  0, unison,     0.0f,  4,  0.1f, 0.6f, 0.2f, 0.9f, 0.45f, 0.0f,  0.4f,  5,  0.35f, false, 0.6f } },
        { "slow motion",      timeCat, "everything half as fast and twice as soft",
                            { 7,  800,  0.8f,  0.2f, -12, unison,    0.0f,  5,  0.0f, 0.4f, 0.0f, 0.7f, 0.1f, -0.3f,  0.45f, 6,  0.3f,  false, 0.7f } },
        { "infinite sustain", timeCat, "a note that refuses to end",
                            { 30, 600,  0.15f, 0.1f,  0, unison,     0.0f,  7,  0.0f, 0.7f, 0.0f, 0.7f, 0.6f,  0.0f,  0.5f,  14, 0.3f,  false, 0.8f } },

        { "glass rain",       textureCat, "small bright droplets on a cold window",
                            { 40, 40,   0.5f,  0.6f, 12, octaves,    0.4f,  6,  0.1f, 1.0f, 0.4f, 1.0f, 0.1f,  0.5f,  0.45f, 4,  0.3f,  false, 0.6f } },
        { "pollen",           textureCat, "a warm cloud of microscopic light",
                            { 70, 25,   0.3f,  0.5f,  0, major,      0.5f,  15, 0.3f, 1.0f, 0.0f, 1.0f, 0.05f, 0.3f,  0.5f,  5,  0.4f,  false, 0.65f } },
        { "murmuration",      textureCat, "a thousand birds turning as one",
                            { 60, 90,   0.8f,  0.8f,  0, unison,     0.0f,  18, 0.25f, 1.0f, 0.0f, 1.0f, 0.1f, 0.0f,  0.45f, 5,  0.8f,  false, 0.7f } },
        { "velvet",           textureCat, "soft, dark, close to the skin",
                            { 14, 260,  0.2f,  0.1f,  0, unison,     0.0f,  12, 0.0f, 0.6f, 0.0f, 0.5f, 0.1f, -0.7f,  0.3f,  3,  0.2f,  false, 0.6f } },
        { "sand",             textureCat, "grains of sound, pouring slowly",
                            { 50, 18,   0.4f,  0.3f,  0, unison,     0.0f,  25, 0.5f, 1.0f, 0.6f, 0.9f, 0.0f,  0.1f,  0.2f,  2,  0.5f,  false, 0.55f } },
        { "broken music box", textureCat, "a lullaby with missing teeth",
                            { 9,  70,   0.7f,  0.6f, 12, pentatonic, 0.6f,  30, 0.2f, 0.8f, 0.7f, 0.8f, 0.2f,  0.35f, 0.4f,  4,  0.5f,  false, 0.65f } },
        { "whisper",          textureCat, "the sound remembered, not heard",
                            { 45, 30,   0.5f,  0.7f,  0, unison,     0.0f,  40, 0.5f, 1.0f, 0.2f, 1.0f, 0.0f,  0.6f,  0.55f, 6,  0.5f,  false, 0.5f } },
        { "cluster",          textureCat, "tones pressed close, trembling",
                            { 20, 300,  0.3f,  0.2f,  0, cluster,    0.9f,  20, 0.1f, 0.6f, 0.0f, 0.8f, 0.2f, -0.1f,  0.45f, 6,  0.4f,  false, 0.65f } },

        { "shimmer halo",     spaceCat, "octaves rise forever into light",
                            { 14, 420,  0.3f,  0.2f,  0, shimmer,    0.7f,  6,  0.0f, 0.6f, 0.0f, 0.9f, 0.55f, 0.3f,  0.7f,  14, 0.35f, false, 0.7f } },
        { "cathedral swarm",  spaceCat, "a vast stone room full of wings",
                            { 25, 220,  0.5f,  0.5f,  0, octaves,    0.4f,  10, 0.2f, 0.8f, 0.0f, 1.0f, 0.3f, -0.1f,  0.85f, 20, 0.4f,  false, 0.75f } },
        { "underwater choir", spaceCat, "voices heard from beneath the surface",
                            { 10, 500,  0.4f,  0.3f, -12, octaves,   0.5f,  14, 0.2f, 0.5f, 0.0f, 0.8f, 0.25f, -0.85f, 0.6f, 9,  0.5f,  false, 0.75f } },
        { "moon tide",        spaceCat, "slow waves of light pulling at the dark",
                            { 6,  1400, 0.9f,  0.5f,  0, fifths,     0.5f,  8,  0.2f, 0.5f, 0.0f, 0.9f, 0.35f, -0.2f, 0.7f,  16, 0.6f,  false, 0.8f } },
        { "deep field",       spaceCat, "an endless, starlit distance",
                            { 12, 900,  2.0f,  1.0f,  0, sus,        0.5f,  10, 0.3f, 0.7f, 0.0f, 1.0f, 0.45f, 0.0f,  0.9f,  30, 0.4f,  false, 0.85f } },
        { "sub bloom",        spaceCat, "a warm low glow beneath everything",
                            { 8,  600,  0.3f,  0.2f, -12, sub,       0.8f,  5,  0.0f, 0.5f, 0.0f, 0.5f, 0.2f, -0.5f,  0.5f,  8,  0.25f, false, 0.6f } },
        { "cloud chamber",    spaceCat, "particles leaving soft trails in the air",
                            { 35, 150,  0.6f,  0.7f,  0, unison,     0.0f,  12, 0.3f, 1.0f, 0.1f, 1.0f, 0.3f,  0.2f,  0.75f, 12, 0.45f, false, 0.7f } },
        { "northern hall",    spaceCat, "cold air, warm voices, an endless ceiling",
                            { 16, 380,  0.4f,  0.3f,  0, major,      0.4f,  8,  0.1f, 0.6f, 0.0f, 0.9f, 0.4f,  0.1f,  0.8f,  18, 0.35f, false, 0.75f } },

        { "drift",            motionCat, "pitch wanders like a leaf on water",
                            { 14, 400,  0.4f,  0.3f,  0, unison,     0.0f,  30, 0.1f, 0.6f, 0.0f, 0.8f, 0.2f,  0.0f,  0.45f, 7,  1.0f,  false, 0.7f } },
        { "orbit",            motionCat, "sound circles slowly around your head",
                            { 18, 250,  0.3f,  0.2f,  0, octaves,    0.3f,  8,  0.0f, 0.3f, 0.1f, 1.0f, 0.2f,  0.1f,  0.4f,  5,  0.7f,  false, 0.7f } },
        { "pulse",            motionCat, "a heartbeat made of tiny echoes",
                            { 8,  80,   0.1f,  0.0f,  0, unison,     0.0f,  0,  0.0f, 0.0f, 0.8f, 0.4f, 0.2f,  0.0f,  0.3f,  3,  0.1f,  false, 0.6f } },
        { "tremolo dream",    motionCat, "a shivering, sweet vibrato of grains",
                            { 30, 60,   0.05f, 0.02f, 0, unison,     0.0f,  12, 0.0f, 0.0f, 0.5f, 0.6f, 0.0f,  0.1f,  0.35f, 4,  0.3f,  false, 0.7f } },
        { "wind chimes",      motionCat, "a breeze through hanging glass",
                            { 6,  120,  1.0f,  1.0f, 12, pentatonic, 1.0f,  8,  0.0f, 1.0f, 0.5f, 1.0f, 0.25f, 0.4f,  0.6f,  10, 0.6f,  false, 0.6f } },
        { "falling",          motionCat, "everything tumbling gently downward",
                            { 14, 350,  0.4f,  0.3f, -5, unison,     0.0f,  8,  0.3f, 0.6f, 0.0f, 0.8f, 0.45f, -0.1f, 0.5f,  8,  0.4f,  false, 0.7f } },
        { "rising",           motionCat, "a staircase of light, ascending",
                            { 14, 350,  0.4f,  0.3f,  7, unison,     0.0f,  8,  0.0f, 0.6f, 0.0f, 0.8f, 0.45f, 0.1f,  0.5f,  8,  0.4f,  false, 0.7f } },
        { "chaos garden",     motionCat, "everything at once, beautifully wrong",
                            { 40, 120,  2.0f,  1.0f,  0, wholeTone,  0.8f,  35, 0.5f, 1.0f, 0.3f, 1.0f, 0.4f,  0.0f,  0.6f,  10, 0.9f,  false, 0.7f } },
    };
    return book;
}
} // namespace bouquet
