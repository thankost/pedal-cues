#include "Modellers.h"

// Boss DD-500 / RV-500 / MD-500 pages (Effects & Pedals tab), from the makers' MIDI charts:
// DD-500 Owner's Manual (e02, MIDI tables p.20-21) and MIDI Implementation 1.00; RV-500 Owner's Manual (eng01, p.24-25) and
// MD-500 Owner's Manual (eng01, p.26-27) with their MIDI Implementations 1.00. All three: 297 patches 01A-99C, sent as
// bank select CC#0 = index / 128 and Program Change = index % 128 (01A = 0/0, 43B = 0/127, 43C = 1/0, 99C = 2/40).
namespace modellers
{
namespace
{
const juce::Colour bossColour { 0xffff5a5f };

const juce::String onOffNote ("The manual lists this message as ON / OFF without saying which values count: these tiles send 127 for ON and 0 for OFF.");

const juce::String presetsNote ("Presets: 297 patches, 01A-99C. A preset tile sends bank select (CC#0 = 0 for 01A-43B, 1 for 43C-86A, 2 for 86B-99C) "
                                "and then the Program Change, the numbers the pedal itself sends for each patch. When it receives them, the pedal "
                                "loads whatever its MIDI PC MAP (BNK-PC#) says for that bank and number, and the manual doesn't list the map's "
                                "defaults: if a preset tile loads the wrong patch, check the pedal's MIDI PC MAP. PC IN must be ON (and CC IN for "
                                "the other tiles).");

const juce::String ccDefaultsNote ("These are the CC numbers shown in the manual's MIDI tables. Each one can be changed in the pedal's MIDI menu "
                                   "(OFF, CC#1-31 or CC#64-95): if you changed one, use a custom MIDI device instead.");

const juce::String connectionNote ("Connection: MIDI IN and MIDI OUT (5-pin DIN in the manual's drawing) and USB MIDI. MIDI IN->OUT and "
                                   "USB IN->OUT (MIDI menu) choose where received MIDI is passed on: set USB IN->OUT to MIDI to pass MIDI from "
                                   "a computer on USB to the MIDI OUT (the manual's screenshot shows it OFF).");

// The switch-emulation CCs (127 pressed, 0 released, per the MIDI tables) are press tiles: 127, then 0 1/16 later.
const juce::String pressNote ("Emulates pressing the switch: 127, then 0 (released) 1/16 later, so it does what that switch does in the patch "
                              "and the pedal's settings (FSW MODE or CTL 1/2 FUNC in the pedal's menus).");

std::vector<Control> ctlSwitches()
{
    return { { "CTL 1", 80, 127, 0, true }, { "CTL 2", 81, 127, 0, true } };
}

Profile bossBase (const juce::String& id, const juce::String& model, const juce::String& aliases, const juce::String& manual)
{
    Profile p;
    p.id = id; p.brand = "Boss"; p.model = model; p.shortName = model;
    p.aliases = "boss roland 500 series " + aliases; p.colour = bossColour;
    p.manual = manual;
    p.pedal = true; p.scheme = Scheme::boss500;
    p.sceneWord = "Scene"; p.sceneCount = 0;
    p.mainTitle = "Controls";
    p.testMessage = "CC 0=0, PC 0";   // 01A (as the PC MAP sends it on)
    p.channelHint = "Must match the " + model + ": MIDI menu > Rx CHANNEL (Ch.1-16).";
    p.usbToThru = 2; p.usbThruSetting = "USB IN->OUT set to MIDI (MIDI menu)";
    // Every preset tile sends its own CC#0 bank, and a spare CC#0 = 0 only sets the bank the next Program Change uses (the
    // assignable CCs are 1-31 and 64-95, so CC#0 is never a control): the usual padding is harmless.
    p.padCc = -1;
    p.switchesTitle = "Switches";
    p.switchesNote = pressNote;
    return p;
}
}

void addBossPedals (std::vector<Profile>& v)
{
    // DD-500 Digital Delay
    {
        auto p = bossBase ("boss.dd-500", "DD-500", "dd500 dd 500 digital delay looper",
                           "DD-500 Owner's Manual (e02) and MIDI Implementation 1.00");
        p.faceplate = juce::Colour (0xffdfdeda);   // enclosure colour measured from the official product photo (boss.info/global/products/dd-500)
        p.utilities = { { "Delay on", "CC 21=127", "Delay on (CC#21 ON). " + onOffNote, 3 },
                        { "Bypass", "CC 21=0", "Bypasses the delay (CC#21 OFF). " + onOffNote, 9 } };
        p.looper = { { "Phrase loop on", "CC 22=127", "Turns the phrase loop on (CC#22 ON). " + onOffNote, 3 },
                     { "Phrase loop off", "CC 22=0", "Turns the phrase loop off (CC#22 OFF).", 9 },
                     { "Record/overdub", "CC 23=127", "Records or overdubs (CC#23), with the phrase loop on.", 0 },
                     { "Play", "CC 24=127", "Plays the loop (CC#24), with the phrase loop on.", 3 },
                     { "Stop", "CC 25=127", "Stops playback (CC#25), with the phrase loop on.", 9 } };
        p.looperTitle = "Phrase loop";
        p.switches = { { "[A]", 82, 127, 0, true }, { "[B]", 83, 127, 0, true }, { "[TAP/CTL]", 84, 127, 0, true } };
        for (const auto& c : ctlSwitches())
            p.switches.push_back (c);
        p.pedals = { { "Expression", 16 }, { "Feedback", 17 }, { "E. Level", 18 }, { "Tone", 19 }, { "Mod depth", 20 } };
        p.pedalNote = "Expression (CC#16) works like a pedal on the EXP jack: it moves what the patch assigns to EXP. The others move that knob "
                      "like turning it.";
        p.notes = ccDefaultsNote + "\n\n" + presetsNote + "\n\n"
                  "Controls: delay on / bypass (CC#21). Phrase loop (receive only): on/off CC#22, record/overdub CC#23, play CC#24, stop "
                  "CC#25 (CC#26 clears the phrase; there's no tile for it, so a stray clip can't erase a loop). The manual gives these as "
                  "ON / OFF without numbers: the tiles send 127 / 0.\n\n"
                  "Expression: EXP pedal CC#16, knobs FEEDBACK CC#17, E. LEVEL CC#18, TONE CC#19, MOD DEPTH CC#20 (0-127). The "
                  "TIME/VALUE knob is pitch bend (with MIDI TIME CONTROL on), not a CC, so it isn't here.\n\n"
                  "Switches: [A] CC#82, [B] CC#83, [TAP/CTL] CC#84 and CTL 1 / CTL 2 (CC#80 / 81) emulate pressing a switch (127 "
                  "pressed, 0 released): each tile presses and releases it. Tap tempo works through [TAP/CTL] (with TAP/CTL set to TAP, "
                  "as it is out of the box): one tile is one tap. The manual has no tuner CC.\n\n"
                  "MIDI channel: Rx CHANNEL in the MIDI menu; the manual doesn't say its factory setting.\n\nThat's every CC in the manual's MIDI "
                  "tables. midi.guide has no chart for the 500 series, so it's from the Boss documents only.\n\n" + connectionNote;
        v.push_back (p);
    }

    // RV-500 Reverb
    {
        auto p = bossBase ("boss.rv-500", "RV-500", "rv500 rv 500 reverb delay shimmer",
                           "RV-500 Owner's Manual (eng01) and MIDI Implementation 1.00");
        p.faceplate = juce::Colour (0xff393a3e);   // enclosure colour measured from the official product photo (boss.info/global/products/rv-500)
        p.utilities = { { "Effect on", "CC 27=127", "Effect on (CC#27 ON). In simul mode it turns the selected patch on. " + onOffNote, 3 },
                        { "Bypass", "CC 27=0", "Bypass (CC#27 OFF). In simul mode it turns the selected patch off. " + onOffNote, 9 },
                        { "Patch A on", "CC 28=127", "Patch A on (CC#28 ON). " + onOffNote, 4 },
                        { "Patch A bypass", "CC 28=0", "Patch A bypassed (CC#28 OFF).", 9 },
                        { "Patch B on", "CC 29=127", "Patch B on (CC#29 ON). " + onOffNote, 6 },
                        { "Patch B bypass", "CC 29=0", "Patch B bypassed (CC#29 OFF).", 9 } };
        p.pedals = { { "Expression", 16 },
                     { "Reverb time/value", 17 }, { "Reverb pre-delay", 18 }, { "Reverb E. Level", 19 }, { "Reverb low", 20 }, { "Reverb high", 21 },
                     { "Delay time/value", 22 }, { "Delay pre-delay knob", 23 }, { "Delay E. Level", 24 }, { "Delay low", 25 }, { "Delay high", 26 } };
        p.switches = ctlSwitches();
        p.pedalNote = "Expression (CC#16) works like a pedal on the EXP jack. The others move a knob like turning it: CC#17-21 for the reverb, "
                      "CC#22-26 for the delay.";
        p.notes = ccDefaultsNote + "\n\n" + presetsNote + "\n\n"
                  "Controls: effect on / bypass CC#27 (in simul mode it turns the selected patch on / off), patch A on / bypass CC#28, "
                  "patch B on / bypass CC#29. The manual gives these as ON / OFF without numbers: the tiles send 127 / 0.\n\n"
                  "Expression: EXP pedal CC#16. Knobs (0-127), reverb / delay: TIME/VALUE CC#17 / 22, PRE-DELAY CC#18 / 23, E. LEVEL "
                  "CC#19 / 24, LOW CC#20 / 25, HIGH CC#21 / 26. When a TIME CC is set to CC#1-31, the pedal also reads the matching "
                  "fine-step CC (CC#33-63); these clips send only the main CC.\n\n"
                  "Switches: CTL 1 / CTL 2 (CC#80 / 81, 127 pressed, 0 released) emulate pressing an external switch: each tile presses "
                  "and releases it. The manual has no tap tempo, tuner or [A] / [B] / [TAP/CTL] CCs for the RV-500, and no looper.\n\n"
                  "MIDI channel: Rx CHANNEL in the MIDI menu (Ch.1 in the manual's screenshot).\n\nThat's every CC in the manual's MIDI "
                  "tables. midi.guide has no chart for the 500 series, so it's from the Boss documents only.\n\n" + connectionNote;
        v.push_back (p);
    }

    // MD-500 Modulation
    {
        auto p = bossBase ("boss.md-500", "MD-500", "md500 md 500 modulation chorus flanger phaser tremolo",
                           "MD-500 Owner's Manual (eng01) and MIDI Implementation 1.00");
        p.faceplate = juce::Colour (0xff6fb1d0);   // enclosure colour measured from the official product photo (boss.info/global/products/md-500)
        p.utilities = { { "Effect on", "CC 27=127", "Effect on (CC#27 ON). In simul mode it turns the selected patch on. " + onOffNote, 3 },
                        { "Bypass", "CC 27=0", "Bypass (CC#27 OFF). In simul mode it turns the selected patch off. " + onOffNote, 9 },
                        { "Patch A on", "CC 28=127", "Patch A on (CC#28 ON). " + onOffNote, 4 },
                        { "Patch A bypass", "CC 28=0", "Patch A bypassed (CC#28 OFF).", 9 },
                        { "Patch B on", "CC 29=127", "Patch B on (CC#29 ON). " + onOffNote, 6 },
                        { "Patch B bypass", "CC 29=0", "Patch B bypassed (CC#29 OFF).", 9 } };
        p.pedals = { { "Expression", 16 }, { "Rate/value", 17 }, { "Depth", 18 }, { "E. Level", 19 }, { "Param 1", 20 }, { "Param 2", 21 } };
        p.switches = ctlSwitches();
        p.pedalNote = "Expression (CC#16) works like a pedal on the EXP jack. The others move that knob like turning it.";
        p.notes = ccDefaultsNote + "\n\n" + presetsNote + "\n\n"
                  "Controls: effect on / bypass CC#27 (in simul mode it turns the selected patch on / off), patch A on / bypass CC#28, "
                  "patch B on / bypass CC#29. The manual gives these as ON / OFF without numbers: the tiles send 127 / 0.\n\n"
                  "Expression: EXP pedal CC#16. Knobs (0-127): RATE/VALUE CC#17, DEPTH CC#18, E. LEVEL CC#19, PARAM 1 CC#20, PARAM 2 "
                  "CC#21. When the RATE/VALUE CC is set to CC#1-31, some types also read the fine-step CC (CC#33-63); these clips send only "
                  "the main CC.\n\n"
                  "Switches: CTL 1 / CTL 2 (CC#80 / 81, 127 pressed, 0 released) emulate pressing an external switch: each tile presses "
                  "and releases it. The manual has no tap tempo, tuner or [A] / [B] / [TAP/CTL] CCs for the MD-500.\n\n"
                  "MIDI channel: Rx CHANNEL in the MIDI menu (Ch.1 in the manual's screenshot).\n\nThat's every CC in the manual's MIDI "
                  "tables. midi.guide has no chart for the 500 series, so it's from the Boss documents only.\n\n" + connectionNote;
        v.push_back (p);
    }
}
}
