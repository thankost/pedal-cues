#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "CueModel.h"

#include <functional>
#include <vector>

class PedalCuesProcessor;

// A draggable cue. Drag it onto the DAW timeline to drop a named MIDI clip;
// click the round play button to send it to the pedal immediately.
class Tile final : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    enum class Look { row, screen, footswitch, stomp, utility, whammyMode, sweep };

    Tile (PedalCuesProcessor&, Look);

    juce::String title, subtitle, badge;
    juce::Colour colour { juce::Colours::grey };
    bool highlighted = false;
    bool active = true;
    std::vector<juce::Point<float>> curve;   // sweep: normalised (x, y) CC points
    std::vector<juce::Colour> chips;         // screen: scene colours
    juce::String screenHeading { "LOADED PRESET" }, chipsHeading { "SCENES" };   // screen labels (Kemper: performance, slots)
    juce::String screenHint { "Drag this onto the timeline to load the preset" };
    bool numberedChips = false;              // screen: chips labelled 1, 2, 3 instead of A, B, C
    int chipLetters = 8;                     // screen: letters before they repeat (QC Mini: A-D on Page I, A-D on Page II)

    std::function<cues::Cue()> makeCue;
    std::function<void()> onClick;
    std::function<void()> onDoubleClick;
    std::function<void()> onContextMenu;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    void sendNow();

    // The tile last dragged out as a MIDI file: when that file lands in the Song Builder window, it takes this cue (with
    // its flags and the tile's name and colour) instead of reading the file back.
    struct LastDrag { juce::String path; cues::Cue cue; juce::String title; juce::Colour colour; };
    static inline LastDrag lastDrag;

private:
    juce::Rectangle<int> playArea() const;

    void paintRow (juce::Graphics&, juce::Rectangle<float>, bool hover);
    void paintScreen (juce::Graphics&, juce::Rectangle<float>, bool hover);
    void paintFootswitch (juce::Graphics&, juce::Rectangle<float>, bool hover);
    void paintStomp (juce::Graphics&, juce::Rectangle<float>, bool hover);
    void paintUtility (juce::Graphics&, juce::Rectangle<float>, bool hover);
    void paintWhammyMode (juce::Graphics&, juce::Rectangle<float>, bool hover);
    void paintSweep (juce::Graphics&, juce::Rectangle<float>, bool hover);
    void paintPlay (juce::Graphics&, bool hover);

    const Look look;
    PedalCuesProcessor& processor;
    bool dragStarted = false;
    bool overPlay = false;
    juce::uint32 flashUntil = 0;
};
