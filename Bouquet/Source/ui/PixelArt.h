#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace bouquet::ui::pixel
{
// The pixel art edition: everything is painted onto a coarse grid (one art pixel is
// `size` points), snapped to a small palette with an ordered dither, and blown back up
// with hard square edges.
constexpr int size = 2;

// Paints a component and its children onto the pixel grid instead of the screen. It
// keeps the low-resolution picture between frames and only repaints what changed.
class Canvas : public juce::CachedComponentImage
{
public:
    explicit Canvas (juce::Component& owner) : owner (owner) {}

    void paint (juce::Graphics&) override;
    bool invalidateAll() override;
    bool invalidate (const juce::Rectangle<int>&) override;
    void releaseResources() override { image = {}; }

private:
    void render();

    juce::Component& owner;
    juce::Image image;
    juce::RectangleList<int> dirty;
};

// Snaps the colours inside area (in pixels of image) to the palette, dithered.
void quantise (juce::Image&, juce::Rectangle<int> area);

// Keeps the editor's zoom to whole multiples of half size, so every art pixel covers
// the same number of screen pixels.
class ZoomConstrainer : public juce::ComponentBoundsConstrainer
{
public:
    ZoomConstrainer (int width, int height) : width (width), height (height) {}

    void checkBounds (juce::Rectangle<int>& bounds, const juce::Rectangle<int>& previous, const juce::Rectangle<int>& limits,
                      bool top, bool left, bool bottom, bool right) override;

private:
    int width, height;
};
} // namespace bouquet::ui::pixel
