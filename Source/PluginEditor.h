#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

class PedalCuesEditor final : public juce::AudioProcessorEditor,
                              private juce::ValueTree::Listener,
                              private juce::AsyncUpdater,
                              private juce::Timer
{
public:
    explicit PedalCuesEditor (PedalCuesProcessor&);
    ~PedalCuesEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    struct Page : public juce::Component
    {
        virtual void refresh() = 0;
    };

private:
    void showPage (int index);

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { triggerAsyncUpdate(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { triggerAsyncUpdate(); }
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { triggerAsyncUpdate(); }
    void valueTreeRedirected (juce::ValueTree&) override { triggerAsyncUpdate(); }

    void handleAsyncUpdate() override;
    void timerCallback() override;

    PedalCuesProcessor& pedalProcessor;
    juce::ValueTree state;

    juce::TooltipWindow tooltips { this, 600 };
    juce::Label titleLabel, tempoLabel;
    juce::OwnedArray<juce::TextButton> tabButtons;
    std::vector<std::unique_ptr<Page>> pages;
    int currentPage = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalCuesEditor)
};
