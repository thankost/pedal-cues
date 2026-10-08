// Draw > Points (MovesPanel's DrawPad): breakpoints <-> the drawing's samples, the grid and snapping
// (included in CueModelTests.cpp).
{
    using namespace cues::whammy;
    const auto n = (size_t) drawPoints;

    // Two points render a straight ramp, and a ramp simplifies back to two points.
    const auto ramp = renderBreakpoints ({ { 0.0f, 0.0f }, { 1.0f, 1.0f } });
    CHECK (ramp.size() == n && ramp.front() == 0.0f && ramp.back() == 1.0f);
    bool linear = true;
    for (size_t i = 0; i < n; ++i)
        linear = linear && std::abs (ramp[i] - (float) i / (float) (n - 1)) < 1.0e-5f;
    CHECK (linear);
    const auto rampPoints = simplifyDrawing (ramp);
    CHECK (rampPoints.size() == 2 && rampPoints[0].time == 0.0f && rampPoints[1].time == 1.0f
           && rampPoints[1].value == 1.0f);

    // A flat line is two points; so is an empty pad.
    CHECK (simplifyDrawing (std::vector<float> (n, 0.0f)).size() == 2);

    // A triangle (peak exactly at sample 64) keeps its peak through the round trip, with three points.
    std::vector<float> tri (n);
    for (size_t i = 0; i < n; ++i)
        tri[i] = i <= 64 ? (float) i / 64.0f : (float) (n - 1 - i) / (float) (n - 1 - 64);
    const auto triPoints = simplifyDrawing (tri);
    CHECK (triPoints.size() == 3 && std::abs (triPoints[1].time - 64.0f / (float) (n - 1)) < 1.0e-6f
           && triPoints[1].value == 1.0f);
    const auto triBack = renderBreakpoints (triPoints);
    float worst = 0.0f;
    for (size_t i = 0; i < n; ++i)
        worst = juce::jmax (worst, std::abs (triBack[i] - tri[i]));
    CHECK (triBack[64] == 1.0f && worst < 1.0e-5f);

    // A sine: a handful of points per wave, never further than the tolerance from the samples.
    const auto sine = waveDrawing (Wave::sine, 2.0, 0.0, 0.0, 0.0, 1.0);
    const auto sinePoints = simplifyDrawing (sine, 0.01f);
    const auto sineBack = renderBreakpoints (sinePoints);
    worst = 0.0f;
    for (size_t i = 0; i < n; ++i)
        worst = juce::jmax (worst, std::abs (sineBack[i] - sine[i]));
    CHECK (sinePoints.size() >= 8 && sinePoints.size() <= 48 && worst <= 0.01f + 1.0e-6f);
    CHECK (sinePoints.front().time == 0.0f && sinePoints.back().time == 1.0f);

    // What the pad saves survives encode / decode unchanged, so a commit never re-simplifies the user's points.
    const std::vector<Breakpoint> mine { { 0.0f, 0.2f }, { 0.3f, 0.5f }, { 0.5f, 0.5f }, { 0.7f, 0.9f }, { 1.0f, 0.0f } };
    const auto rendered = renderBreakpoints (mine);
    CHECK (encodeDrawing (decodeDrawing (encodeDrawing (rendered))) == encodeDrawing (rendered));

    // The grid: 1/16 when its lines are at least 8 px apart, else 1/8, else 1/4. Snapping a 4-beat move to 1/16.
    CHECK (gridStepBeats (4.0, 600.0) == 0.25 && gridStepBeats (32.0, 600.0) == 0.5 && gridStepBeats (32.0, 200.0) == 1.0);
    CHECK (std::abs (snapTime (0.3f, 4.0, 0.25) - 5.0f / 16.0f) < 1.0e-6f);
    CHECK (snapTime (0.01f, 4.0, 0.25) == 0.0f && snapTime (0.99f, 4.0, 0.25) == 1.0f && snapTime (1.4f, 4.0, 0.25) == 1.0f);
    CHECK (std::abs (snapValue (0.4567f) - 0.46f) < 1.0e-6f && snapValue (-0.2f) == 0.0f && snapValue (1.3f) == 1.0f);

    // Hover labels: bar.beat, a sixteenth inside the beat, or off the grid.
    CHECK (positionLabel (0.0) == "1.1" && positionLabel (5.0) == "2.2" && positionLabel (2.5) == "1.3.3");
    CHECK (positionLabel (1.37) == "1.2 +0.37" && positionLabel (1.98) == "1.3 -0.02" && positionLabel (3.97) == "2.1 -0.03");

    // Segment shapes: square holds the left value, linear is linear, slow start/end is symmetric, fast start / end bend
    // the right way, bezier tension bulges up (above 0) or sags (below 0) whichever way the line runs, 0 = linear.
    {
        CHECK (segmentValue (Segment::square, 0.0f, 0.2f, 0.8f, 0.99f) == 0.2f && segmentValue (Segment::square, 0.0f, 0.2f, 0.8f, 1.0f) == 0.8f);
        CHECK (std::abs (segmentValue (Segment::linear, 0.0f, 0.0f, 1.0f, 0.3f) - 0.3f) < 1.0e-6f);
        const auto s1 = segmentValue (Segment::slowStartEnd, 0.0f, 0.0f, 1.0f, 0.2f);
        const auto s2 = segmentValue (Segment::slowStartEnd, 0.0f, 0.0f, 1.0f, 0.8f);
        CHECK (std::abs (s1 + s2 - 1.0f) < 1.0e-5f && s1 < 0.2f
               && std::abs (segmentValue (Segment::slowStartEnd, 0.0f, 0.0f, 1.0f, 0.5f) - 0.5f) < 1.0e-6f);
        CHECK (segmentValue (Segment::fastStart, 0.0f, 0.0f, 1.0f, 0.25f) > 0.25f && segmentValue (Segment::fastEnd, 0.0f, 0.0f, 1.0f, 0.25f) < 0.25f);
        CHECK (segmentValue (Segment::fastStart, 0.0f, 1.0f, 0.0f, 0.25f) < 0.75f);   // a fall that drops early
        CHECK (std::abs (segmentValue (Segment::bezier, 0.0f, 0.2f, 0.6f, 0.5f) - 0.4f) < 1.0e-6f);
        CHECK (segmentValue (Segment::bezier, 0.8f, 0.0f, 1.0f, 0.5f) > 0.5f && segmentValue (Segment::bezier, -0.8f, 0.0f, 1.0f, 0.5f) < 0.5f);
        CHECK (segmentValue (Segment::bezier, 0.8f, 1.0f, 0.0f, 0.5f) > 0.5f && segmentValue (Segment::bezier, -0.8f, 1.0f, 0.0f, 0.5f) < 0.5f);
        for (int k = 0; k < numSegments; ++k)   // every shape starts at a and ends at b
            CHECK (segmentValue ((Segment) k, 0.5f, 0.1f, 0.9f, 0.0f) == 0.1f
                   && std::abs (segmentValue ((Segment) k, 0.5f, 0.1f, 0.9f, 1.0f) - 0.9f) < 1.0e-6f);

        // The renderer honours them: a square first half holds 0.2 up to the middle point.
        std::vector<Breakpoint> stepped { { 0.0f, 0.2f, Segment::square }, { 0.5f, 0.8f }, { 1.0f, 0.8f } };
        const auto r = renderBreakpoints (stepped);
        CHECK (r[0] == 0.2f && r[63] == 0.2f && r[64] == 0.8f && r[127] == 0.8f);
        std::vector<Breakpoint> bent { { 0.0f, 0.0f, Segment::bezier, 1.0f }, { 1.0f, 1.0f } };
        CHECK (renderBreakpoints (bent)[32] > ramp[32] + 0.2f);
        CHECK (segmentName (Segment::slowStartEnd) == "Slow start/end");
    }

    // Moving a selection: order kept, the first / last only move up and down, it stops at an unselected neighbour.
    {
        std::vector<Breakpoint> p { { 0.0f, 0.1f }, { 0.25f, 0.3f }, { 0.5f, 0.5f }, { 0.75f, 0.7f }, { 1.0f, 0.9f } };
        p[1].selected = p[2].selected = true;
        auto m = moveSelection (p, 0.1f, 0.1f);
        CHECK (std::abs (m[1].time - 0.35f) < 1.0e-6f && std::abs (m[2].time - 0.6f) < 1.0e-6f
               && std::abs (m[2].value - 0.6f) < 1.0e-6f && m[3].time == 0.75f && m[3].value == 0.7f);
        m = moveSelection (p, 0.6f, 0.0f);                          // stops one sample before the point at 0.75
        CHECK (std::abs (m[2].time - (0.75f - minPointGap)) < 1.0e-6f && m[1].time < m[2].time);
        m = moveSelection (p, -0.6f, 0.0f);                         // and one after the first point
        CHECK (std::abs (m[1].time - minPointGap) < 1.0e-6f);
        p[0].selected = p[4].selected = true;                       // endpoints: value only
        m = moveSelection (p, 0.1f, 0.5f);
        CHECK (m[0].time == 0.0f && m[4].time == 1.0f && m[0].value == 0.6f && m[4].value == 1.0f);
        bool sorted = true;
        for (size_t i = 1; i < m.size(); ++i)
            sorted = sorted && m[i].time > m[i - 1].time;
        CHECK (sorted);

        // Compress / expand around the middle of the selected values, tilt, invert, delete.
        std::vector<Breakpoint> q { { 0.0f, 0.2f }, { 0.5f, 0.6f }, { 1.0f, 0.4f } };
        for (auto& b : q) b.selected = true;
        auto c = scaleSelection (q, 0.5f);                          // range 0.2..0.6, centre 0.4
        CHECK (std::abs (c[0].value - 0.3f) < 1.0e-6f && std::abs (c[1].value - 0.5f) < 1.0e-6f && std::abs (c[2].value - 0.4f) < 1.0e-6f);
        c = scaleSelection (q, 3.0f);
        CHECK (c[0].value == 0.0f && std::abs (c[1].value - 1.0f) < 1.0e-6f);
        auto t = tiltSelection (q, 0.1f, -0.1f);                     // left up, right down, the middle stays
        CHECK (std::abs (t[0].value - 0.3f) < 1.0e-6f && std::abs (t[1].value - 0.6f) < 1.0e-6f && std::abs (t[2].value - 0.3f) < 1.0e-6f);
        invertSelection (q);
        CHECK (std::abs (q[0].value - 0.8f) < 1.0e-6f && std::abs (q[1].value - 0.4f) < 1.0e-6f);
        CHECK (deleteSelection (q) == 1 && q.size() == 2 && q[0].time == 0.0f && q[1].time == 1.0f);
        CHECK (deleteSelection (q) == 0 && q.size() == 2);
        setSelectionShape (q, Segment::square);
        CHECK (q[0].shape == Segment::square && numSelected (q) == 2);
    }

    // Freehand inside Points: the stroke replaces the points under it (simplified), the rest keep their place and shape.
    {
        std::vector<Breakpoint> p { { 0.0f, 0.0f, Segment::fastEnd }, { 0.2f, 0.5f }, { 0.4f, 0.9f }, { 0.6f, 0.1f }, { 1.0f, 1.0f } };
        std::vector<float> stroke (n, 0.0f);
        for (int i = 38; i <= 90; ++i)
            stroke[(size_t) i] = 0.3f;                              // a flat line at 0.3 across 0.3..0.71
        const auto m = mergeStroke (p, stroke, 38, 90);
        CHECK (m.size() == 5 && m[0].shape == Segment::fastEnd && m[1].time == 0.2f
               && std::abs (m[2].time - 38.0f / 127.0f) < 1.0e-6f && m[2].value == 0.3f
               && std::abs (m[3].time - 90.0f / 127.0f) < 1.0e-6f && m.back().time == 1.0f);
        const auto all = mergeStroke (p, ramp, 0, (int) n - 1);    // a stroke across everything: just the ramp
        CHECK (all.size() == 2 && all[0].time == 0.0f && all[1].time == 1.0f);
    }

    // Undo / redo: states after each edit, redo dropped by a new edit, merged steps (a dragged Wave slider), a limit.
    {
        DrawHistory h (3);
        const std::vector<Breakpoint> a { { 0.0f, 0.0f }, { 1.0f, 0.0f } }, b { { 0.0f, 0.5f }, { 1.0f, 0.0f } },
                                      c { { 0.0f, 0.5f }, { 1.0f, 1.0f } }, d { { 0.0f, 1.0f }, { 1.0f, 1.0f } };
        h.record (a);
        CHECK (! h.canUndo() && ! h.canRedo());
        h.record (b);
        h.record (b);                                               // nothing changed: no step
        CHECK (h.size() == 2 && h.canUndo());
        CHECK (samePoints (h.undo(), a) && h.canRedo() && ! h.canUndo());
        CHECK (samePoints (h.redo(), b) && ! h.canRedo());
        h.undo();
        h.record (c);                                               // a new edit after an undo drops the redo
        CHECK (! h.canRedo() && h.size() == 2 && samePoints (h.undo(), a));
        h.redo();
        h.record (b, "wave");
        h.record (d, "wave");                                       // merged into one step
        CHECK (h.size() == 3 && samePoints (h.undo(), c));
        h.redo();
        h.record (a);
        h.record (b);
        CHECK (h.size() == 4);                                      // 3 undo steps at most
        int steps = 0;
        while (h.canUndo()) { h.undo(); ++steps; }
        CHECK (steps == 3);
    }

    // Set value...: positions typed as bar.beat(.sixteenth) or +beats.
    CHECK (parsePosition ("1.1") == 0.0 && parsePosition ("2.3") == 6.0 && parsePosition ("1.2.3") == 1.5
           && parsePosition ("+2.5") == 2.5 && parsePosition ("3") == 8.0);
    CHECK (parsePosition ("") < 0.0 && parsePosition ("1.5") < 0.0 && parsePosition ("abc") < 0.0 && parsePosition ("0.1") < 0.0);
}
