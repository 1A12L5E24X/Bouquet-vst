#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/SoundLab.h"
#include "Botany.h"
#include "RealFlowers.h"

class BouquetProcessor;

namespace bouquet::ui
{
// The garden: every note you play grows a flower out of its own key, and the flowers of a
// chord are gathered into a bouquet. They rise whole from the soil, keep growing and opening
// for as long as the key is held, are molded by how you play (pressing in draws them up and
// opens them, the mod wheel bends them), and slowly rot once let go. The higher the note,
// the brighter the flower; low notes bring rain down on it, high notes shine light.
class Garden : public juce::Component
{
public:
    // The garden reaches this far above the soil, so rain and light have room to fall from.
    static constexpr int headroom = 64;

    // noteToX maps a MIDI note to an x position in this component (the key below it).
    Garden (BouquetProcessor&, std::function<float (float note)> noteToX);

    // Advances growth, molding and decay; call once per frame.
    void tick (float dt);
    bool isAlive() const { return ! bouquets.empty(); }

    void paint (juce::Graphics&) override;
    void resized() override;

    // 0 resting .. 1 drawn in: the garden inhales while you play.
    float getBreath() const { return breath; }

    // The gusts sent out by each new chord (x in this component's coordinates).
    const std::vector<botany::Ripple>& getRipples() const { return ripples; }

    // The same chord (notes, shape, instrument, touch) always grows the same bouquet;
    // a different one grows a different bouquet.
    static uint32_t seedFor (std::vector<int> notes, int chordShape, int instrument, float velocity);

private:
    struct Flower
    {
        botany::Genome genome;
        float note, height;
        float keyX = 0.0f;       // where its key is: the flower grows out of here
        float spread = 0.0f;     // keyX relative to the middle of the bouquet
        float bright = 0.5f;     // 0 dark .. 1 bright, from the note (Weather.h)
        float rain = 0.0f, light = 0.0f;
        float delay = 0.0f;      // seconds after the bouquet forms that this one starts to rise
        float rainDue = 0.0f, moteDue = 0.0f;
    };

    struct Bunch
    {
        std::vector<int> notes;       // every note that grew a flower
        std::vector<int> held;        // notes still held
        std::vector<Flower> flowers;
        int chordShape = 0, instrument = 0;
        float velocity = 0.8f;
        float x = 0.0f;
        float age = 0.0f;
        bool formed = false;          // shown: its flowers are ready to draw
        float growth = 0.0f, growSeconds = 1.0f;   // 0 seed .. 1 grown (in time since formed)
        float growthTime = 0.0f;      // seconds since formed
        float heldFor = 0.0f;         // seconds the keys have been held: it keeps growing meanwhile
        float stiffness = 200.0f;     // how firmly it springs back into shape
        float stretch = 0.0f, stretchSpeed = 0.0f;   // squashed (-) .. drawn up (+)
        float open = 0.0f;            // how far its flowers have opened
        bool released = false;
        bool fromInput = false;       // planted by live input rather than notes
        float rot = 0.0f;             // 0 fresh .. 1 gone
        uint32_t seed = 0;
        juce::Colour ribbon;
    };

    void noteOn (const NoteEvent&);
    void noteOff (const NoteEvent&);
    void listenToLiveInput (float dt);
    void arrange (Bunch&);
    void releaseBunch (Bunch&);
    bool ready (const Bunch&);
    void mold (Bunch&, float dt, float press);

    // Where one flower stands and how far it has grown at this moment.
    struct Placement
    {
        juce::Point<float> base, head, c1, c2;
        float lean = 0.0f, fan = 0.0f, risen = 0.0f, opening = 0.0f, creep = 1.0f, droop = 0.0f, wide = 1.0f, breeze = 0.0f;
        float weather = 0.0f;    // how much of its weather it has earned: it must be up, and not rotted away
    };
    Placement place (const Bunch&, const Flower&, float time, float sway) const;
    void paintBunch (juce::Graphics&, const Bunch&, float time, float sway) const;
    void paintSprouts (juce::Graphics&, const Bunch&) const;

    // Weather: rain falls onto a flower from a low note, light shines on one from a high note.
    struct Drop  { juce::Point<float> pos; float speed, length, landY, alpha; };
    struct Splash { juce::Point<float> at; float age; };
    struct Mote  { juce::Point<float> pos, vel; float life, size, phase; };
    void tickWeather (float dt);
    void paintSky (juce::Graphics&, float time) const;       // clouds and shafts of light, behind the flowers
    void paintWeather (juce::Graphics&, float time) const;   // rain, splashes, sparks, in front of them

    // Grass grows only where keys are played (and a little around them), then recedes.
    struct Patch
    {
        float x, level = 0.0f, target = 0.0f;
        uint32_t seed;
    };
    void growGrass (int note, float strength);
    float grassCoverage (float x) const;
    float keyWidth() const;
    void paintRealGrass (juce::Graphics&, float time, bool front) const;

    BouquetProcessor& processor;
    std::function<float (float)> noteToX;
    std::vector<Drop> drops;
    std::vector<Splash> splashes;
    std::vector<Mote> motes;
    std::vector<Bunch> bouquets;
    std::array<NoteEvent, 256> incoming {};

    // Live input: a swell after a quiet moment plants a bouquet.
    float quietFor = 10.0f, loudFor = 0.0f;

    juce::SharedResourcePointer<real::FlowerSprites> sprites;
    botany::Grass grass;
    std::vector<Patch> patches;
    std::vector<botany::Ripple> ripples;
    float breath = 0.0f;
    juce::Random random;
};
} // namespace bouquet::ui
