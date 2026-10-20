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
class Meadow : public juce::Component
{
public:
    explicit Meadow (BouquetProcessor&);

    void open();
    void close();
    void highlight (int index) { hovered = index; }
    bool isOpen() const { return opening; }

    // Advances the fade, growth and hover; call once per frame.
    void tick (float dt);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    struct Plant
    {
        Preset preset;
        botany::Genome genome;
        juce::Point<float> centre;
        float radius, angle, appear = 0.0f, hover = 0.0f, delay = 0.0f;
        int variant = 0;
    };

    void rebuild();
    void plant();
    int plantAt (juce::Point<float>) const;
    float radiusOf (const Plant&) const;
    void paintGround (juce::Graphics&);

    BouquetProcessor& processor;
    juce::SharedResourcePointer<real::FlowerSprites> sprites;
    std::vector<Plant> plants;
    juce::Image drawnGround;   // the illustrated lawn, made once per size
    int hovered = -1;
    float visibility = 0.0f;
    bool opening = false;
    juce::String pendingGrowth;   // name of a newly planted preset, grows in from seed

    GlowItem plantButton { "Plant this sound", GlowItem::Style::pill, juce::Colour (0xff5cc9a7), juce::Colour (0xfff2c14e) };
    GlowItem closeButton { "Back to the lab", GlowItem::Style::pill, juce::Colour (0xffa58cf0), juce::Colour (0xff6fa8f5) };
    juce::TextEditor nameField;
};
} // namespace bouquet::ui
