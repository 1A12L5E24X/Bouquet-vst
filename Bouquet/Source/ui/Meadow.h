#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Botany.h"
#include "RealFlowers.h"
#include "Widgets.h"
#include "../Presets.h"

class BouquetProcessor;

namespace bouquet::ui
{
// The preset garden, seen from above: every preset is a flower in a sunlit lawn. Hover
// to read its name, click to pick it, plant the current sound to grow a new one.
//
// And a bucket of water: drag it onto a flower and tip it to water it. A richer version
// of that preset grows, and what the water brings out of it is a surprise every time.
//
// Beside it a shelf of tools, each carried to a flower the same way: garden soil makes
// its sound weirder, pruning shears smaller, a growth tonic bigger, a jar of fireflies
// glowy, a jar of crickets glitchy. The flower itself is shaped to match.
class Meadow : public juce::Component
{
public:
    explicit Meadow (BouquetProcessor&);

    void open();
    void close();
    void highlight (int index) { hovered = index; }
    bool isOpen() const { return opening; }

    // Pours the bucket on a flower, as if carried there.
    void water (int plant);

    // Uses a tool from the shelf on a flower, as if carried there.
    void tend (Tending tool, int plant) { useTool ((int) tool, plant); }

    // Advances the fade, growth and hover; call once per frame.
    void tick (float dt);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Plant
    {
        Preset preset;
        botany::Genome genome;
        juce::Point<float> centre;
        float radius, angle, appear = 0.0f, hover = 0.0f, delay = 0.0f;
        float wet = 0.0f;   // glistening after being watered
        float earth = 0.0f;   // a patch of dark soil, after being soiled
        float bigness = 0.0f, glow = 0.0f, glitch = 0.0f;   // how the tools have shaped it (see traits)
        int variant = 0;
    };

    void rebuild();
    void plant();
    int plantAt (juce::Point<float>) const;
    float radiusOf (const Plant&) const;
    void paintGround (juce::Graphics&);

    // --- The bucket
    struct Drop
    {
        juce::Point<float> pos, vel;
        juce::Colour colour;
        float size, life, landY;   // falls until it reaches landY, then splashes
        bool splash = false;
    };
    struct Note
    {
        juce::String title, text;
        juce::Point<float> at;
        float age = 0.0f;
    };

    bool bucketAt (juce::Point<float>) const;
    float bucketScale() const;
    juce::Point<float> lipFor (juce::Point<float> base) const;   // where the upright pail's lip is
    juce::Point<float> lip() const;
    void finishPour();
    void tickBucket (float dt);
    void paintBucket (juce::Graphics&, float time) const;
    void paintLabel (juce::Graphics&, const juce::String& title, const juce::String& text, juce::Point<float> above, float alpha) const;

    // --- The shelf of tools
    struct Tool
    {
        Tending kind;
        juce::Point<float> home, pos, grab;
        float lift = 1.0f, hover = 0.0f, sway = 0.0f;
        bool carrying = false;
    };
    // Things in the air while a tool is used: crumbs of soil, cut petals, drops of tonic,
    // fireflies, crickets.
    struct Bit
    {
        juce::Point<float> pos, vel;
        juce::Colour colour;
        float size, life, maxLife, angle = 0.0f, spin = 0.0f, landY = 0.0f, aux = 0.0f;
        bool landed = false;
        Tending kind = Tending::soil;
    };
    struct Use
    {
        int tool = -1, plant = -1;
        juce::Point<float> at, held;   // the flower; where the tool is held over it
        float radius = 0.0f, t = 0.0f, emit = 0.0f;
        bool done = false;
    } use;

    int toolAt (juce::Point<float>) const;
    void useTool (int tool, int plant);
    void finishUse();
    void tickTools (float dt);
    void paintTools (juce::Graphics&, float time) const;
    void paintBits (juce::Graphics&, float time) const;
    static void paintToolArt (juce::Graphics&, Tending, float size, float open, float time);
    juce::Point<float> toolSize() const { return { 64.0f, 64.0f }; }

    BouquetProcessor& processor;
    juce::SharedResourcePointer<real::FlowerSprites> sprites;
    std::array<Tool, (size_t) Tending::count> tools;
    std::vector<Bit> bits;
    int hoveredTool = -1;
    std::vector<Plant> plants;
    juce::Image drawnGround;   // the illustrated lawn, made once per size
    int hovered = -1;
    float visibility = 0.0f;
    bool opening = false;
    juce::String pendingGrowth;   // name of a newly planted preset, grows in from seed

    juce::Point<float> bucketHome, bucketPos, grabOffset;   // where the upright pail stands
    juce::Point<float> pourLip;                              // tipping: where its lip is held
    float tilt = 0.0f;     // degrees, as rendered: 0 upright .. 90 on its side
    float sway = 0.0f;     // radians, swinging as it is carried
    float lift = 1.0f;     // larger once picked up
    float hover = 0.0f;
    bool carrying = false, hoverBucket = false;
    int target = -1;
    struct Pour
    {
        int plant = -1;             // the flower being watered (until the new one grows)
        juce::Point<float> at;      // where it stands
        float radius = 0.0f, t = 0.0f;
        bool done = false;
    } pour;
    std::vector<Drop> drops;
    std::vector<Note> notes;
    juce::Random random;

    GlowItem plantButton { "Plant this sound", GlowItem::Style::pill, juce::Colour (0xff5cc9a7), juce::Colour (0xfff2c14e) };
    GlowItem closeButton { "Back to the lab", GlowItem::Style::pill, juce::Colour (0xffa58cf0), juce::Colour (0xff6fa8f5) };
    juce::TextEditor nameField;
};
} // namespace bouquet::ui
