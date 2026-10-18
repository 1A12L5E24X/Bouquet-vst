#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "../PluginProcessor.h"
#include "MurmurLookAndFeel.h"
#include "Widgets.h"
#include "AuraPad.h"

namespace murmur::ui
{
// Everything lives in this fixed-size surface; the editor scales it to the window.
class Surface : public juce::Component
{
public:
    static constexpr int designWidth = 1160;
    static constexpr int designHeight = 740;

    explicit Surface (MurmurProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void frame (float dt);

private:
    void renderBackground();
    void paintPanel (juce::Graphics&, juce::Rectangle<float>, juce::Colour glow);
    void paintSectionLabel (juce::Graphics&, const juce::String&, int x, int y);
    void setParam (const juce::String& id, float value);
    float param (const juce::String& id) const;
    void showCategory (int category);
    void syncSelections();

    MurmurProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    // Sound Lab
    juce::OwnedArray<GlowItem> instruments, chords, voicings;
    GlowItem octaveDown { "-", GlowItem::Style::pill, colours::coral, colours::peach };
    GlowItem octaveUp   { "+", GlowItem::Style::pill, colours::coral, colours::peach };
    GlowKnob tone, attack, release, strum, level;

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
    GlowItem mutate { "mutate", GlowItem::Style::pill, colours::violet, colours::aqua };
    GlowKnob output;

    GlowKeyboard keyboard;

    juce::Image aurora, background;
    std::array<juce::int8, 256 * 256> grainNoise {};
    struct Column { int offset, t; };
    std::vector<Column> columns;
    double backgroundTime = -1.0;
    juce::Rectangle<float> soundPanel, magicPanel;
    float heartLevel = 0.0f;
    int shownInstrument = -1, shownOctave = 99, shownSpell = -1;
    const juce::Rectangle<int> heartArea { 0, 0, 66, 94 };
    juce::Rectangle<int> spellLineArea;
};
} // namespace murmur::ui

class MurmurEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit MurmurEditor (MurmurProcessor&);
    ~MurmurEditor() override;

    void resized() override;

private:
    void timerCallback() override;

    murmur::ui::MurmurLookAndFeel lookAndFeel;
    murmur::ui::Surface surface;
    double lastFrame = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MurmurEditor)
};
