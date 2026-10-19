#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "../PluginProcessor.h"
#include "BouquetLookAndFeel.h"
#include "Widgets.h"
#include "AuraPad.h"
#include "Garden.h"
#include "Meadow.h"

namespace bouquet::ui
{
// Everything lives in this fixed-size surface; the editor scales it to the window.
class Surface : public juce::Component
{
public:
    static constexpr int designWidth = 1160;
    static constexpr int designHeight = 860;

    explicit Surface (BouquetProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void frame (float dt);
    void showPresetGarden (int highlight = -1) { meadow.open(); meadow.highlight (highlight); }

private:
    void renderBackground();
    int pixelScale() const;
    void paintPanel (juce::Graphics&, juce::Rectangle<float>, juce::Colour glow);
    void paintSectionLabel (juce::Graphics&, const juce::String&, int x, int y);
    void setParam (const juce::String& id, float value);
    float param (const juce::String& id) const;
    void showCategory (int category);
    void syncSelections();
    juce::Rectangle<int> soundArea() const;
    juce::Rectangle<int> magicArea() const;
    std::vector<juce::Component*> magicComponents();

    BouquetProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    // Sound Lab
    juce::OwnedArray<GlowItem> instruments, chords, voicings;
    GlowItem octaveDown { "-", GlowItem::Style::pill, colours::coral, colours::peach };
    GlowItem octaveUp   { "+", GlowItem::Style::pill, colours::coral, colours::peach };
    GlowKnob tone, attack, release, strum, forest, wildlife, level;

    // Magic Lab
    juce::OwnedArray<GlowItem> categories, spells;
    std::vector<int> visibleSpells;
    int category = 0;
    AuraPad aura;
    GlowKnob mix, space, life;
    FreezeOrb freeze;
    juce::ComboBox harmony;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> harmonyAttachment;
    juce::OwnedArray<GlowKnob> inner;

    // Header
    GlowItem gardenButton { "garden", GlowItem::Style::pill, colours::mint, colours::butter };
    GlowItem magicSwitch  { "magic lab", GlowItem::Style::pill, colours::violet, colours::sky };
    GlowItem flowerStyle  { "photoreal", GlowItem::Style::pill, colours::rose, colours::peach };
    GlowItem syncSwitch   { "tempo sync", GlowItem::Style::pill, colours::mint, colours::sky };
    GlowItem mutate { "mutate", GlowItem::Style::pill, colours::violet, colours::aqua };
    GlowKnob output;

    GlowKeyboard keyboard;
    Garden garden;
    Meadow meadow;

    juce::Image aurora, background;
    juce::SharedResourcePointer<real::FlowerSprites> sprites;
    juce::Image skyScaled;          // the photoreal sky at the window's pixel height
    bool paintSky (juce::Graphics&, int pixelW, int pixelH, float t, float warm, float cool);
    std::array<juce::int8, 256 * 256> grainNoise {};
    struct Column { int offset, t; };
    std::vector<Column> columns;
    int backgroundScale = 1;
    double backgroundTime = -1.0;
    juce::Rectangle<float> soundPanel, magicPanel;
    int shownInstrument = -1, shownOctave = 99, shownSpell = -1;
    juce::Rectangle<int> spellLineArea;
    float magicShown = 1.0f;   // eases towards the Magic Lab switch: 1 open, 0 folded away
};
} // namespace bouquet::ui

class BouquetEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit BouquetEditor (BouquetProcessor&);
    ~BouquetEditor() override;

    void resized() override;

    // Opens the preset meadow (optionally with one flower highlighted, as if hovered).
    void showPresetGarden (int highlight = -1) { surface.showPresetGarden (highlight); }

private:
    void timerCallback() override;

    bouquet::ui::BouquetLookAndFeel lookAndFeel;
    bouquet::ui::Surface surface;
    double lastFrame = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BouquetEditor)
};
