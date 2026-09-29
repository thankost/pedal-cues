#include "Tile.h"
#include "PluginProcessor.h"

Tile::Tile (PedalCuesProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
}

juce::Rectangle<int> Tile::playArea() const
{
    return { getWidth() - 22, 3, 19, 19 };
}

void Tile::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (2.0f);
    const auto flashing = juce::Time::getMillisecondCounter() < flashUntil;

    auto fill = colour;
    if (flashing)
        fill = fill.brighter (0.6f);

    g.setColour (fill.withMultipliedAlpha (highlighted || flashing ? 1.0f : 0.82f));
    g.fillRoundedRectangle (bounds, 6.0f);

    if (highlighted)
    {
        g.setColour (juce::Colours::white);
        g.drawRoundedRectangle (bounds.reduced (1.0f), 6.0f, 2.0f);
    }

    const auto textColour = colour.getPerceivedBrightness() > 0.62f ? juce::Colours::black : juce::Colours::white;
    auto text = getLocalBounds().reduced (8, 4).withTrimmedRight (makeCue ? 16 : 0);

    if (subtitle.isNotEmpty())
    {
        g.setColour (textColour.withAlpha (0.75f));
        g.setFont (juce::FontOptions (11.0f));
        g.drawFittedText (subtitle, text.removeFromBottom (14), juce::Justification::bottomLeft, 1);
    }

    g.setColour (textColour);
    g.setFont (juce::FontOptions (14.5f, juce::Font::bold));
    g.drawFittedText (title, text, juce::Justification::topLeft, 2, 0.8f);

    if (makeCue)
    {
        const auto area = playArea().toFloat().reduced (4.0f);
        juce::Path triangle;
        triangle.addTriangle (area.getX(), area.getY(), area.getX(), area.getBottom(), area.getRight(), area.getCentreY());
        g.setColour (textColour.withAlpha (overPlay ? 1.0f : 0.45f));
        g.fillPath (triangle);
    }
}

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
