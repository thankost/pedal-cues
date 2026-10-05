#include "DeviceTemplates.h"
#include "State.h"

#include <functional>

// Every number below comes from the manufacturer's manual named in the template's notes (Line 6 Owner's Manuals for
// firmware 3.80, POD Go 2.50, the Helix Stadium online manual Rev D; Fractal Audio Owner's Manuals; Boss GT-1000 MIDI
// Implementation 1.20 and GT-1000CORE Parameter Guide). Message text uses the custom-device syntax (cues::custom::parse):
// "PC n", "CC n=v", "bank n" (CC#0). Programs count from the template's programBase (0, or 1 for Boss).
namespace templates
{
namespace
{
const juce::Colour fractalColour { 0xff4e9bd8 };
const juce::Colour bossColour { 0xffe8c547 };
const juce::Colour darkglassColour { 0xff9aa5b1 };

using Tiles = std::vector<TileDef>;

Tiles range (int count, const std::function<TileDef (int)>& make)
{
    Tiles t;
    for (int i = 0; i < count; ++i)
        t.push_back (make (i));
    return t;
}

Tiles operator+ (Tiles a, const Tiles& b)
{
    a.insert (a.end(), b.begin(), b.end());
    return a;
}

juce::String n (int v) { return juce::String (v); }

//==============================================================================
// Fractal Audio
const juce::String fractalBypassNote ("Fractal units can be set to treat bypass CCs as a toggle (any value flips it). These tiles assume "
                                      "the default: 0-63 = off (bypassed), 64-127 = on.");

juce::String assignNote (const juce::String& where)
{
    return "This unit has no default MIDI CC for this: assign it in " + where + " (the suggested number is in the tile), or change the tile to your number.";
}

// Axe-Fx III, FM9, FM3: no default CCs. The tiles use the Axe-Fx II's defaults as suggestions.
Template fractalModern (const juce::String& id, const juce::String& model, const juce::String& aliases, const juce::String& manual,
                        int presets, const juce::String& connection)
{
    Template t;
    t.id = id;
    t.brand = "Fractal Audio";
    t.model = model;
    t.aliases = "fractal " + aliases;
    t.colour = fractalColour;
    const auto where = juce::String ("SETUP > MIDI/Remote");
    t.notes = "Why this is an editable device and not a page like the Axe-Fx II: this unit has no default MIDI CCs. You assign them on the unit "
              "(" + where + "), so the numbers differ from player to player. These tiles suggest numbers; set the same ones on the unit, or change "
              "the tiles to yours. Preset tiles work right away.\n\n"
              "Built from the " + manual + ". Not tested on hardware: check the numbers on your unit.\n\n"
              "IMPORTANT: this unit has no default MIDI CCs. Before using the scene, tuner and looper tiles, assign the same numbers on the unit in "
              + where + " (Other: Scene Select 34, Tuner 15; Looper: Record 28, Play 29, Once 30, Dub 31, Reverse 32), or change the tiles to the "
              "numbers you use. These suggestions are the Axe-Fx II's old defaults.\n\n"
              "Presets: " + n (presets) + ", numbered from 000. A tile sends the bank (CC#0: 0 = 000-127, 1 = 128-255, ...) and then the "
              "Program Change. \"Display Offset\" on the unit only changes the number shown, not what a Program Change loads.\n\n"
              "Scenes: Scene Select values 0-7 = scenes 1-8. Drop a scene tile just after its preset tile, not in the same tile.\n\n"
              + connection + "\n\n" + fractalBypassNote;
    t.groups = {
        { "Presets", range (8, [] (int i) { return TileDef { "Preset " + juce::String (i).paddedLeft ('0', 3), "bank 0, PC " + n (i), {}, 4 + i % 6 }; })
                         + Tiles { { "Preset 128", "bank 1, PC 0", "Bank 1 (CC#0 = 1), Program Change 0 = preset 128.", 0 } } },
        { "Scenes", range (8, [&] (int i) { return TileDef { "Scene " + n (i + 1), "CC 34=" + n (i), assignNote (where + " > Other > Scene Select"), i % 10 }; }) },
        { "Utilities", Tiles { { "Tuner on", "CC 15=127", assignNote (where + " > Other > Tuner"), 5 },
                               { "Tuner off", "CC 15=0", assignNote (where + " > Other > Tuner"), 9 } } },
        { "Looper", Tiles { { "Looper record", "CC 28=127", assignNote (where + " > Looper"), 0 }, { "Looper play", "CC 29=127", assignNote (where + " > Looper"), 3 },
                            { "Looper once", "CC 30=127", assignNote (where + " > Looper"), 4 }, { "Looper dub", "CC 31=127", assignNote (where + " > Looper"), 1 },
                            { "Looper reverse", "CC 32=127", assignNote (where + " > Looper"), 5 } } },
    };
    return t;
}

Template vp4()
{
    Template t;
    t.id = "fractal.vp4";
    t.pedal = true;   // Virtual Pedalboard: an effects unit
    t.brand = "Fractal Audio";
    t.model = "VP4";
    t.aliases = "fractal vp4 vp 4 virtual pedalboard";
    t.colour = fractalColour;
    t.notes = "Why this is an editable device and not a page: the VP4 has no default MIDI CCs, you assign them on the unit, so the numbers differ "
              "from player to player. Preset tiles work right away.\n\nBuilt from the VP4 Owner's Manual. Not tested on hardware: check the numbers on your unit.\n\n"
              "Presets: 104, A1-Z4 = Program Change 0-103. The VP4 doesn't receive bank select.\n\n"
              "IMPORTANT: the VP4 has no default MIDI CCs. Assign Scene Select in its MIDI settings (the manual's example is CC#17, used in the "
              "scene tiles), or change the tiles to your number. Values 0-3 = scenes 1-4.\n\n"
              "MIDI uses 3.5 mm TRS jacks: you need a Type A TRS-to-5-pin adapter. It also takes MIDI over USB.";
    t.groups = {
        { "Presets", range (8, [] (int i) { return TileDef { juce::String::charToString ((juce::juce_wchar) ('A' + i / 4)) + n (i % 4 + 1),
                                                             "PC " + n (i), {}, 4 + i % 6 }; }) },
        { "Scenes", range (4, [] (int i) { return TileDef { "Scene " + n (i + 1), "CC 17=" + n (i), assignNote ("the VP4's MIDI settings (Scene Select)"), i % 10 }; }) },
    };
    return t;
}

//==============================================================================
// Boss GT-1000 / GT-1000CORE: Program Changes load what the unit's PROGRAM MAP says, and CCs only do what an ASSIGN uses
// them for (sources CC#1-31, CC#64-95). Bank select is CC#0 (then CC#32 = 0), as the MIDI Implementation shows.
Template bossGt1000()
{
    Template t;
    t.id = "boss.gt-1000";
    t.brand = "Boss";
    t.model = "GT-1000 / GT-1000CORE";
    t.aliases = "boss roland gt1000 gt 1000 core gt-1000core";
    t.colour = bossColour;
    t.programBase = 1;   // the PROGRAM MAP counts PC#1-128
    t.expCc = 11;
    const juce::String assign ("Set this on the GT-1000: MENU > CONTROL ASSIGN > ASSIGN SETTING, an ASSIGN with SOURCE = the CC in this tile "
                               "(CC#1-31 or CC#64-95), MODE = MOMENT, and the TARGET you want (for example an effect's on/off). Or change the "
                               "tile to the CC you already use.");
    t.notes = "Why this is a template and not a page: the GT-1000 has no fixed MIDI CCs. It only reacts to a CC that an ASSIGN uses as its "
              "source, and which patch a Program Change loads is up to its PROGRAM MAP. So you set those on the unit, and these tiles follow.\n\n"
              "Built from the Boss GT-1000 MIDI Implementation and the GT-1000CORE Parameter Guide. Not tested on hardware: check on your unit.\n\n"
              "1. MIDI channel: MENU > MIDI > MIDI SETTING > RX CHANNEL, the same as at the top of its PedalCues tab.\n\n"
              "2. Presets: set MENU > MIDI > MAP SELECT to PROG, then in PROGRAM MAP BANK1 set PC#1, PC#2... to the patches you want "
              "(U001-U250, P001-P250). The preset tiles send bank select (CC#0, then CC#32 = 0) and the Program Change: BANK1 = bank 0, "
              "BANK2 = bank 1. Rename the tiles after your patches.\n\n"
              "3. Expression (the Expression view): make an ASSIGN with SOURCE = CC#11 (or the CC you pick there) and the TARGET you want to "
              "move, for example foot volume, a wah or a delay level. Keep ACT LOW 0 and ACT HIGH 127.\n\n"
              "4. Switches: each switch tile needs an ASSIGN with SOURCE = its CC and MODE = MOMENT, so 127 = on and 0 = off.\n\n"
              "Connection: MIDI IN / OUT (5-pin) and USB. To pass MIDI on to a second device, set MIDI THRU (for MIDI IN) or USB THRU "
              "(for USB) in MENU > MIDI > MIDI SETTING to MIDI OUT.";
    t.groups = {
        { "Presets", range (8, [] (int i) { return TileDef { "BANK1 PC#" + n (i + 1), "bank 0, CC 32=0, PC " + n (i + 1),
                                                             "Loads what PROGRAM MAP BANK1 PC#" + n (i + 1) + " is set to on the GT-1000.", 4 + i % 6 }; })
                         + Tiles { { "BANK2 PC#1", "bank 1, CC 32=0, PC 1", "Loads what PROGRAM MAP BANK2 PC#1 is set to on the GT-1000.", 0 } } },
        { "Switches (assign on the unit)", range (4, [&] (int i) { return TileDef { "Switch " + n (i + 1) + " on", "CC " + n (80 + i) + "=127", assign, i % 10 }; })
                                           + range (4, [&] (int i) { return TileDef { "Switch " + n (i + 1) + " off", "CC " + n (80 + i) + "=0", assign, 9 }; }) },
    };
    return t;
}
//==============================================================================
// Effect pedals with no default MIDI CCs (Effects & Pedals tab): Eventide H9 (H9 User Guide Rev D, software 5.4+), Eventide H90
// (manual 1.11.4, section 7.4 and the mapping chart), Boss RC-500 (Owner's Manual eng02, Parameter Guide eng01) and RC-600
// (Owner's Manual eng05, Parameter Guide 1.3+). Presets work right away; every CC tile is a suggestion to assign on the pedal.
const juce::Colour eventideColour { 0xff6fb7ff };
const juce::Colour bossPedalColour { 0xffff5a5f };   // the Boss pedal pages' accent (DD-500 / RV-500 / MD-500)

TileDef assigned (const juce::String& name, const juce::String& messages, const juce::String& what, int colour)
{
    return { name, messages, what + " Assign it on the pedal (the suggested CC is in the tile), or change the tile to the CC you use.", colour };
}

Template eventideH9()
{
    Template t;
    t.id = "eventide.h9";
    t.pedal = true;
    t.brand = "Eventide";
    t.model = "H9 / H9 MAX / H9 CORE";
    t.aliases = "eventide h9 max core harmonizer";
    t.colour = eventideColour;
    t.programBase = 0;   // the RCV.MAP's Prg No. is the Program Change number (0-127); Prg No. 1 = P01 by default
    t.expCc = 22;        // KB0, the running algorithm's first parameter (KB0-KB9 are patched from C22 by default)
    const juce::String rcv ("the H9's System > MIDI > RCV.CTL (destination");
    t.notes = "Why this is a template and not a page: the H9 has no default MIDI CCs for bypass, tap, the tuner or the looper. You patch "
              "them yourself in System > MIDI > RCV.CTL (sources C0-C99 or pitch bend), so the numbers differ from player to player. "
              "These tiles suggest numbers; set the same ones on the H9, or change the tiles to yours. Preset tiles work right away.\n\n"
              "Built from the H9 Harmonizer User Guide (Rev D, software 5.4 and later). Not tested on hardware: check the numbers on your "
              "pedal.\n\n"
              "Presets: P01-P99 (the H9 CORE ships with 25). By default the receive map (RCV.MAP) loads P01-P99 with Program Change "
              "1-99, so P01 = PC 1; what PC 0 does by default isn't documented. You can remap any Program Change to a preset, or to "
              "bypass / active / toggle / tuner / next / last. No bank select. A preset saved bypassed loads bypassed.\n\n"
              "Assigned tiles (RCV.CTL destination in brackets): Active [ACT] CC#80, Bypass [BYP] CC#81, Tap [TAP] CC#82, Tuner [TUN] CC#83; "
              "Looper algorithm only: Record [REC] CC#84, Play [PLY] CC#85, Stop [STP] CC#86. The guide doesn't say which values these "
              "switch destinations react to: the tiles send 127. Keep away from C22 and up: KB0-KB9 are patched there by default.\n\n"
              "Expression: the Expression view starts on CC#22 = KB0, the running algorithm's first parameter (KB0-KB9 are patched to "
              "C22 and up by default; the guide names C22-C29 in one place and C22-C31 in another). The mapping is system-wide: it "
              "applies to every preset.\n\n"
              "MIDI channel: System > MIDI > RCV CH (OFF, OMNI, 1-16); the guide doesn't give its factory setting.\n\n"
              "Connection: 5-pin MIDI In and MIDI Out/Thru, and USB MIDI (mini USB). IMPORTANT: when USB is connected, the H9 ignores "
              "its 5-pin MIDI In (thru included), so don't daisy-chain it from a MIDI cable while it's on USB. OUTPUT (XMT, THRU, "
              "THRU+C, MERGE) sets what the MIDI Out sends.";
    t.groups = {
        { "Presets", range (8, [] (int i) { return TileDef { "P" + juce::String (i + 1).paddedLeft ('0', 2), "PC " + n (i + 1),
                                                             "Program Change " + n (i + 1) + " loads P" + juce::String (i + 1).paddedLeft ('0', 2)
                                                             + " with the default receive map (RCV.MAP).", 4 + i % 6 }; }) },
        { "Switches (assign on the pedal)", Tiles { assigned ("Active", "CC 80=127", "Turns the effect on: " + rcv + " ACT).", 3),
                                                    assigned ("Bypass", "CC 81=127", "Bypasses the effect: " + rcv + " BYP).", 9),
                                                    assigned ("Tap", "CC 82=127", "One tap: " + rcv + " TAP).", 1),
                                                    assigned ("Tuner", "CC 83=127", "The tuner: " + rcv + " TUN).", 5) } },
        { "Looper (assign on the pedal)", Tiles { assigned ("Looper record", "CC 84=127", "Looper algorithm only: " + rcv + " REC).", 0),
                                                  assigned ("Looper play", "CC 85=127", "Looper algorithm only: " + rcv + " PLY).", 3),
                                                  assigned ("Looper stop", "CC 86=127", "Looper algorithm only: " + rcv + " STP).", 9) } },
    };
    return t;
}

Template eventideH90()
{
    Template t;
    t.id = "eventide.h90";
    t.pedal = true;
    t.brand = "Eventide";
    t.model = "H90";
    t.aliases = "eventide h90 harmonizer";
    t.colour = eventideColour;
    t.programBase = 1;   // "By default, the H90 will transmit and receive Program Changes using PC numbers 1-100"
    t.expCc = 11;
    const juce::String where ("System > MIDI > Global Control (Parameter: ");
    t.notes = "Why this is a template and not a page: the H90 has no default MIDI CC mappings (its manual: \"all mappings are customizable "
              "by the user\"). You map them in System > MIDI > Global Control, so the numbers differ from player to player. These tiles "
              "suggest numbers; set the same ones on the H90, or change the tiles to yours. Program tiles work right away.\n\n"
              "Built from the Eventide H90 user manual (version 1.11.4). Not tested on hardware: check the numbers on your pedal.\n\n"
              "Programs: up to 99 in the active list (the Playlist). By default the H90 counts Program Changes 1-100, and these tiles "
              "count the same way (Program 1 = PC 1, the first Program Change). With PC Offset on, it counts 0-99 instead. The manual "
              "doesn't spell out which number on the wire loads Program 1: if the tiles load the program next to the one you want, "
              "check PC Offset (System > MIDI). Bank select isn't documented.\n\n"
              "Assigned tiles (Global Control parameter in brackets): Tap [Tap Tempo] CC#80, Tuner [Tuner] CC#81 (enters or leaves "
              "it), Preset P on / bypassed [P Act/Byp (M)] CC#82 (0-63 = bypassed, 64-127 = active), Next program [Inc + Load] "
              "CC#83. Toggles react to values of 64 and up. The manual doesn't give the CC range you can pick from.\n\n"
              "No looper tiles: the H90's looper has no MIDI CC functions of its own; it's played through HotSwitches and Perform "
              "parameters, which you can map the same way.\n\n"
              "MIDI channel: System > MIDI > Channel (1-16), or Receive Omni.\n\n"
              "Connection: 5-pin MIDI In and MIDI Out/Thru, USB-C (MIDI over USB) and Bluetooth MIDI. Output Mode Thru passes on only "
              "what arrives at the 5-pin MIDI In (and the H90 then sends nothing of its own); the manual doesn't say whether MIDI "
              "from USB is passed on.";
    t.groups = {
        { "Programs", range (8, [] (int i) { return TileDef { "Program " + n (i + 1), "PC " + n (i + 1),
                                                              "Program " + n (i + 1) + " of the active list (default numbering, PC Offset off).", 4 + i % 6 }; }) },
        { "Switches (assign on the pedal)", Tiles { assigned ("Tap", "CC 80=127", "One tap: " + where + "Tap Tempo).", 1),
                                                    assigned ("Tuner", "CC 81=127", "Enters or leaves the tuner: " + where + "Tuner).", 5),
                                                    assigned ("P active", "CC 82=127", "Preset P active (64-127): " + where + "P Act/Byp (M)).", 3),
                                                    assigned ("P bypassed", "CC 82=0", "Preset P bypassed (0-63): " + where + "P Act/Byp (M)).", 9),
                                                    assigned ("Next program", "CC 83=127", "Loads the next program: " + where + "Inc + Load).", 7) } },
    };
    return t;
}

const juce::String rcAssignHow ("The manual doesn't say how a CC value fires a switch-type target: these tiles send 127. Keep the ASSIGN's "
                                "source window at 0-127.");

Template bossRc500()
{
    Template t;
    t.id = "boss.rc-500";
    t.pedal = true;
    t.brand = "Boss";
    t.model = "RC-500";
    t.aliases = "boss roland rc500 rc 500 loop station looper";
    t.colour = bossPedalColour;
    t.programBase = 1;   // "program change messages numbered 01 through 99, corresponding to the 99 individual memories 1-99"
    t.expCc = 11;
    const juce::String where ("in this memory's ASSIGN settings (ASSIGN ON, SOURCE = this CC, TARGET ");
    t.notes = "Why this is a template and not a page: the RC-500 has no fixed MIDI CCs. A CC only does something when one of a memory's "
              "ASSIGN1-8 uses it as its SOURCE (CC#1-31 or CC#64-95), and that's set up in every memory. So you set those on the RC-500, "
              "and these tiles follow. Memory tiles work right away.\n\n"
              "Built from the RC-500 Owner's Manual and Parameter Guide. Not tested on hardware: check the numbers on your pedal.\n\n"
              "Memories: 01-99 = Program Change 1-99 in the manual's numbering (the first Program Change loads memory 01; the guide "
              "doesn't say this in so many words, the RC-600's manual does). Bank select (CC#0, CC#32) is ignored.\n\n"
              "Assigned tiles (TARGET in brackets), each needs an ASSIGN in every memory you use it in: Rec/play [CUR REC/PLY] CC#80, "
              "Stop [CUR STOP] CC#81, Undo/redo [CUR UND/RED] CC#82, Tap tempo [TAP TEMPO] CC#83, All start [ALL START] CC#84. "
              + rcAssignHow + "\n\n"
              "MIDI channel: RX CTL CH (1 out of the box) in the MIDI settings. IMPORTANT: OMNI is ON out of the box, so the RC-500 "
              "answers on every channel: turn OMNI off if another pedal shares the cable.\n\n"
              "Connection: MIDI IN and MIDI OUT are 3.5 mm TRS jacks: use Boss TRS/MIDI cables (BMIDI-5-35); the manual doesn't name "
              "the TRS type. USB carries MIDI too. MIDI THRU and USB THRU are OFF out of the box: set them to MIDI OUT to pass MIDI on. "
              "It follows MIDI clock and starts all tracks on a MIDI Start (SYNC START ALL).";
    t.groups = {
        { "Memories", range (8, [] (int i) { return TileDef { "Memory " + juce::String (i + 1).paddedLeft ('0', 2), "PC " + n (i + 1), {}, 4 + i % 6 }; }) },
        { "Looper (assign on the pedal)", Tiles { assigned ("Rec/play", "CC 80=127", "Records, overdubs or plays the current track: " + where + "CUR REC/PLY).", 0),
                                                  assigned ("Stop", "CC 81=127", "Stops the current track: " + where + "CUR STOP).", 9),
                                                  assigned ("Undo/redo", "CC 82=127", "Undo or redo on the current track: " + where + "CUR UND/RED).", 6),
                                                  assigned ("Tap tempo", "CC 83=127", "One tap: " + where + "TAP TEMPO).", 1),
                                                  assigned ("All start", "CC 84=127", "Starts all tracks: " + where + "ALL START).", 3) } },
    };
    return t;
}

Template bossRc600()
{
    Template t;
    t.id = "boss.rc-600";
    t.pedal = true;
    t.brand = "Boss";
    t.model = "RC-600";
    t.aliases = "boss roland rc600 rc 600 loop station looper";
    t.colour = bossPedalColour;
    t.programBase = 0;   // "Program Change messages numbered 0 through 98, corresponding to the 99 individual memories 01-99"
    t.expCc = 11;
    const juce::String where ("in ASSIGN (SW ON, SOURCE = MIDI CC# of this tile, TARGET ");
    t.notes = "Why this is a template and not a page: the RC-600 has no fixed MIDI CCs. A CC only does something when one of ASSIGN1-16 uses "
              "it as its SOURCE (MIDI CC#01-31 or CC#64-95), and the ASSIGN settings are stored in each memory (write the memory to keep "
              "them). So you set those on the RC-600, and these tiles follow. Memory tiles work right away.\n\n"
              "Built from the RC-600 Owner's Manual and Parameter Guide (version 1.3 and later). Not tested on hardware: check the numbers "
              "on your pedal.\n\n"
              "Memories: 01-99 = Program Change 0-98. Bank select (CC#0, CC#32) is ignored.\n\n"
              "Assigned tiles (TARGET in brackets): Rec/play [CUR.TRK REC/PLY] CC#80, Play/stop [CUR.TRK PLY/STP] CC#81, Undo/redo "
              "[CUR.TRK UN/RED] CC#82, Tap tempo [TAP TEMPO] CC#83, All start/stop [ALL ST/STP] CC#84. " + rcAssignHow + " (SOURCE ACT.LO "
              "0, ACT.HI 127, as the guide suggests.)\n\n"
              "MIDI channel: RX CH CTL (1 out of the box) in the MIDI settings. The guide has no OMNI setting.\n\n"
              "Connection: MIDI IN and MIDI OUT (5-pin in the rear panel drawing) and USB (MIDI too). THRU for MIDI IN and for USB IN is "
              "OFF out of the box: set it to MIDI OUT to pass MIDI on. It follows MIDI clock and starts all tracks on a MIDI Start "
              "(SYNC START ALL).";
    t.groups = {
        { "Memories", range (8, [] (int i) { return TileDef { "Memory " + juce::String (i + 1).paddedLeft ('0', 2), "PC " + n (i), {}, 4 + i % 6 }; }) },
        { "Looper (assign on the pedal)", Tiles { assigned ("Rec/play", "CC 80=127", "Records, overdubs or plays the current track: " + where + "CUR.TRK REC/PLY).", 0),
                                                  assigned ("Play/stop", "CC 81=127", "Plays or stops the current track: " + where + "CUR.TRK PLY/STP).", 3),
                                                  assigned ("Undo/redo", "CC 82=127", "Undo or redo on the current track: " + where + "CUR.TRK UN/RED).", 6),
                                                  assigned ("Tap tempo", "CC 83=127", "One tap: " + where + "TAP TEMPO).", 1),
                                                  assigned ("All start/stop", "CC 84=127", "Starts or stops all tracks: " + where + "ALL ST/STP).", 9) } },
    };
    return t;
}

//==============================================================================
// Darkglass Microtubes Infinity: Darkglass publishes no MIDI chart for it. These numbers come from the community chart in
// Morningstar's openmidi database (data/brands/darkglass/microtubesinfinity.yaml); no Program Changes are documented.
Template microtubesInfinity()
{
    Template t;
    t.id = "darkglass.microtubes-infinity";
    t.pedal = true;   // a bass preamp / drive pedal
    t.brand = "Darkglass";
    t.model = "Microtubes Infinity";
    t.aliases = "darkglass microtubes infinity mt inf bass";
    t.colour = darkglassColour;
    t.source = "a community MIDI chart (openmidi), not Darkglass's manual";
    t.expCc = 1;          // Drive: the Expression view moves it
    t.cc0IsControl = true;
    const juce::String modeNote ("Distortion mode (CC#13): each mode has a range of values; this tile sends one from the middle.");
    t.notes = "Why this is a template: Darkglass doesn't publish a MIDI chart for the Microtubes Infinity. Its manual only says it takes "
              "MIDI on a 3.5 mm TRS jack and over USB. These numbers come from a community chart (Morningstar's openmidi database), not "
              "from Darkglass. Not tested on hardware: check them on your pedal.\n\n"
              "No Program Changes are documented, so there are no preset tiles. Add one with + Tile if your pedal loads its presets that way.\n\n"
              "Controls (0-127): CC#0 Compression, 1 Drive, 2 Character, 3 Blend, 4 Level, 5 Headphone volume, 6-11 EQ sliders 1-6 (left "
              "to right), 12 Compression ratio, 13 Distortion mode, 14 Active cab sim. The Expression view moves any of them: pick the CC "
              "(it starts on Drive, CC#1).\n\n"
              "Distortion mode (CC#13): 0-21 bypass distortion, 22-42 clean tube preamp, 43-63 Vintage Microtubes, 64-85 Microtubes B3K, "
              "86-106 Vintage multi-band, 107-127 B3K multi-band.\n\n"
              "Cab sim (CC#14): the chart doesn't say which values turn it on; the tiles assume 0 = off, 127 = on.\n\n"
              "IMPORTANT, the MIDI cable: the Infinity's MIDI jack is TRS Type B, unlike Type A on most gear. Use a Type B cable or adapter, "
              "or a controller that can switch to Type B. USB MIDI works too.\n\n"
              "CC#0 is Compression here, not bank select, so PedalCues never sends a spare CC#0 in this device's clips.";
    t.groups = {
        { "Distortion", Tiles { { "Distortion off", "CC 13=10", modeNote, 9 }, { "Clean tube", "CC 13=32", modeNote, 4 },
                                { "Vintage", "CC 13=53", modeNote, 1 }, { "B3K", "CC 13=75", modeNote, 0 },
                                { "Vintage multi-band", "CC 13=96", modeNote, 6 }, { "B3K multi-band", "CC 13=117", modeNote, 2 } } },
        { "Cab sim", Tiles { { "Cab sim on", "CC 14=127", "Assumed 0 = off, 127 = on (not in the chart).", 3 },
                             { "Cab sim off", "CC 14=0", "Assumed 0 = off, 127 = on (not in the chart).", 9 } } },
        { "Set a control", Tiles { { "Drive half", "CC 1=64", "Drive (CC#1) at half. Use the Expression view for moves.", 0 },
                                   { "Blend full", "CC 3=127", "Blend (CC#3) fully wet.", 5 },
                                   { "Blend half", "CC 3=64", "Blend (CC#3) at half.", 5 } } },
    };
    return t;
}
}

//==============================================================================
const std::vector<Template>& all()
{
    static const std::vector<Template> list {
        fractalModern ("fractal.axe-fx-3", "Axe-Fx III (Mk I / Mk II / Turbo)", "axe fx axefx 3 iii mk2 turbo",
                       "Axe-Fx III Owner's Manual (firmware 20.x)", 1024,
                       "Connection: MIDI over USB works like 5-pin MIDI, but USB MIDI is never passed on to the MIDI Thru. 512 presets on the Mk I, "
                       "1024 on the Mk II."),
        fractalModern ("fractal.fm9", "FM9 (+ Turbo)", "fm9 fm 9 turbo", "FM9 Owner's Manual", 512,
                       "Connection: the FM9 appears as a MIDI device over USB. USB MIDI is not passed on to its OUT/THRU jack."),
        fractalModern ("fractal.fm3", "FM3 (Mk I / Mk II / Turbo)", "fm3 fm 3 mk2 turbo", "FM3 Owner's Manual", 512,
                       "Connection: IMPORTANT: the FM3 can't be controlled over USB MIDI (Fractal: \"unpredictable behavior\"). Use a 5-pin MIDI "
                       "cable from your interface's MIDI Out."),
        vp4(),
        bossGt1000(),
        microtubesInfinity(),
        eventideH9(),
        eventideH90(),
        bossRc500(),
        bossRc600(),
    };
    return list;
}

const Template* find (const juce::String& id)
{
    for (const auto& t : all())
        if (t.id == id)
            return &t;
    return nullptr;
}

juce::String disclaimer (const Template& t)
{
    if (t.source.isNotEmpty())
        return "From " + t.source + ". Not tested on hardware: check the numbers on your unit.";
    return "From the " + t.brand + " " + t.model + " manual. Not tested on hardware: check the numbers on your unit.";
}

juce::ValueTree createUnit (const Template& t)
{
    // A short name for the tab: "Axe-Fx III (Mk I / Mk II / Turbo)" -> "Axe-Fx III", "POD Go / POD Go Wireless" -> "POD Go".
    auto unit = state::createCustomUnit (t.model.upToFirstOccurrenceOf (" (", false, false).upToFirstOccurrenceOf (" /", false, false).trim());
    unit.removeAllChildren (nullptr);
    unit.setProperty (IDs::templateId, t.id, nullptr);
    unit.setProperty (IDs::colour, t.colour.toString(), nullptr);
    unit.setProperty (IDs::programBase, t.programBase, nullptr);
    unit.setProperty (IDs::expCc, t.expCc, nullptr);
    unit.setProperty (IDs::padRepeat, t.cc0IsControl, nullptr);
    unit.setProperty (IDs::category, t.pedal ? "pedal" : "amp", nullptr);
    unit.setProperty (IDs::notes, juce::String(), nullptr);   // the manual's notes stay read-only (About this unit); these are the player's own
    const auto& palette = state::palette();
    for (const auto& g : t.groups)
    {
        juce::ValueTree group (IDs::Group);
        group.setProperty (IDs::name, g.name, nullptr);
        for (const auto& d : g.tiles)
        {
            juce::ValueTree tile (IDs::CueTile);
            tile.setProperty (IDs::name, d.name, nullptr);
            tile.setProperty (IDs::messages, d.messages, nullptr);
            tile.setProperty (IDs::note, d.note, nullptr);
            tile.setProperty (IDs::colour, juce::Colour (palette.getReference (((d.colour % palette.size()) + palette.size()) % palette.size()).argb).toString(), nullptr);
            group.appendChild (tile, nullptr);
        }
        unit.appendChild (group, nullptr);
    }
    return unit;
}
}
