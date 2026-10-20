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
    // Whether this species has been rendered (otherwise draw it illustrated).
    bool has (botany::Species s) const { return infos.count (infoKey (s, 0)) != 0; }
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
        float imageScale = 1.0f;                     // image pixels per sprite pixel
        juce::Point<float> base, head;               // in sprite pixels
        std::vector<juce::Point<float>> stem;        // ground up
    };
    // The frame at a fractional position, cross-faded between its two neighbours.
    Frame get (botany::Species, int variant, int colour, float position);

    // Starts decoding a flower's frames in the background, so they are ready by the time
    // it has grown (call when it is planted).
    void prefetch (botany::Species, int variant, int colour);

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

    // A daylight sky with drifting cumulus, wider than the window (photoreal backdrop).
    juce::Image sky();

    // The meadow the keyboard lies in: the field (drawn under the keys) and the blades
    // in front of them (drawn over the keys). Both span the window's width.
    juce::Image meadow (bool overKeys);

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
    // Atlases are kept at this fraction of their rendered size: sharp at the garden's
    // sizes on a Retina screen, and light enough to keep a whole garden in memory.
    static constexpr float atlasScale = 0.7f;
    static int scaled (int px) { return juce::roundToInt ((float) px * atlasScale); }

    static int infoKey (botany::Species s, int variant) { return (int) s * 4 + variant; }
    static int atlasKey (botany::Species s, int variant, int colour) { return infoKey (s, variant) * 16 + colour; }
    juce::Image atlas (int key, const juce::String& baseName, bool wait);
    juce::Image frameImage (int key, const Info&, const juce::Image& sheet, int frame) const;
public:
    // Decodes a colour JPEG and its alpha mask into one premultiplied image, scaled to
    // width x height on the way (0 keeps the file's size).
    static juce::Image loadWithAlpha (const juce::File& colour, const juce::File& alpha, int width = 0, int height = 0);

private:

    juce::File folder;
    std::map<int, Info> infos;
    int bloomCount = 10, rotCount = 14;
    int frameWidth = 0, frameHeight = 0, frameRows = 4;

    std::mutex lock;                                  // guards atlases, requested
    std::map<int, Atlas> atlases;
    std::map<int, juce::String> requested;
    juce::uint32 useCounter = 0;
    std::unique_ptr<juce::ThreadPool> pool;

    struct Blend { juce::Image image; int a = -1, b = -1, step = -1; juce::uint32 lastUsed = 0; };
    std::map<int, Blend> blends;                      // per atlas: the last cross-fade made

    std::map<int, juce::Image> tops;
    juce::Image grassSheet, extrasSheet, groundImage, skyImage, meadowUnder, meadowOver;
    int grassTufts = 0, extrasSize = 64;
    float grassGroundY = 0.0f;
};

// Draws one rendered flower standing at `base`, `height` tall, leaning by `lean`
// radians, with live flies and dripping blood on top when it rots.
void paintFlower (juce::Graphics&, FlowerSprites&, botany::Species, int variant, int colour,
                  juce::Point<float> base, float height, float lean, float grow, float bloom, float rot,
                  float time, float glow, float alpha = 1.0f);
} // namespace bouquet::ui::real
