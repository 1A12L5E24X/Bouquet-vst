#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../dsp/SoundLab.h"
#include "Botany.h"
#include "RealFlowers.h"

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
    void resized() override;

    // 0 resting .. 1 drawn in: the garden inhales while you play.
    float getBreath() const { return breath; }

    // The same chord (notes, shape, instrument, touch) always grows the same bouquet;
    // a different one grows a different bouquet.
    static uint32_t seedFor (std::vector<int> notes, int chordShape, int instrument, float velocity);

private:
    struct Flower
    {
        botany::Genome genome;
        float note, height, spread;
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

    BouquetProcessor& processor;
    std::function<float (float)> noteToX;
    std::vector<Bunch> bouquets;
    std::array<NoteEvent, 256> incoming {};

    // Live input: a swell after a quiet moment plants a bouquet.
    float quietFor = 10.0f, loudFor = 0.0f;

    juce::SharedResourcePointer<real::FlowerSprites> sprites;
    botany::Grass grass;
    std::vector<botany::Ripple> ripples;
    float breath = 0.0f;
    juce::Random random;
};
} // namespace bouquet::ui
