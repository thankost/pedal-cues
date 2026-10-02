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

    PEDALCUES_ID (name)
    PEDALCUES_ID (colour)
    PEDALCUES_ID (setlist)
    PEDALCUES_ID (bank)
    PEDALCUES_ID (slot)

    PEDALCUES_ID (qcChannel)
    PEDALCUES_ID (whChannel)
    PEDALCUES_ID (whPcBase)
    PEDALCUES_ID (sendSetlist)
    PEDALCUES_ID (selectedPreset)
    PEDALCUES_ID (comboPresetScene)
    PEDALCUES_ID (stompOn)
    PEDALCUES_ID (whChords)
    PEDALCUES_ID (whBypass)
    PEDALCUES_ID (whHeelFirst)
    PEDALCUES_ID (sweepBeats)
    PEDALCUES_ID (sweepCurve)
    PEDALCUES_ID (sweepReset)
    PEDALCUES_ID (sweepDraw)
    PEDALCUES_ID (sweepDrawing)
    PEDALCUES_ID (qcExpressionView)
    PEDALCUES_ID (expPedal)
    PEDALCUES_ID (expLoadFirst)
    PEDALCUES_ID (expBeats)
    PEDALCUES_ID (expCurve)
    PEDALCUES_ID (expReset)
    PEDALCUES_ID (expDraw)
    PEDALCUES_ID (expDrawing)
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
    bool saveLibrary (const juce::ValueTree& root, const juce::File&);
    bool loadLibrary (juce::ValueTree& root, const juce::File&);
}
