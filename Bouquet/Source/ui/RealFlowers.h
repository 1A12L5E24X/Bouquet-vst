#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include <mutex>
#include "Botany.h"

namespace bouquet::ui::real
{
// Pre-rendered (Blender, Cycles) flowers: every species in three shape variants and
// eight colourways, each a sequence from bud to bloom and on through its rot, plus the
// same flowers seen from above, grass tufts, a fly and a drop of blood, and the meadow
// ground. Rendered by tools/flowers; installed with the plugin (see Assets.h).
//
// Atlases are decoded on a background thread the first time they are needed; until
// then the flower is simply not drawn yet (it is still a seed). Memory is bounded.
class FlowerSprites
{
public:
    FlowerSprites();
    ~FlowerSprites();

    bool available() const { return ! infos.empty(); }
    static constexpr int numVariants = 3;

    // Decode atlases on the calling thread instead of in the background (offline use).
    bool waitForLoads = false;

    int bloomFrames() const { return bloomCount; }
    int rotFrames() const { return rotCount; }

    // The colourway closest to a colour.
    static int nearestColour (juce::Colour);
    static juce::Colour colourway (int index);

    // Where in the sequence a moment of the flower's life is: a fractional frame.
    float framePosition (float bloom, float rot) const;

    struct Frame
    {
        juce::Image image;                           // invalid while still loading
        juce::Point<float> base, head;               // in sprite pixels
        std::vector<juce::Point<float>> stem;        // ground up
    };
    // The frame at a fractional position, cross-faded between its two neighbours.
    Frame get (botany::Species, int variant, int colour, float position);

    // The flower in full bloom seen from straight above (centred), or invalid.
    juce::Image top (botany::Species, int variant, int colour);

    // Grass tufts (side view): the ground line is at groundY pixels from the top.
    int grassCount() const { return grassSheet.isValid() ? grassTufts : 0; }
    juce::Image grass (int index) const;
    float grassGround() const { return grassGroundY; }

    juce::Image fly (int wingPhase) const;
    juce::Image drop() const;

    // The preset meadow seen from above.
    juce::Image ground();

private:
    struct Info
    {
        int width = 0, height = 0;
        std::vector<std::vector<juce::Point<float>>> stems;
        std::vector<juce::Point<float>> heads;
    };
    struct Atlas
    {
        juce::Image image;
        juce::uint32 lastUsed = 0;
    };
    class Loader;

    static int infoKey (botany::Species s, int variant) { return (int) s * 4 + variant; }
    static int atlasKey (botany::Species s, int variant, int colour) { return infoKey (s, variant) * 16 + colour; }
    juce::Image atlas (int key, const juce::String& baseName, bool wait);
    juce::Image frameImage (int key, const Info&, const juce::Image& sheet, int frame) const;
    static juce::Image loadWithAlpha (const juce::File& colour, const juce::File& alpha);

    juce::File folder;
    std::map<int, Info> infos;
    int bloomCount = 10, rotCount = 14;

    std::mutex lock;                                  // guards atlases, requested
    std::map<int, Atlas> atlases;
    std::map<int, juce::String> requested;
    juce::uint32 useCounter = 0;
    std::unique_ptr<juce::ThreadPool> pool;

    struct Blend { juce::Image image; int a = -1, b = -1, step = -1; juce::uint32 lastUsed = 0; };
    std::map<int, Blend> blends;                      // per atlas: the last cross-fade made

    std::map<int, juce::Image> tops;
    juce::Image grassSheet, extrasSheet, groundImage;
    int grassTufts = 0, extrasSize = 64;
    float grassGroundY = 0.0f;
};

// Draws one rendered flower standing at `base`, `height` tall, leaning by `lean`
// radians, with live flies and dripping blood on top when it rots.
void paintFlower (juce::Graphics&, FlowerSprites&, botany::Species, int variant, int colour,
                  juce::Point<float> base, float height, float lean, float grow, float bloom, float rot,
                  float time, float glow, float alpha = 1.0f);
} // namespace bouquet::ui::real
