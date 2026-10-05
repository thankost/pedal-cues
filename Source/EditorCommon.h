#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "Tile.h"
#include "Theme.h"
#include "Fuzzy.h"

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

juce::String ampUnitName (int unit);   // "Quad Cortex", "Kemper Profiler", "Kemper Player", "Quad Cortex Mini"

// The amp unit on the first tab, for labels and the wiring guide: a Quad Cortex, a Kemper or a custom unit (beta).
struct AmpInfo
{
    enum class Kind { quadCortex, kemper, custom, modeller };   // modeller: a Fractal / Line 6 page (Modellers.h)
    Kind kind = Kind::quadCortex;
    juce::String name { "Quad Cortex" };   // the tab: "Quad Cortex", "Kemper Profiler", "Axe-Fx II"
    juce::String box { "Quad Cortex" };    // a wiring box: "Quad Cortex", "Kemper", "Axe-Fx II"
    juce::String shortName { "QC" };       // in sentences and track names: "QC", "Kemper", "Axe-Fx II"
    juce::Colour colour { theme::qcBlue };
    // Connection facts for the wiring guide.
    bool hasDin = true;                    // a MIDI In jack, 5-pin or TRS (POD Go: USB only)
    bool hasThru = true;                   // a MIDI Out / Thru to pass MIDI on (Nano Cortex: none)
    bool canChain() const { return hasDin && hasThru; }
    juce::String midiIn { "5-pin MIDI In" }; // QC Mini, Flex Prime, MX5: "TRS MIDI In"
    bool usbMidi = true;                   // MIDI over USB from the computer (HeadRush: not in its manuals)
    int usbToThru = 0;                     // 0 never (QC, Kemper), 1 yes, 2 only with a setting, 3 not documented
    juce::String usbThruSetting;
    juce::String channelHint;
    bool isKemper() const { return kind == Kind::kemper; }
    bool isModeller() const { return kind == Kind::modeller; }
    bool isCustom() const { return kind == Kind::custom; }
    bool isTrs() const { return midiIn.startsWith ("TRS"); }
};
AmpInfo ampInfo (const juce::ValueTree& state);

std::unique_ptr<Page> makeCustomPage (PedalCuesProcessor&);   // a custom unit (beta)
std::unique_ptr<Page> makeModellerPage (PedalCuesProcessor&); // a Fractal / Line 6 unit with defined MIDI numbers
void newCustomUnit (juce::ValueTree state);      // adds one with example tiles, shows it and asks for its name
void importCustomUnit (juce::ValueTree state);   // Import unit: a .pedalcues-unit file
void exportCustomUnit (juce::ValueTree unit);
// The hover text for "Switch to the preset's setlist" (Quad Cortex, Helix, POD Go, Stadium pages).
inline juce::String setlistSwitchTooltip (const juce::String& unit)
{
    return "What it does: each preset tile also sends its setlist number (CC#32) before the preset change, so the " + unit
         + " switches to that preset's setlist even when it's on another one.\n\n"
           "Turn it on if your presets are in more than one setlist. Leave it off if you keep the " + unit
         + " in one setlist: a preset then loads from whichever setlist is active.\n\n"
           "Clips keep what they were dragged with: drag preset clips in again after changing this.";
}

// The unit picker: built-in units, the Fractal / Line 6 templates and your MIDI devices, with a search. onDone runs after a pick.
void showUnitPicker (juce::ValueTree state, juce::Component& target, std::function<void()> onDone);
std::unique_ptr<juce::Component> makeUnitPicker (juce::ValueTree state, const juce::String& search = {});   // screenshots
std::unique_ptr<juce::Component> makeTileEditor (PedalCuesProcessor&, juce::ValueTree tile, bool isNew);   // custom tile: guided MIDI editor
std::unique_ptr<Page> makeQcExpression (PedalCuesProcessor&);   // Quad Cortex page > Expression
std::unique_ptr<Page> makeWhammyPage   (PedalCuesProcessor&);
std::unique_ptr<Page> makeSettingsPage (PedalCuesProcessor&);
void showWiringGuide (const juce::ValueTree& state);   // Help > Wiring guide, for the unit on the first tab
void showQcSyncDialog (juce::ValueTree state);   // Quad Cortex page > Sync from QC (USB)
std::unique_ptr<juce::Component> makeWiringGuide (const AmpInfo&, int view = -1);   // view: 0 one device, 1 daisy chain, 2 separate

inline const juce::String repoUrl { "https://github.com/thankost/pedal-cues" };
inline const juce::String author { "Thanasis Kostopoulos" };
inline const juce::String guideUrl { "https://thankost.github.io/pedal-cues/guide.html" };
inline const juce::String changelogUrl { "https://thankost.github.io/pedal-cues/changelog.html" };
inline const juce::String helpUrl { "https://thankost.github.io/pedal-cues/help.html" };
inline const juce::String coffeeUrl { "https://buymeacoffee.com/athkost" };
inline const juce::String paypalUrl { "https://paypal.me/athkost" };
inline const juce::String revolutUrl { "https://revolut.me/athkost" };
inline const juce::String sponsorsUrl { "https://github.com/sponsors/thankost" };

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
// A search field for preset lists: fuzzy matching, Esc clears, Return picks the best match (onSubmit).
class SearchBox final : public juce::TextEditor
{
public:
    explicit SearchBox (const juce::String& placeholder);
    void paintOverChildren (juce::Graphics&) override;
    std::function<void()> onSearch, onSubmit;
};

// Shows the rows whose title / subtitle match the search, best first, in a scrolling list; hides the rest.
// Returns the row indices in the order shown.
std::vector<int> layoutFilteredRows (juce::OwnedArray<Tile>& rows, juce::Component& list, int width, int rowHeight,
                                     const juce::String& query);

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
