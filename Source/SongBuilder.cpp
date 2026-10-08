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
    // A whole tempo without decimals (juce::String (x, 0) would print every decimal), else two.
    return std::abs (bpm - std::round (bpm)) < 0.005 ? juce::String (juce::roundToInt (bpm)) : juce::String (bpm, 2);
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

// The "glide" checkbox in the tempo dialogs; the dialog's callback deletes it (AlertWindow doesn't own custom components).
juce::ToggleButton* gradualToggle (juce::AlertWindow& w, bool on)
{
    auto* t = new juce::ToggleButton ("Gradually transition to the next tempo");
    t->setToggleState (on, juce::dontSendNotification);
    t->setColour (juce::ToggleButton::tickColourId, builderViolet);
    t->setSize (360, 28);
    w.addCustomComponent (t);
    return t;
}

// isNew: the change was just added, so Cancel takes it away again.
void editTempoDialog (juce::ValueTree change, bool isNew = false)
{
    auto* w = new juce::AlertWindow ("Tempo change", "The tempo from this point on. Bars don't move.", juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("bpm", bpmText ((double) change.getProperty (IDs::bpm, 120.0)), "Tempo (BPM)");
    auto* gradual = gradualToggle (*w, (bool) change.getProperty (IDs::tempoRamp, false));
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, change, isNew, gradual] (int result) mutable
    {
        const std::unique_ptr<juce::ToggleButton> owned (gradual);
        if (result != 1)
        {
            if (isNew)
                change.getParent().removeChild (change, nullptr);
            return;
        }
        const auto bpm = w->getTextEditorContents ("bpm").getDoubleValue();
        if (bpm > 0.0)
            change.setProperty (IDs::bpm, juce::jlimit (songs::minBpm, songs::maxBpm, bpm), nullptr);
        change.setProperty (IDs::tempoRamp, gradual->getToggleState(), nullptr);
    }), true);
}

void editSectionDialog (juce::ValueTree section)
{
    auto* w = new juce::AlertWindow ("Edit section", "Its name, length, time signature and tempo. Cues after it move with it.",
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", section[IDs::name].toString(), "Name");
    w->addTextEditor ("bars", section[IDs::bars].toString(), "Bars");
    w->addTextEditor ("meter", meterText (section), "Time signature (up to 255 / 1, 2, 4, 8, 16, 32 or 64)");
    w->addTextEditor ("bpm", bpmText ((double) section.getProperty (IDs::bpm, 120.0)), "Tempo (BPM)");
    auto* gradual = gradualToggle (*w, (bool) section.getProperty (IDs::tempoRamp, false));
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, section, gradual] (int result) mutable
    {
        const std::unique_ptr<juce::ToggleButton> owned (gradual);
        if (result != 1)
            return;
        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isNotEmpty())
            section.setProperty (IDs::name, name, nullptr);
        const auto bars = w->getTextEditorContents ("bars").getIntValue();
        if (bars > 0)
            section.setProperty (IDs::bars, juce::jlimit (1, songs::maxBars, bars), nullptr);
        const auto meter = juce::StringArray::fromTokens (w->getTextEditorContents ("meter"), "/", "");
        if (meter.size() == 2 && meter[0].getIntValue() > 0 && ! songs::isNoteValue (meter[1].getIntValue()))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Time signature",
                                                    "The note value (the number under the line) has to be 1, 2, 4, 8, 16, 32 or 64: "
                                                    "MIDI files, and so your DAW, can't store others. The time signature stayed "
                                                    + meterText (section) + ".");
        else if (meter.size() == 2 && meter[0].getIntValue() > 0 && meter[1].getIntValue() > 0)
        {
            const auto fresh = songs::createSection ("x", 1, meter[0].getIntValue(), meter[1].getIntValue(), 120.0);   // clamps
            section.setProperty (IDs::timeNum, fresh[IDs::timeNum], nullptr);
            section.setProperty (IDs::timeDen, fresh[IDs::timeDen], nullptr);
        }
        const auto bpm = w->getTextEditorContents ("bpm").getDoubleValue();
        if (bpm > 0.0)
            section.setProperty (IDs::bpm, juce::jlimit (songs::minBpm, songs::maxBpm, bpm), nullptr);
        section.setProperty (IDs::tempoRamp, gradual->getToggleState(), nullptr);
    }), true);
}

constexpr int rulerHeight = 42, barsHeight = 18, blockHeight = 23, newLaneHeight = 44, audioHeight = 58;
constexpr int tempoLaneHeight = 26, tempoLaneTop = rulerHeight + barsHeight;    // the tempo lane, under the bar numbers
constexpr int clickLaneHeight = 30, clickLaneTop = tempoLaneTop + tempoLaneHeight;   // the click track lane, under it
// Track height (the user's, saved as Songs' trackHeight): the default fits two lanes of cues, taller ones more.
constexpr int defaultTrackHeight = 58, minTrackHeight = 34, maxTrackHeight = 240, labelRowHeight = 58;

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
    std::function<void (int height)> onResize;   // dragging the bottom edge: every track's height

    TrackLabel()
    {
        setTooltip ("M mutes this track while playing, S plays only the soloed tracks. Tick to include it in Drag song and Export. "
                    "Drag the name to your DAW for this track alone. Right-click to set its MIDI channel, duplicate it for another "
                    "player, rename, recolour, move or delete it. Drag its bottom edge to make every track taller or shorter.");
    }

    // The name, M / S and the tick sit in the top row however tall the track is.
    juce::Rectangle<int> topRow() const { return getLocalBounds().withHeight (juce::jmin (getHeight(), labelRowHeight)).reduced (0, 4); }
    juce::Rectangle<int> tickArea() const { return topRow().removeFromRight (30); }
    // Mute and solo for Play (export follows the tick).
    juce::Rectangle<int> muteArea() const { return topRow().withTrimmedRight (30).removeFromRight (22).withSizeKeepingCentre (20, 18); }
    juce::Rectangle<int> soloArea() const { return topRow().withTrimmedRight (52).removeFromRight (22).withSizeKeepingCentre (20, 18); }
    bool onEdge (juce::Point<int> p) const { return p.y >= getHeight() - 6; }

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

        for (auto [area, letter, prop, colour] : { std::tuple<juce::Rectangle<int>, const char*, juce::Identifier, juce::Colour>
                                                   { muteArea(), "M", IDs::muted, accent }, { soloArea(), "S", IDs::soloed, ledGreen } })
        {
            const auto lit = (bool) track.getProperty (prop, false);
            g.setColour (lit ? colour : raised);
            g.fillRoundedRectangle (area.toFloat(), 3.0f);
            g.setColour (lit ? juce::Colours::black : dim);
            g.setFont (font (11.0f, true));
            g.drawText (letter, area, juce::Justification::centred, false);
        }

        auto words = topRow().withTrimmedLeft (4).reduced (10, 6).withTrimmedRight (74);
        g.setColour (on ? text : dim);
        g.setFont (font (13.0f, true));
        if (words.getHeight() < 30)   // a short track: the name only
        {
            g.drawText (track[IDs::name].toString(), words, juce::Justification::centredLeft, true);
            return;
        }
        g.drawText (track[IDs::name].toString(), words.removeFromTop (words.getHeight() / 2 + 2), juce::Justification::bottomLeft, true);
        g.setColour (songs::trackChannel (track) > 0 ? builderViolet : dim);
        g.setFont (font (11.0f));
        g.drawText (channelText (song, track), words, juce::Justification::topLeft, true);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseMove (const juce::MouseEvent& e) override
    {
        setMouseCursor (onEdge (e.getPosition()) ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        started = false;
        resizeFrom = ! e.mods.isPopupMenu() && onEdge (e.getPosition()) ? getHeight() : 0;
        if (e.mods.isPopupMenu() && onMenu)
            onMenu();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (resizeFrom > 0)
        {
            resizeFrom = 0;
            return;
        }
        if (started || e.mods.isPopupMenu())
            return;
        if (tickArea().contains (e.getPosition()))
            track.setProperty (IDs::include, ! (bool) track.getProperty (IDs::include, true), nullptr);
        else if (muteArea().contains (e.getPosition()))
            track.setProperty (IDs::muted, ! (bool) track.getProperty (IDs::muted, false), nullptr);
        else if (soloArea().contains (e.getPosition()))
            track.setProperty (IDs::soloed, ! (bool) track.getProperty (IDs::soloed, false), nullptr);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (resizeFrom > 0)
        {
            if (onResize)
                onResize (resizeFrom + e.getDistanceFromDragStartY());
            return;
        }
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
    int resizeFrom = 0;   // the height when a bottom-edge drag started
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

    // Snap to grid, like Reaper: the step from the Grid box, on with Snap; Alt held does the opposite for one move.
    int gridStep = 0;
    bool snapOn = true;
    std::function<void (double factor, int anchorX)> onZoom;   // Alt / Option + mouse wheel
    std::function<void (int steps)> onTrackHeight;               // Cmd / Ctrl + mouse wheel (onZoom: Alt / Option)
    int trackHeight = defaultTrackHeight;
    int trackTop (int index) const { return clickLaneTop + clickLaneHeight + index * trackHeight; }

    // The backing track: drawn under the tracks; drag it sideways to line it up with bar 1.
    juce::AudioThumbnail* thumbnail = nullptr;
    std::function<void()> onAddAudio;
    std::function<void (int x)> onAudioMenu;
    std::function<void (double beat)> onPlayFrom;
    std::function<void (juce::ValueTree section)> onTapTempo;

    double beatAt (int x) const { return juce::jmax (0.0, x / pixelsPerBeat); }
    int xOf (double beat) const { return juce::roundToInt (beat * pixelsPerBeat); }
    int audioTop() const { return trackTop ((int) songs::tracks (song).size()); }
    int newLaneTop() const { return audioTop() + audioHeight; }
    int contentHeight() const { return newLaneTop() + newLaneHeight + 4; }

    double snap (double beat) const
    {
        if (! song.isValid())
            return 0.0;
        const auto on = snapOn != juce::ModifierKeys::currentModifiers.isAltDown();
        return on ? songs::snapBeat (song, beat, gridStep) : juce::jmax (0.0, std::round (beat * 96.0) / 96.0);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override
    {
        // Like Reaper: Cmd / Ctrl + wheel = track height, Alt / Option + wheel = zoom.
        const auto delta = wheel.deltaY != 0.0f ? wheel.deltaY : wheel.deltaX;   // some systems turn a modified wheel sideways
        if ((e.mods.isCommandDown() || e.mods.isCtrlDown()) && onTrackHeight)
            onTrackHeight (delta > 0 ? 1 : -1);
        else if (e.mods.isAltDown() && onZoom)
            onZoom (delta > 0 ? 1.25 : 0.8, e.x);
        else
            juce::Component::mouseWheelMove (e, wheel);   // the viewport scrolls
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

        // Lanes per track (two at the default height, more when taller), so cues close together don't hide each other.
        const auto lanes = (size_t) juce::jmax (1, (trackHeight - 8) / (blockHeight + 2));
        std::vector<std::vector<int>> laneEnds (list.size(), std::vector<int> (lanes, -1000));
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
                           + ". Click to select (Shift adds), then Copy / Paste. Drag to move it (hold Alt to switch snapping for the move); right-click for more.");
            const auto x = xOf (beat);
            const auto width = juce::jmax (juce::roundToInt (juce::jmax (1.0, (double) c.getProperty (IDs::lengthBeats, 1.0)) * pixelsPerBeat), 74);
            // The first lane that's free here, else the one that frees up first.
            auto& ends = laneEnds[(size_t) index];
            auto lane = (int) (std::find_if (ends.begin(), ends.end(), [x] (int end) { return x >= end; }) - ends.begin());
            if (lane == (int) ends.size())
                lane = (int) (std::min_element (ends.begin(), ends.end()) - ends.begin());
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
        paintAudio (g);

        // The lane for a new track.
        auto lane = juce::Rectangle<int> (0, newLaneTop() + 4, w, newLaneHeight - 4);
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
            g.drawText (s[IDs::bars].toString() + ((int) s[IDs::bars] == 1 ? " bar  " : " bars  ") + meterText (s) + "  " + bpmText ((double) s.getProperty (IDs::bpm, 120.0))
                        + (songs::rampsTempo (song, s) ? " " + juce::String::charToString ((juce::juce_wchar) 0x2192) + " " + bpmText (songs::sectionEndBpm (song, s)) + " BPM"
                                                       : juce::String (songs::tempoChanges (s).empty() ? " BPM" : " BPM, changes")),
                        label, juce::Justification::topLeft, true);

            if (song[IDs::loopSection].toString() == s[IDs::uid].toString())   // the looped section: a band over its bar numbers
            {
                g.setColour (ledGreen.withAlpha (0.25f));
                g.fillRect (x0, rulerHeight, x1 - x0, barsHeight);
                g.setColour (ledGreen);
                g.fillRect (x0, rulerHeight + barsHeight - 2, x1 - x0, 2);
            }
            const auto bb = songs::barBeats (s);
            const auto bars = juce::jmax (1, (int) s.getProperty (IDs::bars, 1));
            // Grid lines at the grid step, when they're far enough apart to read.
            const auto step = songs::gridStepBeats (gridStep, s);
            if (step * pixelsPerBeat >= 6.0 && step < bb - 1.0e-9)
            {
                for (double b = step; b < length - 1.0e-9; b += step)
                    if (std::fmod (b, bb) > 1.0e-6 && bb - std::fmod (b, bb) > 1.0e-6)
                    {
                        g.setColour (outline.withAlpha (0.28f));
                        g.drawVerticalLine (xOf (start + b), (float) trackTop (0), (float) audioTop());
                        // ...and a tick on the ruler, so the Grid you pick shows where clips and the song position snap.
                        g.setColour (dim.withAlpha (0.55f));
                        g.drawVerticalLine (xOf (start + b), (float) (rulerHeight + barsHeight - 5), (float) (rulerHeight + barsHeight));
                    }
            }
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
            paintClickLane (g, s, start, length, colour);
            start += length;
        }
        paintTempoLane (g);
        g.setColour (outline.brighter (0.4f));
        g.drawVerticalLine (xOf (start), (float) rulerHeight, (float) trackTop (count));

        // The song position (click a track or the bar numbers): Play starts here, Paste puts cues here (on the clicked track).
        if (cursorBeat >= 0.0)
        {
            const auto x = (float) xOf (cursorBeat);
            g.setColour (builderViolet);
            g.fillRect (x - 0.5f, (float) rulerHeight, 2.0f, (float) (newLaneTop() - rulerHeight));
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
            g.fillRect ((float) xOf (playBeat) - 0.5f, (float) rulerHeight, 2.0f, (float) (newLaneTop() - rulerHeight));
        }

        if (dropBeat >= 0.0)
        {
            const auto x = xOf (dropBeat);
            const auto top = dropTrack >= 0 ? trackTop (dropTrack) : newLaneTop() + 4;
            const auto h = dropTrack >= 0 ? trackHeight : newLaneHeight - 4;
            g.setColour (accent);
            g.fillRect (x - 1, top, 3, h);
            g.setFont (font (11.0f, true));
            g.drawText (songs::barLabel (song, dropBeat), x + 5, top + h - 17, 140, 15, juce::Justification::centredLeft, false);
        }
    }

    // The tempo lane: the tempo as a line (low to high across the song), a dot and its value at every change; a gliding
    // stretch slopes. Like Reaper's tempo envelope, but the bars stay where they are.
    // Where a tempo sits in the lane: the song's slowest at the bottom, its fastest at the top.
    std::function<float (double)> tempoY() const
    {
        const auto segments = songs::tempoSegments (song);
        double lo = 1.0e9, hi = 0.0;
        for (const auto& t : segments)
        {
            lo = juce::jmin (lo, t.bpm0, t.bpm1);
            hi = juce::jmax (hi, t.bpm0, t.bpm1);
        }
        const auto plot = juce::Rectangle<int> (0, tempoLaneTop, getWidth(), tempoLaneHeight).reduced (0, 2).toFloat()
                              .reduced (0.0f, 4.0f).withTrimmedTop (1.0f);
        return [plot, lo, hi] (double bpm) { return hi - lo < 1.0e-6 ? plot.getCentreY() : plot.getBottom() - (float) ((bpm - lo) / (hi - lo)) * plot.getHeight(); };
    }

    bool onTempoLine (juce::Point<int> p) const
    {
        const auto beat = beatAt (p.x);
        return song.isValid() && beat < songs::songLengthBeats (song) && std::abs (tempoY() (songs::tempoAt (song, beat)) - (float) p.y) <= 5.0f;
    }

    void paintTempoLane (juce::Graphics& g)
    {
        const auto lane = juce::Rectangle<int> (0, tempoLaneTop, getWidth(), tempoLaneHeight).reduced (0, 2);
        g.setColour (surface.withAlpha (0.55f));
        g.fillRect (lane.withWidth (xOf (songs::songLengthBeats (song))));
        const auto segments = songs::tempoSegments (song);
        if (segments.empty())
            return;
        const auto yOf = tempoY();
        juce::Path line;
        for (size_t i = 0; i < segments.size(); ++i)
        {
            const auto& t = segments[i];
            const auto x0 = (float) xOf (t.from), x1 = (float) xOf (t.to);
            if (i == 0)
                line.startNewSubPath (x0, yOf (t.bpm0));
            else
                line.lineTo (x0, yOf (t.bpm0));
            line.lineTo (x1, yOf (t.bpm1));
        }
        g.setColour (accent.withAlpha (0.9f));
        g.strokePath (line, juce::PathStrokeType (1.5f));

        // The changes inside sections, with their values; a section's own tempo sits in its header.
        g.setFont (font (10.0f, true));
        double start = 0.0;
        for (const auto& s : song)
        {
            if (! s.hasType (IDs::SongSection))
                continue;
            for (const auto& c : songs::tempoChanges (s))
            {
                const auto x = (float) xOf (start + (double) c[IDs::beat]);
                const auto y = yOf ((double) c[IDs::bpm]);
                const auto picked = c == draggingTempo || c == hoverTempo;
                g.setColour (picked ? text : accent);
                g.fillEllipse (juce::Rectangle<float> (picked ? 9.0f : 7.0f, picked ? 9.0f : 7.0f).withCentre ({ x, y }));
                const auto label = bpmText ((double) c[IDs::bpm]) + ((bool) c[IDs::tempoRamp] ? " " + juce::String::charToString ((juce::juce_wchar) 0x2192) : juce::String());
                g.setColour (text.withAlpha (0.9f));
                g.drawText (label, juce::Rectangle<float> (x + 6.0f, (float) lane.getY(), 60.0f, (float) lane.getHeight()),
                            juce::Justification::centredLeft, false);
            }
            start += songs::sectionLengthBeats (s);
        }

        // While dragging: the tempo, next to the mouse.
        const auto owner = draggingTempo.isValid() ? draggingTempo : tempoLineOwner;
        if (owner.isValid())
        {
            const auto label = bpmText ((double) owner[IDs::bpm]) + " BPM";
            g.setFont (font (11.5f, true));
            const auto w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 14;
            auto pill = juce::Rectangle<int> (tempoDragX + 12, tempoLaneTop + 2, w, tempoLaneHeight - 4);
            if (pill.getRight() > getWidth() - 4)
                pill.setX (tempoDragX - 12 - w);
            g.setColour (accent);
            g.fillRoundedRectangle (pill.toFloat(), 5.0f);
            g.setColour (juce::Colours::black);
            g.drawText (label, pill, juce::Justification::centred, false);
        }
    }

    // Whose tempo the line has at a beat: the last change before it in its section, or the section itself.
    juce::ValueTree tempoOwnerAt (double beat) const
    {
        const auto section = songs::sectionAt (song, beat);
        juce::ValueTree owner = section;
        const auto into = beat - songs::sectionStartBeat (song, section);
        for (const auto& c : songs::tempoChanges (section))
            if ((double) c[IDs::beat] <= into + 1.0e-9)
                owner = c;
        return owner;
    }

    // The tempo change near x on the tempo lane (6 px), or none.
    juce::ValueTree tempoChangeAt (int x) const
    {
        juce::ValueTree best;
        int bestDistance = 7;
        double start = 0.0;
        for (const auto& s : song)
        {
            if (! s.hasType (IDs::SongSection))
                continue;
            for (const auto& c : songs::tempoChanges (s))
                if (const auto d = std::abs (xOf (start + (double) c[IDs::beat]) - x); d < bestDistance)
                {
                    bestDistance = d;
                    best = c;
                }
            start += songs::sectionLengthBeats (s);
        }
        return best;
    }

    void tempoMenu (juce::ValueTree change, double beat)
    {
        auto section = change.isValid() ? change.getParent() : songs::sectionAt (song, beat);
        if (! section.isValid())
            return;
        const auto into = juce::jlimit (0.0, songs::sectionLengthBeats (section), beat - songs::sectionStartBeat (song, section));
        juce::PopupMenu m;
        if (change.isValid())
        {
            m.addSectionHeader ("Tempo change: " + bpmText ((double) change[IDs::bpm]) + " BPM, " + songs::barLabel (song, beat));
            m.addItem (1, "Edit tempo change...");
            m.addItem (2, "Gradually transition to the next tempo", true, (bool) change.getProperty (IDs::tempoRamp, false));
            m.addItem (3, "Delete tempo change", true);
        }
        else
        {
            m.addSectionHeader (songs::barLabel (song, beat));
            m.addItem (4, "Add a tempo change here...", into > 1.0e-6);
            m.addItem (5, "\"" + section[IDs::name].toString() + "\": gradually transition from its start", true,
                       (bool) section.getProperty (IDs::tempoRamp, false));
        }
        auto songTree = song;
        m.showMenuAsync (juce::PopupMenu::Options(), [songTree, change, section, into] (int r) mutable
        {
            if (r == 1)
                editTempoDialog (change);
            else if (r == 2)
                change.setProperty (IDs::tempoRamp, ! (bool) change.getProperty (IDs::tempoRamp, false), nullptr);
            else if (r == 3)
                section.removeChild (change, nullptr);
            else if (r == 4)
                editTempoDialog (songs::addTempoChange (section, into, songs::tempoAt (songTree, songs::sectionStartBeat (songTree, section) + into)), true);
            else if (r == 5)
                section.setProperty (IDs::tempoRamp, ! (bool) section.getProperty (IDs::tempoRamp, false), nullptr);
        });
    }

    // The click track lane: a block per section with its count, and a tick per click (bars tallest, subdivisions shortest).
    void paintClickLane (juce::Graphics& g, const juce::ValueTree& s, double start, double length, juce::Colour colour)
    {
        const auto x0 = xOf (start), x1 = xOf (start + length);
        auto lane = juce::Rectangle<int> (x0, clickLaneTop, x1 - x0, clickLaneHeight).reduced (1, 3);
        g.setColour (colour.withAlpha (0.10f));
        g.fillRoundedRectangle (lane.toFloat(), 4.0f);
        const auto bb = songs::barBeats (s);
        const auto beatUnit = 4.0 / juce::jmax (1, (int) s.getProperty (IDs::timeDen, 4));
        auto step = songs::clickStepBeats (s);
        if (step > 0.0 && step * pixelsPerBeat < 2.5)   // too dense to draw at this zoom: the beats, or the bars
            step = beatUnit * pixelsPerBeat >= 2.5 ? juce::jmax (step, beatUnit) : bb;
        if (step > 0.0)
            for (int k = 0; k * step < length - 1.0e-9; ++k)
            {
                const auto b = k * step;
                const auto inBar = std::fmod (b, bb);
                const auto onBar = inBar < 1.0e-6 || bb - inBar < 1.0e-6;
                const auto inBeat = std::fmod (inBar, beatUnit);
                const auto onBeat = inBeat < 1.0e-6 || beatUnit - inBeat < 1.0e-6;
                const auto h = onBar ? lane.getHeight() - 4 : onBeat ? lane.getHeight() / 2 : lane.getHeight() / 4;
                g.setColour (onBar ? text.withAlpha (0.85f) : dim.withAlpha (onBeat ? 0.8f : 0.5f));
                g.fillRect (xOf (start + b), lane.getBottom() - 2 - h, 1, h);
            }
        if (lane.getWidth() > 46)
        {
            const auto label = songs::clickDivNames()[juce::jlimit (0, 7, (int) s.getProperty (IDs::clickDiv, 0))];
            g.setFont (font (10.5f, true));
            const auto w = juce::GlyphArrangement::getStringWidthInt (g.getCurrentFont(), label) + 8;
            auto tag = lane.withWidth (w).withTrimmedLeft (3).withSizeKeepingCentre (w, 15).translated (3, 0);
            g.setColour (surface.withAlpha (0.9f));
            g.fillRoundedRectangle (tag.toFloat(), 3.0f);
            g.setColour (text);
            g.drawText (label, tag, juce::Justification::centred, false);
        }
    }

    void clickLaneMenu (juce::ValueTree section)
    {
        if (! section.isValid())
            return;
        const auto div = (int) section.getProperty (IDs::clickDiv, 0);
        juce::PopupMenu m, all;
        m.addSectionHeader ("Click in " + section[IDs::name].toString());
        const auto names = songs::clickDivNames();
        for (int i = 0; i < names.size(); ++i)
        {
            m.addItem (1 + i, names[i] + (i == 0 ? "  (" + meterText (section) + ")" : juce::String()), true, div == i);
            all.addItem (101 + i, names[i]);
        }
        m.addSeparator();
        m.addSubMenu ("Every section", all);
        auto songTree = song;
        m.showMenuAsync (juce::PopupMenu::Options(), [songTree, section] (int r) mutable
        {
            if (r >= 1 && r <= 8)
                section.setProperty (IDs::clickDiv, r - 1, nullptr);
            else if (r >= 101 && r <= 108)
                for (auto sTree : songTree)
                    if (sTree.hasType (IDs::SongSection))
                        sTree.setProperty (IDs::clickDiv, r - 101, nullptr);
        });
    }

    void paintAudio (juce::Graphics& g)
    {
        auto lane = juce::Rectangle<int> (0, audioTop() + 4, getWidth(), audioHeight - 8);
        g.setColour (surface.withAlpha (0.7f));
        g.fillRoundedRectangle (lane.toFloat(), 6.0f);
        const auto file = song[IDs::audioFile].toString();
        if (file.isEmpty() || thumbnail == nullptr || thumbnail->getTotalLength() <= 0.0)
        {
            g.setColour (outline);
            g.drawRoundedRectangle (lane.toFloat().reduced (0.5f), 6.0f, 1.0f);
            g.setColour (dim);
            g.setFont (font (12.5f));
            g.drawText (file.isEmpty() ? juce::String ("Click to add a backing track (MP3, WAV, AIFF, FLAC) to play along")
                                       : "Can't read " + juce::File (file).getFileName() + ": right-click to replace it",
                        lane.reduced (12, 0), juce::Justification::centredLeft, true);
            return;
        }
        // The waveform, piece by piece through the tempo map (a steady stretch at a time; a gliding one in small pieces):
        // song time + offset = time in the file.
        const auto offset = dragging ? dragOffset : (double) song.getProperty (IDs::audioOffset, 0.0);
        g.setColour (juce::Colour (0xff9b87f5).withAlpha (0.75f));
        for (const auto& seg : songs::tempoSegments (song))
        {
            const auto pieces = std::abs (seg.bpm1 - seg.bpm0) < 1.0e-9 ? 1 : juce::jmax (1, (xOf (seg.to) - xOf (seg.from)) / 6);
            for (int k = 0; k < pieces; ++k)
            {
                const auto from = seg.from + (seg.to - seg.from) * k / pieces, to = seg.from + (seg.to - seg.from) * (k + 1) / pieces;
                const auto t0 = songs::beatToSeconds (song, from) + offset, t1 = songs::beatToSeconds (song, to) + offset;
                const auto x0 = xOf (from), x1 = xOf (to);
                const auto a = juce::jlimit (0.0, thumbnail->getTotalLength(), t0), b = juce::jlimit (0.0, thumbnail->getTotalLength(), t1);
                if (b > a && t1 > t0 && x1 > x0)
                {
                    const auto px0 = x0 + juce::roundToInt ((a - t0) / (t1 - t0) * (x1 - x0));
                    const auto px1 = x0 + juce::roundToInt ((b - t0) / (t1 - t0) * (x1 - x0));
                    thumbnail->drawChannels (g, { px0, lane.getY() + 2, juce::jmax (1, px1 - px0), lane.getHeight() - 4 }, a, b, 0.9f);
                }
            }
        }
        g.setColour (dim);
        g.setFont (font (11.0f));
        g.drawText (juce::File (file).getFileName() + (std::abs (offset) > 1.0e-3 ? "  (bar 1 at " + juce::String (offset, 2) + " s)" : juce::String()),
                    lane.reduced (8, 3), juce::Justification::topLeft, true);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        // The tempo lane, like a point in an envelope: up / down sets the tempo (1 BPM a pixel, Shift 0.1), left / right
        // moves a change along its section on the grid. Dragging the line itself only changes that stretch's tempo.
        if (draggingCursor)
        {
            cursorBeat = juce::jlimit (0.0, songs::songLengthBeats (song), snap (beatAt (e.x)));
            repaint();
            return;
        }
        if (draggingTempo.isValid() || tempoLineOwner.isValid())
        {
            auto owner = draggingTempo.isValid() ? draggingTempo : tempoLineOwner;
            tempoDragX = e.x;
            const auto step = e.mods.isShiftDown() ? 0.1 : 1.0;
            const auto bpm = std::round ((tempoDragBpm + (e.getMouseDownY() - e.y) * step) / step) * step;
            owner.setProperty (IDs::bpm, juce::jlimit (songs::minBpm, songs::maxBpm, bpm), nullptr);
            if (draggingTempo.isValid() && std::abs (e.getDistanceFromDragStartX()) > 3)
            {
                const auto section = draggingTempo.getParent();
                const auto start = songs::sectionStartBeat (song, section), length = songs::sectionLengthBeats (section);
                const auto into = juce::jlimit (1.0 / 96.0, length - 1.0 / 96.0, snap (beatAt (e.x)) - start);
                draggingTempo.setProperty (IDs::beat, std::round (into * 960.0) / 960.0, nullptr);
            }
            repaint();
            return;
        }
        if (! dragging)
            return;
        // Dragging right moves the audio later: bar 1 lands earlier in the file.
        dragOffset = dragStartOffset - (songs::beatToSeconds (song, beatAt (e.x)) - songs::beatToSeconds (song, beatAt (e.getMouseDownX())));
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto inLane = e.y >= tempoLaneTop && e.y < clickLaneTop;
        const auto onTempo = inLane ? tempoChangeAt (e.x) : juce::ValueTree();
        if (onTempo != hoverTempo)
        {
            hoverTempo = onTempo;
            repaint();
        }
        if (inLane)
        {
            setMouseCursor (onTempo.isValid() ? juce::MouseCursor::DraggingHandCursor
                            : onTempoLine (e.getPosition()) ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor);
            return;
        }
        // The song position's line can be dragged when not playing.
        if (e.y >= rulerHeight && playBeat < 0.0 && cursorBeat >= 0.0 && std::abs (e.x - xOf (cursorBeat)) <= 4)
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
            hoverSection = {};
            return;
        }
        const auto s = e.y < rulerHeight ? songs::sectionAt (song, beatAt (e.x)) : juce::ValueTree();
        setMouseCursor (s.isValid() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        if (s != hoverSection)
        {
            hoverSection = s;
            repaint();
        }
    }
    void mouseExit (const juce::MouseEvent&) override { hoverSection = {}; hoverTempo = {}; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (e.y < rulerHeight || ! song.isValid())
            return;
        if (e.y >= tempoLaneTop && e.y < clickLaneTop)   // the tempo lane: drag a change, right-click for the menu
        {
            auto change = tempoChangeAt (e.x);
            if (change.isValid() && e.mods.isAltDown())   // Alt / Option-click deletes it, like a point in an envelope
            {
                change.getParent().removeChild (change, nullptr);
                hoverTempo = {};
                return;
            }
            if (e.mods.isPopupMenu())
                tempoMenu (change, change.isValid() ? songs::sectionStartBeat (song, change.getParent()) + (double) change[IDs::beat]
                                                    : snap (beatAt (e.x)));
            else if (change.isValid())
            {
                draggingTempo = change;
                tempoDragBpm = (double) change[IDs::bpm];
                tempoDragX = e.x;
            }
            else if (const auto beat = beatAt (e.x); onTempoLine (e.getPosition()))
            {
                tempoDragX = e.x;
                tempoLineOwner = tempoOwnerAt (beat);   // the line: drag it up or down
                tempoDragBpm = (double) tempoLineOwner.getProperty (IDs::bpm, 120.0);
            }
            return;
        }
        if (e.y >= audioTop() && e.y < newLaneTop())   // the backing-track lane
        {
            if (e.mods.isPopupMenu())
            {
                if (onAudioMenu)
                    onAudioMenu (e.x);
            }
            else if (song[IDs::audioFile].toString().isEmpty() || thumbnail == nullptr || thumbnail->getTotalLength() <= 0.0)
            {
                if (onAddAudio)
                    onAddAudio();
            }
            else
            {
                dragging = true;
                dragStartOffset = dragOffset = (double) song.getProperty (IDs::audioOffset, 0.0);
            }
            return;
        }
        if (e.y >= clickLaneTop && e.y < trackTop (0) && e.mods.isPopupMenu())   // the click lane: how this section counts
        {
            clickLaneMenu (songs::sectionAt (song, beatAt (e.x)));
            return;
        }
        // The ruler (bar numbers and the click lane), or the song position's line itself when not playing: set the song
        // position, and drag it along.
        const auto onCursorLine = playBeat < 0.0 && cursorBeat >= 0.0 && std::abs (e.x - xOf (cursorBeat)) <= 4
                                  && getComponentAt (e.getPosition()) == this;
        if (e.y < trackTop (0) || onCursorLine)
        {
            cursorBeat = juce::jlimit (0.0, songs::songLengthBeats (song), snap (beatAt (e.x)));
            draggingCursor = true;
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
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.y < tempoLaneTop || e.y >= clickLaneTop || ! song.isValid() || e.mods.isAltDown())
            return;   // (an Alt-double-click is two deletes, not an add)
        if (const auto change = tempoChangeAt (e.x); change.isValid())
        {
            editTempoDialog (change);
            return;
        }
        const auto beat = snap (beatAt (e.x));
        const auto section = songs::sectionAt (song, beat);
        const auto into = beat - songs::sectionStartBeat (song, section);
        if (section.isValid() && into > 1.0e-6 && into < songs::sectionLengthBeats (section) - 1.0e-6)
            editTempoDialog (songs::addTempoChange (section, into, songs::tempoAt (song, beat)), true);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (draggingCursor)
        {
            draggingCursor = false;
            return;
        }
        if (draggingTempo.isValid() || tempoLineOwner.isValid())
        {
            draggingTempo = tempoLineOwner = {};
            repaint();
            return;
        }
        if (dragging)
        {
            dragging = false;
            auto songTree = song;
            songTree.setProperty (IDs::audioOffset, std::round (dragOffset * 1000.0) / 1000.0, nullptr);
            return;
        }
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
        m.addSeparator();
        m.addItem (7, "Play from here");
        m.addItem (8, "Loop this section while playing", true, song[IDs::loopSection].toString() == section[IDs::uid].toString());
        m.addItem (9, "Tap its tempo...");
        auto songTree = song;
        juce::Component::SafePointer<SongGrid> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [songTree, section, safe] (int r) mutable
        {
            const auto index = songTree.indexOf (section);
            if (r == 7 && safe != nullptr && safe->onPlayFrom)
                safe->onPlayFrom (songs::sectionStartBeat (songTree, section));
            else if (r == 8)
                songTree.setProperty (IDs::loopSection, songTree[IDs::loopSection].toString() == section[IDs::uid].toString()
                                                            ? juce::var() : section[IDs::uid], nullptr);
            else if (r == 9 && safe != nullptr && safe->onTapTempo)
                safe->onTapTempo (section);
            else if (r == 1)
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
                s.setProperty (IDs::tempoRamp, section[IDs::tempoRamp], nullptr);
                for (const auto& c : section)   // its tempo changes
                    if (c.hasType (IDs::SongTempo))
                        s.appendChild (c.createCopy(), nullptr);
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
    juce::ValueTree draggingTempo, hoverTempo;   // a tempo change being dragged / under the mouse
    juce::ValueTree tempoLineOwner;              // the change (or section) whose stretch of the line is dragged up or down
    double tempoDragBpm = 120.0;                 // its tempo when the drag started
    int tempoDragX = 0;                          // where the mouse is, for the tempo label
    bool draggingCursor = false;                 // the song position, dragged along the ruler
    double dropBeat = -1.0;
    int dropTrack = -1;
    std::vector<juce::ValueTree> selection;
    bool dragging = false;   // the backing track, sideways
    double dragOffset = 0.0, dragStartOffset = 0.0;
    static inline std::vector<songs::ClipCue> clipboard;   // shared by every song, so cues can be pasted into another song
};

//==============================================================================
// Drag the song into the DAW: the ticked tracks as one MIDI file, and the click track and backing track as WAVs when ticked,
// all starting at bar 1.
struct DragSongButton final : public juce::TextButton
{
    std::function<juce::StringArray()> makeFiles;
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 6 || ! makeFiles)
            return;
        started = true;
        const auto files = makeFiles();
        if (! files.isEmpty())
            juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);
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
    // mode: 0 one MIDI file, 1 a MIDI file per track, 2 a song package (MIDI + click WAV + backing WAV) for a player.
    ExportPanel (juce::ValueTree s, int countInBars, std::function<void (int mode, bool withCountIn)> onExportChosen)
        : song (std::move (s)), bars (countInBars), onExport (std::move (onExportChosen))
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
        package.setButtonText ("Song package");
        // What each choice writes, named as the files will be.
        const auto songName = juce::File::createLegalFileName (song[IDs::name].toString());
        juce::StringArray trackFiles;
        for (auto track : songs::tracks (song))
            if ((bool) track.getProperty (IDs::include, true))
                trackFiles.add ("\"" + juce::File::createLegalFileName (song[IDs::name].toString() + " - " + track[IDs::name].toString()) + ".mid\"");
        // Plain-ASCII literals: a non-ASCII character in a char* string shows as junk, so the bullet is made here.
        const auto bullet = juce::String::charToString ((juce::juce_wchar) 0x2022) + " ";
        oneFile.setTooltip ("Saves one MIDI file, \"" + songName + ".mid\", with:\n"
                            + bullet + "the tempo map, time signatures and section markers\n"
                            + bullet + "a MIDI track for each ticked track, every cue on its own MIDI channel\n"
                              "In your DAW, one track that sends on the original channels reaches every device on that MIDI output.");
        juce::String perTrackList;
        for (const auto& f : trackFiles)
            perTrackList << "\n" << bullet << f << ": that track's cues";
        perTrack.setTooltip ("Saves a MIDI file for each ticked track, in the folder you pick:"
                             + (perTrackList.isEmpty() ? juce::String ("\n(no tracks ticked)") : perTrackList)
                             + "\nEach file also has the tempo map, time signatures and section markers. Use this when your "
                               "devices are on different MIDI outputs: one DAW track per device.");
        const auto hasBacking = song[IDs::audioFile].toString().isNotEmpty();
        package.setTooltip ("Saves a folder, \"" + songName + "\", for a backing-track player, with:\n"
                            + bullet + "\"" + songName + " - cues.mid\": the ticked tracks' cues, with the tempo map and section markers\n"
                            + bullet + "\"" + songName + " - click.wav\": the click track, as set in the click lane\n"
                            + (hasBacking ? bullet + "\"" + songName + " - backing.wav\": the backing track, lined up with bar 1\n"
                                          : bullet + "No backing track WAV: this song has no backing track\n")
                            + "All the files start at the same moment. The WAVs are 48 kHz, 24-bit.");
        countIn.setButtonText ("Start with a count-in (" + juce::String (bars) + (bars == 1 ? " bar)" : " bars)"));
        countIn.setTooltip ("The files start with " + juce::String (bars) + (bars == 1 ? " bar" : " bars")
                            + " of count-in: the MIDI and the backing track that much later, the click counting in. "
                              "The length follows Count-in on the song card (1 bar when it's off).");
        countIn.setToggleState (lastCountIn, juce::dontSendNotification);
        addAndMakeVisible (countIn);
        for (auto* b : { &oneFile, &perTrack, &package })
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
        oneFile.setToggleState (lastMode == 0, juce::dontSendNotification);
        perTrack.setToggleState (lastMode == 1, juce::dontSendNotification);
        package.setToggleState (lastMode == 2, juce::dontSendNotification);
        exportButton.onClick = [this]
        {
            apply();
            lastMode = package.getToggleState() ? 2 : perTrack.getToggleState() ? 1 : 0;
            lastCountIn = countIn.getToggleState();
            const auto mode = lastMode;
            const auto lead = lastCountIn;
            auto done = onExport;
            close();
            if (done)
                done (mode, lead);
        };
        addAndMakeVisible (exportButton);
        cancel.onClick = [this] { close(); };
        addAndMakeVisible (cancel);

        setSize (640, 150 + rows.size() * 36 + 76);
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
        oneFile.setBounds (choice.removeFromLeft (180));
        perTrack.setBounds (choice.removeFromLeft (172).withTrimmedLeft (4));
        package.setBounds (choice.removeFromLeft (172).withTrimmedLeft (4));
        r.removeFromTop (6);
        countIn.setBounds (r.removeFromTop (28).withTrimmedLeft (70));
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
    int bars = 0;
    std::function<void (int, bool)> onExport;
    juce::TextButton oneFile, perTrack, package;
    juce::ToggleButton countIn;
    juce::Label filesLabel;
    static inline int lastMode = 0;           // remembered while the app runs
    static inline bool lastCountIn = false;
    juce::Label intro;
    juce::OwnedArray<Row> rows;
    juce::TextButton exportButton { "Export..." }, cancel { "Cancel" };
};

//==============================================================================
// Click... : the metronome's sound, accent, volume and where it plays, and the gap between songs in a setlist.
class ClickPanel final : public juce::Component
{
public:
    std::function<void (float gain)> onLiveGain;   // the volume while it's dragged (saved when you let go)
    explicit ClickPanel (juce::ValueTree songsNode) : node (std::move (songsNode))
    {
        const auto c = songs::clickSettings (node);
        sound.addItemList (songs::clickSoundNames(), 1);
        sound.setSelectedId (c.sound + 1, juce::dontSendNotification);
        sound.onChange = [this] { node.setProperty (IDs::clickSound, sound.getSelectedId() - 1, nullptr); };
        accentToggle.setButtonText ("Accent the first beat of each bar");
        accentToggle.setToggleState (c.accent, juce::dontSendNotification);
        accentToggle.onClick = [this] { node.setProperty (IDs::clickAccent, accentToggle.getToggleState(), nullptr); };
        volume.setRange (0.0, 1.0, 0.01);
        volume.setValue (c.gain, juce::dontSendNotification);
        volume.setColour (juce::Slider::trackColourId, builderViolet);
        volume.onDragEnd = [this] { node.setProperty (IDs::clickGain, volume.getValue(), nullptr); };
        volume.onValueChange = [this] { if (onLiveGain) onLiveGain ((float) volume.getValue()); };   // heard while dragging
        route.addItemList ({ "Click and backing track on both sides", "Click left, backing track right",
                             "Click right, backing track left" }, 1);
        route.setSelectedId ((int) node.getProperty (IDs::clickRoute, 0) + 1, juce::dontSendNotification);
        route.setTooltip ("Where Play sends the click and the backing track. Both sides: mixed together in both ears. Click left / "
                          "backing track right (or the reverse): each on its own side of the output, e.g. the click to a "
                          "drummer's or your in-ears on one channel and the backing track to the front of house on the other. "
                          "Only for playing in PedalCues; exported WAVs are separate files.");
        route.onChange = [this] { node.setProperty (IDs::clickRoute, route.getSelectedId() - 1, nullptr); };
        for (auto [label, text] : { std::pair<juce::Label*, const char*> { &soundLabel, "SOUND" }, { &volumeLabel, "VOLUME" },
                                    { &routeLabel, "OUTPUT" } })
        {
            styleCaption (*label, text);
            addAndMakeVisible (label);
        }
        routeLabel.setTooltip (route.getTooltip());
        for (auto [button, id, what] : { std::tuple<juce::TextButton*, juce::Identifier, const char*> { &accentFile, IDs::clickFileAccent, "Accent" },
                                                                                                    { &beatFile, IDs::clickFileBeat, "Beat" } })
        {
            const auto path = node[id].toString();
            button->setButtonText (juce::String (what) + ": " + (path.isNotEmpty() ? juce::File (path).getFileName() : juce::String ("pick a WAV...")));
            button->setTooltip ("Your own click sound (WAV, AIFF, FLAC, MP3) for the " + juce::String (what).toLowerCase()
                                + (id == IDs::clickFileAccent ? " (each bar's first beat)" : " (the other beats; softer for subdivisions)"));
            button->onClick = [this, id = id, button = button]
            {
                chooser = std::make_unique<juce::FileChooser> ("Pick a click sound", juce::File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3");
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                      [this, id, button] (const juce::FileChooser& fc)
                {
                    if (fc.getResult() == juce::File())
                        return;
                    node.setProperty (id, fc.getResult().getFullPathName(), nullptr);
                    node.setProperty (IDs::clickSound, 4, nullptr);
                    sound.setSelectedId (5, juce::dontSendNotification);
                    button->setButtonText (button->getButtonText().upToFirstOccurrenceOf (":", false, false) + ": " + fc.getResult().getFileName());
                });
            };
        }
        for (auto* c2 : std::initializer_list<juce::Component*> { &sound, &accentToggle, &volume, &route, &accentFile, &beatFile })
            addAndMakeVisible (c2);
        setSize (440, 196);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        const auto row = [&] (juce::Label& l, juce::Component& c)
        {
            auto line = r.removeFromTop (34);
            l.setBounds (line.removeFromLeft (100));
            c.setBounds (line.reduced (0, 3));
        };
        row (soundLabel, sound);
        auto files = r.removeFromTop (32).withTrimmedLeft (100).reduced (0, 2);
        accentFile.setBounds (files.removeFromLeft (files.getWidth() / 2).withTrimmedRight (3));
        beatFile.setBounds (files.withTrimmedLeft (3));
        accentToggle.setBounds (r.removeFromTop (30).withTrimmedLeft (100));
        row (volumeLabel, volume);
        row (routeLabel, route);
    }

private:
    juce::ValueTree node;
    juce::ComboBox sound, route;
    juce::ToggleButton accentToggle;
    juce::Slider volume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Label soundLabel, volumeLabel, routeLabel;
    juce::TextButton accentFile, beatFile;
    std::unique_ptr<juce::FileChooser> chooser;
};

// Tap tempo: tap the button (or T / Space) along with the song, then set the section's tempo, or every section's.
// The backing track, in one place: its file, volume, where bar 1 is in it, and whether the song is long enough for it.
// (Right-clicking the waveform still lines up a bar where you click.)
class BackingPanel final : public juce::Component
{
public:
    std::function<void()> onReplace;
    std::function<void (float gain)> onLiveGain;   // the volume while it's dragged (saved when you let go)

    BackingPanel (juce::ValueTree s, double fileSeconds) : song (std::move (s)), audioSeconds (fileSeconds)
    {
        styleCaption (fileLabel, "FILE");
        styleCaption (volumeLabel, "VOLUME");
        styleCaption (offsetLabel, "BAR 1 AT");
        styleCaption (lengthLabel, "LENGTH");
        for (auto* l : { &fileLabel, &volumeLabel, &offsetLabel, &lengthLabel })
            addAndMakeVisible (l);

        fileName.setFont (font (14.0f, true));
        fileName.setColour (juce::Label::textColourId, text);
        addAndMakeVisible (fileName);
        for (auto* b : { &replace, &remove, &earlier, &later, &fileStart, &extend })
        {
            b->setColour (juce::TextButton::buttonColourId, surfaceHi);
            addAndMakeVisible (b);
        }
        replace.onClick = [this] { if (onReplace) onReplace(); close(); };
        remove.setTooltip ("Take the backing track out of this song (the file itself stays where it is)");
        remove.onClick = [this]
        {
            song.removeProperty (IDs::audioFile, nullptr);
            song.removeProperty (IDs::audioOffset, nullptr);
            close();
        };

        volume.textFromValueFunction = [] (double v) { return juce::String (juce::roundToInt (v * 100.0)) + " %"; };
        volume.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue() / 100.0; };
        volume.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);
        volume.setRange (0.0, 1.0, 0.01);
        volume.setValue ((double) song.getProperty (IDs::audioGain, 0.8), juce::dontSendNotification);
        volume.updateText();
        volume.setColour (juce::Slider::trackColourId, builderViolet);
        volume.onDragEnd = [this] { song.setProperty (IDs::audioGain, volume.getValue(), nullptr); };
        volume.onValueChange = [this]
        {
            if (onLiveGain)
                onLiveGain ((float) volume.getValue());
            if (! volume.isMouseButtonDown())   // typed in the box
                song.setProperty (IDs::audioGain, volume.getValue(), nullptr);
        };
        addAndMakeVisible (volume);

        offset.setJustification (juce::Justification::centred);
        offset.setInputRestrictions (10, "-0123456789.");
        offset.setTooltip ("Seconds into the file where bar 1 is (negative: the audio starts after bar 1). Type and press Return, "
                           "or drag the waveform sideways.");
        offset.onReturnKey = [this] { setOffset (offset.getText().getDoubleValue()); };
        offset.onFocusLost = [this] { setOffset (offset.getText().getDoubleValue()); };
        addAndMakeVisible (offset);
        seconds.setText ("seconds into the file", juce::dontSendNotification);
        seconds.setColour (juce::Label::textColourId, dim);
        seconds.setFont (font (12.5f));
        addAndMakeVisible (seconds);
        earlier.setTooltip ("Bar 1 10 ms earlier in the file");
        later.setTooltip ("Bar 1 10 ms later in the file");
        earlier.onClick = [this] { setOffset ((double) song.getProperty (IDs::audioOffset, 0.0) - 0.01); };
        later.onClick = [this] { setOffset ((double) song.getProperty (IDs::audioOffset, 0.0) + 0.01); };
        fileStart.setTooltip ("Bar 1 right at the start of the file");
        fileStart.onClick = [this] { setOffset (0.0); };

        lengthText.setColour (juce::Label::textColourId, dim);
        lengthText.setFont (font (12.5f));
        lengthText.setJustificationType (juce::Justification::centredLeft);
        addAndMakeVisible (lengthText);
        extend.onClick = [this]
        {
            songs::extendLastSection (song, songs::barsToCoverAudio (song, audioSeconds));
            refresh();
        };

        hint.setText ("To line up a bar with the recording, right-click the waveform on a downbeat.", juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, dim);
        hint.setFont (font (12.0f));
        addAndMakeVisible (hint);
        refresh();
        setSize (560, 214);
    }

    void paint (juce::Graphics& g) override { g.fillAll (surface); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        const auto row = [&] (juce::Label& l) { auto line = r.removeFromTop (36); l.setBounds (line.removeFromLeft (84)); return line.reduced (0, 4); };
        {
            auto line = row (fileLabel);
            remove.setBounds (line.removeFromRight (78));
            line.removeFromRight (6);
            replace.setBounds (line.removeFromRight (90));
            line.removeFromRight (8);
            fileName.setBounds (line);
        }
        volume.setBounds (row (volumeLabel));
        {
            auto line = row (offsetLabel);
            offset.setBounds (line.removeFromLeft (70));
            line.removeFromLeft (6);
            seconds.setBounds (line.removeFromLeft (150));
            fileStart.setBounds (line.removeFromRight (80));
            line.removeFromRight (6);
            later.setBounds (line.removeFromRight (64));
            line.removeFromRight (4);
            earlier.setBounds (line.removeFromRight (64));
        }
        {
            auto line = row (lengthLabel);
            extend.setBounds (line.removeFromRight (150));
            line.removeFromRight (8);
            lengthText.setBounds (line);
        }
        r.removeFromTop (6);
        hint.setBounds (r.removeFromTop (22));
    }

private:
    void setOffset (double secondsIntoFile)
    {
        song.setProperty (IDs::audioOffset, std::round (secondsIntoFile * 1000.0) / 1000.0, nullptr);
        refresh();
    }

    void refresh()
    {
        fileName.setText (juce::File (song[IDs::audioFile].toString()).getFileName(), juce::dontSendNotification);
        offset.setText (juce::String ((double) song.getProperty (IDs::audioOffset, 0.0), 3), juce::dontSendNotification);
        const auto bars = songs::barsToCoverAudio (song, audioSeconds);
        lengthText.setText (bars > 0 ? "The audio runs about " + juce::String (bars) + (bars == 1 ? " bar" : " bars") + " past the song's end."
                                     : juce::String ("The song is long enough for the audio."), juce::dontSendNotification);
        extend.setButtonText (bars > 0 ? "Add " + juce::String (bars) + (bars == 1 ? " bar" : " bars") + " to the end" : juce::String ("Nothing to add"));
        extend.setEnabled (bars > 0);
        extend.setTooltip ("Adds the bars to the last section, at the tempo it ends on, so the whole recording plays");
    }

    void close()
    {
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
    }

    juce::ValueTree song;
    double audioSeconds;
    juce::Label fileLabel, volumeLabel, offsetLabel, lengthLabel, fileName, seconds, lengthText, hint;
    juce::TextButton replace { "Replace..." }, remove { "Remove" }, earlier { "-10 ms" }, later { "+10 ms" }, fileStart { "File start" },
                     extend { "Add bars" };
    juce::Slider volume { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::TextEditor offset;
};

class TapPanel final : public juce::Component
{
public:
    TapPanel (juce::ValueTree s, juce::ValueTree sec) : song (std::move (s)), section (std::move (sec))
    {
        setWantsKeyboardFocus (true);
        tap.setButtonText ("Tap");
        tap.setColour (juce::TextButton::buttonColourId, builderViolet);
        tap.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        tap.setTriggeredOnMouseDown (true);
        tap.onClick = [this] { addTap(); };
        addAndMakeVisible (tap);
        bpm.setJustificationType (juce::Justification::centred);
        bpm.setFont (font (28.0f, true));
        bpm.setColour (juce::Label::textColourId, text);
        addAndMakeVisible (bpm);
        hint.setText ("Tap along with the song (or press T / Space). Taps more than 2 s apart start over.", juce::dontSendNotification);
        hint.setColour (juce::Label::textColourId, dim);
        hint.setFont (font (12.0f));
        hint.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (hint);
        setThis.setButtonText ("Set " + section[IDs::name].toString());
        setAll.setButtonText ("Set every section");
        setThis.onClick = [this] { apply (false); };
        setAll.onClick = [this] { apply (true); };
        addAndMakeVisible (setThis);
        addAndMakeVisible (setAll);
        update();
        setSize (380, 230);
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (16, 12);
        tap.setBounds (r.removeFromTop (64));
        r.removeFromTop (6);
        bpm.setBounds (r.removeFromTop (40));
        hint.setBounds (r.removeFromTop (34));
        auto buttons = r.removeFromBottom (32);
        setAll.setBounds (buttons.removeFromRight (buttons.getWidth() / 2).withTrimmedLeft (4));
        setThis.setBounds (buttons.withTrimmedRight (4));
    }
    bool keyPressed (const juce::KeyPress& k) override
    {
        if (k.getTextCharacter() == 't' || k.getTextCharacter() == 'T' || k == juce::KeyPress::spaceKey)
        {
            addTap();
            return true;
        }
        return false;
    }
    void parentHierarchyChanged() override { if (isShowing()) grabKeyboardFocus(); }

private:
    void addTap()
    {
        const auto now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        if (! taps.empty() && now - taps.back() > 2.0)
            taps.clear();
        taps.push_back (now);
        if (taps.size() > 16)
            taps.erase (taps.begin());
        update();
    }
    void update()
    {
        const auto t = songs::tempoFromTaps (taps);
        bpm.setText (t > 0.0 ? juce::String (t, 1) + " BPM" : juce::String ("-"), juce::dontSendNotification);
        setThis.setEnabled (t > 0.0);
        setAll.setEnabled (t > 0.0);
    }
    void apply (bool all)
    {
        const auto t = songs::tempoFromTaps (taps);
        if (t <= 0.0)
            return;
        for (auto sTree : song)
            if (sTree.hasType (IDs::SongSection) && (all || sTree == section))
                sTree.setProperty (IDs::bpm, t, nullptr);
        if (auto* w = findParentComponentOfClass<juce::DialogWindow>())
            w->exitModalState (0);
    }

    juce::ValueTree song, section;
    std::vector<double> taps;
    juce::TextButton tap, setThis, setAll;
    juce::Label bpm, hint;
};

//==============================================================================
// "Click track" next to its lane: M switches the metronome while playing, the tick sends it along with Drag song, and
// dragging the name drops the click track (a WAV of the whole song) into the DAW.
struct ClickLabel final : public juce::Component, public juce::SettableTooltipClient
{
    juce::ValueTree song, settings;
    std::function<juce::File()> makeFile;

    ClickLabel()
    {
        setTooltip ("The click track: right-click a section in its lane to set how it counts; click or drag the lane to set where Play starts. Drag this name into your DAW for the click as a "
                    "WAV (the whole song from bar 1). The tick sends it along with Drag song; M mutes the click while playing.");
    }
    juce::Rectangle<int> tickArea() const { return getLocalBounds().reduced (0, 2).removeFromRight (30); }
    juce::Rectangle<int> muteArea() const { return getLocalBounds().reduced (0, 2).withTrimmedRight (30).removeFromRight (22).withSizeKeepingCentre (20, 18); }

    void paint (juce::Graphics& g) override
    {
        auto row = getLocalBounds().reduced (0, 2);
        g.setColour (isMouseOver() ? raised : surfaceHi);
        g.fillRoundedRectangle (row.toFloat(), 6.0f);
        g.setColour (text.withAlpha (0.6f));
        g.fillRoundedRectangle (row.removeFromLeft (4).toFloat(), 2.0f);
        g.setColour (text);
        g.setFont (font (13.0f, true));
        g.drawText ("Click track", row.reduced (10, 0).withTrimmedRight (56), juce::Justification::centredLeft, true);
        paintTick (g, tickArea(), (bool) song.getProperty (IDs::clickInclude, true));
        const auto muted = ! (bool) settings.getProperty (IDs::metronome, true);
        g.setColour (muted ? accent : raised);
        g.fillRoundedRectangle (muteArea().toFloat(), 3.0f);
        g.setColour (muted ? juce::Colours::black : dim);
        g.setFont (font (11.0f, true));
        g.drawText ("M", muteArea(), juce::Justification::centred, false);
    }

    static void paintTick (juce::Graphics& g, juce::Rectangle<int> area, bool on)
    {
        auto tick = area.withSizeKeepingCentre (16, 16).toFloat();
        g.setColour (on ? builderViolet : outline.brighter (0.3f));
        if (on)
        {
            g.fillRoundedRectangle (tick, 3.0f);
            juce::Path p;
            p.startNewSubPath (tick.getX() + 3.5f, tick.getCentreY());
            p.lineTo (tick.getX() + 7.0f, tick.getBottom() - 4.0f);
            p.lineTo (tick.getRight() - 3.5f, tick.getY() + 4.0f);
            g.setColour (juce::Colours::black);
            g.strokePath (p, juce::PathStrokeType (2.0f));
        }
        else
            g.drawRoundedRectangle (tick, 3.0f, 1.5f);
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent&) override { started = false; }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (started)
            return;
        if (tickArea().contains (e.getPosition()))
            song.setProperty (IDs::clickInclude, ! (bool) song.getProperty (IDs::clickInclude, true), nullptr);
        else if (muteArea().contains (e.getPosition()))
            settings.setProperty (IDs::metronome, ! (bool) settings.getProperty (IDs::metronome, true), nullptr);
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 6 || ! makeFile)
            return;
        started = true;
        const auto f = makeFile();
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (f.getFullPathName()), false, this);
    }

private:
    bool started = false;
};

//==============================================================================
// The backing track's name next to its lane, with its volume.
struct AudioLabel final : public juce::Component, public juce::SettableTooltipClient
{
    juce::ValueTree song;
    juce::Slider volume { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    std::function<void()> onMenu, onAdd;
    juce::TextButton settings { "..." };
    std::function<void (float gain)> onLiveVolume;   // the volume while it's dragged (saved when you let go)
    std::function<juce::File()> makeFile;   // the backing track lined up with bar 1, as a WAV, for dragging into the DAW
    bool started = false;
    juce::Rectangle<int> tickArea() const { return getLocalBounds().reduced (0, 4).removeFromTop (26).removeFromRight (30); }

    AudioLabel()
    {
        volume.setRange (0.0, 1.0, 0.01);
        volume.setColour (juce::Slider::trackColourId, juce::Colour (0xff9b87f5));
        volume.setTooltip ("The backing track's volume");
        volume.onDragEnd = [this] { auto sTree = song; sTree.setProperty (IDs::audioGain, volume.getValue(), nullptr); };
        volume.onValueChange = [this] { if (onLiveVolume) onLiveVolume ((float) volume.getValue()); };   // heard while dragging
        addAndMakeVisible (volume);
        settings.setColour (juce::TextButton::buttonColourId, raised);
        settings.setTooltip ("Backing track settings: the file, volume, where bar 1 is, and the song's length");
        settings.onClick = [this] { if (onMenu) onMenu(); };
        addChildComponent (settings);
        setTooltip ("The backing track: plays with Play, after the count-in. Drag its waveform sideways to line it up with "
                    "bar 1. Drag this name into your DAW for it as a WAV lined up with bar 1; the tick sends it along with "
                    "Drag song. \"...\" (or right-click) opens its settings.");
    }

    void refresh()
    {
        const auto has = song[IDs::audioFile].toString().isNotEmpty();
        volume.setVisible (has);
        settings.setVisible (has);
        volume.setValue ((double) song.getProperty (IDs::audioGain, 0.8), juce::dontSendNotification);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto row = getLocalBounds().reduced (0, 4);
        g.setColour (isMouseOver() ? raised : surfaceHi);
        g.fillRoundedRectangle (row.toFloat(), 6.0f);
        g.setColour (juce::Colour (0xff9b87f5));
        g.fillRoundedRectangle (row.removeFromLeft (4).toFloat(), 2.0f);
        auto words = row.reduced (10, 5);
        g.setColour (text);
        g.setFont (font (13.0f, true));
        g.drawText ("Backing track", words.removeFromTop (18), juce::Justification::centredLeft, true);
        if (song[IDs::audioFile].toString().isEmpty())
        {
            g.setColour (dim);
            g.setFont (font (11.0f));
            g.drawText ("+ Add audio...", words, juce::Justification::centredLeft, true);
        }
    }

    juce::Rectangle<int> muteArea() const { return getLocalBounds().reduced (0, 4).removeFromTop (26).withTrimmedRight (30).removeFromRight (22).withSizeKeepingCentre (20, 18); }
    void paintOverChildren (juce::Graphics& g) override
    {
        if (song[IDs::audioFile].toString().isEmpty())
            return;
        const auto lit = (bool) song.getProperty (IDs::audioMuted, false);
        g.setColour (lit ? accent : raised);
        g.fillRoundedRectangle (muteArea().toFloat(), 3.0f);
        g.setColour (lit ? juce::Colours::black : dim);
        g.setFont (font (11.0f, true));
        g.drawText ("M", muteArea(), juce::Justification::centred, false);
        ClickLabel::paintTick (g, tickArea(), (bool) song.getProperty (IDs::audioInclude, true));
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 6 || ! makeFile || song[IDs::audioFile].toString().isEmpty())
            return;
        started = true;
        const auto f = makeFile();
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles (juce::StringArray (f.getFullPathName()), false, this);
    }
    void resized() override
    {
        volume.setBounds (getLocalBounds().reduced (0, 4).withTrimmedLeft (10).withTrimmedRight (6).removeFromBottom (24));
        settings.setBounds (getLocalBounds().reduced (0, 4).removeFromTop (26).withTrimmedRight (56).removeFromRight (26).withSizeKeepingCentre (24, 18));
    }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        started = false;
        if (e.mods.isPopupMenu() && onMenu)
            onMenu();
        else if (song[IDs::audioFile].toString().isEmpty() && onAdd)
            onAdd();
        else if (muteArea().contains (e.getPosition()))
        {
            auto sTree = song;
            sTree.setProperty (IDs::audioMuted, ! (bool) sTree.getProperty (IDs::audioMuted, false), nullptr);
            repaint();
        }
        else if (tickArea().contains (e.getPosition()))
        {
            auto sTree = song;
            sTree.setProperty (IDs::audioInclude, ! (bool) sTree.getProperty (IDs::audioInclude, true), nullptr);
            repaint();
        }
    }
};

// The songs list: click opens a song, ⌘ / Ctrl-click and Shift-click select several; ⌘C ⌘V ⌘D ⌘A and Delete work on
// the selection (right-click for the same).
struct SongList final : public juce::Component
{
    std::function<bool (const juce::KeyPress&)> onKey;
    SongList() { setWantsKeyboardFocus (true); }
    bool keyPressed (const juce::KeyPress& k) override { return onKey != nullptr && onKey (k); }
};

//==============================================================================
class SongBuilder final : public juce::Component, public juce::DragAndDropContainer, private juce::ChangeListener,
                          private juce::ValueTree::Listener, private juce::AsyncUpdater, private juce::Timer
{
public:
    SongBuilder (PedalCuesProcessor& p, std::function<juce::String()> deviceName)
        : proc (p), state (p.state), grid (p), saved (p)
    {
        addAndMakeVisible (saved);
        grid.deviceName = std::move (deviceName);
        grid.onTogglePlay = [this] { togglePlay(); };
        grid.thumbnail = &thumbnail;
        grid.onAddAudio = [this] { chooseAudio(); };
        grid.onAudioMenu = [this] (int x) { audioMenu (x); };
        grid.onPlayFrom = [this] (double beat)
        {
            stopPlaying();
            grid.cursorBeat = beat;
            togglePlay();
        };
        grid.onTapTempo = [this] (juce::ValueTree section) { openTapTempo (section); };
        clickButton.setButtonText ("Click...");
        clickButton.setTooltip ("The click's sound, accent, volume and output (e.g. click left, backing track right)");
        clickButton.onClick = [this]
        {
            auto panel = std::make_unique<ClickPanel> (songs::songsNode (state));
            juce::Component::SafePointer<SongBuilder> safe (this);
            panel->onLiveGain = [safe] (float gain)
            {
                if (safe != nullptr && safe->isTimerRunning())
                {
                    auto c = safe->clickSound();
                    c.gain = gain;
                    safe->proc.songAudio.setSound (c);
                }
            };
            juce::CallOutBox::launchAsynchronously (std::move (panel), clickButton.getScreenBounds(), nullptr);
        };
        addChildComponent (clickButton);
        grid.onZoom = [this] (double factor, int anchorX) { zoomBy (factor, anchorX); };
        grid.onTrackHeight = [this] (int steps) { setTrackHeight (grid.trackHeight + steps * 12); };
        thumbnail.addChangeListener (this);
        audioLabel.onAdd = [this] { chooseAudio(); };
        audioLabel.onLiveVolume = [this] (float gain) { if (isTimerRunning()) proc.songAudio.setBackingGain (gain); };
        audioLabel.onMenu = [this] { showBackingPanel(); };
        labels.addAndMakeVisible (audioLabel);
        labels.addAndMakeVisible (clickLabel);
        clickLabel.makeFile = [this] { return clickTrackFile (current()); };
        audioLabel.makeFile = [this] { return backingTrackFile (current()); };

        listContent.onKey = [this] (const juce::KeyPress& k) { return songKey (k); };

        metronomeToggle.setButtonText ("Metronome");
        metronomeToggle.setTooltip ("A click on every beat while the song plays (accented on each bar), through your output");
        metronomeToggle.onClick = [this] { songsNode().setProperty (IDs::metronome, metronomeToggle.getToggleState(), nullptr); };
        addChildComponent (metronomeToggle);
        countInBox.addItemList ({ "No count-in", "Count-in 1 bar", "Count-in 2 bars" }, 1);
        countInBox.setTooltip ("Clicks before the song starts, in its first bar's time signature and tempo");
        countInBox.onChange = [this] { songsNode().setProperty (IDs::countIn, countInBox.getSelectedId() - 1, nullptr); };
        addChildComponent (countInBox);

        gridBox.addItemList (songs::gridStepNames(), 1);
        gridBox.setTooltip ("Grid: where cues snap and the grid lines. Beat follows each section's time signature.");
        gridBox.onChange = [this] { songsNode().setProperty (IDs::gridStep, gridBox.getSelectedId() - 1, nullptr); };
        addChildComponent (gridBox);
        styleCaption (gridLabel, "GRID");
        addChildComponent (gridLabel);
        snapToggle.setButtonText ("Snap");
        snapToggle.setTooltip ("Snap to the grid (hold Alt for the opposite while you drag)");
        snapToggle.onClick = [this] { songsNode().setProperty (IDs::snap, snapToggle.getToggleState(), nullptr); };
        addChildComponent (snapToggle);
        for (auto* b : { &zoomOut, &zoomIn, &zoomFit })
            addChildComponent (b);
       #if JUCE_MAC
        const juce::String cmdKey ("Cmd"), altKey ("Option");
       #else
        const juce::String cmdKey ("Ctrl"), altKey ("Alt");
       #endif
        zoomOut.setTooltip ("Zoom out (or - , or " + altKey + " + mouse wheel)");
        zoomIn.setTooltip ("Zoom in (or + , or " + altKey + " + mouse wheel)");
        zoomFit.setTooltip ("Fit the whole song in the window");
        zoomOut.onClick = [this] { zoomBy (0.8, -1); };
        zoomIn.onClick = [this] { zoomBy (1.25, -1); };
        zoomFit.onClick = [this] { zoom = 1.0; layoutTracks(); };
        // Track height: in the corner above the track names.
        styleCaption (heightLabel, "TRACK HEIGHT");
        styleCaption (tempoLabel, "TEMPO");
        tempoLabel.setTooltip ("The tempo through the song, like Reaper's tempo markers: each section starts at its tempo, and "
                               "tempo changes can sit anywhere inside it, even partway through a bar, without moving the bars. "
                               "Double-click the lane to add one; drag one up or down for its tempo (Shift: finer) and left or right to move it, or "
                               "drag the line up or down; double-click to edit, Alt / Option-click to delete, "
                               "right-click for more. "
                               "A change (or a section's start) can glide gradually to the next tempo, for a ritardando or "
                               "accelerando.");
        for (auto* c : std::initializer_list<juce::Component*> { &shorter, &taller, &heightLabel, &tempoLabel })
            labels.addAndMakeVisible (c);
        shorter.setTooltip ("Shorter tracks (or " + cmdKey + " + - , " + cmdKey + " + mouse wheel, or drag a track name's bottom edge)");
        taller.setTooltip ("Taller tracks, with room for more cues stacked (or " + cmdKey + " + + , " + cmdKey
                           + " + mouse wheel, or drag a track name's bottom edge)");
        shorter.onClick = [this] { setTrackHeight (grid.trackHeight - 12); };
        taller.onClick = [this] { setTrackHeight (grid.trackHeight + 12); };

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
        dragSong.setTooltip ("Drag onto your DAW's timeline: the ticked tracks as one MIDI file (with the tempo map, time "
                             "signatures and a marker per section), plus the click track and the backing track as WAVs when "
                             "they're ticked, all starting at bar 1");
        dragSong.makeFiles = [this]
        {
            // The ticked tracks as MIDI, then the click and the backing track (when ticked) as WAVs, all from bar 1.
            const auto song = current();
            juce::StringArray files;
            if (const auto midi = songs::writeSongFile (song, songs::includedTracks (song)); midi.existsAsFile())
                files.add (midi.getFullPathName());
            if ((bool) song.getProperty (IDs::clickInclude, true))
                if (const auto click = clickTrackFile (song); click.existsAsFile())
                    files.add (click.getFullPathName());
            if ((bool) song.getProperty (IDs::audioInclude, true) && song[IDs::audioFile].toString().isNotEmpty())
                if (const auto backing = backingTrackFile (song); backing.existsAsFile())
                    files.add (backing.getFullPathName());
            return files;
        };
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

        // Above the cards they sit on (each card's translucent fill would dim them otherwise).
        for (auto* c : std::initializer_list<juce::Component*> { &metronomeToggle, &countInBox, &clickButton, &gridLabel, &gridBox, &snapToggle,
                                                                &zoomOut, &zoomIn, &zoomFit })
            c->toFront (false);

        state.addListener (this);
        rebuild();
    }

    ~SongBuilder() override
    {
        stopPlaying();
        thumbnail.removeChangeListener (this);
        state.removeListener (this);
    }

    void stopPlaying()
    {
        if (! isTimerRunning())
            return;
        stopTimer();
        proc.stopPreview();
        proc.songAudio.stop();
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
        saveSong.setBounds (second.removeFromRight (160));
        second.removeFromRight (8);
        countInBox.setBounds (second.removeFromRight (140));
        second.removeFromRight (6);
        clickButton.setBounds (second.removeFromRight (70));
        second.removeFromRight (6);
        metronomeToggle.setBounds (second.removeFromRight (120));
        songName.setBounds (sc.removeFromTop (30));
        summary.setBounds (sc.removeFromTop (20));
        r.removeFromTop (12);

        gridSection.setBounds (r);
        // The header row from the right: + Track, + Section, zoom, Snap, Grid (the title and hint keep the left).
        auto gh = gridSection.getBounds().removeFromTop (Section::headerHeight).reduced (Section::padding, 5);
        addTrack.setBounds (gh.removeFromRight (86));
        gh.removeFromRight (6);
        addSection.setBounds (gh.removeFromRight (94));
        gh.removeFromRight (14);
        zoomFit.setBounds (gh.removeFromRight (40));
        gh.removeFromRight (3);
        zoomIn.setBounds (gh.removeFromRight (28));
        gh.removeFromRight (3);
        zoomOut.setBounds (gh.removeFromRight (28));
        gh.removeFromRight (12);
        snapToggle.setBounds (gh.removeFromRight (78));
        gh.removeFromRight (4);
        gridBox.setBounds (gh.removeFromRight (96));
        gh.removeFromRight (2);
        gridLabel.setBounds (gh.removeFromRight (42));
        tracksView.setBounds (gridSection.contentArea());
        layoutTracks();
    }

private:
    juce::ValueTree current() { return songs::selectedSong (state); }

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override
    {
        applyLiveAudio();   // the metronome, the click and the backing track follow changes while playing
        triggerAsyncUpdate();
    }

    // - / + zoom; Cmd / Ctrl + - / + change the track height (= and the keypad's + count as +).
    bool keyPressed (const juce::KeyPress& key) override
    {
        const auto c = key.getTextCharacter();
        const auto code = key.getKeyCode();
        const auto plus = c == '+' || c == '=' || code == juce::KeyPress::numberPadAdd;
        const auto minus = c == '-' || c == '_' || code == juce::KeyPress::numberPadSubtract;
        if (! plus && ! minus)
            return false;
        if (key.getModifiers().isCommandDown())
            setTrackHeight (grid.trackHeight + (plus ? 12 : -12));
        else
            zoomBy (plus ? 1.25 : 0.8, -1);
        return true;
    }

    void applyLiveAudio (bool starting = false)
    {
        if (! playing.isValid() || ! (starting || isTimerRunning()))
            return;
        const auto settings = songs::songsNode (state);
        auto& audio = proc.songAudio;
        audio.setSound (clickSound());
        audio.setRoute ((int) settings.getProperty (IDs::clickRoute, 0));
        audio.setMetronome ((bool) settings.getProperty (IDs::metronome, true));
        audio.setBackingGain ((float) (double) playing.getProperty (IDs::audioGain, 0.8));
        audio.setBackingMuted ((bool) playing.getProperty (IDs::audioMuted, false));
    }
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
            setlistPlaying = false;
            stopPlaying();
            return;
        }
        playing = current();
        if (! playing.isValid())
            return;
        playFrom = juce::jlimit (0.0, songs::songLengthBeats (playing), grid.cursorBeat >= 0.0 ? grid.cursorBeat : 0.0);
        looping = songs::loopRange (playing, loopStart, loopEnd);
        if (looping && (playFrom < loopStart || playFrom >= loopEnd))
            playFrom = loopStart;

        // The count-in first (its clicks always sound), then the song's MIDI (the played tracks: ticked, not muted, or the
        // soloed ones), the metronome if it's on and the backing track (unless muted). A loop ends at its section's end.
        const auto settings = state.getChildWithName (IDs::Songs);
        const auto countIn = juce::jlimit (0, 2, (int) settings.getProperty (IDs::countIn, 1));
        lead = songs::countInSeconds (playing, playFrom, countIn);
        const auto until = looping ? songs::beatToSeconds (playing, loopEnd) - songs::beatToSeconds (playing, playFrom) : 1.0e9;

        auto events = songs::playbackEvents (playing, songs::playedTracks (playing), playFrom);
        events.erase (std::remove_if (events.begin(), events.end(), [until] (const auto& ev) { return ev.first >= until - 1.0e-6; }), events.end());
        for (auto& ev : events)
            ev.first += lead;
        proc.previewTimed (events);

        const auto audioPath = playing[IDs::audioFile].toString();
        if (audioPath.isNotEmpty() && proc.songAudio.backingFile().getFullPathName() != audioPath)
            proc.songAudio.loadBacking (juce::File (audioPath));
        else if (audioPath.isEmpty() && proc.songAudio.backingFile() != juce::File())
            proc.songAudio.clearBacking();
        proc.songAudio.start (proc.nowSample(), clickSound(), (int) settings.getProperty (IDs::clickRoute, 0),
                              (float) (double) playing.getProperty (IDs::audioGain, 0.8));
        applyLiveAudio (true);

        // Every click is scheduled; the metronome switch (live) decides which sound, the count-in's always do.
        std::vector<SongAudio::Click> clicks;
        for (const auto& c : songs::metronomeClicks (playing, playFrom, countIn))
            if (c.seconds < lead - 1.0e-6)
                clicks.push_back ({ c.seconds, c.accent, c.sub, true });
            else if (c.seconds - lead < until - 1.0e-6)
                clicks.push_back ({ c.seconds, c.accent, c.sub, false });
        proc.songAudio.addClicks (clicks);
        proc.songAudio.addBacking (lead, (double) playing.getProperty (IDs::audioOffset, 0.0) + songs::beatToSeconds (playing, playFrom));

        nextLoopAt = lead + until;   // seconds from start: when the loop's next pass begins
        playStarted = juce::Time::getMillisecondCounterHiRes();
        play.setButtonText (juce::String::fromUTF8 ("\u25A0  Stop"));
        startTimerHz (30);
    }

    // The loop's next pass, scheduled a moment before it's due (MIDI, clicks and the backing track restarted at its start).
    void scheduleLoopPass()
    {
        const auto length = songs::beatToSeconds (playing, loopEnd) - songs::beatToSeconds (playing, loopStart);
        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - playStarted) / 1000.0;
        auto events = songs::playbackEvents (playing, songs::playedTracks (playing), loopStart);
        events.erase (std::remove_if (events.begin(), events.end(), [length] (const auto& ev) { return ev.first >= length - 1.0e-6; }), events.end());
        for (auto& ev : events)
            ev.first += nextLoopAt - elapsed;
        proc.previewTimed (events);
        std::vector<SongAudio::Click> clicks;
        for (const auto& c : songs::metronomeClicks (playing, loopStart, 0))
            if (c.seconds < length - 1.0e-6)
                clicks.push_back ({ c.seconds, c.accent, c.sub });
        proc.songAudio.addClicks (clicks, nextLoopAt);
        proc.songAudio.addBacking (nextLoopAt, (double) playing.getProperty (IDs::audioOffset, 0.0) + songs::beatToSeconds (playing, loopStart));
        nextLoopAt += length;
    }

    void timerCallback() override
    {
        if (current() != playing)   // another song picked
        {
            setlistPlaying = false;
            stopPlaying();
            return;
        }
        const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - playStarted) / 1000.0;
        if (looping)
        {
            if (elapsed > nextLoopAt - 1.0)
                scheduleLoopPass();
            const auto length = songs::beatToSeconds (playing, loopEnd) - songs::beatToSeconds (playing, loopStart);
            const auto firstPass = songs::beatToSeconds (playing, loopEnd) - songs::beatToSeconds (playing, playFrom);
            auto t = elapsed - lead;
            double beat;
            if (t < firstPass)
                beat = songs::secondsToBeat (playing, songs::beatToSeconds (playing, playFrom) + juce::jmax (0.0, t));
            else
                beat = songs::secondsToBeat (playing, songs::beatToSeconds (playing, loopStart) + std::fmod (t - firstPass, juce::jmax (0.01, length)));
            grid.playBeat = beat;
            grid.repaint();
            return;
        }
        const auto t = elapsed - lead;   // negative: counting in
        const auto beat = songs::secondsToBeat (playing, songs::beatToSeconds (playing, playFrom) + juce::jmax (0.0, t));
        if (beat >= songs::songLengthBeats (playing))
        {
            stopPlaying();
            if (setlistPlaying)
                nextInSetlist();
            return;
        }
        grid.playBeat = beat;
        grid.repaint();
    }

    // A setlist: the songs in the list's order, from the one picked, with the setlist gap between them.
    void playSetlistFrom (juce::ValueTree song)
    {
        stopPlaying();
        setlistPlaying = true;
        grid.cursorBeat = 0.0;
        if (song != current())
            state.setProperty (IDs::selectedSong, song[IDs::uid], nullptr);
        juce::Component::SafePointer<SongBuilder> safe (this);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) { safe->rebuild(); safe->togglePlay(); } });
    }

    void nextInSetlist()
    {
        const auto list = state.getChildWithName (IDs::Songs);
        const auto next = list.getChild (list.indexOf (current()) + 1);
        if (! next.isValid())
        {
            setlistPlaying = false;
            return;
        }
        const auto gap = (double) list.getProperty (IDs::setlistGap, 4.0);
        juce::Component::SafePointer<SongBuilder> safe (this);
        juce::Timer::callAfterDelay ((int) (gap * 1000.0), [safe, next]
        {
            if (safe != nullptr && safe->setlistPlaying && ! safe->isTimerRunning())
                safe->playSetlistFrom (next);
        });
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
                t->badge = juce::String (songs::songBars (song)) + (songs::songBars (song) == 1 ? " bar" : " bars");
                t->colour = paletteColour (index++);
                t->highlighted = song == selected || isPicked (song);
                t->setTooltip ("Click to open. Cmd / Ctrl-click or Shift-click to select several, then copy, paste, "
                               "duplicate or delete them (keys or right-click).");
                t->onClick = [this, song] { songClicked (song); };
                t->onContextMenu = [this, song] { if (! isPicked (song)) picked = { song[IDs::uid].toString() }; songMenu (song); };
                listContent.addAndMakeVisible (t);
            }

        const auto has = selected.isValid();
        for (auto* c : std::initializer_list<juce::Component*> { &songSection, &songName, &summary, &dragSong, &exportSong, &saveSong, &play, &saved,
                                                                &metronomeToggle, &countInBox, &clickButton, &gridBox, &gridLabel, &snapToggle, &zoomOut, &zoomIn, &zoomFit,
                                                                &gridSection, &addSection, &addTrack, &tracksView })
            c->setVisible (has);
        empty.setVisible (! has);

        // The shared settings: metronome, count-in, grid and snap.
        const auto settings = state.getChildWithName (IDs::Songs);
        metronomeToggle.setToggleState ((bool) settings.getProperty (IDs::metronome, true), juce::dontSendNotification);
        countInBox.setSelectedId ((int) settings.getProperty (IDs::countIn, 1) + 1, juce::dontSendNotification);
        gridBox.setSelectedId ((int) settings.getProperty (IDs::gridStep, 0) + 1, juce::dontSendNotification);
        snapToggle.setToggleState ((bool) settings.getProperty (IDs::snap, true), juce::dontSendNotification);
        grid.trackHeight = juce::jlimit (minTrackHeight, maxTrackHeight, (int) settings.getProperty (IDs::trackHeight, defaultTrackHeight));
        grid.gridStep = juce::jlimit (0, songs::gridStepNames().size() - 1, (int) settings.getProperty (IDs::gridStep, 0));
        grid.snapOn = (bool) settings.getProperty (IDs::snap, true);

        // The backing track's waveform (the file itself is opened for playback when Play is pressed).
        const auto audioPath = selected[IDs::audioFile].toString();
        if (audioPath != thumbnailFile)
        {
            thumbnailFile = audioPath;
            const juce::File f (audioPath);
            if (audioPath.isNotEmpty() && f.existsAsFile())
                thumbnail.setSource (new juce::FileInputSource (f));
            else
                thumbnail.clear();
        }
        audioLabel.song = selected;
        audioLabel.refresh();
        clickLabel.song = selected;
        clickLabel.settings = songs::songsNode (state);
        clickLabel.repaint();

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
                    tempoChanges |= std::abs ((double) c.getProperty (IDs::bpm, 120.0) - (double) first.getProperty (IDs::bpm, 120.0)) > 1.0e-6
                                    || ! songs::tempoChanges (c).empty() || songs::rampsTempo (selected, c);
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
                l->onResize = [this] (int height) { setTrackHeight (height); };
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
        labels.setBounds (0, 0, 216, height);   // names, S / M and the tick
        for (int i = 0; i < trackLabels.size(); ++i)
            trackLabels[i]->setBounds (0, grid.trackTop (i), 208, grid.trackHeight);
        audioLabel.setBounds (0, grid.audioTop(), 208, audioHeight);
        clickLabel.setBounds (0, clickLaneTop, 208, clickLaneHeight);
        tempoLabel.setBounds (8, tempoLaneTop, 200, tempoLaneHeight);
        {
            auto corner = juce::Rectangle<int> (0, rulerHeight - 12, 208, 30);
            taller.setBounds (corner.removeFromRight (28));
            corner.removeFromRight (3);
            shorter.setBounds (corner.removeFromRight (28));
            heightLabel.setBounds (corner.withTrimmedLeft (8));
        }
        gridView.setBounds (labels.getRight(), 0, width - labels.getRight(), height);

        // Zoom 1 = the whole song fits (at least 6 px a beat); zoom in from there.
        const auto beats = juce::jmax (1.0, songs::songLengthBeats (grid.song));
        const auto fit = juce::jmax (6.0, (gridView.getWidth() - 4) / beats);
        grid.pixelsPerBeat = juce::jlimit (2.0, 400.0, fit * zoom);
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
        const auto n = (int) pickedSongs().size();
        const auto many = n > 1 ? " " + juce::String (n) + " songs" : juce::String();
        juce::PopupMenu m;
        m.addSectionHeader (n > 1 ? juce::String (n) + " songs selected" : song[IDs::name].toString());
        m.addItem (1, "Rename...", n == 1);
        m.addItem (2, "Duplicate" + many);
        m.addItem (5, "Copy" + many);
        m.addItem (6, songClipboard.size() > 1 ? "Paste " + juce::String ((int) songClipboard.size()) + " songs" : juce::String ("Paste"),
                   ! songClipboard.empty());
        m.addItem (4, "Save as file...", n == 1);
        m.addSeparator();
        m.addItem (7, "Play the setlist from here");
        // The pause between songs when the setlist plays.
        juce::PopupMenu gaps;
        const auto gapNow = (double) state.getChildWithName (IDs::Songs).getProperty (IDs::setlistGap, 4.0);
        for (int g : { 0, 2, 4, 8, 15, 30 })
            gaps.addItem (200 + g, g == 0 ? juce::String ("None: straight into the next song") : juce::String (g) + " seconds",
                          true, std::abs (gapNow - g) < 0.5);
        m.addSubMenu ("Gap between songs", gaps);
        m.addItem (8, "Export the setlist as song packages...");
        m.addSeparator();
        m.addItem (3, "Delete" + many + "...");
        juce::Component::SafePointer<SongBuilder> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, song] (int r) mutable
        {
            if (safe == nullptr)
                return;
            if (r == 1)
                renameNode (song, "Rename song");
            else if (r == 4)
                safe->chooseSaveSong (song);
            else if (r == 2)   // duplicate the picked songs (without touching the clipboard)
            {
                std::vector<juce::ValueTree> copies;
                for (const auto& sTree : safe->pickedSongs())
                    copies.push_back (sTree.createCopy());
                safe->pasteSongs (copies);
            }
            else if (r == 5)
                safe->copySongs();
            else if (r == 6)
                safe->pasteSongs (songClipboard);
            else if (r == 3)
                safe->deleteSongs (true);
            else if (r == 7)
                safe->playSetlistFrom (song);
            else if (r >= 200)
                safe->songsNode().setProperty (IDs::setlistGap, r - 200, nullptr);
            else if (r == 8)
            {
                std::vector<juce::ValueTree> all;
                for (const auto& sTree : safe->state.getChildWithName (IDs::Songs))
                    all.push_back (sTree);
                safe->exportCountIn = juce::jlimit (0, 2, (int) safe->state.getChildWithName (IDs::Songs).getProperty (IDs::countIn, 1));
                safe->choosePackageFolder (all);
            }
        });
    }

    juce::ValueTree songsNode() { return songs::songsNode (state); }

    // The click's settings, with the custom samples loaded (kept until their files change).
    songs::ClickSound clickSound()
    {
        const auto settings = songs::songsNode (state);
        auto c = songs::clickSettings (settings);
        if (c.sound != 4)
            return c;
        const auto key = settings[IDs::clickFileAccent].toString() + "|" + settings[IDs::clickFileBeat].toString();
        if (key != clickSamplesKey)
        {
            clickSamplesKey = key;
            accentSample = beatSample = nullptr;
            samplesRate = 0.0;
            for (auto [id, target] : { std::pair<juce::Identifier, std::shared_ptr<juce::AudioBuffer<float>>*> { IDs::clickFileAccent, &accentSample },
                                                                                                              { IDs::clickFileBeat, &beatSample } })
            {
                std::unique_ptr<juce::AudioFormatReader> reader (proc.songAudio.formats().createReaderFor (juce::File (settings[id].toString())));
                if (reader == nullptr)
                    continue;
                const auto n = (int) juce::jmin<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 2.0));
                auto buffer = std::make_shared<juce::AudioBuffer<float>> (1, juce::jmax (1, n));
                reader->read (buffer.get(), 0, n, 0, true, false);
                if (samplesRate > 0.0 && std::abs (reader->sampleRate - samplesRate) > 1.0)   // match the first one's rate
                {
                    auto resampled = std::make_shared<juce::AudioBuffer<float>> (1, juce::jmax (1, (int) (n * samplesRate / reader->sampleRate)));
                    juce::LagrangeInterpolator interp;
                    interp.process (reader->sampleRate / samplesRate, buffer->getReadPointer (0), resampled->getWritePointer (0), resampled->getNumSamples());
                    buffer = resampled;
                }
                else if (samplesRate <= 0.0)
                    samplesRate = reader->sampleRate;
                *target = buffer;
            }
        }
        c.accentSample = accentSample;
        c.beatSample = beatSample;
        c.sampleRate = samplesRate > 0.0 ? samplesRate : 48000.0;
        return c;
    }

    // A WAV in a temp folder named after its content, for dragging into the DAW (like the tiles' MIDI files).
    static juce::File tempWav (const juce::AudioBuffer<float>& audio, double rate, const juce::String& key, const juce::String& name)
    {
        const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PedalCues")
                             .getChildFile (juce::String::toHexString (key.hashCode64()));
        dir.createDirectory();
        const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".wav");
        if (! file.existsAsFile() && writeWav (audio, rate, file).isNotEmpty())
            return {};
        return file;
    }

    // The whole song's click from bar 1 (no count-in: it lines up with the MIDI dropped at the same spot).
    juce::File clickTrackFile (const juce::ValueTree& song)
    {
        constexpr double rate = 48000.0;
        const auto settings = songs::songsNode (state);
        const auto key = song.toXmlString() + settings.toXmlString().fromFirstOccurrenceOf ("<Songs", true, false).upToFirstOccurrenceOf (">", true, false);
        const auto click = songs::renderClickTrack (song, 0, rate, clickSound(), 1.0);
        return tempWav (click, rate, "click" + key, song[IDs::name].toString() + " - click");
    }

    // The backing track lined up with bar 1, the song's length.
    juce::File backingTrackFile (const juce::ValueTree& song)
    {
        const auto path = song[IDs::audioFile].toString();
        if (path.isEmpty() || ! juce::File (path).existsAsFile())
            return {};
        constexpr double rate = 48000.0;
        const auto seconds = songs::songSeconds (song) + 1.0;
        const auto key = "backing" + path + song[IDs::audioOffset].toString() + song[IDs::audioGain].toString() + juce::String (seconds);
        const auto audio = proc.songAudio.renderBacking (juce::File (path), (double) song.getProperty (IDs::audioOffset, 0.0), 0.0, seconds, rate,
                                                         (float) (double) song.getProperty (IDs::audioGain, 0.8));
        return tempWav (audio, rate, key, song[IDs::name].toString() + " - backing");
    }

    void openTapTempo (juce::ValueTree section)
    {
        juce::DialogWindow::LaunchOptions o;
        o.dialogTitle = "Tap tempo";
        o.dialogBackgroundColour = background;
        o.content.setOwned (new TapPanel (current(), section));
        o.escapeKeyTriggersCloseButton = true;
        o.useNativeTitleBar = true;
        o.resizable = false;
        o.componentToCentreAround = this;
        o.launchAsync();
    }

    // Zoom by a factor, keeping the beat under anchorX (in the grid) in place; -1: the middle of the view.
    // Every track's height (saved with the songs), keeping the track under the view's top where it was.
    void setTrackHeight (int height)
    {
        height = juce::jlimit (minTrackHeight, maxTrackHeight, height);
        if (height == grid.trackHeight)
            return;
        songsNode().setProperty (IDs::trackHeight, height, nullptr);
        grid.trackHeight = height;
        layoutTracks();
    }

    void zoomBy (double factor, int anchorX)
    {
        const auto viewX = anchorX >= 0 ? anchorX - gridView.getViewPositionX() : gridView.getWidth() / 2;
        const auto beat = grid.beatAt (gridView.getViewPositionX() + viewX);
        zoom = juce::jlimit (1.0, 64.0, zoom * factor);
        layoutTracks();
        gridView.setViewPosition (juce::jmax (0, grid.xOf (beat) - viewX), gridView.getViewPositionY());
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override { grid.repaint(); }   // the waveform as it loads

    void chooseAudio()
    {
        chooser = std::make_unique<juce::FileChooser> ("Pick a backing track", juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                       proc.songAudio.formats().getWildcardForAllFormats());
        juce::Component::SafePointer<SongBuilder> safe (this);
        auto song = current();
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe, song] (const juce::FileChooser& fc) mutable
        {
            const auto file = fc.getResult();
            if (safe == nullptr || file == juce::File())
                return;
            std::unique_ptr<juce::AudioFormatReader> reader (safe->proc.songAudio.formats().createReaderFor (file));
            if (reader == nullptr)
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Backing track",
                                                        file.getFileName() + " can't be read. Use an MP3, WAV, AIFF, FLAC or Ogg file.");
                return;
            }
            const auto audioSeconds = reader->sampleRate > 0.0 ? (double) reader->lengthInSamples / reader->sampleRate : 0.0;
            song.setProperty (IDs::audioFile, file.getFullPathName(), nullptr);
            if (! song.hasProperty (IDs::audioGain))
                song.setProperty (IDs::audioGain, 0.8, nullptr);
            safe->offerToCoverAudio (song, audioSeconds);
        });
    }

    double backingSeconds (const juce::ValueTree& song)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (proc.songAudio.formats().createReaderFor (juce::File (song[IDs::audioFile].toString())));
        return reader != nullptr && reader->sampleRate > 0.0 ? (double) reader->lengthInSamples / reader->sampleRate : 0.0;
    }

    void showBackingPanel()
    {
        auto song = current();
        if (song[IDs::audioFile].toString().isEmpty())
        {
            chooseAudio();
            return;
        }
        auto panel = std::make_unique<BackingPanel> (song, backingSeconds (song));
        juce::Component::SafePointer<SongBuilder> safe (this);
        panel->onReplace = [safe] { if (safe != nullptr) safe->chooseAudio(); };
        panel->onLiveGain = [safe] (float gain) { if (safe != nullptr && safe->isTimerRunning()) safe->proc.songAudio.setBackingGain (gain); };
        juce::CallOutBox::launchAsynchronously (std::move (panel), audioLabel.getScreenBounds(), nullptr);
    }

    // A new backing track longer than the song: offer the bars it needs, so it isn't cut off at the song's end.
    void offerToCoverAudio (juce::ValueTree song, double audioSeconds)
    {
        const auto bars = songs::barsToCoverAudio (song, audioSeconds);
        if (bars <= 0)
            return;
        auto* w = new juce::AlertWindow ("Backing track", "The recording runs about " + juce::String (bars) + (bars == 1 ? " bar" : " bars")
                                         + " past the song's end, so the end would be cut off. Add "
                                         + (bars == 1 ? juce::String ("it") : juce::String ("them")) + " to the last section?",
                                         juce::MessageBoxIconType::NoIcon);
        w->addButton ("Add " + juce::String (bars) + (bars == 1 ? " bar" : " bars"), 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Keep the song as it is", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([song, bars] (int r) mutable
        {
            if (r == 1)
                songs::extendLastSection (song, bars);
        }), true);
    }

    // x: where in the grid it was right-clicked (-1: from the label), for "bar 1 is here" and fitting the tempo.
    void audioMenu (int x)
    {
        auto song = current();
        const auto has = song[IDs::audioFile].toString().isNotEmpty();
        const auto offset = (double) song.getProperty (IDs::audioOffset, 0.0);
        const auto beat = x >= 0 ? grid.beatAt (x) : -1.0;
        const auto fileSeconds = beat >= 0.0 ? songs::beatToSeconds (song, beat) + offset : -1.0;   // the audio under the mouse
        const auto nearestBar = beat >= 0.0 ? juce::jmax (2, songs::barNumberAt (song, beat + songs::barBeats (songs::sectionAt (song, beat)) / 2.0)) : 2;
        juce::PopupMenu m;
        m.addSectionHeader (has ? juce::File (song[IDs::audioFile].toString()).getFileName() : juce::String ("Backing track"));
        if (has && beat >= 0.0)
        {
            m.addItem (10, "Bar 1 starts here in the audio");
            m.addItem (11, "Bar " + juce::String (nearestBar) + " starts here (fit the tempo)...");
            m.addSeparator();
        }
        if (has)
            m.addItem (5, "Backing track settings...");
        else
            m.addItem (1, "Add audio...");
        juce::Component::SafePointer<SongBuilder> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, song, fileSeconds, nearestBar] (int r) mutable
        {
            if (safe == nullptr)
                return;
            if (r == 10)
                song.setProperty (IDs::audioOffset, std::round (fileSeconds * 1000.0) / 1000.0, nullptr);
            else if (r == 11)
                askText ("Which bar starts at this point in the audio? Every section's tempo is scaled so it lands there.",
                         juce::String (nearestBar), [song, fileSeconds] (const juce::String& t) mutable
                {
                    if (t.getIntValue() > 1 && ! songs::fitTempoToAudio (song, t.getIntValue(), fileSeconds))
                        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Fit the tempo",
                                                                "That point is before bar 1 in the audio: set where bar 1 is first.");
                });
            else if (r == 1)
                safe->chooseAudio();
            else if (r == 5)
                safe->showBackingPanel();
        });
    }

    // The songs list's selection: `picked` (uids); the open song is the last one clicked.
    bool isPicked (const juce::ValueTree& song) const { return picked.contains (song[IDs::uid].toString()); }

    std::vector<juce::ValueTree> pickedSongs()
    {
        std::vector<juce::ValueTree> list;
        for (auto song : state.getChildWithName (IDs::Songs))
            if (isPicked (song))
                list.push_back (song);
        if (list.empty() && current().isValid())
            list.push_back (current());
        return list;
    }

    void songClicked (juce::ValueTree song)
    {
        const auto mods = juce::ModifierKeys::currentModifiers;
        const auto uid = song[IDs::uid].toString();
        auto list = state.getChildWithName (IDs::Songs);
        if (mods.isCommandDown() || mods.isCtrlDown())
        {
            if (picked.isEmpty() && current().isValid())
                picked.add (current()[IDs::uid].toString());
            if (picked.contains (uid) && picked.size() > 1)
            {
                // Deselect it, without opening it: if it was the open song, the last song still selected opens instead.
                picked.removeString (uid);
                listContent.grabKeyboardFocus();
                if (song == current())
                    state.setProperty (IDs::selectedSong, picked[picked.size() - 1], nullptr);
                else
                    rebuild();
                return;
            }
            picked.addIfNotAlreadyThere (uid);
        }
        else if (mods.isShiftDown() && current().isValid())
        {
            const auto a = list.indexOf (current()), b = list.indexOf (song);
            picked.clear();
            for (int i = juce::jmin (a, b); i <= juce::jmax (a, b); ++i)
                picked.add (list.getChild (i)[IDs::uid].toString());
            listContent.grabKeyboardFocus();
            rebuild();
            return;   // the open song stays
        }
        else
            picked = { uid };
        listContent.grabKeyboardFocus();
        if (song != current())
            state.setProperty (IDs::selectedSong, uid, nullptr);
        else
            rebuild();
    }

    bool songKey (const juce::KeyPress& k)
    {
        const auto cmd = juce::ModifierKeys::commandModifier;
        if (k == juce::KeyPress ('c', cmd, 0)) { copySongs(); return true; }
        if (k == juce::KeyPress ('x', cmd, 0)) { copySongs(); deleteSongs (false); return true; }
        if (k == juce::KeyPress ('v', cmd, 0)) { pasteSongs (songClipboard); return true; }
        if (k == juce::KeyPress ('d', cmd, 0))
        {
            std::vector<juce::ValueTree> copies;
            for (const auto& song : pickedSongs())
                copies.push_back (song.createCopy());
            pasteSongs (copies);
            return true;
        }
        if (k == juce::KeyPress ('a', cmd, 0))
        {
            picked.clear();
            for (const auto& song : state.getChildWithName (IDs::Songs))
                picked.add (song[IDs::uid].toString());
            rebuild();
            return true;
        }
        if (k == juce::KeyPress::deleteKey || k == juce::KeyPress::backspaceKey) { deleteSongs (true); return true; }
        if (k == juce::KeyPress::escapeKey) { picked.clear(); rebuild(); return true; }
        return false;
    }

    void copySongs()
    {
        songClipboard.clear();
        for (const auto& song : pickedSongs())
            songClipboard.push_back (song.createCopy());
    }

    // After the last picked song, as new copies ("Name (copy)"), which become the selection.
    void pasteSongs (const std::vector<juce::ValueTree>& from)
    {
        if (from.empty())
            return;
        auto list = songs::songsNode (state);
        int at = list.getNumChildren();
        for (const auto& song : pickedSongs())
            at = juce::jmax (at == list.getNumChildren() ? -1 : at, list.indexOf (song) + 1);
        if (at < 0)
            at = list.getNumChildren();
        picked.clear();
        juce::String last;
        for (const auto& song : from)
        {
            auto copy = songs::copySong (song, song[IDs::name].toString() + " (copy)");
            list.addChild (copy, at++, nullptr);
            picked.add (last = copy[IDs::uid].toString());
        }
        state.setProperty (IDs::selectedSong, last, nullptr);
    }

    void deleteSongs (bool ask)
    {
        auto doomed = pickedSongs();
        if (doomed.empty())
            return;
        auto run = [this, doomed]
        {
            auto list = state.getChildWithName (IDs::Songs);
            for (auto song : doomed)
                list.removeChild (song, nullptr);
            picked.clear();
        };
        if (! ask)
        {
            run();
            return;
        }
        const auto what = doomed.size() == 1 ? "\"" + doomed.front()[IDs::name].toString() + "\""
                                             : juce::String ((int) doomed.size()) + " songs";
        auto* w = new juce::AlertWindow ("Delete song" + juce::String (doomed.size() == 1 ? "" : "s"),
                                         "Delete " + what + " and their cues? Clips already in your DAW aren't affected.",
                                         juce::MessageBoxIconType::NoIcon);
        w->addButton ("Delete", 1);
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<SongBuilder> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, run] (int res)
        {
            if (safe != nullptr && res == 1)
                run();
        }), true);
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
        // The export's count-in follows the song card's, or 1 bar when that's off.
        const auto bars = juce::jmax (1, juce::jlimit (0, 2, (int) state.getChildWithName (IDs::Songs).getProperty (IDs::countIn, 1)));
        o.content.setOwned (new ExportPanel (song, bars, [safe, bars] (int mode, bool withCountIn)
        {
            if (safe == nullptr)
                return;
            safe->exportCountIn = withCountIn ? bars : 0;
            if (mode == 2)
                safe->choosePackageFolder ({ safe->current() });
            else if (mode == 1)
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

    // A song package for a backing-track player, in a folder named after the song: the cues (MIDI), the click (WAV) and the
    // backing track lined up with bar 1 (WAV), all starting together (with the count-in when asked). Returns a problem or "".
    juce::String writePackage (const juce::ValueTree& song, const juce::File& folder)
    {
        const auto dir = folder.getChildFile (juce::File::createLegalFileName (song[IDs::name].toString()));
        if (! dir.createDirectory())
            return "Couldn't make " + dir.getFullPathName();
        const auto name = juce::File::createLegalFileName (song[IDs::name].toString());
        const auto counted = songs::withCountIn (song, exportCountIn);
        {
            const auto midi = songs::songMidi (counted, songs::includedTracks (counted));
            juce::MemoryOutputStream data;
            const auto file = dir.getChildFile (name + " - cues.mid");
            if (! midi.writeTo (data, 1) || ! file.replaceWithData (data.getData(), data.getDataSize()))
                return "Couldn't write " + file.getFullPathName();
        }
        constexpr double rate = 48000.0;
        const auto settings = state.getChildWithName (IDs::Songs);
        const auto click = songs::renderClickTrack (song, exportCountIn, rate, clickSound());
        if (const auto problem = writeWav (click, rate, dir.getChildFile (name + " - click.wav")); problem.isNotEmpty())
            return problem;
        const auto audioPath = song[IDs::audioFile].toString();
        if (audioPath.isNotEmpty() && juce::File (audioPath).existsAsFile())
        {
            const auto backing = proc.songAudio.renderBacking (juce::File (audioPath), (double) song.getProperty (IDs::audioOffset, 0.0),
                                                               songs::countInSeconds (song, 0.0, exportCountIn),
                                                               click.getNumSamples() / rate, rate,
                                                               (float) (double) song.getProperty (IDs::audioGain, 0.8));
            if (const auto problem = writeWav (backing, rate, dir.getChildFile (name + " - backing.wav")); problem.isNotEmpty())
                return problem;
        }
        return {};
    }

    static juce::String writeWav (const juce::AudioBuffer<float>& audio, double rate, const juce::File& file)
    {
        file.deleteFile();
        auto stream = file.createOutputStream();
        if (stream == nullptr)
            return "Couldn't write " + file.getFullPathName();
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (wav.createWriterFor (stream.get(), rate, (unsigned int) audio.getNumChannels(), 24, {}, 0));
        if (writer == nullptr)
            return "Couldn't write " + file.getFullPathName();
        stream.release();   // the writer owns it now
        writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
        return {};
    }

    // One song, or the whole setlist (numbered "01 Name", "02 Name"... in the list's order): a package each.
    void choosePackageFolder (std::vector<juce::ValueTree> list)
    {
        if (list.empty())
            return;
        chooser = std::make_unique<juce::FileChooser> (list.size() == 1 ? "Choose a folder for the song package" : "Choose a folder for the setlist",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory));
        juce::Component::SafePointer<SongBuilder> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [safe, list] (const juce::FileChooser& fc)
        {
            const auto folder = fc.getResult();
            if (safe == nullptr || folder == juce::File() || ! folder.isDirectory())
                return;
            juce::StringArray problems;
            int n = 1;
            for (const auto& song : list)
            {
                auto target = folder;
                if (list.size() > 1)
                {
                    target = folder.getChildFile (juce::String (n++).paddedLeft ('0', 2) + " " + juce::File::createLegalFileName (song[IDs::name].toString()));
                    target.createDirectory();
                }
                if (const auto problem = safe->writePackage (song, target); problem.isNotEmpty())
                    problems.add (problem);
            }
            if (! problems.isEmpty())
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Export", problems.joinIntoString ("\n"));
            else
                folder.revealToUser();
        });
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
                const auto midi = songs::songMidi (songs::withCountIn (song, safe->exportCountIn), { track[IDs::uid].toString() });
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
            const auto midi = songs::songMidi (songs::withCountIn (song, safe->exportCountIn), songs::includedTracks (song));
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
    SongList listContent;
    juce::OwnedArray<Tile> songRows;

    Section songSection { "songs.song", "Song", "drag it into your DAW, or export it", builderViolet };
    juce::Label songName, summary;
    DragSongButton dragSong;
    juce::TextButton exportSong, saveSong, play, clickButton;
    bool looping = false, setlistPlaying = false;
    double loopStart = 0.0, loopEnd = 0.0, nextLoopAt = 0.0;
    juce::ToggleButton metronomeToggle, snapToggle;
    juce::ComboBox countInBox, gridBox;
    juce::TextButton zoomOut { "-" }, zoomIn { "+" }, zoomFit { "Fit" }, shorter { "-" }, taller { "+" };
    juce::Label heightLabel, tempoLabel;
    double zoom = 1.0, lead = 0.0;   // lead: the count-in, in seconds
    juce::AudioThumbnailCache thumbnailCache { 4 };
    juce::AudioThumbnail thumbnail { 512, proc.songAudio.formats(), thumbnailCache };
    juce::String thumbnailFile;
    AudioLabel audioLabel;
    ClickLabel clickLabel;
    juce::String clickSamplesKey;   // the custom click samples loaded (paths)
    std::shared_ptr<juce::AudioBuffer<float>> accentSample, beatSample;
    double samplesRate = 48000.0;
    juce::StringArray picked;   // songs selected in the list (uids)
    static inline std::vector<juce::ValueTree> songClipboard;
    juce::ValueTree playing;
    double playFrom = 0.0, playStarted = 0.0;
    int exportCountIn = 0;   // bars of count-in the next export starts with

    Section gridSection { "songs.grid", "Arrangement", "drag tiles in from the tabs", builderViolet };
    juce::Label gridLabel;
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

std::unique_ptr<juce::Component> makeSongBackingPanel (juce::ValueTree song, double audioSeconds)
{
    return std::make_unique<BackingPanel> (song, audioSeconds);
}

std::unique_ptr<juce::Component> makeSongExportPanel (juce::ValueTree song)
{
    return std::make_unique<ExportPanel> (song, 1, nullptr);
}

std::unique_ptr<juce::Component> makeSongBuilder (PedalCuesProcessor& p, std::function<juce::String()> deviceName)
{
    return std::make_unique<SongBuilder> (p, std::move (deviceName));
}
} // namespace ui
