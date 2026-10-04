#pragma once

#include "CueModel.h"

#include <vector>

// Fractal Audio, Line 6 and HeadRush units with defined MIDI numbers (Line 6's fixed map, Fractal's factory defaults), each
// shown on its own page like the Quad Cortex. A profile says how presets are numbered, how scenes / snapshots are
// selected, its footswitches or blocks, utilities, looper, pedals and connection facts. Every number comes from the
// manufacturer's manual named in `manual`; none of it is tested on hardware (the page says so).
// The Axe-Fx III, FM9, FM3 and VP4 have no default CCs, so they are editable device templates instead (DeviceTemplates.h).
namespace modellers
{
enum class Scheme
{
    helix,      // setlists (CC#32 0-7), 32 banks x A-D, PC 0-127
    podGo,      // setlists Factory / User (CC#32 0-1), 32 banks x A-D
    stadium,    // CC#32: 0 factory, 1-4 user groups of 128 (1A-128D), 5+ your setlists; PC 0-127 = 32 banks x A-D
    hxStomp,    // 42 banks x A-C, PC 0-125, no setlists
    hxFour,     // 32 banks x A-D, PC 0-127, no setlists
    axeFx2,     // banks A-F of 128 (CC#0), shown A000-F127
    ax8,        // 64 banks of 8, shown 01:1-64:8, CC#0 = index / 128, PC = index % 128
    fx8,        // 16 banks A-P of 8, shown A1-P8, Program Change only
    headrush,   // Core, Prime, Flex Prime: each rig's MIDI PROG, shown 1-128 = PC 0-127; no bank select, no setlists
    headrushOld // Pedalboard, Gigboard, MX5: MIDI Prog shown 0-127 = PC 0-127
};

struct Action { juce::String name, messages, note; int colour = 5; };   // messages in cues::custom syntax
struct Control { juce::String name; int cc = 0; int value = 127; };   // value: what a press sends (Line 6)

struct Profile
{
    juce::String id, brand, model, shortName, aliases;
    juce::Colour colour;
    juce::String manual;                    // "Helix Owner's Manual, firmware 3.80"
    Scheme scheme = Scheme::helix;

    juce::String sceneWord;                 // "Snapshot" / "Scene"
    int sceneCount = 8, sceneCc = 69;
    bool sceneBuffered = false;             // Line 6: a snapshot sent during a preset load waits for it (same tick is fine)
    bool scenePerCc = false;                // HeadRush: scene N is its own CC (sceneCc + N, any value), not a value of sceneCc
    juce::String sceneNote;

    juce::String switchesTitle;             // "Footswitches" / "Blocks"
    std::vector<Control> switches;
    bool switchesOnOff = false;             // Fractal blocks: on / off (127 / 0). Line 6 footswitches: a press (127)
    juce::String switchesNote;

    std::vector<Action> utilities, looper;
    std::vector<Control> pedals;            // expression: Helix EXP 1-3, Fractal external controllers
    juce::String pedalNote;

    juce::String tunerOn, tunerOff;         // MIDI Setup's Test button (custom message syntax)
    juce::String channelHint;               // MIDI Setup: where the channel is set on the unit
    bool hasDin = true;                     // a MIDI In jack (POD Go: USB only)
    juce::String midiIn { "5-pin MIDI In" };  // Flex Prime, MX5: "TRS MIDI In"
    bool usbMidi = true;                    // MIDI over USB from a computer (HeadRush: not in the manuals)
    int usbToThru = 0;                      // 0 never, 1 yes, 2 only with a setting (usbThruSetting)
    juce::String usbThruSetting;
    juce::String notes;                     // "About this unit" on the page: what to check, connection facts
};

const std::vector<Profile>& all();
const Profile* find (const juce::String& id);

// Presets: each one is a setlist (CC#32 value, or -1 for units without setlists) and an index within it.
bool hasSetlists (const Profile&);
juce::StringArray setlistNames (const Profile&);          // index = CC#32 value
int presetsPerSetlist (const Profile&, int setlist);
juce::StringArray bankNames (const Profile&, int setlist); // "01".."32", "A".."F", "01".."64", "A".."P"
juce::StringArray slotNames (const Profile&);              // "A".."D", "000".."127", "1".."8"
int slotsPerBank (const Profile&);
juce::String presetLabel (const Profile&, int setlist, int index);    // "01A", "33A", "A000", "17:1", "A1", "Prog 12"
juce::String slotTitle (const Profile&);                              // the Edit preset field: "Preset", "MIDI PROG"
juce::String sceneCcs (const Profile&);                               // "CC#69", "CC#21-30"
juce::String setlistLabel (const Profile&, int setlist);              // "USER 1", "" when none
int defaultSetlist (const Profile&);

void addPresetLoad (cues::Cue&, const Profile&, int channel, int setlist, int index, bool sendSetlist, double beat);
cues::Cue preset (const Profile&, int channel, int setlist, int index, bool sendSetlist, const juce::String& name);
cues::Cue scene (const Profile&, int channel, int sceneIndex, const juce::String& name);
cues::Cue sceneAfterPreset (const Profile&, int channel, int setlist, int index, bool sendSetlist,
                            const juce::String& presetName, int sceneIndex, const juce::String& sceneName);
cues::Cue switchCue (const Profile&, int channel, int switchIndex, bool on, const juce::String& name);
cues::Cue switchAfterPreset (const Profile&, int channel, int setlist, int index, bool sendSetlist,
                             const juce::String& presetName, int switchIndex, bool on, const juce::String& name);
cues::Cue action (const Profile&, int channel, const Action&);

constexpr double fractalGap = 0.25;   // Fractal: the scene / block follows the preset 1/16 later, like the QC
}
