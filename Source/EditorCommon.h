#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "Tile.h"
#include "Theme.h"

#include <functional>
#include <memory>

namespace ui
{
// Set (per computer) once the user confirms the MIDI channels in MIDI Setup; until then the pedal pages show a reminder.
inline const juce::String channelsConfirmedFlag { "channelsConfirmed" };

// Base for the three tab pages.
struct Page : public juce::Component
{
    virtual void refresh() = 0;
};

std::unique_ptr<Page> makeQcPage       (PedalCuesProcessor&);
std::unique_ptr<Page> makeKemperPage   (PedalCuesProcessor&);
std::unique_ptr<Page> makeAmpPage      (PedalCuesProcessor&);   // first tab: the Quad Cortex or Kemper page

juce::String ampUnitName (int unit);   // "Quad Cortex", "Kemper Profiler", "Kemper Player"
std::unique_ptr<Page> makeQcExpression (PedalCuesProcessor&);   // Quad Cortex page > Expression
std::unique_ptr<Page> makeWhammyPage   (PedalCuesProcessor&);
std::unique_ptr<Page> makeSettingsPage (PedalCuesProcessor&);
void showWiringGuide (int ampUnit);   // Help > Wiring guide, for the Quad Cortex or a Kemper
void showQcSyncDialog (juce::ValueTree state);   // Quad Cortex page > Sync from QC (USB)
std::unique_ptr<juce::Component> makeWiringGuide (int ampUnit);

inline const juce::String repoUrl { "https://github.com/thankost/pedal-cues" };
inline const juce::String author { "Thanasis Kostopoulos" };
inline const juce::String guideUrl { "https://thankost.github.io/pedal-cues/guide.html" };
inline const juce::String changelogUrl { "https://thankost.github.io/pedal-cues/changelog.html" };
inline const juce::String helpUrl { "https://thankost.github.io/pedal-cues/help.html" };
inline const juce::String coffeeUrl { "https://buymeacoffee.com/athkost" };
inline const juce::String paypalUrl { "https://paypal.me/athkost" };
inline const juce::String revolutUrl { "https://revolut.me/athkost" };

// The "Report a problem" form on GitHub, with this version, the computer and the host filled in.
inline juce::URL problemReportUrl()
{
    const auto host = PedalCuesProcessor::isStandalone() ? juce::String ("none (standalone app)")
                                                          : juce::String (juce::PluginHostType().getHostDescription());
    return juce::URL (repoUrl + "/issues/new")
        .withParameter ("template", "problem.yml")
        .withParameter ("version", JucePlugin_VersionString)
        .withParameter ("os", juce::SystemStats::getOperatingSystemName())
        .withParameter ("daw", host);
}

//==============================================================================
// A rounded card with a small caps title and an optional hint, drawn behind the
// components a page lays out inside it. Its component ID doubles as a tour target.
class Section final : public juce::Component
{
public:
    Section (const juce::String& id, const juce::String& title, const juce::String& hint,
             juce::Colour accentColour = theme::accent);

    void paint (juce::Graphics&) override;

    // In the parent's coordinate space.
    juce::Rectangle<int> headerArea() const;   // right part of the title row, for toggles/buttons
    juce::Rectangle<int> contentArea() const;

    static constexpr int headerHeight = 36;
    static constexpr int padding = 10;

    juce::String title, hint;
    juce::Colour accentColour;
    juce::Colour fill { theme::surface.withAlpha (0.55f) };
};

//==============================================================================
juce::ValueTree nthOfType (const juce::ValueTree& parent, const juce::Identifier& type, int n);
juce::Colour paletteColour (int index);
juce::PopupMenu colourMenu (juce::Colour current, int baseId);

void layoutGrid (juce::OwnedArray<Tile>& tiles, juce::Rectangle<int> area, int columns, int gap = 6,
                 int first = 0, int count = -1);

void askText (const juce::String& title, const juce::String& current, std::function<void (const juce::String&)> done);
void renameNode (juce::ValueTree node, const juce::String& title);
void editPresetDialog (juce::ValueTree preset);

void styleCaption (juce::Label&, const juce::String& text);
} // namespace ui
