#pragma once

#include "Modellers.h"

#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>

#include <vector>

// Import from HX Edit: reads the files HX Edit exports (.hlx preset, .hls setlist, .hlb bundle) and fills a Line 6 page's
// preset list with their setlist, preset and snapshot names and snapshot LED colours. Files only: nothing is sent to the unit.
// Format (not published by Line 6; the same in several open parsers: HackLabsGuitar/helix-py-api BSD-3, PhaseDog/
// HelixNativePresets MIT, jyanes83's bundle parser):
//   .hlx  plain JSON { "data": { "meta": { "name" }, "tone": { "snapshot0".."snapshot7": { "@name", "@ledcolor" }, "dsp0"... } } }
//   .hls / .hlb  JSON wrapper { "meta": { "name" }, "encoded_data": base64 (zlib (JSON)) }
//         .hls decodes to an array of presets (array position = slot); .hlb to { "setlists": [ { "meta": { "name" }, "presets": [...] } ] }
//   A preset there is { "meta": { "name" }, "tone": {...} } (or wrapped in "data" like an .hlx; both are read).
namespace helix
{
struct Snapshot { juce::String name; int ledColour = -1; };   // ledColour: the file's @ledcolor (0-11), -1 when missing
struct Preset { int slot = 0; juce::String name; std::vector<Snapshot> snapshots; bool empty = false; };
struct Setlist { juce::String name; std::vector<Preset> presets; };
struct Result
{
    std::vector<Setlist> setlists;   // .hlx: one setlist with one preset (slot 0: the file doesn't say where it goes)
    juce::String error;              // empty when it worked
    juce::String kind;               // "preset", "setlist", "bundle"
    bool ok() const { return error.isEmpty(); }
};

Result readFile (const juce::File&);
Result readJson (const juce::String&);

// Helix snapshot LED colours (@ledcolor, helix-py-api snapshot.py LEDColor): 0 Auto, 1 White, 2 Red, 3 Dark orange,
// 4 Light orange, 5 Yellow, 6 Green, 7 Turquoise, 8 Blue, 9 Violet, 10 Pink, 11 Off. Returns a transparent colour for
// Auto, -1 and anything unknown (the caller then uses its own palette), grey for Off.
juce::Colour snapshotColour (int ledColour);

// The Line 6 pages whose HX Edit exports this reads: Helix Floor / LT / Rack, HX Stomp, HX Stomp XL, HX Effects.
// Not POD Go (POD Go Edit's export not checked) or Helix Stadium (a different, undocumented format).
bool supports (const modellers::Profile&);

// Writes setlist `setlistIndexInFile` of the file into the page's data (state::modeller): a setlist or bundle replaces every
// ModPreset whose setlist is `targetSetlist` (the CC#32 value, -1 on units without setlists); a single preset ("preset" kind)
// replaces only the one at (targetSetlist, its slot), so set Preset::slot before calling. The new presets go where the old
// ones were (else after the last preset), in slot order, with presetIndex = slot, the file's names, scene names and colours
// (only profile.sceneCount scenes). ModSwitch nodes are kept. Slots the unit doesn't have are skipped. When nothing would be
// written (all skipped), the page is left untouched. Returns the number of presets written.
int apply (juce::ValueTree modellerNode, const modellers::Profile&, const Result&, int setlistIndexInFile, int targetSetlist, bool skipEmpty);
}

namespace ui
{
// Line 6 page > Import from HX Edit: picks a file, shows what's in it, imports one setlist (or all of a bundle). HelixImportDialog.cpp.
void showHelixImport (juce::ValueTree state, const juce::String& profileId);
}
