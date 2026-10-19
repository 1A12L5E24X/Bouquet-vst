#include "AuraPad.h"

namespace bouquet::ui
{
AuraPad::AuraPad (juce::AudioProcessorValueTreeState& state, GranularEngine& e)
    : engine (e),
      xAttach (*state.getParameter ("auraX"), [this] (float v) { auraX = v; }),
      yAttach (*state.getParameter ("auraY"), [this] (float v) { auraY = v; })
{
    xAttach.sendInitialUpdate();
    yAttach.sendInitialUpdate();
    shownX = auraX;
    shownY = auraY;
    motes.reserve (500);
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

juce::Rectangle<float> AuraPad::field() const
{
    return getLocalBounds().toFloat().reduced (1.0f);
}

void AuraPad::tick (float dt)
{
    using namespace colours;

    const int n = engine.popEvents (incoming.data(), (int) incoming.size());
    for (int i = 0; i < n; ++i)
    {
        const auto& ev = incoming[(size_t) i];
        if (motes.size() >= 480)
            motes.erase (motes.begin());

        juce::Colour c = ev.harmonic ? (ev.pitch > 0.0f ? butter : peach)
                                     : (ev.reverse ? rose : (ev.pitch > 0.5f ? sky : lilac));
        if (std::abs (ev.pitch) > 0.5f && ! ev.harmonic && ! ev.reverse)
            c = c.interpolatedWith (aqua, 0.5f);

        Mote m;
        m.x = 0.03f + 0.94f * std::pow (std::clamp (ev.time01, 0.0f, 1.0f), 0.55f);
        m.y = 0.5f - std::clamp (ev.pitch / 56.0f, -0.46f, 0.46f) + (random.nextFloat() - 0.5f) * 0.05f;
        m.vx = (ev.reverse ? -1.0f : 1.0f) * (0.01f + random.nextFloat() * 0.02f) + ev.pan * 0.01f;
        m.vy = (random.nextFloat() - 0.5f) * 0.02f;
        m.age = 0.0f;
        m.life = std::clamp (ev.duration * 1.5f, 0.35f, 3.5f);
        m.size = std::clamp (6.0f + 18.0f * std::sqrt (ev.duration) * std::min (1.0f, ev.amp * 1.5f + 0.3f), 6.0f, 26.0f);
        m.colour = c;
        motes.push_back (m);
    }

    for (auto& m : motes)
    {
        m.age += dt;
        m.x += m.vx * dt;
        m.y += m.vy * dt;
    }
    motes.erase (std::remove_if (motes.begin(), motes.end(), [] (const Mote& m) { return m.age >= m.life; }), motes.end());

    engine.readMemory (memory);

    // The cursorPos eases towards the parameter, so automation glides visibly too.
    shownX += (auraX - shownX) * 0.25f;
    shownY += (auraY - shownY) * 0.25f;
    for (size_t i = trail.size() - 1; i > 0; --i)
        trail[i] = trail[i - 1];
    trail[0] = { shownX, shownY };
}

void AuraPad::paint (juce::Graphics& g)
{
    using namespace colours;
    const auto f = field();
    const float t = (float) clock();
    const float wet = std::min (1.0f, engine.wetLevel.load() * 1.8f);
    const float breath = engine.breath.load();
    const float corner = 18.0f;

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (f, corner);
        g.reduceClipRegion (clip);

        const juce::Point<float> cursorPos (f.getX() + shownX * f.getWidth(), f.getY() + (1.0f - shownY) * f.getHeight());

        // A slow inner aurora that follows the cursorPos and swells with the sound.
        drawGlow (g, cursorPos, f.getHeight() * (0.75f + 0.25f * wet + 0.05f * breath), lilac.withAlpha (0.24f + 0.2f * wet));

        // Memory: a soft ribbon of what the plugin has heard, newest on the left.
        {
            const float midY = f.getCentreY();
            const int bins = (int) memory.size();
            juce::Path upper;
            float smoothed = 0.0f;
            for (int i = 0; i < bins; ++i)
            {
                const float x = f.getX() + (0.03f + 0.94f * std::pow ((float) i / (float) (bins - 1), 0.55f)) * f.getWidth();
                smoothed += (std::min (1.0f, std::sqrt (memory[(size_t) i])) - smoothed) * 0.5f;
                const float h = smoothed * f.getHeight() * 0.3f + 0.6f;
                if (i == 0) upper.startNewSubPath (x, midY - h);
                else        upper.lineTo (x, midY - h);
            }
            // Mirror the upper edge to close the ribbon.
            juce::Path lower (upper);
            lower.applyTransform (juce::AffineTransform::verticalFlip (midY * 2.0f));
            juce::ColourGradient fill (violet.withAlpha (0.35f), f.getX(), midY,
                                       sky.withAlpha (0.05f), f.getRight(), midY, false);
            g.setGradientFill (fill);
            g.strokePath (upper, juce::PathStrokeType (1.0f));
            g.strokePath (lower, juce::PathStrokeType (1.0f));
            g.setGradientFill (juce::ColourGradient (lilac.withAlpha (0.14f), f.getX(), midY,
                                                     sky.withAlpha (0.0f), f.getRight(), midY, false));
            for (auto* edge : { &upper, &lower })
            {
                juce::Path area (*edge);
                area.lineTo (f.getRight(), midY);
                area.lineTo (f.getX(), midY);
                area.closeSubPath();
                g.fillPath (area);
            }
        }

        // Pitch guides: the octaves above and below.
        g.setColour (ink.withAlpha (0.045f));
        for (float st : { -24.0f, -12.0f, 12.0f, 24.0f })
        {
            const float y = f.getY() + (0.5f - st / 56.0f) * f.getHeight();
            for (float x = f.getX() + 8.0f; x < f.getRight() - 8.0f; x += 6.0f)
                g.fillRect (x, y, 2.0f, 0.8f);
        }

        // Grains.
        for (const auto& m : motes)
        {
            const float life01 = m.age / m.life;
            const float fadeIn = std::min (1.0f, life01 * 8.0f);
            const float a = fadeIn * (1.0f - life01) * (1.0f - life01);
            const juce::Point<float> p (f.getX() + m.x * f.getWidth(), f.getY() + m.y * f.getHeight());
            drawGlow (g, p, m.size * 1.4f, m.colour.withAlpha (0.85f * a));
            g.setColour (ink.withAlpha (0.55f * a));
            g.fillEllipse (juce::Rectangle<float> (1.6f, 1.6f).withCentre (p));
        }

        // The cursorPos and its comet tail.
        for (size_t i = trail.size() - 1; i > 0; --i)
        {
            const auto& p = trail[i];
            const float k = 1.0f - (float) i / (float) trail.size();
            drawGlow (g, { f.getX() + p.x * f.getWidth(), f.getY() + (1.0f - p.y) * f.getHeight() },
                      10.0f * k + 4.0f, lavender.withAlpha (0.10f * k));
        }
        const float ring = 13.0f + 3.0f * std::sin (t * 1.6f) + 10.0f * wet;
        drawGlow (g, cursorPos, ring * 2.4f, lilac.withAlpha (0.5f + 0.3f * wet));
        g.setColour (violet.withAlpha (0.45f + 0.4f * (dragging ? 1.0f : 0.0f)));
        g.drawEllipse (juce::Rectangle<float> (ring * 1.4f, ring * 1.4f).withCentre (cursorPos), 1.0f);
        g.setColour (frost);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (cursorPos));
        g.setColour (violet);
        g.drawEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (cursorPos), 1.3f);
    }

    g.setColour (ink.withAlpha (0.09f));
    g.drawRoundedRectangle (f, corner, 1.0f);

    // Edge words.
    g.setFont (Fonts::serifItalic (13.0f));
    g.setColour (ink.withAlpha (0.45f));
    const auto inner = f.reduced (14.0f, 8.0f);
    g.drawText ("dust",  inner, juce::Justification::centredLeft, false);
    g.drawText ("smear", inner, juce::Justification::centredRight, false);
    g.drawText ("vast",  inner, juce::Justification::centredTop, false);
    g.drawText ("close", inner, juce::Justification::centredBottom, false);
}

void AuraPad::paintBackdrop (juce::Graphics& g) const
{
    using namespace colours;
    const auto f = field() + getPosition().toFloat();
    const float t = (float) clock();
    const float wet = std::min (1.0f, engine.wetLevel.load() * 1.8f);
    constexpr float corner = 18.0f;

    juce::Graphics::ScopedSaveState save (g);
    juce::Path clip;
    clip.addRoundedRectangle (f, corner);
    g.reduceClipRegion (clip);

    g.setColour (frost.withAlpha (0.6f));
    g.fillRect (f);
    drawGlow (g, { f.getX() + f.getWidth() * (0.5f + 0.3f * std::sin (t * 0.13f)), f.getBottom() },
              f.getWidth() * 0.5f, sky.withAlpha (0.2f));
    drawGlow (g, { f.getX() + f.getWidth() * (0.5f + 0.35f * std::cos (t * 0.09f)), f.getY() },
              f.getWidth() * 0.4f, butter.withAlpha (0.12f + 0.08f * wet));
}

void AuraPad::setFromMouse (juce::Point<float> p)
{
    const auto f = field();
    const float x = std::clamp ((p.x - f.getX()) / f.getWidth(), 0.0f, 1.0f);
    const float y = std::clamp (1.0f - (p.y - f.getY()) / f.getHeight(), 0.0f, 1.0f);
    xAttach.setValueAsPartOfGesture (x);
    yAttach.setValueAsPartOfGesture (y);
}

void AuraPad::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    xAttach.beginGesture();
    yAttach.beginGesture();
    setFromMouse (e.position);
}

void AuraPad::mouseDrag (const juce::MouseEvent& e)
{
    setFromMouse (e.position);
}

void AuraPad::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    xAttach.endGesture();
    yAttach.endGesture();
}

void AuraPad::mouseDoubleClick (const juce::MouseEvent&)
{
    xAttach.setValueAsCompleteGesture (0.5f);
    yAttach.setValueAsCompleteGesture (0.5f);
}
} // namespace bouquet::ui
