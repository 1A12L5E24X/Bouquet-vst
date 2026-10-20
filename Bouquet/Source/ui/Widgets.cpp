#include "Widgets.h"

namespace bouquet::ui
{
void drawSparkle (juce::Graphics& g, juce::Point<float> c, float r, juce::Colour colour)
{
    juce::Path p;
    const float k = r * 0.22f;
    p.startNewSubPath (c.x, c.y - r);
    p.quadraticTo (c.x + k, c.y - k, c.x + r, c.y);
    p.quadraticTo (c.x + k, c.y + k, c.x, c.y + r);
    p.quadraticTo (c.x - k, c.y + k, c.x - r, c.y);
    p.quadraticTo (c.x - k, c.y - k, c.x, c.y - r);
    p.closeSubPath();
    g.setColour (colour);
    g.fillPath (p);
}

//==============================================================================
GlowKnob::GlowKnob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& text,
                    juce::Colour a, juce::Colour b, bool bipolar)
    : label (text), param (state.getParameter (paramID))
{
    slider.setColour (juce::Slider::rotarySliderFillColourId, a);
    slider.setColour (juce::Slider::thumbColourId, b);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityBasedMode (false);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    slider.onDragStart = slider.onDragEnd = [this] { repaint(); };
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);
    slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
}

void GlowKnob::refreshIfTouched()
{
    const bool touched = slider.isMouseOverOrDragging();
    if (touched || touched != wasTouched)
        repaint();
    wasTouched = touched;
}

void GlowKnob::resized()
{
    auto r = getLocalBounds();
    r.removeFromBottom (16);
    const int side = std::min (r.getWidth(), r.getHeight());
    slider.setBounds (r.withSizeKeepingCentre (side, side));
}

void GlowKnob::paint (juce::Graphics& g)
{
    const bool touching = slider.isMouseOverOrDragging();
    const auto text = touching ? (param->getCurrentValueAsText() + " " + param->getLabel()).trim() : label.toUpperCase();
    g.setFont (touching ? Fonts::sans (10.5f) : Fonts::sansMedium (getWidth() < 52 ? 8.5f : 9.0f, getWidth() < 52 ? 0.05f : 0.1f));
    g.setColour (colours::ink.withAlpha (touching ? 0.9f : 0.5f));
    g.drawText (text, getLocalBounds().removeFromBottom (16), juce::Justification::centred, false);
}

//==============================================================================
GlowItem::GlowItem (const juce::String& t, Style s, juce::Colour a, juce::Colour b)
    : text (t), style (s), accentA (a), accentB (b),
      font (s == Style::text ? Fonts::serif (19.0f) : Fonts::serifItalic (17.0f))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void GlowItem::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && onClick)
        onClick();
}

bool GlowItem::animate()
{
    const float hoverTarget = isMouseOver() ? 1.0f : 0.0f;
    const float selectTarget = selected ? 1.0f : 0.0f;
    const bool settled = std::abs (hoverTarget - hoverLight) < 0.002f && std::abs (selectTarget - selectLight) < 0.002f;

    hoverLight += (hoverTarget - hoverLight) * 0.25f;
    selectLight += (selectTarget - selectLight) * 0.18f;

    // The chosen item keeps breathing; everything else rests once settled.
    return selected || ! settled;
}

void GlowItem::paint (juce::Graphics& g)
{
    using namespace colours;
    const auto bounds = getLocalBounds().toFloat();
    const float breathe = 0.85f + 0.15f * (float) std::sin (clock() * 1.7 + (double) getX() * 0.01);

    if (style == Style::pill)
    {
        const auto cell = bounds.reduced (0.5f);
        g.setColour (frost.withAlpha (0.45f + 0.25f * hoverLight));
        g.fillRoundedRectangle (cell, 10.0f);
        g.setColour (ink.withAlpha (0.2f + 0.1f * hoverLight));
        g.drawRoundedRectangle (cell, 10.0f, 1.0f);
        g.setFont (Fonts::sansMedium (12.5f));
        g.setColour (ink.withAlpha (0.8f + 0.2f * hoverLight));
        g.drawText (text, cell, juce::Justification::centred, false);
        return;
    }

    if (style == Style::tab)
    {
        // A word that underlines itself when chosen.
        const float textW = juce::GlyphArrangement::getStringWidth (font, text);
        g.setFont (font);
        g.setColour (ink.withAlpha (0.5f + 0.2f * hoverLight + 0.5f * selectLight));
        g.drawText (text, bounds, juce::Justification::centredLeft, false);
        if (selectLight > 0.01f)
        {
            g.setColour (accentA.withAlpha (0.75f * selectLight));
            g.fillRoundedRectangle (0.0f, bounds.getBottom() - 2.0f, textW * selectLight, 2.0f, 1.0f);
        }
        return;
    }

    // Text style: a word, with a dot in front that lights up when it is chosen.
    float textX = 0.0f;
    if (showDot)
    {
        const juce::Point<float> dot (5.0f, bounds.getCentreY() + 1.0f);
        if (selectLight > 0.01f)
            drawGlow (g, dot, 10.0f, accentA.withAlpha (0.55f * selectLight * breathe));
        g.setColour ((selectLight > 0.5f ? accentA : ink).withAlpha (0.25f + 0.75f * selectLight));
        const float d = 4.0f + 4.0f * selectLight;
        g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (dot));
        textX = 16.0f;
    }

    g.setFont (font);
    g.setColour (ink.withAlpha (0.5f + 0.25f * hoverLight + 0.5f * selectLight));
    g.drawText (text, bounds.withTrimmedLeft (textX), showDot ? juce::Justification::centredLeft
                                                              : juce::Justification::centred, false);
}

//==============================================================================
Segmented::Segmented (juce::Colour a, int cols) : accent (a), columns (cols)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Segmented::setItems (const juce::StringArray& newItems)
{
    items = newItems;
    hovered = -1;
    repaint();
}

void Segmented::setText (int index, const juce::String& t)
{
    if (juce::isPositiveAndBelow (index, items.size()) && items[index] != t)
    {
        items.set (index, t);
        repaint();
    }
}

void Segmented::setSelected (int index)
{
    if (index != selected)
    {
        selected = index;
        repaint();
    }
}

juce::Rectangle<float> Segmented::cell (int index) const
{
    const int rows = std::max (1, (items.size() + columns - 1) / columns);
    const int row = index / columns, col = index % columns;
    const auto b = getLocalBounds().toFloat();
    float total = 0.0f, before = 0.0f;
    for (int c = 0; c < columns; ++c)
    {
        const float w = c < (int) weights.size() ? weights[(size_t) c] : 1.0f;
        if (c < col) before += w;
        total += w;
    }
    const float w = col < (int) weights.size() ? weights[(size_t) col] : 1.0f;
    const float h = b.getHeight() / (float) rows;
    return { b.getX() + b.getWidth() * before / total, b.getY() + h * (float) row, b.getWidth() * w / total, h };
}

int Segmented::cellAt (juce::Point<float> p) const
{
    for (int i = 0; i < items.size(); ++i)
        if (cell (i).contains (p))
            return i;
    return -1;
}

void Segmented::mouseMove (const juce::MouseEvent& e)
{
    const int h = cellAt (e.position);
    if (h != hovered)
    {
        hovered = h;
        repaint();
    }
}

void Segmented::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    repaint();
}

void Segmented::mouseUp (const juce::MouseEvent& e)
{
    const int i = cellAt (e.position);
    if (e.mouseWasClicked() && i >= 0 && i != passive && onSelect)
        onSelect (i);
}

void Segmented::paint (juce::Graphics& g)
{
    using namespace colours;
    const auto frame = getLocalBounds().toFloat().reduced (0.5f);
    constexpr float radius = 10.0f;
    juce::Path outline;
    outline.addRoundedRectangle (frame, radius);

    g.setColour (frost.withAlpha (0.4f));
    g.fillPath (outline);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (outline);

        if (hovered >= 0 && hovered != passive && hovered != selected)
        {
            g.setColour (frost.withAlpha (0.5f));
            g.fillRect (cell (hovered));
        }

        // Hairlines between the cells.
        g.setColour (ink.withAlpha (0.11f));
        const int rows = std::max (1, (items.size() + columns - 1) / columns);
        for (int c = 1; c < columns; ++c)
            g.fillRect (cell (c).getX() - 0.5f, frame.getY(), 1.0f, frame.getHeight());
        for (int r = 1; r < rows; ++r)
            g.fillRect (frame.getX(), cell (r * columns).getY() - 0.5f, frame.getWidth(), 1.0f);

        if (juce::isPositiveAndBelow (selected, items.size()))
        {
            const auto c = cell (selected).reduced (0.5f);
            g.setColour (accent.withAlpha (0.15f));
            g.fillRect (c);
            g.setColour (accent.withAlpha (0.6f));
            g.drawRect (c, 1.0f);
        }
    }
    g.setColour (ink.withAlpha (0.2f));
    g.strokePath (outline, juce::PathStrokeType (1.0f));

    for (int i = 0; i < items.size(); ++i)
    {
        const bool on = i == selected;
        g.setFont (on ? Fonts::sansMedium (12.5f) : Fonts::sans (12.5f));
        g.setColour (on ? accent.darker (0.25f) : ink.withAlpha (i == passive ? 0.9f : 0.72f));
        g.drawText (items[i], cell (i).reduced (2.0f, 0.0f), juce::Justification::centred, true);
    }
}

//==============================================================================
Toggle::Toggle (const juce::String& l) : label (l)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void Toggle::setOn (bool o)
{
    if (o != on)
    {
        on = o;
        repaint();
    }
}

bool Toggle::animate()
{
    const float target = on ? 1.0f : 0.0f;
    if (std::abs (target - position) < 0.002f)
    {
        position = target;
        return false;
    }
    position += (target - position) * 0.35f;
    return true;
}

void Toggle::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && onClick)
        onClick();
}

void Toggle::paint (juce::Graphics& g)
{
    using namespace colours;
    const auto b = getLocalBounds().toFloat();
    const juce::Rectangle<float> track (0.0f, b.getCentreY() - 9.0f, 30.0f, 18.0f);

    g.setColour (ink.withAlpha (0.18f));
    g.fillRoundedRectangle (track, 9.0f);
    if (position > 0.0f)
    {
        g.setGradientFill (juce::ColourGradient (coral.withAlpha (position), track.getX(), 0.0f,
                                                 lilac.withAlpha (position), track.getRight(), 0.0f, false));
        g.fillRoundedRectangle (track, 9.0f);
    }

    const auto knob = juce::Rectangle<float> (14.0f, 14.0f).withPosition (track.getX() + 2.0f + 12.0f * position, track.getY() + 2.0f);
    g.setColour (ink.withAlpha (0.18f));
    g.fillEllipse (knob.translated (0.0f, 0.8f));
    g.setColour (frost);
    g.fillEllipse (knob);

    g.setFont (Fonts::sansMedium (12.5f));
    g.setColour (ink.withAlpha (isMouseOver() ? 0.95f : 0.75f));
    g.drawText (label, b.withTrimmedLeft (39.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
HeaderButton::HeaderButton (const juce::String& l, Icon i) : label (l), icon (i)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void HeaderButton::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && onClick)
        onClick();
}

void HeaderButton::paint (juce::Graphics& g)
{
    using namespace colours;
    const auto b = getLocalBounds().toFloat();
    const float alpha = isMouseOver() ? 1.0f : 0.8f;
    const float cy = b.getCentreY();

    juce::Path p;
    if (icon == Icon::sprout)
    {
        // A stem with two leaves.
        const float x = 8.0f, y = cy - 8.0f;
        p.startNewSubPath (x, y + 14.0f);
        p.lineTo (x, y + 5.0f);
        p.startNewSubPath (x, y + 9.0f);
        p.cubicTo (x - 3.0f, y + 9.0f, x - 5.0f, y + 7.0f, x - 5.0f, y + 4.0f);
        p.cubicTo (x - 2.0f, y + 4.0f, x, y + 6.0f, x, y + 9.0f);
        p.startNewSubPath (x, y + 7.0f);
        p.cubicTo (x + 2.0f, y + 7.0f, x + 5.0f, y + 5.0f, x + 5.0f, y + 2.0f);
        p.cubicTo (x + 2.0f, y + 2.0f, x, y + 4.0f, x, y + 7.0f);
    }
    else
    {
        // A four-pointed star, drawn in outline.
        const float x = 7.0f, r = 6.0f, k = 1.3f;
        p.startNewSubPath (x, cy - r);
        p.lineTo (x + k, cy - k);
        p.lineTo (x + r, cy);
        p.lineTo (x + k, cy + k);
        p.lineTo (x, cy + r);
        p.lineTo (x - k, cy + k);
        p.lineTo (x - r, cy);
        p.lineTo (x - k, cy - k);
        p.closeSubPath();
    }
    g.setColour (ink.withAlpha (alpha * 0.85f));
    g.strokePath (p, juce::PathStrokeType (1.15f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setFont (Fonts::sansMedium (12.5f));
    g.setColour (ink.withAlpha (alpha * 0.92f));
    g.drawText (label, b.withTrimmedLeft (23.0f), juce::Justification::centredLeft, false);
}

//==============================================================================
GlowKeyboard::GlowKeyboard (juce::MidiKeyboardState& keyState)
    : juce::MidiKeyboardComponent (keyState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setColour (whiteNoteColourId, juce::Colours::transparentBlack);
    setColour (keySeparatorLineColourId, juce::Colours::transparentBlack);
    setColour (shadowColourId, juce::Colours::transparentBlack);
    setAvailableRange (36, 107);
    setScrollButtonsVisible (false);
    setWantsKeyboardFocus (false);
    setBlackNoteLengthProportion (0.61f);
    setBlackNoteWidthProportion (0.57f);
}

void GlowKeyboard::paint (juce::Graphics& g)
{
    // All the keys inside one rounded frame.
    juce::Path frame;
    frame.addRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (frame);
        g.setColour (colours::paper);
        g.fillAll();
        juce::MidiKeyboardComponent::paint (g);
    }
    g.setColour (colours::ink.withAlpha (0.25f));
    g.strokePath (frame, juce::PathStrokeType (0.8f));
}

void GlowKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                                  juce::Colour, juce::Colour)
{
    using namespace colours;
    juce::ColourGradient body (juce::Colours::white, area.getX(), area.getY(),
                               juce::Colour (0xffece7df), area.getX(), area.getBottom(), false);
    body.addColour (0.88, isOver ? juce::Colour (0xfff9f6f2) : juce::Colour (0xfff6f3ee));
    if (isDown)
    {
        body = juce::ColourGradient (juce::Colours::white, area.getX(), area.getY(),
                                     juce::Colour (0xfff3e2d9), area.getX(), area.getBottom(), false);
    }
    g.setGradientFill (body);
    g.fillRect (area);
    if (isDown)
    {
        g.setColour (coral);
        g.fillRect (area.withTop (area.getBottom() - 3.0f));
    }

    // A hairline between neighbours.
    g.setColour (ink.withAlpha (0.14f));
    g.fillRect (area.getRight() - 0.5f, area.getY(), 0.6f, area.getHeight());

    if (note % 12 == 0)
    {
        g.setFont (Fonts::sansMedium (8.0f, 0.08f));
        g.setColour (ink.withAlpha (0.5f));
        g.drawText ("C" + juce::String (note / 12 - 1), area.reduced (5.0f, 4.0f), juce::Justification::bottomLeft, false);
    }
}

void GlowKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    using namespace colours;
    const auto key = area.reduced (0.5f, 0.0f);
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle (key.translated (0.0f, 2.0f), 3.0f);

    juce::ColourGradient body (juce::Colour (isOver ? 0xff48423d : 0xff3a3531), key.getX(), key.getY(),
                               juce::Colour (0xff1f1c19), key.getX(), key.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (key, 3.0f);
    g.setColour (juce::Colour (0xff4a4440));
    g.fillRoundedRectangle (key.withTop (key.getBottom() - 3.0f), 2.0f);

    if (isDown)
    {
        g.setColour (coral.withAlpha (0.45f));
        g.fillRoundedRectangle (key, 3.0f);
    }
}

//==============================================================================
FreezeOrb::FreezeOrb (juce::AudioProcessorValueTreeState& state)
    : param (state.getParameter ("freeze")),
      attachment (*param, [this] (float v) { on = v > 0.5f; repaint(); })
{
    attachment.sendInitialUpdate();
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void FreezeOrb::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked())
        attachment.setValueAsCompleteGesture (on ? 0.0f : 1.0f);
}

void FreezeOrb::paint (juce::Graphics& g)
{
    using namespace colours;
    auto r = getLocalBounds().toFloat();
    auto labelArea = r.removeFromBottom (16.0f);
    const auto box = r.reduced (0.5f);
    const float hover = isMouseOver() ? 1.0f : 0.0f;
    const float t = (float) clock();

    g.setColour (on ? violet.withAlpha (0.15f) : frost.withAlpha (0.4f + 0.2f * hover));
    g.fillRoundedRectangle (box, 10.0f);
    g.setColour (on ? violet.withAlpha (0.6f) : ink.withAlpha (0.2f));
    g.drawRoundedRectangle (box, 10.0f, 1.0f);

    // A six-armed crystal, turning slowly while frozen.
    const auto c = box.getCentre();
    const float arm = 7.0f, spin = on ? t * 0.15f : 0.0f;
    g.setColour (on ? violet.darker (0.25f) : ink.withAlpha (0.7f));
    for (int i = 0; i < 6; ++i)
    {
        const float a = spin + (float) i * juce::MathConstants<float>::pi / 3.0f;
        g.drawLine (c.x, c.y, c.x + arm * std::cos (a), c.y + arm * std::sin (a), 1.1f);
    }

    g.setFont (Fonts::sansMedium (10.0f, 0.14f));
    g.setColour (ink.withAlpha (on ? 0.9f : 0.5f));
    g.drawText (on ? "FROZEN" : "FREEZE", labelArea, juce::Justification::centred, false);
}
} // namespace bouquet::ui
