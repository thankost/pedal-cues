#include "Modellers.h"

// Meris, Chase Bliss, Walrus Audio, Source Audio, Electro-Harmonix pages (Effects & Pedals tab), from the makers' MIDI charts:
// Meris pedal manuals (Mercury7 v4, Ottobit Jr. v6, Polymoon 4, Enzo v5, Hedra v9) + the Meris MIDI I/O manual v1b (PC 0 = bypass);
// Meris LVX v1.5.1, MercuryX v1.5, Enzo X v1.5.1, Ottobit X v1.1.1; the Chase Bliss MIDI manuals; the Walrus Audio MAKO MKII
// manuals; the Source Audio MIDI implementations and owner's manuals; the EHX POG3 (web v1) and Oceans Abyss (web v2) manuals.
// None of it is tested on hardware. Left out on purpose: save / reset / erase commands (Chase Bliss CC#56 factory reset, CC#111
// and CC#27 preset save, POG3 CC#80 save), footswitch CCs that need a press and then a release (Meris X hold modifier), and
// enumerations whose value order the manual doesn't state (Walrus programs, divisions, shapes).
namespace modellers
{
namespace
{
const juce::Colour merisColour       { 0xffb48cff };
const juce::Colour chaseBlissColour  { 0xff7fd1ae };
const juce::Colour walrusColour      { 0xff5cc8c8 };
const juce::Colour sourceAudioColour { 0xffff9f43 };
const juce::Colour ehxColour         { 0xffe74c3c };

// Tile colours: palette indices (State.cpp): 0 red, 1 orange, 2 yellow, 3 green, 4 teal, 5 blue, 6 purple, 7 pink, 8 white, 9 grey.
juce::String n (int v) { return juce::String (v); }

const juce::String oneTap ("One tap. Tempo needs several taps a beat apart: drop the clip on each beat, or set the tempo in the preset.");
const juce::String anyToggle ("Any value toggles it: each clip switches it the other way.");

Action act (const juce::String& name, int cc, int value, const juce::String& note = {}, int colour = 5)
{
    return { name, "CC " + n (cc) + "=" + n (value), note, colour };
}

// A list of values of one CC, as tiles.
void addValues (std::vector<Action>& list, int cc, std::initializer_list<std::pair<const char*, int>> items, const juce::String& note, int colour)
{
    for (const auto& [name, value] : items)
        list.push_back (act (name, cc, value, note, colour));
}

// A Chase Bliss 3-position toggle: 1 = left, 2 = middle, 3 = right.
void addToggle (std::vector<Action>& list, int cc, const char* left, const char* middle, const char* right, const juce::String& note, int colour)
{
    addValues (list, cc, { { left, 1 }, { middle, 2 }, { right, 3 } }, note, colour);
}

ModelGroup group (const juce::String& title, int cc, std::initializer_list<std::pair<const char*, int>> items, const juce::String& note, int colour)
{
    ModelGroup g { title, "CC#" + n (cc), {} };
    addValues (g.actions, cc, items, note, colour);
    return g;
}

// A Meris X enumeration printed as receive ranges: send the middle of each range.
struct Range { const char* name; int lo, hi; };
ModelGroup ranges (const juce::String& title, int cc, std::initializer_list<Range> items, const juce::String& note, int colour)
{
    ModelGroup g { title, "CC#" + n (cc), {} };
    for (const auto& r : items)
        g.actions.push_back (act (r.name, cc, (r.lo + r.hi) / 2, note, colour));
    return g;
}

std::vector<Control> knobs (std::initializer_list<std::pair<const char*, int>> items)
{
    std::vector<Control> c;
    for (const auto& [name, cc] : items)
        c.push_back ({ name, cc });
    return c;
}

Profile base (const char* id, const char* brand, const char* model, const char* shortName, const char* aliases, juce::Colour colour,
              const char* manual)
{
    Profile p;
    p.id = id; p.brand = brand; p.model = model; p.shortName = shortName; p.aliases = aliases; p.colour = colour; p.manual = manual;
    p.beta = true; p.pedal = true;
    p.scheme = Scheme::numbered;
    p.sceneWord = "Scene"; p.sceneCount = 0;
    p.mainTitle = "Controls";
    p.switchesTitle = "Switches";
    p.switchesOnOff = true;
    return p;
}

//==============================================================================
// Meris legacy pedals: 16 presets, PC 0 = bypass, PC 1-16 = presets 1-16; MIDI on the EXP jack (MIDI I/O box).
const juce::String merisLegacyNotes (
    "Presets: 16, Program Change 1-16 = presets 1-16 (they also turn the pedal on). Program Change 0 bypasses it (Meris MIDI I/O manual). "
    "To load a preset but stay bypassed, drop the preset tile, then Effect Off. No bank select.\n\n"
    "Connection: no 5-pin jack. MIDI In and Out share the EXP jack (TRS) once Global Settings > EXP jack mode is MIDI; Meris receive "
    "on the tip and send on the ring, so use the Meris MIDI I/O box (5-pin to TRS). Output mode MIDI THRU (Global Settings, R switch) "
    "passes MIDI on to the next pedal through the MIDI I/O.\n\nMIDI channel: 1-16 or Omni (Global Settings, bottom-row middle knob); "
    "the default isn't in the manual.");

Profile merisLegacy (const char* id, const char* model, const char* shortName, const char* aliases, const char* manual)
{
    auto p = base (id, "Meris", model, shortName, aliases, merisColour, manual);
    p.presetCount = 16; p.labelFrom = 1; p.pcOffset = 1;   // PC 0 = bypass, so preset 1 = PC 1
    p.padCc = -1;   // CC#0 is in none of the five charts (they use CC#4, 9, 14-31), so the usual CC#0 = 0 is ignored
    p.switches = { { "Effect", 14, 127, 0 } };   // 0-63 bypass, 64-127 on; sends 0 / 127
    p.switchesNote = "Effect (CC#14) turns the pedal on or bypasses it, keeping the preset.";
    p.utilities = { { "Bypass (PC 0)", "PC 0", "Program Change 0 bypasses the pedal (Meris MIDI I/O manual).", 9 } };
    p.pedalNote = "Expression (CC#4) morphs between the preset's heel and toe settings. The others move that knob like turning it.";
    p.testMessage = "PC 1";   // preset 1
    p.channelHint = "Must match the pedal: Global Settings (hold the left / ALT switch at power-up), bottom-row middle knob. Default not in the manual.";
    p.midiIn = "TRS MIDI In (EXP jack, tip)";
    p.hasDin = true;          // a MIDI In jack (TRS on the EXP jack, through the Meris MIDI I/O)
    p.hasThru = true;         // MIDI THRU output mode, on the same jack's ring
    p.usbMidi = false; p.usbToThru = 0;
    return p;
}

// Meris X series: 99 presets + 3 favourites, PC 0 = bypass; 5-pin MIDI In / Out and USB-C.
Profile merisX (const char* id, const char* model, const char* shortName, const char* aliases, const char* manual, int clockCc)
{
    auto p = base (id, "Meris", model, shortName, aliases, merisColour, manual);
    p.presetCount = 102; p.labelFrom = 1; p.pcOffset = 1;   // PC 1-99 = presets 1-99, PC 100-102 = favourites 1-3
    p.reservedPrograms = { { 99, "Favorite 1" }, { 100, "Favorite 2" }, { 101, "Favorite 3" } };
    p.switches = { { "Effect", 14, 127, 0 } };
    p.switchesNote = "Effect (CC#14) turns the pedal on (64-127) or bypasses it (0-63), keeping the preset.";
    p.utilities = { { "Bypass (PC 0)", "PC 0", "Program Change 0 bypasses the pedal.", 9 },
                    { "Tuner", "CC 117=127", "Toggles tuner mode (CC#117 = 127).", 5 } };
    addValues (p.utilities, clockCc, { { "Clock: global", 21 }, { "Clock: listen", 64 }, { "Clock: ignore", 106 } },
               "This preset's MIDI clock setting (CC#" + n (clockCc) + "): use the global one, always listen, or always ignore.", 8);
    p.tunerOn = "CC 117=127"; p.tunerOff = "CC 117=127";   // a toggle: Test sends it twice
    p.testMessage = "PC 1";
    p.channelHint = "Must match the pedal: Globals > MIDI CHANNEL (1-16 or Omni). Default not in the manual.";
    p.midiIn = "5-pin MIDI In";
    p.hasDin = true; p.hasThru = true; p.usbMidi = true;
    p.usbToThru = 3;   // MIDI OUT can be set to Thru; whether USB MIDI reaches the 5-pin Out isn't documented
    return p;
}

juce::String merisXNotes (const juce::String& extra)
{
    return "Presets: 99 (33 banks of 3) plus the FAVORITES bank: Program Change 1-99 = presets 1-99, PC 100-102 = favourites 1-3. "
           "PC 0 bypasses the pedal. No bank select.\n\n" + extra
           + "\n\nThe CCs act on the loaded preset. Models sends the middle of each range in the manual's table. The hold modifier (CC#118) "
             "needs a press and then a release, so it isn't a tile.\n\nConnection: 5-pin MIDI In and Out, and USB-C MIDI. Globals > MIDI OUT "
             "can be MIDI Thru (passes MIDI In on); whether USB MIDI is passed on isn't documented. MIDI channel 1-16 or Omni (Globals); "
             "the default isn't in the manual.";
}

//==============================================================================
// Chase Bliss TRS pedals: 122 slots, PC 0 = Live, PC n = slot n; channel 2 out of the box.
const juce::String cbSwitchNote ("On sends 127 (any value of 1 or more is on), Off sends 0.");
const juce::String cbToggleNote ("Sets the 3-position toggle like flipping it (1 = left, 2 = middle, 3 = right).");

Profile chaseBliss (const char* id, const char* model, const char* shortName, const char* aliases, const char* manual,
                    const char* channelButtons = "both footswitches")
{
    auto p = base (id, "Chase Bliss", model, shortName, aliases, chaseBlissColour, manual);
    p.presetCount = 122; p.labelFrom = 1; p.pcOffset = 1;   // PC 0 = Live (the knobs), PC 1-122 = slots 1-122
    // CC#0 / CC#32 are controls on several Chase Bliss pedals (MOOD MKII CC#0 = pitch bend; CC#32 on Gen Loss MKII, Onward, Clean,
    // MOOD MKII), so pad with CC#40: not in any of the twelve charts read (all use 1-33, 51-59, 61-84, 93, 100-111).
    p.padCc = 40;
    p.utilities = { { "Live", "PC 0", "Program Change 0 = Live: the pedal follows its knobs again.", 8 } };
    p.switchesNote = cbSwitchNote;
    p.pedalNote = "Expression (CC#100) is the pedal's expression over MIDI. The others move that knob like turning it.";
    p.testMessage = "PC 1";   // slot 1 = the preset toggle's right position
    p.channelHint = juce::String ("Channel 2 out of the box. To change it, hold ") + channelButtons
                  + " while powering up, then send a Program Change on the new channel.";
    p.midiIn = "TRS MIDI In (1/4\" jack, via a MIDIBox)";
    p.hasDin = true;     // a MIDI In jack (TRS)
    p.hasThru = false;   // no MIDI Out / Thru in the manuals
    p.usbMidi = false; p.usbToThru = 0;
    return p;
}

juce::String cbNotes (const juce::String& extra, const char* saveButtons = "both footswitches")
{
    return "Presets: 122 slots. Program Change 1-122 = slots 1-122 (an empty slot recalls nothing); slots 1 and 2 are the preset "
           "toggle (1 = right, 2 = left). Program Change 0 = Live: the current knob settings. To save, send the Program Change while "
           "holding " + juce::String (saveButtons) + " (no save tile here).\n\n" + extra
           + "\n\nNot here on purpose: CC#56 (factory reset) and the save CC.\n\nConnection: standard 1/4\" TRS cable from a Chase "
             "Bliss MIDIBox (5-pin to TRS). Chase Bliss pedals receive on the ring (the Meris manual says so), so an Empress / Meris "
             "style box needs a cable that swaps tip and ring. No MIDI Out or Thru is documented. MIDI channel 2 out of the box.";
}

// Chase Bliss Automatones: 30 presets in 3 banks of 10 (shown 0-9), 5-pin MIDI In and Thru.
Profile automatone (const char* id, const char* model, const char* shortName, const char* aliases, const char* manual)
{
    auto p = base (id, "Chase Bliss", model, shortName, aliases, chaseBlissColour, manual);
    p.presetCount = 30; p.labelFrom = 0; p.pcOffset = 0;   // PC 0-29: bank 1 = 0-9, bank 2 = 10-19, bank 3 = 20-29
    for (int i = 0; i < 30; ++i)
        p.reservedPrograms.push_back ({ i, n (i / 10 + 1) + ":" + n (i % 10) });   // "2:7" = bank 2, preset 7 (PC 17)
    p.padCc = 40;   // same rule as the other Chase Bliss pedals; CC#40 isn't in the Automatone charts (14-31, 100-102)
    p.switchesNote = cbSwitchNote;
    p.pedalNote = "Expression (CC#100) is the pedal's expression over MIDI. The others move that fader like sliding it.";
    p.testMessage = "PC 0";   // bank 1, preset 0
    p.channelHint = "Channel 2 out of the box. To change it, hold both footswitches while powering up, release when the display lights, "
                    "then send a Program Change on the new channel.";
    p.midiIn = "5-pin MIDI In";
    p.hasDin = true; p.hasThru = true; p.usbMidi = false; p.usbToThru = 0;
    return p;
}

juce::String automatoneNotes (const juce::String& extra)
{
    return "Presets: 30, three banks of 10 shown 0-9 (left LED off / red / green). Program Change 0-29: 1:4 = PC 4, 2:7 = PC 17, "
           "3:0 = PC 20. Saving (CC#27) isn't a tile.\n\n" + extra
           + "\n\nConnection: 5-pin MIDI In and MIDI Thru (passes what arrives at MIDI In). No USB MIDI in the manual. MIDI channel 2 "
             "out of the box.";
}

//==============================================================================
// Walrus Audio MAKO MKII: 128 presets, PC 0-8 = banks A-C red / green / blue.
Profile walrus (const char* id, const char* model, const char* shortName, const char* aliases, const char* manual)
{
    auto p = base (id, "Walrus Audio", model, shortName, aliases, walrusColour, manual);
    p.presetCount = 128; p.labelFrom = 0; p.pcOffset = 0;
    const char* banks[] = { "A", "B", "C" };
    const char* colours[] = { "Red", "Green", "Blue" };
    for (int i = 0; i < 9; ++i)
        p.reservedPrograms.push_back ({ i, juce::String (banks[i / 3]) + " " + colours[i % 3] });
    p.padCc = -1;   // CC#0 isn't in the MKII charts (the lowest CC used is 3)
    p.switchesNote = "On sends 127, Off sends 0, as in the manual's table.";
    p.testMessage = "PC 0";   // bank A, red
    p.channelHint = "Must match the pedal: Global Preferences > MIDI > Chnl (channel 1 out of the box).";
    p.midiIn = "MIDI In (jack type not in the manual)";
    p.hasDin = true; p.hasThru = true; p.usbMidi = false; p.usbToThru = 0;
    return p;
}

juce::String walrusNotes (const juce::String& extra)
{
    return "MKII only: the MKI uses different numbers (use a custom MIDI device for a MKI).\n\nPresets: 128. Program Change 0-8 = the "
           "pedal's banks A-C, Red / Green / Blue (PC 0 = A Red, 3 = B Red, 8 = C Blue); 9-127 are only reachable over MIDI and shown "
           "here with their Program Change number. No bank select.\n\n" + extra
           + "\n\nConnection: MIDI IN and MIDI THRU jacks (the manual doesn't say which TRS type). USB-C is for firmware and IRs; USB "
             "MIDI isn't documented. MIDI channel 1 out of the box (Global Preferences > MIDI).";
}

//==============================================================================
// Source Audio One Series: 128 presets, preset 1 = Program Change 0.
Profile sourceAudio (const char* id, const char* model, const char* shortName, const char* aliases, const char* manual)
{
    auto p = base (id, "Source Audio", model, shortName, aliases, sourceAudioColour, manual);
    p.presetCount = 128; p.labelFrom = 1; p.pcOffset = 0;
    // CC#32 is a parameter (Nemesis LFO lock, Ventris diffusion B) and CC#1 an engine select, so pad with CC#60: not in the
    // Nemesis, Ventris or Collider charts (they use 1-55, 80-108).
    p.padCc = 60;
    p.switchesNote = "On sends 127, Off sends 0 (it bypasses at 0-63 / 0-64, engages above).";
    p.testMessage = "PC 0";   // preset 1
    p.channelHint = "Must match the pedal: Neuro editor > Hardware Options (channel 1 out of the box).";
    p.midiIn = "5-pin MIDI In";
    p.hasDin = true; p.hasThru = true; p.usbMidi = true;
    p.usbToThru = 3;   // MIDI THRU echoes MIDI In; USB MIDI to Thru isn't documented
    return p;
}

juce::String sourceAudioNotes (const juce::String& extra, bool remappable)
{
    return "These are the default MIDI numbers." + juce::String (remappable ? " The map can be changed in the Neuro Desktop Editor (Device > "
           "Edit Device MIDI Map); if you did, use a custom MIDI device instead." : "")
           + "\n\nPresets: 128. Presets 1-128 = Program Change 1-128 in the manual's 1-based counting, so preset 1 = data 0"
           + (remappable ? " (the Nemesis manual says so; this manual doesn't spell it out)." : " (\"PC #1-100 or 0-99 depending on your controller\").")
           + " Presets 1-4 (1-8 with Preset Extension) are on the pedal, the rest over MIDI only. No bank select.\n\n" + extra
           + "\n\nConnection: 5-pin MIDI In and MIDI Thru (echoes MIDI In), and class-compliant USB MIDI; whether USB MIDI reaches the "
             "Thru isn't documented. MIDI channel 1 out of the box (Neuro editor > Hardware Options).";
}

std::vector<Action> sourceAudioControls()
{
    return { act ("Tap", 93, 127, oneTap, 1),
             act ("Next preset", 82, 127, "Next preset (any value; wraps within presets 1-4, or 1-8 with Preset Extension).", 8),
             act ("Previous preset", 80, 127, "Previous preset (any value; wraps within presets 1-4, or 1-8).", 8) };
}
}

//==============================================================================
void addBoutiquePedals (std::vector<Profile>& v)
{
    // ---------------------------------------------------------------- Meris legacy
    {
        auto p = merisLegacy ("meris.mercury7", "Mercury7", "Mercury7", "mercury 7 mercury7 reverb shimmer",
                              "Mercury7 full Manual v4 + Meris MIDI I/O manual v1b");
        p.switches.push_back ({ "Swell", 28, 127, 0 });
        p.utilities.push_back (act ("Ultraplate", 29, 0, "Algorithm (CC#29): Ultraplate.", 6));
        p.utilities.push_back (act ("Cathedra", 29, 127, "Algorithm (CC#29): Cathedra.", 6));
        p.pedals = knobs ({ { "Expression", 4 }, { "Space decay", 16 }, { "Modulate", 17 }, { "Mix", 18 }, { "Lo freq", 19 },
                            { "Pitch vector", 20 }, { "Hi freq", 21 }, { "Predelay", 22 }, { "Mod speed", 23 }, { "Pitch vector mix", 24 },
                            { "Density", 25 }, { "Attack time", 26 }, { "Vibrato depth", 27 } });
        p.notes = merisLegacyNotes + "\n\nControls: Effect (CC#14), Swell (CC#28), algorithm Ultraplate / Cathedra (CC#29). No tap or tempo "
                  "CC in the manual. Pitch vector (CC#20) is a knob here: the manual has no value table for its settings.";
        v.push_back (p);
    }
    {
        auto p = merisLegacy ("meris.ottobit-jr", "Ottobit Jr.", "Ottobit Jr", "ottobit jr junior bitcrusher sequencer stutter",
                              "Ottobit Jr FULL Manual v6 + Meris MIDI I/O manual v1b");
        p.switches.push_back ({ "Stutter hold", 31, 127, 0 });
        p.utilities.push_back (act ("Tap", 28, 127, oneTap, 1));
        addValues (p.utilities, 29, { { "Seq: pitch", 0 }, { "Seq: sample rate", 63 }, { "Seq: filter", 127 } }, "Sequencer type (CC#29).", 6);
        p.models = { ranges ("Sequencer", 20, { { "Seq off", 0, 3 }, { "Seq 1x", 4, 32 }, { "Seq 2x", 33, 59 }, { "Seq 4x", 60, 87 },
                                                { "Seq 8x", 88, 115 }, { "Seq infinite", 116, 127 } }, "Sequencer (CC#20).", 3),
                     ranges ("Sequencer multiply", 21, { { "x1", 0, 0 }, { "x2", 1, 19 }, { "x4", 20, 39 }, { "x8", 40, 59 }, { "x16", 60, 79 },
                                                         { "x32", 80, 99 }, { "x64", 100, 119 }, { "x128", 120, 127 } }, "Sequencer multiply (CC#21).", 4),
                     ranges ("Stutter", 19, { { "Stutter off", 0, 0 }, { "Full 1x", 1, 11 }, { "Full 2x", 12, 17 }, { "Full 3x", 18, 22 },
                                              { "Full 4x", 23, 28 }, { "Full 6x", 29, 34 }, { "Full 8x", 35, 40 }, { "Full 16x", 41, 45 },
                                              { "Double 1x", 46, 51 }, { "Double 2x", 52, 57 }, { "Double 3x", 58, 63 }, { "Double 4x", 64, 68 },
                                              { "Double 6x", 69, 74 }, { "Double 8x", 75, 80 }, { "Double 16x", 81, 86 }, { "Half 1x", 87, 91 },
                                              { "Half 2x", 92, 97 }, { "Half 3x", 98, 103 }, { "Half 4x", 104, 109 }, { "Half 6x", 110, 114 },
                                              { "Half 8x", 115, 120 }, { "Half 16x", 121, 126 }, { "Random", 127, 127 } }, "Stutter (CC#19).", 7) };
        p.pedals = knobs ({ { "Expression", 4 }, { "Tempo", 15 }, { "Sample rate", 16 }, { "Filter", 17 }, { "Bits", 18 }, { "Stutter", 19 },
                            { "Sequencer", 20 }, { "Seq multiply", 21 }, { "Step 1", 22 }, { "Step 2", 23 }, { "Step 3", 24 }, { "Step 4", 25 },
                            { "Step 5", 26 }, { "Step 6", 27 } });
        p.notes = merisLegacyNotes + "\n\nControls: Effect (CC#14), stutter hold (CC#31), tap (CC#28), sequencer type (CC#29). Models: "
                  "sequencer (CC#20), multiply (CC#21) and stutter (CC#19) settings from the manual's value tables. Syncs to MIDI clock.";
        v.push_back (p);
    }
    {
        auto p = merisLegacy ("meris.polymoon", "Polymoon", "Polymoon", "polymoon delay modulated flanger phaser",
                              "Polymoon FULL Manual 4 + Meris MIDI I/O manual v1b");
        p.switches.push_back ({ "Dotted 8th", 9, 127, 0 });
        p.switches.push_back ({ "Flanger feedback", 30, 127, 0 });
        p.switches.push_back ({ "Half speed", 31, 127, 0 });
        p.utilities.push_back (act ("Tap", 28, 127, oneTap, 1));
        addValues (p.utilities, 29, { { "Phaser off", 0 }, { "Phaser slow", 63 }, { "Phaser sync slow", 95 }, { "Phaser sync", 127 } },
                   "Phaser mode (CC#29).", 6);
        p.models = { ranges ("Multiply", 19, { { "Multiply 1", 0, 7 }, { "Multiply 2", 8, 32 }, { "Multiply 3", 33, 62 }, { "Multiply 4", 63, 87 },
                                               { "Multiply 5", 88, 115 }, { "Multiply 6", 116, 127 } }, "Multiply (CC#19).", 3),
                     ranges ("Flanger mode", 26, { { "Env down", 0, 32 }, { "Env up", 33, 88 }, { "LFO", 89, 127 } }, "Dynamic flanger mode (CC#26).", 4) };
        p.pedals = knobs ({ { "Expression", 4 }, { "Tempo", 15 }, { "Time", 16 }, { "Feedback", 17 }, { "Mix", 18 }, { "Multiply", 19 },
                            { "Dimension", 20 }, { "Dynamics", 21 }, { "Early mod", 22 }, { "Feedback filter", 23 }, { "Delay level", 24 },
                            { "Late mod", 25 }, { "Flanger speed", 27 } });
        p.pedalNote += " Tempo (CC#15) is 10 ms steps up to 120; values above 120 aren't defined.";
        p.notes = merisLegacyNotes + "\n\nControls: Effect (CC#14), dotted 8th (CC#9, off = 1/4 note), flanger feedback (CC#30), half speed "
                  "(CC#31), tap (CC#28), phaser mode (CC#29). Models: multiply (CC#19) and flanger mode (CC#26). Tempo CC#15 = 10 ms "
                  "steps (0-120). Syncs to MIDI clock.";
        v.push_back (p);
    }
    {
        auto p = merisLegacy ("meris.enzo", "Enzo", "Enzo", "enzo synth synthesizer arp poly mono", "Enzo Manual v5 + Meris MIDI I/O manual v1b");
        p.utilities.push_back (act ("Tap", 28, 127, oneTap, 1));
        addValues (p.utilities, 29, { { "Dry", 0 }, { "Mono", 63 }, { "Arp", 95 }, { "Poly", 127 } }, "Synth mode (CC#29).", 6);
        addValues (p.utilities, 30, { { "Sawtooth", 0 }, { "Square", 127 } }, "Synth waveshape (CC#30).", 7);
        addValues (p.utilities, 9, { { "Triggered env", 0 }, { "Env follower", 64 } }, "Envelope type (CC#9).", 4);
        p.models = { ranges ("Filter type", 23, { { "Ladder LP", 0, 4 }, { "Ladder BP", 5, 32 }, { "Ladder HP", 33, 59 }, { "SV lowpass", 60, 87 },
                                                  { "SV bandpass", 88, 115 }, { "SV highpass", 116, 127 } }, "Filter type (CC#23).", 3) };
        p.pedals = knobs ({ { "Expression", 4 }, { "Tempo", 15 }, { "Pitch", 16 }, { "Filter", 17 }, { "Mix", 18 }, { "Sustain", 19 },
                            { "Filter envelope", 20 }, { "Modulation", 21 }, { "Portamento", 22 }, { "Delay level", 24 }, { "Ring mod", 25 },
                            { "Filter bandwidth", 26 }, { "Delay feedback", 27 } });
        p.pedalNote += " Tempo (CC#15) is 10 ms steps up to 120; values above 120 aren't defined.";
        p.notes = merisLegacyNotes + "\n\nControls: Effect (CC#14), tap (CC#28), synth mode (CC#29), waveshape (CC#30), envelope type "
                  "(CC#9). Models: filter type (CC#23; Ladder shelving bandpass = Ladder BP, SV = state variable). Tempo CC#15 = 10 ms "
                  "steps (0-120). MIDI notes aren't in this manual.";
        v.push_back (p);
    }
    {
        auto p = merisLegacy ("meris.hedra", "Hedra", "Hedra", "hedra pitch shifter delay harmony", "Hedra Manual v9 + Meris MIDI I/O manual v1b");
        p.switches.push_back ({ "Half speed", 9, 64, 0 });   // transmits 64
        p.switches.push_back ({ "Smoothing", 30, 127, 0 });
        p.switches.push_back ({ "Swell", 31, 127, 0 });
        p.utilities.push_back (act ("Tap", 28, 127, oneTap, 1));
        addValues (p.utilities, 29, { { "Series + pitch fb", 0 }, { "Series", 63 }, { "Dual + cross fb", 95 }, { "Dual", 127 } }, "Delay mode (CC#29).", 6);
        p.models = { ranges ("Key", 16, { { "C", 0, 2 }, { "Db", 3, 11 }, { "D", 12, 25 }, { "Eb", 26, 37 }, { "E", 38, 47 }, { "F", 48, 58 },
                                          { "Gb", 59, 66 }, { "G", 67, 77 }, { "Ab", 78, 87 }, { "A", 88, 97 }, { "Bb", 98, 108 }, { "B", 109, 119 },
                                          { "Chromatic", 120, 127 } }, "Key (CC#16).", 3),
                     ranges ("Scale", 22, { { "Major", 0, 11 }, { "Minor", 12, 37 }, { "Melodic minor", 38, 58 }, { "Harmonic minor", 59, 78 },
                                            { "Double harmonic", 79, 97 }, { "Lydian penta", 98, 119 }, { "Minor penta", 120, 127 } }, "Scale type (CC#22).", 4) };
        p.pedals = knobs ({ { "Expression", 4 }, { "Tempo", 15 }, { "Micro tune", 17 }, { "Mix", 18 }, { "Pitch 1", 19 }, { "Pitch 2", 20 },
                            { "Pitch 3", 21 }, { "Correction / glide", 23 }, { "Feedback", 24 }, { "Time div 1", 25 }, { "Time div 2", 26 },
                            { "Time div 3", 27 } });
        p.notes = merisLegacyNotes + "\n\nControls: Effect (CC#14), half speed (CC#9), pitch smoothing (CC#30), volume swell (CC#31), tap "
                  "(CC#28), delay mode (CC#29). Models: key (CC#16) and scale (CC#22). Pitch 1-3 (CC#19-21) depend on the key: chromatic, "
                  "diatonic or pentatonic tables in the manual. MIDI notes tune the pitch voices. Syncs to MIDI clock.";
        v.push_back (p);
    }

    // ---------------------------------------------------------------- Meris X series
    {
        auto p = merisX ("meris.lvx", "LVX", "LVX", "lvx modular delay looper", "LVX v1.5.1", 121);
        p.padCc = 106;   // CC#32 is a delay parameter here; CC#105-116 aren't in the LVX table
        p.switches.push_back ({ "Half speed", 119, 127, 0 });
        p.utilities.push_back (act ("Tap", 99, 127, oneTap, 1));
        p.looper = { act ("Record/dub", 100, 127, "Looper record / overdub press (CC#100).", 0), act ("Play/stop", 101, 127, "Looper play / stop press (CC#101).", 3),
                     act ("Looper FX 1", 102, 127, "Looper FX 1 press (CC#102).", 6), act ("Looper FX 2", 103, 127, "Looper FX 2 press (CC#103).", 7),
                     act ("Quantize on", 104, 127, "Looper quantize on (CC#104).", 4), act ("Quantize off", 104, 0, "Looper quantize off (CC#104).", 9) };
        const juce::String typeNote ("Changes the loaded preset's block type.");
        p.models = { ranges ("Delay structure", 13, { { "Standard", 0, 21 }, { "Multitap", 22, 42 }, { "Multifilter", 43, 63 }, { "Poly", 64, 85 },
                                                      { "Reverse", 86, 106 }, { "Series", 107, 127 } }, typeNote, 3),
                     ranges ("Delay type", 16, { { "Digital", 0, 42 }, { "BBD", 43, 85 }, { "Tape", 86, 127 } }, typeNote, 4),
                     ranges ("Preamp", 5, { { "Preamp off", 0, 18 }, { "Volume pedal", 19, 36 }, { "Tube", 37, 54 }, { "Transistor", 55, 73 },
                                            { "Op-Amp", 74, 91 }, { "Drive", 92, 109 }, { "Bitcrusher", 110, 127 } }, typeNote, 0),
                     ranges ("Dynamics", 62, { { "Dynamics off", 0, 25 }, { "Compressor", 26, 51 }, { "Swell", 52, 76 }, { "Diffusion", 77, 102 },
                                               { "Limiter", 103, 127 } }, typeNote, 1),
                     ranges ("Pitch", 70, { { "Pitch off", 0, 21 }, { "Poly Chroma", 22, 42 }, { "Harmony", 43, 63 }, { "Micro Tune", 64, 85 },
                                            { "Mono Chroma", 86, 106 }, { "Lo-Fi", 107, 127 } }, typeNote, 5),
                     ranges ("Filter", 78, { { "Filter off", 0, 25 }, { "Ladder", 26, 51 }, { "State Var", 52, 76 }, { "Comb", 77, 102 },
                                             { "Parametric", 103, 127 } }, typeNote, 6),
                     ranges ("Modulation", 86, { { "Mod off", 0, 15 }, { "Chorus", 16, 31 }, { "Flanger", 32, 47 }, { "Dyn Flanger", 48, 63 },
                                                 { "Cassette", 64, 79 }, { "Barberpole", 80, 95 }, { "Granulize", 96, 111 }, { "Ring Mod", 112, 127 } }, typeNote, 7) };
        p.pedals = knobs ({ { "Expression", 4 }, { "Mix", 1 }, { "Time", 15 }, { "Feedback", 19 }, { "Cross feedback", 20 }, { "Delay mod", 21 },
                            { "Dry trim", 2 }, { "Wet trim", 3 }, { "Looper level", 95 }, { "Looper feedback", 96 } });
        p.pedalNote = "Expression (CC#4) moves the preset's expression assignments. The others move that control like turning it.";
        p.notes = merisXNotes ("Controls: Effect (CC#14), half speed (CC#119), tap (CC#99), tuner (CC#117), the preset's MIDI clock (CC#121). "
                               "Looper: CC#100-104 presses and quantize. Models: delay structure (CC#13), delay type (CC#16) and the preamp, "
                               "dynamics, pitch, filter and modulation block types.");
        v.push_back (p);
    }
    {
        auto p = merisX ("meris.mercuryx", "MercuryX", "MercuryX", "mercury x mercuryx modular reverb", "MercuryX v1.5", 28);
        p.padCc = 50;   // CC#32 is the reverb structure; CC#46-61 aren't in the MercuryX table
        const juce::String typeNote ("Changes the loaded preset's block type.");
        p.models = { ranges ("Reverb", 32, { { "Ultraplate", 0, 15 }, { "Cathedra", 16, 31 }, { "Spring", 32, 47 }, { "78 Room", 48, 63 },
                                             { "78 Plate", 64, 79 }, { "78 Hall", 80, 95 }, { "Prism", 96, 111 }, { "Gravity", 112, 127 } }, typeNote, 3),
                     ranges ("Delay", 16, { { "Digital", 0, 42 }, { "BBD", 43, 85 }, { "Magnetic", 86, 127 } }, typeNote, 4),
                     ranges ("Delay structure", 13, { { "Standard", 0, 63 }, { "Reverse", 64, 127 } }, typeNote, 4),
                     ranges ("Preamp", 5, { { "Preamp off", 0, 25 }, { "Volume pedal", 26, 51 }, { "Tube", 52, 76 }, { "Transistor", 77, 102 },
                                            { "Op-Amp", 103, 127 } }, typeNote, 0),
                     ranges ("Dynamics", 62, { { "Dynamics off", 0, 15 }, { "Compressor", 16, 31 }, { "Comp link", 32, 47 }, { "Swell", 48, 63 },
                                               { "Diffusion", 64, 79 }, { "Limiter", 80, 95 }, { "Limiter link", 96, 111 }, { "Freeze", 112, 127 } }, typeNote, 1),
                     ranges ("Pitch", 70, { { "Pitch off", 0, 31 }, { "Poly Chroma", 32, 63 }, { "Micro Shift", 64, 95 }, { "Lo-Fi", 96, 127 } }, typeNote, 5),
                     ranges ("Filter", 78, { { "Filter off", 0, 31 }, { "Ladder", 32, 63 }, { "State Var", 64, 95 }, { "Parametric", 96, 127 } }, typeNote, 6),
                     ranges ("Modulation", 86, { { "Mod off", 0, 21 }, { "Chorus", 22, 42 }, { "Vibrato", 43, 63 }, { "Vowel Mod", 64, 85 },
                                                 { "Tremolo", 86, 106 }, { "Hazy", 107, 127 } }, typeNote, 7) };
        p.pedals = knobs ({ { "Expression", 4 }, { "Mix", 1 }, { "Time", 15 }, { "Feedback", 19 }, { "Modulation", 21 }, { "Damping", 22 },
                            { "Dry blend", 23 }, { "Predelay blend", 42 }, { "Dry trim", 2 }, { "Wet trim", 3 } });
        for (int i = 1; i <= 9; ++i)
            p.pedals.push_back ({ "Reverb param " + n (i), 32 + i });   // CC#33-41
        p.pedalNote = "Expression (CC#4) moves the preset's expression assignments. Reverb params 1-9 (CC#33-41) depend on the reverb structure.";
        p.notes = merisXNotes ("Controls: Effect (CC#14), tuner (CC#117), the preset's MIDI clock (CC#28). There's no tap CC (tap is done with "
                               "the hold modifier) and no looper. Models: reverb structure (CC#32), delay type and structure, and the preamp, "
                               "dynamics, pitch, filter and modulation block types. Half speed (CC#24) has no thresholds in the table, so it "
                               "isn't a switch.");
        v.push_back (p);
    }
    {
        auto p = merisX ("meris.enzo-x", "Enzo X", "Enzo X", "enzo x synth synthesizer arp poly", "Enzo X v1.5.1", 21);
        p.padCc = 70;   // CC#32 is the arp mode; CC#63-82 aren't in the Enzo X table
        p.switches.push_back ({ "Half speed", 12, 127, 0 });
        p.switches.push_back ({ "Arp latch", 37, 127, 0 });
        p.utilities.push_back (act ("Tap", 99, 127, oneTap, 1));
        const juce::String typeNote ("Changes the loaded preset's setting.");
        p.models = { ranges ("Synth mode", 22, { { "Mono synth", 0, 25 }, { "Poly synth", 26, 51 }, { "Arp synth", 52, 76 }, { "Dry mono", 77, 102 },
                                                 { "Dry poly", 103, 127 } }, typeNote, 3),
                     ranges ("Filter", 38, { { "Ladder", 0, 42 }, { "State Variable", 43, 85 }, { "Twin", 86, 127 } }, typeNote, 6),
                     ranges ("Filter shape", 40, { { "Lowpass", 0, 42 }, { "Bandpass", 43, 85 }, { "Highpass", 86, 127 } }, typeNote, 6),
                     ranges ("Drive", 5, { { "Drive off", 0, 21 }, { "Volume pedal", 22, 42 }, { "Tube", 43, 63 }, { "Transistor", 64, 85 },
                                           { "Op-Amp", 86, 106 }, { "Bitcrusher", 107, 127 } }, typeNote, 0),
                     ranges ("Ambience", 10, { { "Ambience off", 0, 25 }, { "Echo", 26, 51 }, { "Small", 52, 76 }, { "Medium", 77, 102 },
                                               { "Large", 103, 127 } }, typeNote, 4),
                     ranges ("Modulation", 86, { { "Mod off", 0, 21 }, { "Chorus", 22, 42 }, { "Flanger", 43, 63 }, { "Vibrato", 64, 85 },
                                                 { "Phaser", 86, 106 }, { "Ring Mod", 107, 127 } }, typeNote, 7) };
        p.pedals = knobs ({ { "Expression", 4 }, { "Synth pitch", 23 }, { "Filter freq", 39 }, { "Resonance", 41 }, { "Filter env amount", 44 },
                            { "Glide", 28 }, { "Mix", 60 }, { "Ambience mix", 19 }, { "Feedback / decay", 11 }, { "Time", 15 },
                            { "Mod speed", 88 }, { "Mod depth", 89 }, { "Osc 1 gain", 29 }, { "Osc 2 gain", 30 } });
        p.pedalNote = "Expression (CC#4) moves the preset's expression assignments. The others move that control like turning it.";
        p.notes = merisXNotes ("Controls: Effect (CC#14; bypassing also silences stuck notes), half speed (CC#12), arp latch (CC#37), tap "
                               "(CC#99), tuner (CC#117), the preset's MIDI clock (CC#21). Models: synth mode (CC#22), filter type and shape, "
                               "drive, ambience and modulation types. The pedal also plays MIDI notes in the synth modes.");
        v.push_back (p);
    }
    {
        auto p = merisX ("meris.ottobit-x", "Ottobit X", "Ottobit X", "ottobit x bitcrusher glitch stutter", "Ottobit X v1.1.1 Full Manual", 21);
        p.padCc = 85;   // CC#32 is the mod speed; CC#79-98 aren't in the Ottobit X table
        p.switches.push_back ({ "Bit scaling", 28, 127, 0 });
        p.switches.push_back ({ "Glitch quantize", 78, 127, 0 });
        p.utilities.push_back (act ("Tap", 99, 127, oneTap, 1));
        const juce::String typeNote ("Changes the loaded preset's block type.");
        p.models = { ranges ("Glitch", 62, { { "Glitch off", 0, 18 }, { "Freeze", 19, 36 }, { "Stutter", 37, 54 }, { "Push Loop", 55, 73 },
                                             { "Wikki Wikki", 74, 91 }, { "Tape Stop", 94, 109 }, { "Stutter Step", 110, 127 } }, typeNote, 0),
                     ranges ("Modulation", 30, { { "Mod off", 0, 21 }, { "Ring Mod", 22, 42 }, { "Div Trem", 43, 63 }, { "Tape Mod", 64, 85 },
                                                 { "Freq Shift", 86, 106 }, { "Vibe", 107, 127 } }, typeNote, 7),
                     ranges ("Pitch", 46, { { "Pitch off", 0, 21 }, { "Poly Chroma", 22, 42 }, { "Otto Tune", 43, 63 }, { "Micro Tune", 64, 85 },
                                            { "Mono Chroma", 86, 106 }, { "Lo-Fi", 107, 127 } }, typeNote, 5),
                     ranges ("Filter", 54, { { "Ladder", 0, 42 }, { "State Variable", 43, 85 }, { "Otto Tron", 86, 127 } }, typeNote, 6),
                     ranges ("Ambience", 10, { { "Ambience off", 0, 42 }, { "VHS Delay", 43, 85 }, { "VHS Verb", 86, 127 } }, typeNote, 4),
                     ranges ("Preamp", 5, { { "Preamp off", 0, 25 }, { "Volume pedal", 26, 51 }, { "Tube", 52, 76 }, { "Vinyl", 77, 102 },
                                            { "Wavefold", 103, 127 } }, typeNote, 1) };
        p.pedals = knobs ({ { "Expression", 4 }, { "Mix", 1 }, { "Sample rate", 22 }, { "Bits", 23 }, { "Time", 15 }, { "Feedback / decay", 11 },
                            { "Ambience mix", 19 }, { "Filter freq", 56 }, { "Resonance", 57 }, { "Glitch mix", 69 }, { "Mod depth", 33 },
                            { "Dry trim", 2 }, { "Wet trim", 3 } });
        p.pedalNote = "Expression (CC#4) moves the preset's expression assignments. The others move that control like turning it.";
        p.notes = merisXNotes ("Controls: Effect (CC#14), bit scaling (CC#28), glitch quantize (CC#78), tap (CC#99), tuner (CC#117), the preset's "
                               "MIDI clock (CC#21). Models: glitch, modulation, pitch, filter, ambience and preamp types (the manual's glitch "
                               "ranges overlap at 92-93, so Wikki Wikki and Tape Stop avoid those values). Key and scale (CC#51/52) have no "
                               "value table.");
        v.push_back (p);
    }

    // ---------------------------------------------------------------- Chase Bliss
    {
        auto p = chaseBliss ("chasebliss.mood-mkii", "MOOD MKII", "MOOD", "mood mkii mk2 mood2 micro looper reverb delay slip",
                             "MOOD MKII MIDI Manual");
        p.switches = { { "Ring bypass", 102, 127, 0 }, { "Droplet bypass", 103, 127, 0 }, { "Freeze", 105, 127, 0 }, { "Overdub", 106, 127, 0 } };
        p.switchesNote = "Ring = the micro-looper side, droplet = the wet side. " + cbSwitchNote;
        p.utilities.push_back (act ("Tap", 93, 127, oneTap + " Any value above 0 is a tap.", 1));
        addToggle (p.utilities, 21, "Reverb", "Delay", "Slip", "Wet channel (CC#21).", 6);
        addToggle (p.utilities, 22, "Routing: in", "Routing: ring+in", "Routing: ring", "Routing (CC#22).", 4);
        addToggle (p.utilities, 23, "Env", "Tape", "Stretch", "Micro-looper mode (CC#23).", 7);
        addToggle (p.utilities, 31, "Sync ring>drop", "No sync", "Sync drop>ring", "Sync (hidden option, CC#31).", 2);
        addToggle (p.utilities, 32, "Spread: droplet", "Spread: both", "Spread: ring", "Spread (hidden option, CC#32).", 2);
        p.utilities.push_back (act ("Half buffer", 33, 0, "Buffer length (CC#33): half, like the MKI.", 8));
        p.utilities.push_back (act ("Full buffer", 33, 127, "Buffer length (CC#33): full.", 8));
        p.utilities.push_back (act ("Stop ramping", 52, 0, "Stops the ramping (CC#52 = 0).", 9));
        p.utilities.push_back (act ("Resume ramping", 52, 127, "Resumes the ramping (CC#52 above 0).", 3));
        p.pedals = knobs ({ { "Expression", 100 }, { "Time", 14 }, { "Mix", 15 }, { "Length", 16 }, { "Modify (droplet)", 17 }, { "Clock", 18 },
                            { "Modify (ring)", 19 }, { "Ramp speed", 20 }, { "Stereo width", 24 }, { "Fade", 26 }, { "Tone", 27 },
                            { "Level balance", 28 }, { "Direct micro-loop", 29 } });
        p.notes = cbNotes ("Controls: the ring and droplet bypasses (CC#102/103), freeze (CC#105), overdub (CC#106), tap (CC#93), the toggles "
                           "(CC#21-23) and hidden options (CC#31-33), ramping (CC#52).\n\nCC#0 is Pitch Bend and CC#1 the mod wheel on this "
                           "pedal (Synth Mode, entered by any MIDI note), so these clips never send them; the Ableton padding uses CC#40.");
        v.push_back (p);
    }
    {
        auto p = chaseBliss ("chasebliss.blooper", "Blooper", "Blooper", "blooper looper", "Blooper MIDI Configuration");
        p.presetCount = 16; p.labelFrom = 1; p.pcOffset = 0;   // zero-based: PC 0-15 = loops 1-16
        p.utilities.clear();                                    // no Live mode: PC 0 is loop 1
        p.testMessage = "PC 0";
        p.switches = { { "Ramping", 52, 127, 0 } };
        p.looper = { act ("Record", 11, 1, "CC#11 = 1.", 0), act ("Play", 11, 2, "CC#11 = 2.", 3), act ("Overdub", 11, 3, "CC#11 = 3.", 1),
                     act ("Stop", 11, 4, "CC#11 = 4.", 9), act ("Undo", 11, 5, "CC#11 = 5.", 6), act ("Redo", 11, 6, "CC#11 = 6.", 6) };
        p.utilities = { act ("Mod A on/off", 30, 127, anyToggle, 7), act ("Mod B on/off", 31, 127, anyToggle, 7) };
        addToggle (p.utilities, 21, "Mod A left", "Mod A center", "Mod A right", "Mod A toggle (CC#21).", 4);
        addToggle (p.utilities, 22, "Mode left", "Mode center", "Mode right", "Looper mode toggle (CC#22).", 6);
        addToggle (p.utilities, 23, "Mod B left", "Mod B center", "Mod B right", "Mod B toggle (CC#23).", 4);
        p.utilities.push_back (act ("Clock listen", 51, 127, "Follows MIDI clock (CC#51 above 0).", 8));
        p.utilities.push_back (act ("Clock ignore", 51, 0, "Ignores MIDI clock (CC#51 = 0).", 9));
        p.models = { group ("Note division", 54, { { "Whole", 0 }, { "Half", 1 }, { "Dotted", 2 }, { "Quarter", 3 }, { "Eighth", 4 },
                                                   { "Triplet", 5 }, { "Sixteenth", 6 }, { "32nd", 7 } }, "Note division (CC#54).", 5) };
        p.pedals = knobs ({ { "Expression", 100 }, { "Volume", 14 }, { "Layers", 15 }, { "Repeats", 16 }, { "Mod A", 17 }, { "Stability", 18 },
                            { "Mod B", 19 }, { "Ramp", 20 } });
        p.notes = "Presets: 16 loop slots, two banks of 8 (blue / red LEDs). Program Change 0-15 = loops 1-16 (zero-based: \"Preset 1\" here "
                  "is loop 1 = PC 0), in loop mode. Saving (PC while holding the right footswitch) isn't a tile.\n\nLooper: CC#11 = 1-6 "
                  "(record, play, overdub, stop, undo, redo), one message per action. The chart also has CC#1-9 (0 off, above 0 on) and "
                  "CC#11 = 7 (erase); erase is left out on purpose. CC#1 is Record on this pedal, not the mod wheel.\n\nControls: Mod A / B "
                  "on/off (CC#30/31, any value toggles), the three toggles (CC#21-23), ramping (CC#52), MIDI clock (CC#51), note division "
                  "(CC#54). No tap or bypass CC in the chart.\n\nConnection: MIDIBox \"Ring Active\" port, standard 1/4\" TRS cable to the "
                  "TAP/MIDI jack. No MIDI Out or Thru documented. MIDI channel 2 out of the box (hold both footswitches at power-up, then "
                  "send a Program Change on the new channel).";
        v.push_back (p);
    }
    {
        auto p = chaseBliss ("chasebliss.dark-world", "Dark World", "Dark World", "dark world reverb dual", "Dark World MIDI Configuration");
        p.switches = { { "Bypass switch", 102, 127, 0 } };
        p.switchesNote = "On (127) engages the last saved bypass state, Off (0) bypasses.";
        addValues (p.utilities, 103, { { "Dark + World", 127 }, { "Dark only", 85 }, { "World only", 45 }, { "Both off", 0 } },
                   "Which channels are engaged (CC#103).", 3);
        addToggle (p.utilities, 21, "Dark left", "Dark middle", "Dark right", "Dark program toggle (CC#21).", 6);
        addToggle (p.utilities, 22, "Para left", "Para middle", "Para right", "Routing toggle (CC#22).", 4);
        addToggle (p.utilities, 23, "World left", "World middle", "World right", "World program toggle (CC#23).", 7);
        p.pedals = knobs ({ { "Expression", 100 }, { "Decay", 14 }, { "Mix", 15 }, { "Dwell", 16 }, { "Modify", 17 }, { "Tone", 18 }, { "Pre-delay", 19 } });
        p.notes = cbNotes ("Controls: bypass (CC#102), channels (CC#103: 127 both, 85 Dark only, 45 World only, 0 both bypassed), the three "
                           "toggles (CC#21-23, left / middle / right; the MIDI chart doesn't name them, the panel shows MOD / SHIM / BLACK, "
                           "PARA / D>W / W>D and SPRING / PLATE / HALL). Tap, clock and dip switch CCs aren't in this chart.");
        v.push_back (p);
    }
    {
        auto p = chaseBliss ("chasebliss.thermae", "Thermae", "Thermae", "thermae analog delay pitch", "Thermae MIDI Configuration", "TAP + BYPASS");
        p.switches = { { "Bypass", 102, 127, 0 }, { "Hold", 24, 127, 0 }, { "Slowdown", 25, 127, 0 } };
        p.switchesNote = "Bypass On engages (127 while engaged also resets any ramping). Hold On = runaway oscillation, Off resumes. "
                         "Slowdown (sequence mode) 127 / 0.";
        p.utilities.push_back (act ("Tap", 93, 127, oneTap, 1));
        addToggle (p.utilities, 21, "L left", "L middle", "L right", "Left toggle (CC#21); what it does depends on the mode.", 6);
        addToggle (p.utilities, 22, "M left", "M middle", "M right", "Middle toggle (CC#22); what it does depends on the mode.", 4);
        addToggle (p.utilities, 23, "R left", "R middle", "R right", "Right toggle (CC#23); what it does depends on the mode.", 7);
        p.utilities.push_back (act ("Clock listen", 51, 127, "Listens to MIDI clock (CC#51 = 127, the default).", 8));
        p.utilities.push_back (act ("Clock ignore", 51, 0, "Ignores MIDI clock (CC#51 = 0).", 9));
        p.pedals = knobs ({ { "Expression", 100 }, { "Mix", 14 }, { "LPF", 15 }, { "Regen", 16 }, { "Glide", 17 }, { "Int 1 (speed)", 18 },
                            { "Int 2 (depth)", 19 }, { "Ramp", 20 } });
        p.notes = cbNotes ("Controls: bypass (CC#102), hold (CC#24), slowdown (CC#25), tap (CC#93, any value), the L / M / R toggles "
                           "(CC#21-23), MIDI clock listen / ignore (CC#51). Follows MIDI clock (24 ppqn).", "TAP + BYPASS");
        v.push_back (p);
    }
    {
        auto p = chaseBliss ("chasebliss.habit", "Habit", "Habit", "habit echo collector memory delay", "Habit MIDI Manual");
        p.switches = { { "Bypass", 102, 127, 0 }, { "Loop", 24, 127, 0 }, { "Scan", 25, 127, 0 } };
        p.utilities.push_back (act ("Tap", 93, 127, oneTap, 1));
        addToggle (p.utilities, 21, "Mod number 1", "Mod number 2", "Mod number 3", "Mod number toggle (CC#21).", 6);
        addToggle (p.utilities, 22, "Mod bank 1", "Mod bank 2", "Mod bank 3", "Mod bank toggle (CC#22).", 4);
        addToggle (p.utilities, 23, "Mode 1", "Mode 2", "Mode 3", "Mode toggle (CC#23).", 7);
        p.utilities.push_back (act ("Clock listen", 51, 127, "Listens to MIDI clock (CC#51 above 0).", 8));
        p.utilities.push_back (act ("Clock ignore", 51, 0, "Ignores MIDI clock (CC#51 = 0).", 9));
        p.pedals = knobs ({ { "Expression", 100 }, { "Volume", 14 }, { "Repeats", 15 }, { "Size", 16 }, { "Mod", 17 }, { "Spread", 18 },
                            { "Scan", 19 }, { "Ramp", 20 } });
        p.notes = cbNotes ("Controls: bypass (CC#102), loop (CC#24, the right hold), scan (CC#25, the left hold), tap (CC#93), the three toggles "
                           "(CC#21-23), MIDI clock (CC#51). Clear (CC#26) is left out on purpose.", "TAP + BYPASS");
        v.push_back (p);
    }
    {
        auto p = automatone ("chasebliss.cxm-1978", "CXM 1978", "CXM", "cxm 1978 automatone reverb faders", "CXM 1978 MIDI Configuration");
        p.switches = { { "Bypass", 102, 127, 0 }, { "Sustain", 31, 0, 127 } };   // aux switch 4: 0 = sustain on, 1 or more = off
        p.switchesNote = "Bypass On engages (127), Off bypasses (0). Sustain (aux switch 4, CC#31): On sends 0, Off sends 127.";
        addValues (p.utilities, 22, { { "Jump off", 1 }, { "Jump 0", 2 }, { "Jump 5", 3 } }, "Jump (CC#22).", 8);
        addValues (p.utilities, 23, { { "Room", 1 }, { "Plate", 2 }, { "Hall", 3 } }, "Reverb type (CC#23).", 6);
        addValues (p.utilities, 24, { { "Diffusion low", 1 }, { "Diffusion med", 2 }, { "Diffusion high", 3 } }, "Diffusion (CC#24).", 4);
        addValues (p.utilities, 25, { { "Tank mod low", 1 }, { "Tank mod med", 2 }, { "Tank mod high", 3 } }, "Tank mod (CC#25).", 7);
        addValues (p.utilities, 26, { { "HiFi", 1 }, { "Standard", 2 }, { "LoFi", 3 } }, "Clock (CC#26).", 5);
        for (int i = 1; i <= 3; ++i)
            p.utilities.push_back (act ("Aux " + n (i), 27 + i, 127, "Aux performance switch " + n (i) + " (CC#" + n (27 + i) + ", any value triggers it).", 2));
        p.pedals = knobs ({ { "Expression", 100 }, { "Bass", 14 }, { "Mids", 15 }, { "Cross", 16 }, { "Treble", 17 }, { "Mix", 18 }, { "Pre-delay", 19 } });
        p.notes = automatoneNotes ("Controls: bypass (CC#102), the arcade buttons (CC#22-26: jump, type, diffusion, tank mod, clock), aux "
                                   "performance switches 1-3 (CC#28-30, any value) and sustain (CC#31). Faders CC#14-19 in Expression.");
        v.push_back (p);
    }
    {
        auto p = automatone ("chasebliss.preamp-mkii", "Preamp MKII", "Preamp", "preamp mkii mk2 automatone benson faders", "Preamp MKII MIDI Configuration");
        p.switches = { { "Bypass", 102, 127, 0 } };
        addValues (p.utilities, 22, { { "Jump off", 1 }, { "Jump 0", 2 }, { "Jump 5", 3 } }, "Jump (CC#22).", 8);
        addValues (p.utilities, 23, { { "Mids off", 1 }, { "Mids pre", 2 }, { "Mids post", 3 } }, "Mids (CC#23).", 6);
        addValues (p.utilities, 24, { { "Q low", 1 }, { "Q mid", 2 }, { "Q high", 3 } }, "Q (CC#24).", 4);
        addValues (p.utilities, 25, { { "Diode off", 1 }, { "Diode silicon", 2 }, { "Diode germanium", 3 } }, "Diode (CC#25).", 7);
        addValues (p.utilities, 26, { { "Fuzz off", 1 }, { "Fuzz open", 2 }, { "Fuzz gated", 3 } }, "Fuzz (CC#26).", 0);
        p.pedals = knobs ({ { "Expression", 100 }, { "Volume", 14 }, { "Treble", 15 }, { "Mids", 16 }, { "Freq", 17 }, { "Bass", 18 }, { "Gain", 19 } });
        p.notes = automatoneNotes ("Controls: bypass (CC#102), the arcade buttons (CC#22-26: jump, mids, Q, diode, fuzz). Sliders CC#14-19 in "
                                   "Expression.");
        v.push_back (p);
    }
    {
        auto p = chaseBliss ("chasebliss.gen-loss-mkii", "Generation Loss MKII", "Gen Loss", "generation loss gen loss mkii mk2 tape vhs wow flutter",
                             "Generation Loss MKII MIDI Manual (2022-GEN02)");
        p.switches = { { "Bypass", 102, 127, 0 }, { "Aux", 103, 127, 0 }, { "Aux left", 105, 127, 0 }, { "Aux center", 106, 127, 0 },
                       { "Aux right", 107, 127, 0 }, { "Ramp/bounce", 52, 127, 0 } };
        addValues (p.utilities, 32, { { "Line level", 1 }, { "Instrument", 2 }, { "High gain", 3 } }, "Input gain (CC#32).", 2);
        p.utilities.push_back (act ("True bypass", 26, 0, "Bypass mode (CC#26 below 64): true bypass.", 8));
        p.utilities.push_back (act ("DSP bypass", 26, 127, "Bypass mode (CC#26 above 64): DSP bypass.", 8));
        addToggle (p.utilities, 21, "Aux toggle 1", "Aux toggle 2", "Aux toggle 3", "Aux toggle (CC#21).", 6);
        addToggle (p.utilities, 22, "Dry 1", "Dry 2", "Dry 3", "Dry toggle (CC#22).", 4);
        addToggle (p.utilities, 23, "Noise 1", "Noise 2", "Noise 3", "Noise toggle (CC#23).", 7);
        p.models = { group ("Models", 16, { { "No model", 0 }, { "CPR-3300 Gen 1", 15 }, { "CPR-3300 Gen 2", 24 }, { "CPR-3300 Gen 3", 33 },
                                            { "Portamax-RT", 43 }, { "Portamax-HT", 53 }, { "CAM-8", 62 }, { "Dictatron-EX", 72 },
                                            { "Dictatron-IN", 82 }, { "Fishy 60", 91 }, { "MS-Walker", 101 }, { "AMU-2", 111 }, { "M-PEX", 127 } },
                            "Picks the tape model (CC#16, the Model knob).", 3) };
        p.pedals = knobs ({ { "Expression", 100 }, { "Wow", 14 }, { "Volume", 15 }, { "Model / LP", 16 }, { "Flutter", 17 }, { "Saturate / GEN", 18 },
                            { "Failure / HP", 19 }, { "Ramp speed", 20 }, { "Aux onset", 24 }, { "Hiss", 27 }, { "Mechanical noise", 28 },
                            { "Crinkle & pop", 29 } });
        p.notes = cbNotes ("Controls: bypass (CC#102), aux (CC#103), the aux switch left / center / right (CC#105-107), ramp / bounce (CC#52), "
                           "input gain (CC#32 = 1 line, 2 instrument, 3 high gain), bypass mode (CC#26), the three toggles (CC#21-23). "
                           "Models: CC#16 values from the manual.\n\nCC#32 is the input gain here, so the Ableton padding uses CC#40, never "
                           "a bank select. Tap and MIDI clock CCs aren't in this manual.");
        v.push_back (p);
    }
    {
        auto p = chaseBliss ("chasebliss.lossy", "Lossy", "Lossy", "lossy goodhertz lofi mp3 freeze", "Lossy MIDI Manual");
        p.switches = { { "Bypass", 102, 127, 0 }, { "Freeze slushie", 103, 127, 0 }, { "Freeze solid", 105, 127, 0 }, { "Gate", 106, 127, 0 },
                       { "Dry kill", 57, 127, 0 }, { "Ramp/bounce", 52, 127, 0 } };
        addToggle (p.utilities, 21, "Slope 6 dB", "Slope 24 dB", "Slope 96 dB", "Filter slope (CC#21).", 6);
        addToggle (p.utilities, 22, "Packet repeat", "Packet clean", "Packet loss", "Packet mode (CC#22).", 4);
        addToggle (p.utilities, 23, "Loss inverse", "Loss standard", "Loss jitter", "Loss mode (CC#23).", 7);
        addToggle (p.utilities, 33, "Weighting dark", "Weighting neutral", "Weighting bright", "Weighting (hidden, CC#33).", 2);
        p.pedals = knobs ({ { "Expression", 100 }, { "Filter", 14 }, { "Global", 15 }, { "Reverb", 16 }, { "Freq", 17 }, { "Speed", 18 },
                            { "Loss", 19 }, { "Ramp speed", 20 }, { "Gate", 24 }, { "Freezer", 25 }, { "Verb decay", 26 },
                            { "Limiter threshold", 27 }, { "Auto gain", 28 }, { "Loss gain", 29 } });
        p.notes = cbNotes ("Controls: bypass (CC#102), freeze slushie / solid (CC#103/105), gate (CC#106), dry kill (CC#57), ramp / bounce "
                           "(CC#52), filter slope, packet and loss mode (CC#21-23), weighting (CC#33). No tempo, so no tap or clock CCs.");
        v.push_back (p);
    }
    {
        auto p = chaseBliss ("chasebliss.brothers-am", "Brothers AM", "Brothers AM", "brothers am analogman drive boost overdrive",
                             "Brothers AM MIDI Manual (2025-BAM01)");
        p.switches = { { "Channel 1", 102, 127, 0 }, { "Channel 2", 103, 127, 0 } };
        addToggle (p.utilities, 21, "Gain 2 boost", "Gain 2 OD", "Gain 2 dist", "Gain 2 type (CC#21).", 6);
        addToggle (p.utilities, 22, "Full sun", "Treble off", "Half sun", "Treble boost (CC#22).", 2);
        addToggle (p.utilities, 23, "Gain 1 dist", "Gain 1 OD", "Gain 1 boost", "Gain 1 type (CC#23).", 4);
        p.pedals = knobs ({ { "Expression", 100 }, { "Gain 1", 16 }, { "Volume 1", 18 }, { "Tone 1", 19 }, { "Presence 1", 29 },
                            { "Gain 2", 14 }, { "Volume 2", 15 }, { "Tone 2", 17 }, { "Presence 2", 27 } });
        p.notes = cbNotes ("Controls: channel 1 / 2 bypass (CC#102/103), the gain 2 type, treble boost and gain 1 type toggles (CC#21-23).");
        v.push_back (p);
    }
    {
        auto p = chaseBliss ("chasebliss.onward", "Onward", "Onward", "onward glitch freeze sampler", "Onward MIDI Manual");
        p.switches = { { "Freeze", 102, 127, 0 }, { "Glitch", 103, 127, 0 }, { "Glitch hold", 105, 127, 0 }, { "Freeze hold", 106, 127, 0 },
                       { "Dry kill", 57, 127, 0 }, { "Trails", 58, 127, 0 }, { "Ramp/bounce", 52, 127, 0 } };
        p.switchesNote = "Freeze and Glitch are the two bypasses (CC#102/103). " + cbSwitchNote;
        p.utilities.push_back (act ("Tap", 93, 127, oneTap, 1));
        p.utilities.push_back (act ("Retrigger glitch", 108, 127, "Retriggers the glitch (CC#108, any value).", 0));
        p.utilities.push_back (act ("Retrigger freeze", 109, 127, "Retriggers the freeze (CC#109, any value).", 5));
        addToggle (p.utilities, 21, "Error timing", "Error condition", "Error playback", "Error type (CC#21).", 6);
        addToggle (p.utilities, 22, "Fade long", "Fade user", "Fade short", "Fade (CC#22).", 4);
        addToggle (p.utilities, 23, "Vibrato", "Animate off", "Chorus", "Animate (CC#23).", 7);
        addToggle (p.utilities, 31, "Error: glitch", "Error: both", "Error: freeze", "Error routing (hidden, CC#31).", 2);
        addToggle (p.utilities, 32, "Sustain: glitch", "Sustain: both", "Sustain: freeze", "Sustain routing (hidden, CC#32).", 2);
        addToggle (p.utilities, 33, "Effects: glitch", "Effects: both", "Effects: freeze", "Effects routing (hidden, CC#33).", 2);
        p.utilities.push_back (act ("Clock follow", 51, 127, "Follows MIDI clock (CC#51 above 0).", 8));
        p.utilities.push_back (act ("Clock ignore", 51, 0, "Ignores MIDI clock (CC#51 = 0).", 9));
        p.models = { group ("Clock division", 53, { { "Whole", 0 }, { "Dotted half", 1 }, { "Half", 2 }, { "Dotted quarter", 3 }, { "Quarter", 4 },
                                                    { "Dotted 8th", 5 }, { "8th", 6 }, { "8th triplet", 7 }, { "16th", 8 } },
                            "MIDI sync subdivision (CC#53).", 5) };
        p.pedals = knobs ({ { "Expression", 100 }, { "Size", 14 }, { "Mix", 15 }, { "Octave", 16 }, { "Error", 17 }, { "Sustain", 18 },
                            { "Texture", 19 }, { "Ramp speed", 20 }, { "Sensitivity", 24 }, { "Balance", 25 }, { "Duck depth", 26 },
                            { "Error blend", 27 }, { "User fade", 28 }, { "Filter", 29 } });
        p.notes = cbNotes ("Controls: freeze / glitch (CC#102/103) and their holds (CC#105/106), retrigger (CC#108/109, any value), tap (CC#93), "
                           "dry kill (CC#57), trails (CC#58), ramp / bounce (CC#52), the toggles (CC#21-23), the routings (CC#31-33), MIDI clock "
                           "(CC#51) and its subdivision (CC#53).\n\nCC#32 is the sustain routing here, so the Ableton padding uses CC#40. The "
                           "channel is set by the first Program Change or CC after powering up with both footswitches held.");
        v.push_back (p);
    }
    {
        auto p = chaseBliss ("chasebliss.clean", "Clean", "Clean", "clean compressor dynamics swell", "Clean MIDI Manual");
        p.switches = { { "Bypass", 102, 127, 0 }, { "Swell", 103, 127, 0 }, { "Dynamics max", 106, 127, 0 }, { "Ramp/bounce", 52, 127, 0 } };
        addToggle (p.utilities, 21, "Release fast", "Release user", "Release slow", "Release (CC#21).", 6);
        addToggle (p.utilities, 22, "Shifty", "Manual", "Modulated", "Mode (CC#22).", 4);
        addToggle (p.utilities, 23, "Wobbly", "Physics off", "Twitchy", "Physics (CC#23).", 7);
        addToggle (p.utilities, 31, "Env analog", "Env hybrid", "Env adaptive", "Envelope mode (hidden, CC#31).", 2);
        addValues (p.utilities, 32, { { "ASR shifty", 1 }, { "ENV shifty", 3 } }, "Shifty mode (hidden, CC#32).", 2);
        addToggle (p.utilities, 33, "Spread: EQ", "Spread: both", "Spread: vol/comp", "Spread routing (hidden, CC#33).", 2);
        p.pedals = knobs ({ { "Expression", 100 }, { "Dynamics", 14 }, { "Sensitivity", 15 }, { "Wet", 16 }, { "Attack", 17 }, { "EQ", 18 },
                            { "Dry", 19 }, { "Ramp speed", 20 }, { "Gate release", 24 }, { "Gate sensitivity", 25 }, { "Swell in", 26 },
                            { "User release", 27 }, { "Balance filter", 28 }, { "Swell out", 29 } });
        p.notes = cbNotes ("Controls: bypass (CC#102), swell (CC#103), dynamics max (CC#106), ramp / bounce (CC#52), the toggles (CC#21-23) and "
                           "hidden options (CC#31-33; shifty mode lists only 1 = ASR and 3 = ENV). Tap and clock CCs aren't documented.\n\n"
                           "CC#32 is the shifty mode here, so the Ableton padding uses CC#40.");
        v.push_back (p);
    }

    // ---------------------------------------------------------------- Walrus Audio MAKO MKII
    {
        auto p = walrus ("walrus.mako-d1", "MAKO D1 MKII", "D1", "mako d1 mkii mk2 delay walrus", "MAKO D1 MKII manual (07_2022_D1_v2)");
        p.padCc = 40;   // CC#32 is the feedback mode; CC#40 isn't in the D1 chart (14-39, 89, 98)
        p.switches = { { "Effect", 29, 127, 0 } };
        p.utilities = { act ("Tap", 30, 127, oneTap, 1), act ("Ignore clock", 89, 127, "MIDI Clock Ignore on (CC#89 = 127).", 9),
                        act ("Follow clock", 89, 0, "MIDI Clock Ignore off (CC#89 = 0).", 8) };
        p.pedals = knobs ({ { "Time", 14 }, { "Repeats", 15 }, { "Mix", 20 }, { "Mod depth", 21 }, { "Tone", 22 }, { "Age", 23 }, { "Duck", 25 },
                            { "Spread", 98 }, { "Ramp", 36 }, { "Mod rate", 37 }, { "Grain size", 38 }, { "Grain mix", 39 } });
        p.pedalNote = "The D1 has no expression CC; these move that knob like turning it.";
        p.notes = walrusNotes ("Controls: effect on / bypass (CC#29), tap (CC#30), MIDI clock ignore (CC#89). The program (CC#24), divisions "
                               "(CC#28/31), feedback mode (CC#32), mod shape and grain pitch take numbers whose order the manual doesn't "
                               "state, so they aren't tiles. Accepts MIDI clock (send only a few pulses at a time).");
        v.push_back (p);
    }
    {
        auto p = walrus ("walrus.mako-r1", "MAKO R1 MKII", "R1", "mako r1 mkii mk2 reverb walrus", "R1 MKII Digital Manual");
        p.switches = { { "Effect", 30, 127, 0 }, { "Sustain", 31, 127, 0 }, { "Width", 35, 127, 0 } };
        p.pedals = knobs ({ { "Decay", 3 }, { "Pre delay", 9 }, { "Mix", 14 }, { "Rate", 15 }, { "Depth", 20 }, { "Swell", 21 }, { "Duck", 22 },
                            { "EQ low", 23 }, { "EQ high", 24 }, { "Size", 25 }, { "Diffuse", 26 }, { "Feedback EQ", 27 }, { "Octave type", 81 } });
        p.pedalNote = "The R1 has no expression CC; these move that control like turning it.";
        p.notes = walrusNotes ("Controls: effect on / bypass (CC#30), sustain (CC#31), width (CC#35). The program (CC#28, 0-5) has no value "
                               "order in the manual, so it isn't a tile. No tap CC.");
        v.push_back (p);
    }
    {
        auto p = walrus ("walrus.mako-m1", "MAKO M1 MKII", "M1", "mako m1 mkii mk2 modulation chorus phaser tremolo rotary walrus",
                         "M1 MKII Digital Manual");
        p.switches = { { "Effect", 31, 127, 0 }, { "Rotary fast", 86, 127, 0 } };
        p.utilities = { act ("Tap", 85, 127, oneTap, 1), act ("Ignore clock", 89, 127, "MIDI Clock Ignore on (CC#89 = 127).", 9),
                        act ("Follow clock", 89, 0, "MIDI Clock Ignore off (CC#89 = 0).", 8) };
        p.pedals = knobs ({ { "Rate", 3 }, { "Depth", 9 }, { "Tone", 19 }, { "Symmetry", 20 }, { "X", 21 }, { "Wet/dry mix", 104 },
                            { "Output gain", 88 }, { "Flange feedback", 105 }, { "Rotary slow rate", 106 }, { "Rotary fast rate", 107 },
                            { "Lo-fi mix", 14 }, { "Lo-fi env", 22 }, { "Lo-fi drive", 23 }, { "Lo-fi space", 24 }, { "Lo-fi noise", 26 },
                            { "Lo-fi warble", 27 } });
        p.pedalNote = "The M1 has no expression CC; these move that control like turning it.";
        p.notes = walrusNotes ("Controls: effect on / bypass (CC#31), rotary speed (CC#86), tap (CC#85), MIDI clock ignore (CC#89). Tap held "
                               "(CC#87) is a hold, so it isn't a tile. The program (CC#18), shape, division, type and lo-fi age have no value "
                               "order in the manual.");
        v.push_back (p);
    }
    {
        auto p = walrus ("walrus.mako-acs1", "MAKO ACS1 MKII", "ACS1", "mako acs1 mkii mk2 amp cab simulator walrus", "ACS1 MKII Digital Manual");
        p.switches = { { "Boost", 31, 127, 0 }, { "IR bypass", 85, 127, 0 }, { "Amp bypass", 86, 127, 0 } };
        p.pedals = knobs ({ { "Gain left", 24 }, { "Gain right", 25 }, { "Volume left", 22 }, { "Volume right", 23 }, { "Bass left", 3 },
                            { "Bass right", 9 }, { "Mid left", 14 }, { "Mid right", 15 }, { "Treble left", 20 }, { "Treble right", 21 },
                            { "Presence", 104 }, { "Resonance", 105 }, { "Room decay", 103 }, { "Gate threshold", 89 }, { "Gate release", 90 } });
        p.pedalNote = "The ACS1 has no expression CC; these move that control like turning it.";
        p.notes = walrusNotes ("Controls: boost (CC#31), IR bypass (CC#85), amp bypass (CC#86). The manual prints the bypass CC#30 as \"0-5\", so "
                               "it isn't a tile. Amp, cab and room choices (CC#26-29, 102) aren't named per value.");
        v.push_back (p);
    }

    // ---------------------------------------------------------------- Source Audio
    {
        auto p = sourceAudio ("sourceaudio.nemesis", "Nemesis Delay", "Nemesis", "nemesis adt delay source audio one series",
                              "Nemesis MIDI Implementation + Nemesis Delay ADT User's Guide");
        p.switches = { { "Effect", 101, 127, 0 }, { "Infinite hold", 97, 127, 0 } };
        p.utilities = { act ("Bypass toggle", 102, 127, anyToggle, 9) };
        for (const auto& a : sourceAudioControls())
            p.utilities.push_back (a);
        p.models = { group ("Delay engines", 1, { { "Digital", 0 }, { "Diffuse", 1 }, { "Analog", 2 }, { "Tape", 3 }, { "Noise Tape", 4 },
                                                  { "Degrade", 5 }, { "Shifter", 6 }, { "Helix", 7 }, { "Reverse", 8 }, { "Sweeper", 9 },
                                                  { "Rhythmic", 10 }, { "Slapback", 11 }, { "Resonant Analog", 12 }, { "Tremolo", 13 },
                                                  { "Sequenced Filters", 14 }, { "Dub", 15 }, { "Chorus", 16 }, { "Flanger", 17 },
                                                  { "Double Helix", 18 }, { "Complex Rhythmic", 19 }, { "Lo-Fi Retro", 20 },
                                                  { "Warped Record", 21 }, { "Compound Shifter", 22 }, { "Oil Can", 23 } },
                            "Changes the delay engine (CC#1).", 3),
                     group ("Tap division", 42, { { "Whole", 0 }, { "Dotted half", 1 }, { "Half", 2 }, { "Golden ratio", 3 }, { "Dotted quarter", 4 },
                                                  { "Swing quarter", 5 }, { "Quarter", 6 }, { "Dotted 8th", 7 }, { "Swing 8th", 8 },
                                                  { "Inverse golden", 9 }, { "8th", 10 }, { "Triplet", 11 }, { "16th", 12 }, { "Sextuplet", 13 },
                                                  { "32nd", 14 } }, "Tempo division (CC#42).", 5) };
        p.pedals = knobs ({ { "Delay time", 2 }, { "Feedback", 5 }, { "Mix", 6 }, { "Mod depth", 7 }, { "Mod rate", 8 }, { "Intensity", 9 },
                            { "Output level", 10 }, { "Octave shift", 50 }, { "Delay send", 51 } });
        p.pedalNote = "No remote expression CC in the Nemesis chart; these move that control like turning it.";
        p.notes = sourceAudioNotes ("Controls: effect on / bypass (CC#101; CC#38 does the same), infinite hold (CC#97), bypass toggle (CC#102, any "
                                    "value), tap (CC#93), next / previous preset (CC#82/80). Models: the 24 delay engines (CC#1) and the tap "
                                    "division (CC#42). Syncs to MIDI clock. Recall preset bypassed / engaged (CC#103/104) is left out: the "
                                    "manual doesn't say how presets are counted there.\n\nCC#1 is the engine select, not a mod wheel, and CC#32 "
                                    "is the LFO lock, so the Ableton padding uses CC#60.", false);
        v.push_back (p);
    }
    {
        auto p = sourceAudio ("sourceaudio.ventris", "Ventris Dual Reverb", "Ventris", "ventris dual reverb source audio one series",
                              "Ventris MIDI Implementation + Ventris Dual Reverb Owner's Manual (SA262)");
        p.switches = { { "Effect", 101, 127, 0 } };
        p.utilities = { act ("Bypass toggle", 102, 127, anyToggle, 9), act ("Hold", 97, 127, "Remote hold (CC#97, any value): sustains the active reverb's trail.", 5) };
        for (const auto& a : sourceAudioControls())
            p.utilities.push_back (a);
        addValues (p.utilities, 50, { { "Reverb A", 0 }, { "Reverb B", 1 }, { "A+B parallel", 2 }, { "A+B cascade", 3 } }, "Dual / single mode (CC#50).", 6);
        std::initializer_list<std::pair<const char*, int>> engines { { "Room", 0 }, { "Hall", 1 }, { "E-Dome", 2 }, { "True Spring", 3 }, { "Plate", 4 },
                                                                       { "Lo-Fi", 5 }, { "Modverb", 6 }, { "Shimmer", 7 }, { "Echoverb", 8 },
                                                                       { "Swell", 9 }, { "Offspring", 10 }, { "Reverse", 11 } };
        p.models = { group ("Engine A", 1, engines, "Changes reverb engine A (CC#1).", 3), group ("Engine B", 25, engines, "Changes reverb engine B (CC#25).", 4) };
        p.pedals = knobs ({ { "Expression", 100 }, { "Time A", 2 }, { "Mix A", 3 }, { "Pre-delay A", 4 }, { "Treble A", 5 }, { "Bass A", 7 },
                            { "Time B", 26 }, { "Mix B", 27 }, { "Pre-delay B", 28 }, { "Treble B", 29 }, { "Bass B", 31 },
                            { "A/B crossfade", 55 }, { "Reverb send", 54 } });
        p.pedalNote = "Expression (CC#100) overrides every expression input. The others move that control like turning it.";
        p.notes = sourceAudioNotes ("Controls: effect on / bypass (CC#101: 0-64 bypass, 65-127 engage), bypass toggle (CC#102), hold (CC#97), tap "
                                    "(CC#93), next / previous preset (CC#82/80), dual / single mode (CC#50). Models: engines 0-11 for A (CC#1) and "
                                    "B (CC#25). The MIDI implementation names 12 Outboard Spring and 13 Metal Box, but the owner's manual says "
                                    "12-23 are the extended engines, so those are left out. Recall preset CCs (103/104) are left out.\n\nCC#32 is "
                                    "diffusion B, so the Ableton padding uses CC#60.", true);
        v.push_back (p);
    }
    {
        auto p = sourceAudio ("sourceaudio.collider", "Collider Delay+Reverb", "Collider", "collider delay reverb source audio one series",
                              "Collider MIDI Implementation + Collider Owner's Manual (SA263)");
        p.switches = { { "Delay", 101, 127, 0 }, { "Reverb", 103, 127, 0 } };
        p.utilities = { act ("Delay toggle", 102, 127, anyToggle, 9), act ("Reverb toggle", 104, 127, anyToggle, 9),
                        act ("Hold", 97, 127, "Remote hold (CC#97, any value), like pressing the REVERB footswitch.", 5) };
        for (const auto& a : sourceAudioControls())
            p.utilities.push_back (a);
        std::initializer_list<std::pair<const char*, int>> engines { { "Digital", 0 }, { "Analog", 1 }, { "Tape", 2 }, { "Reverse", 3 }, { "Oil Can", 4 },
                                                                       { "Room", 5 }, { "Hall", 6 }, { "True Spring", 7 }, { "Plate", 8 },
                                                                       { "Shimmer", 9 }, { "E-Dome", 10 }, { "Swell", 11 } };
        p.models = { group ("Delay (A) engine", 1, engines, "Changes the Delay (A) engine (CC#1).", 3),
                     group ("Reverb (B) engine", 15, engines, "Changes the Reverb (B) engine (CC#15).", 4),
                     group ("Delay division", 9, { { "Quarter", 0 }, { "Dotted 8th", 1 }, { "Golden ratio", 2 }, { "Eighth", 3 }, { "Triplet", 4 },
                                                   { "Sixteenth", 5 } }, "Delay tap division (CC#9).", 5) };
        p.pedals = knobs ({ { "Expression", 100 }, { "Delay time", 2 }, { "Delay mix", 3 }, { "Delay feedback", 4 }, { "Delay tone", 5 },
                            { "Delay output", 8 }, { "Reverb time", 16 }, { "Reverb mix", 17 }, { "Reverb decay", 18 }, { "Reverb tone", 19 },
                            { "Reverb output", 22 } });
        p.pedalNote = "Expression (CC#100) is the remote expression. The others move that control like turning it.";
        p.notes = sourceAudioNotes ("Controls: Delay (A) and Reverb (B) on / bypass (CC#101/103: 0-64 bypass, 65-127 engage) and toggles "
                                    "(CC#102/104, any value), hold (CC#97), tap (CC#93), next / previous preset (CC#82/80). Models: the two "
                                    "engines (CC#1, CC#15) and the delay division (CC#9). Recall preset CCs (105-108) are left out. CC#1 is the "
                                    "engine select, not a mod wheel; the Ableton padding uses CC#60.", true);
        v.push_back (p);
    }

    // ---------------------------------------------------------------- Electro-Harmonix
    {
        auto p = base ("ehx.pog3", "Electro-Harmonix", "POG3", "POG3", "pog3 pog 3 polyphonic octave generator ehx", ehxColour,
                       "POG3 manual (web v1)");
        p.presetCount = 100; p.labelFrom = 1; p.pcOffset = 0;   // listed PC 1-100 = data 0-99 (the 1-based reading is inferred)
        p.padCc = -1;   // CC#0 isn't in the POG3 chart (CC#1, 4, 7, 9, 36, 70-82, 102-119, 122-125)
        p.switches = { { "Effect", 79, 127, 0 }, { "Left out", 70, 127, 0 }, { "Right out", 71, 127, 0 }, { "Direct out", 72, 127, 0 },
                       { "EXP", 73, 127, 0 }, { "Focus", 74, 127, 0 }, { "Dry attack", 75, 127, 0 }, { "Dry filter", 76, 127, 0 },
                       { "Dry detune", 77, 127, 0 } };
        p.switchesNote = "Effect (CC#79): On = effect, Off = bypass. The others enable (127) or disable (0) that output or option.";
        p.utilities = { { "Live mode", "PC 127", "Goes to Live mode (the manual's PC 128, sent as 127).", 8 } };
        p.pedals = knobs ({ { "Expression", 4 }, { "Detune slider", 1 }, { "Master volume", 7 }, { "Input gain", 9 }, { "Dry level", 111 },
                            { "-2 oct level", 112 }, { "-1 oct level", 113 }, { "+5th level", 114 }, { "+1 oct level", 115 },
                            { "+2 oct level", 116 }, { "Attack", 117 }, { "Filter", 118 }, { "Detune", 119 }, { "Q", 108 }, { "Env", 109 },
                            { "Spread", 110 } });
        p.pedalNote = "Expression (CC#4, the MSB alone). CC#1 is the Detune slider, not a mod wheel. The others move that slider like sliding it.";
        p.testMessage = "PC 0";   // preset 1
        p.channelHint = "Must match the POG3: Global MIDI Channel (Omni out of the box).";
        p.midiIn = "5-pin MIDI In";
        p.hasDin = true; p.hasThru = true; p.usbMidi = false; p.usbToThru = 0;
        p.notes = "Presets: 100. The manual lists Program Change 1-100 (and functions up to PC 128), so it counts from 1: preset 1 = data 0. "
                  "That reading is inferred, the manual doesn't say. Factory presets are 1-10. No bank select.\n\nControls: effect on / bypass "
                  "(CC#79: 0 = bypass, any other value = on), the outputs and options (CC#70-77), Live mode (PC 128 in the manual, sent as "
                  "127). Load preset + effect on / off (CC#81/82) and the footswitch PCs (122-127) aren't tiles. CC#80 saves a preset, so it's "
                  "left out.\n\nConnection: 5-pin MIDI In and MIDI Out, which works as a MIDI Thru when Global MIDI Thru is on (off out of "
                  "the box). USB is for the EHXport app; USB MIDI isn't documented. MIDI channel Omni out of the box.";
        v.push_back (p);
    }
    {
        auto p = base ("ehx.oceans-abyss", "Electro-Harmonix", "Oceans Abyss", "Abyss", "oceans abyss reverb ehx multi", ehxColour,
                       "Oceans Abyss manual (web v2)");
        p.presetCount = 128; p.labelFrom = 1; p.pcOffset = 0;   // PC 1-128 = data 0-127 (the 1-based reading is inferred)
        p.padCc = -1;   // CC#0 isn't in the global or block CC maps
        p.switches = { { "Main", 4, 127, 0 }, { "Group A", 9, 127, 0 }, { "Group B", 41, 127, 0 }, { "Infinite A", 64, 127, 0 },
                       { "Infinite B", 65, 127, 0 } };
        for (int b = 0; b < 8; ++b)
            p.switches.push_back ({ "Block " + n (b + 1), 35, 2 * b + 1, 2 * b });   // CC#35: even = bypass, odd = engage
        p.switches.push_back ({ "Reverb A", 35, 17, 16 });
        p.switches.push_back ({ "Reverb B", 35, 19, 18 });
        p.switches.push_back ({ "FX loop", 35, 21, 20 });
        p.switchesNote = "Main, Group A / B and Infinite: On 127, Off 0. Blocks 1-8 (column by column, top row first), Reverb A / B and the FX loop "
                         "use CC#35 values (odd = engage, even = bypass).";
        p.utilities = { act ("Tap A", 66, 127, oneTap, 1), act ("Tap B", 67, 127, oneTap, 1),
                        act ("Kick A", 68, 127, "Kicks FX group A (CC#68; strength follows the value, 127 = full).", 0),
                        act ("Kick B", 69, 127, "Kicks FX group B (CC#69).", 0),
                        act ("Live", 3, 0, "LIVE mode (CC#3 = 0-63).", 8), act ("Preset mode", 3, 127, "Preset mode (CC#3 = 64-127).", 8) };
        p.pedals = knobs ({ { "Expression", 11 }, { "Mod depth", 1 }, { "Input level", 6 }, { "Output level", 7 }, { "Blend", 8 } });
        p.pedalNote = "Expression is CC#11 here (CC#4 is the main bypass). Mod depth is the global CC#1.";
        p.testMessage = "PC 0";   // preset 1
        p.channelHint = "Must match the pedal: MIDI Channel (Omni out of the box); each grid block can also have its own channel.";
        p.midiIn = "5-pin MIDI In";
        p.hasDin = true; p.hasThru = true; p.usbMidi = false; p.usbToThru = 0;
        p.notes = "Presets: 128. The manual lists Program Change 1-128, so it counts from 1: preset 1 = data 0 (inferred; the manual doesn't say). "
                  "No bank select.\n\nControls: main bypass (CC#4), FX group A / B (CC#9 / CC#41), infinite A / B (CC#64/65), tap A / B "
                  "(CC#66/67), kick A / B (CC#68/69), LIVE / Preset (CC#3), and each block's bypass (CC#35). The per-block reverb, delay "
                  "and FX loop parameter CCs aren't here yet: use a custom MIDI device for those.\n\nDelay and modulation blocks of the same "
                  "type share CC numbers; give a block its own channel (Block 1-8 Channel) to address it alone.\n\nConnection: 5-pin MIDI In "
                  "and MIDI Out (= MIDI Thru: All out of the box, No Clock or Off). USB-C is for EHXport; USB MIDI isn't documented. MIDI "
                  "channel Omni out of the box.";
        v.push_back (p);
    }
}
}
