#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "CueModel.h"

#include <functional>

class PedalCuesProcessor;

// A coloured block. Drag it onto the DAW timeline to drop a named MIDI clip;
// click the play corner to send it to the pedal immediately.
class Tile final : public juce::Component,
                   public juce::SettableTooltipClient
{
public:
    explicit Tile (PedalCuesProcessor&);

    juce::String title, subtitle;
    juce::Colour colour { juce::Colours::grey };
    bool highlighted = false;

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

private:
    juce::Rectangle<int> playArea() const;

    PedalCuesProcessor& processor;
    bool dragStarted = false;
    bool overPlay = false;
    juce::uint32 flashUntil = 0;
};
