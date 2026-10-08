#include "MovesPanel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

using namespace theme;

namespace ui
{
namespace
{
constexpr std::array<double, 12> lengths { 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 24.0, 32.0 };

// The pad's grid (snap) choices, in beats; 0 = Off.
struct GridChoice { const char* name; double beats; };
const std::array<GridChoice, 8> grids { { { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 },
                                          { "1/4 T", 2.0 / 3.0 }, { "1/8 T", 1.0 / 3.0 }, { "1/16 T", 1.0 / 6.0 }, { "Off", 0.0 } } };

void fillGridBox (juce::ComboBox& box)
{
    box.clear (juce::dontSendNotification);
    for (size_t i = 0; i < grids.size(); ++i)
        box.addItem (grids[i].name, (int) i + 1);
    box.setTooltip ("The grid points snap to (hold Shift while dragging to place them off the grid). Off: no time snapping; "
                    "values still snap to 1 %.");
}

void selectGrid (juce::ComboBox& box, double beats)
{
    for (size_t i = 0; i < grids.size(); ++i)
        if (std::abs (grids[i].beats - beats) < 1.0e-9)
            box.setSelectedId ((int) i + 1, juce::dontSendNotification);
}

#if JUCE_MAC
const juce::String cmdKey ("Cmd"), altKey ("Option");
#else
const juce::String cmdKey ("Ctrl"), altKey ("Alt");
#endif

juce::String padTooltip()
{
    return "Click: add a point. Drag a point or a line to move it (Shift: off the grid, " + cmdKey + "+Shift: one direction only). "
         + altKey + "-drag a line: curve it, " + altKey + "-click a point: delete it. Double-click a point: type its value. "
         + cmdKey + "-click: select more, right-drag: select an area, " + cmdKey + "+A: all. " + cmdKey + "-drag: draw freehand. "
         "Right-click: line shapes, invert, scale. " + cmdKey + "+Z: undo. Bottom = heel, top = toe.";
}

juce::String padHelp()
{
    return "Click: add a point.   Drag a point or a line: move it (Shift: off the grid, " + cmdKey + "+Shift: one direction only).   "
         + cmdKey + "-drag: draw freehand.\n"
         + altKey + "-drag a line: curve it (" + altKey + "-double-click: straight again).   " + altKey + "-click a point: delete it.   "
         "Double-click a point: type its value.\n"
         + cmdKey + "-click: add to the selection.   Right-drag: select an area.   " + cmdKey + "+A: select all.   Arrows: nudge.   "
         "Delete: remove.   Selection box: drag its top or bottom edge to scale, a top corner to tilt.\n"
         "Right-click: line shape (square, linear, slow start/end, fast start, fast end, bezier), invert, scale.   "
         + cmdKey + "+Z / " + cmdKey + "+Shift+Z: undo / redo.";
}

void curveOf (Tile& t, const cues::Cue& cue)
{
    t.curve.clear();
    for (const auto& [beat, msg] : cue.events)
        if (msg.isController())
            t.curve.push_back ({ (float) (beat / cue.lengthBeats), (float) msg.getControllerValue() / 127.0f });
}

void styleButton (juce::Button& b)
{
    b.setColour (juce::TextButton::buttonColourId, surface);
    b.setColour (juce::TextButton::textColourOffId, text);
}

} // namespace

//==============================================================================
// What the pads edit: the drawing's samples, the breakpoints (with line shapes and the selection) they're rendered
// from, the grid and the undo history. One per panel, shared by its pad and the larger editor's.
struct MovesPanel::DrawDoc
{
    std::vector<float> points = std::vector<float> ((size_t) cues::whammy::drawPoints, 0.0f);
    std::vector<cues::whammy::Breakpoint> bps;
    double beats = 4.0;
    double grid = 0.25;   // 1/16
    cues::whammy::DrawHistory history { 100 };

    // After 'points' changed from outside (a saved drawing, Wave, Import MIDI, Clear, Smooth, the project): new breakpoints
    // (straight lines), and an undo step, unless the samples are still what the breakpoints render (a pad's own edit
    // coming back from the state). 'mergeKey': see DrawHistory.
    void sync (const juce::String& mergeKey = {})
    {
        if (bps.size() < 2
            || cues::whammy::encodeDrawing (cues::whammy::renderBreakpoints (bps)) != cues::whammy::encodeDrawing (points))
        {
            bps = cues::whammy::simplifyDrawing (points);
            history.record (bps, mergeKey);
        }
        else if (history.size() == 0)
        {
            history.record (bps);
        }
    }
};

//==============================================================================
// The drawing canvas, heel (bottom) to toe (top) across the move's length: breakpoints joined by lines on a grid, edited
// like a DAW's envelope / CC lane (see padHelp). Every edit renders them into the drawing's samples.
class MovesPanel::DrawPad final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    DrawPad (const MovesConfig& c, DrawDoc& d) : points (d.points), config (c), doc (d)
    {
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        setWantsKeyboardFocus (true);
        setTooltip (padTooltip());
    }

    std::vector<float>& points;   // the drawing: cues::whammy::drawPoints samples, 0 = heel, 1 = toe (the doc's)
    std::function<void()> onChange, onCommit;   // while dragging / when an edit is done

    void syncBreakpoints (const juce::String& mergeKey = {})
    {
        doc.sync (mergeKey);
        if (hovered >= (int) doc.bps.size()) hovered = -1;
        if (hoveredSegment >= (int) doc.bps.size() - 1) hoveredSegment = -1;
    }

    void undo()
    {
        if (! doc.history.canUndo())
            return;
        doc.bps = doc.history.undo();
        restored();
    }

    void redo()
    {
        if (! doc.history.canRedo())
            return;
        doc.bps = doc.history.redo();
        restored();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (background);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (hasKeyboardFocus (false) ? outline.brighter (0.25f) : outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

        const auto plot = plotArea();
        paintGrid (g, plot);

        // TOE / HALF / HEEL with their MIDI values (127 / 64 / 0); the heel's value sits above it, inside the pad.
        for (auto [name, value, y, valueAbove] : { std::tuple<const char*, const char*, float, bool> { "TOE", "(127)", plot.getY(), false },
                                                   { "HALF", "(64)", plot.getCentreY(), false },
                                                   { "HEEL", "(0)", plot.getBottom(), true } })
        {
            const auto nameBox = juce::Rectangle<float> (b.getX() + 6.0f, y - 6.0f, 36.0f, 12.0f);
            g.setColour (dim.withAlpha (0.8f));
            g.setFont (font (10.0f, true));
            g.drawText (name, nameBox, juce::Justification::centredLeft);
            g.setColour (dim.withAlpha (0.55f));
            g.setFont (font (9.5f));
            g.drawText (value, nameBox.translated (0.0f, valueAbove ? -11.0f : 11.0f), juce::Justification::centredLeft);
        }

        const auto& bps = doc.bps;
        if (bps.size() < 2)
            return;

        juce::Path line, fill;
        const auto start = toScreen (bps.front());
        line.startNewSubPath (start);
        fill.startNewSubPath (start.x, plot.getBottom());
        fill.lineTo (start);
        for (int i = 0; i + 1 < (int) bps.size(); ++i)
        {
            const auto poly = segmentPolyline (i);
            for (size_t k = 1; k < poly.size(); ++k)
            {
                line.lineTo (poly[k]);
                fill.lineTo (poly[k]);
            }
        }
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (config.colour.withAlpha (0.35f), 0.0f, plot.getY(),
                                                 config.colour.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (config.line);
        g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

        // The line under the mouse (drag it up / down, Alt-drag to curve it).
        const auto shownSegment = gesture == Gesture::moveSegment || gesture == Gesture::bendSegment ? segment : hoveredSegment;
        if (shownSegment >= 0 && shownSegment + 1 < (int) bps.size() && (gesture == Gesture::none || shownSegment == segment))
        {
            juce::Path seg;
            const auto poly = segmentPolyline (shownSegment);
            seg.startNewSubPath (poly.front());
            for (size_t k = 1; k < poly.size(); ++k)
                seg.lineTo (poly[k]);
            g.setColour (config.line.withAlpha (0.35f));
            g.strokePath (seg, juce::PathStrokeType (6.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
        }

        paintSelectionBox (g);
        paintBreakpoints (g);

        if (gesture == Gesture::lasso)
        {
            const auto r = juce::Rectangle<float> (downPos, lassoEnd);
            g.setColour (config.line.withAlpha (0.12f));
            g.fillRect (r);
            g.setColour (config.line.withAlpha (0.8f));
            g.drawRect (r, 1.0f);
        }

        if (std::all_of (points.begin(), points.end(), [] (float v) { return v <= 0.0f; }) && bps.size() <= 2)
        {
            g.setColour (dim);
            g.setFont (font (13.0f, true));
            g.drawText ("Click to add points, drag them to shape the move. " + cmdKey + "-drag to draw freehand.",
                        plot, juce::Justification::centred);
        }
    }

    void focusGained (FocusChangeType) override { repaint(); }
    void focusLost (FocusChangeType) override   { repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();
        if (doc.bps.size() < 2)
            syncBreakpoints();

        before = doc.bps;
        downPos = lassoEnd = e.position;
        lockAxis = 0;
        if (e.getNumberOfClicks() == 1)
            addedByFirstClick = deletedByFirstClick = false;

        const auto hit = pointAt (e.position);
        const auto handle = hit < 0 ? handleAt (e.position) : Gesture::none;
        const auto seg = hit < 0 && handle == Gesture::none ? segmentAt (e.position) : -1;
        grabbed = hit;
        segment = seg;

        if (e.mods.isPopupMenu())
        {
            gesture = Gesture::rightPending;
            return;
        }
        if (hit >= 0 && e.mods.isAltDown())
        {
            gesture = Gesture::none;
            deletedByFirstClick = e.getNumberOfClicks() == 1;
            removePoint (hit);
            return;
        }
        if (hit >= 0 && e.mods.isCommandDown() && ! e.mods.isShiftDown())
        {
            gesture = Gesture::none;
            doc.bps[(size_t) hit].selected = ! doc.bps[(size_t) hit].selected;
            repaint();
            return;
        }
        if (handle != Gesture::none)
        {
            gesture = handle;
            from = doc.bps;
            return;
        }
        if (hit < 0 && e.mods.isCommandDown())
        {
            // Freehand right here: the stroke replaces the points under it when you let go.
            gesture = Gesture::freehand;
            from = doc.bps;
            for (auto& p : from)
                p.selected = false;
            stroke = cues::whammy::renderBreakpoints (from);
            strokeLo = strokeHi = lastIndex = -1;   // drawn from the first drag on: a bare click changes nothing
            return;
        }
        if (hit >= 0)
        {
            if (! doc.bps[(size_t) hit].selected)
                selectOnly (hit);
            gesture = Gesture::movePoints;
            from = doc.bps;
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
            repaint();
            return;
        }
        if (seg >= 0)
        {
            gesture = e.mods.isAltDown() ? Gesture::bendSegment : Gesture::segmentPending;
            from = doc.bps;
            return;
        }

        // A new point where you click (snapped), dragged straight away.
        const auto added = addPoint (e.position, e.mods.isShiftDown());
        addedByFirstClick = added.second && e.getNumberOfClicks() == 1;
        grabbed = added.first;
        selectOnly (grabbed);
        gesture = Gesture::movePoints;
        from = doc.bps;
        hovered = grabbed;
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        render();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto plot = plotArea();
        const auto dragged = e.getDistanceFromDragStart() > 3;
        auto dv = (downPos.y - e.position.y) / plot.getHeight();

        switch (gesture)
        {
            case Gesture::rightPending:
                if (! dragged)
                    return;
                gesture = Gesture::lasso;
                lassoBase.clear();
                for (const auto& p : doc.bps)
                    lassoBase.push_back (e.mods.isCommandDown() && p.selected);
                [[fallthrough]];
            case Gesture::lasso:
            {
                lassoEnd = e.position;
                const auto r = juce::Rectangle<float> (downPos, lassoEnd);
                for (size_t i = 0; i < doc.bps.size(); ++i)
                    doc.bps[i].selected = (i < lassoBase.size() && lassoBase[i]) || r.contains (toScreen (doc.bps[i]));
                repaint();
                return;
            }
            case Gesture::segmentPending:
                if (! dragged || segment < 0)
                    return;
                for (size_t i = 0; i < doc.bps.size(); ++i)
                    doc.bps[i].selected = (int) i == segment || (int) i == segment + 1;
                from = doc.bps;
                grabbed = segment;
                gesture = Gesture::moveSegment;
                [[fallthrough]];
            case Gesture::movePoints:
            case Gesture::moveSegment:
                moveTo (e);
                return;
            case Gesture::bendSegment:
            {
                if (segment < 0 || segment + 1 >= (int) doc.bps.size())
                    return;
                const auto& f = from[(size_t) segment];
                auto& p = doc.bps[(size_t) segment];
                p.shape = cues::whammy::Segment::bezier;
                p.tension = juce::jlimit (-1.0f, 1.0f, (f.shape == cues::whammy::Segment::bezier ? f.tension : 0.0f) + dv * 2.0f);
                render();
                return;
            }
            case Gesture::freehand:
                if (lastIndex < 0)
                    strokeTo (downPos);
                strokeTo (e.position);
                return;
            case Gesture::scaleTop:
            case Gesture::scaleBottom:
            {
                float lo = 2.0f, hi = -1.0f;
                for (const auto& p : from)
                    if (p.selected) { lo = juce::jmin (lo, p.value); hi = juce::jmax (hi, p.value); }
                const auto half = (hi - lo) * 0.5f;
                if (half < 0.005f)
                    return;
                const auto newHalf = gesture == Gesture::scaleTop ? half + dv : half - dv;
                doc.bps = cues::whammy::scaleSelection (from, newHalf / half);
                render();
                return;
            }
            case Gesture::tiltLeft:
            case Gesture::tiltRight:
                doc.bps = gesture == Gesture::tiltLeft ? cues::whammy::tiltSelection (from, dv, -dv)
                                                       : cues::whammy::tiltSelection (from, -dv, dv);
                render();
                return;
            case Gesture::none:
                return;
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        const auto was = gesture;
        gesture = Gesture::none;
        if (was == Gesture::rightPending)
        {
            showMenu (grabbed, e.position);
            return;
        }
        if (was == Gesture::segmentPending)
        {
            // A click on a line (no drag) adds a point there, like a click anywhere else.
            const auto added = addPoint (e.position, e.mods.isShiftDown());
            addedByFirstClick = added.second && e.getNumberOfClicks() == 1;
            selectOnly (added.first);
            render();
        }
        finishEdit();
        updateHover (e.position);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.mods.isAltDown())
        {
            if (deletedByFirstClick)   // Alt-double-click on a point: its first click deleted it, leave the line alone
                return;
            // Alt-double-click a line: straight again.
            const auto seg = segmentAt (e.position);
            if (seg >= 0)
            {
                doc.bps[(size_t) seg].shape = cues::whammy::Segment::linear;
                doc.bps[(size_t) seg].tension = 0.0f;
                edited();
            }
            return;
        }
        if (e.mods.isPopupMenu() || e.mods.isCommandDown() || addedByFirstClick)
            return;
        const auto hit = pointAt (e.position);
        if (hit >= 0)
            showSetValue (hit);
    }

    void mouseMove (const juce::MouseEvent& e) override { updateHover (e.position); }

    void mouseExit (const juce::MouseEvent&) override
    {
        if ((hovered >= 0 || hoveredSegment >= 0) && gesture == Gesture::none)
        {
            hovered = hoveredSegment = -1;
            repaint();
        }
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        using KP = juce::KeyPress;
        const auto cmd = juce::ModifierKeys::commandModifier;
        const auto shift = juce::ModifierKeys::shiftModifier;
        if (key == KP ('z', cmd, 0))                                             { undo(); return true; }
        if (key == KP ('z', cmd | shift, 0) || key == KP ('y', juce::ModifierKeys::ctrlModifier, 0)) { redo(); return true; }
        if (key == KP ('a', cmd, 0))
        {
            for (auto& p : doc.bps)
                p.selected = true;
            repaint();
            return true;
        }

        const auto selected = cues::whammy::numSelected (doc.bps);
        if (selected == 0)
            return false;   // Space, arrows and the rest go on to the window (and the DAW)

        if (key == KP::escapeKey)
        {
            for (auto& p : doc.bps)
                p.selected = false;
            repaint();
            return true;
        }
        if (key == KP::deleteKey || key == KP::backspaceKey)
        {
            if (cues::whammy::deleteSelection (doc.bps) > 0)
                edited();
            return true;
        }

        const auto code = key.getKeyCode();
        const auto big = key.getModifiers().isShiftDown() ? 10.0f : 1.0f;
        if (code == KP::upKey || code == KP::downKey)
        {
            doc.bps = cues::whammy::moveSelection (doc.bps, 0.0f, (code == KP::upKey ? 0.01f : -0.01f) * big);
            edited();
            return true;
        }
        if (code == KP::leftKey || code == KP::rightKey)
        {
            const auto step = (doc.grid > 0.0 ? doc.grid : 0.25) / juce::jmax (0.01, doc.beats);
            doc.bps = cues::whammy::moveSelection (doc.bps, (float) step * big * (code == KP::rightKey ? 1.0f : -1.0f), 0.0f);
            edited();
            return true;
        }
        return false;
    }

private:
    enum class Gesture { none, movePoints, segmentPending, moveSegment, bendSegment, freehand, rightPending, lasso,
                         scaleTop, scaleBottom, tiltLeft, tiltRight };

    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (10.0f, 12.0f).withTrimmedLeft (34.0f); }

    juce::Point<float> toScreen (const cues::whammy::Breakpoint& p) const
    {
        const auto plot = plotArea();
        return { plot.getX() + plot.getWidth() * p.time, plot.getBottom() - plot.getHeight() * p.value };
    }

    float timeAt (float x) const
    {
        const auto plot = plotArea();
        return juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    }

    float valueAt (float y) const
    {
        const auto plot = plotArea();
        return juce::jlimit (0.0f, 1.0f, (plot.getBottom() - y) / plot.getHeight());
    }

    float snapT (float t) const
    {
        if (doc.grid <= 0.0 || doc.grid > doc.beats)
            return juce::jlimit (0.0f, 1.0f, t);
        return cues::whammy::snapTime (t, doc.beats, doc.grid);
    }

    // A segment on screen: straight pieces that follow its shape.
    std::vector<juce::Point<float>> segmentPolyline (int i) const
    {
        const auto& a = doc.bps[(size_t) i];
        const auto& b = doc.bps[(size_t) i + 1];
        const auto pa = toScreen (a), pb = toScreen (b);
        if (a.shape == cues::whammy::Segment::square)
            return { pa, { pb.x, pa.y }, pb };
        if (a.shape == cues::whammy::Segment::linear
            || (a.shape == cues::whammy::Segment::bezier && std::abs (a.tension) < 1.0e-4f))
            return { pa, pb };
        const auto steps = juce::jlimit (4, 64, (int) ((pb.x - pa.x) / 3.0f));
        std::vector<juce::Point<float>> out;
        const auto plot = plotArea();
        for (int k = 0; k <= steps; ++k)
        {
            const auto x = (float) k / (float) steps;
            const auto v = cues::whammy::segmentValue (a.shape, a.tension, a.value, b.value, x);
            out.push_back ({ pa.x + (pb.x - pa.x) * x, plot.getBottom() - plot.getHeight() * v });
        }
        return out;
    }

    // The grid: lines at the chosen step (fewer when they'd be too close), beats and bars stronger; every 10 % across.
    void paintGrid (juce::Graphics& g, juce::Rectangle<float> plot) const
    {
        for (int k = 1; k < 10; ++k)
        {
            g.setColour (outline.withAlpha (k == 5 ? 1.0f : 0.45f));
            g.drawHorizontalLine (juce::roundToInt (plot.getBottom() - plot.getHeight() * (float) k / 10.0f), plot.getX(), plot.getRight());
        }
        if (doc.beats <= 0.0)
            return;

        auto step = doc.grid > 0.0 ? doc.grid : 1.0;
        while (plot.getWidth() * step / doc.beats < 6.0 && step < doc.beats)
            step *= 2.0;
        const auto lines = (int) std::floor (doc.beats / step + 1.0e-6);
        if (lines < 1 || lines > 1024)
            return;
        for (int k = 1; k <= lines; ++k)
        {
            const auto beat = k * step;
            if (beat >= doc.beats - 1.0e-6)
                break;
            const auto x = plot.getX() + plot.getWidth() * (float) (beat / doc.beats);
            const auto onBeat = std::abs (beat - std::round (beat)) < 1.0e-6;
            const auto bar = onBeat && (int) std::round (beat) % 4 == 0;
            g.setColour (bar ? dim.withAlpha (0.5f) : onBeat ? outline.withAlpha (1.0f) : outline.withAlpha (0.45f));
            g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
        }
    }

    // Two or more points selected: a thin box around them. Its top / bottom edge scales their values, a top corner tilts them.
    std::optional<juce::Rectangle<float>> selectionBox() const
    {
        if (gesture == Gesture::lasso || gesture == Gesture::rightPending || cues::whammy::numSelected (doc.bps) < 2)
            return std::nullopt;
        auto x0 = 1.0e9f, y0 = 1.0e9f, x1 = -1.0e9f, y1 = -1.0e9f;
        for (const auto& p : doc.bps)
            if (p.selected)
            {
                const auto s = toScreen (p);
                x0 = juce::jmin (x0, s.x); x1 = juce::jmax (x1, s.x);
                y0 = juce::jmin (y0, s.y); y1 = juce::jmax (y1, s.y);
            }
        return juce::Rectangle<float>::leftTopRightBottom (x0, y0, x1, y1).expanded (10.0f)
                   .getIntersection (getLocalBounds().toFloat().reduced (5.0f));
    }

    Gesture handleAt (juce::Point<float> pos) const
    {
        const auto box = selectionBox();
        if (! box)
            return Gesture::none;
        if (pos.getDistanceFrom (box->getTopLeft()) <= 7.0f)  return Gesture::tiltLeft;
        if (pos.getDistanceFrom (box->getTopRight()) <= 7.0f) return Gesture::tiltRight;
        if (pos.x >= box->getX() && pos.x <= box->getRight())
        {
            if (std::abs (pos.y - box->getY()) <= 5.0f)      return Gesture::scaleTop;
            if (std::abs (pos.y - box->getBottom()) <= 5.0f) return Gesture::scaleBottom;
        }
        return Gesture::none;
    }

    void paintSelectionBox (juce::Graphics& g) const
    {
        const auto box = selectionBox();
        if (! box)
            return;
        const auto active = [this] (Gesture h) { return gesture == h || (gesture == Gesture::none && hoveredHandle == h); };
        g.setColour (config.line.withAlpha (0.55f));
        g.drawRect (*box, 1.0f);

        // Scale bars on the top and bottom edges, tilt squares on the top corners.
        for (auto h : { Gesture::scaleTop, Gesture::scaleBottom })
        {
            const auto y = h == Gesture::scaleTop ? box->getY() : box->getBottom();
            const auto bar = juce::Rectangle<float> (juce::jmin (28.0f, box->getWidth() * 0.5f), 4.0f).withCentre ({ box->getCentreX(), y });
            g.setColour (active (h) ? config.line : config.line.withAlpha (0.7f));
            g.fillRoundedRectangle (bar, 2.0f);
        }
        for (auto h : { Gesture::tiltLeft, Gesture::tiltRight })
        {
            const auto c = h == Gesture::tiltLeft ? box->getTopLeft() : box->getTopRight();
            const auto sq = juce::Rectangle<float> (active (h) ? 9.0f : 7.0f, active (h) ? 9.0f : 7.0f).withCentre (c);
            g.setColour (background);
            g.fillRect (sq);
            g.setColour (config.line);
            g.drawRect (sq, 1.5f);
        }
    }

    void paintBreakpoints (juce::Graphics& g) const
    {
        const auto& bps = doc.bps;
        for (size_t i = 0; i < bps.size(); ++i)
        {
            const auto p = toScreen (bps[i]);
            const auto active = (int) i == hovered || (gesture == Gesture::movePoints && (int) i == grabbed);
            const auto sel = bps[i].selected;
            const auto r = active ? 5.5f : sel ? 5.0f : 4.0f;
            const auto dot = juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p);
            g.setColour (active || sel ? config.line : background);
            g.fillEllipse (dot);
            g.setColour (config.line);
            g.drawEllipse (dot, 1.8f);
            if (sel)
            {
                g.setColour (text);
                g.drawEllipse (dot.expanded (2.0f), 1.2f);
            }
        }

        const auto shown = gesture == Gesture::movePoints ? grabbed : hovered;
        if (shown < 0 || shown >= (int) bps.size())
            return;

        // Where and how far: bar.beat, then 0-127 and percent.
        const auto& bp = bps[(size_t) shown];
        const auto value = juce::roundToInt (bp.value * 127.0f);
        const auto label = cues::whammy::positionLabel (bp.time * doc.beats) + "   " + juce::String (value)
                         + " (" + juce::String (juce::roundToInt (bp.value * 100.0f)) + "%)";
        const auto f = font (11.0f, true);
        const auto w = juce::GlyphArrangement::getStringWidth (f, label) + 14.0f;
        const auto p = toScreen (bp);
        auto box = juce::Rectangle<float> (w, 18.0f).withCentre ({ p.x, p.y - 18.0f });
        if (box.getY() < 2.0f)
            box.setY (p.y + 10.0f);
        box = box.constrainedWithin (getLocalBounds().toFloat().reduced (2.0f));
        g.setColour (raised.withAlpha (0.95f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (config.line.withAlpha (0.8f));
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (text);
        g.setFont (f);
        g.drawText (label, box, juce::Justification::centred);
    }

    // The breakpoint under the mouse (the nearest within a few pixels), or -1.
    int pointAt (juce::Point<float> pos) const
    {
        int best = -1;
        auto bestDistance = 8.0f;
        for (size_t i = 0; i < doc.bps.size(); ++i)
        {
            const auto d = toScreen (doc.bps[i]).getDistanceFrom (pos);
            if (d <= bestDistance)
            {
                bestDistance = d;
                best = (int) i;
            }
        }
        return best;
    }

    // The segment whose drawn line passes within a few pixels, or -1.
    int segmentAt (juce::Point<float> pos) const
    {
        int best = -1;
        auto bestDistance = 6.0f;
        for (int i = 0; i + 1 < (int) doc.bps.size(); ++i)
        {
            const auto pa = toScreen (doc.bps[(size_t) i]), pb = toScreen (doc.bps[(size_t) i + 1]);
            if (pos.x < pa.x - 6.0f || pos.x > pb.x + 6.0f)
                continue;
            const auto poly = segmentPolyline (i);
            for (size_t k = 1; k < poly.size(); ++k)
            {
                juce::Point<float> nearest;
                const auto d = juce::Line<float> (poly[k - 1], poly[k]).getDistanceFromPoint (pos, nearest);
                if (d <= bestDistance)
                {
                    bestDistance = d;
                    best = i;
                }
            }
        }
        return best;
    }

    void updateHover (juce::Point<float> pos)
    {
        if (gesture != Gesture::none)
            return;
        const auto hit = pointAt (pos);
        const auto handle = hit < 0 ? handleAt (pos) : Gesture::none;
        const auto seg = hit < 0 && handle == Gesture::none ? segmentAt (pos) : -1;
        if (hit != hovered || seg != hoveredSegment || handle != hoveredHandle)
        {
            hovered = hit;
            hoveredSegment = seg;
            hoveredHandle = handle;
            setMouseCursor (hit >= 0 ? juce::MouseCursor::PointingHandCursor
                            : handle == Gesture::tiltLeft ? juce::MouseCursor::TopLeftCornerResizeCursor
                            : handle == Gesture::tiltRight ? juce::MouseCursor::TopRightCornerResizeCursor
                            : handle != Gesture::none || seg >= 0 ? juce::MouseCursor::UpDownResizeCursor
                            : juce::MouseCursor::CrosshairCursor);
            repaint();
        }
    }

    void selectOnly (int index)
    {
        for (size_t i = 0; i < doc.bps.size(); ++i)
            doc.bps[i].selected = (int) i == index;
    }

    // Adds a point at 'pos' (snapped unless 'free'); with no room between its neighbours, the nearer one is used instead.
    // Returns its index and whether it's new.
    std::pair<int, bool> addPoint (juce::Point<float> pos, bool free)
    {
        auto& bps = doc.bps;
        auto t = timeAt (pos.x);
        auto v = valueAt (pos.y);
        if (! free)
        {
            t = snapT (t);
            v = cues::whammy::snapValue (v);
        }
        const auto at = (int) (std::upper_bound (bps.begin(), bps.end(), t,
                                                 [] (float time, const cues::whammy::Breakpoint& p) { return time < p.time; })
                               - bps.begin());
        const auto gap = cues::whammy::minPointGap;
        const auto lo = at > 0 ? bps[(size_t) at - 1].time + gap : 0.0f;
        const auto hi = at < (int) bps.size() ? bps[(size_t) at].time - gap : 1.0f;
        if (at == 0 || at == (int) bps.size() || lo > hi)
        {
            const auto prevGap = at > 0 ? t - bps[(size_t) at - 1].time : 2.0f;
            const auto nextGap = at < (int) bps.size() ? bps[(size_t) at].time - t : 2.0f;
            const auto index = juce::jlimit (0, (int) bps.size() - 1, prevGap <= nextGap ? at - 1 : at);
            bps[(size_t) index].value = v;
            return { index, false };
        }
        // The new point carries on the shape of the line it splits.
        auto p = bps[(size_t) at - 1];
        p.time = juce::jlimit (lo, hi, t);
        p.value = v;
        p.selected = false;
        bps.insert (bps.begin() + at, p);
        return { at, true };
    }

    void moveTo (const juce::MouseEvent& e)
    {
        if (grabbed < 0 || grabbed >= (int) from.size() || e.getDistanceFromDragStart() < 2)
            return;
        const auto plot = plotArea();
        auto dx = (e.position.x - downPos.x) / plot.getWidth();
        auto dy = (downPos.y - e.position.y) / plot.getHeight();
        const auto lock = e.mods.isCommandDown() && e.mods.isShiftDown();
        if (lock)
        {
            // One direction only: whichever moved more first.
            if (lockAxis == 0 && e.getDistanceFromDragStart() > 4)
                lockAxis = std::abs (e.position.x - downPos.x) >= std::abs (e.position.y - downPos.y) ? 1 : 2;
            if (lockAxis != 1) dx = 0.0f;
            if (lockAxis != 2) dy = 0.0f;
        }
        const auto last = (int) from.size() - 1;
        if (gesture == Gesture::moveSegment || grabbed == 0 || grabbed == last)
            dx = 0.0f;   // the first and last points (and a dragged line) only move up and down

        const auto& g = from[(size_t) grabbed];
        auto dt = dx, dv = dy;
        if (! (e.mods.isShiftDown() && ! lock))
        {
            if (std::abs (dt) > 0.0f)
                dt = snapT (g.time + dt) - g.time;
            dv = cues::whammy::snapValue (g.value + dv) - g.value;
        }
        doc.bps = cues::whammy::moveSelection (from, dt, dv);
        render();
    }

    // Freehand: the samples from where the stroke started to here, interpolated between mouse events.
    void strokeTo (juce::Point<float> pos)
    {
        const auto plot = plotArea();
        const auto n = (int) stroke.size();
        if (n < 2)
            return;
        const auto index = juce::jlimit (0, n - 1, juce::roundToInt ((pos.x - plot.getX()) / plot.getWidth() * (float) (n - 1)));
        const auto value = valueAt (pos.y);
        if (lastIndex < 0 || lastIndex == index)
        {
            stroke[(size_t) index] = value;
        }
        else
        {
            const auto step = index > lastIndex ? 1 : -1;
            for (int i = lastIndex; i != index + step; i += step)
                stroke[(size_t) i] = lastValue + (value - lastValue) * (float) (i - lastIndex) / (float) (index - lastIndex);
        }
        strokeLo = strokeLo < 0 ? index : juce::jmin (strokeLo, index);
        strokeHi = strokeHi < 0 ? index : juce::jmax (strokeHi, index);
        lastIndex = index;
        lastValue = value;
        doc.bps = cues::whammy::mergeStroke (from, stroke, strokeLo, strokeHi);
        render();
    }

    void removePoint (int index)
    {
        if (index <= 0 || index >= (int) doc.bps.size() - 1)   // the first and last always stay
            return;
        doc.bps.erase (doc.bps.begin() + index);
        hovered = hoveredSegment = -1;
        edited();
    }

    // The breakpoints changed: new samples, shown in both pads and on the tile.
    void render()
    {
        points = cues::whammy::renderBreakpoints (doc.bps);
        repaint();
        if (onChange)
            onChange();
    }

    // An edit is done: an undo step, and the drawing is saved.
    void edited()
    {
        render();
        doc.history.record (doc.bps);
        if (onCommit) onCommit();
    }

    void finishEdit()
    {
        doc.history.record (doc.bps);   // no step if only the selection changed
        if (! cues::whammy::samePoints (before, doc.bps))
        {
            if (onCommit) onCommit();
        }
        repaint();
    }

    void restored()
    {
        hovered = hoveredSegment = -1;
        points = cues::whammy::renderBreakpoints (doc.bps);
        repaint();
        if (onChange) onChange();
        if (onCommit) onCommit();
    }

    void showMenu (int hit, juce::Point<float> at)
    {
        if (hit >= 0 && ! doc.bps[(size_t) hit].selected)
            selectOnly (hit);
        repaint();
        const auto selected = cues::whammy::numSelected (doc.bps);
        bool interior = false, segments = false;
        auto current = -1;
        for (size_t i = 0; i < doc.bps.size(); ++i)
            if (doc.bps[i].selected)
            {
                interior = interior || (i > 0 && i + 1 < doc.bps.size());
                if (i + 1 < doc.bps.size())
                {
                    if (! segments)
                        current = (int) doc.bps[i].shape;
                    else if (current != (int) doc.bps[i].shape)
                        current = -1;
                    segments = true;
                }
            }

        const auto item = [] (juce::PopupMenu& m, int id, const juce::String& name, bool enabled, const juce::String& keys = {},
                              bool ticked = false)
        {
            juce::PopupMenu::Item it (name);
            it.itemID = id;
            it.isEnabled = enabled;
            it.isTicked = ticked;
            it.shortcutKeyDescription = keys;
            m.addItem (it);
        };
        juce::PopupMenu shapes;
        for (int k = 0; k < cues::whammy::numSegments; ++k)
            item (shapes, 100 + k, cues::whammy::segmentName ((cues::whammy::Segment) k), segments, {}, k == current);

        juce::PopupMenu m;
        item (m, 1, "Delete", interior, "Delete");
        item (m, 2, "Set value...", selected > 0, "double-click");
        m.addSubMenu ("Line shape", shapes, segments);
        item (m, 3, "Invert", selected > 0);
        item (m, 4, "Scale / compress...", selected > 0);
        m.addSeparator();
        item (m, 5, "Select all", true, cmdKey + "+A");
        item (m, 6, "Clear selection", selected > 0, "Esc");
        m.addSeparator();
        item (m, 7, "Undo", doc.history.canUndo(), cmdKey + "+Z");
        item (m, 8, "Redo", doc.history.canRedo(), cmdKey + "+Shift+Z");

        juce::Component::SafePointer<DrawPad> safe (this);
        const auto target = localAreaToGlobal (juce::Rectangle<int> (juce::roundToInt (at.x), juce::roundToInt (at.y), 1, 1));
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (target), [safe] (int result)
        {
            if (safe == nullptr || result == 0)
                return;
            auto& bps = safe->doc.bps;
            if (result >= 100)
            {
                cues::whammy::setSelectionShape (bps, (cues::whammy::Segment) (result - 100));
                safe->edited();
            }
            else if (result == 1)
            {
                if (cues::whammy::deleteSelection (bps) > 0)
                    safe->edited();
            }
            else if (result == 2)
            {
                for (size_t i = 0; i < bps.size(); ++i)
                    if (bps[i].selected)
                    {
                        safe->showSetValue ((int) i);
                        break;
                    }
            }
            else if (result == 3)
            {
                cues::whammy::invertSelection (bps);
                safe->edited();
            }
            else if (result == 4) safe->showScale();
            else if (result == 5) { for (auto& p : bps) p.selected = true; safe->repaint(); }
            else if (result == 6) { for (auto& p : bps) p.selected = false; safe->repaint(); }
            else if (result == 7) safe->undo();
            else if (result == 8) safe->redo();
        });
    }

    // Double-click a point (or Set value...): its value as 0-127 and, for one point, its position.
    void showSetValue (int index)
    {
        if (index < 0 || index >= (int) doc.bps.size())
            return;
        if (! doc.bps[(size_t) index].selected)
            selectOnly (index);
        repaint();
        const auto count = cues::whammy::numSelected (doc.bps);
        const auto& bp = doc.bps[(size_t) index];
        const auto movable = count == 1 && index > 0 && index + 1 < (int) doc.bps.size();

        auto* w = new juce::AlertWindow ("Set value", count > 1 ? juce::String ("Sets all ") + juce::String (count) + " selected points."
                                                                : juce::String(), juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("value", juce::String (juce::roundToInt (bp.value * 127.0f)), "Value (0-127: 0 = heel, 127 = toe)");
        if (movable)
        {
            auto pos = cues::whammy::positionLabel (bp.time * doc.beats);
            if (pos.containsChar (' '))
                pos = "+" + juce::String (bp.time * doc.beats, 3);
            w->addTextEditor ("position", pos, "Position (bar.beat like 1.3 or 2.1.3, or +beats from the start)");
        }
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        juce::Component::SafePointer<DrawPad> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w, index, movable] (int result)
        {
            if (result != 1 || safe == nullptr || index >= (int) safe->doc.bps.size())
                return;
            auto& bps = safe->doc.bps;
            const auto valueText = w->getTextEditorContents ("value").trim();
            if (valueText.isNotEmpty() && valueText.containsOnly ("0123456789"))
            {
                const auto v = (float) juce::jlimit (0, 127, valueText.getIntValue()) / 127.0f;
                for (auto& p : bps)
                    if (p.selected)
                        p.value = v;
            }
            if (movable)
            {
                const auto beat = cues::whammy::parsePosition (w->getTextEditorContents ("position"));
                if (beat >= 0.0 && safe->doc.beats > 0.0)
                {
                    const auto gap = cues::whammy::minPointGap;
                    const auto lo = bps[(size_t) index - 1].time + gap, hi = bps[(size_t) index + 1].time - gap;
                    if (lo <= hi)
                        bps[(size_t) index].time = juce::jlimit (lo, hi, (float) (beat / safe->doc.beats));
                }
            }
            safe->edited();
        }), true);
    }

    void showScale()
    {
        auto* w = new juce::AlertWindow ("Scale / compress",
                                         "Scales the selected points around the middle of their range: below 100 % brings them "
                                         "closer together, above 100 % spreads them apart. (You can also drag the top or bottom "
                                         "edge of the selection box.)",
                                         juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("percent", "50", "Percent");
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        juce::Component::SafePointer<DrawPad> safe (this);
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
        {
            const auto t = w->getTextEditorContents ("percent").trim().trimCharactersAtEnd ("% ");
            if (result != 1 || safe == nullptr || t.isEmpty() || ! t.containsOnly ("0123456789."))
                return;
            safe->doc.bps = cues::whammy::scaleSelection (safe->doc.bps, (float) t.getDoubleValue() / 100.0f);
            safe->edited();
        }), true);
    }

    const MovesConfig& config;
    DrawDoc& doc;
    Gesture gesture = Gesture::none, hoveredHandle = Gesture::none;
    std::vector<cues::whammy::Breakpoint> before, from;   // at mouse down / where a drag started
    juce::Point<float> downPos, lassoEnd;
    std::vector<bool> lassoBase;
    int grabbed = -1, segment = -1, hovered = -1, hoveredSegment = -1, lockAxis = 0;
    bool addedByFirstClick = false, deletedByFirstClick = false;
    std::vector<float> stroke;
    int strokeLo = -1, strokeHi = -1, lastIndex = -1;
    float lastValue = 0.0f;
};


//==============================================================================
// Draw > Wave...: a sine, triangle, square or saw (like Reaper's CC LFO) written into the drawing as you set it.
class MovesPanel::WaveEditor final : public juce::Component
{
public:
    WaveEditor (WaveSettings start, juce::Colour colour, std::function<void (const WaveSettings&)> changed)
        : settings (start), onChange (std::move (changed))
    {
        for (int w = 0; w < cues::whammy::numWaves; ++w)
            typeBox.addItem (cues::whammy::waveName ((cues::whammy::Wave) w), w + 1);
        typeBox.setSelectedId (settings.type + 1, juce::dontSendNotification);
        typeBox.onChange = [this] { settings.type = typeBox.getSelectedId() - 1; update(); };
        addAndMakeVisible (typeBox);

        auto setup = [this, colour] (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& tip,
                                     double lo, double hi, double step, double value, const juce::String& suffix)
        {
            styleCaption (l, name);
            l.setTooltip (tip);
            addAndMakeVisible (l);
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
            s.setRange (lo, hi, step);
            s.setTextValueSuffix (suffix);
            s.setValue (value, juce::dontSendNotification);
            s.setColour (juce::Slider::trackColourId, colour);
            s.setTooltip (tip);
            s.onValueChange = [this] { read(); };
            addAndMakeVisible (s);
        };
        setup (cyclesSlider, cyclesLabel, "WAVES", "How many waves across the move's length (it follows Length, so it's in time)",
               0.5, 8.0, 0.5, settings.cycles, "");
        setup (phaseSlider, phaseLabel, "PHASE", "Where the wave starts: 0 = its low point, 180 = its high point", 0.0, 360.0, 15.0, settings.phase, " deg");
        setup (skewSlider, skewLabel, "SHAPE", "Tilts each wave: a triangle leans towards a saw, a sine rises fast and falls slowly "
               "(or the other way round), a square gets a shorter or longer high part. Saws don't use it.", -100.0, 100.0, 5.0, settings.shape * 100.0, " %");
        setup (lowSlider, lowLabel, "LOW", "The wave's lowest point (0 = heel)", 0.0, 100.0, 5.0, settings.low * 100.0, " %");
        setup (highSlider, highLabel, "HIGH", "The wave's highest point (100 = toe)", 0.0, 100.0, 5.0, settings.high * 100.0, " %");
        setup (growSlider, growLabel, "GROW", "Above 0: the waves build up from Low to full across the move. Below 0: they die away "
               "(Reaper calls it amp skew)", -100.0, 100.0, 5.0, settings.grow * 100.0, " %");
        setup (speedSlider, speedLabel, "SPEED", "Above 0: the waves speed up across the move. Below 0: they slow down "
               "(Reaper calls it frequency skew)", -100.0, 100.0, 5.0, settings.speed * 100.0, " %");

        styleCaption (typeLabel, "TYPE");
        addAndMakeVisible (typeLabel);
        hint.setText ("Changes replace the drawing. Fix it by hand after, or save it in My drawings.", juce::dontSendNotification);
        hint.setFont (font (11.5f));
        hint.setColour (juce::Label::textColourId, dim);
        hint.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (hint);
        updateSkew();
        setSize (340, 330);
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto rowOf = [&r] (juce::Label& l, juce::Component& c)
        {
            auto line = r.removeFromTop (30);
            l.setBounds (line.removeFromLeft (64));
            c.setBounds (line.reduced (0, 2));
            r.removeFromTop (4);
        };
        rowOf (typeLabel, typeBox);
        rowOf (cyclesLabel, cyclesSlider);
        rowOf (phaseLabel, phaseSlider);
        rowOf (skewLabel, skewSlider);
        rowOf (lowLabel, lowSlider);
        rowOf (highLabel, highSlider);
        rowOf (growLabel, growSlider);
        rowOf (speedLabel, speedSlider);
        hint.setBounds (r);
    }

private:
    void read()
    {
        settings.cycles = cyclesSlider.getValue();
        settings.phase = phaseSlider.getValue();
        settings.shape = skewSlider.getValue() / 100.0;
        settings.grow = growSlider.getValue() / 100.0;
        settings.speed = speedSlider.getValue() / 100.0;
        settings.low = lowSlider.getValue() / 100.0;
        settings.high = highSlider.getValue() / 100.0;
        update();
    }

    void update()
    {
        updateSkew();
        if (onChange)
            onChange (settings);
    }

    void updateSkew()
    {
        const auto saw = settings.type == (int) cues::whammy::Wave::sawUp || settings.type == (int) cues::whammy::Wave::sawDown;
        skewSlider.setEnabled (! saw);
        skewLabel.setAlpha (saw ? 0.4f : 1.0f);
    }

    WaveSettings settings;
    std::function<void (const WaveSettings&)> onChange;
    juce::ComboBox typeBox;
    juce::Slider cyclesSlider, phaseSlider, skewSlider, lowSlider, highSlider, growSlider, speedSlider;
    juce::Label typeLabel, cyclesLabel, phaseLabel, skewLabel, lowLabel, highLabel, growLabel, speedLabel, hint;
};

//==============================================================================
//==============================================================================
// The pad in a larger window (the ⤢ button on the pad): the same drawing, history and grid, with every tool around it.
// Wave... opens beside the pad here, so the drawing stays in view.
class MovesPanel::LargeEditor final : public juce::Component
{
public:
    explicit LargeEditor (MovesPanel& p) : pad (p.config, *p.doc), panel (p)
    {
        pad.setComponentID (p.config.idPrefix + ".drawLarge");
        pad.onChange = [this] { panel.padChanged(); };
        pad.onCommit = [this] { panel.commitDrawing(); };
        addAndMakeVisible (pad);

        for (size_t i = 0; i < lengths.size(); ++i)
            lengthBox.addItem (cues::formatBeats (lengths[i]), (int) i + 1);
        lengthBox.onChange = [this]
        {
            const auto i = lengthBox.getSelectedItemIndex();
            if (i >= 0)
                panel.state.setProperty (panel.config.beatsId, lengths[(size_t) i], nullptr);
        };
        fillGridBox (gridBox);
        gridBox.onChange = [this]
        {
            const auto id = gridBox.getSelectedId();
            if (id > 0)
                panel.setGrid (grids[(size_t) id - 1].beats);
        };
        styleCaption (lengthLabel, "LENGTH");
        styleCaption (gridLabel, "GRID");

        for (auto* b : { &waveButton, &importButton, &undoButton, &redoButton, &clearButton, &smoothButton })
        {
            styleButton (*b);
            addAndMakeVisible (b);
        }
        waveButton.setClickingTogglesState (true);
        waveButton.setColour (juce::TextButton::buttonOnColourId, p.config.colour);
        waveButton.setColour (juce::TextButton::textColourOnId,
                              p.config.colour.getPerceivedBrightness() > 0.6f ? juce::Colours::black : juce::Colours::white);
        waveButton.setTooltip (p.waveButton.getTooltip());
        importButton.setTooltip (p.importButton.getTooltip());
        clearButton.setTooltip (p.clearButton.getTooltip());
        smoothButton.setTooltip (p.smoothButton.getTooltip());
        undoButton.setTooltip ("Undo the last change to the drawing (" + cmdKey + "+Z)");
        redoButton.setTooltip ("Redo (" + cmdKey + "+Shift+Z)");
        waveButton.onClick = [this] { showWave (waveButton.getToggleState()); };
        importButton.onClick = [this] { panel.chooseMidiFile(); };
        undoButton.onClick = [this] { pad.undo(); };
        redoButton.onClick = [this] { pad.redo(); };
        clearButton.onClick = [this] { panel.clearDrawing(); };
        smoothButton.onClick = [this] { panel.smoothDrawing(); };

        help.setText (padHelp(), juce::dontSendNotification);
        help.setFont (font (12.0f));
        help.setColour (juce::Label::textColourId, dim);
        help.setJustificationType (juce::Justification::topLeft);
        help.setMinimumHorizontalScale (1.0f);
        for (auto* c : std::initializer_list<juce::Component*> { &lengthLabel, &lengthBox, &gridLabel, &gridBox, &help })
            addAndMakeVisible (c);
        waveView.setScrollBarsShown (true, false, false, false);
        waveView.setScrollBarThickness (8);
        addChildComponent (waveView);

        refresh();
        setSize (1000, 500);
    }

    void refresh()
    {
        const auto beats = (double) panel.state[panel.config.beatsId];
        int best = 0;
        for (size_t i = 0; i < lengths.size(); ++i)
            if (std::abs (lengths[i] - beats) < std::abs (lengths[(size_t) best] - beats))
                best = (int) i;
        lengthBox.setSelectedItemIndex (best, juce::dontSendNotification);
        selectGrid (gridBox, panel.doc->grid);
        undoButton.setEnabled (panel.doc->history.canUndo());
        redoButton.setEnabled (panel.doc->history.canRedo());
        pad.repaint();
    }

    void showWave (bool show)
    {
        waveButton.setToggleState (show, juce::dontSendNotification);
        if (! show)
        {
            waveView.setViewedComponent (nullptr, false);
            wave.reset();
        }
        else if (wave == nullptr)
        {
            ++panel.waveSession;
            juce::Component::SafePointer<MovesPanel> safe (&panel);
            wave = std::make_unique<WaveEditor> (panel.wave, panel.config.colour, [safe] (const WaveSettings& s)
            {
                if (safe != nullptr)
                {
                    safe->wave = s;
                    safe->applyWave();
                }
            });
            waveView.setViewedComponent (wave.get(), false);
        }
        resized();
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        // One row of tools, or two when the window is narrow (undo, redo, clear and smooth on the second).
        const auto twoRows = r.getWidth() < 900;
        auto bar = r.removeFromTop (32);
        auto bar2 = bar;
        if (twoRows)
        {
            r.removeFromTop (6);
            bar2 = r.removeFromTop (32).withWidth (312);
        }
        lengthLabel.setBounds (bar.removeFromLeft (58));
        lengthBox.setBounds (bar.removeFromLeft (110).reduced (0, 2));
        bar.removeFromLeft (18);
        gridLabel.setBounds (bar.removeFromLeft (40));
        gridBox.setBounds (bar.removeFromLeft (96).reduced (0, 2));
        bar.removeFromLeft (18);
        waveButton.setBounds (bar.removeFromLeft (96).reduced (0, 2));
        bar.removeFromLeft (8);
        importButton.setBounds (bar.removeFromLeft (128).reduced (0, 2));
        if (twoRows)
            bar = bar2;
        smoothButton.setBounds (bar.removeFromRight (80).reduced (0, 2));
        bar.removeFromRight (8);
        clearButton.setBounds (bar.removeFromRight (70).reduced (0, 2));
        bar.removeFromRight (18);
        redoButton.setBounds (bar.removeFromRight (64).reduced (0, 2));
        bar.removeFromRight (8);
        undoButton.setBounds (bar.removeFromRight (64).reduced (0, 2));

        r.removeFromTop (10);
        // Wave settings beside the pad, scrolling when the window is short.
        if (wave != nullptr)
        {
            const auto column = r.removeFromRight (wave->getWidth() + waveView.getScrollBarThickness());
            waveView.setBounds (column);
            wave->setSize (wave->getWidth(), juce::jmax (330, column.getHeight()));
            r.removeFromRight (10);
        }
        waveView.setVisible (wave != nullptr);

        juce::AttributedString text;
        text.append (help.getText(), help.getFont(), dim);
        juce::TextLayout layout;
        layout.createLayout (text, (float) r.getWidth() - 8.0f);
        const auto helpH = juce::jmin ((int) std::ceil (layout.getHeight()) + 6, r.getHeight() / 3);
        help.setBounds (r.removeFromBottom (helpH));
        r.removeFromBottom (8);
        pad.setBounds (r);
    }

    DrawPad pad;

private:
    MovesPanel& panel;
    juce::Label lengthLabel, gridLabel, help;
    juce::ComboBox lengthBox, gridBox;
    juce::TextButton waveButton { "Wave..." }, importButton { "Import MIDI..." }, undoButton { "Undo" }, redoButton { "Redo" },
                     clearButton { "Clear" }, smoothButton { "Smooth" };
    std::unique_ptr<WaveEditor> wave;
    juce::Viewport waveView;   // declared after 'wave': it lets go of it first
};

class MovesPanel::LargeWindow final : public juce::DocumentWindow
{
public:
    LargeWindow (MovesPanel& p, const juce::String& title)
        : juce::DocumentWindow (title, background, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        editor = new LargeEditor (p);
        setContentOwned (editor, false);
        setResizable (true, false);
        setResizeLimits (700, 360, 4000, 3000);
        centreWithSize (1000, 500);
    }
    void closeButtonPressed() override { setVisible (false); }
    LargeEditor* editor = nullptr;
};

MovesPanel::MovesPanel (PedalCuesProcessor& p, MovesConfig c)
    : proc (p), state (p.state), config (std::move (c)),
      section (config.idPrefix + ".sweeps", config.title, config.hint, config.colour)
{
    doc = std::make_unique<DrawDoc>();
    pad = std::make_unique<DrawPad> (config, *doc);
    pad->setComponentID (config.idPrefix + ".draw");
    drawings.addListener (this);   // another panel or PedalCues instance saved, renamed or deleted one

    addAndMakeVisible (section);
    addAndMakeVisible (controls);
    for (auto* slot : { &header, &row })
    {
        slot->setInterceptsMouseClicks (false, true);
        addAndMakeVisible (slot);
    }
    controls.setComponentID (config.idPrefix + ".sweepControls");
    controls.setInterceptsMouseClicks (false, true);

    for (size_t i = 0; i < lengths.size(); ++i)
        lengthBox.addItem (cues::formatBeats (lengths[i]), (int) i + 1);
    lengthBox.onChange = [this]
    {
        const auto i = lengthBox.getSelectedItemIndex();
        if (i >= 0)
            state.setProperty (config.beatsId, lengths[(size_t) i], nullptr);
    };

    curveSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    curveSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    curveSlider.setRange (0.25, 4.0, 0.01);
    curveSlider.setSkewFactorFromMidPoint (1.0);
    curveSlider.setColour (juce::Slider::trackColourId, config.colour);
    curveSlider.setTooltip ("1 = linear, below 1 = fast start, above 1 = slow start");
    curveSlider.onValueChange = [this] { state.setProperty (config.curveId, curveSlider.getValue(), nullptr); };

    styleCaption (lengthLabel, "LENGTH");
    styleCaption (curveLabel, "CURVE");
    resetToggle.setButtonText (config.resetText);
    resetToggle.setTooltip (config.resetTooltip);
    resetToggle.setColour (juce::ToggleButton::tickColourId, config.line);
    resetToggle.onClick = [this] { state.setProperty (config.resetId, resetToggle.getToggleState(), nullptr); };

    for (auto* comp : std::initializer_list<juce::Component*> { &lengthLabel, &lengthBox, &curveLabel, &curveSlider, &resetToggle })
        addAndMakeVisible (comp);

    // Draw mode: Wave... takes Curve's place (Curve only shapes the ready-made moves).
    waveButton.setComponentID (config.idPrefix + ".wave");
    waveButton.setColour (juce::TextButton::buttonColourId, surface);
    waveButton.setColour (juce::TextButton::textColourOffId, text);
    waveButton.setTooltip ("Make a wave: sine, triangle, square or saw, with how many waves, phase, shape, range, grow and speed "
                           "(like Reaper's CC LFO)");
    waveButton.onClick = [this] { showWaveEditor(); };
    waveButton.setTooltip (waveButton.getTooltip() + ". Click it again to close the wave settings.");
    addChildComponent (waveButton);
    importButton.setComponentID (config.idPrefix + ".importMidi");
    importButton.setColour (juce::TextButton::buttonColourId, surface);
    importButton.setColour (juce::TextButton::textColourOffId, text);
    importButton.setTooltip ("Load a move from a MIDI file, e.g. a clip with CC automation you drew in your DAW (or drop the .mid "
                             "file on this card). Then Save it in My drawings to reuse it in any song.");
    importButton.onClick = [this] { chooseMidiFile(); };
    addChildComponent (importButton);

    // Shapes / Draw switch in the card header.
    const auto onText = config.colour.getPerceivedBrightness() > 0.6f ? juce::Colours::black : juce::Colours::white;
    for (auto* b : { &shapesButton, &drawButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4301);
        b->setColour (juce::TextButton::buttonColourId, surface);
        b->setColour (juce::TextButton::buttonOnColourId, config.colour);
        b->setColour (juce::TextButton::textColourOffId, dim);
        b->setColour (juce::TextButton::textColourOnId, onText);
        addAndMakeVisible (b);
    }
    shapesButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    drawButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    drawButton.setComponentID (config.idPrefix + ".drawMode");
    shapesButton.setTooltip (config.shapesTooltip);
    drawButton.setTooltip (config.drawTooltip);
    shapesButton.onClick = [this] { if (shapesButton.getToggleState()) state.setProperty (config.drawId, false, nullptr); };
    drawButton.onClick   = [this] { if (drawButton.getToggleState())   state.setProperty (config.drawId, true, nullptr); };

    // The pad's grid (shared with the larger editor).
    styleCaption (gridLabel, "GRID");
    addChildComponent (gridLabel);
    fillGridBox (gridBox);
    gridBox.setComponentID (config.idPrefix + ".drawGrid");
    gridBox.onChange = [this]
    {
        const auto id = gridBox.getSelectedId();
        if (id > 0)
            setGrid (grids[(size_t) id - 1].beats);
    };
    selectGrid (gridBox, doc->grid);
    addChildComponent (gridBox);

    for (auto* b : { &clearButton, &smoothButton })
    {
        b->setColour (juce::TextButton::buttonColourId, surface);
        b->setColour (juce::TextButton::textColourOffId, text);
        addChildComponent (b);
    }
    for (auto* b : { &saveButton, &moreButton })
    {
        b->setColour (juce::TextButton::buttonColourId, surface);
        b->setColour (juce::TextButton::textColourOffId, text);
        addChildComponent (b);
    }
    saveButton.setTooltip ("Save this drawing in My drawings (it's kept with your setup)");
    moreButton.setTooltip ("Save as new, rename or delete a saved drawing");
    saveButton.onClick = [this] { saveDrawing(); };
    moreButton.onClick = [this] { showDrawingMenu(); };

    libraryBox.setTextWhenNothingSelected ("My drawings");
    libraryBox.setTextWhenNoChoicesAvailable ("No saved drawings");
    libraryBox.setTooltip ("Load one of your saved drawings to use or edit it. Shared by the Whammy and the QC expression.");
    libraryBox.onChange = [this] { loadDrawing (libraryBox.getSelectedId()); };
    addChildComponent (libraryBox);

    clearButton.setTooltip ("Reset the drawing to heel");
    smoothButton.setTooltip ("Round off sharp edges in the drawing");
    clearButton.onClick = [this] { clearDrawing(); };
    smoothButton.onClick = [this] { smoothDrawing(); };

    pad->onChange = [this] { padChanged(); };
    pad->onCommit = [this] { commitDrawing(); };
    addChildComponent (*pad);

    expandButton = std::make_unique<juce::TextButton> ("Larger editor");
    expandButton->setColour (juce::TextButton::buttonColourId, surface);
    expandButton->setColour (juce::TextButton::textColourOffId, text);
    expandButton->setComponentID (config.idPrefix + ".drawLarger");
    expandButton->setTooltip ("Edit in a larger window");
    expandButton->onClick = [this] { openLargeEditor(); };
    addChildComponent (*expandButton);

    refresh();
}

MovesPanel::~MovesPanel()
{
    if (waveBox != nullptr)
        waveBox->dismiss();
    largeWindow.reset();   // its pad uses the doc
    drawings.removeListener (this);
}

void MovesPanel::clearDrawing()
{
    std::fill (pad->points.begin(), pad->points.end(), 0.0f);
    commitDrawing();
}

void MovesPanel::smoothDrawing()
{
    auto& v = pad->points;
    for (int pass = 0; pass < 2 && v.size() > 2; ++pass)
    {
        const auto copy = v;
        for (size_t i = 1; i + 1 < v.size(); ++i)
            v[i] = (copy[i - 1] + 2.0f * copy[i] + copy[i + 1]) * 0.25f;
    }
    commitDrawing();
}

void MovesPanel::setGrid (double beats)
{
    doc->grid = beats;
    selectGrid (gridBox, beats);
    if (large != nullptr)
        large->refresh();
    pad->repaint();
}

void MovesPanel::padChanged()
{
    updateDrawTile();
    pad->repaint();
    if (large != nullptr)
        large->pad.repaint();
}

void MovesPanel::openLargeEditor()
{
    if (largeWindow == nullptr)
    {
        auto window = std::make_unique<LargeWindow> (*this, "Draw: " + config.title + " - PedalCues");
        large = window->editor;
        largeWindow = std::move (window);
    }
    large->refresh();
    largeWindow->setVisible (true);
    largeWindow->toFront (true);
}

void MovesPanel::refresh()
{
    const auto beats = (double) state[config.beatsId];
    const auto drawMode = (bool) state[config.drawId];

    resetToggle.setToggleState ((bool) state[config.resetId], juce::dontSendNotification);
    curveSlider.setValue ((double) state[config.curveId], juce::dontSendNotification);

    int bestLength = 0;
    for (size_t i = 0; i < lengths.size(); ++i)
        if (std::abs (lengths[i] - beats) < std::abs (lengths[(size_t) bestLength] - beats))
            bestLength = (int) i;
    lengthBox.setSelectedItemIndex (bestLength, juce::dontSendNotification);

    (drawMode ? drawButton : shapesButton).setToggleState (true, juce::dontSendNotification);
    curveLabel.setVisible (! drawMode);
    curveSlider.setVisible (! drawMode);
    waveButton.setVisible (drawMode);
    importButton.setVisible (drawMode);

    pad->points = cues::whammy::decodeDrawing (state[config.drawingId].toString());
    pad->syncBreakpoints();
    doc->beats = beats;
    pad->setVisible (drawMode);
    for (auto* c : std::initializer_list<juce::Component*> { &clearButton, &smoothButton, &saveButton, &moreButton, &libraryBox,
                                                             &gridLabel, &gridBox, expandButton.get() })
        c->setVisible (drawMode);
    if (! drawMode && largeWindow != nullptr)
        largeWindow->setVisible (false);
    if (large != nullptr)
        large->refresh();

    // My drawings, with the loaded one selected.
    libraryBox.clear (juce::dontSendNotification);
    const auto saved = drawings;
    for (int i = 0; i < saved.getNumChildren(); ++i)
        libraryBox.addItem (saved.getChild (i)[IDs::name].toString(), i + 1);
    if (saved.getNumChildren() > 0)
        libraryBox.addSeparator();
    libraryBox.addItem ("New drawing", newDrawingId);
    for (int i = 0; i < saved.getNumChildren(); ++i)
        if (saved.getChild (i)[IDs::name].toString() == drawingName())
            libraryBox.setSelectedId (i + 1, juce::dontSendNotification);
    pad->repaint();

    drawTile.reset (new Tile (proc, Tile::Look::sweep));
    drawTile->title = drawingName().isNotEmpty() ? drawingName() : juce::String ("Drawn move");
    drawTile->colour = config.line;
    drawTile->setTooltip (config.drawnTooltip + config.tileNote);
    drawTile->makeCue = [this] { return finished (config.makeDrawn (pad->points, drawingName())); };
    addChildComponent (drawTile.get());
    drawTile->setVisible (drawMode);
    updateDrawTile();

    shapeTiles.clear();
    for (int s = 0; s < config.numShapes; ++s)
    {
        auto* t = shapeTiles.add (new Tile (proc, Tile::Look::sweep));
        t->makeCue = [this, s] { return finished (config.makeShape (s)); };
        curveOf (*t, config.makeShape (s));
        t->title = config.shapeName (s);
        t->subtitle = (config.shapeHolds && config.shapeHolds (s) ? "hold " : "") + cues::formatBeats (beats);
        t->colour = config.line;
        t->setTooltip (config.shapeDescription (s) + ". Drag onto the timeline where the move should start." + config.tileNote);
        addChildComponent (t);
        t->setVisible (! drawMode);
    }

    resized();
}

void MovesPanel::resized()
{
    section.setBounds (getLocalBounds());

    auto content = section.contentArea();
    if (config.extraRowHeight > 0)
    {
        row.setBounds (content.removeFromTop (config.extraRowHeight));
        content.removeFromTop (8);
    }

    // The tools row, left to right: Length, then in Draw: Grid, Wave..., Import MIDI..., Larger editor; in Shapes: the
    // curve. The reset switch ends the row, or starts a second one when the row is too narrow (then the pad is shorter).
    const auto drawMode = (bool) state[config.drawId];
    const auto resetWidth = juce::GlyphArrangement::getStringWidthInt (font (14.0f), resetToggle.getButtonText()) + 56;
    const auto toolsWidth = 58 + 110 + 20 + (drawMode ? 42 + 100 + 20 + 110 + 8 + 140 + 8 + 124 : 258);
    const auto twoRows = content.getWidth() < toolsWidth + 20 + resetWidth;
    extraControlsRow = twoRows ? 36 : 0;
    auto controlsRow = content.withHeight (34);
    controls.setBounds (controlsRow.withHeight (34 + extraControlsRow));
    lengthLabel.setBounds (controlsRow.removeFromLeft (58));
    lengthBox.setBounds (controlsRow.removeFromLeft (110).reduced (0, 3));
    controlsRow.removeFromLeft (20);
    if (drawMode)
    {
        gridLabel.setBounds (controlsRow.removeFromLeft (42));
        gridBox.setBounds (controlsRow.removeFromLeft (100).reduced (0, 3));
        controlsRow.removeFromLeft (20);
        waveButton.setBounds (controlsRow.removeFromLeft (110).reduced (0, 3));
        controlsRow.removeFromLeft (8);
        importButton.setBounds (controlsRow.removeFromLeft (140).reduced (0, 3));
        controlsRow.removeFromLeft (8);
        expandButton->setBounds (controlsRow.removeFromLeft (124).reduced (0, 3));
    }
    else
    {
        auto curveArea = controlsRow.removeFromLeft (258);
        curveLabel.setBounds (curveArea.removeFromLeft (52));
        curveSlider.setBounds (curveArea);
    }
    if (twoRows)
        resetToggle.setBounds (content.withTrimmedTop (36).withHeight (34).withWidth (resetWidth));
    else
        resetToggle.setBounds (controlsRow.withTrimmedLeft (20).withWidth (resetWidth));

    {
        auto hdr = section.headerArea().withSizeKeepingCentre (section.headerArea().getWidth(), 28);
        drawButton.setBounds (hdr.removeFromRight (80));
        shapesButton.setBounds (hdr.removeFromRight (80));
        hdr.removeFromRight (16);
        header.setBounds (hdr.removeFromRight (config.extraHeaderWidth));
    }

    auto tiles = content.withTrimmedTop (controlsHeight + extraControlsRow).expanded (3);
    const auto rows = juce::jmax (1, config.shapeRows);
    const auto columns = (config.numShapes + rows - 1) / rows;
    layoutGrid (shapeTiles, tiles, columns, rows > 1 ? 6 : 0);

    auto area = tiles;
    const auto tileW = area.getWidth() / juce::jmax (1, columns);
    if (drawTile != nullptr)
        drawTile->setBounds (area.removeFromRight (tileW).withSizeKeepingCentre (tileW, juce::jmin (area.getHeight(), 160)));
    area.removeFromRight (6);
    {
        // My drawings, Save / ..., Clear / Smooth.
        const auto numRows = 3;
        const auto gap = 8;
        const auto rowH = juce::jlimit (22, 30, (area.getHeight() - 6 - (numRows - 1) * gap) / numRows);
        auto buttons = area.removeFromRight (150).reduced (3, 0).withSizeKeepingCentre (144, numRows * rowH + (numRows - 1) * gap);
        libraryBox.setBounds (buttons.removeFromTop (rowH));
        buttons.removeFromTop (gap);
        auto saveRow = buttons.removeFromTop (rowH);
        moreButton.setBounds (saveRow.removeFromRight (36));
        saveRow.removeFromRight (6);
        saveButton.setBounds (saveRow);
        buttons.removeFromTop (gap);
        auto editRow = buttons.removeFromTop (rowH);
        clearButton.setBounds (editRow.removeFromLeft ((editRow.getWidth() - 6) / 2));
        editRow.removeFromLeft (6);
        smoothButton.setBounds (editRow);
    }
    area.removeFromRight (6);
    pad->setBounds (area.reduced (3));
}

void MovesPanel::updateDrawTile()
{
    if (drawTile == nullptr)
        return;

    curveOf (*drawTile, config.makeDrawn (pad->points, drawingName()));
    drawTile->subtitle = cues::formatBeats ((double) state[config.beatsId]) + (drawingEdited() ? "  -  edited" : "");
    drawTile->repaint();
}

std::unique_ptr<juce::Component> MovesPanel::makeWaveEditor (WaveSettings s, juce::Colour colour)
{
    return std::make_unique<WaveEditor> (s, colour, nullptr);
}

void MovesPanel::showWaveEditor()
{
    // Open: Wave... closes it again.
    if (waveBox != nullptr)
    {
        if (waveBox->isVisible())
        {
            waveBox->dismiss();
            waveBox = nullptr;
            return;
        }
        waveBox = nullptr;
    }

    ++waveSession;   // one undo step for everything changed while it's open
    juce::Component::SafePointer<MovesPanel> safe (this);
    auto editor = std::make_unique<WaveEditor> (wave, config.colour, [safe] (const WaveSettings& s)
    {
        if (safe != nullptr)
        {
            safe->wave = s;
            safe->applyWave();
        }
    });
    const auto target = waveButton.getScreenBounds();
    auto& box = juce::CallOutBox::launchAsynchronously (std::move (editor), target, nullptr);
    waveBox = &box;

    // Above the button (so the drawing below stays in view); without room there, beside the pad.
    const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (target);
    if (display == nullptr)
        return;
    const auto screen = display->userArea;
    const auto padArea = pad->getScreenBounds();
    const auto w = box.getWidth(), h = box.getHeight();
    if (target.getY() - screen.getY() >= h + 4)
        box.updatePosition (target, screen.withBottom (target.getY() + 2));
    else if (screen.getRight() - padArea.getRight() >= w + 4)
        box.updatePosition (padArea, screen.withLeft (padArea.getRight() - 2));
    else if (padArea.getX() - screen.getX() >= w + 4)
        box.updatePosition (padArea, screen.withRight (padArea.getX() + 2));
}

void MovesPanel::applyWave()
{
    pad->points = cues::whammy::waveDrawing ((cues::whammy::Wave) wave.type, wave.cycles, wave.phase, wave.shape, wave.low, wave.high,
                                             wave.grow, wave.speed);
    commitDrawing ("wave" + juce::String (waveSession));
}

void MovesPanel::chooseMidiFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a move from a MIDI file", juce::File(), "*.mid;*.midi");
    juce::Component::SafePointer<MovesPanel> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult().existsAsFile())
            safe->importMidi (fc.getResult());
    });
}

bool MovesPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("mid;midi");
}

void MovesPanel::paintOverChildren (juce::Graphics& g)
{
    if (! fileHover)
        return;
    // A MIDI file held over the card: a light veil with a +, like dropping a file anywhere else.
    const auto b = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawRoundedRectangle (b.reduced (1.0f), 10.0f, 2.0f);
    auto centre = b.withSizeKeepingCentre (b.getWidth(), 110.0f);
    const auto plus = centre.removeFromTop (64.0f).withSizeKeepingCentre (64.0f, 64.0f);
    g.fillEllipse (plus);
    g.setColour (background);
    g.fillRect (plus.withSizeKeepingCentre (30.0f, 5.0f));
    g.fillRect (plus.withSizeKeepingCentre (5.0f, 30.0f));
    g.setColour (juce::Colours::white);
    g.setFont (font (16.0f, true));
    g.drawText ("Drop to import this move", centre.withTrimmedTop (10.0f), juce::Justification::centredTop);
}

void MovesPanel::filesDropped (const juce::StringArray& files, int, int)
{
    fileHover = false;
    repaint();
    if (isInterestedInFileDrag (files))
        importMidi (juce::File (files[0]));
}

void MovesPanel::importMidi (const juce::File& file)
{
    juce::MidiFile midi;
    juce::FileInputStream in (file);
    if (! in.openedOk() || ! midi.readFrom (in))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import MIDI", "That isn't a MIDI file PedalCues can read.");
        return;
    }
    const auto move = cues::whammy::importMove (midi, config.controller ? config.controller() : -1,
                                                std::vector<double> (lengths.begin(), lengths.end()));
    if (move.error.isNotEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import MIDI", move.error);
        return;
    }

    // Into the pad, at the clip's length, then offered for My drawings under the file's name.
    state.setProperty (config.drawId, true, nullptr);
    state.setProperty (config.beatsId, move.beats, nullptr);
    pad->points = move.points;
    state.setProperty (config.drawingNameId, juce::String(), nullptr);
    commitDrawing();
    if (move.truncated)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Import MIDI",
                                                "The clip is longer than 8 bars: PedalCues kept its first 8 bars.");
    saveDrawingAs (file.getFileNameWithoutExtension());
}

void MovesPanel::commitDrawing (const juce::String& mergeKey)
{
    // New points (and an undo step) if the samples came from elsewhere (Clear, Smooth, Wave, Import, a saved drawing).
    pad->syncBreakpoints (mergeKey);
    pad->repaint();
    updateDrawTile();
    state.setProperty (config.drawingId, cues::whammy::encodeDrawing (pad->points), nullptr);
    if (large != nullptr)
        large->refresh();
}

//==============================================================================
juce::String MovesPanel::drawingName() const
{
    const auto name = state[config.drawingNameId].toString();
    return state::findDrawing (drawings, name).isValid() ? name : juce::String();
}

bool MovesPanel::drawingEdited() const
{
    const auto saved = state::findDrawing (drawings, drawingName());
    // Compare at today's resolution: drawings saved before v0.8.5 have 64 points.
    return saved.isValid() && cues::whammy::encodeDrawing (cues::whammy::decodeDrawing (saved[IDs::points].toString()))
                                  != cues::whammy::encodeDrawing (pad->points);
}

void MovesPanel::loadDrawing (int itemId)
{
    if (itemId == newDrawingId)
    {
        std::fill (pad->points.begin(), pad->points.end(), 0.0f);
        state.setProperty (config.drawingNameId, juce::String(), nullptr);
        commitDrawing();
        return;
    }

    const auto d = drawings.getChild (itemId - 1);
    if (! d.isValid())
        return;

    pad->points = cues::whammy::decodeDrawing (d[IDs::points].toString());
    state.setProperty (config.drawingNameId, d[IDs::name], nullptr);
    commitDrawing();
}

void MovesPanel::saveDrawing()
{
    if (drawingName().isEmpty())
    {
        saveDrawingAs ("My move");
        return;
    }

    state::saveDrawing (drawings, drawingName(), cues::whammy::encodeDrawing (pad->points));
    state::storeMyDrawings();
}

void MovesPanel::saveDrawingAs (const juce::String& suggestion)
{
    juce::Component::SafePointer<MovesPanel> safe (this);
    askText ("Save drawing as", suggestion, [safe] (const juce::String& name)
    {
        if (safe == nullptr || name.isEmpty())
            return;

        state::saveDrawing (safe->drawings, name, cues::whammy::encodeDrawing (safe->pad->points));
        state::storeMyDrawings();
        safe->state.setProperty (safe->config.drawingNameId, name, nullptr);
    });
}

void MovesPanel::showDrawingMenu()
{
    const auto name = drawingName();

    juce::PopupMenu m;
    m.addItem (1, "Save as new drawing");
    m.addItem (2, name.isNotEmpty() ? "Rename \"" + name + "\"" : juce::String ("Rename"), name.isNotEmpty());
    m.addItem (3, name.isNotEmpty() ? "Delete \"" + name + "\"" : juce::String ("Delete"), name.isNotEmpty());

    juce::Component::SafePointer<MovesPanel> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton), [safe, name] (int result)
    {
        if (safe == nullptr)
            return;

        if (result == 1)
        {
            safe->saveDrawingAs (name.isNotEmpty() ? name + " 2" : juce::String ("My move"));
        }
        else if (result == 2)
        {
            askText ("Rename drawing", name, [safe, name] (const juce::String& to)
            {
                if (safe != nullptr && state::renameDrawing (safe->drawings, name, to))
                {
                    state::storeMyDrawings();
                    safe->state.setProperty (safe->config.drawingNameId, to, nullptr);
                }
            });
        }
        else if (result == 3)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                              .withTitle ("Delete drawing")
                                              .withMessage ("Delete \"" + name + "\" from My drawings? Clips you already dragged "
                                                            "onto the timeline keep working.")
                                              .withButton ("Delete")
                                              .withButton ("Cancel"),
                                          [safe, name] (int button)
            {
                if (safe == nullptr || button != 1)
                    return;
                state::deleteDrawing (safe->drawings, name);
                state::storeMyDrawings();
                safe->state.setProperty (safe->config.drawingNameId, juce::String(), nullptr);
            });
        }
    });
}
} // namespace ui
