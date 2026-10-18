#include "Widgets.h"

namespace murmur::ui
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
    const auto text = touching ? param->getCurrentValueAsText() + " " + param->getLabel() : label;
    g.setFont (Fonts::sans (10.5f, 0.04f));
    g.setColour (colours::ink.withAlpha (touching ? 0.9f : 0.45f));
    g.drawText (text.trim(), getLocalBounds().removeFromBottom (16), juce::Justification::centred, false);
}

//==============================================================================
GlowItem::GlowItem (const juce::String& t, Style s, juce::Colour a, juce::Colour b)
    : text (t), style (s), accentA (a), accentB (b),
      font (s == Style::text ? Fonts::serif (19.0f) : Fonts::serif (15.0f))
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
        const auto pill = bounds.reduced (2.0f);
        const float radius = pill.getHeight() * 0.5f;

        if (selectLight > 0.01f)
        {
            // Light pooling outward from the chosen pill.
            drawGlow (g, pill.getCentre(), pill.getWidth() * 0.75f, accentA.withAlpha (0.16f * selectLight * breathe));
            juce::ColourGradient grad (accentA.withAlpha (0.32f * selectLight), pill.getX(), pill.getY(),
                                       accentB.withAlpha (0.22f * selectLight), pill.getRight(), pill.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (pill, radius);
        }

        g.setColour (ink.withAlpha (0.025f + 0.03f * hoverLight));
        g.fillRoundedRectangle (pill, radius);
        g.setColour (selectLight > 0.5f ? accentB.interpolatedWith (ink, 0.3f).withAlpha (0.55f)
                                        : ink.withAlpha (0.10f + 0.12f * hoverLight));
        g.drawRoundedRectangle (pill, radius, 1.0f);

        g.setFont (font);
        g.setColour (ink.withAlpha (0.55f + 0.3f * hoverLight + 0.45f * selectLight));
        g.drawText (text, pill, juce::Justification::centred, false);
        return;
    }

    // Text style: a word floating in its own light.
    const float textW = juce::GlyphArrangement::getStringWidth (font, text);
    float textX = 0.0f;
    if (showDot)
    {
        const juce::Point<float> dot (7.0f, bounds.getCentreY());
        if (selectLight > 0.01f)
            drawGlow (g, dot, 14.0f, accentA.withAlpha (0.7f * selectLight * breathe));
        g.setColour (ink.withAlpha (0.15f + 0.85f * selectLight));
        g.fillEllipse (juce::Rectangle<float> (4.0f + selectLight, 4.0f + selectLight).withCentre (dot));
        textX = 20.0f;
    }

    if (selectLight > 0.01f)
        drawGlow (g, { textX + textW * 0.5f, bounds.getCentreY() }, textW * 0.75f + 10.0f,
                  accentA.withAlpha (0.13f * selectLight * breathe));

    g.setFont (font);
    g.setColour (ink.withAlpha (0.42f + 0.33f * hoverLight + 0.58f * selectLight));
    g.drawText (text, bounds.withTrimmedLeft (textX), showDot ? juce::Justification::centredLeft
                                                              : juce::Justification::centred, false);
}

//==============================================================================
GlowKeyboard::GlowKeyboard (juce::MidiKeyboardState& keyState)
    : juce::MidiKeyboardComponent (keyState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    // The background stays clear; the editor paints the glass behind the keys.
    setColour (whiteNoteColourId, juce::Colours::transparentBlack);
    setColour (keySeparatorLineColourId, juce::Colours::transparentBlack);
    setColour (shadowColourId, juce::Colours::transparentBlack);
    setAvailableRange (36, 107);
    setScrollButtonsVisible (false);
    setWantsKeyboardFocus (false);
    setBlackNoteLengthProportion (0.6f);
    setBlackNoteWidthProportion (0.55f);
}

void GlowKeyboard::drawWhiteNote (int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver,
                                  juce::Colour, juce::Colour)
{
    using namespace colours;
    auto key = area.reduced (1.0f, 0.0f);
    juce::ColourGradient body (ink.withAlpha (0.035f), key.getX(), key.getY(),
                               ink.withAlpha (isOver ? 0.10f : 0.065f), key.getX(), key.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (key, 5.0f);

    if (isDown)
    {
        drawGlow (g, { key.getCentreX(), key.getBottom() - 10.0f }, key.getWidth() * 1.6f, coral.withAlpha (0.65f));
        juce::ColourGradient lit (peach.withAlpha (0.05f), key.getX(), key.getY(),
                                  coral.withAlpha (0.55f), key.getX(), key.getBottom(), false);
        g.setGradientFill (lit);
        g.fillRoundedRectangle (key, 5.0f);
    }

    if (note % 12 == 0)
    {
        g.setFont (Fonts::sans (9.0f, 0.05f));
        g.setColour (ink.withAlpha (0.3f));
        g.drawText ("C" + juce::String (note / 12 - 1), key.removeFromBottom (16.0f), juce::Justification::centred, false);
    }
}

void GlowKeyboard::drawBlackNote (int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown, bool isOver, juce::Colour)
{
    using namespace colours;
    const auto key = area.reduced (0.5f, 0.0f).withTrimmedBottom (1.0f);
    g.setColour (nightDeep.withAlpha (0.92f));
    g.fillRoundedRectangle (key, 4.0f);
    g.setColour (ink.withAlpha (isOver ? 0.16f : 0.08f));
    g.drawRoundedRectangle (key, 4.0f, 0.8f);

    if (isDown)
    {
        drawGlow (g, key.getCentre(), key.getWidth() * 2.0f, rose.withAlpha (0.6f));
        g.setColour (rose.withAlpha (0.5f));
        g.fillRoundedRectangle (key, 4.0f);
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
    const float size = std::min (r.getWidth(), r.getHeight()) - 6.0f;
    const auto c = r.getCentre();
    const float t = (float) clock();
    const float pulse = on ? 0.8f + 0.2f * std::sin (t * 2.2f) : 0.0f;
    const float hover = isMouseOver() ? 1.0f : 0.0f;

    if (on)
        drawGlow (g, c, size * 1.2f, aqua.withAlpha (0.35f * pulse));

    g.setColour (ink.withAlpha (on ? 0.10f : 0.03f + 0.03f * hover));
    g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (c));
    g.setColour (on ? aqua.withAlpha (0.8f) : ink.withAlpha (0.18f + 0.15f * hover));
    g.drawEllipse (juce::Rectangle<float> (size, size).withCentre (c), 1.0f);

    // A six-armed crystal.
    const float arm = size * 0.26f;
    g.setColour (on ? ink : ink.withAlpha (0.5f + 0.3f * hover));
    const float spin = on ? t * 0.15f : 0.0f;
    for (int i = 0; i < 6; ++i)
    {
        const float a = spin + (float) i * juce::MathConstants<float>::pi / 3.0f;
        g.drawLine (c.x, c.y, c.x + arm * std::cos (a), c.y + arm * std::sin (a), 1.1f);
    }

    g.setFont (Fonts::sans (10.5f, 0.04f));
    g.setColour (ink.withAlpha (on ? 0.9f : 0.45f));
    g.drawText (on ? "frozen" : "freeze", labelArea, juce::Justification::centred, false);
}
} // namespace murmur::ui
