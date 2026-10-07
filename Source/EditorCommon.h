#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "Tile.h"
#include "Theme.h"
#include "Fuzzy.h"

#include <functional>
#include <memory>

namespace modellers { struct Profile; }

namespace ui
{
// Set (per computer) once the user clicks Done in a page's MIDI strip; until then the strips show the reminder.
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

// The pedal on the second tab: the Whammy, or a custom MIDI device (its name, colour and channel).
struct PedalInfo
{
    bool isCustom = false;
    const modellers::Profile* page = nullptr;   // a pedal page (DL4 MkII, HX One)
    bool isNone = false;                     // no pedal picked: the tab shows how to add one
    juce::String name { "Whammy V" };        // the device (the tab's tooltip, the MIDI strip): "Whammy V" or "Whammy DT"
    juce::String shortName { "Whammy" };     // in sentences and track names ("Whammy Cues")
    juce::Colour colour { theme::whammyRed };
    int channel = 2;
};
PedalInfo pedalInfo (const juce::ValueTree& state);

std::unique_ptr<Page> makeCustomPage (PedalCuesProcessor&, bool pedalsTab);   // a custom MIDI device (beta), on the first tab or the pedals tab
std::unique_ptr<Page> makePedalPage (PedalCuesProcessor&);    // the second tab: the Whammy or a custom MIDI device
std::unique_ptr<Page> makeModellerPage (PedalCuesProcessor&, bool pedalsTab = false); // a unit with defined MIDI numbers, on the first tab or the pedals tab (DL4 MkII)
void newCustomUnit (juce::ValueTree state, bool pedalsTab = false);
void confirmDeleteDevice (juce::ValueTree state, juce::ValueTree unit, std::function<void()> deleted);   // asks, then deletes      // adds one with example tiles, shows it on that tab, asks its name
void importCustomUnit (juce::ValueTree state, bool pedalsTab = false);   // Import device: a .pedalcues-device file, onto that tab
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
void showUnitPicker (juce::ValueTree state, juce::Component& target, std::function<void()> onDone, bool pedalsTab = false);
std::unique_ptr<juce::Component> makeUnitPicker (juce::ValueTree state, const juce::String& search = {}, bool pedalsTab = false);   // screenshots
std::unique_ptr<juce::Component> makeTileEditor (PedalCuesProcessor&, juce::ValueTree tile, bool isNew);   // custom tile: guided MIDI editor
std::unique_ptr<Page> makeQcExpression (PedalCuesProcessor&);   // Quad Cortex page > Expression
std::unique_ptr<Page> makeWhammyPage   (PedalCuesProcessor&);
std::unique_ptr<Page> makeSettingsPage (PedalCuesProcessor&);   // the Connect your rig panel (devices, wiring, DAW tracks, standalone port)
std::unique_ptr<Page> makeMidiStrip (PedalCuesProcessor&, bool pedalTab);
// Song Builder (beta): its own window, where tiles dragged from the tabs become a song. deviceName: the device on the
// tab being dragged from, to name a new track. makeSongBuilder is the window's content alone (DocShots).
std::unique_ptr<juce::DocumentWindow> makeSongBuilderWindow (PedalCuesProcessor&, std::function<juce::String()> deviceName);
std::unique_ptr<juce::Component> makeSongBuilder (PedalCuesProcessor&, std::function<juce::String()> deviceName);
std::unique_ptr<juce::Component> makeSongExportPanel (juce::ValueTree song);   // the Export MIDI file window's content (DocShots)   // above each device page: channel, Done, Test, How to connect
void showConnectDialog (PedalCuesProcessor&);                               // How to connect: the Connect your rig window
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
    juce::Colour hintColour { theme::dim };   // amber for a warning ("Strymon doesn't publish the order...")
    juce::Colour accentColour;
    juce::Colour fill { theme::surface.withAlpha (0.55f) };
};

//==============================================================================
// The automatic-save status: "Saving..." for a moment after a change, then "Saved" (standalone: written on this
// computer) or "In your project" (plugin: the DAW keeps it with the project), bright for two seconds, then dim; it
// stays in view so a missed save can still be checked. Hover it for what it means. compact: the icon only.
class SaveIndicator final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit SaveIndicator (PedalCuesProcessor&);
    bool compact = false;
    bool alignRight = false;   // in a window's bottom-right corner: the words end at the right edge
    static inline bool settledForScreenshots = false;   // DocShots: show the resting state, not the save it never gets to finish
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    PedalCuesProcessor& proc;
    bool shownPending = false;
    double savedAt = 0.0;   // when the last save finished: the words show for a moment, then fade to a dim tick
    float wordsAlpha() const;
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
// A pedal's faceplate banner: brushed gradient, screws, a heavy italic model name and a tagline (Whammy, effect pedal pages).
class Faceplate final : public juce::Component
{
public:
    Faceplate() { setInterceptsMouseClicks (false, false); }
    juce::String model { "WHAMMY V" }, tagline { "MIDI MODE  +  TREADLE AUTOMATION" };
    juce::Colour top { 0xffe0303f }, bottom { 0xff9e1426 };   // the Whammy's red
    juce::Colour ink { juce::Colours::white };                 // the logotype; dark on white / silver / yellow pedals
    int logoWidth = 360;
    void paint (juce::Graphics&) override;
};

} // namespace ui
