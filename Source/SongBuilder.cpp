#include "EditorCommon.h"
#include "DeviceTemplates.h"
#include "Modellers.h"
#include "Songs.h"
#include "State.h"

#include <map>

using namespace theme;

// Song Builder (beta), in its own window: sections along the top, tracks the user names, and cues dragged in from the
// Amps & Modelers / Effects & Pedals tabs (or .mid clips from the DAW). The whole song, or the ticked tracks, or one track,
// drags into the DAW as a MIDI file with the tempo map, time signatures and markers, or exports for a backing-track player.
namespace ui
{
namespace
{
const juce::Colour builderViolet { 0xff9b87f5 };

juce::Colour colourOf (const juce::ValueTree& node, juce::Colour fallback)
{
    const auto stored = node[IDs::colour].toString();
    return stored.isNotEmpty() ? juce::Colour::fromString (stored) : fallback;
}

juce::String meterText (const juce::ValueTree& s)
{
    return juce::String ((int) s.getProperty (IDs::timeNum, 4)) + "/" + juce::String ((int) s.getProperty (IDs::timeDen, 4));
}

juce::String bpmText (double bpm)
{
    return juce::String (bpm, std::abs (bpm - std::round (bpm)) < 0.005 ? 0 : 2);
}

juce::String durationText (double seconds)
{
    const auto s = juce::roundToInt (seconds);
    return juce::String (s / 60) + ":" + juce::String (s % 60).paddedLeft ('0', 2);
}

// The cues keep the channel they were dropped with: "Own channel (2)", "Own channels (1, 2)", or "Own channels" (no cues).
juce::String ownChannelsText (const juce::ValueTree& song, const juce::ValueTree& track)
{
    juce::StringArray used;
    for (auto c : songs::channelsUsed (song, track))
        used.add (juce::String (c));
    if (used.isEmpty())
        return "Own channels";
    return (used.size() == 1 ? "Own channel (" : "Own channels (") + used.joinIntoString (", ") + ")";
}

// "Channel 3", or the own channels.
juce::String channelText (const juce::ValueTree& song, const juce::ValueTree& track)
{
    if (const auto ch = songs::trackChannel (track); ch > 0)
        return "Channel " + juce::String (ch);
    return ownChannelsText (song, track);
}

juce::PopupMenu channelMenu (int current, int baseId)
{
    juce::PopupMenu m;
    m.addItem (baseId, "Own channels (leave the cues as they are)", true, current == 0);
    m.addSeparator();
    for (int ch = 1; ch <= 16; ++ch)
        m.addItem (baseId + ch, "Channel " + juce::String (ch), true, current == ch);
    return m;
}

bool isMidiFile (const juce::String& path)
{
    return path.endsWithIgnoreCase (".mid") || path.endsWithIgnoreCase (".midi") || path.endsWithIgnoreCase (".smf");
}

void editSectionDialog (juce::ValueTree section)
{
    auto* w = new juce::AlertWindow ("Edit section", "Its name, length, time signature and tempo. Cues after it move with it.",
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", section[IDs::name].toString(), "Name");
    w->addTextEditor ("bars", section[IDs::bars].toString(), "Bars");
    w->addTextEditor ("meter", meterText (section), "Time signature");
    w->addTextEditor ("bpm", bpmText ((double) section.getProperty (IDs::bpm, 120.0)), "Tempo (BPM)");
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, section] (int result) mutable
    {
        if (result != 1)
            return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isNotEmpty())
            section.setProperty (IDs::name, name, nullptr);
        const auto bars = w->getTextEditorContents ("bars").getIntValue();
        if (bars > 0)
            section.setProperty (IDs::bars, juce::jlimit (1, 999, bars), nullptr);
        const auto meter = juce::StringArray::fromTokens (w->getTextEditorContents ("meter"), "/", "");
        if (meter.size() == 2 && meter[0].getIntValue() > 0 && meter[1].getIntValue() > 0)
        {
            const auto fresh = songs::createSection ("x", 1, meter[0].getIntValue(), meter[1].getIntValue(), 120.0);   // clamps
            section.setProperty (IDs::timeNum, fresh[IDs::timeNum], nullptr);
            section.setProperty (IDs::timeDen, fresh[IDs::timeDen], nullptr);
        }
        const auto bpm = w->getTextEditorContents ("bpm").getDoubleValue();
        if (bpm > 0.0)
            section.setProperty (IDs::bpm, juce::jlimit (20.0, 400.0, bpm), nullptr);
    }), true);
}

constexpr int rulerHeight = 42, barsHeight = 18, trackHeight = 58, blockHeight = 23, newLaneHeight = 44;
int trackTop (int index) { return rulerHeight + barsHeight + index * trackHeight; }

//==============================================================================
// A placed cue: click to select it (Shift adds), drag it along or onto another track, right-click for the menu.
struct CueBlock final : public juce::Component, public juce::SettableTooltipClient
{
    juce::ValueTree cue;
    bool selected = false;
    std::function<void()> onMenu;
    std::function<void (bool add)> onSelect;
    int grabX = 0;   // where the mouse took it, so a drag keeps that spot under the mouse

    void paint (juce::Graphics& g) override
    {
        const auto colour = colourOf (cue, dim);
        auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (colour.withAlpha (isMouseOver() ? 0.55f : 0.38f));
        g.fillRoundedRectangle (b, 5.0f);
        g.setColour (colour);
        g.drawRoundedRectangle (b, 5.0f, 1.0f);
        g.fillRect (b.removeFromLeft (3.0f));
        if (selected)
        {
            g.setColour (juce::Colours::white);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 5.0f, 2.0f);
        }
        g.setColour (text);
        g.setFont (font (12.0f, true));
        g.drawText (cue[IDs::name].toString(), getLocalBounds().reduced (7, 0), juce::Justification::centredLeft, true);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        dragged = false;
        grabX = e.x;
        if (auto* p = getParentComponent())
            p->grabKeyboardFocus();   // for copy / paste / delete
        if (e.mods.isPopupMenu() && onMenu)
            onMenu();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! dragged && ! e.mods.isPopupMenu() && onSelect)
            onSelect (e.mods.isShiftDown() || e.mods.isCommandDown());
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() || dragged || e.getDistanceFromDragStart() < 4)
            return;
        dragged = true;
        if (auto* c = juce::DragAndDropContainer::findParentDragContainerFor (this))
            c->startDragging ("pedalcues.cue", this, juce::ScaledImage (createComponentSnapshot (getLocalBounds(), true, 2.0f), 2.0f));
    }

private:
    bool dragged = false;
};

//==============================================================================
// A track's name on the left: tick it to export it, drag it to the DAW for that track alone, right-click for the rest.
struct TrackLabel final : public juce::Component, public juce::SettableTooltipClient
{
    juce::ValueTree song, track;
    std::function<void()> onMenu;

    TrackLabel()
    {
        setTooltip ("Tick to include this track in Drag song and Export. Drag the name to your DAW for this track alone. "
                    "Right-click to set its MIDI channel, duplicate it for another player, rename, recolour, move or delete it.");
    }

    juce::Rectangle<int> tickArea() const { return getLocalBounds().reduced (0, 4).removeFromRight (30); }

    void paint (juce::Graphics& g) override
    {
        auto row = getLocalBounds().reduced (0, 4);
        g.setColour (isMouseOver() ? raised : surfaceHi);
        g.fillRoundedRectangle (row.toFloat(), 6.0f);
        g.setColour (colourOf (track, builderViolet));
        g.fillRoundedRectangle (row.removeFromLeft (4).toFloat(), 2.0f);

        const auto on = (bool) track.getProperty (IDs::include, true);
        auto tick = tickArea().withSizeKeepingCentre (16, 16).toFloat();
        g.setColour (on ? builderViolet : outline.brighter (0.3f));
        if (on)
            g.fillRoundedRectangle (tick, 3.0f);
        else
            g.drawRoundedRectangle (tick, 3.0f, 1.5f);
        if (on)
        {
            juce::Path p;
            p.startNewSubPath (tick.getX() + 3.5f, tick.getCentreY());
            p.lineTo (tick.getX() + 7.0f, tick.getBottom() - 4.0f);
            p.lineTo (tick.getRight() - 3.5f, tick.getY() + 4.0f);
            g.setColour (juce::Colours::black);
            g.strokePath (p, juce::PathStrokeType (2.0f));
        }

        auto words = row.reduced (10, 6).withTrimmedRight (26);
        g.setColour (on ? text : dim);
        g.setFont (font (13.0f, true));
        g.drawText (track[IDs::name].toString(), words.removeFromTop (words.getHeight() / 2 + 2), juce::Justification::bottomLeft, true);
        g.setColour (songs::trackChannel (track) > 0 ? builderViolet : dim);
        g.setFont (font (11.0f));
        g.drawText (channelText (song, track), words, juce::Justification::topLeft, true);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        started = false;
        if (e.mods.isPopupMenu() && onMenu)
            onMenu();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! started && ! e.mods.isPopupMenu() && tickArea().contains (e.getPosition()))
            track.setProperty (IDs::include, ! (bool) track.getProperty (IDs::include, true), nullptr);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.mods.isPopupMenu() || e.getDistanceFromDragStart() < 6)
            return;
        started = true;
        const auto file = songs::writeSongFile (song, { track[IDs::uid].toString() }, false,
                                                song[IDs::name].toString() + " - " + track[IDs::name].toString());
        if (file.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (file.getFullPathName()), false, this);
    }

private:
    bool started = false;
};

//==============================================================================
// The arrangement: sections along the top (click one to edit it), bar numbers, the tracks, and a lane under them where a
// drop starts a new track.
class SongGrid final : public juce::Component, public juce::DragAndDropTarget, public juce::FileDragAndDropTarget
{
public:
    explicit SongGrid (PedalCuesProcessor& p) : proc (p) { setWantsKeyboardFocus (true); }

    juce::ValueTree song;
    double pixelsPerBeat = 20.0;
    std::function<juce::String()> deviceName;   // the tab a tile is being dragged from: names a new track
    std::function<void()> onTogglePlay;         // Space
    double playBeat = -1.0;                     // the playhead while the song plays
    double cursorBeat = -1.0;                   // the song position: Play starts here, Paste puts cues here
    juce::ValueTree cursorTrack;                // ...on this track

    double beatAt (int x) const { return juce::jmax (0.0, x / pixelsPerBeat); }
    int xOf (double beat) const { return juce::roundToInt (beat * pixelsPerBeat); }
    int contentHeight() const { return trackTop ((int) songs::tracks (song).size()) + newLaneHeight + 4; }

    // To the section's beat (an eighth in 7/8), or a 1/16 with Alt held.
    double snap (double beat) const
    {
        const auto section = songs::sectionAt (song, beat);
        if (! section.isValid())
            return 0.0;
        const auto start = songs::sectionStartBeat (song, section);
        const auto unit = juce::ModifierKeys::currentModifiers.isAltDown() ? 0.25 : 4.0 / juce::jmax (1, (int) section.getProperty (IDs::timeDen, 4));
        return start + std::round ((beat - start) / unit) * unit;
    }

    void rebuild()
    {
        blocks.clear();
        if (! song.isValid())
            return;
        selection.erase (std::remove_if (selection.begin(), selection.end(), [this] (const auto& c) { return c.getParent() != song; }),
                         selection.end());   // deleted, or another song
        if (cursorTrack.isValid() && cursorTrack.getParent() != song)
            cursorTrack = {}, cursorBeat = -1.0;
        const auto list = songs::tracks (song);
        std::vector<juce::ValueTree> cuesInOrder;
        for (auto c : song)
            if (c.hasType (IDs::SongCue))
                cuesInOrder.push_back (c);
        std::sort (cuesInOrder.begin(), cuesInOrder.end(), [this] (const auto& a, const auto& b)
                   { return songs::cueBeat (song, a) < songs::cueBeat (song, b); });

        // Two lanes per track, so cues close together don't hide each other.
        std::vector<std::array<int, 2>> laneEnds (list.size(), { -1000, -1000 });
        for (auto c : cuesInOrder)
        {
            const auto beat = songs::cueBeat (song, c);
            const auto it = std::find_if (list.begin(), list.end(), [&] (const auto& t) { return t[IDs::uid] == c[IDs::track]; });
            if (beat < 0.0 || it == list.end())
                continue;
            const auto index = (int) (it - list.begin());
            auto* b = blocks.add (new CueBlock());
            b->cue = c;
            b->setTooltip (c[IDs::name].toString() + " at " + songs::barLabel (song, beat)
                           + ". Click to select (Shift adds), then Copy / Paste. Drag to move it (Alt: 1/16); right-click for more.");
            const auto x = xOf (beat);
            const auto width = juce::jmax (juce::roundToInt (juce::jmax (1.0, (double) c.getProperty (IDs::lengthBeats, 1.0)) * pixelsPerBeat), 74);
            auto& ends = laneEnds[(size_t) index];
            const int lane = x >= ends[0] ? 0 : (x >= ends[1] ? 1 : (ends[0] <= ends[1] ? 0 : 1));
            ends[(size_t) lane] = x + width + 2;
            b->setBounds (x, trackTop (index) + 5 + lane * (blockHeight + 2), width, blockHeight);
            b->selected = isSelected (c);
            b->onMenu = [this, c] { cueMenu (c); };
            b->onSelect = [this, c] (bool add) { select (c, add); };
            addAndMakeVisible (b);
        }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        if (! song.isValid())
            return;
        const auto w = getWidth();
        const auto count = (int) songs::tracks (song).size();

        for (int r = 0; r < count; ++r)
        {
            g.setColour (r % 2 == 0 ? surface : surface.brighter (0.03f));
            g.fillRect (0, trackTop (r), w, trackHeight);
        }
        // The lane for a new track.
        auto lane = juce::Rectangle<int> (0, trackTop (count) + 4, w, newLaneHeight - 4);
        g.setColour (dropBeat >= 0.0 && dropTrack < 0 ? builderViolet.withAlpha (0.18f) : surface.withAlpha (0.5f));
        g.fillRoundedRectangle (lane.toFloat(), 6.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (lane.toFloat().reduced (0.5f), 6.0f, 1.0f);
        g.setColour (dim);
        g.setFont (font (12.5f));
        g.drawText (count == 0 ? "Drag a tile here from Amps & Modelers or Effects & Pedals (or a .mid clip): it starts a track for that device"
                               : "Drop here for a new track",
                    lane.reduced (12, 0), juce::Justification::centredLeft, true);

        double start = 0.0;
        int bar = 1, index = 0;
        for (const auto& s : song)
        {
            if (! s.hasType (IDs::SongSection))
                continue;
            const auto length = songs::sectionLengthBeats (s);
            const auto x0 = xOf (start), x1 = xOf (start + length);
            const auto colour = colourOf (s, paletteColour (index++));
            auto head = juce::Rectangle<int> (x0, 0, x1 - x0, rulerHeight).reduced (1, 3);
            g.setColour (colour.withAlpha (s == hoverSection ? 0.42f : 0.26f));
            g.fillRoundedRectangle (head.toFloat(), 6.0f);
            g.setColour (colour);
            g.fillRoundedRectangle (head.removeFromLeft (3).toFloat(), 1.5f);
            auto label = head.reduced (7, 2);
            g.setColour (text);
            g.setFont (font (13.0f, true));
            g.drawText (s[IDs::name].toString(), label.removeFromTop (label.getHeight() / 2 + 1), juce::Justification::bottomLeft, true);
            g.setColour (dim);
            g.setFont (font (11.0f));
            g.drawText (s[IDs::bars].toString() + " bars  " + meterText (s) + "  " + bpmText ((double) s.getProperty (IDs::bpm, 120.0)) + " BPM",
                        label, juce::Justification::topLeft, true);

            const auto bb = songs::barBeats (s);
            const auto bars = juce::jmax (1, (int) s.getProperty (IDs::bars, 1));
            for (int i = 0; i < bars; ++i, ++bar)
            {
                const auto x = xOf (start + i * bb);
                g.setColour (i == 0 ? outline.brighter (0.4f) : outline.withAlpha (0.7f));
                g.drawVerticalLine (x, (float) rulerHeight, (float) trackTop (count));
                if (bb * pixelsPerBeat >= 22.0 || i % 4 == 0)
                {
                    g.setColour (dim);
                    g.setFont (font (10.5f));
                    g.drawText (juce::String (bar), x + 3, rulerHeight, 40, barsHeight, juce::Justification::centredLeft, false);
                }
            }
            start += length;
        }
        g.setColour (outline.brighter (0.4f));
        g.drawVerticalLine (xOf (start), (float) rulerHeight, (float) trackTop (count));

        // The song position (click a track or the bar numbers): Play starts here, Paste puts cues here (on the clicked track).
        if (cursorBeat >= 0.0)
        {
            const auto x = (float) xOf (cursorBeat);
            g.setColour (builderViolet);
            g.fillRect (x - 0.5f, (float) rulerHeight, 2.0f, (float) (trackTop (count) - rulerHeight));
            juce::Path marker;
            marker.addTriangle (x - 5.0f, (float) rulerHeight, x + 6.0f, (float) rulerHeight, x + 0.5f, (float) rulerHeight + 7.0f);
            g.fillPath (marker);
            const auto list = songs::tracks (song);
            const auto index = (int) (std::find (list.begin(), list.end(), cursorTrack) - list.begin());
            if (cursorTrack.isValid() && index < (int) list.size())   // the paste track, a little brighter
            {
                g.setColour (builderViolet.withAlpha (0.12f));
                g.fillRect (0, trackTop (index), getWidth(), trackHeight);
            }
        }
        if (playBeat >= 0.0)
        {
            g.setColour (accent);
            g.fillRect ((float) xOf (playBeat) - 0.5f, (float) rulerHeight, 2.0f, (float) (trackTop (count) - rulerHeight));
        }

        if (dropBeat >= 0.0)
        {
            const auto x = xOf (dropBeat);
            const auto top = dropTrack >= 0 ? trackTop (dropTrack) : trackTop (count) + 4;
            const auto h = dropTrack >= 0 ? trackHeight : newLaneHeight - 4;
            g.setColour (accent);
            g.fillRect (x - 1, top, 3, h);
            g.setFont (font (11.0f, true));
            g.drawText (songs::barLabel (song, dropBeat), x + 5, top + h - 17, 140, 15, juce::Justification::centredLeft, false);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto s = e.y < rulerHeight ? songs::sectionAt (song, beatAt (e.x)) : juce::ValueTree();
        if (s != hoverSection)
        {
            hoverSection = s;
            setMouseCursor (s.isValid() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }
    void mouseExit (const juce::MouseEvent&) override { hoverSection = {}; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (e.y < rulerHeight || ! song.isValid())
            return;
        if (e.y < trackTop (0))   // the bar numbers: move the song position only
        {
            cursorBeat = snap (beatAt (e.x));
            repaint();
            return;
        }
        // A spot on a track: Paste goes there. Clicking empty space also clears the selection.
        if (auto track = trackAtY (e.y); track.isValid())
        {
            cursorTrack = track;
            cursorBeat = snap (beatAt (e.x));
        }
        if (! e.mods.isShiftDown())
            setSelection ({});
        repaint();
        if (e.mods.isPopupMenu() && cursorTrack.isValid())
            spotMenu();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.y < rulerHeight && song.isValid() && beatAt (e.x) < songs::songLengthBeats (song))
            sectionMenu (songs::sectionAt (song, beatAt (e.x)));
    }

    // ⌘C / ⌘V (Ctrl on Windows and Linux), ⌘D duplicates, ⌘A selects all, Delete deletes, Esc clears.
    bool keyPressed (const juce::KeyPress& key) override
    {
        const auto cmd = juce::ModifierKeys::commandModifier;
        if (key == juce::KeyPress ('c', cmd, 0)) { copy(); return true; }
        if (key == juce::KeyPress ('x', cmd, 0)) { copy(); deleteSelection(); return true; }
        if (key == juce::KeyPress ('v', cmd, 0)) { paste(); return true; }
        if (key == juce::KeyPress ('d', cmd, 0)) { duplicateSelection(); return true; }
        if (key == juce::KeyPress ('a', cmd, 0))
        {
            std::vector<juce::ValueTree> all;
            for (auto c : song)
                if (c.hasType (IDs::SongCue))
                    all.push_back (c);
            setSelection (all);
            return true;
        }
        if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) { deleteSelection(); return true; }
        if (key == juce::KeyPress::escapeKey) { setSelection ({}); return true; }
        if (key == juce::KeyPress::spaceKey) { if (onTogglePlay) onTogglePlay(); return true; }
        return false;
    }

    // Placed cues, moved inside the window.
    bool isInterestedInDragSource (const SourceDetails& d) override
    {
        return song.isValid() && dynamic_cast<CueBlock*> (d.sourceComponent.get()) != nullptr;
    }
    void itemDragMove (const SourceDetails& d) override { showDrop (d.localPosition.toInt(), grabOffset (d)); }
    void itemDragExit (const SourceDetails&) override { clearDrop(); }
    void itemDropped (const SourceDetails& d) override
    {
        const auto pos = d.localPosition.toInt();
        const auto beat = snap (beatAt (pos.x - grabOffset (d)));
        clearDrop();
        if (auto* block = dynamic_cast<CueBlock*> (d.sourceComponent.get()))
        {
            auto track = trackAtY (pos.y);
            if (! track.isValid())
                track = songs::trackByUid (song, block->cue[IDs::track].toString());
            songs::moveCue (song, block->cue, track, beat);
        }
    }

    // Tiles dragged in from the tabs (they arrive as the .mid file they write), or .mid clips from the DAW / Finder.
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        if (! song.isValid())
            return false;
        for (const auto& f : files)
            if (isMidiFile (f))
                return true;
        return false;
    }
    void fileDragMove (const juce::StringArray&, int x, int y) override { showDrop ({ x, y }, 0); }
    void fileDragExit (const juce::StringArray&) override { clearDrop(); }
    void filesDropped (const juce::StringArray& files, int x, int y) override
    {
        auto beat = snap (beatAt (x));
        auto track = trackAtY (y);
        clearDrop();
        for (const auto& path : files)
        {
            if (! isMidiFile (path))
                continue;
            cues::Cue cue;
            juce::String title;
            auto colour = builderViolet;
            if (path == Tile::lastDrag.path)
            {
                cue = Tile::lastDrag.cue;
                title = Tile::lastDrag.title;
                colour = Tile::lastDrag.colour;
            }
            else
            {
                juce::FileInputStream in ((juce::File (path)));
                juce::MidiFile midi;
                if (! in.openedOk() || ! midi.readFrom (in))
                    continue;
                title = juce::File (path).getFileNameWithoutExtension();
                cue = songs::cueFromMidiFile (midi, title);
            }
            if (cue.events.empty())
                continue;
            cue.name = title;
            if (! track.isValid())   // the new-track lane: named after the device on the tab it came from
            {
                const auto name = deviceName && path == Tile::lastDrag.path ? deviceName() : juce::String ("Track");
                track = songs::addTrack (song, name, colour);
            }
            songs::placeCue (song, track, beat, cue, colour);
            beat += juce::jmax (1.0, cue.lengthBeats);   // several files: one after another
        }
    }

private:
    juce::ValueTree trackAtY (int y) const
    {
        const auto list = songs::tracks (song);
        const auto index = (y - trackTop (0)) / trackHeight;
        return y >= trackTop (0) && index < (int) list.size() ? list[(size_t) index] : juce::ValueTree();
    }

    void showDrop (juce::Point<int> pos, int offset)
    {
        const auto list = songs::tracks (song);
        const auto t = trackAtY (pos.y);
        dropTrack = t.isValid() ? (int) (std::find (list.begin(), list.end(), t) - list.begin()) : -1;
        dropBeat = snap (beatAt (pos.x - offset));
        repaint();
    }
    void clearDrop() { dropBeat = -1.0; repaint(); }

    int grabOffset (const SourceDetails& d) const
    {
        if (auto* block = dynamic_cast<CueBlock*> (d.sourceComponent.get()))
            return juce::jlimit (0, block->getWidth(), block->grabX);
        return 0;
    }

    bool isSelected (const juce::ValueTree& c) const { return std::find (selection.begin(), selection.end(), c) != selection.end(); }

    void setSelection (std::vector<juce::ValueTree> list)
    {
        selection = std::move (list);
        for (auto* b : blocks)
        {
            b->selected = isSelected (b->cue);
            b->repaint();
        }
    }

    void select (const juce::ValueTree& c, bool add)
    {
        auto list = add ? selection : std::vector<juce::ValueTree>();
        if (add && isSelected (c))
            list.erase (std::find (list.begin(), list.end(), c));
        else
            list.push_back (c);
        setSelection (list);
    }

    void copy()
    {
        if (! selection.empty())
            clipboard = songs::copyCues (song, selection);
    }

    void paste()
    {
        if (clipboard.empty() || ! song.isValid())
            return;
        auto track = cursorTrack;
        if (! track.isValid())   // nowhere clicked yet: the first copied cue's own track, or the first track
            track = songs::trackByUid (song, clipboard.front().track);
        if (! track.isValid() && ! songs::tracks (song).empty())
            track = songs::tracks (song).front();
        const auto beat = cursorBeat >= 0.0 ? cursorBeat : 0.0;
        auto pasted = songs::pasteCues (song, clipboard, beat, track);
        cursorTrack = track;
        cursorBeat = juce::jmin (songs::songLengthBeats (song) - 0.25, beat + songs::clipLength (clipboard));   // the next paste follows
        setSelection (std::move (pasted));
    }

    void duplicateSelection()
    {
        if (selection.empty())
            return;
        const auto clip = songs::copyCues (song, selection);
        double first = songs::cueBeat (song, selection.front());
        for (const auto& c : selection)
            first = juce::jmin (first, songs::cueBeat (song, c));
        const auto track = songs::trackByUid (song, clip.front().track);
        setSelection (songs::pasteCues (song, clip, first + songs::clipLength (clip), track));
    }

    void deleteSelection()
    {
        for (auto c : selection)
            if (c.getParent().isValid())
                c.getParent().removeChild (c, nullptr);
        setSelection ({});
    }

    // Right-click an empty spot on a track.
    void spotMenu()
    {
        juce::PopupMenu m;
        m.addSectionHeader (cursorTrack[IDs::name].toString() + "  -  " + songs::barLabel (song, cursorBeat));
        m.addItem (1, clipboard.size() > 1 ? "Paste " + juce::String ((int) clipboard.size()) + " cues here" : juce::String ("Paste here"),
                   ! clipboard.empty());
        m.addItem (2, "Play from here");
        juce::Component::SafePointer<SongGrid> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe] (int r)
        {
            if (safe != nullptr && r == 1)
                safe->paste();
            else if (safe != nullptr && r == 2 && safe->onTogglePlay)
                safe->onTogglePlay();
        });
    }

    void cueMenu (juce::ValueTree cue)
    {
        if (! isSelected (cue))
            setSelection ({ cue });
        const auto n = (int) selection.size();
        const auto many = n > 1 ? " " + juce::String (n) + " cues" : juce::String();
        juce::PopupMenu m;
        m.addSectionHeader (n > 1 ? juce::String (n) + " cues selected"
                                  : cue[IDs::name].toString() + "  -  " + songs::barLabel (song, songs::cueBeat (song, cue)));
        m.addItem (1, "Send it to the device now", n == 1);
        m.addSeparator();
        m.addItem (3, "Copy" + many);
        m.addItem (4, "Duplicate" + many + " (right after)");
        m.addItem (2, "Delete" + many);
        juce::Component::SafePointer<SongGrid> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, cue] (int r)
        {
            if (safe == nullptr)
                return;
            if (r == 1)
                safe->proc.preview (songs::cueFromTree (cue));
            else if (r == 3)
                safe->copy();
            else if (r == 4)
                safe->duplicateSelection();
            else if (r == 2)
                safe->deleteSelection();
        });
    }

    void sectionMenu (juce::ValueTree section)
    {
        if (! section.isValid())
            return;
        int count = 0, ordinal = 0;
        for (const auto& s : song)
            if (s.hasType (IDs::SongSection))
            {
                if (s == section)
                    ordinal = count;
                ++count;
            }

        juce::PopupMenu m;
        m.addSectionHeader (section[IDs::name].toString());
        m.addItem (1, "Edit section...");
        m.addSubMenu ("Colour", colourMenu (colourOf (section, paletteColour (ordinal)), 100));
        m.addItem (2, "Add a section after it");
        m.addItem (3, "Duplicate it (with its cues)");
        m.addItem (4, "Move earlier", ordinal > 0);
        m.addItem (5, "Move later", ordinal < count - 1);
        m.addSeparator();
        m.addItem (6, "Delete it and its cues", count > 1);
        auto songTree = song;
        m.showMenuAsync (juce::PopupMenu::Options(), [songTree, section] (int r) mutable
        {
            const auto index = songTree.indexOf (section);
            if (r == 1)
                editSectionDialog (section);
            else if (r >= 100 && r < 164)
                section.setProperty (IDs::colour, paletteColour (r - 100).toString(), nullptr);
            else if (r == 2)
            {
                auto s = songs::createSection ("Section", 8, section[IDs::timeNum], section[IDs::timeDen], section[IDs::bpm]);
                songTree.addChild (s, index + 1, nullptr);
                editSectionDialog (s);
            }
            else if (r == 3)
            {
                auto s = songs::createSection (section[IDs::name].toString(), section[IDs::bars], section[IDs::timeNum],
                                               section[IDs::timeDen], section[IDs::bpm]);
                s.setProperty (IDs::colour, section[IDs::colour], nullptr);
                songTree.addChild (s, index + 1, nullptr);
                for (int i = 0; i < songTree.getNumChildren(); ++i)
                {
                    const auto c = songTree.getChild (i);
                    if (c.hasType (IDs::SongCue) && c[IDs::section] == section[IDs::uid])
                    {
                        auto copy = c.createCopy();
                        copy.setProperty (IDs::section, s[IDs::uid], nullptr);
                        songTree.appendChild (copy, nullptr);
                    }
                }
            }
            else if (r == 4 || r == 5)
            {
                int target = index;
                do target += (r == 4 ? -1 : 1);
                while (target >= 0 && target < songTree.getNumChildren() && ! songTree.getChild (target).hasType (IDs::SongSection));
                if (target >= 0 && target < songTree.getNumChildren())
                    songTree.moveChild (index, target, nullptr);
            }
            else if (r == 6)
                songs::removeSection (songTree, section);
        });
    }

    PedalCuesProcessor& proc;
    juce::OwnedArray<CueBlock> blocks;
    juce::ValueTree hoverSection;
    double dropBeat = -1.0;
    int dropTrack = -1;
    std::vector<juce::ValueTree> selection;
    static inline std::vector<songs::ClipCue> clipboard;   // shared by every song, so cues can be pasted into another song
};

//==============================================================================
// Drag the song (the ticked tracks) into the DAW as one MIDI file.
struct DragSongButton final : public juce::TextButton
{
    std::function<juce::File()> makeFile;
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 6 || ! makeFile)
            return;
        started = true;
        const auto f = makeFile();
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (f.getFullPathName()), false, this);
    }
    void mouseDown (const juce::MouseEvent& e) override { started = false; juce::TextButton::mouseDown (e); }

private:
    bool started = false;
};

//==============================================================================
// Export: which tracks, and the channel each sends on, checked before the file is written.
class ExportPanel final : public juce::Component
{
public:
    ExportPanel (juce::ValueTree s, std::function<void (bool filePerTrack)> onExportChosen)
        : song (std::move (s)), onExport (std::move (onExportChosen))
    {
        intro.setText ("Pick the tracks to export and each one's MIDI channel. Picking a channel moves every cue on that track "
                       "to it (and the ones you add later); \"Own channels\" leaves the cues as they are. Two players with "
                       "the same changes: duplicate the track and give the copy the other rig's channel.", juce::dontSendNotification);
        intro.setColour (juce::Label::textColourId, dim);
        intro.setFont (font (13.0f));
        intro.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (intro);

        for (auto track : songs::tracks (song))
        {
            auto* row = rows.add (new Row());
            row->track = track;
            row->include.setButtonText (track[IDs::name].toString());
            row->include.setToggleState ((bool) track.getProperty (IDs::include, true), juce::dontSendNotification);
            row->include.setColour (juce::ToggleButton::tickColourId, builderViolet);
            addAndMakeVisible (row->include);

            row->channel.addItem (ownChannelsText (song, track), 1);
            for (int ch = 1; ch <= 16; ++ch)
                row->channel.addItem ("Channel " + juce::String (ch), ch + 1);
            row->channel.setSelectedId (songs::trackChannel (track) + 1, juce::dontSendNotification);
            addAndMakeVisible (row->channel);
        }

        exportButton.setColour (juce::TextButton::buttonColourId, builderViolet);
        exportButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        oneFile.setButtonText ("One file, all tracks");
        perTrack.setButtonText ("A file per track");
        perTrack.setTooltip ("Saves each ticked track as \"Song - Track.mid\" in the folder you pick, each with the tempo map, "
                             "time signatures and section markers");
        oneFile.setTooltip ("One MIDI file: the tempo map and markers, then a track per ticked track");
        for (auto* b : { &oneFile, &perTrack })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (3301);
            b->setColour (juce::TextButton::buttonOnColourId, builderViolet);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            addAndMakeVisible (b);
        }
        filesLabel.setText ("Save as", juce::dontSendNotification);
        filesLabel.setColour (juce::Label::textColourId, dim);
        filesLabel.setFont (font (13.0f));
        addAndMakeVisible (filesLabel);
        oneFile.setToggleState (! lastFilePerTrack, juce::dontSendNotification);
        perTrack.setToggleState (lastFilePerTrack, juce::dontSendNotification);
        exportButton.onClick = [this]
        {
            apply();
            lastFilePerTrack = perTrack.getToggleState();
            const auto each = lastFilePerTrack;
            auto done = onExport;
            close();
            if (done)
                done (each);
        };
        addAndMakeVisible (exportButton);
        cancel.onClick = [this] { close(); };
        addAndMakeVisible (cancel);

        setSize (560, 150 + rows.size() * 36 + 40);
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20, 16);
        intro.setBounds (r.removeFromTop (56));
        r.removeFromTop (8);
        for (auto* row : rows)
        {
            auto line = r.removeFromTop (36).reduced (0, 4);
            row->channel.setBounds (line.removeFromRight (220));
            row->include.setBounds (line);
        }
        r.removeFromTop (10);
        auto choice = r.removeFromTop (32);
        filesLabel.setBounds (choice.removeFromLeft (70));
        oneFile.setBounds (choice.removeFromLeft (190));
        perTrack.setBounds (choice.removeFromLeft (190).withTrimmedLeft (4));
        auto buttons = r.removeFromBottom (34);
        cancel.setBounds (buttons.removeFromRight (100));
        buttons.removeFromRight (8);
        exportButton.setBounds (buttons.removeFromRight (150));
    }

private:
    struct Row { juce::ValueTree track; juce::ToggleButton include; juce::ComboBox channel; };

    void apply()
    {
        for (auto* row : rows)
        {
            row->track.setProperty (IDs::include, row->include.getToggleState(), nullptr);
            if (const auto channel = row->channel.getSelectedId() - 1; channel != songs::trackChannel (row->track))
                songs::setTrackChannel (song, row->track, channel);   // rewrites the track's cues
        }
    }

    void close()
    {
        if (auto* w = findParentComponentOfClass<juce::DialogWindow>())
            w->exitModalState (0);
    }

    juce::ValueTree song;
    std::function<void (bool)> onExport;
    juce::TextButton oneFile, perTrack;
    juce::Label filesLabel;
    static inline bool lastFilePerTrack = false;   // remembered while the app runs
    juce::Label intro;
    juce::OwnedArray<Row> rows;
    juce::TextButton exportButton { "Export..." }, cancel { "Cancel" };
};

//==============================================================================
class SongBuilder final : public juce::Component, public juce::DragAndDropContainer,
                          private juce::ValueTree::Listener, private juce::AsyncUpdater, private juce::Timer
{
public:
    SongBuilder (PedalCuesProcessor& p, std::function<juce::String()> deviceName)
        : proc (p), state (p.state), grid (p), saved (p)
    {
        addAndMakeVisible (saved);
        grid.deviceName = std::move (deviceName);
        grid.onTogglePlay = [this] { togglePlay(); };

        addAndMakeVisible (listSection);
        addSong.setColour (juce::TextButton::buttonColourId, accent);
        addSong.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        addSong.setTooltip ("Start a new song (right-click a song to rename, duplicate or delete it)");
        addSong.onClick = [this] { newSong(); };
        addAndMakeVisible (addSong);
        importSong.setTooltip ("Add a song: a song file (Save song..., e.g. from a bandmate), or a MIDI file from your DAW or "
                               "another player, with its cues or just its markers, tempo and time signatures");
        importSong.onClick = [this] { importMenu(); };
        addAndMakeVisible (importSong);
        listView.setViewedComponent (&listContent, false);
        listView.setScrollBarsShown (true, false);
        listView.setScrollBarThickness (8);
        addAndMakeVisible (listView);

        addChildComponent (songSection);
        songName.setFont (font (24.0f, true));
        songName.setColour (juce::Label::textColourId, text);
        songName.setEditable (false, true, false);
        songName.setTooltip ("Double-click to rename the song");
        songName.onTextChange = [this]
        {
            auto s = current();
            if (songName.getText().trim().isNotEmpty())
                s.setProperty (IDs::name, songName.getText().trim(), nullptr);
        };
        addChildComponent (songName);
        summary.setColour (juce::Label::textColourId, dim);
        summary.setFont (font (13.0f));
        addChildComponent (summary);
        dragSong.setButtonText ("Drag song to the DAW");
        dragSong.setColour (juce::TextButton::buttonColourId, builderViolet);
        dragSong.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        dragSong.setTooltip ("Drag onto your DAW's timeline: one MIDI file with the ticked tracks, the tempo map, time "
                             "signatures and a marker for each section");
        dragSong.makeFile = [this] { return songs::writeSongFile (current(), songs::includedTracks (current())); };
        addChildComponent (dragSong);
        exportSong.setButtonText ("Export MIDI file(s)...");
        exportSong.setTooltip ("Save the ticked tracks as one MIDI file or a file per track, e.g. for a backing-track player that plays MIDI next to your stems");
        exportSong.onClick = [this] { chooseExport(); };
        addChildComponent (exportSong);
        play.setButtonText (juce::String::fromUTF8 ("\u25B6  Play"));
        play.setTooltip ("Play the ticked tracks to your devices from the song position (click a spot on the arrangement to "
                         "move it). Space plays and stops.");
        play.setColour (juce::TextButton::buttonColourId, ledGreen.darker (0.2f));
        play.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        play.onClick = [this] { togglePlay(); };
        addChildComponent (play);
        saveSong.setButtonText ("Save song...");
        saveSong.setTooltip ("Save this song (sections, tracks and cues) as a file, to keep it or share it. It's also saved "
                             "with your project.");
        saveSong.onClick = [this] { chooseSaveSong (current()); };
        addChildComponent (saveSong);

        addChildComponent (gridSection);
        addSection.setButtonText ("+ Section");
        addSection.setTooltip ("Add a section at the end (click a section's name to edit, move or delete it)");
        addSection.onClick = [this] { appendSection(); };
        addChildComponent (addSection);
        addTrack.setButtonText ("+ Track");
        addTrack.setTooltip ("Add a track for a device: the ones on your tabs, any other, or an empty one. Dropping a tile under "
                             "the tracks also starts one.");
        addTrack.onClick = [this] { newTrack(); };
        addChildComponent (addTrack);
        tracksView.setViewedComponent (&tracksArea, false);
        tracksView.setScrollBarsShown (true, false);
        tracksView.setScrollBarThickness (8);
        addChildComponent (tracksView);
        tracksArea.addAndMakeVisible (labels);
        gridView.setViewedComponent (&grid, false);
        gridView.setScrollBarsShown (false, true);
        gridView.setScrollBarThickness (8);
        tracksArea.addAndMakeVisible (gridView);

        empty.setText ("Build a song once: sections along the top, a track per device. Drag tiles in from the Amps & Modelers "
                       "and Effects & Pedals tabs, then drag the whole song into your DAW, or export it for a backing-track "
                       "player.\n\nClick + Song, or Import song... for a song file or a MIDI file from your DAW.",
                       juce::dontSendNotification);
        empty.setColour (juce::Label::textColourId, dim);
        empty.setFont (font (15.0f));
        empty.setJustificationType (juce::Justification::centred);
        addChildComponent (empty);

        state.addListener (this);
        rebuild();
    }

    ~SongBuilder() override
    {
        stopPlaying();
        state.removeListener (this);
    }

    void stopPlaying()
    {
        if (! isTimerRunning())
            return;
        stopTimer();
        proc.stopPreview();
        grid.playBeat = -1.0;
        grid.repaint();
        play.setButtonText (juce::String::fromUTF8 ("\u25B6  Play"));
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        auto left = r.removeFromLeft (230);
        r.removeFromLeft (14);

        listSection.setBounds (left);
        addSong.setBounds (listSection.headerArea().removeFromRight (84).withSizeKeepingCentre (84, 26));
        auto lc = listSection.contentArea();
        importSong.setBounds (lc.removeFromBottom (32));
        lc.removeFromBottom (8);
        listView.setBounds (lc);
        layoutList();

        empty.setBounds (r.reduced (40));

        // Two rows on the right: Play, Drag song, Export / Save song (under Export).
        songSection.setBounds (r.removeFromTop (120));
        saved.alignRight = true;   // the song card's title row, on the right, like the main window's version line
        saved.setBounds (songSection.headerArea().removeFromRight (160).withSizeKeepingCentre (160, 20));
        saved.toFront (false);
        auto sc = songSection.contentArea();
        auto buttons = sc.removeFromRight (520);
        auto top = buttons.removeFromTop (30);
        play.setBounds (top.removeFromLeft (100));
        top.removeFromLeft (8);
        dragSong.setBounds (top.removeFromLeft (200));
        top.removeFromLeft (8);
        exportSong.setBounds (top);
        buttons.removeFromTop (6);
        auto second = buttons.removeFromTop (30);
        saveSong.setBounds (second.removeFromRight (exportSong.getWidth()));
        songName.setBounds (sc.removeFromTop (30));
        summary.setBounds (sc.removeFromTop (20));
        r.removeFromTop (12);

        gridSection.setBounds (r);
        auto gh = gridSection.headerArea();
        addTrack.setBounds (gh.removeFromRight (90).withSizeKeepingCentre (90, 26));
        gh.removeFromRight (6);
        addSection.setBounds (gh.removeFromRight (96).withSizeKeepingCentre (96, 26));
        tracksView.setBounds (gridSection.contentArea());
        layoutTracks();
    }

private:
    juce::ValueTree current() { return songs::selectedSong (state); }

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { triggerAsyncUpdate(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { triggerAsyncUpdate(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { triggerAsyncUpdate(); }
    void valueTreeRedirected (juce::ValueTree&) override { triggerAsyncUpdate(); }

    void handleAsyncUpdate() override
    {
        // Rebuilding mid-drag would delete the component being dragged.
        if (isDragAndDropActive())
        {
            juce::Component::SafePointer<SongBuilder> safe (this);
            juce::Timer::callAfterDelay (200, [safe] { if (safe != nullptr) safe->triggerAsyncUpdate(); });
            return;
        }
        const auto list = state.getChildWithName (IDs::Songs);
        const auto signature = (list.isValid() ? list.toXmlString() : juce::String()) + state[IDs::selectedSong].toString();
        if (signature != lastSignature)
            rebuild();
    }

    void togglePlay()
    {
        if (isTimerRunning())
        {
            stopPlaying();
            return;
        }
        playing = current();
        if (! playing.isValid())
            return;
        playFrom = juce::jlimit (0.0, songs::songLengthBeats (playing), grid.cursorBeat >= 0.0 ? grid.cursorBeat : 0.0);
        proc.previewTimed (songs::playbackEvents (playing, songs::includedTracks (playing), playFrom));
        playStarted = juce::Time::getMillisecondCounterHiRes();
        play.setButtonText (juce::String::fromUTF8 ("\u25A0  Stop"));
        startTimerHz (30);
    }

    void timerCallback() override
    {
        if (current() != playing)   // another song picked
        {
            stopPlaying();
            return;
        }
        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - playStarted) / 1000.0;
        const auto beat = songs::secondsToBeat (playing, songs::beatToSeconds (playing, playFrom) + elapsed);
        if (beat >= songs::songLengthBeats (playing))
        {
            stopPlaying();
            return;
        }
        grid.playBeat = beat;
        grid.repaint();
    }

    void rebuild()
    {
        const auto list = state.getChildWithName (IDs::Songs);
        lastSignature = (list.isValid() ? list.toXmlString() : juce::String()) + state[IDs::selectedSong].toString();

        songRows.clear();
        const auto selected = current();
        int index = 0;
        if (list.isValid())
            for (auto song : list)
            {
                auto* t = songRows.add (new Tile (proc, Tile::Look::row));
                t->title = song[IDs::name].toString();
                t->badge = juce::String (songs::songBars (song)) + " bars";
                t->colour = paletteColour (index++);
                t->highlighted = song == selected;
                t->setTooltip ("Click to open. Right-click to rename, duplicate or delete.");
                t->onClick = [this, song] { state.setProperty (IDs::selectedSong, song[IDs::uid], nullptr); };
                t->onContextMenu = [this, song] { songMenu (song); };
                listContent.addAndMakeVisible (t);
            }

        const auto has = selected.isValid();
        for (auto* c : std::initializer_list<juce::Component*> { &songSection, &songName, &summary, &dragSong, &exportSong, &saveSong, &play, &saved,
                                                                &gridSection, &addSection, &addTrack, &tracksView })
            c->setVisible (has);
        empty.setVisible (! has);

        trackLabels.clear();
        if (has)
        {
            songName.setText (selected[IDs::name].toString(), juce::dontSendNotification);
            const auto first = selected.getChildWithName (IDs::SongSection);
            int sections = 0, cueCount = 0;
            bool tempoChanges = false, meterChanges = false;
            for (const auto& c : selected)
            {
                if (c.hasType (IDs::SongSection))
                {
                    ++sections;
                    tempoChanges |= std::abs ((double) c.getProperty (IDs::bpm, 120.0) - (double) first.getProperty (IDs::bpm, 120.0)) > 1.0e-6;
                    meterChanges |= meterText (c) != meterText (first);
                }
                cueCount += c.hasType (IDs::SongCue) ? 1 : 0;
            }
            const auto trackCount = (int) songs::tracks (selected).size();
            summary.setText (juce::String (sections) + (sections == 1 ? " section, " : " sections, ") + juce::String (songs::songBars (selected))
                                 + " bars, " + durationText (songs::songSeconds (selected)) + ",  "
                                 + bpmText ((double) first.getProperty (IDs::bpm, 120.0)) + (tempoChanges ? " BPM (changes)" : " BPM") + ",  "
                                 + meterText (first) + (meterChanges ? " (changes)" : "") + ",  "
                                 + juce::String (trackCount) + (trackCount == 1 ? " track, " : " tracks, ")
                                 + juce::String (cueCount) + (cueCount == 1 ? " cue" : " cues"),
                             juce::dontSendNotification);

            for (auto track : songs::tracks (selected))
            {
                auto* l = trackLabels.add (new TrackLabel());
                l->song = selected;
                l->track = track;
                l->onMenu = [this, track] { trackMenu (track); };
                labels.addAndMakeVisible (l);
            }
        }

        grid.song = selected;
        resized();
    }

    void layoutList()
    {
        const auto w = listView.getWidth() - listView.getScrollBarThickness() - 2;
        int y = 0;
        for (auto* t : songRows)
        {
            t->setBounds (0, y, w, 46);
            y += 50;
        }
        listContent.setSize (w, y);
    }

    void layoutTracks()
    {
        if (! grid.song.isValid() || tracksView.getWidth() <= 0)
            return;
        const auto width = tracksView.getWidth() - tracksView.getScrollBarThickness() - 2;
        const auto height = grid.contentHeight() + gridView.getScrollBarThickness();   // the scroll bar right under the tracks
        tracksArea.setSize (width, height);
        labels.setBounds (0, 0, 170, height);
        for (int i = 0; i < trackLabels.size(); ++i)
            trackLabels[i]->setBounds (0, trackTop (i), 162, trackHeight);
        gridView.setBounds (labels.getRight(), 0, width - labels.getRight(), height);

        const auto beats = juce::jmax (1.0, songs::songLengthBeats (grid.song));
        grid.pixelsPerBeat = juce::jmax (6.0, (gridView.getWidth() - 4) / beats);   // the whole song fits; very long ones scroll
        grid.setSize (juce::jmax (gridView.getWidth(), juce::roundToInt (beats * grid.pixelsPerBeat) + 2),
                      height - gridView.getScrollBarThickness());
        grid.rebuild();
    }

    void newSong()
    {
        auto list = songs::songsNode (state);
        auto song = songs::createSong ("Song " + juce::String (list.getNumChildren() + 1));
        list.appendChild (song, nullptr);
        state.setProperty (IDs::selectedSong, song[IDs::uid], nullptr);
    }

    // + Track: the devices on the two tabs first, then any device PedalCues knows (by brand) or your own, or an empty one.
    void newTrack()
    {
        struct Choice { juce::String name; juce::Colour colour; };
        std::vector<Choice> choices;
        juce::PopupMenu m;
        const auto add = [&] (juce::PopupMenu& menu, const juce::String& name, juce::Colour colour, const juce::String& label = {})
        {
            choices.push_back ({ name, colour });
            menu.addColouredItem ((int) choices.size(), label.isNotEmpty() ? label : name, colour, true, false);
        };

        const auto amp = ampInfo (state);
        const auto pedal = pedalInfo (state);
        m.addSectionHeader ("Your tabs");
        add (m, amp.name, amp.colour, amp.name + "  (Amps & Modelers)");
        add (m, pedal.name, pedal.colour, pedal.name + "  (Effects & Pedals)");

        // Every unit by brand: the built-in pages, the modeller and pedal pages, and the templates.
        std::map<juce::String, std::vector<Choice>> byBrand;
        byBrand["Neural DSP"] = { { "Quad Cortex", qcBlue }, { "Quad Cortex Mini", qcBlue } };
        byBrand["Kemper"] = { { "Kemper Profiler", kemperGreen }, { "Kemper Player", kemperGreen } };
        byBrand["DigiTech"] = { { "Whammy V", whammyRed }, { "Whammy DT", whammyRed } };
        for (const auto& p : modellers::all())
            byBrand[p.brand].push_back ({ p.model, p.colour });
        for (const auto& t : templates::all())
            byBrand[t.brand].push_back ({ t.model, t.colour });
        juce::PopupMenu others;
        for (auto& [brand, list] : byBrand)
        {
            juce::PopupMenu sub;
            for (const auto& c : list)
                add (sub, c.name, c.colour);
            others.addSubMenu (brand, sub);
        }
        const auto units = state.getChildWithName (IDs::CustomUnits);
        if (units.getNumChildren() > 0)
        {
            juce::PopupMenu mine;
            for (const auto& u : units)
                add (mine, u[IDs::name].toString(), colourOf (u, builderViolet));
            others.addSubMenu ("Your MIDI devices", mine);
        }
        m.addSubMenu ("Other device", others);
        m.addSeparator();
        m.addItem (-1, "Empty track...");

        auto song = current();
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addTrack), [song, choices] (int r) mutable
        {
            const auto n = (int) songs::tracks (song).size();
            if (r == -1)
            {
                askText ("New track", "Track " + juce::String (n + 1), [song, n] (const juce::String& name) mutable
                {
                    if (name.isNotEmpty())
                        songs::addTrack (song, name, paletteColour (n));
                });
                return;
            }
            if (r < 1 || r > (int) choices.size())
                return;
            // A second track for the same device (another player): "Quad Cortex 2".
            auto name = choices[(size_t) r - 1].name;
            int count = 0;
            for (const auto& t : songs::tracks (song))
                count += t[IDs::name].toString().startsWith (name) ? 1 : 0;
            if (count > 0)
                name << " " << (count + 1);
            songs::addTrack (song, name, choices[(size_t) r - 1].colour);
        });
    }

    void appendSection()
    {
        auto song = current();
        juce::ValueTree last;
        for (auto c : song)
            if (c.hasType (IDs::SongSection))
                last = c;
        auto s = songs::createSection ("Section", 8, last.getProperty (IDs::timeNum, 4), last.getProperty (IDs::timeDen, 4),
                                       last.getProperty (IDs::bpm, 120.0));
        song.addChild (s, last.isValid() ? song.indexOf (last) + 1 : 0, nullptr);
        editSectionDialog (s);
    }

    void trackMenu (juce::ValueTree track)
    {
        const auto list = songs::tracks (current());
        const auto at = (int) (std::find (list.begin(), list.end(), track) - list.begin());
        juce::PopupMenu m;
        m.addSectionHeader (track[IDs::name].toString());
        m.addSubMenu ("MIDI channel", channelMenu (songs::trackChannel (track), 200));
        m.addItem (6, "Duplicate (e.g. for another player)");
        m.addSeparator();
        m.addItem (1, "Rename...");
        m.addSubMenu ("Colour", colourMenu (colourOf (track, builderViolet), 100));
        m.addItem (2, "Include in Drag song and Export", true, (bool) track.getProperty (IDs::include, true));
        m.addItem (3, "Move up", at > 0);
        m.addItem (4, "Move down", at < (int) list.size() - 1);
        m.addSeparator();
        m.addItem (5, "Delete the track and its cues...");
        auto song = current();
        m.showMenuAsync (juce::PopupMenu::Options(), [song, track] (int r) mutable
        {
            if (r == 1)
                renameNode (track, "Rename track");
            else if (r >= 100 && r < 164)
                track.setProperty (IDs::colour, paletteColour (r - 100).toString(), nullptr);
            else if (r >= 200 && r <= 216)
            {
                // Moves every cue on the track to that channel. A track that mixes devices asks first.
                const auto channel = r - 200;
                const auto used = songs::channelsUsed (song, track);
                if (channel == 0 || used.size() <= 1)
                {
                    songs::setTrackChannel (song, track, channel);
                    return;
                }
                juce::StringArray list;
                for (auto c : used)
                    list.add (juce::String (c));
                auto* w = new juce::AlertWindow ("Change the channel",
                                                 "The cues on \"" + track[IDs::name].toString() + "\" use channels " + list.joinIntoString (", ")
                                                     + ", so they may be for different devices. Put them all on channel "
                                                     + juce::String (channel) + "?", juce::MessageBoxIconType::NoIcon);
                w->addButton ("Change all", 1);
                w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                w->enterModalState (true, juce::ModalCallbackFunction::create ([song, track, channel] (int res) mutable
                {
                    if (res == 1)
                        songs::setTrackChannel (song, track, channel);
                }), true);
            }
            else if (r == 6)
                songs::duplicateTrack (song, track);
            else if (r == 2)
                track.setProperty (IDs::include, ! (bool) track.getProperty (IDs::include, true), nullptr);
            else if (r == 3 || r == 4)
                songs::moveTrack (song, track, r == 3 ? -1 : 1);
            else if (r == 5)
            {
                auto* w = new juce::AlertWindow ("Delete track", "Delete \"" + track[IDs::name].toString() + "\" and its cues?",
                                                 juce::MessageBoxIconType::NoIcon);
                w->addButton ("Delete", 1);
                w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                w->enterModalState (true, juce::ModalCallbackFunction::create ([song, track] (int res) mutable
                {
                    if (res == 1)
                        songs::removeTrack (song, track);
                }), true);
            }
        });
    }

    void songMenu (juce::ValueTree song)
    {
        juce::PopupMenu m;
        m.addSectionHeader (song[IDs::name].toString());
        m.addItem (1, "Rename...");
        m.addItem (2, "Duplicate");
        m.addItem (4, "Save as file...");
        m.addSeparator();
        m.addItem (3, "Delete...");
        juce::Component::SafePointer<SongBuilder> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, song] (int r) mutable
        {
            if (safe == nullptr)
                return;
            auto list = song.getParent();
            if (r == 1)
                renameNode (song, "Rename song");
            else if (r == 4)
                safe->chooseSaveSong (song);
            else if (r == 2)
            {
                auto copy = songs::copySong (song, song[IDs::name].toString() + " (copy)");
                list.addChild (copy, list.indexOf (song) + 1, nullptr);
                safe->state.setProperty (IDs::selectedSong, copy[IDs::uid], nullptr);
            }
            else if (r == 3)
            {
                auto* w = new juce::AlertWindow ("Delete song", "Delete \"" + song[IDs::name].toString() + "\" and its cues? "
                                                 "Clips already in your DAW aren't affected.", juce::MessageBoxIconType::NoIcon);
                w->addButton ("Delete", 1);
                w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                w->enterModalState (true, juce::ModalCallbackFunction::create ([list, song] (int res) mutable
                {
                    if (res == 1)
                        list.removeChild (song, nullptr);
                }), true);
            }
        });
    }

    void chooseSaveSong (juce::ValueTree song)
    {
        if (! song.isValid())
            return;
        const auto name = juce::File::createLegalFileName (song[IDs::name].toString()) + songs::songFileExtension;
        chooser = std::make_unique<juce::FileChooser> ("Save the song", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                                             .getChildFile (name), "*" + songs::songFileExtension);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [song] (const juce::FileChooser& fc)
        {
            if (fc.getResult() == juce::File())
                return;
            const auto target = fc.getResult().withFileExtension (songs::songFileExtension);
            if (! songs::saveSongFile (song, target))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Save song", "Couldn't write " + target.getFullPathName());
        });
    }

    void chooseSongFiles()
    {
        chooser = std::make_unique<juce::FileChooser> ("Open songs", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                       "*" + songs::songFileExtension);
        juce::Component::SafePointer<SongBuilder> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems, [safe] (const juce::FileChooser& fc)
        {
            if (safe == nullptr)
                return;
            juce::StringArray problems;
            juce::ValueTree last;
            for (const auto& file : fc.getResults())
            {
                juce::String error;
                auto song = songs::loadSongFile (file, error);
                if (! song.isValid())
                {
                    problems.add (error);
                    continue;
                }
                songs::songsNode (safe->state).appendChild (song, nullptr);
                last = song;
            }
            if (last.isValid())
                safe->state.setProperty (IDs::selectedSong, last[IDs::uid], nullptr);
            if (! problems.isEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Open songs", problems.joinIntoString ("\n"));
        });
    }

    void importMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Song file (" + songs::songFileExtension + ")...");
        m.addItem (2, "MIDI file, with its cues...");
        m.addItem (3, "MIDI file, just the map (markers, tempo, time signatures)...");
        juce::Component::SafePointer<SongBuilder> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&importSong), [safe] (int r)
        {
            if (safe == nullptr)
                return;
            if (r == 1)
                safe->chooseSongFiles();
            else if (r == 2 || r == 3)
                safe->chooseMap (r == 2);
        });
    }

    // A MIDI file as a song: its markers, tempo and time signatures as sections, and with withCues each MIDI track's
    // messages as a track of cues.
    void chooseMap (bool withCues)
    {
        chooser = std::make_unique<juce::FileChooser> (withCues ? "Import a song from a MIDI file" : "Import a song map (markers, tempo, time signatures) from a MIDI file",
                                                       juce::File(), "*.mid;*.midi;*.smf");
        juce::Component::SafePointer<SongBuilder> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, withCues] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File())
                return;
            const auto file = fc.getResult();
            juce::FileInputStream in (file);
            juce::MidiFile midi;
            if (! in.openedOk() || ! midi.readFrom (in))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Import song", "This isn't a MIDI file PedalCues can read.");
                return;
            }
            auto result = songs::importMap (midi, file.getFileNameWithoutExtension(), withCues);
            if (result.error.isNotEmpty())
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Import song", result.error);
                return;
            }
            songs::songsNode (safe->state).appendChild (result.song, nullptr);
            safe->state.setProperty (IDs::selectedSong, result.song[IDs::uid], nullptr);
            if (! result.warnings.isEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Imported, with notes",
                                                        result.warnings.joinIntoString ("\n"));
        });
    }

    void chooseExport()
    {
        auto song = current();
        if (songs::tracks (song).empty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Export MIDI file(s)",
                                                    "This song has no tracks yet: drag tiles in from the tabs first.");
            return;
        }
        juce::Component::SafePointer<SongBuilder> safe (this);
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "Export MIDI file(s)";
        o.dialogBackgroundColour = background;
        o.content.setOwned (new ExportPanel (song, [safe] (bool filePerTrack)
        {
            if (safe == nullptr)
                return;
            if (filePerTrack)
                safe->chooseExportFolder();
            else
                safe->chooseExportFile();
        }));
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.componentToCentreAround = this;
        o.launchAsync();
    }

    // A file per ticked track, "Song - Track.mid", in the folder the user picks.
    void chooseExportFolder()
    {
        const auto song = current();
        chooser = std::make_unique<juce::FileChooser> ("Choose a folder for the track files",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
        juce::Component::SafePointer<SongBuilder> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe, song] (const juce::FileChooser& fc)
        {
            const auto folder = fc.getResult();
            if (safe == nullptr || folder == juce::File() || ! folder.isDirectory())
                return;
            juce::StringArray written, failed;
            for (const auto& track : songs::tracks (song))
            {
                if (! songs::includedTracks (song).contains (track[IDs::uid].toString()))
                    continue;
                const auto name = juce::File::createLegalFileName (song[IDs::name].toString() + " - " + track[IDs::name].toString());
                const auto target = folder.getChildFile (name + ".mid");
                const auto midi = songs::songMidi (song, { track[IDs::uid].toString() });
                juce::MemoryOutputStream data;
                if (midi.writeTo (data, 1) && target.replaceWithData (data.getData(), data.getDataSize()))
                    written.add (target.getFileName());
                else
                    failed.add (target.getFileName());
            }
            if (! failed.isEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Export MIDI files",
                                                        "Couldn't write " + failed.joinIntoString (", ") + " in " + folder.getFullPathName());
            else if (! written.isEmpty())
                folder.revealToUser();
        });
    }

    void chooseExportFile()
    {
        const auto song = current();
        const auto name = juce::File::createLegalFileName (song[IDs::name].toString()) + ".mid";
        chooser = std::make_unique<juce::FileChooser> ("Export the ticked tracks as a MIDI file",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (name), "*.mid");
        juce::Component::SafePointer<SongBuilder> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [safe, song] (const juce::FileChooser& fc)
        {
            if (safe == nullptr || fc.getResult() == juce::File())
                return;
            auto target = fc.getResult().withFileExtension ("mid");
            const auto midi = songs::songMidi (song, songs::includedTracks (song));
            juce::MemoryOutputStream data;
            if (! midi.writeTo (data, 1) || ! target.replaceWithData (data.getData(), data.getDataSize()))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Export MIDI file", "Couldn't write " + target.getFullPathName());
        });
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;

    Section listSection { "songs.list", "Songs", "beta", builderViolet };
    juce::TextButton addSong { "+ Song" }, importSong { "Import song..." };
    juce::Viewport listView;
    juce::Component listContent;
    juce::OwnedArray<Tile> songRows;

    Section songSection { "songs.song", "Song", "drag it into your DAW, or export it", builderViolet };
    juce::Label songName, summary;
    DragSongButton dragSong;
    juce::TextButton exportSong, saveSong, play;
    juce::ValueTree playing;
    double playFrom = 0.0, playStarted = 0.0;

    Section gridSection { "songs.grid", "Arrangement", "drag tiles in from the tabs; click a spot to play or paste there", builderViolet };
    juce::TextButton addSection, addTrack;
    juce::Viewport tracksView;    // scrolls the tracks up and down
    juce::Component tracksArea;
    juce::Component labels;
    juce::OwnedArray<TrackLabel> trackLabels;
    juce::Viewport gridView;      // scrolls the song left and right
    SongGrid grid;

    juce::Label empty;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::String lastSignature;
    SaveIndicator saved;
};

struct SongBuilderWindow final : public juce::DocumentWindow
{
    SongBuilderWindow (PedalCuesProcessor& p, std::function<juce::String()> deviceName)
        : juce::DocumentWindow ("Song Builder (beta) - PedalCues", background, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new SongBuilder (p, std::move (deviceName)), false);
        setResizable (true, false);
        setResizeLimits (1100, 560, 3000, 2000);   // the song card needs its two rows of buttons
        centreWithSize (1180, 680);
    }
    void closeButtonPressed() override
    {
        if (auto* builder = dynamic_cast<SongBuilder*> (getContentComponent()))
            builder->stopPlaying();
        setVisible (false);
    }
};
} // namespace

std::unique_ptr<juce::DocumentWindow> makeSongBuilderWindow (PedalCuesProcessor& p, std::function<juce::String()> deviceName)
{
    return std::make_unique<SongBuilderWindow> (p, std::move (deviceName));
}

std::unique_ptr<juce::Component> makeSongExportPanel (juce::ValueTree song)
{
    return std::make_unique<ExportPanel> (song, nullptr);
}

std::unique_ptr<juce::Component> makeSongBuilder (PedalCuesProcessor& p, std::function<juce::String()> deviceName)
{
    return std::make_unique<SongBuilder> (p, std::move (deviceName));
}
} // namespace ui
