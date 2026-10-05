#pragma once

#include <optional>

#include <juce_audio_processors/juce_audio_processors.h>

#include "EditorCommon.h"
#include "PluginProcessor.h"
#include "Theme.h"
#include "Tour.h"
#include "Update.h"

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
    void layoutContent();
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;

    // TourHost
    void showPage (int index) override;
    void showQcExpression (bool show) override;
    void showWhammyDt (bool show) override;
    void showQuadCortex (bool show) override;
    void showWhammy (bool show) override;
    juce::Colour tabColour (int tab) const;
    juce::Rectangle<int> targetBounds (const juce::StringArray& componentIds) override;
    void closeTour (bool finished) override;

    void startTour (int step = 0);
    void showSupportDialog();
    void showUpdateDialog (const update::Info&);

    // Setup = your names plus the MIDI settings. Saved in every project automatically; these are the explicit actions.
    void saveDefaultSetup();
    void loadDefaultSetup();
    void exportSetup();
    void importSetup();

    // Opens the ☰ menu under target. extraItemName/extra add an app-only item (standalone: audio/MIDI settings).
    void showHelpMenu (juce::Component* target, const juce::String& extraItemName = {}, std::function<void()> extra = {});

    // Standalone: the app's menu bar has Options/Help, so the header's ? button is hidden.
    void setHelpInMenuBar (bool);
    void showAboutDialog();
    void checkForUpdates (bool force);

    // Version and update status: under the title in the header, or in the standalone window's title bar.
    // The "Update available" pill opens the update window; the small refresh button checks again.
    struct UpdateBadge final : public juce::Component, public juce::SettableTooltipClient
    {
        UpdateBadge();

        update::Info info;
        std::function<void()> onOpen, onRefresh;

        void setInfo (const update::Info&);
        void paint (juce::Graphics&) override;
        void resized() override { layoutRefresh(); }
        void mouseUp (const juce::MouseEvent&) override;

    private:
        juce::String label() const;
        juce::Rectangle<float> textArea() const;
        void layoutRefresh();

        struct RefreshButton final : public juce::Button
        {
            RefreshButton() : juce::Button ("Check for updates") {}
            void paintButton (juce::Graphics&, bool over, bool down) override;
        } refresh;
    };

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

    // The ▾ on the first tab: picks the amp unit (Quad Cortex, Kemper Profiler, Kemper Player).
    struct UnitMenuButton final : public juce::Button
    {
        UnitMenuButton() : juce::Button ("unit") {}
        juce::Colour arrowColour { juce::Colours::black };
        void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    } unitMenuButton, pedalMenuButton;

    // Until the MIDI channels are confirmed in MIDI Setup: a reminder above the pedal pages (clips keep the channel
    // they were dragged with, so it matters before building songs).
    struct ChannelBanner final : public juce::Component
    {
        ChannelBanner();
        void paint (juce::Graphics&) override;
        void resized() override;
        juce::TextButton openButton { "Open MIDI Setup" };
    } channelBanner;
    bool channelsConfirmed = true;
    static constexpr int channelBannerHeight = 44;
    void showUnitMenu();
    void showPedalMenu();   // the second tab's arrow: the Whammy or a custom MIDI device
    // The ☰ menu: tour, guide, setup (save/load/export/import), updates, about, support.
    struct MenuButton final : public juce::Button
    {
        MenuButton() : juce::Button ("Menu") {}
        void paintButton (juce::Graphics&, bool over, bool down) override;
    } helpButton;
    std::unique_ptr<juce::FileChooser> chooser;

    UpdateBadge updateBadge;
    std::vector<std::unique_ptr<ui::Page>> pages;
    std::unique_ptr<ui::TourOverlay> tour;
    std::optional<std::pair<juce::var, juce::var>> tourSavedWhammy;
    std::optional<juce::var> tourSavedPedal;  // the pedals tab's choice, while the tour shows the Whammy page
    std::optional<juce::var> tourSavedUnit;   // a Kemper player's unit, while the tour shows the Quad Cortex page   // model and Drop Tune view, while the tour shows the DT
    int currentPage = 0;
    double shownBpm = 0.0;
    juce::Rectangle<float> tempoPill() const;
    void showTempoEditor();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalCuesEditor)
};
