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
// One layer of the meadow the keyboard lies in (photoreal only): the field behind and
// under the keys (still), or the blades in front of them, drawn over the keys, which
// sway gently, more with the mod wheel. It never takes the mouse, so the keys still play.
class MeadowLayer : public juce::Component
{
public:
    MeadowLayer (BouquetProcessor& p, bool overKeys) : processor (p), over (overKeys) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;

    // Bends the grass for this moment; call once per frame. True when it changed.
    bool sway (float time, float wind, int pixelScale);

private:
    BouquetProcessor& processor;
    bool over;
    juce::SharedResourcePointer<real::FlowerSprites> sprites;
    juce::Image source, bent;      // at the screen's pixel density
    std::vector<float> wave;
};

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
    void paintPanel (juce::Graphics&, juce::Rectangle<float>, const juce::Image& frosted);
    juce::Image frost (juce::Rectangle<float>, int scale) const;
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
    juce::OwnedArray<GlowItem> instruments;
    Segmented chords   { colours::coral, 7 };
    Segmented voicings { colours::coral, 3 };
    Segmented octave   { colours::coral, 3 };
    GlowKnob tone, attack, release, strum, forest, wildlife, level;

    // Magic Lab
    juce::OwnedArray<GlowItem> categories;
    Segmented spells { colours::violet, 4 };
    std::vector<int> visibleSpells;
    int category = 0;
    AuraPad aura;
    GlowKnob mix, space, life;
    FreezeOrb freeze;
    juce::ComboBox harmony;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> harmonyAttachment;
    juce::OwnedArray<GlowKnob> inner;

    // Header
    HeaderButton gardenButton { "Garden", HeaderButton::Icon::sprout };
    Toggle flowerStyle { "Photoreal" };
    Toggle syncSwitch  { "Tempo sync" };
    Toggle magicSwitch { "Magic Lab" };
    HeaderButton mutate { "Mutate", HeaderButton::Icon::sparkle };
    GlowKnob output;

    MeadowLayer meadowUnder;
    Garden garden;
    GlowKeyboard keyboard;
    MeadowLayer meadowOver;
    Meadow meadow;

    juce::Image aurora, background;
    juce::SharedResourcePointer<real::FlowerSprites> sprites;
    juce::Image skyStrip;           // the photoreal sky at the window's pixel height, then mirrored
    bool paintSky (juce::Graphics&, int pixelW, int pixelH, float t);
    std::array<juce::int8, 256 * 256> grainNoise {};
    struct Column { int offset, t; };
    std::vector<Column> columns;
    int backgroundScale = 1;
    double backgroundTime = -1.0;
    juce::Rectangle<float> soundPanel, magicPanel;
    int shownInstrument = -1, shownSpell = -1;
    juce::Rectangle<int> spellLineArea;
    float magicShown = 1.0f;   // eases towards the Magic Lab switch: 1 open, 0 folded away
    int frameCount = 0;
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
