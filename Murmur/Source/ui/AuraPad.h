#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../dsp/GranularEngine.h"
#include "MurmurLookAndFeel.h"

namespace murmur::ui
{
// The aura: an XY field that shapes the cloud (left dust, right smear; low close,
// high vast) and shows every grain as a mote of light. Height is pitch, distance
// from the left edge is how far back in memory the grain was taken from.
class AuraPad : public juce::Component
{
public:
    AuraPad (juce::AudioProcessorValueTreeState&, GranularEngine&);

    // Pulls new grains from the engine and advances the particles; call once per frame.
    void tick (float dt);

    void paint (juce::Graphics&) override;

    // The slow parts of the pad (glass and drifting light), painted into the editor's
    // cached background at the pad's position.
    void paintBackdrop (juce::Graphics&) const;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    struct Mote
    {
        float x, y, vx, vy, age, life, size;
        juce::Colour colour;
    };

    juce::Rectangle<float> field() const;
    void setFromMouse (juce::Point<float>);

    GranularEngine& engine;
    juce::ParameterAttachment xAttach, yAttach;
    float auraX = 0.5f, auraY = 0.5f;
    float shownX = 0.5f, shownY = 0.5f;
    bool dragging = false;

    std::vector<Mote> motes;
    std::array<GrainEvent, 512> incoming {};
    std::array<float, GranularEngine::memoryBins> memory {};
    std::array<juce::Point<float>, 14> trail {};
    juce::Random random;
};
} // namespace murmur::ui
