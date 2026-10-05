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
// Footswitch CCs (a press then a release) are never tiles: the function CCs (bypass, tap, looper, infinite...) are.
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
}

void addStrymon (std::vector<Profile>& v)
{
    // ---- TimeLine / BigSky / Mobius ----
    {
        auto p = firstGen ("timeline", "TimeLine", "timeline delay looper multidelay", "TimeLine User Manual (Rev F) and MIDI Program Changes chart",
                           Scheme::strymonAB);
        engage (p.utilities, 102, "the TimeLine");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        p.utilities.push_back ({ "Phase reset", cc (125, 127), "Resets the modulation phase (CC#125). " + anyValue, 7 });
        onOff (p.utilities, "Clock on", "Clock off", 63, 1, 0, "MIDI clock sync for the loaded preset (CC#63: 0 = off, 1 = on).", 4);
        p.looper = { { "Record", cc (87, 127), anyValue, 0 }, { "Play", cc (86, 127), anyValue, 3 }, { "Stop", cc (85, 127), anyValue, 9 },
                     { "Reverse", cc (94, 127), "Forward / reverse (CC#94). " + toggles, 5 },
                     { "Half speed", cc (95, 127), "Full / half speed (CC#95). " + toggles, 7 },
                     { "Pre/post", cc (96, 127), "Looper before / after the delay (CC#96). " + toggles, 8 },
                     { "Undo", cc (89, 127), "Back to the initial loop (CC#89). " + anyValue, 6 }, { "Redo", cc (90, 127), anyValue, 6 } };
        p.pedals = { { "Expression", 100 }, { "Time", 3 }, { "Repeats", 9 }, { "Mix", 14 }, { "Filter", 15 }, { "Grit", 16 },
                     { "Speed", 17 }, { "Depth", 18 }, { "Looper level", 98 } };
        p.pedalNote = "Expression (CC#100) moves what the preset's expression setup assigns. The others move that knob like turning it.";
        p.notes = "Presets: 200, 00A-99B. Each preset tile sends the MIDI bank (CC#0: 0 = 00A-63B, 1 = 64A-99B) and then Program Change "
                  "(PC 0 = 00A, 1 = 00B, 2 = 01A...). It powers up in bank 0.\n\nControls: engage / bypass (CC#102: 0 = bypass, 127 = engage), "
                  "remote tap (CC#93), phase reset (CC#125) and MIDI clock on / off (CC#63).\n\nLooper: record CC#87, play 86, stop 85, "
                  "undo 89, redo 90 (any value); reverse 94, half speed 95 and pre/post 96 toggle. The manual also lists MIDI notes for "
                  "absolute reverse / half speed (notes 103, 104): tiles only send CC and PC, so those aren't here.\n\nThe A / B / TAP "
                  "footswitch CCs (80-82) need a press and a release, so they aren't tiles. The delay type encoder (CC#19) isn't a tile "
                  "either: the manual doesn't give the order of its values.\n\nConnection: 5-pin MIDI In and Out (MIDI Through: THRU, MERGE "
                  "or OFF). No USB MIDI. The MIDI channel default isn't stated in the manual.";
        v.push_back (p);
    }
    {
        auto p = firstGen ("bigsky", "BigSky", "bigsky big sky reverb", "BigSky User Manual (Rev D)", Scheme::strymonABC);
        engage (p.utilities, 102, "the BigSky");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Hold", "Release", 97, 127, 0,
               "Like holding the preset footswitch (CC#97: 127 = held, 0 = let go): Infinite Sustain or Freeze, as the preset's Hold "
               "parameter says.", 5);
        p.pedals = { { "Expression", 100 }, { "Decay", 17 }, { "Pre-delay", 18 }, { "Mix", 15 }, { "Tone", 3 }, { "Param 1", 9 },
                     { "Param 2", 16 }, { "Mod", 14 } };
        p.pedalNote = "Expression (CC#100) moves what the preset's expression setup assigns. The others move that knob like turning it.";
        p.notes = "Presets: 300, 00A-99C. Each preset tile sends the MIDI bank (CC#0: 0 = 00A-42B, 1 = 42C-85A, 2 = 85B-99C) and then "
                  "Program Change (PC 0 = 00A, 1 = 00B, 2 = 00C, 3 = 01A...). It powers up in bank 0.\n\nControls: engage / bypass "
                  "(CC#102: 0 = bypass, 127 = engage), remote tap (CC#93), and Hold / Release (CC#97), like holding the preset switch: "
                  "Infinite Sustain or Freeze, set per preset. CC#70 picks between those two, but the manual doesn't say which value is "
                  "which, so it isn't a tile.\n\nThe A / B / C footswitch CCs (80-82) need a press and a release, so they aren't tiles. "
                  "The reverb type encoder (CC#19) isn't a tile either: the manual doesn't give the order of its values. No looper or "
                  "tuner.\n\nConnection: 5-pin MIDI In and Out (MIDI Through: THRU, MERGE or OFF). No USB MIDI. The MIDI channel default "
                  "isn't stated in the manual.";
        v.push_back (p);
    }
    {
        auto p = firstGen ("mobius", "Mobius", "mobius modulation chorus flanger phaser rotary vibe tremolo", "Mobius User Manual (Rev F)",
                           Scheme::strymonAB);
        engage (p.utilities, 102, "the Mobius");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        p.utilities.push_back ({ "Phase reset", cc (125, 127), "Resets the modulation phase (CC#125). " + anyValue, 7 });
        onOff (p.utilities, "Clock on", "Clock off", 70, 1, 0, "MIDI clock sync for the loaded preset (CC#70: 0 = off, 1 = on).", 4);
        p.pedals = { { "Expression", 100 }, { "Speed", 17 }, { "Depth", 18 }, { "Level", 15 }, { "Param 1", 9 }, { "Param 2", 16 } };
        p.pedalNote = "Expression (CC#100) moves what the preset's expression setup assigns. The others move that knob like turning it.";
        p.notes = "Presets: 200, 00A-99B. Each preset tile sends the MIDI bank (CC#0: 0 = 00A-63B, 1 = 64A-99B) and then Program Change "
                  "(PC 0 = 00A, 1 = 00B, 2 = 01A...). It powers up in bank 0.\n\nControls: engage / bypass (CC#102: 0 = bypass, 127 = engage), "
                  "remote tap (CC#93), phase reset (CC#125) and MIDI clock on / off (CC#70). A MIDI Start also resets the modulation phase.\n\n"
                  "The A / B / TAP footswitch CCs (80-82) need a press and a release, so they aren't tiles. The effect type encoder (CC#19) "
                  "isn't a tile either: the manual doesn't give the order of its values.\n\nConnection: 5-pin MIDI In and Out (MIDI Through: "
                  "THRU, MERGE or OFF). No USB MIDI. The MIDI channel default isn't stated in the manual.";
        v.push_back (p);
    }

    // ---- MX ----
    {
        auto p = mx ("timeline-mx", "TimeLine MX", "TimeLine MX", "timeline mx dual delay looper", "TimeLine MX User Manual (Rev C)");
        engage (p.utilities, 102, "the preset");
        onOff (p.utilities, "Delay 1 on", "Delay 1 off", 31, 127, 0, "Delay 1 on / bypassed (CC#31: 0 = bypassed, 1-127 = engaged).", 3);
        onOff (p.utilities, "Delay 2 on", "Delay 2 off", 32, 127, 0, "Delay 2 on / bypassed (CC#32: 0 = bypassed, 1-127 = engaged).", 3);
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Infinite on", "Infinite off", 97, 127, 0, "Infinite repeats (CC#97: 0 = off, 1-127 = on).", 5);
        onOff (p.utilities, "Persist on", "Persist off", 123, 127, 0, "Delay trails continue when bypassed (CC#123: 0 = off, 1-127 = on).", 4);
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
        p.pedalNote = "Expression (CC#100) works when the preset's EXP Setup has MIDI EXP on, and moves what that setup assigns. The others "
                      "move that knob of delay 1 or 2 like turning it.";
        p.notes = "Presets: 300, 000A-149B. Each preset tile sends the MIDI bank (CC#0: 0 = 000A-063B, 1 = 064A-127B, 2 = 128A-149B) and then "
                  "Program Change (PC 0 = 000A, 1 = 000B, 2 = 001A...). It powers up in bank 0.\n\nControls: preset engage / bypass (CC#102), "
                  "delay 1 / 2 on or bypassed (CC#31, 32; 0 = bypassed, 1-127 = engaged), remote tap (CC#93), infinite (CC#97) and persist "
                  "(CC#123). Models: delay 1 / 2 type (CC#1, 2 = 0-11) and the dual mode (CC#124).\n\nLooper (it answers outside Looper Mode "
                  "too): record CC#119, play 120, stop 121, undo 98, redo 99 (any value); reverse 125, half speed 126 and pre/post 84 toggle. "
                  "Looper level (CC#127, 0-100), output level (CC#7/8, 0-100), pan (CC#9/10, 0-16) and boost (CC#122, 0-60) take smaller "
                  "ranges, so they aren't on the pedal pad. CC#29/30 and 33/34 change meaning with the delay type.\n\nThe A / B / TAP "
                  "footswitch CCs (80-82) need a press and a release, so they aren't tiles.\n\nConnection: 5-pin MIDI In and Out, USB MIDI, "
                  "and TRS MIDI on the EXP jack (EXP MODE = MIDI). MIDI THRU is off out of the box; THRU / Merge only echo on the port the "
                  "message came in on, so MIDI from USB never reaches the 5-pin Out. Channel 1 out of the box.";
        v.push_back (p);
    }
    {
        auto p = mx ("bigsky-mx", "BigSky MX", "BigSky MX", "bigsky mx big sky dual reverb", "BigSky MX User Manual (Rev B)");
        engage (p.utilities, 102, "the preset");
        onOff (p.utilities, "Infinite on", "Infinite off", 97, 127, 0, "Infinite on / off (CC#97: 0 = off, 1-127 = on).", 5);
        onOff (p.utilities, "Persist on", "Persist off", 84, 127, 0, "Reverb trails continue when bypassed (CC#84: 0 = off, 1-127 = on).", 4);
        onOff (p.utilities, "Latching", "Momentary", 98, 127, 0, "How Infinite behaves (CC#98: 0 = momentary, 1-127 = latching).", 7);
        const char* infinite[] = { "Freeze", "Infinite", "Off" };
        p.models = { choices ("Dual mode", 99, 0, { "Off", "Parallel", "Series 1>>2", "Series 1<<2", "Split L|R", "Split R|L" },
                              "How the two reverbs are combined in the loaded preset (CC#99).", 7),
                     ModelGroup { "Reverb 1 infinite mode", "CC#17 = 0-2", {} }, ModelGroup { "Reverb 2 infinite mode", "CC#18 = 0-2", {} } };
        for (int i = 0; i < 3; ++i)
        {
            p.models[1].actions.push_back ({ infinite[i], cc (17, i), "Reverb 1's infinite mode in the loaded preset.", i == 2 ? 9 : 5 });
            p.models[2].actions.push_back ({ infinite[i], cc (18, i), "Reverb 2's infinite mode in the loaded preset.", i == 2 ? 9 : 6 });
        }
        p.pedals = { { "Expression", 100 }, { "Decay 1", 3 }, { "Decay 2", 4 }, { "Pre-delay 1", 5 }, { "Pre-delay 2", 6 }, { "Tone 1", 11 },
                     { "Tone 2", 12 }, { "Mod 1", 13 }, { "Mod 2", 14 }, { "Mix 1", 15 }, { "Mix 2", 16 }, { "Param 1 (R1)", 19 },
                     { "Param 1 (R2)", 20 }, { "Param 2 (R1)", 21 }, { "Param 2 (R2)", 22 } };
        p.pedalNote = "Expression (CC#100) works when the preset's EXP Setup has MIDI EXP on, and moves what that setup assigns. The others "
                      "move that knob of reverb 1 or 2 like turning it.";
        p.notes = "Presets: 300, 000A-149B. Each preset tile sends the MIDI bank (CC#0: 0 = 000A-063B, 1 = 064A-127B, 2 = 128A-149B) and then "
                  "Program Change (PC 0 = 000A, 1 = 000B, 2 = 001A...). It powers up in bank 0.\n\nControls: preset engage / bypass (CC#102: "
                  "0 = bypassed, 1-127 = engaged), infinite on / off (CC#97), persist (CC#84) and infinite latching / momentary (CC#98). "
                  "Models: the dual mode (CC#99) and each reverb's infinite mode (CC#17, 18). There's no tap or MIDI clock CC.\n\nThe reverb "
                  "type (CC#1, 2) isn't a tile: the MIDI table doesn't give the order of its values. Output level (CC#7/8, 0-16), pan (CC#9/10, "
                  "0-16), low end (CC#23/24, 0-20) and boost (CC#79, 0-60) take smaller ranges, so they aren't on the pedal pad. The table "
                  "prints Shimmer Shift 2 of reverb 1 as CC#72, the same as reverb 2's Shift 1, so neither is here.\n\nThe A / B / Infinite "
                  "footswitch CCs (80-82) need a press and a release, so they aren't tiles.\n\nConnection: 5-pin MIDI In and Out, USB MIDI, "
                  "and TRS MIDI on the EXP jack (EXP MODE = MIDI; received on the tip). MIDI THRU is off out of the box; THRU / Merge only "
                  "echo on the port the message came in on, so MIDI from USB never reaches the 5-pin Out. Channel 1 out of the box.";
        v.push_back (p);
    }

    // ---- Volante: 5-pin In / Out and USB ----
    {
        auto p = small ("volante", "Volante", "volante magnetic echo tape drum delay sos looper", "Volante User Manual (Rev E)", manualOnly,
                        true, "5-pin MIDI In");
        p.hasThru = true; p.usbToThru = 3;
        p.channelHint = "Must match the Volante: power-up menu (hold TAP while powering up), REC LEVEL knob: 1 (out of the box), 2 or 3; "
                        "4-16 = the channel of the next Program Change.";
        engage (p.utilities, 102, "the Volante");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Echo on", "Echo off", 78, 127, 0, "The echo (CC#78: 0 = off, 127 = on).", 3);
        onOff (p.utilities, "Reverb on", "Reverb off", 79, 127, 0, "The spring reverb (CC#79: 0 = off, 127 = on).", 6);
        onOff (p.utilities, "Reverse", "Forward", 44, 127, 0, "Reverse (CC#44: 0 = normal, 1-127 = reverse).", 5);
        onOff (p.utilities, "Pause", "Unpause", 43, 127, 0, "Pause with the ramp (CC#43: 0 = unpause, 1-127 = pause).", 7);
        onOff (p.utilities, "Hold", "Release", 45, 127, 0, "Infinite hold with oscillation (CC#45: 0 = release, 1-127 = hold).", 8);
        p.looperTitle = "SOS looper";
        p.looper = { { "SOS on", cc (41, 127), "SOS mode (CC#41: 0 = normal, 1-127 = SOS).", 3 }, { "SOS off", cc (41, 0), {}, 9 },
                     { "SOS switch", cc (49, 127), "Records, splices or clears, like the SOS footswitch (CC#49). " + anyValue, 0 },
                     { "Exit SOS", cc (50, 127), "Leaves the SOS looper (CC#50). " + anyValue, 9 } };
        p.models = { choices ("Type", 11, 1, { "Studio", "Drum", "Tape" }, "The echo type of the loaded preset (CC#11)."),
                     choices ("Speed", 19, 1, { "Double", "Half", "Normal" }, "The tape speed of the loaded preset (CC#19).", 6),
                     ModelGroup { "Spacing", "CC#18 = 0 / 34 / 94 / 127",
                                  { { "Even", cc (18, 0), {}, 7 }, { "Triplet", cc (18, 34), {}, 7 }, { "Golden", cc (18, 94), {}, 7 },
                                    { "Silver", cc (18, 127), {}, 7 } } } };
        p.pedals = { { "Expression", 100 }, { "Time", 17 }, { "Repeats", 20 }, { "Echo level", 12 }, { "Rec level", 13 }, { "Mechanics", 14 },
                     { "Wear", 15 }, { "Low cut", 16 }, { "Spring", 39 }, { "Spring decay", 40 }, { "Head 1 level", 25 }, { "Head 2 level", 26 },
                     { "Head 3 level", 27 }, { "Head 4 level", 28 }, { "SOS repeats", 47 }, { "SOS loop", 48 } };
        p.pedalNote = "Expression (CC#100) moves what the preset's expression setup assigns, when MIDI Expression is on (CC#60). The others "
                      "move that control like turning it.";
        p.notes = presetsNote (manualOnly) + "\n\nControls: engage / bypass (CC#102), remote tap (CC#93), echo and reverb on / off "
                  "(CC#78, 79: 0 = off, 127 = on), reverse (CC#44), pause with ramp (CC#43) and infinite hold (CC#45). Models: type (CC#11 "
                  "1-3), speed (CC#19 1-3) and head spacing (CC#18). SOS looper: CC#41 (mode), 49 (record / splice / clear), 50 (exit).\n\n"
                  "Heads 1-4 playback (CC#21-24) and feedback (CC#34-37) are on / off (0 / 1-127) but not tiles. Kill Dry (CC#84) is left "
                  "out: the MIDI table and the power-up menu describe it the opposite way round. The footswitch CCs (80-82) need a press and "
                  "a release, so they aren't tiles.\n\nConnection: 5-pin MIDI In and Out and USB MIDI (it answers on both). The EXP jack "
                  "takes TRS MIDI Program Changes in Preset Mode. MIDI Out sends only the Volante's own messages out of the box; set DIN "
                  "MIDI Through in the power-up menu to pass MIDI on. The manual doesn't say whether USB MIDI reaches the Out. Channel 1 "
                  "out of the box.";
        v.push_back (p);
    }

    // ---- EXP-jack MIDI only: Sunset, Riverside, Compadre, Iridium, cloudburst ----
    {
        auto p = small ("sunset", "Sunset", "sunset dual overdrive drive", "Sunset User Manual (Rev C)", manualOnly, false, "TRS MIDI In (EXP jack)");
        engage (p.utilities, 33, "both sides");
        onOff (p.utilities, "A on", "A off", 10, 127, 0, "Side A (CC#10: 0 = bypass, 1-127 = on).", 3);
        onOff (p.utilities, "B on", "B off", 15, 127, 0, "Side B (CC#15: 0 = bypass, 1-127 = on).", 6);
        p.utilities.push_back ({ "Bright +", cc (21, 1), "Bright switch (CC#21 = 1).", 7 });
        p.utilities.push_back ({ "Bright -", cc (21, 2), "Bright switch (CC#21 = 2).", 7 });
        p.utilities.push_back ({ "Bright mid", cc (21, 3), "Bright switch, middle (CC#21 = 3).", 7 });
        p.models = { choices ("Side A circuit", 11, 1, { "Treble", "Ge", "Texas" }, "Side A's circuit in the loaded preset (CC#11)."),
                     choices ("Side B circuit", 16, 1, { "JFET", "2stage", "Hard" }, "Side B's circuit in the loaded preset (CC#16).", 6),
                     choices ("Config", 20, 1, { "A > B", "B > A", "A + B" }, "How the two sides are combined (CC#20).", 7) };
        p.pedals = { { "Expression", 100 }, { "Volume", 7 }, { "Level A", 12 }, { "Drive A", 13 }, { "Tone A", 14 }, { "Level B", 17 },
                     { "Drive B", 18 }, { "Tone B", 19 }, { "Noise gate", 22 } };
        p.pedalNote = smallExp + " Volume (CC#7) is the volume pedal.";
        p.notes = presetsNote (manualOnly) + "\n\nControls: both sides on / bypassed (CC#33: 0 = bypass, 127 = on), side A (CC#10) and B "
                  "(CC#15) on / off, and the bright switch (CC#21: 1 = +, 2 = -, 3 = middle). Models: each side's circuit (CC#11, 16) and the "
                  "config (CC#20). There's no tap or tuner.\n\n" + trsConnection + " No 5-pin or USB MIDI.";
        v.push_back (p);
    }
    {
        auto p = small ("riverside", "Riverside", "riverside multistage drive overdrive distortion", "Riverside User Manual (Rev B)", manualOnly,
                        false, "TRS MIDI In (EXP jack)");
        engage (p.utilities, 102, "the Riverside");
        onOff (p.utilities, "Boost on", "Boost off", 18, 127, 0, "The boost (CC#18: 0 = off, 1-127 = on).", 3);
        p.models = { choices ("Gain", 19, 1, { "Low", "High" }, "The gain switch (CC#19)."),
                     choices ("Push", 20, 1, { "Normal", "Mid" }, "The push switch (CC#20).", 6),
                     choices ("Presence", 21, 1, { "+", "-", "Middle" }, "The presence switch (CC#21).", 7) };
        p.pedals = { { "Expression", 100 }, { "Volume", 7 }, { "Level", 12 }, { "Drive", 13 }, { "Bass", 14 }, { "Mid", 15 }, { "Treble", 16 },
                     { "Boost level", 17 }, { "Noise gate", 22 } };
        p.pedalNote = smallExp + " Volume (CC#7) is the volume pedal.";
        p.notes = presetsNote (manualOnly) + "\n\nControls: engage / bypass (CC#102: 0 = off, 1-127 = on) and the boost (CC#18). Models: gain "
                  "(CC#19), push (CC#20) and presence (CC#21). The manual's test tip mentions CC#10 for the Favorite footswitch, but CC#10 "
                  "isn't in its CC table, so there's no Favorite tile.\n\n" + trsConnection + " No 5-pin or USB MIDI.";
        v.push_back (p);
    }
    {
        auto p = small ("compadre", "Compadre", "compadre compressor boost", "Compadre User Manual (Rev C)", withFavorite, false,
                        "TRS MIDI In (FAV/MIDI jack)");
        onOff (p.utilities, "Comp on", "Comp off", 13, 127, 0, "The compressor (CC#13: 0 = bypass, 1-127 = on).", 3);
        onOff (p.utilities, "Boost on", "Boost off", 20, 127, 0, "The boost (CC#20: 0 = bypass, 1-127 = on).", 6);
        p.models = { choices ("Compressor", 11, 1, { "Studio", "Squeeze" }, "The compression type (CC#11)."),
                     choices ("Boost EQ", 17, 1, { "Flat", "Treble", "Mid" }, "The boost EQ (CC#17).", 6),
                     choices ("Boost type", 18, 1, { "Clean", "Dirty" }, "The boost type (CC#18).", 7) };
        p.pedals = { { "Comp level", 12 }, { "Compression", 14 }, { "Dry", 15 }, { "Boost", 19 } };
        p.pedalNote = "Moves that knob like turning it. The Compadre's MIDI table has no expression CC.";
        p.notes = "Presets: 300, numbered 0-299. Each preset tile sends the bank (CC#0: 0 = 0-127, 1 = 128-255, 2 = 256-299) and then "
                  "Program Change = the number within the bank. Program Change 0 = the FAV patch (the one the MiniSwitch recalls), 1-3 = "
                  "MultiSwitch Plus footswitches 1-3, and bank 0's PC 127 = Manual mode (the knobs; nothing can be stored there).\n\n"
                  "Controls: compressor and boost on / off (CC#13, 20). There's no overall bypass CC in the table. Models: compression type "
                  "(CC#11), boost EQ (CC#17) and boost type (CC#18). The manual's test tip says CC#27 turns COMP on, but the table has "
                  "Compression Off/On on CC#13 and no CC#27.\n\nConnection: the FAV/MIDI jack set to MIDI mode in the power-up menu, with a "
                  "Strymon MIDI EXP cable or a TRS cable to a TRS MIDI controller or interface (the manual doesn't say Type A or B). Leave "
                  "MIDI Output OFF (the default) when it only receives. No 5-pin or USB MIDI.";
        v.push_back (p);
    }
    {
        const Reserved iridiumReserved { { 0, "Favorite" }, { 127, "Manual mode" } };
        auto p = small ("iridium", "Iridium", "iridium amp ir cab cabinet simulator", "Iridium User Manual (Rev D)", iridiumReserved, false,
                        "TRS MIDI In (EXP jack)");
        p.testMessage = "CC 0=0, PC 1";   // PC 0 is the onboard FAV preset
        engage (p.utilities, 102, "the Iridium");
        onOff (p.utilities, "Amp on", "Amp off", 21, 0, 127, "Amp disable (CC#21: 0 = amp enabled, 1-127 = disabled, cab only).", 3);
        onOff (p.utilities, "Volume pre", "Volume post", 9, 0, 127, "Where the volume pedal sits (CC#9: 0 = pre, 1-127 = post).", 7);
        p.models = { choices ("Amp", 19, 1, { "Round", "Chime", "Punch" }, "The amp of the loaded preset (CC#19)."),
                     choices ("Cab", 20, 0, { "Round a", "Round b", "Round c", "Chime a", "Chime b", "Chime c", "Punch a", "Punch b", "Punch c" },
                              "The cab IR of the loaded preset (CC#20).", 6),
                     choices ("Room size", 18, 1, { "Small", "Medium", "Large" }, "The room size (CC#18).", 7) };
        p.pedals = { { "Expression", 100 }, { "Volume", 7 }, { "Level", 12 }, { "Drive", 13 }, { "Bass", 14 }, { "Mid", 15 }, { "Treble", 16 },
                     { "Room", 17 } };
        p.pedalNote = smallExp + " Volume (CC#7) is the volume pedal.";
        p.notes = "Presets: 300, numbered 0-299. Each preset tile sends the bank (CC#0: 0 = 0-127, 1 = 128-255, 2 = 256-299) and then "
                  "Program Change = the number within the bank. Program Change 0 = the onboard FAV footswitch preset, and bank 0's PC 127 = "
                  "Manual mode (the knobs; nothing can be stored there).\n\nControls: engage / bypass (CC#102), amp on / off (CC#21; off "
                  "leaves the cab) and the volume pedal pre / post (CC#9). Models: amp (CC#19 1-3), cab (CC#20 0-8) and room size (CC#18 "
                  "1-3). The footswitch CCs (27, 28) need a press and a release, so they aren't tiles.\n\n" + trsConnection + " Its USB "
                  "jack is for IR files and firmware only (unplug it while setting up MIDI). No 5-pin MIDI.";
        v.push_back (p);
    }
    {
        auto p = small ("cloudburst", "cloudburst", "cloudburst cloud burst ambient reverb", "cloudburst User Manual (Rev D)", manualEveryBank,
                        false, usbTrsTip);
        engage (p.utilities, 102, "the cloudburst");
        onOff (p.utilities, "Freeze", "Unfreeze", 97, 127, 0, "Freeze (CC#97: 0 = release, 1-127 = hold).", 5);
        onOff (p.utilities, "Infinite", "Release", 98, 127, 0, "Infinite (CC#98: 0 = release, 1-127 = hold).", 6);
        p.models = { choices ("Ensemble", 11, 1, { "Off", "mp", "forte" }, "The ensemble switch of the loaded preset (CC#11).") };
        p.pedals = { { "Expression", 100 }, { "Decay", 12 }, { "Pre-delay", 13 }, { "Tone", 14 }, { "Mod", 15 }, { "Mix", 16 },
                     { "Ensemble mp", 17 } };
        p.pedalNote = smallExp;
        p.notes = presetsNote (manualEveryBank) + "\n\nControls: engage / bypass (CC#102), freeze (CC#97) and infinite (CC#98), held "
                  "while on. Models: ensemble (CC#11: off, mp, forte). The footswitch CC (27) needs a press and a release, so it isn't a "
                  "tile.\n\n" + trsConnection + " MIDI is received on the tip and sent on the ring. Its USB-C jack is for firmware and the "
                  "Nixie editor: the manual doesn't mention MIDI over USB. No 5-pin MIDI.";
        v.push_back (p);
    }

    // ---- v2 pedals: TRS EXP/MIDI jack + USB-C MIDI, no 5-pin ----
    const auto v2Connection = trsConnection + " USB-C takes MIDI from a computer. No 5-pin MIDI.";
    {
        auto p = small ("flint-v2", "Flint V2", "flint v2 tremolo reverb", "Flint v2 User Manual (Rev C)", withFavorite, true, usbTrs);
        p.shortName = "Flint";
        engage (p.utilities, 33, "both sides");
        onOff (p.utilities, "Trem on", "Trem off", 10, 127, 0, "The tremolo (CC#10: 0 = off, 1-127 = on).", 3);
        onOff (p.utilities, "Reverb on", "Reverb off", 16, 127, 0, "The reverb (CC#16: 0 = off, 1-127 = on).", 6);
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        p.models = { choices ("Tremolo", 11, 1, { "'61", "'63 tube", "'65 photo" }, "The tremolo type (CC#11)."),
                     choices ("Reverb", 17, 1, { "'60s", "'70s", "'80s" }, "The reverb type (CC#17).", 6),
                     ModelGroup { "Order", "CC#23 = 0 / 127", { { "Reverb > trem", cc (23, 0), "Effect order (CC#23 = 0).", 7 },
                                                                { "Trem > reverb", cc (23, 127), "Effect order (CC#23 = 1-127).", 7 } } } };
        p.pedals = { { "Expression", 100 }, { "Intensity", 12 }, { "Speed", 13 }, { "Trem boost/cut", 15 }, { "Mix", 18 }, { "Color", 19 },
                     { "Decay", 20 }, { "Pre-delay", 21 }, { "Reverb boost/cut", 22 } };
        p.pedalNote = smallExp;
        p.notes = presetsNote (withFavorite) + "\n\nControls: both sides on / bypassed (CC#33), tremolo (CC#10) and reverb (CC#16) on / off, "
                  "remote tap (CC#93) and MIDI clock (CC#63). Models: tremolo type (CC#11), reverb type (CC#17) and the effect order (CC#23). "
                  "The footswitch CCs (27, 28) need a press and a release, so they aren't tiles.\n\n" + v2Connection;
        v.push_back (p);
    }
    {
        auto p = small ("el-capistan-v2", "El Capistan V2", "el capistan elcap v2 dtape tape echo delay", "El Capistan v2 User Manual (Rev C)",
                        withFavorite, true, usbTrs);
        p.shortName = "El Cap";
        engage (p.utilities, 102, "the El Capistan");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Infinite", "Release", 97, 127, 0, "Infinite repeats (CC#97: 0 = release, 1-127 = hold).", 5);
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        p.models = { choices ("Tape head", 11, 1, { "Fixed", "Multi", "Single" }, "The tape head (CC#11)."),
                     choices ("Mode", 15, 1, { "A", "B", "C" }, "The mode switch (CC#15).", 6) };
        p.pedals = { { "Expression", 100 }, { "Time", 12 }, { "Repeats", 18 }, { "Mix", 16 }, { "Wow & flutter", 13 }, { "Tape age", 14 },
                     { "Spring", 17 }, { "Low end", 19 }, { "Tape bias", 20 }, { "Crinkle", 21 }, { "Boost/cut", 22 } };
        p.pedalNote = smallExp;
        p.notes = presetsNote (withFavorite) + "\n\nControls: engage / bypass (CC#102), remote tap (CC#93), infinite repeats (CC#97, held "
                  "while on) and MIDI clock (CC#63). Models: tape head (CC#11) and mode (CC#15). The footswitch CCs (27, 28) need a press "
                  "and a release, so they aren't tiles. The manual's table is titled \"USB - MIDI CC numbers\"; it doesn't say whether the "
                  "TRS jack uses the same map.\n\n" + v2Connection;
        v.push_back (p);
    }
    {
        auto p = small ("deco-v2", "Deco V2", "deco v2 tape saturation doubletracker flanger", "Deco v2 User Manual (Rev D)", withFavorite, true, usbTrs);
        p.shortName = "Deco";
        engage (p.utilities, 33, "both sides");
        onOff (p.utilities, "Saturation on", "Saturation off", 10, 127, 0, "Tape saturation (CC#10: 0 = off, 1-127 = on).", 3);
        onOff (p.utilities, "Doubler on", "Doubler off", 16, 127, 0, "The doubletracker (CC#16: 0 = off, 1-127 = on).", 6);
        onOff (p.utilities, "Flange on", "Flange off", 97, 127, 0, "Auto-flange (CC#97: 0 = off, 1-127 = on).", 5);
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        p.models = { choices ("Voice", 11, 1, { "Classic", "Cassette" }, "The saturation voice (CC#11)."),
                     choices ("Doubletracker", 17, 1, { "Sum", "Invert", "Bounce" }, "The doubletracker type (CC#17).", 6),
                     ModelGroup { "Wide stereo", "CC#23 = 0 / 127", { { "Wide on", cc (23, 127), "Wide stereo mode (CC#23).", 7 },
                                                                      { "Wide off", cc (23, 0), "Wide stereo mode (CC#23).", 9 } } } };
        p.pedals = { { "Expression", 100 }, { "Saturation", 12 }, { "Volume", 13 }, { "Tone", 14 }, { "Low trim", 15 }, { "Lag time", 18 },
                     { "Wobble", 19 }, { "Blend", 20 }, { "Doubler boost/cut", 21 }, { "Flange time", 22 } };
        p.pedalNote = smallExp;
        p.notes = presetsNote (withFavorite) + "\n\nControls: both sides on / bypassed (CC#33), tape saturation (CC#10), doubletracker "
                  "(CC#16) and auto-flange (CC#97) on / off, remote tap (CC#93). Models: voice (CC#11), doubletracker type (CC#17) and wide "
                  "stereo (CC#23). MIDI clock on / off is CC#63. No footswitch CCs are listed.\n\n" + v2Connection;
        v.push_back (p);
    }
    {
        auto p = small ("dig-v2", "DIG V2", "dig v2 dual digital delay", "DIG v2 User Manual (Rev D)", withFavorite, true, usbTrs);
        p.shortName = "DIG";
        engage (p.utilities, 102, "the DIG");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Circular", "Release", 97, 127, 0, "Circular repeats (CC#97: 0 = release, 1-127 = hold).", 5);
        onOff (p.utilities, "Sync", "Free", 21, 0, 1, "Delay 2 synced to delay 1 or free (CC#21: 0 = sync, 1 = free).", 7);
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        p.models = { choices ("Type", 15, 1, { "24/96", "adm", "12 bit" }, "The delay type (CC#15)."),
                     choices ("Mod", 11, 1, { "Off", "Light", "Deep" }, "The modulation (CC#11).", 6),
                     choices ("Config", 22, 0, { "Series", "Ping pong", "Parallel" }, "How the two delays are combined (CC#22).", 7) };
        p.switchesTitle = "Delay 1 subdivision";
        p.switches = { { "Dotted 1/8", 19, 0 }, { "Quarter", 19, 1 }, { "Half", 19, 2 } };
        p.switchesNote = "Sets delay 1's subdivision (CC#19) in the loaded preset.";
        p.pedals = { { "Expression", 100 }, { "Time", 12 }, { "Time 2", 13 }, { "Repeats", 18 }, { "Mix", 16 }, { "Mix 2", 17 }, { "Tone", 14 } };
        p.pedalNote = smallExp;
        p.notes = presetsNote (withFavorite) + "\n\nControls: engage / bypass (CC#102), remote tap (CC#93), circular repeats (CC#97, held "
                  "while on), sync / free (CC#21), MIDI clock (CC#63) and delay 1's subdivision (CC#19). Models: type (CC#15), mod (CC#11) and "
                  "config (CC#22). Delay 2 repeats (CC#20: 0 = delay, 1-127 = track) and dry level (CC#23: 0-126, 127 = track mix) aren't "
                  "tiles.\n\n" + v2Connection;
        v.push_back (p);
    }
    {
        auto p = small ("lex-v2", "Lex V2", "lex v2 rotary leslie speaker", "Lex v2 User Manual (Rev C)", withFavorite, true, usbTrs);
        p.shortName = "Lex";
        engage (p.utilities, 102, "the Lex");
        onOff (p.utilities, "Fast", "Slow", 22, 127, 0, "Slow / fast (CC#22: 0 = slow, 1-127 = fast).", 3);
        onOff (p.utilities, "Brake", "Release", 97, 127, 0, "The brake (CC#97: 0 = release, 1-127 = hold).", 5);
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Clock on", "Clock off", 63, 127, 0, "MIDI clock sync (CC#63: 0 = off, 1-127 = on).", 4);
        p.models = { choices ("Mic", 11, 1, { "Front", "Rear" }, "The mic position (CC#11)."),
                     choices ("Ramp", 16, 1, { "Slow", "Medium", "Fast" }, "How fast the rotor changes speed (CC#16).", 6),
                     choices ("Cab filter", 21, 0, { "Guitar amp", "Full range" }, "The cab filter (CC#21).", 7) };
        p.pedals = { { "Expression", 100 }, { "Speed", 12 }, { "Speed (full)", 13 }, { "Mic distance", 14 }, { "Horn level", 15 }, { "Volume", 17 },
                     { "Dry", 18 }, { "Drive", 19 }, { "Slow speed", 23 }, { "Fast speed", 24 } };
        p.pedalNote = smallExp;
        p.notes = presetsNote (withFavorite) + "\n\nControls: engage / bypass (CC#102), slow / fast (CC#22), the brake (CC#97, held while "
                  "on), remote tap (CC#93) and MIDI clock (CC#63). Models: mic (CC#11), ramp (CC#16) and cab filter (CC#21). Bi-amp output "
                  "mode is CC#20 (0 = stereo, 1 = bi-amp).\n\n" + v2Connection;
        v.push_back (p);
    }
    {
        auto p = small ("bluesky-v2", "blueSky V2", "bluesky blue sky v2 reverb", "blueSky v2 User Manual (Rev C)", withFavorite, true, usbTrs);
        p.shortName = "blueSky";
        engage (p.utilities, 102, "the blueSky");
        p.models = { choices ("Reverb", 11, 1, { "Plate", "Room", "Spring" }, "The reverb type (CC#11)."),
                     choices ("Mod", 15, 1, { "Off", "Light", "Deep" }, "The modulation (CC#15).", 6) };
        p.pedals = { { "Expression", 100 }, { "Decay", 12 }, { "Mix", 16 }, { "Pre-delay", 14 }, { "Low", 13 }, { "High", 17 }, { "Shimmer", 18 } };
        p.pedalNote = smallExp;
        p.notes = presetsNote (withFavorite) + "\n\nControls: engage / bypass (CC#102). Models: reverb type (CC#11) and mod (CC#15). No "
                  "footswitch, tap or freeze CCs are listed.\n\n" + v2Connection;
        v.push_back (p);
    }

    // ---- Brig, Olivera, Ultraviolet, Zelzah: TRS EXP/MIDI jack + USB-C MIDI ----
    const auto newConnection = trsConnection + " MIDI is received on the tip and sent on the ring. USB-C takes MIDI from a computer. "
                                               "No 5-pin MIDI.";
    {
        auto p = small ("brig", "Brig", "brig dbucket bucket brigade analog delay", "Brig User Manual (Rev A)", manualEveryBank, true, usbTrsTip);
        engage (p.utilities, 102, "the Brig");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        onOff (p.utilities, "Infinite", "Release", 97, 127, 0, "Infinite (CC#97: 0 = release, 1-127 = hold).", 5);
        p.models = { choices ("Voice", 11, 1, { "3205", "3005", "Multi" }, "The voice (CC#11)."),
                     choices ("Tap division", 17, 0, { "Triplet", "Eighth", "Dotted 1/8", "Quarter" }, "The tap division (CC#17).", 6) };
        p.pedals = { { "Expression", 100 }, { "Time", 12 }, { "Repeats", 14 }, { "Mix", 16 }, { "Filter", 13 }, { "Mod", 15 } };
        p.pedalNote = smallExp;
        p.notes = presetsNote (manualEveryBank) + "\n\nControls: engage / bypass (CC#102), tap (CC#93) and infinite (CC#97, held while "
                  "on). Models: voice (CC#11) and tap division (CC#17). The footswitch CC (27) needs a press and a release, so it isn't a "
                  "tile.\n\n" + newConnection;
        v.push_back (p);
    }
    {
        auto p = small ("olivera", "Olivera", "olivera oil can echo delay", "Olivera User Manual (Rev B)", manualEveryBank, true, usbTrsTip);
        engage (p.utilities, 102, "the Olivera");
        onOff (p.utilities, "Mod on", "Mod off", 96, 127, 0, "The modulation (CC#96: 0 = off, 1-127 = on).", 3);
        onOff (p.utilities, "Infinite", "Release", 97, 127, 0, "Infinite (CC#97: 0 = release, 1-127 = hold).", 5);
        p.models = { choices ("Heads", 11, 1, { "Long", "Short", "Both" }, "The heads (CC#11).") };
        p.pedals = { { "Expression", 100 }, { "Time", 12 }, { "Rate", 13 }, { "Intensity", 14 }, { "Regen", 15 }, { "Mix", 16 }, { "Tone", 17 } };
        p.pedalNote = smallExp;
        p.notes = presetsNote (manualEveryBank) + "\n\nControls: engage / bypass (CC#102), mod on / off (CC#96) and infinite (CC#97, held "
                  "while on). Models: heads (CC#11). No tap CC is listed. The footswitch CC (27) needs a press and a release, so it isn't a "
                  "tile.\n\n" + newConnection;
        v.push_back (p);
    }
    {
        auto p = small ("ultraviolet", "Ultraviolet", "ultraviolet uni vibe univibe chorus vibrato", "Ultraviolet User Manual (Rev A)",
                        manualEveryBank, true, usbTrsTip);
        engage (p.utilities, 102, "the Ultraviolet");
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        p.models = { choices ("Mode", 11, 1, { "Chorus", "Blend", "Vibrato" }, "The mode (CC#11).") };
        p.pedals = { { "Expression", 100 }, { "Speed", 13 }, { "Intensity", 14 }, { "Volume", 15 } };
        p.pedalNote = smallExp;
        p.notes = presetsNote (manualEveryBank) + "\n\nControls: engage / bypass (CC#102) and tap (CC#93). Models: mode (CC#11). Bias "
                  "(CC#12) is left out: the table gives it as 0-127 and as 1 = low, 2 = mid, 3 = high. The footswitch CC (27) needs a press "
                  "and a release, so it isn't a tile.\n\n" + newConnection;
        v.push_back (p);
    }
    {
        auto p = small ("zelzah", "Zelzah", "zelzah multidimensional phaser", "Zelzah User Manual (Rev A)", withFavorite, true, usbTrs);
        engage (p.utilities, 33, "both sides");
        onOff (p.utilities, "4-stage on", "4-stage off", 10, 127, 0, "The 4-stage phaser (CC#10: 0 = bypass, 127 = on).", 3);
        onOff (p.utilities, "6-stage on", "6-stage off", 17, 127, 0, "The 6-stage phaser (CC#17: 0 = bypass, 1-127 = on).", 6);
        p.utilities.push_back ({ "Tap", cc (93, 127), oneTap, 1 });
        p.models = { choices ("Sweep", 11, 1, { "Classic", "Barber", "Envelope" }, "The 4-stage sweep (CC#11)."),
                     choices ("Resonance", 18, 1, { "Off", "Mid", "Strong" }, "The 6-stage resonance (CC#18).", 6),
                     choices ("Routing", 23, 0, { "Series", "Parallel", "Split" }, "How the two phasers are combined (CC#23).", 7) };
        p.pedals = { { "Expression", 100 }, { "Speed 4", 12 }, { "Depth 4", 13 }, { "Mix 4", 14 }, { "Spread 4", 16 }, { "Speed 6", 19 },
                     { "Depth 6", 20 }, { "Voice 6", 21 }, { "Spread 6", 22 } };
        p.pedalNote = smallExp + " \"4\" and \"6\" are the 4-stage and 6-stage phasers.";
        p.notes = presetsNote (withFavorite) + "\n\nControls: both phasers on / bypassed (CC#33), the 4-stage (CC#10) and 6-stage (CC#17) "
                  "on / off, remote tap (CC#93). Models: sweep (CC#11), resonance (CC#18) and routing (CC#23). 4-stage polarity is CC#15 "
                  "(0 = negative, 1 = positive). The footswitch CCs (27, 28) need a press and a release, so they aren't tiles.\n\n"
                + trsConnection + " The manual doesn't say which TRS pin receives. USB-C takes MIDI from a computer. No 5-pin MIDI.";
        v.push_back (p);
    }
}
}
