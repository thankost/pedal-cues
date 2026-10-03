#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>

namespace IDs
{
#define PEDALCUES_ID(n) inline const juce::Identifier n { #n };
    PEDALCUES_ID (PedalCues)
    PEDALCUES_ID (QC)
    PEDALCUES_ID (Preset)
    PEDALCUES_ID (Scene)
    PEDALCUES_ID (Stomp)
    PEDALCUES_ID (Whammy)
    PEDALCUES_ID (Effect)
    PEDALCUES_ID (Drawings)
    PEDALCUES_ID (Drawing)

    PEDALCUES_ID (name)
    PEDALCUES_ID (colour)
    PEDALCUES_ID (setlist)
    PEDALCUES_ID (bank)
    PEDALCUES_ID (slot)
    PEDALCUES_ID (points)

    PEDALCUES_ID (qcChannel)
    PEDALCUES_ID (whChannel)
    PEDALCUES_ID (whPcBase)
    PEDALCUES_ID (sendSetlist)
    PEDALCUES_ID (selectedPreset)
    PEDALCUES_ID (comboPresetScene)
    PEDALCUES_ID (stompOn)
    PEDALCUES_ID (whModel)        // 0 = Whammy V, 1 = Whammy DT
    PEDALCUES_ID (whDropTuneView) // Whammy DT page: Drop Tune tiles instead of the Whammy modes
    PEDALCUES_ID (whChords)
    PEDALCUES_ID (whBypass)
    PEDALCUES_ID (whHeelFirst)
    PEDALCUES_ID (sweepBeats)
    PEDALCUES_ID (sweepCurve)
    PEDALCUES_ID (sweepReset)
    PEDALCUES_ID (sweepDraw)
    PEDALCUES_ID (sweepDrawing)
    PEDALCUES_ID (sweepDrawingName)
    PEDALCUES_ID (qcExpressionView)
    PEDALCUES_ID (expPedal)
    PEDALCUES_ID (expLoadFirst)
    PEDALCUES_ID (setupViaQcChain)   // only in exported setup files: the wiring choice (a per-computer setting)
    PEDALCUES_ID (expBeats)
    PEDALCUES_ID (expCurve)
    PEDALCUES_ID (expReset)
    PEDALCUES_ID (expDraw)
    PEDALCUES_ID (expDrawing)
    PEDALCUES_ID (expDrawingName)
#undef PEDALCUES_ID
}

namespace state
{
    struct NamedColour { const char* name; juce::uint32 argb; };

    // Gig-view style palette.
    const juce::Array<NamedColour>& palette();

    juce::ValueTree createDefault();
    juce::ValueTree createPreset (const juce::String& name, int setlist, int bank, int slot, juce::Colour);

    // Fills in anything missing (older/partial state, hand-edited library files).
    void sanitise (juce::ValueTree& root);

    juce::Colour colourOf (const juce::ValueTree& node, juce::Colour fallback = juce::Colours::grey);

    juce::File defaultLibraryFile();

    // Per-user flags stored next to the library (e.g. whether the quick tour was shown).
    bool getFlag (const juce::String& name);
    void setFlag (const juce::String& name, bool value);
    juce::String getSetting (const juce::String& name);
    void setSetting (const juce::String& name, const juce::String& value);
    // "My drawings": saved freehand moves (Drawings > Drawing { name, points }), shared by the Whammy treadle
    // and QC expression. They live on this computer (drawings.xml next to the default setup), not in projects,
    // so every project and every PedalCues instance sees the same list and updates keep it.
    // 'points' is cues::whammy::encodeDrawing text. Message thread only.
    juce::ValueTree myDrawings();                        // the shared list, read from disk on first use
    void storeMyDrawings();                              // write it to disk; call after every change
    void useMyDrawingsFile (const juce::File&);          // tests and screenshots: keep the user's file untouched

    juce::ValueTree findDrawing (const juce::ValueTree& list, const juce::String& name);
    void saveDrawing (juce::ValueTree list, const juce::String& name, const juce::String& points);   // adds or overwrites
    bool renameDrawing (juce::ValueTree list, const juce::String& from, const juce::String& to);     // false if 'to' is taken
    void deleteDrawing (juce::ValueTree list, const juce::String& name);
    void mergeDrawings (juce::ValueTree into, const juce::ValueTree& from);                          // adds / overwrites by name

    // The setup file: names + MIDI settings, and with 'drawings' also My drawings (Export setup).
    // Loading adds any drawings in the file to 'drawingsInto' (Import setup); it never removes any.
    bool saveLibrary (const juce::ValueTree& root, const juce::File&, const juce::ValueTree* drawings = nullptr);
    bool loadLibrary (juce::ValueTree& root, const juce::File&, juce::ValueTree* drawingsInto = nullptr);
}
