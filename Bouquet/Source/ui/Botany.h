#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <vector>

namespace bouquet { struct Preset; }

// Realistic flowers and grass, shared by the chord garden and the preset meadow.
namespace bouquet::ui::botany
{
enum class Species { daisy, tulip, poppy, cosmos, bell, rose, count };

struct PetalGene
{
    float angle, length, width, fallAt, drift, spin, phase;
};

// Everything that makes one flower itself.
struct Genome
{
    Species species = Species::daisy;
    juce::Colour petal, petalDeep, centre;
    float size = 12.0f;
    float phase = 0.0f;
    float leafAt = 0.35f, leafSide = 1.0f;
    int leaves = 1;
    bool bud = false;
    uint32_t seed = 1;
    std::vector<PetalGene> petals, innerPetals;
};

Genome makeGenome (Species, juce::Colour petal, float size, uint32_t seed);

// The flower that stands for a preset: species from the instrument, colour from the
// harmony and colour controls, size and form from the rest; unique per preset.
Genome genomeForPreset (const Preset&);

// Where and how a plant is drawn this frame.
struct Pose
{
    juce::Point<float> base, c1, c2, head;   // the stem is a cubic Bezier base -> head
    float grow = 1.0f;     // 0..1 how much of the stem exists
    float bloom = 1.0f;    // 0..1 how open the flower is
    float rot = 0.0f;      // 0 fresh .. 1 gone
    float time = 0.0f;
    float stemWidth = 2.6f;
    float alpha = 1.0f;
    float glow = 0.0f;     // highlighted (hover / current)
};

void paintPlant (juce::Graphics&, const Genome&, const Pose&);

// The same flower seen from straight above, `radius` across its petals.
void paintFromAbove (juce::Graphics&, const Genome&, juce::Point<float> centre, float radius,
                     float angle, float open, float glow, float alpha = 1.0f);

// Ripples of wind sent through the grass when a chord is played.
struct Ripple
{
    float x, age, strength;
};

// A strip of grass: hundreds of blades swaying, breathing in when the music starts.
class Grass
{
public:
    void generate (float width, float maxHeight, uint32_t seed, float density = 1.0f);

    // breath: 0 resting .. 1 fully drawn in.  front: the layer in front of the flowers.
    // coverage (optional): how grown the grass is at an x, 0 bare .. 1 full.
    void paint (juce::Graphics&, juce::Point<float> origin, float time, float breath,
                const std::vector<Ripple>&, bool front,
                const std::function<float (float x)>& coverage = {}) const;

private:
    struct Blade
    {
        float x, height, width, lean, phase;
        juce::Colour colour;
        bool front, seedHead;
    };
    std::vector<Blade> blades;
};
} // namespace bouquet::ui::botany
