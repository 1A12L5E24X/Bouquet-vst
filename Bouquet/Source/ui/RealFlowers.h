#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <map>
#include "Botany.h"

namespace bouquet::ui::real
{
// Pre-rendered (Blender, Cycles) flowers: every species in eight colourways, each a
// sequence from bud to bloom and on through its rot. Rendered by tools/flowers and
// shipped inside the plugin bundle (Contents/Resources/Flowers).
class FlowerSprites
{
public:
    FlowerSprites();

    bool available() const { return ! infos.empty(); }
    static juce::File findFolder();

    int bloomFrames() const { return bloomCount; }
    int rotFrames() const { return rotCount; }

    // The colourway closest to a colour.
    static int nearestColour (juce::Colour);
    static juce::Colour colourway (int index);

    // Frame for a moment in the flower's life: bloom 0..1 while opening, then rot 0..1.
    int frameFor (float bloom, float rot) const;

    struct Frame
    {
        juce::Image image;
        juce::Point<float> base, head;               // in sprite pixels
        std::vector<juce::Point<float>> stem;        // ground up
    };
    Frame get (botany::Species, int colour, int frame);

private:
    struct Info
    {
        int width = 0, height = 0;
        std::vector<std::vector<juce::Point<float>>> stems;
        std::vector<juce::Point<float>> heads;
    };

    juce::Image atlas (botany::Species, int colour);

    juce::File folder;
    std::map<int, Info> infos;
    std::map<int, std::pair<juce::Image, juce::uint32>> atlases;   // key -> (image, last used)
    juce::uint32 useCounter = 0;
    int bloomCount = 10, rotCount = 14;
};

// Draws one rendered flower standing at `base`, `height` tall, leaning by `lean`
// radians, with live flies and dripping blood on top when it rots.
void paintFlower (juce::Graphics&, FlowerSprites&, botany::Species, int colour,
                  juce::Point<float> base, float height, float lean, float grow, float bloom, float rot,
                  float time, float glow, float alpha = 1.0f);
} // namespace bouquet::ui::real
