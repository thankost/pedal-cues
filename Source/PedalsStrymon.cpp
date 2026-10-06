#include "Modellers.h"

#include <algorithm>

// Strymon pages (Effects & Pedals tab), from the makers' MIDI charts: the TimeLine (Rev F + MIDI Program Changes chart),
// BigSky (Rev D) and Mobius (Rev F) user manuals; TimeLine MX (Rev C) and BigSky MX (Rev B); Volante (Rev E); Sunset (Rev C),
// Riverside (Rev B), Compadre (Rev C), cloudburst (Rev D), Iridium (Rev D); the v2 pedals Flint (Rev C), El Capistan (Rev C),
// Deco (Rev D), DIG (Rev D), Lex (Rev C), blueSky (Rev C); Brig (Rev A), Olivera (Rev B), Ultraviolet (Rev A), Zelzah (Rev A).
// Every Strymon map is fixed (not assignable). Three generations with different conventions, never mixed:
//  - TimeLine / BigSky / Mobius: 00A-99B(C), bypass CC#102 0 = bypass / 127 = engage, remote tap CC#93 any value, 5-pin only.
//  - MX: 000A-149B, dual-engine CC pairs, bypass 1-127 = engaged, 5-pin + USB + TRS, channel 1, THRU off.
//  - small pedals: presets 0-299, on/off 0 / 1-127, PC 127 = Manual mode, enumerations start at 1.
// Footswitch CCs are press tiles (Control::press: down, then up 1/16 later), with each generation's own values. Controls holds
// the presses, on / off pairs and documented enumerations (per-machine ones named "Trem: Square"); Expression every continuous
// parameter with its real top value (Control::max). midi.guide (pencilresearch/midi, CC BY-SA) has charts for the TimeLine,
// BigSky, Volante and Deco (v1) only; facts used only from there are marked in the profile's notes.
namespace modellers
{
namespace
{
const juce::Colour strymonColour { 0xffd6a756 };

juce::String n (int v) { return juce::String (v); }
juce::String cc (int number, int value) { return "CC " + n (number) + "=" + n (value); }

const juce::String oneTap ("One tap (CC#93, any value). Tempo needs several taps a beat apart: drop the clip on each beat, "
                           "or set the tempo in the preset.");
const juce::String anyValue ("Acts on any value: each clip is one press.");
const juce::String toggles ("Any value toggles it: each clip switches it the other way.");
const juce::String smallChannel ("Must match the pedal's MIDI channel: 1 out of the box, 2 or 3 in its power-up MIDI menu "
                                 "(4-16: it takes the channel of the next Program Change it gets). See the manual.");
const juce::String trsConnection ("Connection: the EXP/MIDI jack set to MIDI mode in the power-up menu, with a Strymon MIDI EXP cable, "
                                  "a Strymon Conduit, or a TRS cable to a TRS MIDI controller or interface (the manual doesn't say "
                                  "Type A or B). Leave MIDI Output OFF (the default) when it only receives.");

using Reserved = std::vector<std::pair<int, juce::String>>;

Profile base (const juce::String& id, const juce::String& model, const juce::String& shortName, const juce::String& aliases,
              const juce::String& manual, Scheme scheme)
{
    Profile p;
    p.id = "strymon." + id; p.brand = "Strymon"; p.model = model; p.shortName = shortName;
    p.aliases = "strymon " + aliases; p.colour = strymonColour; p.manual = manual;
    p.pedal = true; p.scheme = scheme;
    p.sceneWord = "Scene"; p.sceneCount = 0;
    p.mainTitle = "Controls";
    p.padCc = -1;   // every preset tile sends its CC#0 bank, and CC#0 = 0 alone only picks bank 0 for the next PC
    return p;
}

// TimeLine, BigSky, Mobius: 5-pin MIDI In and Out (THRU / MERGE / OFF), no USB.
Profile firstGen (const juce::String& id, const juce::String& model, const juce::String& aliases, const juce::String& manual, Scheme scheme)
{
    auto p = base (id, model, model, aliases, manual, scheme);
    p.usbMidi = false; p.usbToThru = 0;
    p.channelHint = "Must match the " + model + "'s MIDI Channel global setting (1-16; the manual doesn't give the default).";
    p.testMessage = "CC 0=0, PC 0";   // 00A
    return p;
}

// TimeLine MX, BigSky MX: 5-pin MIDI In and Out, USB-C MIDI, TRS MIDI on the EXP jack. THRU / Merge echo only on the port
// the message came in on, so USB MIDI never reaches the 5-pin Out.
Profile mx (const juce::String& id, const juce::String& model, const juce::String& shortName, const juce::String& aliases, const juce::String& manual)
{
    auto p = base (id, model, shortName, aliases, manual, Scheme::strymonMX);
    p.usbToThru = 0;
    p.channelHint = "Must match the " + model + "'s MIDI Channel global setting (1 out of the box).";
    p.testMessage = "CC 0=0, PC 0";   // 000A
    return p;
}

// The small pedals: 300 presets 0-299 = CC#0 bank 0-2 + PC 0-127, some programs with a fixed meaning.
Profile small (const juce::String& id, const juce::String& model, const juce::String& aliases, const juce::String& manual,
               const Reserved& reserved, bool usb, const juce::String& midiIn)
{
    auto p = base (id, model, model, aliases, manual, Scheme::numbered);
    p.presetCount = 300; p.labelFrom = 0; p.sendBankCc0 = true;
    p.reservedPrograms = reserved;
    p.usbMidi = usb; p.midiIn = midiIn;
    p.hasThru = false;   // one TRS jack: its MIDI Out (ring) can't feed a MIDI cable to a second device on its own
    p.usbToThru = 0;
    p.channelHint = smallChannel;
    const auto favourite = ! reserved.empty() && reserved.front().first == 0;
    p.testMessage = favourite ? "CC 0=0, PC 4" : "CC 0=0, PC 0";   // PC 0-3 are the Favorite and MultiSwitch Plus programs
    return p;
}

const Reserved manualOnly { { 127, "Manual mode" } };
const Reserved withFavorite { { 0, "Favorite" }, { 1, "MultiSwitch 1" }, { 2, "MultiSwitch 2" }, { 3, "MultiSwitch 3" }, { 127, "Manual mode" } };
// cloudburst, Brig, Olivera, Ultraviolet: PC 127 is Manual mode in every bank (127 and 255; bank 2 ends at 299).
const Reserved manualEveryBank { { 0, "Favorite" }, { 1, "MultiSwitch 1" }, { 2, "MultiSwitch 2" }, { 3, "MultiSwitch 3" },
                                 { 127, "Manual mode" }, { 255, "Manual mode" } };

const juce::String usbTrs ("TRS MIDI In (EXP/MIDI jack)");
const juce::String usbTrsTip ("TRS MIDI In (EXP/MIDI jack, tip)");

juce::String presetsNote (const Reserved& r)
{
    juce::String s ("Presets: 300, numbered 0-299. Each preset tile sends the bank (CC#0: 0 = 0-127, 1 = 128-255, 2 = 256-299) "
                    "and then Program Change = the number within the bank.");
    const auto everyBank = std::any_of (r.begin(), r.end(), [] (const auto& e) { return e.first == 255; });
    if (everyBank)
        s << " Program Change 0 = the Favorite setting (MiniSwitch), 1-3 = MultiSwitch Plus footswitches 1-3, 127 = Manual mode "
             "(the knobs; nothing can be stored there) in every bank, so 127 and 255 aren't presets. The manual doesn't say whether "
             "PC 0-3 in banks 1 and 2 are special.";
    else if (r.size() > 2)
        s << " Program Change 0 = the Favorite setting (MiniSwitch), 1-3 = MultiSwitch Plus footswitches 1-3, and bank 0's PC 127 "
             "= Manual mode (the knobs; nothing can be stored there).";
    else
        s << " Bank 0's PC 127 = Manual mode (the knobs; nothing can be stored there).";
    return s;
}

void engage (std::vector<Action>& a, int number, const juce::String& what)
{
    a.push_back ({ "Engage", cc (number, 127), "Turns " + what + " on (CC#" + n (number) + ").", 3 });
    a.push_back ({ "Bypass", cc (number, 0), "Bypasses " + what + " (CC#" + n (number) + " = 0).", 9 });
}

// An on/off CC as two tiles, with the device's thresholds.
void onOff (std::vector<Action>& a, const juce::String& on, const juce::String& off, int number, int onValue, int offValue,
            const juce::String& note, int colour)
{
    a.push_back ({ on, cc (number, onValue), note, colour });
    a.push_back ({ off, cc (number, offValue), note, 9 });
}

// A documented enumeration: CC#number = first, first + 1, ... (the Models view).
ModelGroup choices (const juce::String& title, int number, int first, std::initializer_list<const char*> names, const juce::String& note,
                    int colour = 3)
{
    ModelGroup g { title, "CC#" + n (number) + " = " + n (first) + "-" + n (first + (int) names.size() - 1), {} };
    int v = first;
    for (const auto* name : names)
        g.actions.push_back ({ name, cc (number, v++), note, colour });
    return g;
}

const juce::String setsLoaded ("Sets it in the loaded preset, like turning the knob.");
const juce::String smallExp ("Expression (CC#100, 0 = heel, 127 = toe) moves what the preset's expression setup assigns, when MIDI "
                             "Expression is on (CC#60). The others move that knob like turning it.");

// A parameter on the Expression view with its real range (0-max).
Control prm (const juce::String& name, int number, int max = 127)
{
    Control c;
    c.name = name; c.cc = number; c.max = max;
    return c;
}

// Footswitch presses: `down` then `up` 1/16 later, like a foot on the switch.
void presses (Profile& p, std::initializer_list<std::pair<const char*, int>> list, int down, int up, const juce::String& note)
{
    p.switchesTitle = "Footswitches";
    p.switchesOnOff = false;
    for (const auto& [name, number] : list)
    {
        Control c;
        c.name = name; c.cc = number; c.value = down; c.offValue = up; c.press = true;
        p.switches.push_back (c);
    }
    p.switchesNote = note;
}

const juce::String pressGen1 ("Like pressing that footswitch: down = 0, then up = 127 1/16 later (A CC#80, TAP 81, B 82). The manual "
                              "prints A and B as down = 0 / up = 127 and TAP as off = 0 / on = 127; TAP is sent the same way as A and "
                              "B (unconfirmed on hardware). What a press does depends on the pedal's mode, as on the pedal.");
const juce::String pressMx ("Like pressing that footswitch: 0 = press, then 127 = release 1/16 later (CC#80-82). What a press does "
                            "depends on the pedal's mode, as on the pedal.");
const juce::String pressSmall ("Like pressing that footswitch: 127 (press), then 0 (release) 1/16 later (0 = release, 1-127 = press). "
                               "What a press does depends on the pedal's mode, as on the pedal.");

// The values of a documented enumeration as tiles, CC#number = first, first + 1, ...; "Trem: Square" with a prefix.
void values (std::vector<Action>& a, const juce::String& prefix, int number, int first, std::initializer_list<const char*> names,
             const juce::String& note, int colour)
{
    int v = first;
    for (const auto* name : names)
        a.push_back ({ prefix.isEmpty() ? juce::String (name) : prefix + ": " + name, cc (number, v++), note, colour });
}

juce::String onlyIn (const juce::String& what, int number, const juce::String& machine)
{
    return what + " (CC#" + n (number) + "). Only acts when the loaded preset uses the " + machine + " machine.";
}

ModelGroup group (const juce::String& title, const juce::String& hint) { return { title, hint, {} }; }

// The type encoder: the manuals give CC#number = 0-11 but not which value is which machine, so the tiles are numbered and the
// group carries an amber warning (the gap is Strymon's, not ours).
ModelGroup typeKnob (const juce::String& title, int number, const juce::String& what)
{
    ModelGroup g { title, "CC#" + n (number) + " = 0-11", {},
                   "Strymon doesn't publish which number is which " + what + ": try them on your pedal" };
    const auto note = "Sends CC#" + n (number) + " = the tile's number. Strymon's manual gives the range (0-11) but not which "
                      + what + " each number picks, and midi.guide doesn't either: try them on your pedal and tell us what you find "
                      "(Help > Share an idea).";
    for (int i = 0; i < 12; ++i)
        g.actions.push_back ({ "Type " + n (i), cc (number, i), note, 3 });
    return g;
}

// MIDI Expression on / off (CC#60): CC#100 only moves the preset's expression setup while it's on.
void midiExp (std::vector<Action>& a, int onValue)
{
    onOff (a, "Exp on", "Exp off", 60, onValue, 0, "MIDI Expression on / off (CC#60: 0 = off, " + n (onValue) + " = on). The Expression "
           "pedal CC (CC#100) only moves the preset's expression setup while it's on.", 4);
}

// Scrolling the VALUE encoder one step (MX: CC#83, 0 = counter-clockwise, 1 = clockwise).
void scroll (std::vector<Action>& a)
{
    const juce::String note ("Like turning the VALUE encoder one step (CC#83: 0 = counter-clockwise, 1 = clockwise). What it changes "
                             "depends on the screen, as on the pedal.");
    a.push_back ({ "Scroll -", cc (83, 0), note, 7 });
    a.push_back ({ "Scroll +", cc (83, 1), note, 7 });
}

// The v2 pedals' MIDI clock tempo multiplier (CC#25 = 0-6), each pedal with its own list.
void clockDivision (std::vector<Action>& a, std::initializer_list<const char*> names)
{
    int v = 0;
    for (const auto* name : names)
        a.push_back ({ "Clk " + juce::String (name), cc (25, v++), "The MIDI clock tempo multiplier / divider (CC#25 = 0-6), when MIDI "
                       "clock is on.", 8 });
}
}

void addStrymon (std::vector<Profile>& v)
{
    // ---- TimeLine / BigSky / Mobius ----
    {
        auto p = firstGen ("timeline", "TimeLine", "timeline delay looper multidelay", "TimeLine User Manual (Rev F) and MIDI Program Changes chart",
                           Scheme::strymonAB);
        p.faceplate = juce::Colour (0xff595659);   // enclosure colour measured from the official product photo (strymon.net/product/timeline)
        engage (p.utilities, 102, "the TimeLine");
        p.utilities.push_back ({ "Remote tap", cc (93, 127), oneTap, 1 });
        p.utilities.push_back ({ "LFO reset", cc (125, 127), "Resets the modulation phase (CC#125). " + anyValue, 7 });
        onOff (p.utilities, "Clock on", "Clock off", 63, 1, 0, "MIDI clock sync for the loaded preset (CC#63: 0 = off, 1 = on).", 4);
        onOff (p.utilities, "Persist on", "Persist off", 22, 1, 0, "Delay trails continue when bypassed (CC#22: 0 = off, 1 = on).", 4);
        midiExp (p.utilities, 1);
        values (p.utilities, {}, 21, 0, { "Quarter", "Dotted 1/8", "Eighth", "Triplet", "16th" },
                "The tap division of the loaded preset (CC#21 = 0-4).", 1);
        values (p.utilities, "Ice", 46, 0, { "Short", "Medium", "Long" }, onlyIn ("Slice", 46, "Ice"), 3);
        // The other per-machine settings on the Models view (rows of six have room for "Filter: +Triangle").
        auto machine = group ("Machine settings", "dTape CC#58, dBucket 45, Digital 56, Dual 36, Filter 43, Duck 54"),
             lfo = group ("LFO shapes", "Trem CC#29 = 0-4, Filter CC#28 = 0-10"), loFi = group ("Lo-Fi filter", "CC#53 = 0-8");
        values (machine.actions, "dTape", 58, 0, { "Normal", "Fast" }, onlyIn ("Tape speed", 58, "dTape"), 6);
        values (machine.actions, "dBucket", 45, 0, { "Single", "Double" }, onlyIn ("Range", 45, "dBucket"), 6);
        values (machine.actions, "Digital", 56, 0, { "Dyn off", "Dyn on" }, onlyIn ("Repeat dynamics", 56, "Digital"), 6);
        values (machine.actions, "Dual", 36, 0, { "Series", "Parallel" }, onlyIn ("Configuration", 36, "Dual"), 6);
        values (machine.actions, "Filter", 43, 0, { "Pre", "Post" }, onlyIn ("Filter location", 43, "Filter"), 5);
        values (machine.actions, "Duck", 54, 0, { "Normal", "Gate" }, onlyIn ("Feedback ducking", 54, "Duck"), 8);
        values (lfo.actions, "Trem", 29, 0, { "Triangle", "Square", "Sine", "Ramp", "Saw" }, onlyIn ("LFO shape", 29, "Trem"), 5);
        values (lfo.actions, "Filter", 28, 0, { "+Triangle", "-Triangle", "-Square", "+Square", "-Sine", "+Sine", "Ramp", "Down", "Up",
                                                "Saw", "Random" },
                onlyIn ("LFO shape", 28, "Filter"), 7);
        values (loFi.actions, "Lo-Fi", 53, 0, { "Off", "Vintage amp", "Victrola", "Clock radio", "Bullhorn", "Cheerleader", "Telephone",
                                                "Cell phone", "Intercom" },
                onlyIn ("Filter shape", 53, "Lo-Fi"), 6);
        p.models = { typeKnob ("Delay machine", 19, "machine"), machine, lfo, loFi };
        presses (p, { { "A", 80 }, { "Tap", 81 }, { "B", 82 } }, 0, 127, pressGen1);
        for (auto& sw : p.switches)   // the manual prints TAP as "off = 0, on = 127" (A / B: "down = 0, up = 127"): press = 127, then 0
            if (sw.cc == 81)
            {
                sw.value = 127;
                sw.offValue = 0;
            }
        p.looper = { { "Record", cc (87, 127), anyValue, 0 }, { "Play", cc (86, 127), anyValue, 3 }, { "Stop", cc (85, 127), anyValue, 9 },
                     { "Reverse", cc (94, 127), "Forward / reverse (CC#94). " + toggles, 5 },
                     { "Half speed", cc (95, 127), "Full / half speed (CC#95). " + toggles, 7 },
                     { "Pre/post", cc (96, 127), "Looper before / after the delay (CC#96). " + toggles, 8 },
                     { "Undo", cc (89, 127), "Back to the initial loop (CC#89). " + anyValue, 6 }, { "Redo", cc (90, 127), anyValue, 6 } };
        p.pedals = { { "Expression", 100 }, { "Time", 3 }, { "Repeats", 9 }, { "Mix", 14 }, { "Filter", 15 }, { "Grit", 16 },
                     { "Speed", 17 }, { "Depth", 18 }, { "Looper level", 98 }, prm ("Boost", 23, 60), prm ("Smear", 38, 18),
                     prm ("High pass", 47, 20), prm ("dTape Low end", 59, 20), prm ("Dual Time 2", 32, 26), prm ("Dual Repeats 2", 34, 18),
                     prm ("Dual Mix 2", 33, 18), prm ("Pattern", 39, 15), prm ("Swell Rise", 44, 27), prm ("Trem Speed", 61, 34),
                     prm ("Trem Depth", 57, 18), prm ("Filter Q", 40, 11), prm ("Filter Depth", 41, 18), prm ("Filter Speed", 42, 34),
                     prm ("Lo-Fi Mix", 51, 20), prm ("Lo-Fi Vinyl", 52, 18), prm ("Lo-Fi Sample rate", 49, 20), prm ("Lo-Fi Bit depth", 50, 20),
                     prm ("Ice Blend", 25, 20), prm ("Duck Sensitivity", 37, 17), prm ("Duck Release", 55, 20),
                     prm ("Ice Interval", 30, 29) };
        p.pedalNote = "Expression (CC#100) moves what the preset's expression setup assigns, when Expression is on (CC#60). The others "
                      "move that knob or parameter like turning it, in its own range; the ones named after a machine only act when the "
                      "loaded preset uses it.";
        p.notes = "Sources: TimeLine User Manual Rev F (MIDI specification, p.25-26) and its MIDI Program Changes chart; midi.guide "
                  "(community chart) for value meanings the manual's table doesn't print.\n\n"
                  "Presets: 200, 00A-99B. Each preset tile sends the MIDI bank (CC#0: 0 = 00A-63B, 1 = 64A-99B) and then Program Change "
                  "(PC 0 = 00A, 1 = 00B, 2 = 01A...). It powers up in bank 0.\n\nControls: the A / TAP / B footswitches as presses "
                  "(CC#80 / 81 / 82, down = 0, then up = 127; the manual prints TAP as off = 0 / on = 127, sent the same way, "
                  "unconfirmed), engage / bypass (CC#102: 0 = bypass, "
                  "127 = engage), remote tap (CC#93), phase reset (CC#125), MIDI clock (CC#63), persist (CC#22), MIDI expression on / off "
                  "(CC#60) and the tap division (CC#21: Quarter, Dotted Eighth, Eighth, Triplets, Sixteenth: the manual's parameter page "
                  "lists them in that order and midi.guide gives the same values). Per-machine settings act only when the preset uses "
                  "that machine: dTape tape speed (CC#58), dBucket range (CC#45), Digital repeat dynamics (CC#56), Dual config (CC#36), "
                  "Trem LFO shape (CC#29), Filter location (CC#43), Duck feedback (CC#54), Filter LFO shape (CC#28) and Lo-Fi filter "
                  "shape (CC#53) on the Models view, Ice slice (CC#46) on Controls. The manual's table gives only the ranges of these "
                  "per-machine settings; their value names come from midi.guide (community chart). The Ice interval (CC#30, 0-29) is "
                  "on Expression, because 30 tiles don't fit: midi.guide gives 0 -Oct, 1 -Maj7, 2 -min7, 3 -Maj6, 4 -min6, 5 -P5, "
                  "6 -min3, 7 -Maj2, 8 +Maj2, 9 +min3, 10 +Maj3, 11 +P4, 12 +Tritone, 13 +min7, 14 +Maj7, 15 +Oct, 16 +Oct+5th, "
                  "17 +2 Oct, 18 -Tritone, 19 -P4, 20 -Maj3, 21 -min2, 22 -50c, 23 -25c, 24 +25c, 25 +50c, 26 +min2, 27 +P5, "
                  "28 +min6, 29 +Maj6. midi.guide's Filter LFO order (4 = "
                  "-Sine, 5 = +Sine, 7 = Down, 8 = Up, 9 = Saw, 10 = Random) and Ice interval order differ from the lists the TimeLine "
                  "MX manual prints for its own Filter and Ice types, so check those on the pedal.\n\nExpression: every knob and "
                  "parameter in its own range (Boost 0-60, Smear 0-18, High Pass 0-20, Dual Time 2 0-26, Pattern 0-15 = patterns 1-16, "
                  "Swell rise 0-27, Trem / Filter speed 0-34, Ice interval 0-29...).\n\nLooper: record CC#87, play 86, stop 85, "
                  "undo 89, redo 90 (any value); "
                  "reverse 94, half speed 95 and pre/post 96 toggle; looper level CC#98. The manual also lists MIDI notes for absolute "
                  "reverse / half speed (notes 103, 104): tiles only send CC and PC, so those aren't here.\n\nModels: the delay type "
                  "encoder (CC#19 = 0-11) as numbered tiles Type 0-11: neither the manual nor midi.guide says which number is which "
                  "machine (midi.guide: probably the knob's order), so try them on the pedal.\n\nConnection: 5-pin MIDI In and Out "
                  "(MIDI Through: THRU, MERGE or OFF). No USB MIDI. The MIDI channel default isn't stated in the manual.";
        v.push_back (p);
    }
    {
        auto p = firstGen ("bigsky", "BigSky", "bigsky big sky reverb", "BigSky User Manual (Rev D)", Scheme::strymonABC);
        p.faceplate = juce::Colour (0xff017da4);   // enclosure colour measured from the official product photo (strymon.net/product/bigsky)
        engage (p.utilities, 102, "the BigSky");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Hold", "Release", 97, 127, 0,
               "Like holding the preset footswitch (CC#97: 127 = held, 0 = let go): Infinite Sustain or Freeze, as the preset's Hold "
               "parameter says.", 5);
        values (p.utilities, {}, 70, 0, { "Freeze mode", "Inf. mode" }, "What Hold does in the loaded preset (CC#70: 0 = Freeze, 1 = Infinite, "
                "from midi.guide).", 5);
        onOff (p.utilities, "Persist on", "Persist off", 22, 1, 0, "Reverb trails continue when bypassed (CC#22: 0 = off, 1 = on).", 4);
        onOff (p.utilities, "Clock on", "Clock off", 71, 1, 0, "MIDI clock sync for the loaded preset (CC#71: 0 = off, 1 = on).", 4);
        midiExp (p.utilities, 1);
        auto spaces = group ("Room, Hall, Plate, Swell, Shimmer", "CC#59, 40, 68, 67 = 0-1, CC#28 = 0-2"),
             spring = group ("Spring, Magneto", "CC#63 = 0-3, CC#62 = 0-2, CC#57 = 0-2, CC#54 = 0-1"),
             shapes = group ("Chorale, Nonlinear, Reflections", "CC#33 = 0-6, CC#34 = 0-2, CC#46 = 0-5, CC#51 = 0-2");
        values (spaces.actions, "Room", 59, 0, { "Studio", "Club" }, onlyIn ("Room size", 59, "Room"), 6);
        values (spaces.actions, "Hall", 40, 0, { "Concert", "Arena" }, onlyIn ("Hall size", 40, "Hall"), 6);
        values (spaces.actions, "Plate", 68, 0, { "Small", "Large" }, onlyIn ("Plate size", 68, "Plate"), 6);
        values (spaces.actions, "Swell", 67, 0, { "Wet", "Dry" }, onlyIn ("Swell mode", 67, "Swell"), 7);
        values (spaces.actions, "Shimmer", 28, 0, { "Input", "Regen", "In+Regen" }, onlyIn ("Shimmer mode", 28, "Shimmer"), 8);
        values (spring.actions, "Spring", 63, 0, { "Clean", "Combo", "Tube", "Overdrive" }, onlyIn ("Dwell", 63, "Spring"), 3);
        values (spring.actions, "Spring", 62, 0, { "1 spring", "2 springs", "3 springs" }, onlyIn ("Number of springs", 62, "Spring"), 3);
        values (spring.actions, "Magneto", 57, 0, { "3 heads", "4 heads", "6 heads" }, onlyIn ("Number of heads", 57, "Magneto"), 7);
        values (spring.actions, "Magneto", 54, 0, { "Even", "Uneven" }, onlyIn ("Spacing", 54, "Magneto"), 7);
        values (shapes.actions, "Chorale", 33, 0, { "AAHHOO", "AAHH", "AAHHOH", "OH", "OOOHOH", "OOOO", "Random" },
                onlyIn ("Vowel", 33, "Chorale"), 6);
        values (shapes.actions, "Chorale", 34, 0, { "Mild reso", "Medium reso", "High reso" }, onlyIn ("Resonance", 34, "Chorale"), 6);
        values (shapes.actions, "Nonlinear", 46, 0, { "Swoosh", "Reverse", "Ramp", "Gate", "Gauss", "Bounce" }, onlyIn ("Shape", 46, "Nonlinear"), 5);
        values (shapes.actions, "Reflections", 51, 0, { "Square", "Rect", "Oblong" }, onlyIn ("Room shape", 51, "Reflections"), 5);
        p.models = { typeKnob ("Reverb machine", 19, "machine"), spaces, spring, shapes };
        presses (p, { { "A", 80 }, { "C", 81 }, { "B", 82 } }, 0, 127,
                 "Like pressing that footswitch: down = 0, then up = 127 1/16 later (A CC#80, B CC#82, C CC#81). What a press does "
                 "depends on the pedal's mode, as on the pedal.");
        p.pedals = { { "Expression", 100 }, { "Decay", 17 }, { "Pre-delay", 18 }, { "Mix", 15 }, { "Tone", 3 }, { "Param 1", 9 },
                     { "Param 2", 16 }, { "Mod", 14 }, prm ("Boost", 23, 60), prm ("Room Low end", 61, 20), prm ("Room Diffusion", 58, 20),
                     prm ("Hall Low end", 39, 20), prm ("Hall Mid", 42, 20), prm ("Plate Low end", 69, 20), prm ("Spring Low end", 64, 20),
                     prm ("Swell Low end", 65, 20), prm ("Swell Rise", 66, 22), prm ("Bloom Low end", 31, 20), prm ("Bloom Length", 32, 17),
                     prm ("Bloom Feedback", 30, 17), prm ("Cloud Low end", 38, 20), prm ("Cloud Diffusion", 37, 20),
                     prm ("Shimmer Amount", 27, 18), prm ("Shimmer Low end", 24, 20), prm ("Magneto Low end", 55, 20),
                     prm ("Magneto Diffusion", 56, 20), prm ("Nonlinear Low end", 44, 20), prm ("Nonlinear Diffusion", 45, 20),
                     prm ("Nonlinear Late decay", 47, 17), prm ("Nonlinear Late level", 48, 18), prm ("Nonlinear Mod speed", 43, 17),
                     prm ("Reflections Low end", 52, 20), prm ("Reflections Loc Y", 50, 6), prm ("Reflections Loc X", 49, 6) };
        p.pedalNote = "Expression (CC#100) moves what the preset's expression setup assigns, when Expression is on (CC#60). The others "
                      "move that knob or parameter like turning it, in its own range; the ones named after a machine only act when the "
                      "loaded preset uses it.";
        p.notes = "Sources: BigSky User Manual Rev D (MIDI specification, p.23-24); midi.guide (community chart) for value meanings the "
                  "manual's table doesn't print.\n\nPresets: 300, 00A-99C. Each preset tile sends the MIDI bank (CC#0: 0 = 00A-42B, "
                  "1 = 42C-85A, 2 = 85B-99C) and then Program Change (PC 0 = 00A, 1 = 00B, 2 = 00C, 3 = 01A...). It powers up in bank "
                  "0.\n\nControls: the A / C / B footswitches as presses (CC#80 / 81 / 82, down = 0, up = 127), engage / bypass (CC#102: "
                  "0 = bypass, 127 = engage), remote tap (CC#93), Hold / Release (CC#97, like holding the preset switch), what Hold does "
                  "(CC#70: 0 = Freeze, 1 = Infinite: midi.guide (community chart); the manual only says Freeze/Infinite 0-1), persist "
                  "(CC#22), MIDI clock (CC#71) and MIDI expression on / off (CC#60). Per-machine settings act only when the preset uses "
                  "that machine, on the Models view: Room / Hall / Plate size, Swell mode, Shimmer mode, Spring dwell and number of "
                  "springs, Magneto heads and spacing, Chorale vowel and resonance, Nonlinear and Reflections shape. The manual's table gives only "
                  "their ranges; the value names come from midi.guide (community chart).\n\nExpression: every knob and parameter in its "
                  "own range (Boost 0-60, Low end 0-20, Swell rise 0-22, Bloom length 0-17, Reflections location 0-6...).\n\nThe reverb type "
                  "encoder (CC#19 = 0-11) is on Models as numbered tiles Type 0-11: neither source says which number is which machine. "
                  "Left out: the value encoder "
                  "(CC#20, 0-1: the manual doesn't say what each value does); Shimmer shift 1 / 2 (CC#25, 26): neither source gives the "
                  "interval of each value. No looper or tuner.\n\nConnection: 5-pin MIDI In and Out (MIDI Through: THRU, MERGE or OFF). "
                  "No USB MIDI. The MIDI channel default isn't stated in the manual.";
        v.push_back (p);
    }
    {
        auto p = firstGen ("mobius", "Mobius", "mobius modulation chorus flanger phaser rotary vibe tremolo", "Mobius User Manual (Rev F)",
                           Scheme::strymonAB);
        p.faceplate = juce::Colour (0xff015ca2);   // enclosure colour measured from the official product photo (strymon.net/product/mobius)
        engage (p.utilities, 102, "the Mobius");
        p.utilities.push_back ({ "Remote tap", cc (93, 127), oneTap, 1 });
        p.utilities.push_back ({ "LFO reset", cc (125, 127), "Resets the modulation phase (CC#125). " + anyValue, 7 });
        onOff (p.utilities, "Clock on", "Clock off", 70, 1, 0, "MIDI clock sync for the loaded preset (CC#70: 0 = off, 1 = on).", 4);
        midiExp (p.utilities, 1);
        p.models = { typeKnob ("Effect type", 19, "effect") };
        presses (p, { { "A", 80 }, { "Tap", 81 }, { "B", 82 } }, 0, 127, pressGen1);
        for (auto& sw : p.switches)   // the manual prints TAP as "off = 0, on = 127" (A / B: "down = 0, up = 127"): press = 127, then 0
            if (sw.cc == 81)
            {
                sw.value = 127;
                sw.offValue = 0;
            }
        p.pedals = { { "Expression", 100 }, { "Speed", 17 }, { "Depth", 18 }, { "Level", 15 }, { "Param 1", 9 }, { "Param 2", 16 },
                     prm ("Chorus Mix", 29, 17), prm ("Chorus Tone", 30, 20), prm ("Flanger Regen", 25, 17), prm ("Flanger Manual", 26, 17),
                     prm ("Rotary Horn level", 34, 17), prm ("Rotary Drive", 35, 17), prm ("Rotary Slow speed", 36, 17),
                     prm ("Rotary Acceleration", 37, 17), prm ("Vibe Waveshape", 40, 17), prm ("Vibe Low end", 41, 20),
                     prm ("Vibe Headroom", 42, 17), prm ("Phaser Regen", 45, 17), prm ("Phaser Spread", 47, 4), prm ("Phaser Headroom", 68, 17),
                     prm ("Filter Resonance", 50, 18), prm ("Filter Dry level", 51, 18), prm ("Filter Freq middle", 52, 20),
                     prm ("Filter Spread", 69, 4), prm ("Formant Spread", 115, 4), prm ("Pattern Beat 1", 105, 17),
                     prm ("Pattern Beat 2", 106, 18), prm ("Pattern Beat 3", 107, 18), prm ("Pattern Beat 4", 108, 18),
                     prm ("Pattern Beat 5", 109, 18), prm ("Pattern Beat 6", 110, 18), prm ("Pattern Beat 7", 111, 18),
                     prm ("Pattern Beat 8", 112, 18), prm ("Autoswell Rise", 57, 22), prm ("Destroyer Bit depth", 59, 20),
                     prm ("Destroyer Sample rate", 61, 20), prm ("Destroyer Vinyl", 63, 18), prm ("Destroyer Mix", 64, 20),
                     prm ("Quadrature Shift", 54, 17), prm ("Quadrature Mix", 55, 20) };
        p.pedalNote = "Expression (CC#100) moves what the preset's expression setup assigns, when Expression is on (CC#60). The others "
                      "move that knob or parameter like turning it, in its own range; the ones named after an effect only act when the "
                      "loaded preset uses it.";
        p.notes = "Sources: Mobius User Manual Rev F (MIDI specification, p.24-25). midi.guide has no Mobius chart.\n\nPresets: 200, "
                  "00A-99B. Each preset tile sends the MIDI bank (CC#0: 0 = 00A-63B, 1 = 64A-99B) and then Program Change (PC 0 = 00A, "
                  "1 = 00B, 2 = 01A...). It powers up in bank 0.\n\nControls: the A / TAP / B footswitches as presses (CC#80 / 81 / 82; "
                  "down = 0, then up = 127; the manual prints TAP as off = 0 / on = 127, sent the same way, unconfirmed), engage / "
                  "bypass (CC#102: 0 = bypass, 127 = engage), remote "
                  "tap (CC#93), phase reset (CC#125), MIDI clock on / off (CC#70) and MIDI expression on / off (CC#60). A MIDI Start also "
                  "resets the modulation phase.\n\nExpression: every knob and every per-effect amount in its own range (most 0-17 or "
                  "0-20; stereo spread 0-4). The ones named after an effect only act when the preset uses it.\n\nThe effect type "
                  "encoder (CC#19 = 0-11) is on Models as numbered tiles Type 0-11: the manual doesn't say which number is which "
                  "effect.\n\nLeft out, because the manual gives only a range and no value names: tap division (CC#21, 0-6), pre/post "
                  "(CC#22), tap switch tap/speed and rotary tap select (both printed as CC#39), and the per-effect modes and shapes: "
                  "Chorus mode (CC#28), Flanger mode (24), Vibe mode (43), Phaser mode (44) and waveshape (46), Filter mode (48) and "
                  "waveshape (49), Formant vowels 1 / 2 (65, 66) and LFO (67), Vintage Trem mode (31) and pan (32), Pattern Trem waveshape "
                  "(113) and pan (114), Autoswell shape (58), Destroyer filter (62), Quadrature mode (53) and LFO (56).\n\nConnection: "
                  "5-pin MIDI In and Out (MIDI Through: THRU, MERGE or OFF). No USB MIDI. The MIDI channel default isn't stated in the "
                  "manual.";
        v.push_back (p);
    }

    // ---- MX ----
    {
        auto p = mx ("timeline-mx", "TimeLine MX", "TimeLine MX", "timeline mx dual delay looper", "TimeLine MX User Manual (Rev C)");
        p.faceplate = juce::Colour (0xff404245);   // enclosure colour measured from the official product photo (strymon.net/product/timeline-mx)
        engage (p.utilities, 102, "the preset");
        onOff (p.utilities, "Delay 1 on", "Delay 1 off", 31, 127, 0, "Delay 1 on / bypassed (CC#31: 0 = bypassed, 1-127 = engaged).", 3);
        onOff (p.utilities, "Delay 2 on", "Delay 2 off", 32, 127, 0, "Delay 2 on / bypassed (CC#32: 0 = bypassed, 1-127 = engaged).", 3);
        p.utilities.push_back ({ "Remote tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Inf. on", "Inf. off", 97, 127, 0, "Infinite repeats (CC#97: 0 = off, 1-127 = on).", 5);
        onOff (p.utilities, "Persist on", "Persist off", 123, 127, 0, "Delay trails continue when bypassed (CC#123: 0 = off, 1-127 = on).", 4);
        const std::initializer_list<const char*> divisions { "1/4", "1/8 dot", "1/8", "Triplet", "1/16", "Golden", "Silver", "Free" };
        values (p.utilities, "D1", 17, 0, divisions, "Delay 1's tap division in the loaded preset (CC#17 = 0-7).", 1);
        values (p.utilities, "D2", 18, 0, divisions, "Delay 2's tap division in the loaded preset (CC#18 = 0-7).", 6);
        presses (p, { { "A", 80 }, { "B", 81 }, { "Tap", 82 } }, 0, 127, pressMx);
        p.looper = { { "Record", cc (119, 127), anyValue, 0 }, { "Play", cc (120, 127), anyValue, 3 }, { "Stop", cc (121, 127), anyValue, 9 },
                     { "Reverse", cc (125, 127), "Forward / reverse (CC#125). " + toggles, 5 },
                     { "Half speed", cc (126, 127), "Full / half speed (CC#126). " + toggles, 7 },
                     { "Pre/post", cc (84, 127), "The looper before / after the delays (CC#84, global). " + toggles, 8 },
                     { "Undo", cc (98, 127), "Back to the original loop (CC#98). " + anyValue, 6 }, { "Redo", cc (99, 127), anyValue, 6 } };
        const char* types[] = { "Spectral", "Reverse", "Ice", "Lo Fi", "Filter", "Reverb", "dTape", "dBucket", "Digital", "Drum", "Oil Can", "MultiTap" };
        const juce::String typeNote ("Changes the delay type of the loaded preset.");
        ModelGroup d1 { "Delay 1", "CC#1 = 0-11", {} }, d2 { "Delay 2", "CC#2 = 0-11", {} };
        for (int i = 0; i < 12; ++i)
        {
            d1.actions.push_back ({ types[i], cc (1, i), typeNote, 3 });
            d2.actions.push_back ({ types[i], cc (2, i), typeNote, 6 });
        }
        p.models = { d1, d2, choices ("Dual mode", 124, 0, { "Off", "Parallel", "Series 1>>2", "Series 1<<2", "Split L|R", "Split R|L" },
                                      "How the two delays are combined in the loaded preset (CC#124).", 7) };
        p.pedals = { { "Expression", 100 }, { "Time 1", 3 }, { "Time 2", 4 }, { "Repeats 1", 5 }, { "Repeats 2", 6 }, { "Filter 1", 11 },
                     { "Filter 2", 12 }, { "Grit 1", 13 }, { "Grit 2", 14 }, { "Mix 1", 15 }, { "Mix 2", 16 }, { "Param 1 (D1)", 19 },
                     { "Param 1 (D2)", 20 }, { "Param 2 (D1)", 21 }, { "Param 2 (D2)", 22 }, { "Input 1", 113 }, { "Input 2", 114 } };
        // Both delays' parameters, CC pairs (delay 1, delay 2): "Smear 1", "Smear 2".
        auto pair = [&p] (const juce::String& name, int number1, int number2, int max)
        {
            p.pedals.push_back (prm (name + " 1", number1, max));
            p.pedals.push_back (prm (name + " 2", number2, max));
        };
        pair ("Output level", 7, 8, 100);   pair ("Pan", 9, 10, 16);       pair ("Swell", 23, 24, 28);
        pair ("Duck release", 25, 26, 20);  pair ("Duck sensitivity", 27, 28, 18);
        pair ("Mod speed", 29, 30, 127);    pair ("Mod depth", 33, 34, 127);
        pair ("Smear", 35, 36, 18);         pair ("High pass", 37, 38, 19);
        p.pedals.push_back (prm ("Boost", 122, 60));
        p.pedals.push_back (prm ("Looper level", 127, 100));
        pair ("Spectral Octave", 109, 110, 20);   pair ("Spectral Density", 29, 30, 14);
        pair ("Ice Blend", 57, 58, 20);           pair ("Lo Fi Sample rate", 59, 60, 20);  pair ("Lo Fi Bit depth", 61, 62, 20);
        pair ("Lo Fi Mix", 63, 64, 20);           pair ("Lo Fi Vinyl", 65, 66, 18);
        pair ("Filter Speed", 71, 72, 34);        pair ("Filter Depth", 73, 74, 18);       pair ("Filter Q", 75, 76, 11);
        pair ("Trem Speed", 85, 86, 34);          pair ("Trem Depth", 87, 88, 18);
        pair ("dTape Low contour", 41, 42, 21);   pair ("Drum Low cut", 115, 116, 127);    pair ("MultiTap Pattern", 91, 92, 15);
        // Per-type settings with named values: too many to fit as tiles (two delays), so they're here with their ranges.
        pair ("Spectral Grain shape", 103, 104, 4);   pair ("Spectral Direction", 107, 108, 2);  pair ("Spectral Density sync", 111, 112, 1);
        pair ("Ice Interval", 53, 54, 29);            pair ("Ice Slice", 55, 56, 2);             pair ("Lo Fi Filter shape", 67, 68, 8);
        pair ("Filter LFO", 69, 70, 10);              pair ("Trem LFO", 77, 78, 4);              pair ("dTape Voice", 43, 44, 1);
        pair ("dBucket Voice", 45, 46, 1);            pair ("Digital Repeat dynamics", 47, 48, 1); pair ("Digital Voice", 49, 50, 1);
        pair ("Drum Spacing", 117, 118, 3);           pair ("Oil Can Head", 51, 52, 2);          pair ("MultiTap Grid", 89, 90, 3);
        pair ("MultiTap Feedback mode", 95, 96, 5);
        p.pedalNote = "Expression (CC#100) works when the preset's EXP Setup has MIDI EXP on, and moves what that setup assigns. The others "
                      "move that parameter of delay 1 or 2 in its own range; the ones named after a delay type only act when that delay "
                      "uses it. Settings with named values (Grain shape, Interval, LFO, Voice...) jump between them: see About this unit.";
        p.notes = "Sources: TimeLine MX User Manual Rev C (MIDI reference, p.97-105; factory globals p.106). midi.guide has no TimeLine "
                  "MX chart.\n\nPresets: 300, 000A-149B. Each preset tile sends the MIDI bank (CC#0: 0 = 000A-063B, 1 = 064A-127B, "
                  "2 = 128A-149B) and then Program Change (PC 0 = 000A, 1 = 000B, 2 = 001A...). It powers up in bank 0.\n\nControls: the "
                  "A / B / TAP footswitches as presses (CC#80 / 81 / 82: 0 = press, 127 = release), preset engage / bypass (CC#102), delay "
                  "1 / 2 on or bypassed (CC#31, 32; 0 = bypassed, 1-127 = engaged), remote tap (CC#93), infinite (CC#97), persist "
                  "(CC#123) and each delay's tap "
                  "division (CC#17, 18: Quarter, Dotted Eighth, Eighth, Triplet, Sixteenth, Golden Ratio, Silver Ratio, Free). Models: "
                  "delay 1 / 2 type (CC#1, 2 = 0-11) and the dual mode (CC#124).\n\nLooper (it answers outside Looper Mode too): record "
                  "CC#119, play 120, stop 121, undo 98, redo 99 (any value); reverse 125, half speed 126 and pre/post 84 toggle; looper "
                  "level CC#127 (0-100) is on Expression.\n\nExpression: every knob and parameter of both delays in its own range "
                  "(output level 0-100, pan 0-16 with 8 = centre, swell 0-28, smear 0-18, high pass 0-19, boost 0-60...). CC#29/30 and "
                  "33/34 change meaning with the delay type: modulation speed / depth, Spectral density 0-14 / stretch, dTape crinkle / "
                  "wow and flutter. The per-type settings with named values are there too, because two delays' worth don't fit as tiles: "
                  "Spectral grain shape (0 Soft, 1 Swell, 2 Soft Pluck, 3 Pluck, 4 Bounce), direction (0 Forward, 1 Reverse, 2 Both), "
                  "density sync (0 off, 1 on); Ice interval (0 -Octave ... 11 -Min 2nd, 12 -50 cents, 13 -25, 14 +25, 15 +50 cents, "
                  "16 +Min 2nd ... 27 +Octave, 28 +Octave & 5th, 29 +2 Octaves; printed as 0-27 with a list up to 29) and slice (0 Short, "
                  "1 Medium, 2 Long); Lo Fi filter shape (0 Off, 1 Vintage, 2 Victrola, 3 Clock Radio, 4 Bullhorn, 5 Cheerleader, "
                  "6 Antique Telephone, 7 Cell Phone, 8 Intercom); Filter LFO (0 +Triangle, 1 -Triangle, 2 -Square, 3 +Square, 4 +Sine, "
                  "5 -Sine, 6 Ramp, 7 Saw, 8 Random, 9 Down, 10 Up); Trem LFO (0 Triangle, 1 Square, 2 Sine, 3 Ramp, 4 Saw); dTape, "
                  "dBucket and Digital voice (0 MX, 1 Classic); Digital repeat dynamics (0 off, 1 on); Drum spacing (0 Even, 1 Triplet, "
                  "2 Golden Ratio, 3 Silver Ratio); Oil Can head (0 Long, 1 Short, 2 Both); MultiTap grid (0 16th, 1 Swing 16th, "
                  "2 Triplet, 3 Off), pattern (0-15 = Classic 1-16) and feedback mode (0-3 = 1-4 Beat, 4 Parallel, 5 Input). Use Draw or "
                  "Set to (heel = 0, toe = the top value) to pick one.\n\nLeft out: the VALUE encoder (CC#83: 0 = one step "
                  "counter-clockwise, 1 = clockwise), for room on "
                  "Controls; Spectral stretch's full 0-255 range (a CC only "
                  "reaches 127; CC#33/34 is on the pad as Mod depth); the looper's MIDI notes (tiles only send CC and PC).\n\n"
                  "Connection: 5-pin MIDI In and Out, USB MIDI, and TRS MIDI on the EXP jack (EXP MODE = MIDI). MIDI THRU is off out of "
                  "the box; THRU / Merge only echo on the port the message came in on, so MIDI from USB never reaches the 5-pin Out. "
                  "Channel 1 out of the box.";
        v.push_back (p);
    }
    {
        auto p = mx ("bigsky-mx", "BigSky MX", "BigSky MX", "bigsky mx big sky dual reverb", "BigSky MX User Manual (Rev B)");
        p.faceplate = juce::Colour (0xff0385ab);   // enclosure colour measured from the official product photo (strymon.net/product/bigsky-mx)
        engage (p.utilities, 102, "the preset");
        onOff (p.utilities, "Inf. on", "Inf. off", 97, 127, 0, "Infinite on / off (CC#97: 0 = off, 1-127 = on).", 5);
        onOff (p.utilities, "Persist on", "Persist off", 84, 127, 0, "Reverb trails continue when bypassed (CC#84: 0 = off, 1-127 = on).", 4);
        onOff (p.utilities, "Latching", "Momentary", 98, 127, 0, "How Infinite behaves (CC#98: 0 = momentary, 1-127 = latching).", 7);
        scroll (p.utilities);
        presses (p, { { "A", 80 }, { "B", 81 }, { "Infinite", 82 } }, 0, 127, pressMx);
        const char* infinite[] = { "Freeze", "Infinite", "Off" };
        p.models = { choices ("Dual mode", 99, 0, { "Off", "Parallel", "Series 1>>2", "Series 1<<2", "Split L|R", "Split R|L" },
                              "How the two reverbs are combined in the loaded preset (CC#99).", 7),
                     ModelGroup { "Reverb 1 infinite mode", "CC#17 = 0-2", {} }, ModelGroup { "Reverb 2 infinite mode", "CC#18 = 0-2", {} } };
        for (int i = 0; i < 3; ++i)
        {
            p.models[1].actions.push_back ({ infinite[i], cc (17, i), "Reverb 1's infinite mode in the loaded preset.", i == 2 ? 9 : 5 });
            p.models[2].actions.push_back ({ infinite[i], cc (18, i), "Reverb 2's infinite mode in the loaded preset.", i == 2 ? 9 : 6 });
        }
        p.models.insert (p.models.begin(), { typeKnob ("Reverb 1 type", 1, "reverb type"), typeKnob ("Reverb 2 type", 2, "reverb type") });
        p.pedals = { { "Expression", 100 }, { "Decay 1", 3 }, { "Decay 2", 4 }, { "Pre-delay 1", 5 }, { "Pre-delay 2", 6 }, { "Tone 1", 11 },
                     { "Tone 2", 12 }, { "Mod 1", 13 }, { "Mod 2", 14 }, { "Mix 1", 15 }, { "Mix 2", 16 }, { "Param 1 (R1)", 19 },
                     { "Param 1 (R2)", 20 }, { "Param 2 (R1)", 21 }, { "Param 2 (R2)", 22 } };
        auto pair = [&p] (const juce::String& name, int number1, int number2, int max)
        {
            p.pedals.push_back (prm (name + " 1", number1, max));
            p.pedals.push_back (prm (name + " 2", number2, max));
        };
        pair ("Output level", 7, 8, 16);   pair ("Pan", 9, 10, 16);   pair ("Low end", 23, 24, 20);
        p.pedals.push_back (prm ("Boost", 79, 60));
        pair ("Room Diffusion", 27, 28, 20);       pair ("Hall Mid", 35, 36, 20);            pair ("Hall Swell rise", 39, 40, 22);
        pair ("Impulse Attack", 57, 58, 16);       pair ("Impulse Stretch", 59, 60, 16);     pair ("Impulse Feedback", 65, 66, 15);
        pair ("Cloud Diffusion", 67, 68, 20);      pair ("Cloud Ensemble", 69, 70, 15);      pair ("Shimmer Amount", 75, 76, 18);
        pair ("Bloom Length", 87, 88, 17);         pair ("Bloom Feedback", 89, 90, 17);      pair ("Bloom Harmonics", 91, 92, 15);
        pair ("Chorale Choir", 103, 104, 15);      pair ("Magneto Diffusion", 107, 108, 20); pair ("Nonlinear Chop", 117, 118, 17);
        pair ("Nonlinear Diffusion", 119, 120, 20); pair ("Nonlinear Decay", 121, 122, 17);  pair ("Nonlinear Level", 123, 124, 18);
        pair ("Nonlinear Mod speed", 125, 126, 17);
        // Per-reverb settings with named values: two reverbs' worth don't fit as tiles, so they're here with their ranges.
        pair ("Room Size", 25, 26, 1);             pair ("Room Voice", 29, 30, 1);           pair ("Hall Size", 37, 38, 1);
        pair ("Hall Swell type", 41, 42, 1);       pair ("Hall Voice", 43, 44, 1);           pair ("Chamber Color", 45, 46, 4);
        pair ("Plate Size", 47, 48, 1);            pair ("Plate Voice", 49, 50, 1);          pair ("Spring Dwell", 51, 52, 3);
        pair ("Spring Number", 53, 54, 2);         pair ("Spring Voice", 55, 56, 1);         pair ("Impulse Tail", 61, 62, 1);
        pair ("Impulse Direction", 63, 64, 1);     pair ("Shimmer Feedback", 77, 78, 2);     pair ("Shimmer Voice", 85, 86, 1);
        pair ("Chorale Vowel", 93, 94, 6);         pair ("Chorale Resonance", 95, 96, 2);    pair ("Chorale Choir voice", 105, 106, 1);
        pair ("Magneto Heads", 109, 110, 4);       pair ("Magneto Spacing", 111, 112, 1);    pair ("Magneto Ping pong", 113, 114, 1);
        pair ("Nonlinear Shape", 115, 116, 5);
        p.pedalNote = "Expression (CC#100) works when the preset's EXP Setup has MIDI EXP on, and moves what that setup assigns. The others "
                      "move that parameter of reverb 1 or 2 in its own range; the ones named after a reverb type only act when that "
                      "reverb uses it. Settings with named values (Size, Voice, Dwell, Vowel...) jump between them: see About this unit.";
        p.notes = "Sources: BigSky MX User Manual Rev B (MIDI reference, p.62-76; factory globals p.77). midi.guide has no BigSky MX "
                  "chart.\n\nPresets: 300, 000A-149B. Each preset tile sends the MIDI bank (CC#0: 0 = 000A-063B, 1 = 064A-127B, 2 = "
                  "128A-149B) and then Program Change (PC 0 = 000A, 1 = 000B, 2 = 001A...). It powers up in bank 0.\n\nControls: the "
                  "A / B / Infinite footswitches as presses (CC#80 / 81 / 82: 0 = press, 127 = release; note B and Infinite differ from "
                  "the first BigSky), preset engage / bypass (CC#102: 0 = bypassed, 1-127 = engaged), infinite on / off (CC#97), persist "
                  "(CC#84), infinite latching / momentary (CC#98) and the VALUE encoder one step either way (CC#83). Models: the dual "
                  "mode (CC#99) and each reverb's infinite mode (CC#17, 18: Freeze, Infinite, Off). There's no tap or MIDI clock CC.\n\n"
                  "Expression: every knob and parameter of both reverbs in its own range (output level 0-16, pan 0-16 with 8 = centre, "
                  "low end 0-20, boost 0-60 with 30 = 0 dB...). The per-reverb settings with named values are there too, because two "
                  "reverbs' worth don't fit as tiles: Room size (0 Studio, 1 Club), Hall size (0 Concert, 1 Arena), Hall swell type "
                  "(0 Wet, 1 Dry), Chamber color (0 Neutral, 1 Clear, 2 Smooth, 3 Crisp, 4 Deep), Plate size (0 Small, 1 Large), Spring "
                  "dwell (0 Clean, 1 Combo, 2 Tube, 3 Overdrive) and number (0-2 = one to three springs), Impulse tail (0 Envelope, "
                  "1 Gate) and direction (0 Forward, 1 Reverse), Shimmer feedback (0 Input, 1 Regeneration, 2 both), Chorale vowel "
                  "(0 AAHHOO, 1 AAHH, 2 AAHHOH, 3 OH, 4 OOOHOH, 5 OOO, 6 Random), resonance (0 Mild, 1 Medium, 2 High) and choir voice "
                  "(0 Tenor, 1 Baritone), Magneto heads (0 One, 1 Two, 2 Three, 3 Four, 4 Six), spacing (0 Even, 1 Uneven) and ping pong "
                  "(0 off, 1 on), Nonlinear shape (0 Swoosh, 1 Reverse, 2 Ramp, 3 Gate, 4 Gauss, 5 Bounce), and every Voice (0 MX, "
                  "1 Classic). Use Draw or Set to (heel = 0, toe = the top value) to pick one.\n\nModels: reverb 1 / 2 type (CC#1, 2 = 0-11) as "
                  "numbered tiles Type 0-11: the MIDI table doesn't say which number is which reverb type.\n\nLeft out: Shimmer shift 1 / 2: the table prints reverb 1's Shift 2 as "
                  "CC#72, the same as reverb 2's Shift 1, so none of the shift CCs are here.\n\nConnection: 5-pin MIDI In and Out, USB "
                  "MIDI, and TRS MIDI on the EXP jack (EXP MODE = MIDI; received on the tip). MIDI THRU is off out of the box; THRU / "
                  "Merge only echo on the port the message came in on, so MIDI from USB never reaches the 5-pin Out. Channel 1 out of "
                  "the box.";
        v.push_back (p);
    }

    // ---- Volante: 5-pin In / Out and USB ----
    {
        auto p = small ("volante", "Volante", "volante magnetic echo tape drum delay sos looper", "Volante User Manual (Rev E)", manualOnly,
                        true, "5-pin MIDI In");
        p.faceplate = juce::Colour (0xffbfc56c);   // enclosure colour measured from the official product photo (strymon.net/product/volante)
        p.hasThru = true; p.usbToThru = 3;
        p.channelHint = "Must match the Volante: power-up menu (hold TAP while powering up), REC LEVEL knob: 1 (out of the box), 2 or 3; "
                        "4-16 = the channel of the next Program Change.";
        engage (p.utilities, 102, "the Volante");
        p.utilities.push_back ({ "Remote tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Echo on", "Echo off", 78, 127, 0, "The echo (CC#78: 0 = off, 127 = on).", 3);
        onOff (p.utilities, "Reverb on", "Reverb off", 79, 127, 0, "The spring reverb (CC#79: 0 = off, 127 = on).", 6);
        onOff (p.utilities, "Reverse", "Forward", 44, 127, 0, "Reverse (CC#44: 0 = normal, 1-127 = reverse).", 5);
        onOff (p.utilities, "Pause", "Unpause", 43, 127, 0, "Pause with the ramp (CC#43: 0 = unpause, 1-127 = pause).", 7);
        onOff (p.utilities, "Hold", "Release", 45, 127, 0, "Infinite hold with oscillation (CC#45: 0 = release, 1-127 = hold).", 8);
        onOff (p.utilities, "Pause now", "Unpause now", 42, 127, 0, "Pause without the ramp (CC#42: 0 = unpause, 1-127 = pause).", 7);
        onOff (p.utilities, "Hold no osc", "Rel. no osc", 46, 127, 0,
               "Infinite hold without oscillation (CC#46: 0 = release, 1-127 = hold).", 8);
        onOff (p.utilities, "Persist on", "Persist off", 83, 127, 0, "Echo trails continue when bypassed (CC#83: 0 = off, 1-127 = on).", 4);
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        midiExp (p.utilities, 127);
        onOff (p.utilities, "Sum", "Stereo", 85, 127, 0, "Output sum (CC#85: 0 = stereo, 1-127 = summed).", 6);
        presses (p, { { "On", 80 }, { "Fav", 81 }, { "Tap", 82 } }, 127, 0, pressSmall);
        p.looperTitle = "SOS looper";
        p.looper = { { "SOS on", cc (41, 127), "SOS mode (CC#41: 0 = normal, 1-127 = SOS).", 3 }, { "SOS off", cc (41, 0), {}, 9 },
                     { "SOS switch", cc (49, 127), "Records, splices or clears, like the SOS footswitch (CC#49). " + anyValue, 0 },
                     { "Exit SOS", cc (50, 127), "Leaves the SOS looper (CC#50). " + anyValue, 9 } };
        auto typeSpeed = group ("Type, speed", "CC#11 = 1-3, CC#19 = 1-3");
        values (typeSpeed.actions, {}, 11, 1, { "Studio", "Drum", "Tape" }, "The echo type of the loaded preset (CC#11).", 3);
        values (typeSpeed.actions, {}, 19, 1, { "Double", "Half", "Normal" }, "The tape speed of the loaded preset (CC#19).", 6);
        auto heads = group ("Heads", "playback CC#21-24, feedback CC#34-37: 0 = off, 1-127 = on");
        for (int h = 1; h <= 4; ++h)
        {
            const auto head = "Head " + n (h);
            heads.actions.push_back ({ head + " on", cc (20 + h, 127), head + " playback (CC#" + n (20 + h) + ": 0 = off, 1-127 = on).", 3 });
            heads.actions.push_back ({ head + " off", cc (20 + h, 0), head + " playback (CC#" + n (20 + h) + ").", 9 });
            heads.actions.push_back ({ head + " FB on", cc (33 + h, 127), head + " feedback (CC#" + n (33 + h) + ": 0 = off, 1-127 = on).", 6 });
            heads.actions.push_back ({ head + " FB off", cc (33 + h, 0), head + " feedback (CC#" + n (33 + h) + ").", 9 });
        }
        p.models = { typeSpeed,
                     ModelGroup { "Spacing", "CC#18 = 0 / 34 / 94 / 127",
                                  { { "Even", cc (18, 0), {}, 7 }, { "Triplet", cc (18, 34), {}, 7 }, { "Golden", cc (18, 94), {}, 7 },
                                    { "Silver", cc (18, 127), {}, 7 } } },
                     heads };
        p.pedals = { { "Expression", 100 }, { "Time", 17 }, { "Repeats", 20 }, { "Echo level", 12 }, { "Rec level", 13 }, { "Mechanics", 14 },
                     { "Wear", 15 }, { "Low cut", 16 }, { "Spring", 39 }, { "Spring decay", 40 }, { "Head 1 level", 25 }, { "Head 2 level", 26 },
                     { "Head 3 level", 27 }, { "Head 4 level", 28 }, { "SOS repeats", 47 }, { "SOS loop", 48 }, { "Head 1 pan", 29 },
                     { "Head 2 pan", 30 }, { "Head 3 pan", 31 }, { "Head 4 pan", 32 }, { "Pause ramp speed", 38 }, { "Spacing", 18 } };
        p.pedalNote = "Expression (CC#100) moves what the preset's expression setup assigns, when MIDI Expression is on (CC#60). The others "
                      "move that control like turning it.";
        p.notes = "Sources: Volante User Manual Rev E (MIDI specification, p.18-19; power-up menu p.14-15); midi.guide (community chart) "
                  "lists the same CCs without value meanings.\n\n" + presetsNote (manualOnly) + "\n\nControls: the On / Favorite / Tap "
                  "footswitches as presses (CC#80 / 81 / 82: 1-127 = press, 0 = release; the opposite of the TimeLine), engage / bypass "
                  "(CC#102), remote tap (CC#93), echo and reverb on / off (CC#78, 79: 0 = off, 127 = on), reverse (CC#44), pause with "
                  "the ramp (CC#43) and without it (CC#42), infinite hold with oscillation (CC#45) and without (CC#46), persist (CC#83), "
                  "MIDI clock (CC#63), MIDI expression (CC#60) and output sum (CC#85). Models: type (CC#11 1-3), speed (CC#19 1-3), head "
                  "spacing (CC#18) and heads 1-4 playback (CC#21-24) and feedback (CC#34-37) on / off. SOS looper: CC#41 (mode), 49 "
                  "(record / splice / clear), 50 (exit).\n\nExpression: every knob plus head 1-4 level and pan (CC#25-32), pause ramp "
                  "speed (CC#38), SOS levels and spacing as a sweep (CC#18).\n\nLeft out: Kill Dry (CC#84): the MIDI table and the "
                  "power-up menu describe it the opposite way round.\n\nConnection: 5-pin MIDI In and Out and USB MIDI (it answers on "
                  "both). The EXP jack takes TRS MIDI Program Changes in Preset Mode. MIDI Out sends only the Volante's own messages out "
                  "of the box; set DIN MIDI Through in the power-up menu to pass MIDI on. The manual doesn't say whether USB MIDI reaches "
                  "the Out. Channel 1 out of the box.";
        v.push_back (p);
    }

    // ---- EXP-jack MIDI only: Sunset, Riverside, Compadre, Iridium, cloudburst ----
    const juce::String noMidiGuide (" midi.guide has no chart for it.");
    {
        auto p = small ("sunset", "Sunset", "sunset dual overdrive drive", "Sunset User Manual (Rev C)", manualOnly, false, "TRS MIDI In (EXP jack)");
        p.faceplate = juce::Colour (0xffb1071c);   // enclosure colour measured from the official product photo (strymon.net/product/sunset)
        engage (p.utilities, 33, "both sides");
        onOff (p.utilities, "A on", "A off", 10, 127, 0, "Side A (CC#10: 0 = bypass, 1-127 = on).", 3);
        onOff (p.utilities, "B on", "B off", 15, 127, 0, "Side B (CC#15: 0 = bypass, 1-127 = on).", 6);
        p.utilities.push_back ({ "Bright +", cc (21, 1), "Bright switch (CC#21 = 1).", 7 });
        p.utilities.push_back ({ "Bright -", cc (21, 2), "Bright switch (CC#21 = 2).", 7 });
        p.utilities.push_back ({ "Bright mid", cc (21, 3), "Bright switch, middle (CC#21 = 3).", 7 });
        midiExp (p.utilities, 127);
        p.models = { choices ("Side A circuit", 11, 1, { "Treble", "Ge", "Texas" }, "Side A's circuit in the loaded preset (CC#11)."),
                     choices ("Side B circuit", 16, 1, { "JFET", "2stage", "Hard" }, "Side B's circuit in the loaded preset (CC#16).", 6),
                     choices ("Config", 20, 1, { "A > B", "B > A", "A + B" }, "How the two sides are combined (CC#20).", 7) };
        p.pedals = { { "Expression", 100 }, { "Volume", 7 }, { "Level A", 12 }, { "Drive A", 13 }, { "Tone A", 14 }, { "Level B", 17 },
                     { "Drive B", 18 }, { "Tone B", 19 }, { "Noise gate", 22 } };
        p.pedalNote = smallExp + " Volume (CC#7) is the volume pedal.";
        p.notes = "Sources: Sunset User Manual Rev C (MIDI specification, p.19)." + noMidiGuide + "\n\n" + presetsNote (manualOnly)
                + "\n\nControls: both sides on / bypassed (CC#33: 0 = bypass, 127 = on), side A (CC#10) and B (CC#15) on / off, the "
                  "bright switch (CC#21: 1 = +, 2 = -, 3 = middle) and MIDI expression on / off (CC#60: 0 = off, 127 = on). Models: each "
                  "side's circuit (CC#11, 16) and the config (CC#20). There's no tap, tuner or footswitch CC in the table.\n\n"
                + trsConnection + " No 5-pin or USB MIDI.";
        v.push_back (p);
    }
    {
        auto p = small ("riverside", "Riverside", "riverside multistage drive overdrive distortion", "Riverside User Manual (Rev B)", manualOnly,
                        false, "TRS MIDI In (EXP jack)");
        p.faceplate = juce::Colour (0xffdaaf35);   // enclosure colour measured from the official product photo (strymon.net/product/riverside)
        engage (p.utilities, 102, "the Riverside");
        onOff (p.utilities, "Boost on", "Boost off", 18, 127, 0, "The boost (CC#18: 0 = off, 1-127 = on).", 3);
        midiExp (p.utilities, 127);
        p.models = { choices ("Gain", 19, 1, { "Low", "High" }, "The gain switch (CC#19)."),
                     choices ("Push", 20, 1, { "Normal", "Mid" }, "The push switch (CC#20).", 6),
                     choices ("Presence", 21, 1, { "+", "-", "Middle" }, "The presence switch (CC#21).", 7) };
        p.pedals = { { "Expression", 100 }, { "Volume", 7 }, { "Level", 12 }, { "Drive", 13 }, { "Bass", 14 }, { "Mid", 15 }, { "Treble", 16 },
                     { "Boost level", 17 }, { "Noise gate", 22 } };
        p.pedalNote = smallExp + " Volume (CC#7) is the volume pedal.";
        p.notes = "Sources: Riverside User Manual Rev B (MIDI specification, p.22)." + noMidiGuide + "\n\n" + presetsNote (manualOnly)
                + "\n\nControls: engage / bypass (CC#102: 0 = off, 1-127 = on), the boost (CC#18) and MIDI expression on / off "
                  "(CC#60). Models: gain (CC#19), push (CC#20) and presence (CC#21). Left out: the Favorite footswitch: the manual's "
                  "test tip mentions CC#10, but CC#10 isn't in its CC table.\n\n" + trsConnection + " No 5-pin or USB MIDI.";
        v.push_back (p);
    }
    {
        auto p = small ("compadre", "Compadre", "compadre compressor boost", "Compadre User Manual (Rev C)", withFavorite, false,
                        "TRS MIDI In (FAV/MIDI jack)");
        p.faceplate = juce::Colour (0xffad3736);   // enclosure colour measured from the official product photo (strymon.net/product/compadre)
        onOff (p.utilities, "Comp on", "Comp off", 13, 127, 0, "The compressor (CC#13: 0 = bypass, 1-127 = on).", 3);
        onOff (p.utilities, "Boost on", "Boost off", 20, 127, 0, "The boost (CC#20: 0 = bypass, 1-127 = on).", 6);
        p.models = { choices ("Compressor", 11, 1, { "Studio", "Squeeze" }, "The compression type (CC#11)."),
                     choices ("Boost EQ", 17, 1, { "Flat", "Treble", "Mid" }, "The boost EQ (CC#17).", 6),
                     choices ("Boost type", 18, 1, { "Clean", "Dirty" }, "The boost type (CC#18).", 7) };
        p.pedals = { { "Comp level", 12 }, { "Compression", 14 }, { "Dry", 15 }, { "Boost", 19 } };
        p.pedalsTitle = "Knobs";   // the table has no expression CC: this view turns the knobs
        p.pedalNote = "Moves that knob like turning it. The Compadre's MIDI table has no expression CC.";
        p.notes = "Sources: Compadre User Manual Rev C (MIDI specification, p.18-19)." + noMidiGuide + "\n\nPresets: 300, numbered 0-299. "
                  "Each preset tile sends the bank (CC#0: 0 = 0-127, 1 = 128-255, 2 = 256-299) and then Program Change = the number "
                  "within the bank. Program Change 0 = the FAV patch (the one the MiniSwitch recalls), 1-3 = MultiSwitch Plus "
                  "footswitches 1-3, and bank 0's PC 127 = Manual mode (the knobs; nothing can be stored there).\n\nControls: compressor "
                  "and boost on / off (CC#13, 20). The table has no overall bypass, footswitch, tap or expression CC. Models: compression "
                  "type (CC#11), boost EQ (CC#17) and boost type (CC#18). The manual's test tip says CC#27 turns COMP on, but the table "
                  "has Compression Off/On on CC#13 and no CC#27, so there's no CC#27 tile.\n\nConnection: the FAV/MIDI jack set to MIDI "
                  "mode in the power-up menu, with a Strymon MIDI EXP cable or a TRS cable to a TRS MIDI controller or interface (the "
                  "manual doesn't say Type A or B). Leave MIDI Output OFF (the default) when it only receives. No 5-pin or USB MIDI.";
        v.push_back (p);
    }
    {
        const Reserved iridiumReserved { { 0, "Favorite" }, { 127, "Manual mode" } };
        auto p = small ("iridium", "Iridium", "iridium amp ir cab cabinet simulator", "Iridium User Manual (Rev D)", iridiumReserved, false,
                        "TRS MIDI In (EXP jack)");
        p.faceplate = juce::Colour (0xff1e1d22);   // enclosure colour measured from the official product photo (strymon.net/product/iridium)
        p.testMessage = "CC 0=0, PC 1";   // PC 0 is the onboard FAV preset
        engage (p.utilities, 102, "the Iridium");
        onOff (p.utilities, "Amp on", "Amp off", 21, 0, 127, "Amp disable (CC#21: 0 = amp enabled, 1-127 = disabled, cab only).", 3);
        onOff (p.utilities, "Vol pre", "Vol post", 9, 0, 127, "Where the volume pedal sits (CC#9: 0 = pre, 1-127 = post).", 7);
        midiExp (p.utilities, 127);
        presses (p, { { "Fav", 27 }, { "On", 28 } }, 127, 0, pressSmall);
        p.models = { choices ("Amp", 19, 1, { "Round", "Chime", "Punch" }, "The amp of the loaded preset (CC#19)."),
                     choices ("Cab", 20, 0, { "Round a", "Round b", "Round c", "Chime a", "Chime b", "Chime c", "Punch a", "Punch b", "Punch c" },
                              "The cab IR of the loaded preset (CC#20).", 6),
                     choices ("Room size", 18, 1, { "Small", "Medium", "Large" }, "The room size (CC#18).", 7) };
        p.pedals = { { "Expression", 100 }, { "Volume", 7 }, { "Level", 12 }, { "Drive", 13 }, { "Bass", 14 }, { "Mid", 15 }, { "Treble", 16 },
                     { "Room", 17 } };
        p.pedalNote = smallExp + " Volume (CC#7) is the volume pedal.";
        p.notes = "Sources: Iridium User Manual Rev D (MIDI specification, p.31-32)." + noMidiGuide + "\n\nPresets: 300, numbered 0-299. "
                  "Each preset tile sends the bank (CC#0: 0 = 0-127, 1 = 128-255, 2 = 256-299) and then Program Change = the number "
                  "within the bank. Program Change 0 = the onboard FAV footswitch preset, and bank 0's PC 127 = Manual mode (the knobs; "
                  "nothing can be stored there).\n\nControls: the Fav / On footswitches as presses (CC#27, 28: 1-127 = press, 0 = "
                  "release), engage / bypass (CC#102), amp on / off (CC#21; off leaves the cab), the volume pedal pre / post (CC#9) and "
                  "MIDI expression on / off (CC#60). Models: amp (CC#19 1-3), cab (CC#20 0-8) and room size (CC#18 1-3).\n\n"
                + trsConnection + " Its USB jack is for IR files and firmware only (unplug it while setting up MIDI). No 5-pin MIDI.";
        v.push_back (p);
    }
    {
        auto p = small ("cloudburst", "cloudburst", "cloudburst cloud burst ambient reverb", "cloudburst User Manual (Rev D)", manualEveryBank,
                        false, usbTrsTip);
        p.faceplate = juce::Colour (0xff0596c5);   // enclosure colour measured from the official product photo (strymon.net/product/cloudburst)
        engage (p.utilities, 102, "the cloudburst");
        onOff (p.utilities, "Freeze", "Unfreeze", 97, 127, 0, "Freeze (CC#97: 0 = release, 1-127 = hold).", 5);
        onOff (p.utilities, "Infinite", "Release", 98, 127, 0, "Infinite (CC#98: 0 = release, 1-127 = hold).", 6);
        midiExp (p.utilities, 127);
        presses (p, { { "Footswitch", 27 } }, 127, 0, pressSmall);
        p.models = { choices ("Ensemble", 11, 1, { "Off", "mp", "forte" }, "The ensemble switch of the loaded preset (CC#11).") };
        p.pedals = { { "Expression", 100 }, { "Decay", 12 }, { "Pre-delay", 13 }, { "Tone", 14 }, { "Mod", 15 }, { "Mix", 16 },
                     { "Ensemble mp", 17 } };
        p.pedalNote = smallExp;
        p.notes = "Sources: cloudburst User Manual Rev D (MIDI specification, p.30-31)." + noMidiGuide + "\n\n" + presetsNote (manualEveryBank)
                + "\n\nControls: the footswitch as a press (CC#27: 1-127 = press, 0 = release), engage / bypass (CC#102), freeze "
                  "(CC#97) and infinite (CC#98), held while on, and MIDI expression on / off (CC#60). Models: ensemble (CC#11: off, mp, "
                  "forte).\n\n" + trsConnection + " MIDI is received on the tip and sent on the ring. Its USB-C jack is for firmware and "
                  "the Nixie editor: the manual doesn't mention MIDI over USB. No 5-pin MIDI.";
        v.push_back (p);
    }

    // ---- v2 pedals: TRS EXP/MIDI jack + USB-C MIDI, no 5-pin ----
    const auto v2Connection = trsConnection + " USB-C takes MIDI from a computer. No 5-pin MIDI.";
    const std::initializer_list<const char*> clockDown { "x4", "x3", "x2", "x1", "1/2", "1/3", "1/4" };   // El Capistan, DIG, Lex
    {
        auto p = small ("flint-v2", "Flint V2", "flint v2 tremolo reverb", "Flint v2 User Manual (Rev C)", withFavorite, true, usbTrs);
        p.faceplate = juce::Colour (0xff151515);   // enclosure colour measured from the official product photo (strymon.net/product/flint)
        p.shortName = "Flint";
        engage (p.utilities, 33, "both sides");
        onOff (p.utilities, "Trem on", "Trem off", 10, 127, 0, "The tremolo (CC#10: 0 = off, 1-127 = on).", 3);
        onOff (p.utilities, "Reverb on", "Reverb off", 16, 127, 0, "The reverb (CC#16: 0 = off, 1-127 = on).", 6);
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        midiExp (p.utilities, 127);
        clockDivision (p.utilities, { "x1/4", "x1/3", "x1/2", "x1", "x2", "x3", "x4" });
        presses (p, { { "A", 27 }, { "B", 28 } }, 127, 0, pressSmall);
        p.models = { choices ("Tremolo", 11, 1, { "'61", "'63 tube", "'65 photo" }, "The tremolo type (CC#11)."),
                     choices ("Reverb", 17, 1, { "'60s", "'70s", "'80s" }, "The reverb type (CC#17).", 6),
                     ModelGroup { "Order", "CC#23 = 0 / 127", { { "Reverb > trem", cc (23, 0), "Effect order (CC#23 = 0).", 7 },
                                                                { "Trem > reverb", cc (23, 127), "Effect order (CC#23 = 1-127).", 7 } } } };
        p.pedals = { { "Expression", 100 }, { "Intensity", 12 }, { "Speed", 13 }, { "Trem boost/cut", 15 }, { "Mix", 18 }, { "Color", 19 },
                     { "Decay", 20 }, { "Pre-delay", 21 }, { "Reverb boost/cut", 22 }, { "Tap subdivision", 14 } };
        p.pedalNote = smallExp;
        p.notes = "Sources: Flint v2 User Manual Rev C (MIDI specification, p.31-32)." + noMidiGuide + "\n\n" + presetsNote (withFavorite)
                + "\n\nControls: footswitches A / B as presses (CC#27, 28: 1-127 = press, 0 = release), both sides on / bypassed "
                  "(CC#33), tremolo (CC#10) and reverb (CC#16) on / off, remote tap (CC#93), MIDI clock (CC#63), MIDI expression (CC#60) "
                  "and the MIDI clock division (CC#25: 0 = x1/4, 1 = x1/3, 2 = x1/2, 3 = x1, 4 = x2, 5 = x3, 6 = x4). Models: tremolo "
                  "type (CC#11), reverb type (CC#17) and the effect order (CC#23). Tap subdivision (CC#14) is printed as 0-127 with no "
                  "value names, so it's a sweep on Expression.\n\n" + v2Connection;
        v.push_back (p);
    }
    {
        auto p = small ("el-capistan-v2", "El Capistan V2", "el capistan elcap v2 dtape tape echo delay", "El Capistan v2 User Manual (Rev C)",
                        withFavorite, true, usbTrs);
        p.faceplate = juce::Colour (0xff666d75);   // enclosure colour measured from the official product photo (strymon.net/product/elcapistan)
        p.shortName = "El Cap";
        engage (p.utilities, 102, "the El Capistan");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Infinite", "Release", 97, 127, 0, "Infinite repeats (CC#97: 0 = release, 1-127 = hold).", 5);
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        midiExp (p.utilities, 127);
        presses (p, { { "A", 27 }, { "B", 28 } }, 127, 0, pressSmall);
        p.models = { choices ("Tape head", 11, 1, { "Fixed", "Multi", "Single" }, "The tape head (CC#11)."),
                     choices ("Mode", 15, 1, { "A", "B", "C" }, "The mode switch (CC#15).", 6),
                     group ("MIDI clock multiplier", "CC#25 = 0-6") };
        clockDivision (p.models[2].actions, clockDown);
        p.pedals = { { "Expression", 100 }, { "Time", 12 }, { "Repeats", 18 }, { "Mix", 16 }, { "Wow & flutter", 13 }, { "Tape age", 14 },
                     { "Spring", 17 }, { "Low end", 19 }, { "Tape bias", 20 }, { "Crinkle", 21 }, { "Boost/cut", 22 } };
        p.pedalNote = smallExp;
        p.notes = "Sources: El Capistan v2 User Manual Rev C (MIDI specification, p.32-33)." + noMidiGuide + "\n\n" + presetsNote (withFavorite)
                + "\n\nControls: footswitches A / B as presses (CC#27, 28: 1-127 = press, 0 = release), engage / bypass (CC#102), remote "
                  "tap (CC#93), infinite repeats (CC#97, held while on), MIDI clock (CC#63) and MIDI expression (CC#60). Models: tape "
                  "head (CC#11), mode (CC#15) and the MIDI clock tempo multiplier (CC#25: 0 = x4, 1 = x3, 2 = x2, 3 = x1, 4 = 1/2, "
                  "5 = 1/3, 6 = 1/4). The manual's table is titled \"USB - MIDI CC numbers\"; it doesn't say whether the TRS jack uses "
                  "the same map.\n\n" + v2Connection;
        v.push_back (p);
    }
    {
        auto p = small ("deco-v2", "Deco V2", "deco v2 tape saturation doubletracker flanger", "Deco v2 User Manual (Rev D)", withFavorite, true, usbTrs);
        p.faceplate = juce::Colour (0xffb6b6b8);   // enclosure colour measured from the official product photo (strymon.net/product/deco)
        p.shortName = "Deco";
        engage (p.utilities, 33, "both sides");
        onOff (p.utilities, "Sat on", "Sat off", 10, 127, 0, "Tape saturation (CC#10: 0 = off, 1-127 = on).", 3);
        onOff (p.utilities, "Doubler on", "Doubler off", 16, 127, 0, "The doubletracker (CC#16: 0 = off, 1-127 = on).", 6);
        onOff (p.utilities, "Flange on", "Flange off", 97, 127, 0, "Auto-flange (CC#97: 0 = off, 1-127 = on).", 5);
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        midiExp (p.utilities, 127);
        clockDivision (p.utilities, { "x2/3", "x1", "x2", "x3", "x4", "x6", "x8" });
        p.models = { choices ("Voice", 11, 1, { "Classic", "Cassette" }, "The saturation voice (CC#11)."),
                     choices ("Doubletracker", 17, 1, { "Sum", "Invert", "Bounce" }, "The doubletracker type (CC#17).", 6),
                     ModelGroup { "Wide stereo", "CC#23 = 0 / 127", { { "Wide on", cc (23, 127), "Wide stereo mode (CC#23).", 7 },
                                                                      { "Wide off", cc (23, 0), "Wide stereo mode (CC#23).", 9 } } } };
        p.pedals = { { "Expression", 100 }, { "Saturation", 12 }, { "Volume", 13 }, { "Tone", 14 }, { "Low trim", 15 }, { "Lag time", 18 },
                     { "Wobble", 19 }, { "Blend", 20 }, { "Doubler boost/cut", 21 }, { "Flange time", 22 } };
        p.pedalNote = smallExp;
        p.notes = "Sources: Deco v2 User Manual Rev D (MIDI specification, p.33-34). midi.guide's Deco chart is for the first Deco "
                  "(it calls voice 1 \"Tape\", the v2 manual \"classic\"), so it isn't used here.\n\n" + presetsNote (withFavorite)
                + "\n\nControls: both sides on / bypassed (CC#33), tape saturation (CC#10), doubletracker (CC#16) and auto-flange (CC#97) "
                  "on / off, remote tap (CC#93), MIDI clock (CC#63), MIDI expression (CC#60) and the MIDI clock tempo multiplier (CC#25: "
                  "0 = x2/3, 1 = x1, 2 = x2, 3 = x3, 4 = x4, 5 = x6, 6 = x8). Models: voice (CC#11), doubletracker type (CC#17) and wide "
                  "stereo (CC#23). No footswitch CCs are listed.\n\n" + v2Connection;
        v.push_back (p);
    }
    {
        auto p = small ("dig-v2", "DIG V2", "dig v2 dual digital delay", "DIG v2 User Manual (Rev D)", withFavorite, true, usbTrs);
        p.faceplate = juce::Colour (0xffdb81a5);   // enclosure colour measured from the official product photo (strymon.net/product/dig)
        p.shortName = "DIG";
        engage (p.utilities, 102, "the DIG");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Circular", "Release", 97, 127, 0, "Circular repeats (CC#97: 0 = release, 1-127 = hold).", 5);
        onOff (p.utilities, "Sync", "Free", 21, 0, 1, "Delay 2 synced to delay 1 or free (CC#21: 0 = sync, 1 = free).", 7);
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        midiExp (p.utilities, 127);
        p.utilities.push_back ({ "Rpt2 track", cc (20, 127), "Delay 2 repeats follow the Repeats knob (CC#20: 0 = delay, 1-127 = track).", 6 });
        p.utilities.push_back ({ "Rpt2 delay", cc (20, 0), "Delay 2 repeats set on their own (CC#20 = 0: delay).", 9 });
        p.utilities.push_back ({ "Dry track", cc (23, 127), "The dry level tracks the mix (CC#23 = 127; 0-126 set the dry level, on "
                                                           "Expression).", 6 });
        clockDivision (p.utilities, clockDown);
        auto typeMod = group ("Type, mod", "CC#15 = 1-3, CC#11 = 1-3");
        values (typeMod.actions, {}, 15, 1, { "24/96", "adm", "12 bit" }, "The delay type (CC#15).", 3);
        values (typeMod.actions, "Mod", 11, 1, { "Off", "Light", "Deep" }, "The modulation (CC#11).", 6);
        auto subdivision = group ("Delay 1 subdivision", "CC#19 = 0-2");
        values (subdivision.actions, "D1", 19, 0, { "Dotted 1/8", "Quarter", "Half" }, "Delay 1's subdivision in the loaded preset (CC#19).", 1);
        p.models = { typeMod, choices ("Config", 22, 0, { "Series", "Ping pong", "Parallel" }, "How the two delays are combined (CC#22).", 7),
                     subdivision };
        p.pedals = { { "Expression", 100 }, { "Time", 12 }, { "Time 2", 13 }, { "Repeats", 18 }, { "Mix", 16 }, { "Mix 2", 17 }, { "Tone", 14 },
                     prm ("Dry level", 23, 126) };
        p.pedalNote = smallExp;
        p.notes = "Sources: DIG v2 User Manual Rev D (MIDI specification, p.34-35)." + noMidiGuide + "\n\n" + presetsNote (withFavorite)
                + "\n\nControls: engage / bypass (CC#102), remote tap (CC#93), circular repeats (CC#97, held while on), sync / free "
                  "(CC#21), MIDI clock (CC#63), MIDI expression (CC#60), delay 2 repeats delay / track (CC#20: 0 = delay, 1-127 = "
                  "track), the dry level tracking the mix (CC#23 = 127) and the MIDI clock tempo multiplier (CC#25: 0 = x4 ... 3 = x1 "
                  "... 6 = 1/4). Models: type (CC#15) and mod (CC#11), config (CC#22) and delay 1's subdivision (CC#19: 0 = dotted "
                  "eighth, 1 = quarter, 2 = half). Expression: the knobs and the dry level (CC#23 0-126). No footswitch CCs are "
                  "listed.\n\n" + v2Connection;
        v.push_back (p);
    }
    {
        auto p = small ("lex-v2", "Lex V2", "lex v2 rotary leslie speaker", "Lex v2 User Manual (Rev C)", withFavorite, true, usbTrs);
        p.faceplate = juce::Colour (0xff5f2621);   // enclosure colour measured from the official product photo (strymon.net/product/lex)
        p.shortName = "Lex";
        engage (p.utilities, 102, "the Lex");
        onOff (p.utilities, "Fast", "Slow", 22, 127, 0, "Slow / fast (CC#22: 0 = slow, 1-127 = fast).", 3);
        onOff (p.utilities, "Brake", "Release", 97, 127, 0, "The brake (CC#97: 0 = release, 1-127 = hold).", 5);
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        midiExp (p.utilities, 127);
        onOff (p.utilities, "Bi-amp", "Stereo", 20, 1, 0, "The output mode (CC#20: 0 = stereo, 1 = bi-amp).", 6);
        clockDivision (p.utilities, clockDown);
        p.models = { choices ("Mic", 11, 1, { "Front", "Rear" }, "The mic position (CC#11)."),
                     choices ("Ramp", 16, 1, { "Slow", "Medium", "Fast" }, "How fast the rotor changes speed (CC#16).", 6),
                     choices ("Cab filter", 21, 0, { "Guitar amp", "Full range" }, "The cab filter (CC#21).", 7) };
        p.pedals = { { "Expression", 100 }, { "Speed", 12 }, { "Speed (full)", 13 }, { "Mic distance", 14 }, { "Horn level", 15 }, { "Volume", 17 },
                     { "Dry", 18 }, { "Drive", 19 }, { "Slow speed", 23 }, { "Fast speed", 24 } };
        p.pedalNote = smallExp;
        p.notes = "Sources: Lex v2 User Manual Rev C (MIDI specification, p.28-29)." + noMidiGuide + "\n\n" + presetsNote (withFavorite)
                + "\n\nControls: engage / bypass (CC#102), slow / fast (CC#22), the brake (CC#97, held while on), remote tap (CC#93), "
                  "MIDI clock (CC#63), MIDI expression (CC#60), bi-amp / stereo output (CC#20: 0 = stereo, 1 = bi-amp) and the MIDI "
                  "clock tempo multiplier (CC#25: 0 = x4 ... 3 = x1 ... 6 = 1/4). Models: mic (CC#11), ramp (CC#16) and cab filter "
                  "(CC#21). No footswitch CCs are listed.\n\n" + v2Connection;
        v.push_back (p);
    }
    {
        auto p = small ("bluesky-v2", "blueSky V2", "bluesky blue sky v2 reverb", "blueSky v2 User Manual (Rev C)", withFavorite, true, usbTrs);
        p.faceplate = juce::Colour (0xff03add8);   // enclosure colour measured from the official product photo (strymon.net/product/bluesky)
        p.shortName = "blueSky";
        engage (p.utilities, 102, "the blueSky");
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        midiExp (p.utilities, 127);
        p.models = { choices ("Reverb", 11, 1, { "Plate", "Room", "Spring" }, "The reverb type (CC#11)."),
                     choices ("Mod", 15, 1, { "Off", "Light", "Deep" }, "The modulation (CC#15).", 6) };
        p.pedals = { { "Expression", 100 }, { "Decay", 12 }, { "Mix", 16 }, { "Pre-delay", 14 }, { "Low", 13 }, { "High", 17 }, { "Shimmer", 18 } };
        p.pedalNote = smallExp;
        p.notes = "Sources: blueSky v2 User Manual Rev C (MIDI specification, p.25-26)." + noMidiGuide + "\n\n" + presetsNote (withFavorite)
                + "\n\nControls: engage / bypass (CC#102), MIDI clock (CC#63) and MIDI expression (CC#60). Models: reverb type (CC#11) "
                  "and mod (CC#15). No footswitch, tap or freeze CCs are listed.\n\n" + v2Connection;
        v.push_back (p);
    }

    // ---- Brig, Olivera, Ultraviolet, Zelzah: TRS EXP/MIDI jack + USB-C MIDI ----
    const auto newConnection = trsConnection + " MIDI is received on the tip and sent on the ring. USB-C takes MIDI from a computer. "
                                               "No 5-pin MIDI.";
    {
        auto p = small ("brig", "Brig", "brig dbucket bucket brigade analog delay", "Brig User Manual (Rev A)", manualEveryBank, true, usbTrsTip);
        p.faceplate = juce::Colour (0xff5d6445);   // enclosure colour measured from the official product photo (strymon.net/product/brig)
        engage (p.utilities, 102, "the Brig");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Infinite", "Release", 97, 127, 0, "Infinite (CC#97: 0 = release, 1-127 = hold).", 5);
        presses (p, { { "Footswitch", 27 } }, 127, 0, pressSmall);
        p.models = { choices ("Voice", 11, 1, { "3205", "3005", "Multi" }, "The voice (CC#11)."),
                     choices ("Tap division", 17, 0, { "Triplet", "Eighth", "Dotted 1/8", "Quarter" }, "The tap division (CC#17).", 6) };
        p.pedals = { { "Expression", 100 }, { "Time", 12 }, { "Repeats", 14 }, { "Mix", 16 }, { "Filter", 13 }, { "Mod", 15 } };
        p.pedalNote = "Expression (CC#100, 0 = heel, 127 = toe) moves what the preset's expression setup assigns. The others move that "
                      "knob like turning it.";
        p.notes = "Sources: Brig User Manual Rev A (MIDI specification, p.32-33)." + noMidiGuide + "\n\n" + presetsNote (manualEveryBank)
                + "\n\nControls: the footswitch as a press (CC#27: 1-127 = press, 0 = release), engage / bypass (CC#102), tap (CC#93) "
                  "and infinite (CC#97, held while on). Models: voice (CC#11) and tap division (CC#17). The table has no MIDI expression "
                  "on / off or clock CC.\n\n" + newConnection;
        v.push_back (p);
    }
    {
        auto p = small ("olivera", "Olivera", "olivera oil can echo delay", "Olivera User Manual (Rev B)", manualEveryBank, true, usbTrsTip);
        p.faceplate = juce::Colour (0xff5b401b);   // enclosure colour measured from the official product photo (strymon.net/product/olivera)
        engage (p.utilities, 102, "the Olivera");
        onOff (p.utilities, "Mod on", "Mod off", 96, 127, 0, "The modulation (CC#96: 0 = off, 1-127 = on).", 3);
        onOff (p.utilities, "Infinite", "Release", 97, 127, 0, "Infinite (CC#97: 0 = release, 1-127 = hold).", 5);
        midiExp (p.utilities, 127);
        presses (p, { { "Footswitch", 27 } }, 127, 0, pressSmall);
        p.models = { choices ("Heads", 11, 1, { "Long", "Short", "Both" }, "The heads (CC#11).") };
        p.pedals = { { "Expression", 100 }, { "Time", 12 }, { "Rate", 13 }, { "Intensity", 14 }, { "Regen", 15 }, { "Mix", 16 }, { "Tone", 17 } };
        p.pedalNote = smallExp;
        p.notes = "Sources: Olivera User Manual Rev B (MIDI specification, p.32-33)." + noMidiGuide + "\n\n" + presetsNote (manualEveryBank)
                + "\n\nControls: the footswitch as a press (CC#27: 1-127 = press, 0 = release), engage / bypass (CC#102), mod on / off "
                  "(CC#96), infinite (CC#97, held while on) and MIDI expression (CC#60). Models: heads (CC#11). No tap CC is listed.\n\n"
                + newConnection;
        v.push_back (p);
    }
    {
        auto p = small ("ultraviolet", "Ultraviolet", "ultraviolet uni vibe univibe chorus vibrato", "Ultraviolet User Manual (Rev A)",
                        manualEveryBank, true, usbTrsTip);
        p.faceplate = juce::Colour (0xff94629d);   // enclosure colour measured from the official product photo (strymon.net/product/ultraviolet)
        engage (p.utilities, 102, "the Ultraviolet");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        midiExp (p.utilities, 127);
        presses (p, { { "Footswitch", 27 } }, 127, 0, pressSmall);
        p.models = { choices ("Mode", 11, 1, { "Chorus", "Blend", "Vibrato" }, "The mode (CC#11).") };
        p.pedals = { { "Expression", 100 }, { "Speed", 13 }, { "Intensity", 14 }, { "Volume", 15 } };
        p.pedalNote = smallExp;
        p.notes = "Sources: Ultraviolet User Manual Rev A (MIDI specification, p.24-25)." + noMidiGuide + "\n\n" + presetsNote (manualEveryBank)
                + "\n\nControls: the footswitch as a press (CC#27: 1-127 = press, 0 = release), engage / bypass (CC#102), tap (CC#93) "
                  "and MIDI expression (CC#60). Models: mode (CC#11). Left out: Bias (CC#12): the table gives it as 0-127 and as 1 = low, "
                  "2 = mid, 3 = high.\n\n" + newConnection;
        v.push_back (p);
    }
    {
        auto p = small ("zelzah", "Zelzah", "zelzah multidimensional phaser", "Zelzah User Manual (Rev A)", withFavorite, true, usbTrs);
        p.faceplate = juce::Colour (0xff624184);   // enclosure colour measured from the official product photo (strymon.net/product/zelzah)
        engage (p.utilities, 33, "both sides");
        onOff (p.utilities, "4-stage on", "4-stage off", 10, 127, 0, "The 4-stage phaser (CC#10: 0 = bypass, 127 = on).", 3);
        onOff (p.utilities, "6-stage on", "6-stage off", 17, 127, 0, "The 6-stage phaser (CC#17: 0 = bypass, 1-127 = on).", 6);
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        midiExp (p.utilities, 127);
        values (p.utilities, {}, 15, 0, { "Polarity -", "Polarity +" }, "The 4-stage phaser's polarity (CC#15: 0 = negative, 1 = positive).", 7);
        presses (p, { { "A", 27 }, { "B", 28 } }, 127, 0, pressSmall);
        p.models = { choices ("Sweep", 11, 1, { "Classic", "Barber", "Envelope" }, "The 4-stage sweep (CC#11)."),
                     choices ("Resonance", 18, 1, { "Off", "Mid", "Strong" }, "The 6-stage resonance (CC#18).", 6),
                     choices ("Routing", 23, 0, { "Series", "Parallel", "Split" }, "How the two phasers are combined (CC#23).", 7) };
        p.pedals = { { "Expression", 100 }, { "Speed 4", 12 }, { "Depth 4", 13 }, { "Mix 4", 14 }, { "Spread 4", 16 }, { "Speed 6", 19 },
                     { "Depth 6", 20 }, { "Voice 6", 21 }, { "Spread 6", 22 } };
        p.pedalNote = smallExp + " \"4\" and \"6\" are the 4-stage and 6-stage phasers.";
        p.notes = "Sources: Zelzah User Manual Rev A (MIDI specification, p.29-30)." + noMidiGuide + "\n\n" + presetsNote (withFavorite)
                + "\n\nControls: footswitches A / B as presses (CC#27, 28: 1-127 = press, 0 = release), both phasers on / bypassed "
                  "(CC#33), the 4-stage (CC#10) and 6-stage (CC#17) on / off, remote tap (CC#93), MIDI expression (CC#60) and the 4-stage "
                  "polarity (CC#15: 0 = negative, 1 = positive). Models: sweep (CC#11), resonance (CC#18) and routing (CC#23).\n\n"
                + trsConnection + " The manual doesn't say which TRS pin receives. USB-C takes MIDI from a computer. No 5-pin MIDI.";
        v.push_back (p);
    }
}
}
