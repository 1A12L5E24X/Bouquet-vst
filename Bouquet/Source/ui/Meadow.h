#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
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
// And a little lab bench: click the flasks of tonic to dose a beaker of water (each
// pour a different amount), then drag the beaker onto a flower to water it. A richer
// version of that preset grows beside it, a little different every time.
class Meadow : public juce::Component
{
public:
    explicit Meadow (BouquetProcessor&);

    void open();
    void close();
    void highlight (int index) { hovered = index; }
    bool isOpen() const { return opening; }

    // Stirs these tonics into the beaker and pours it on a flower, as if carried there.
    void water (int plant, std::vector<Tonic> tonics);

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
        int variant = 0;
    };

    void rebuild();
    void plant();
    int plantAt (juce::Point<float>) const;
    float radiusOf (const Plant&) const;
    void paintGround (juce::Graphics&);

    // --- The lab bench
    struct Flask
    {
        Tonic tonic;
        juce::Colour colour;
        juce::Point<float> base;   // bottom centre
        float hover = 0.0f, shake = 0.0f;
    };
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

    int flaskAt (juce::Point<float>) const;
    bool beakerAt (juce::Point<float>) const;
    void dose (int flask);
    void emptyBeaker();
    void finishPour();
    juce::Colour beakerColour() const;
    juce::AffineTransform beakerTransform() const;
    juce::Point<float> spout() const;
    void tickLab (float dt);
    void paintLab (juce::Graphics&, float time);
    void paintFlask (juce::Graphics&, const Flask&, float time) const;
    void paintBeaker (juce::Graphics&, float time) const;
    void paintLabel (juce::Graphics&, const juce::String& title, const juce::String& text, juce::Point<float> above, float alpha) const;

    BouquetProcessor& processor;
    juce::SharedResourcePointer<real::FlowerSprites> sprites;
    std::vector<Plant> plants;
    juce::Image drawnGround;   // the illustrated lawn, made once per size
    int hovered = -1;
    float visibility = 0.0f;
    bool opening = false;
    juce::String pendingGrowth;   // name of a newly planted preset, grows in from seed

    std::array<Flask, (size_t) Tonic::count> flasks;
    std::vector<Tonic> mixture;               // what is stirred into the beaker
    float level = 0.35f, shownLevel = 0.35f;  // how full the beaker is, 0..1
    juce::Point<float> beakerHome, beakerPos, grabOffset;
    float tilt = 0.0f, slosh = 0.0f, lift = 1.0f;   // lift: larger once picked up off the bench
    bool carrying = false, hoverBeaker = false;
    int hoveredFlask = -1, target = -1;
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
