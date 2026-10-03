#include "Tour.h"
#include "Theme.h"

using namespace theme;

namespace ui
{
const std::vector<TourStep>& tourSteps()
{
    static const std::vector<TourStep> steps {
        { 0, {}, "Welcome to PedalCues",
          "Pedal changes become drag and drop: every tile you see turns into a named MIDI clip when you drop it on "
          "your timeline. This one-minute tour shows you around. Use the arrow keys or the buttons below.", false, false, true },
        { 0, { "hdr.tabs" }, "Quad Cortex or Kemper?",
          "Click the arrow on the first tab to pick your amp modeller: Quad Cortex, Kemper Profiler (Head, Rack, Stage...) or "
          "Kemper Player, or any other MIDI device (beta: synths, effects, loopers...), where you make your own tiles. The tab, its page and the MIDI it sends "
          "follow your choice (it's also in MIDI Setup). The next steps show the Quad Cortex page; the Kemper page works the same way, "
          "with performances, slots and effects.", false, false, true },
        { 0, { "qc.presetList" }, "1. Add your presets",
          "Add each Quad Cortex preset (a whole rig, often one per song) with '+ Preset', or read them all from the pedal with "
          "'Sync from QC (USB)' (quit Cortex Control first). Click a preset to open it, double-click to edit, right-click to recolour or reorder." },
        { 0, { "qc.screen" }, "2. Load a preset",
          "This screen shows the preset you opened. Drag it onto the timeline where the song starts: the clip "
          "loads that preset at that bar. The chips on the right preview its scene colours." },
        { 0, { "qc.scenes" }, "3. Drop scenes on song sections",
          "The eight scenes are laid out like the QC display (A-D on top, E-H below). Drag a scene to where "
          "the verse, chorus or solo starts. Double-click to rename, right-click for a colour." },
        { 0, { "qc.target" }, "Which preset do scenes act on?",
          "Load 1A first on (the default): scene and stomp tiles load their own preset, then switch, so they work whatever "
          "preset the QC is on. Off: they only switch the scene or footswitch on the preset the QC has loaded "
          "(no reload, no audio gap). The header and tile subtitles show which one is active." },
        { 0, { "qc.scenes" }, "Test any tile live",
          "Every tile has a round play button in its corner. Click it and the MIDI goes to the pedal immediately, "
          "so you can check a cue without pressing play in your DAW." },
        { 0, { "qc.stomps", "qc.utils" }, "4. Stomps, tuner and gig view",
          "Switch single footswitches on or off, open the tuner for a guitar change, open or close Gig View, or flip the QC "
          "between preset, scene and stomp mode. The switch in the Stomps header picks ON or OFF tiles." },
        { 0, { "qc.view", "qc.exp.sweeps" }, "5. Automate the QC's expression",
          "Switch to Expression to move anything you assign to Expression 1 or 2 on the QC: wah, volume, a delay mix, drive. "
          "Drag a swell, a fade or a wah rhythm (or draw your own) to where it should happen.", true },
        { 1, { "wh.faceplate", "wh.modes" }, "6. Whammy modes",
          "The Whammy tab works the same way, laid out like the pedal: Whammy modes on top, Harmony below, Detune on its own row. Colours: "
          "red Whammy, blue Detune, green Harmony. A dark LED means the mode loads bypassed. The switch at the top picks your pedal: "
          "Whammy V or Whammy DT." },
        { 1, { "wh.faceplate", "wh.modes" }, "Got a Whammy DT?",
          "Click Whammy DT at the top. The Modes card then gets a Drop Tune switch with Shift Up and Shift Down tiles: "
          "drop or raise your tuning anywhere in a song, for example -2 for D standard. The Whammy modes and treadle moves "
          "work the same on both pedals.", false, true },
        { 1, { "wh.options" }, "Chords, bypass, heel first",
          "Chords (Whammy V only) uses the polyphonic program range. Load bypassed selects a mode without engaging it. Heel first "
          "parks the treadle at heel before switching so nothing jumps in pitch." },
        { 1, { "wh.sweepControls", "wh.sweeps" }, "7. Automate the treadle",
          "Pick a length and curve, then drag a move. It becomes CC#11 automation that follows your project tempo: "
          "ramps, dives, trills, or a bend that lands exactly on the next bar. Or click Draw, sketch your own move and save it in My drawings." },
        { 2, { "set.pedals" }, "8. MIDI Setup: your pedals",
          "Pick your amp modeller, then set each pedal's MIDI channel to match the pedal itself, and give the amp modeller and the Whammy different channels. "
          "Every cue is sent on these channels, and each clip keeps the channel it was dragged with, so set them once, "
          "before building songs, then click 'My pedals use these channels'." },
        { 2, { "set.tracks" }, "9. DAW tracks",
          "Pick your wiring at the top (a daisy chain through the QC or Kemper, or separate outputs), then make the two cue tracks, "
          "QC (or Kemper) Cues and Whammy Cues, as shown. Not sure how to cable the pedals? Click 'How should I wire my pedals?'." },
        { 2, { "hdr.help" }, "You are ready",
          "The menu button (top right) reopens this tour and the user guide, shows what's new, lets you report a problem, "
          "and saves your setup as the default for new projects. In the standalone app these are in the File and Help menus. Have a great show!" },
    };
    return steps;
}

//==============================================================================
TourOverlay::TourOverlay (TourHost& h, int startStep) : host (h)
{
    setWantsKeyboardFocus (true);
    setAlwaysOnTop (true);

    nextButton.setColour (juce::TextButton::buttonColourId, accent);
    nextButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    backButton.setColour (juce::TextButton::buttonColourId, raised);
    skipButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    skipButton.setColour (juce::TextButton::textColourOffId, dim);

    backButton.onClick = [this] { setStep (step - 1); };
    nextButton.onClick = [this]
    {
        if (step + 1 >= (int) tourSteps().size())
            host.closeTour (true);
        else
            setStep (step + 1);
    };
    skipButton.onClick = [this] { host.closeTour (false); };

    for (auto* b : { &backButton, &nextButton, &skipButton })
        addAndMakeVisible (b);

    step = juce::jlimit (0, (int) tourSteps().size() - 1, startStep);
}

void TourOverlay::setStep (int index)
{
    step = juce::jlimit (0, (int) tourSteps().size() - 1, index);
    const auto& s = tourSteps()[(size_t) step];
    host.showPage (s.page);
    if (s.page == 0)
        host.showQcExpression (s.qcExpression);
    host.showWhammyDt (s.whammyDt);
    host.showQuadCortex (s.page == 0 && ! s.anyUnit);
    layoutCard();
    repaint();
}

void TourOverlay::resized()
{
    layoutCard();
}

void TourOverlay::layoutCard()
{
    const auto& s = tourSteps()[(size_t) step];
    const auto area = getLocalBounds();

    hole = s.targets.isEmpty() ? juce::Rectangle<int>() : host.targetBounds (s.targets);
    if (! hole.isEmpty())
        hole = hole.expanded (6).getIntersection (area.reduced (2));

    const int w = juce::jmin (420, area.getWidth() - 40);
    const int h = 196;
    constexpr int gap = 16;

    if (hole.isEmpty())
        card = juce::Rectangle<int> (w, h).withCentre (area.getCentre());
    else if (area.getBottom() - hole.getBottom() >= h + gap + 10)
        card = { juce::jlimit (20, area.getRight() - w - 20, hole.getCentreX() - w / 2), hole.getBottom() + gap, w, h };
    else if (hole.getY() - area.getY() >= h + gap + 10)
        card = { juce::jlimit (20, area.getRight() - w - 20, hole.getCentreX() - w / 2), hole.getY() - gap - h, w, h };
    else if (area.getRight() - hole.getRight() >= w + gap + 10)
        card = { hole.getRight() + gap, juce::jlimit (20, area.getBottom() - h - 20, hole.getCentreY() - h / 2), w, h };
    else if (hole.getX() - area.getX() >= w + gap + 10)
        card = { hole.getX() - gap - w, juce::jlimit (20, area.getBottom() - h - 20, hole.getCentreY() - h / 2), w, h };
    else
        card = juce::Rectangle<int> (w, h).withCentre (hole.getCentre());

    auto buttons = card.reduced (20, 16).removeFromBottom (32);
    nextButton.setBounds (buttons.removeFromRight (step + 1 >= (int) tourSteps().size() ? 110 : 90));
    buttons.removeFromRight (8);
    backButton.setBounds (buttons.removeFromRight (80));
    backButton.setVisible (step > 0);
    skipButton.setBounds (buttons.removeFromLeft (90));
    skipButton.setVisible (step + 1 < (int) tourSteps().size());

    nextButton.setButtonText (step == 0 ? "Start" : (step + 1 >= (int) tourSteps().size() ? "Let's go" : "Next"));
}

void TourOverlay::paint (juce::Graphics& g)
{
    const auto& s = tourSteps()[(size_t) step];

    juce::Path shade;
    shade.addRectangle (getLocalBounds().toFloat());
    if (! hole.isEmpty())
    {
        shade.addRoundedRectangle (hole.toFloat(), 14.0f);
        shade.setUsingNonZeroWinding (false);
    }
    g.setColour (juce::Colours::black.withAlpha (0.68f));
    g.fillPath (shade);

    if (! hole.isEmpty())
    {
        g.setColour (accent.withAlpha (0.25f));
        g.drawRoundedRectangle (hole.toFloat().expanded (3.0f), 16.0f, 4.0f);
        g.setColour (accent);
        g.drawRoundedRectangle (hole.toFloat(), 14.0f, 2.0f);
    }

    // Card with drop shadow.
    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 24, { 0, 8 }).drawForRectangle (g, card);
    const auto c = card.toFloat();
    g.setColour (surfaceHi);
    g.fillRoundedRectangle (c, 16.0f);
    g.setColour (accent.withAlpha (0.5f));
    g.drawRoundedRectangle (c.reduced (0.5f), 16.0f, 1.0f);

    auto r = card.reduced (22, 18);
    auto top = r.removeFromTop (18);

    g.setColour (accent);
    g.setFont (font (11.5f, true));
    g.drawText ("QUICK TOUR", top, juce::Justification::centredLeft);

    // Progress dots.
    const auto n = (int) tourSteps().size();
    const float dot = 6.0f, spacing = 11.0f;
    auto x = (float) top.getRight() - (float) n * spacing;
    for (int i = 0; i < n; ++i, x += spacing)
    {
        g.setColour (i == step ? accent : (i < step ? accent.withAlpha (0.45f) : raised.brighter (0.2f)));
        g.fillEllipse (x, (float) top.getCentreY() - dot * 0.5f, dot, dot);
    }

    r.removeFromTop (6);
    g.setColour (text);
    g.setFont (font (19.0f, true));
    g.drawText (s.title, r.removeFromTop (28), juce::Justification::centredLeft);

    r.removeFromBottom (40);
    g.setColour (text.withAlpha (0.8f));
    g.setFont (font (13.5f));
    g.drawFittedText (s.body, r, juce::Justification::topLeft, 6, 1.0f);
}

bool TourOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        host.closeTour (false);
        return true;
    }
    if (key == juce::KeyPress::rightKey || key == juce::KeyPress::returnKey || key == juce::KeyPress::spaceKey)
    {
        nextButton.triggerClick();
        return true;
    }
    if (key == juce::KeyPress::leftKey)
    {
        if (step > 0)
            setStep (step - 1);
        return true;
    }
    return false;
}
} // namespace ui
