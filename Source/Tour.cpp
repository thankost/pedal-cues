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
          "your timeline. This one-minute tour shows you around. Use the arrow keys or the buttons below." },
        { 0, { "qc.presetList" }, "1. Add your presets",
          "Add each Quad Cortex preset (a whole rig, often one per song) with '+ Preset'. Enter its name, setlist, bank and slot "
          "exactly as on the pedal. Click a preset to open it, double-click to edit, right-click to recolour or reorder." },
        { 0, { "qc.screen" }, "2. Load a preset",
          "This screen shows the preset you opened. Drag it onto the timeline where the song starts: the clip "
          "loads that preset at that bar. The chips on the right preview its scene colours." },
        { 0, { "qc.scenes" }, "3. Drop scenes on song sections",
          "The eight scenes are laid out like the QC display (A-D on top, E-H below). Drag a scene to where "
          "the verse, chorus or solo starts. Double-click to rename, right-click for a colour." },
        { 0, { "qc.target" }, "Which preset do scenes act on?",
          "Load 1A first (the default): scene and stomp tiles load their own preset, then switch, so they work whatever "
          "preset the QC is on. Current QC preset: they only switch the scene or footswitch on the preset already loaded "
          "(no reload, no audio gap). Tile subtitles show which one is active." },
        { 0, { "qc.scenes" }, "Test any tile live",
          "Every tile has a round play button in its corner. Click it and the MIDI goes to the pedal immediately, "
          "so you can check a cue without pressing play in Reaper." },
        { 0, { "qc.stomps", "qc.utils" }, "4. Stomps, tuner and gig view",
          "Switch single footswitches on or off, open the tuner for a guitar change, or flip the QC between "
          "preset, scene and stomp mode. The switch in the Stomps header picks ON or OFF tiles." },
        { 1, { "wh.modes" }, "5. Whammy V modes",
          "The Whammy tab works the same way, laid out like the pedal: Whammy modes on top, Harmony below, Detune on its own row. Colours: "
          "red Whammy, blue Detune, green Harmony. A dark LED means the mode loads bypassed." },
        { 1, { "wh.options" }, "Chords, bypass, heel first",
          "Chords uses the polyphonic program range. Load bypassed selects a mode without engaging it. Heel first "
          "parks the treadle at heel before switching so nothing jumps in pitch." },
        { 1, { "wh.sweepControls", "wh.sweeps" }, "6. Automate the treadle",
          "Pick a length and curve, then drag a move. It becomes CC#11 automation that follows your project tempo: "
          "ramps, dives, trills, or a bend that lands exactly on the next bar. Or click Draw and sketch your own move." },
        { 2, { "set.midi" }, "7. Match the MIDI channels",
          "Set the channel of each pedal here, and give the QC and the Whammy different channels." },
        { 2, { "set.setup" }, "8. Hook it up once",
          "Pick how the Whammy is connected: through the QC's MIDI Out, or from your audio interface's MIDI Out "
          "(one cue track per pedal). Follow the steps once, and the arrangement plays your whole show." },
        { 2, { "hdr.help" }, "You are ready",
          "Reopen this tour or the illustrated user guide at any time from the ? button. Have a great show!" },
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
