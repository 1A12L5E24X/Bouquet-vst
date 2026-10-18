#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/SoundLab.h"

class BouquetProcessor;

namespace bouquet::ui
{
// The garden: every chord you play grows its own bouquet, which blooms while the
// chord is held and slowly rots once it is let go.
class Garden : public juce::Component
{
public:
    // noteToX maps a MIDI note to an x position in this component (the key below it).
    Garden (BouquetProcessor&, std::function<float (float note)> noteToX);

    // Advances growth, bloom and decay; call once per frame.
    void tick (float dt);
    bool isAlive() const { return ! bouquets.empty(); }

    void paint (juce::Graphics&) override;

    // The same chord (notes, shape, instrument, touch) always grows the same bouquet;
    // a different one grows a different bouquet.
    static uint32_t seedFor (std::vector<int> notes, int chordShape, int instrument, float velocity);

private:
    enum class Species { daisy, tulip, poppy, cosmos, bell, rose, count };

    struct Petal
    {
        float angle, length, width, fallAt, drift, spin;
    };

    struct Flower
    {
        float note;
        Species species;
        juce::Colour petal, petalDeep, centre;
        float height, spread, size, phase, leafAt, leafSide;
        std::vector<Petal> petals;
        bool bud;
    };

    struct Bunch
    {
        std::vector<int> notes;       // every note that grew a flower
        std::vector<int> held;        // notes still held
        std::vector<Flower> flowers;
        int chordShape = 0, instrument = 0;
        float velocity = 0.8f;
        float x = 0.0f;
        float age = 0.0f, growSeconds = 1.0f;
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

    void paintBunch (juce::Graphics&, const Bunch&, float time, float sway) const;
    void paintHead (juce::Graphics&, const Flower&, juce::Point<float> at, float tilt, float bloom, float rot) const;

    BouquetProcessor& processor;
    std::function<float (float)> noteToX;
    std::vector<Bunch> bouquets;
    std::array<NoteEvent, 256> incoming {};

    // Live input: a swell after a quiet moment plants a bouquet.
    float quietFor = 10.0f, loudFor = 0.0f;
    juce::Random random;
};
} // namespace bouquet::ui
