#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "EditorCommon.h"
#include "PluginProcessor.h"
#include "Theme.h"
#include "Tour.h"

class PedalCuesEditor final : public juce::AudioProcessorEditor,
                              public ui::TourHost,
                              private juce::ValueTree::Listener,
                              private juce::AsyncUpdater,
                              private juce::Timer
{
public:
    explicit PedalCuesEditor (PedalCuesProcessor&, bool allowFirstRunTour = true);
    ~PedalCuesEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    // TourHost
    void showPage (int index) override;
    juce::Rectangle<int> targetBounds (const juce::StringArray& componentIds) override;
    void closeTour (bool finished) override;

    void startTour (int step = 0);
    bool isTourVisible() const { return tour != nullptr; }
    void refreshNow() { cancelPendingUpdate(); handleAsyncUpdate(); }

    static constexpr const char* tourDoneFlag = "tourDone";

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { triggerAsyncUpdate(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { triggerAsyncUpdate(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { triggerAsyncUpdate(); }
    void valueTreeRedirected (juce::ValueTree&) override { triggerAsyncUpdate(); }

    void handleAsyncUpdate() override;
    void timerCallback() override;
    void showHelpMenu();

    // Must outlive every child component, so it is declared first.
    juce::SharedResourcePointer<theme::LookAndFeel> lookAndFeel;

    PedalCuesProcessor& pedalProcessor;
    juce::ValueTree state;

    juce::TooltipWindow tooltips { this, 600 };

    struct TabBar final : public juce::Component
    {
        void paint (juce::Graphics&) override;
    } tabBar;

    juce::OwnedArray<juce::TextButton> tabButtons;
    juce::TextButton helpButton { "?" };
    std::vector<std::unique_ptr<ui::Page>> pages;
    std::unique_ptr<ui::TourOverlay> tour;
    int currentPage = 0;
    double shownBpm = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalCuesEditor)
};
