#include "Tile.h"
#include "PluginProcessor.h"
#include "Theme.h"

using namespace theme;

Tile::Tile (PedalCuesProcessor& p, Look l) : look (l), processor (p)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    setRepaintsOnMouseActivity (true);
}

juce::Rectangle<int> Tile::playArea() const
{
    constexpr int d = 22;
    switch (look)
    {
        case Look::row:        return { getWidth() - d - 10, (getHeight() - d) / 2, d, d };
        case Look::footswitch:
        case Look::sweep:      return { getWidth() - d - 9, getHeight() - d - 9, d, d };
        case Look::screen:     return { getWidth() - d - 14, 14, d, d };
        case Look::stomp:
        case Look::utility:
        case Look::whammyMode: return { getWidth() - d - 8, 8, d, d };
    }
    return {};
}

static juce::Colour readableOn (juce::Colour c)
{
    return c.getPerceivedBrightness() > 0.62f ? juce::Colours::black : juce::Colours::white;
}

void Tile::paint (juce::Graphics& g)
{
    const auto hover = isMouseOver (true);
    const auto b = getLocalBounds().toFloat().reduced (3.0f);

    switch (look)
    {
        case Look::row:        paintRow (g, b, hover); break;
        case Look::screen:     paintScreen (g, b, hover); break;
        case Look::footswitch: paintFootswitch (g, b, hover); break;
        case Look::stomp:      paintStomp (g, b, hover); break;
        case Look::utility:    paintUtility (g, b, hover); break;
        case Look::whammyMode: paintWhammyMode (g, b, hover); break;
        case Look::sweep:      paintSweep (g, b, hover); break;
    }

    if (juce::Time::getMillisecondCounter() < flashUntil)
    {
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.fillRoundedRectangle (b, 10.0f);
    }

    if (makeCue)
        paintPlay (g, hover);
}

void Tile::paintRow (juce::Graphics& g, juce::Rectangle<float> b, bool hover)
{
    g.setColour (highlighted ? raised : (hover ? surfaceHi : surface));
    g.fillRoundedRectangle (b, 9.0f);

    if (highlighted)
    {
        g.setColour (colour.withAlpha (0.9f));
        g.drawRoundedRectangle (b.reduced (0.75f), 9.0f, 1.5f);
    }

    g.setColour (colour);
    g.fillRoundedRectangle (b.getX() + 8.0f, b.getY() + 9.0f, 4.0f, b.getHeight() - 18.0f, 2.0f);

    auto area = b.withTrimmedLeft (22.0f).withTrimmedRight (makeCue ? 40.0f : 10.0f);

    if (subtitle.isNotEmpty())
    {
        const juce::Font pillFont (font (11.0f, true));
        const auto w = juce::GlyphArrangement::getStringWidth (pillFont, subtitle) + 14.0f;
        const auto pill = area.removeFromRight (w).withSizeKeepingCentre (w, 20.0f);
        g.setColour (background.withAlpha (0.7f));
        g.fillRoundedRectangle (pill, 10.0f);
        g.setColour (dim);
        g.setFont (pillFont);
        g.drawText (subtitle, pill, juce::Justification::centred);
        area.removeFromRight (8.0f);
    }

    g.setColour (text);
    g.setFont (font (14.0f, true));
    g.drawFittedText (title, area.toNearestInt(), juce::Justification::centredLeft, 2, 0.85f);
}

void Tile::paintScreen (juce::Graphics& g, juce::Rectangle<float> b, bool hover)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1c2029), b.getTopLeft(),
                                             juce::Colour (0xff101216), b.getBottomLeft(), false));
    g.fillRoundedRectangle (b, 14.0f);
    g.setColour (hover ? colour.withAlpha (0.6f) : outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);

    // Coloured glow line along the top, like a lit display edge.
    g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.0f), b.getX() + 20.0f, 0.0f,
                                             colour.withAlpha (0.0f), b.getRight() - 20.0f, 0.0f, false));
    {
        juce::ColourGradient grad (colour.withAlpha (0.0f), b.getX() + 20.0f, 0.0f,
                                   colour.withAlpha (0.0f), b.getRight() - 20.0f, 0.0f, false);
        grad.addColour (0.5, colour);
        g.setGradientFill (grad);
    }
    g.fillRect (b.getX() + 20.0f, b.getY() + 1.0f, b.getWidth() - 40.0f, 2.0f);

    auto area = b.reduced (22.0f, 14.0f).withTrimmedRight (makeCue ? 30.0f : 0.0f);

    constexpr float chip = 20.0f, gap = 5.0f;
    if (! chips.empty())
    {
        auto chipArea = area.removeFromRight ((float) chips.size() * (chip + gap));
        chipArea = chipArea.withSizeKeepingCentre (chipArea.getWidth(), chip + 16.0f);

        g.setColour (dim);
        g.setFont (font (10.5f, true));
        g.drawText (chipsHeading, chipArea.removeFromTop (14.0f), juce::Justification::centredLeft);

        for (size_t i = 0; i < chips.size(); ++i)
        {
            const juce::Rectangle<float> c (chipArea.getX() + (float) i * (chip + gap), chipArea.getY() + 2.0f, chip, chip);
            g.setColour (chips[i]);
            g.fillRoundedRectangle (c, 5.0f);
            g.setColour (readableOn (chips[i]).withAlpha (0.8f));
            g.setFont (font (10.5f, true));
            g.drawText (numberedChips ? juce::String ((int) i + 1) : cues::qc::letter ((int) i % juce::jmax (1, chipLetters)), c, juce::Justification::centred);
        }
        area.removeFromRight (16.0f);
    }

    g.setColour (dim);
    g.setFont (font (10.5f, true));
    g.drawText (screenHeading, area.removeFromTop (16.0f), juce::Justification::centredLeft);

    auto bottom = area.removeFromBottom (20.0f);
    g.setColour (text);
    g.setFont (font (26.0f, true));
    g.drawFittedText (title, area.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);

    if (subtitle.isNotEmpty())
    {
        const juce::Font pillFont (font (11.5f, true));
        const auto w = juce::GlyphArrangement::getStringWidth (pillFont, subtitle) + 16.0f;
        const auto pill = bottom.removeFromLeft (w);
        g.setColour (colour.withAlpha (0.22f));
        g.fillRoundedRectangle (pill, 10.0f);
        g.setColour (colour.brighter (0.4f));
        g.setFont (pillFont);
        g.drawText (subtitle, pill, juce::Justification::centred);
        bottom.removeFromLeft (10.0f);
    }

    g.setColour (dim);
    g.setFont (font (12.0f));
    g.drawText (screenHint, bottom, juce::Justification::centredLeft);
}

void Tile::paintFootswitch (juce::Graphics& g, juce::Rectangle<float> b, bool hover)
{
    g.setGradientFill (juce::ColourGradient (surfaceHi, b.getTopLeft(), surface, b.getBottomLeft(), false));
    g.fillRoundedRectangle (b, 12.0f);
    g.setColour (hover ? colour.withAlpha (0.7f) : outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 12.0f, hover ? 1.5f : 1.0f);

    // Scene colour bar with soft glow.
    const auto tiny = b.getHeight() < 64.0f;   // the smallest window on a busy page: a thinner bar, closer to the top
    const juce::Rectangle<float> bar (b.getX() + 14.0f, b.getY() + (tiny ? 7.0f : 12.0f), b.getWidth() - (tiny ? 44.0f : 28.0f), tiny ? 3.0f : 5.0f);
    g.setColour (colour.withAlpha (0.18f));
    g.fillRoundedRectangle (bar.expanded (4.0f, 3.0f), 5.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (bar, 2.5f);

    // Short tiles (small windows, busy pages): the footswitch on the left and the name beside it, so the name never disappears.
    if (b.getHeight() < 110.0f)
    {
        auto body = b.withTrimmedTop (bar.getBottom() - b.getY() + (tiny ? 1.0f : 4.0f)).reduced (12.0f, tiny ? 3.0f : 6.0f);
        const auto d = juce::jlimit (22.0f, 36.0f, body.getHeight() - 4.0f);
        const auto sw = body.removeFromLeft (d).withSizeKeepingCentre (d, d);
        g.setColour (colour.withAlpha (hover ? 0.35f : 0.2f));
        g.fillEllipse (sw.expanded (4.0f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3e47), sw.getTopLeft(),
                                                 juce::Colour (0xff15171b), sw.getBottomRight(), false));
        g.fillEllipse (sw);
        g.setColour (colour);
        g.drawEllipse (sw.reduced (1.5f), 2.5f);
        g.setColour (text);
        g.setFont (font (d * 0.42f, true));
        g.drawText (badge, sw, juce::Justification::centred);

        body.removeFromLeft (10.0f);
        body.removeFromRight (16.0f);   // the play button
        const auto hasSub = subtitle.isNotEmpty() && body.getHeight() >= 34.0f;
        g.setColour (text);
        g.setFont (font (15.0f, true));
        g.drawFittedText (title, (hasSub ? body.removeFromTop (body.getHeight() * 0.55f) : body).toNearestInt(),
                          hasSub ? juce::Justification::bottomLeft : juce::Justification::centredLeft, 1, 0.8f);
        if (hasSub)
        {
            g.setColour (dim);
            g.setFont (font (11.0f));
            g.drawFittedText (subtitle, body.toNearestInt(), juce::Justification::topLeft, 1);
        }
        return;
    }

    // Footswitch with a coloured LED ring.
    const auto d = juce::jlimit (26.0f, 44.0f, b.getHeight() * 0.3f);
    const juce::Rectangle<float> sw (b.getCentreX() - d * 0.5f, b.getBottom() - 12.0f - d, d, d);
    g.setColour (colour.withAlpha (hover ? 0.35f : 0.2f));
    g.fillEllipse (sw.expanded (5.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3a3e47), sw.getTopLeft(),
                                             juce::Colour (0xff15171b), sw.getBottomRight(), false));
    g.fillEllipse (sw);
    g.setColour (colour);
    g.drawEllipse (sw.reduced (1.5f), 3.0f);
    g.setColour (text);
    g.setFont (font (d * 0.4f, true));
    g.drawText (badge, sw, juce::Justification::centred);

    auto textArea = juce::Rectangle<float>::leftTopRightBottom (b.getX() + 10.0f, bar.getBottom() + 8.0f,
                                                                b.getRight() - 10.0f, sw.getY() - 6.0f);
    if (subtitle.isNotEmpty())
    {
        g.setColour (dim);
        g.setFont (font (11.0f));
        g.drawFittedText (subtitle, textArea.removeFromBottom (16.0f).toNearestInt(), juce::Justification::centred, 1);
    }

    g.setColour (text);
    g.setFont (font (juce::jlimit (14.0f, 20.0f, b.getHeight() * 0.13f), true));
    g.drawFittedText (title, textArea.toNearestInt(), juce::Justification::centred, 2, 0.8f);
}

void Tile::paintStomp (juce::Graphics& g, juce::Rectangle<float> b, bool hover)
{
    g.setColour (hover ? surfaceHi : surface);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    const auto ledColour = active ? ledGreen : juce::Colour (0xff4a4f59);
    const juce::Rectangle<float> led (b.getX() + 11.0f, b.getY() + 11.0f, 8.0f, 8.0f);
    if (active)
    {
        g.setColour (ledColour.withAlpha (0.25f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (ledColour);
    g.fillEllipse (led);

    g.setColour (dim);
    g.setFont (font (10.5f, true));
    g.drawText (subtitle, led.withX (led.getRight() + 6.0f).withWidth (60.0f).expanded (0.0f, 3.0f),
                juce::Justification::centredLeft);

    g.setColour (text);
    g.setFont (font (13.0f, true));
    g.drawFittedText (title, b.reduced (10.0f, 6.0f).withTrimmedTop (20.0f).toNearestInt(),
                      juce::Justification::centredLeft, 2, 0.8f);
}

void Tile::paintUtility (juce::Graphics& g, juce::Rectangle<float> b, bool hover)
{
    g.setColour (hover ? surfaceHi : surface);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    auto area = b.reduced (10.0f, 8.0f);
    if (badge.isEmpty())
    {
        // Compact: a colour bar instead of the badge.
        g.setColour (colour);
        g.fillRoundedRectangle (area.removeFromLeft (4.0f).reduced (0.0f, 2.0f), 2.0f);
    }
    else
    {
        const auto icon = area.removeFromLeft (32.0f).withSizeKeepingCentre (32.0f, 32.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (icon, 8.0f);
        g.setColour (readableOn (colour));
        g.setFont (font (12.0f, true));
        g.drawText (badge, icon, juce::Justification::centred);
    }

    area.removeFromLeft (10.0f);
    area.removeFromRight (makeCue ? 22.0f : 0.0f);
    g.setColour (text);
    auto titleFont = font (13.5f, true);   // shrink a long title (down to 11 pt) before squeezing or cutting it
    const auto titleW = juce::GlyphArrangement::getStringWidth (titleFont, title);
    if (titleW > area.getWidth())
        titleFont = font (juce::jmax (11.0f, 13.5f * area.getWidth() / titleW), true);
    g.setFont (titleFont);
    g.drawFittedText (title, area.removeFromTop (area.getHeight() * 0.55f).toNearestInt(), juce::Justification::bottomLeft, 1);
    g.setColour (dim);
    g.setFont (font (11.0f));
    g.drawFittedText (subtitle, area.toNearestInt(), juce::Justification::topLeft, 1);
}

void Tile::paintWhammyMode (juce::Graphics& g, juce::Rectangle<float> b, bool hover)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff17171a), b.getTopLeft(),
                                             juce::Colour (0xff0c0c0e), b.getBottomLeft(), false));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (hover ? whammyRed.withAlpha (0.8f) : juce::Colour (0xff2a2a2f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, hover ? 1.5f : 1.0f);

    const juce::Rectangle<float> led (b.getX() + 11.0f, b.getY() + 11.0f, 9.0f, 9.0f);
    if (active)
    {
        g.setColour (whammyRed.withAlpha (0.35f));
        g.fillEllipse (led.expanded (4.0f));
        g.setColour (juce::Colour (0xffff4d5e));
    }
    else
    {
        g.setColour (juce::Colour (0xff3b1a1e));
    }
    g.fillEllipse (led);

    g.setColour (colour);
    g.fillRoundedRectangle (b.getX() + 10.0f, b.getBottom() - 6.0f, b.getWidth() - 20.0f, 2.5f, 1.25f);

    auto area = b.reduced (10.0f, 8.0f).withTrimmedTop (16.0f).withTrimmedBottom (4.0f);
    g.setColour (dim);
    g.setFont (font (10.5f, true));
    g.drawText (subtitle, area.removeFromBottom (14.0f), juce::Justification::bottomLeft);

    g.setColour (active ? text : text.withAlpha (0.6f));
    g.setFont (font (14.0f, true));
    g.drawFittedText (title, area.toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);
}

void Tile::paintSweep (juce::Graphics& g, juce::Rectangle<float> b, bool hover)
{
    g.setColour (hover ? surfaceHi : surface);
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (hover ? colour.withAlpha (0.7f) : outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    auto area = b.reduced (10.0f);
    const auto graph = area.removeFromTop (juce::jmax (30.0f, area.getHeight() - 38.0f));

    g.setColour (background);
    g.fillRoundedRectangle (graph, 6.0f);
    g.setColour (outline);
    for (auto frac : { 0.25f, 0.5f, 0.75f })
        g.drawHorizontalLine ((int) (graph.getY() + graph.getHeight() * frac), graph.getX() + 4.0f, graph.getRight() - 4.0f);

    if (! curve.empty())
    {
        const auto plot = graph.reduced (6.0f, 6.0f);
        auto toX = [&] (float x) { return plot.getX() + x * plot.getWidth(); };
        auto toY = [&] (float y) { return plot.getBottom() - y * plot.getHeight(); };

        juce::Path line;
        line.startNewSubPath (toX (0.0f), toY (curve.front().y));
        auto lastY = curve.front().y;
        for (const auto& p : curve)
        {
            line.lineTo (toX (p.x), toY (lastY));
            line.lineTo (toX (p.x), toY (p.y));
            lastY = p.y;
        }
        line.lineTo (toX (1.0f), toY (lastY));

        juce::Path fill (line);
        fill.lineTo (toX (1.0f), plot.getBottom());
        fill.lineTo (toX (0.0f), plot.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (colour.withAlpha (0.35f), 0.0f, plot.getY(),
                                                 colour.withAlpha (0.02f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (colour);
        g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    area.removeFromTop (6.0f);
    area.removeFromRight (makeCue ? 26.0f : 0.0f);
    g.setColour (text);
    g.setFont (font (13.0f, true));
    g.drawFittedText (title, area.removeFromTop (16.0f).toNearestInt(), juce::Justification::centredLeft, 1);
    g.setColour (dim);
    g.setFont (font (11.0f));
    g.drawFittedText (subtitle, area.toNearestInt(), juce::Justification::centredLeft, 1);
}

void Tile::paintPlay (juce::Graphics& g, bool hover)
{
    const auto area = playArea().toFloat();
    const auto alpha = overPlay ? 1.0f : (hover ? 0.75f : 0.3f);

    g.setColour ((overPlay ? accent : background).withAlpha (overPlay ? 1.0f : 0.55f * alpha + 0.2f));
    g.fillEllipse (area);

    const auto t = area.reduced (7.0f).translated (1.0f, 0.0f);
    juce::Path triangle;
    triangle.addTriangle (t.getX(), t.getY(), t.getX(), t.getBottom(), t.getRight(), t.getCentreY());
    g.setColour ((overPlay ? juce::Colours::black : text).withAlpha (overPlay ? 1.0f : alpha));
    g.fillPath (triangle);
}

//==============================================================================
void Tile::mouseDown (const juce::MouseEvent& e)
{
    dragStarted = false;

    if (e.mods.isPopupMenu() && onContextMenu)
        onContextMenu();
}

void Tile::mouseDrag (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || dragStarted || ! makeCue || e.getDistanceFromDragStart() < 6)
        return;

    dragStarted = true;

    const auto file = cues::writeMidiFile (makeCue(), processor.getHostBpm());
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (file.getFullPathName()), false, this);
}

void Tile::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() || dragStarted)
        return;

    if (makeCue && playArea().contains (e.getPosition()))
        sendNow();
    else if (onClick)
        onClick();
}

void Tile::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! playArea().contains (e.getPosition()) && onDoubleClick)
        onDoubleClick();
}

void Tile::mouseMove (const juce::MouseEvent& e)
{
    const auto over = makeCue && playArea().contains (e.getPosition());
    if (over != overPlay)
    {
        overPlay = over;
        setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::DraggingHandCursor);
        repaint();
    }
}

void Tile::mouseExit (const juce::MouseEvent&)
{
    overPlay = false;
    repaint();
}

void Tile::sendNow()
{
    if (! makeCue)
        return;

    processor.preview (makeCue());

    flashUntil = juce::Time::getMillisecondCounter() + 180;
    repaint();

    juce::Component::SafePointer<Tile> safe (this);
    juce::Timer::callAfterDelay (200, [safe]
    {
        if (safe != nullptr)
            safe->repaint();
    });
}
