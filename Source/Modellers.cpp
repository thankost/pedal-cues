#include "Modellers.h"

#include <tuple>

// Numbers from: Line 6 Owner's Manuals for firmware 3.80 (Helix, Helix LT, Helix Rack/Control, HX Stomp, HX Stomp XL,
// HX Effects; MIDI chapters), the POD Go Owner's Manual 2.50, the Helix Stadium online manual (Rev D, v1.3), and the
// Fractal Audio Owner's Manuals (Axe-Fx II Doc Q7.0 default CC table p.194; AX8 pp.99-100; FX8 default CC table), and the
// HeadRush User Guides (Core, Prime, Flex Prime v5.1.0 "External MIDI Control"; Pedalboard and Gigboard v2.1.2, MX5 v1.5).
namespace modellers
{
namespace
{
const juce::Colour line6Colour   { 0xffd9534f };
const juce::Colour fractalColour { 0xff4e9bd8 };
const juce::Colour headrushColour { 0xfff28c28 };
const juce::Colour darkglassColour { 0xff9aa5b1 };
const juce::Colour neuralColour { 0xff3fa9f5 };

juce::String n (int v) { return juce::String (v); }

const juce::String oneTap ("One tap. Tempo needs several taps a beat apart: drop the clip on each beat, or set the tempo in the preset.");
const juce::String toggle ("Any value toggles it: each clip switches it the other way.");

std::vector<Action> helixLooper (int record, int play, int once, int undo, int direction, int speed, int block)
{
    std::vector<Action> a { { "Record", "CC " + n (record) + "=127", {}, 0 }, { "Overdub", "CC " + n (record) + "=0", {}, 1 },
                            { "Play", "CC " + n (play) + "=127", {}, 3 },     { "Stop", "CC " + n (play) + "=0", {}, 9 },
                            { "Play once", "CC " + n (once) + "=127", {}, 4 }, { "Undo/redo", "CC " + n (undo) + "=127", {}, 6 },
                            { "Forward", "CC " + n (direction) + "=0", {}, 5 }, { "Reverse", "CC " + n (direction) + "=127", {}, 5 },
                            { "Full speed", "CC " + n (speed) + "=0", {}, 7 },  { "Half speed", "CC " + n (speed) + "=127", {}, 7 } };
    if (block > 0)
    {
        a.push_back ({ "Looper on", "CC " + n (block) + "=127", "Turns the looper block on (and enters Looper footswitch mode).", 3 });
        a.push_back ({ "Looper off", "CC " + n (block) + "=0", {}, 9 });
    }
    return a;
}

std::vector<Action> fractalLooper()
{
    return { { "Record", "CC 28=127", {}, 0 }, { "Play", "CC 29=127", {}, 3 }, { "Once", "CC 30=127", {}, 4 },
             { "Dub", "CC 31=127", {}, 1 }, { "Reverse", "CC 32=127", {}, 5 }, { "Half speed", "CC 120=127", {}, 7 },
             { "Undo", "CC 121=127", {}, 6 } };
}

std::vector<Control> numbered (const juce::String& prefix, int first, int count, int firstCc)
{
    std::vector<Control> c;
    for (int i = 0; i < count; ++i)
        c.push_back ({ prefix + n (first + i), firstCc + i, 127 });
    return c;
}

const juce::String line6Buffered ("Line 6: a snapshot sent during a preset load is held until the preset has loaded, so with Load preset "
                                  "first the snapshot goes out with the preset.");
const juce::String fsNote ("Presses the footswitch in whatever mode the unit is in (Stomp, Preset or Snapshot), like your foot would. "
                           "It acts on the preset that's loaded.");

Profile helixBase (const juce::String& id, const juce::String& model, const juce::String& aliases, const juce::String& manual, bool exp3)
{
    Profile p;
    p.id = id; p.brand = "Line 6"; p.model = model; p.shortName = model; p.aliases = "line6 helix " + aliases;
    p.colour = line6Colour; p.manual = manual; p.scheme = Scheme::helix;
    p.sceneWord = "Snapshot"; p.sceneCount = 8; p.sceneCc = 69; p.sceneBuffered = true; p.sceneNote = line6Buffered;
    p.switchesTitle = "Footswitches";
    p.switches = numbered ("FS", 1, 5, 49);
    for (const auto& c : numbered ("FS", 7, 5, 54))
        p.switches.push_back (c);
    p.switchesNote = fsNote + " There's no MIDI CC for FS6 or FS12.";
    p.utilities = { { "Tuner", "CC 68=127", "Opens or closes the tuner screen. " + toggle, 5 }, { "Tap", "CC 64=127", oneTap, 1 },
                    { "Next preset", "CC 72=127", "Firmware 3.80 or newer.", 9 }, { "Previous preset", "CC 72=0", "Firmware 3.80 or newer.", 9 },
                    { "EXP toe switch", "CC 59=127", toggle, 7 } };
    p.looper = helixLooper (60, 61, 62, 63, 65, 66, 67);
    p.pedals = { { "EXP 1", 1 }, { "EXP 2", 2 } };
    if (exp3)
        p.pedals.push_back ({ "EXP 3", 3 });
    p.pedalNote = "Moves what the preset assigns to this expression pedal, like a real pedal.";
    p.tunerOn = "CC 68=127"; p.tunerOff = "CC 68=127";
    p.channelHint = "Must match the unit: Global Settings > MIDI/Tempo > MIDI Base Channel.";
    p.usbToThru = 3;   // not documented
    p.notes = "Presets: PC 0-127 = 01A-32D in the setlist the unit is on. With \"Switch to the preset's setlist\" on, each preset also sends its "
              "setlist (CC#32: 0 FACTORY 1, 1 FACTORY 2, 2-6 USER 1-5, 7 TEMPLATES).\n\n" + line6Buffered + "\n\n"
              "Footswitch CCs 49-58 press FS1-FS5 and FS7-FS11 (no CC for FS6 or FS12). Next/previous preset needs firmware 3.80 or newer.\n\n"
              "MIDI Thru passes on what arrives at the 5-pin MIDI In; Line 6 doesn't say whether USB MIDI is passed on too.";
    return p;
}

Profile hxBase (const juce::String& id, const juce::String& model, const juce::String& aliases, const juce::String& manual, Scheme scheme,
                int snapshots, std::vector<Control> switches, int looperBlock)
{
    Profile p;
    p.id = id; p.brand = "Line 6"; p.model = model; p.shortName = model; p.aliases = "line6 helix hx " + aliases;
    p.colour = line6Colour; p.manual = manual; p.scheme = scheme;
    p.sceneWord = "Snapshot"; p.sceneCount = snapshots; p.sceneCc = 69; p.sceneBuffered = true; p.sceneNote = line6Buffered;
    p.switchesTitle = "Footswitches"; p.switches = std::move (switches); p.switchesNote = fsNote;
    p.utilities = { { "Tuner", "CC 68=127", "Opens or closes the tuner screen. " + toggle, 5 }, { "Tap", "CC 64=127", oneTap, 1 },
                    { "All bypass", "CC 70=0", "Bypasses every block (CC#70, 0-63).", 9 }, { "All on", "CC 70=127", "Turns the blocks back on (CC#70, 64-127).", 3 },
                    { "Next preset", "CC 72=127", "Firmware 3.80 or newer.", 9 }, { "Previous preset", "CC 72=0", "Firmware 3.80 or newer.", 9 } };
    p.looper = helixLooper (60, 61, 62, 63, 65, 66, looperBlock);
    p.pedals = { { "EXP 1", 1 }, { "EXP 2", 2 } };
    p.pedalNote = "Moves what the preset assigns to this expression pedal, like a real pedal.";
    p.tunerOn = "CC 68=127"; p.tunerOff = "CC 68=127";
    p.channelHint = "Must match the unit: Global Settings > MIDI/Tempo > MIDI Base Channel (channel 1 out of the box).";
    p.usbToThru = 3;
    p.notes = "No setlists. " + line6Buffered + "\n\nMIDI Thru passes on what arrives at the 5-pin MIDI In; Line 6 doesn't say whether USB MIDI "
              "is passed on too. It listens on MIDI channel 1 out of the box.";
    return p;
}

// HeadRush: every CC acts on any value, except the expression pedals. Blocks toggle; there are no on / off values.
std::vector<Control> headrushBlocks (int count)
{
    return numbered ("Block ", 1, count, 75);
}

std::vector<Action> headrushLooper (bool openClose)
{
    std::vector<Action> a { { "Record", "CC 70=127", toggle, 0 }, { "Start/Stop", "CC 69=127", toggle, 3 }, { "Insert", "CC 71=127", {}, 1 },
                            { "Peel", "CC 72=127", "Removes the last overdub.", 6 }, { "Mute", "CC 73=127", toggle, 9 },
                            { "Reverse", "CC 74=127", toggle, 5 }, { "1/2 speed", "CC 65=127", {}, 7 }, { "2x speed", "CC 66=127", {}, 7 },
                            { "1/2 loop", "CC 67=127", {}, 4 }, { "2x loop", "CC 68=127", {}, 4 } };
    if (openClose)
        a.push_back ({ "Looper screen", "CC 91=127", "Opens or closes the looper. " + toggle, 8 });
    return a;
}

const juce::String headrushBlocksNote ("Block 1-14 as numbered in the rig (CC#75 and up). Each clip toggles the block: on if it was off, off if it "
                                       "was on. Rename the tiles after your blocks.");

Profile headrushBase (const juce::String& id, const juce::String& model, const juce::String& aliases, const juce::String& manual, Scheme scheme)
{
    Profile p;
    p.id = id; p.brand = "HeadRush"; p.model = model; p.shortName = "HeadRush " + model; p.aliases = "headrush head rush " + aliases;
    p.colour = headrushColour; p.manual = manual; p.scheme = scheme;
    p.sceneWord = "Scene"; p.sceneCount = 0; p.sceneCc = 21; p.scenePerCc = true;
    p.sceneNote = "Scene 1 is CC#21, scene 2 CC#22 and so on (any value). With Load preset first, the scene follows the rig 1/16 later.";
    p.switchesTitle = "Blocks";
    p.switchesNote = headrushBlocksNote;
    p.utilities = { { "Tap", "CC 64=127", oneTap, 1 } };
    p.channelHint = "Must match the unit: Global Settings > MIDI > MIDI Channel (or Omni). Prog Change Recv must be on.";
    p.usbMidi = false;
    p.usbToThru = 0;
    return p;
}

Profile fractalBase (const juce::String& id, const juce::String& model, const juce::String& shortName, const juce::String& aliases,
                     const juce::String& manual, Scheme scheme)
{
    Profile p;
    p.id = id; p.brand = "Fractal Audio"; p.model = model; p.shortName = shortName; p.aliases = "fractal " + aliases;
    p.colour = fractalColour; p.manual = manual; p.scheme = scheme;
    p.sceneWord = "Scene"; p.sceneCount = 8; p.sceneCc = 34; p.sceneBuffered = false;
    p.sceneNote = "Scene Select is CC#34 (factory default). With Load preset first, the scene follows the preset 1/16 later, like on the QC.";
    p.switchesTitle = "Blocks"; p.switchesOnOff = true;
    p.switchesNote = "Factory default CCs. Fractal units can be set to treat these as a toggle (any value flips it); the tiles assume the "
                     "default: 0-63 = off (bypassed), 64-127 = on.";
    p.looper = fractalLooper();
    p.pedalNote = "An External Controller: it moves whatever a preset's modifier uses it for. Set the modifier's source to this controller.";
    p.tunerOn = "CC 15=127"; p.tunerOff = "CC 15=0";
    p.channelHint = "Must match the unit's MIDI channel (Setup > MIDI, 1 by default).";
    p.usbToThru = 3;
    return p;
}
}

const std::vector<Profile>& all()
{
    static const std::vector<Profile> list = []
    {
        std::vector<Profile> v;

        // Line 6
        v.push_back (helixBase ("line6.helix-floor", "Helix Floor", "floor", "Helix Owner's Manual, firmware 3.80", true));
        v.push_back (helixBase ("line6.helix-lt", "Helix LT", "lt", "Helix LT Owner's Manual, firmware 3.80", false));
        {
            auto p = helixBase ("line6.helix-rack", "Helix Rack", "rack control", "Helix Rack/Helix Control Owner's Manual, firmware 3.80", true);
            p.model = "Helix Rack (+ Control)";
            p.switchesNote = fsNote + " The footswitches are on Helix Control. There's no MIDI CC for FS6 or FS12.";
            v.push_back (p);
        }
        {
            std::vector<Control> fs { { "FS1", 49, 127 }, { "FS2", 50, 127 }, { "FS3", 51, 127 }, { "FS4 (ext.)", 52, 127 }, { "FS5 (ext.)", 53, 127 } };
            auto p = hxBase ("line6.hx-stomp", "HX Stomp", "stomp", "HX Stomp Owner's Manual, firmware 3.80", Scheme::hxStomp, 3, fs, 0);
            p.switchesNote = fsNote + " FS4 and FS5 are the external footswitch jack (tip and ring).";
            p.sceneNote = line6Buffered + " The manual says HX Stomp has three snapshots per preset (its MIDI table also lists a fourth value).";
            p.notes = "Presets: 42 banks of three, 01A-42C = PC 0-125. " + p.notes + "\n\nThe manual says three snapshots per preset, but its MIDI "
                      "table also lists value 3 = Snapshot 4: this page uses three.";
            v.push_back (p);
        }
        {
            auto p = hxBase ("line6.hx-stomp-xl", "HX Stomp XL", "stomp xl", "HX Stomp XL Owner's Manual, firmware 3.80", Scheme::hxFour, 4,
                             numbered ("FS", 1, 8, 49), 0);
            p.notes = "Presets: 32 banks of four, 01A-32D = PC 0-127. " + p.notes;
            v.push_back (p);
        }
        {
            auto p = hxBase ("line6.hx-effects", "HX Effects", "effects fx", "HX Effects Owner's Manual, firmware 3.80", Scheme::hxFour, 4,
                             numbered ("FS", 1, 6, 49), 67);
            p.notes = "Presets: 32 banks of four, 01A-32D = PC 0-127. " + p.notes;
            v.push_back (p);
        }
        {
            Profile p = hxBase ("line6.pod-go", "POD Go", "pod go wireless podgo", "POD Go Owner's Manual 2.50 (also covers POD Go Wireless)",
                                Scheme::podGo, 4, numbered ("FS", 1, 8, 49), 0);
            p.model = "POD Go / POD Go Wireless";
            p.utilities = { { "Tuner", "CC 68=127", "Opens or closes the tuner screen. " + toggle, 5 }, { "Tap", "CC 64=127", oneTap, 1 } };
            p.hasDin = false;
            p.usbToThru = 0;
            p.channelHint = "Must match POD Go: Global Settings > MIDI/Tempo > MIDI Channel (1 out of the box).";
            p.notes = "POD Go takes MIDI over USB only: it has no 5-pin MIDI, so it can't be in a daisy chain or pass MIDI on to another pedal. "
                      "Set its cue track's MIDI output to POD Go (USB).\n\nPresets: PC 0-127 = 01A-32D. With \"Switch to the preset's setlist\" on, "
                      "each preset also sends its setlist (CC#32: 0 Factory, 1 User).\n\n" + line6Buffered;
            v.push_back (p);
        }
        {
            Profile p;
            p.id = "line6.helix-stadium"; p.brand = "Line 6"; p.model = "Helix Stadium / Stadium XL"; p.shortName = "Helix Stadium";
            p.aliases = "line6 helix stadium xl"; p.colour = line6Colour; p.manual = "Helix Stadium online manual (Rev D, v1.3)";
            p.scheme = Scheme::stadium;
            p.sceneWord = "Snapshot"; p.sceneCount = 8; p.sceneCc = 69; p.sceneBuffered = true; p.sceneNote = line6Buffered;
            p.switchesTitle = "Footswitch mode";
            p.switches = { { "Stomp A", 37, 0 }, { "Stomp B", 37, 1 }, { "Preset", 37, 2 }, { "Snapshot", 37, 3 }, { "Combo", 37, 4 }, { "Transport", 37, 6 } };
            p.switchesNote = "Sets the footswitch mode (CC#37). Stadium has no MIDI CCs that press single footswitches.";
            p.utilities = { { "Tuner", "CC 9=34", "Opens or closes the tuner. " + toggle, 5 }, { "Mute all", "CC 9=24", toggle, 0 },
                            { "Tap", "CC 64=127", oneTap, 1 }, { "Preset up", "CC 9=13", {}, 9 }, { "Preset down", "CC 9=12", {}, 9 },
                            { "Toe switch", "CC 36=127", toggle, 7 } };
            p.looper = helixLooper (58, 59, 60, 53, 55, 54, 62);
            p.looper.push_back ({ "Clear", "CC 52=127", {}, 0 });
            p.pedals = { { "EXP 1", 1 }, { "EXP 2", 2 } };
            p.pedalNote = "Moves what the preset assigns to this expression pedal, like a real pedal.";
            p.tunerOn = "CC 9=34"; p.tunerOff = "CC 9=34";
            p.channelHint = "Must match Stadium's Global MIDI Channel (Global Settings > MIDI, 1 out of the box).";
            p.usbToThru = 2; p.usbThruSetting = "MIDI Over USB C";
            p.notes = "Stadium uses a different MIDI map from older Helix units.\n\nPresets: with \"Switch to the preset's setlist\" on, each preset sends "
                      "CC#32 first: 0 = FACTORY PRESETS, 1-4 = the USER PRESETS groups (1A-32D, 33A-64D, 65A-96D, 97A-128D), 5 and up = your setlists. "
                      "Then PC 0-127.\n\n" + line6Buffered + "\n\nChannels: this page uses the Global MIDI Channel. Block bypass and parameter control use "
                      "a separate Bypass/Ctrl channel (2 out of the box), which this page doesn't cover.\n\nWith MIDI Thru on, Stadium passes on MIDI from "
                      "its MIDI In and from USB-C (with MIDI Over USB C on).";
            v.push_back (p);
        }

        // HeadRush (fixed CC maps)
        {
            auto p = headrushBase ("headrush.core", "Core", "core", "HeadRush Core User Guide v5.1.0", Scheme::headrush);
            p.sceneCount = 10;
            p.switches = headrushBlocks (14);
            p.utilities = { { "Tuner", "CC 92=127", "Opens or closes the tuner. " + toggle, 5 }, { "Tap", "CC 64=127", oneTap, 1 },
                            { "Next rig", "CC 17=127", {}, 9 }, { "Previous rig", "CC 16=127", {}, 9 },
                            { "Rig mode", "CC 97=127", "Footswitches load rigs.", 8 }, { "Stomp mode", "CC 94=127", "Footswitches switch blocks.", 8 } };
            p.looper = headrushLooper (true);
            p.pedals = { { "Expression", 1 } };
            p.pedalNote = "The external expression pedal (CC#1): moves what the rig assigns to it, like a real pedal.";
            p.tunerOn = "CC 92=127"; p.tunerOff = "CC 92=127";
            p.notes = "Presets: each rig has a MIDI PROG number (Rig settings, shown 1-128 = Program Change 0-127). Set it on the rig, then enter the "
                      "same number here. HeadRush doesn't use bank select or setlists over MIDI.\n\nScenes: 10 per rig, CC#21-30.\n\n"
                      + headrushBlocksNote + "\n\nThe footswitch CCs (CC#49-53) aren't tiles: they do what each footswitch does in the current "
                      "mode, and need a press and a release.\n\nConnection: 5-pin MIDI In and MIDI Out / Thru. The manual doesn't mention MIDI over USB "
                      "from a computer, so use a MIDI cable from your interface. MIDI Thru passes on what arrives at the MIDI In.";
            v.push_back (p);
        }
        {
            auto p = headrushBase ("headrush.prime", "Prime", "prime", "HeadRush Prime User Guide v5.1.0", Scheme::headrush);
            p.sceneCount = 8;
            p.switches = headrushBlocks (14);
            p.utilities = { { "Tuner", "CC 92=127", "Opens or closes the tuner. " + toggle, 5 }, { "Tap", "CC 64=127", oneTap, 1 },
                            { "Next rig", "CC 17=127", {}, 9 }, { "Previous rig", "CC 16=127", {}, 9 },
                            { "Rig mode", "CC 97=127", "Footswitches load rigs.", 8 }, { "Stomp mode", "CC 94=127", "Footswitches switch blocks.", 8 } };
            p.looper = headrushLooper (true);
            p.pedals = { { "Built-in pedal", 1 }, { "External pedal", 2 } };
            p.pedalNote = "Moves what the rig assigns to this expression pedal (CC#1 built-in, CC#2 external), like a real pedal.";
            p.tunerOn = "CC 92=127"; p.tunerOff = "CC 92=127";
            p.notes = "Presets: each rig has a MIDI PROG number (Rig settings, shown 1-128 = Program Change 0-127). Set it on the rig, then enter the "
                      "same number here. HeadRush doesn't use bank select or setlists over MIDI.\n\nScenes: 8 per rig, CC#21-28.\n\n"
                      + headrushBlocksNote + "\n\nThe footswitch CCs (CC#49-60) aren't tiles: they do what each footswitch does in the current "
                      "mode, and need a press and a release.\n\nConnection: 5-pin MIDI In and MIDI Out / Thru. The manual doesn't mention MIDI over USB "
                      "from a computer, so use a MIDI cable from your interface. MIDI Thru passes on what arrives at the MIDI In.";
            v.push_back (p);
        }
        {
            auto p = headrushBase ("headrush.flex-prime", "Flex Prime", "flex prime", "HeadRush Flex Prime User Guide v5.1.0", Scheme::headrush);
            p.sceneCount = 6;
            p.switches = headrushBlocks (14);
            p.utilities = { { "Tuner", "CC 92=127", "Opens or closes the tuner. " + toggle, 5 }, { "Tap", "CC 64=127", oneTap, 1 },
                            { "Next rig", "CC 17=127", {}, 9 }, { "Previous rig", "CC 16=127", {}, 9 },
                            { "Rig mode", "CC 97=127", "Footswitches load rigs.", 8 }, { "Stomp mode", "CC 94=127", "Footswitches switch blocks.", 8 } };
            p.looper = headrushLooper (true);
            p.pedals = { { "Built-in pedal", 1 }, { "External pedal", 2 } };
            p.pedalNote = "Moves what the rig assigns to this expression pedal (CC#1 built-in, CC#2 external), like a real pedal.";
            p.tunerOn = "CC 92=127"; p.tunerOff = "CC 92=127";
            p.midiIn = "TRS MIDI In";
            p.notes = "Presets: each rig has a MIDI PROG number (Rig settings, shown 1-128 = Program Change 0-127). Set it on the rig, then enter the "
                      "same number here. HeadRush doesn't use bank select or setlists over MIDI.\n\nScenes: 6 per rig, CC#21-26.\n\n"
                      + headrushBlocksNote + "\n\nThe footswitch CCs (CC#49-51) aren't tiles: they do what each footswitch does in the current "
                      "mode, and need a press and a release.\n\nConnection: 3.5 mm TRS MIDI In and MIDI Out / Thru (Type A): use a TRS MIDI cable or a "
                      "5-pin to TRS adapter. The manual doesn't clearly cover MIDI over USB from a computer, so use a MIDI cable from your interface.";
            v.push_back (p);
        }
        for (const auto& [id, model, aliases, manual, blocks, trs, fsCcs] : {
                 std::tuple<const char*, const char*, const char*, const char*, int, bool, const char*>
                 { "headrush.pedalboard", "Pedalboard", "pedalboard", "HeadRush Pedalboard User Guide v2.1.2", 11, false, "CC#49-60" },
                 { "headrush.gigboard", "Gigboard", "gigboard", "HeadRush Gigboard User Guide v2.1.2", 11, false, "CC#50-53" },
                 { "headrush.mx5", "MX5", "mx5 mx 5", "HeadRush MX5 User Guide v1.5", 11, true, "CC#50-52" } })
        {
            auto p = headrushBase (id, model, aliases, manual, Scheme::headrushOld);
            p.switches = headrushBlocks (blocks);
            p.switchesNote = juce::String ("Block 1-11 as numbered in the rig (CC#75-85). Each clip toggles the block: on if it was off, off if it was "
                                           "on. Rename the tiles after your blocks.");
            p.looper = headrushLooper (false);
            if (trs)
                p.midiIn = "TRS MIDI In";
            p.notes = "Presets: each rig has a MIDI Prog number (0-127 = Program Change 0-127). Set it on the rig, then enter the same number here. "
                      "No bank select, setlists or scenes over MIDI, and no tuner CC.\n\n" + p.switchesNote + "\n\nThe footswitch CCs ("
                      + juce::String (fsCcs) + ") aren't tiles: they do what each footswitch does in the current mode.\n\n"
                      + (trs ? "Connection: 3.5 mm TRS MIDI In (Type A): use a TRS MIDI cable or a 5-pin to TRS adapter."
                             : "Connection: 5-pin MIDI In and MIDI Out / Thru.")
                      + " The manual doesn't mention MIDI over USB from a computer, so use a MIDI cable from your interface.";
            v.push_back (p);
        }

        // Neural DSP Nano Cortex (User Manual 2.2.0, Incoming MIDI CC List)
        {
            Profile p;
            p.id = "neural.nano-cortex"; p.brand = "Neural DSP"; p.model = "Nano Cortex"; p.shortName = "Nano Cortex";
            p.aliases = "neural dsp nano cortex nanocortex"; p.colour = neuralColour; p.manual = "Nano Cortex User Manual 2.2.0";
            p.beta = false;   // same maker and MIDI conventions as the Quad Cortex, like the QC Mini
            p.scheme = Scheme::nano;
            p.sceneWord = "Scene"; p.sceneCount = 0;
            p.switchesTitle = "Slots"; p.switchesOnOff = true;
            p.switches = { { "Input Gate", 34, 127 }, { "Capture", 35, 127 }, { "Cab / IR", 36, 127 }, { "FX 1", 37, 127 }, { "FX 2", 38, 127 },
                           { "FX 3", 39, 127 }, { "FX 4", 40, 127 }, { "FX 5", 41, 127 } };
            p.switchesNote = "Turns the slot on or off in the loaded preset (0-63 = off, 64-127 = on).";
            p.utilities = { { "Tuner on", "CC 43=127", {}, 5 }, { "Tuner off", "CC 43=0", {}, 9 }, { "Tap", "CC 42=127", oneTap, 1 } };
            p.pedals = { { "Expression", 1 } };
            p.pedalNote = "Moves what the preset assigns to the expression pedal (CC#1), like a real pedal. Handy on the Nano: its EXP/MIDI "
                          "jack is either a pedal or MIDI, so with MIDI on it there's no room for a real pedal.";
            p.tunerOn = "CC 43=127"; p.tunerOff = "CC 43=0";
            p.channelHint = "Must match the Nano: Cortex Cloud app > Settings > MIDI CHANNEL. For TRS MIDI, set EXP/MIDI INPUT MODE to MIDI.";
            p.midiIn = "TRS MIDI In (EXP/MIDI jack)";
            p.hasThru = false;
            p.usbToThru = 0;
            p.notes = "Presets: the 64 slots under ALL PRESETS are Program Change 0-63 (the app's PC/CC button shows the numbers). No banks, "
                      "setlists or scenes over MIDI.\n\nSlots: Input Gate, Capture, Cab/IR and FX 1-5 on or off (CC#34-41). Utilities: tuner "
                      "(CC#43) and tap (CC#42). Expression: CC#1.\n\nConnection: USB-C MIDI, or TRS MIDI Type A into the EXP/MIDI jack (set "
                      "EXP/MIDI INPUT MODE to MIDI in the app). The Nano has no MIDI Out, so it can't pass MIDI on to another pedal: give a "
                      "second pedal its own output. Its MIDI THRU setting only forwards TRS MIDI to USB.";
            v.push_back (p);
        }

        // Darkglass Anagram (Anagram Manual, KosmOS 1.17, MIDI Support)
        {
            Profile p;
            p.id = "darkglass.anagram"; p.brand = "Darkglass"; p.model = "Anagram"; p.shortName = "Anagram";
            p.aliases = "darkglass anagram kosmos bass guitar essentials"; p.colour = darkglassColour;
            p.manual = "Anagram Manual, KosmOS 1.17";
            p.scheme = Scheme::hxStomp;   // 42 banks x A-C, shown 01A-42C
            p.pcOffset = 1;
            p.sceneWord = "Scene"; p.sceneCount = 3; p.sceneCc = 107; p.sceneValueBase = 1; p.sceneLetters = true;
            p.sceneNote = "Scene Select is CC#107. With Load preset first, the scene follows the preset 1/16 later.";
            p.switchesTitle = "Footswitches (Stomp mode)"; p.switchesOnOff = true;
            p.switches = { { "Foot A", 17, 127 }, { "Foot B", 18, 127 }, { "Foot C", 19, 127 } };
            p.switchesNote = "The Stomp mode footswitch bindings (CC#17-19, the default numbers). With the Anagram's Toggle Logic setting on "
                             "Value (the default), 0-63 = off and 64-127 = on; on Toggle, every clip flips it.";
            p.utilities = { { "Tuner", "CC 86=127", "Enters or exits the tuner. " + toggle, 5 },
                            { "Preset mode", "CC 85=1", {}, 8 }, { "Stomp mode", "CC 85=2", {}, 8 }, { "Scene mode", "CC 85=3", {}, 8 },
                            { "Next preset", "CC 105=127", {}, 9 }, { "Previous preset", "CC 106=127", {}, 9 },
                            { "Next scene", "CC 108=127", {}, 9 }, { "Previous scene", "CC 109=127", {}, 9 } };
            p.looper = { { "Play/Stop", "CC 111=127", toggle, 3 }, { "Rec/Dub", "CC 112=127", toggle, 0 }, { "Undo", "CC 114=127", {}, 6 },
                         { "Redo", "CC 115=127", {}, 6 }, { "Clear slot", "CC 113=127", {}, 9 }, { "Looper screen", "CC 110=127", toggle, 8 } };
            p.pedals = { { "Expression", 89 }, { "Knob 1", 20 }, { "Knob 2", 21 }, { "Knob 3", 22 }, { "Knob 4", 23 }, { "Knob 5", 24 },
                         { "Knob 6", 25 } };
            p.pedalNote = "Moves what the preset binds to the expression pedal (CC#89) or to a knob binding (CC#20-25, the default numbers).";
            p.tunerOn = "CC 86=127"; p.tunerOff = "CC 86=127";
            p.channelHint = "Must match the Anagram: Device settings > MIDI > MIDI In Chan (Omni listens on every channel).";
            p.midiIn = "TRS MIDI In";
            p.usbToThru = 3;
            p.notes = "Presets: 126, 01A-42C. Program Change 1 = 01A, 2 = 01B ... 126 = 42C: the Anagram ignores value 0, and its MIDI Style "
                      "numbering (001-126) matches the Program Change. Not tested on a unit yet: if 01A doesn't load, tell us.\n\n"
                      "Scenes: each preset holds three (A-C), selected with CC#107 = 1, 2, 3. The manual's table lists values 1-126 (01A-42C); "
                      "PedalCues sends 1-3 for the open preset's scenes. Tell us if your Anagram needs something else.\n\n"
                      "The footswitch, knob and expression CCs are the default binding numbers; if you changed them in Bindings > Edit CCs, "
                      "use a custom MIDI device. \"Ignore Redundant PC\" decides whether reloading the open preset does anything.\n\n"
                      "Connection: 3.5 mm TRS MIDI In and Out (Type A), and USB MIDI (turn USB MIDI on). MIDI Through passes incoming MIDI "
                      "on to the ports you choose.";
            v.push_back (p);
        }

        // Darkglass Infinity 500 Combo and Exponent 500 (their manuals' default MIDI mapping). "Per-value" switch type by
        // default: a bypass is 0 = effect on, 1 = effect off; the FX loop is 0 = off, 1 = on. CC#0 is a control on both.
        auto darkglassAmp = [] (const juce::String& id, const juce::String& model, const juce::String& aliases, const juce::String& manual)
        {
            Profile p;
            p.id = id; p.brand = "Darkglass"; p.model = model; p.shortName = model; p.aliases = "darkglass " + aliases + " amp bass";
            p.colour = darkglassColour; p.manual = manual; p.scheme = Scheme::darkglassAmp; p.pcOffset = 2;
            p.sceneCount = 0; p.switchesOnOff = true; p.cc0IsControl = true;
            p.utilities = { { "Bypass", "PC 0", "Bypasses the amp's processing (Program Change 0).", 9 },
                            { "Mute", "PC 1", "Mutes the amp (Program Change 1).", 0 } };
            p.testMessage = "PC 2";   // preset 1
            p.channelHint = "It listens on every channel (Omni) out of the box; set a channel in the Darkglass Suite (Configuration).";
            p.midiIn = "5-pin MIDI In"; p.hasThru = false; p.usbToThru = 0;
            return p;
        };
        {
            auto p = darkglassAmp ("darkglass.infinity-500-combo", "Infinity 500 Combo", "infinity 500 combo", "Infinity 500 Combo manual (MIDI mapping)");
            p.switchesTitle = "Effects";
            p.switches = { { "Noise gate", 1, 0, 1 }, { "Octaver", 3, 0, 1 }, { "Compressor", 6, 0, 1 }, { "Drive", 9, 0, 1 }, { "FX loop", 0, 1, 0 } };
            p.switchesNote = "Default Per-value switch type: the effects send 0 = on, 1 = off (bypassed); the FX loop sends 1 = on, 0 = off.";
            p.utilities.push_back ({ "Comp pre-drive", "CC 7=0", "Compressor position (CC#7): 0 = pre-drive, 1 = post-drive.", 7 });
            p.utilities.push_back ({ "Comp post-drive", "CC 7=1", "Compressor position (CC#7): 0 = pre-drive, 1 = post-drive.", 7 });
            p.utilities.push_back ({ "Leo Bass", "CC 10=0", "Drive mode (CC#10).", 0 });
            p.utilities.push_back ({ "Vintage MT", "CC 10=1", "Drive mode (CC#10): Vintage Microtubes.", 1 });
            p.utilities.push_back ({ "B3K", "CC 10=2", "Drive mode (CC#10): Microtubes B3K.", 2 });
            p.utilities.push_back ({ "Alpha Omega", "CC 10=3", "Drive mode (CC#10).", 4 });
            p.looperTitle = "IR slots";
            p.looper.push_back ({ "IR bypass", "CC 22=0", "IR slot (CC#22): 0 = IR bypass, 1-7 = slots.", 9 });
            for (int i = 1; i <= 7; ++i)
                p.looper.push_back ({ "IR slot " + n (i), "CC 22=" + n (i), {}, 3 + i });
            p.pedals = { { "Drive amount", 11 }, { "Drive tone", 12 }, { "Drive blend", 13 }, { "Drive level", 14 }, { "Comp amount", 8 },
                         { "Octaver filter", 4 }, { "Octaver blend", 5 }, { "Gate threshold", 2 }, { "Tweeter", 15 },
                         { "EQ low shelf", 16 }, { "EQ 250 Hz", 17 }, { "EQ 500 Hz", 18 }, { "EQ 1.5 kHz", 19 }, { "EQ 3 kHz", 20 },
                         { "EQ slider 6", 21 }, { "Preset level", 23 } };
            p.pedalNote = "Moves that control of the loaded preset (0-127), like turning the knob or slider.";
            p.notes = "These are the default MIDI numbers from the manual. If you changed the mapping or the switch type in the Darkglass Suite "
                      "(Configuration), use a custom MIDI device instead.\n\nPresets 1-5 = Program Change 2-6; Bypass = PC 0, Mute = PC 1.\n\n"
                      "Effects use the default Per-value switch type: 0 = effect on, 1 = effect off (noise gate CC#1, octaver CC#3, compressor "
                      "CC#6, drive CC#9); FX loop CC#0: 1 = on, 0 = off. Compressor position CC#7, drive mode CC#10 (Leo Bass, Vintage "
                      "Microtubes, B3K, Alpha Omega), IR slot CC#22 (the IR slots view).\n\nUpdate to firmware 1.2 or higher before using MIDI "
                      "with anything other than the Darkglass MIDI Footswitch.\n\nConnection: 5-pin MIDI In on the back panel (the 7-pin "
                      "connector is only for the Darkglass MIDI Footswitch) or USB MIDI. No MIDI Out, so it can't pass MIDI on to another pedal.";
            v.push_back (p);
        }
        {
            auto p = darkglassAmp ("darkglass.exponent-500", "Exponent 500", "exponent 500 head", "Exponent 500 manual (MIDI configuration)");
            p.switchesTitle = "Footswitches";
            for (int i = 0; i < 5; ++i)
                p.switches.push_back ({ "Footswitch " + n (i + 1), 106 + i, 1, 0 });
            p.switchesNote = "Switches the effect bypasses bound to this Darkglass MIDI Footswitch button in the preset (CC#106-110, "
                             "Per-value switch type: 1 and 0 are the two positions).";
            p.pedals = { { "Quick-Pot A", 0 }, { "Quick-Pot B", 1 }, { "Quick-Pot C", 2 }, { "Quick-Pot D", 3 }, { "Quick-Pot E", 4 },
                         { "Master volume", 5 } };
            p.pedalNote = "Moves the Quick-Pot (what it controls is set per preset) or the master volume, like turning it.";
            p.notes = "These are the default MIDI numbers from the manual. If you changed the mapping or the switch type in the Darkglass Suite, "
                      "use a custom MIDI device instead.\n\nPresets 1-5 = Program Change 2-6; Bypass = PC 0, Mute = PC 1.\n\n"
                      "Quick-Pots A-E = CC#0-4, master volume CC#5 (the Expression view). Footswitches 1-5 = CC#106-110: they switch the "
                      "effect bypasses bound to each Darkglass MIDI Footswitch button in the preset.\n\nConnection: 5-pin MIDI In on the back "
                      "panel (the 7-pin connector is only for the Darkglass MIDI Footswitch) or USB MIDI. No MIDI Out, so it can't pass MIDI on.";
            v.push_back (p);
        }

        // Line 6 effect pedals (Effects & Pedals tab): DL4 MkII (Owner's Manual Rev D, firmware 1.02, MIDI chapter) and
        // HX One (Owner's Manual Rev C, firmware 3.70, MIDI chapter). Both: MIDI channel 1 out of the box, 5-pin MIDI In and
        // Out/Thru, USB MIDI; all MIDI control is global (whatever preset is loaded).
        {
            Profile p;
            p.id = "line6.dl4-mkii"; p.brand = "Line 6"; p.model = "DL4 MkII"; p.shortName = "DL4";
            p.aliases = "dl4 mk2 mkii delay modeler looper stompbox green"; p.colour = juce::Colour (0xff5cb85c);
            p.manual = "DL4 MkII Owner's Manual (Rev D, firmware 1.02)";
            p.pedal = true; p.scheme = Scheme::dl4;
            p.sceneWord = "Scene"; p.sceneCount = 0;
            p.mainTitle = "Controls";
            p.switchesTitle = "Note value";
            const char* notes[] = { "1/8 triplet", "1/8", "Dotted 1/8", "1/4 triplet", "1/4", "Dotted 1/4", "1/2 triplet", "1/2", "Dotted 1/2" };
            for (int i = 0; i < 9; ++i)
                p.switches.push_back ({ notes[i], 12, i });
            p.switchesNote = "Sets the delay's note value (Time Subdivisions, CC#12) in the loaded preset.";
            p.utilities = { { "Preset on", "CC 4=0", "Turns the loaded preset on (CC#4 = 0-63).", 3 },
                            { "Bypass", "CC 4=127", "Bypasses the loaded preset (CC#4 = 64-127), like pressing its lit footswitch.", 9 },
                            { "Tap", "CC 64=127", oneTap, 1 },
                            { "Looper mode on", "CC 9=127", "Classic Looper mode on (CC#9).", 4 },
                            { "Looper mode off", "CC 9=0", "Classic Looper mode off (CC#9).", 9 },
                            { "Reverb > delay", "CC 19=0", "Reverb-delay routing (CC#19): reverb before the delay.", 7 },
                            { "Parallel", "CC 19=1", "Reverb-delay routing (CC#19): reverb and delay in parallel.", 7 },
                            { "Delay > reverb", "CC 19=2", "Reverb-delay routing (CC#19): reverb after the delay.", 7 } };
            p.looper = { { "Record", "CC 60=127", {}, 0 }, { "Overdub", "CC 60=0", {}, 1 }, { "Play", "CC 61=127", {}, 3 }, { "Stop", "CC 61=0", {}, 9 },
                         { "Play once", "CC 62=127", {}, 4 }, { "Undo", "CC 63=0", "Undoes the last overdub (CC#63 = 0-63).", 6 },
                         { "Redo", "CC 63=127", "Redoes the last overdub (CC#63 = 64-127).", 6 },
                         { "Forward", "CC 65=0", {}, 5 }, { "Reverse", "CC 65=127", {}, 5 },
                         { "Full speed", "CC 66=0", {}, 7 }, { "Half speed", "CC 66=127", {}, 7 } };
            const juce::String clears ("Changes the loaded preset's model. Changing a model clears the preset's pedal and footswitch assignments "
                                       "(for good if you then save the preset).");
            ModelGroup mk2 { "MkII delays", "CC#1 = 0-14", {} }, legacy { "Legacy delays", "CC#1 = 15-29", {} }, reverbs { "Reverbs", "CC#2 = 0-15", {} };
            const char* delays[] = { "Vintage Digital", "Crisscross", "Euclidean", "Dual Delay", "Pitch Echo", "ADT", "Ducked", "Harmony",
                                     "Heliosphere", "Transistor", "Cosmos", "Multi Pass", "Adriatic", "Elephant Man", "Glitch",
                                     "Digital", "Digital w/ Mod", "Echo Platter", "Stereo", "Ping Pong", "Reverse", "Dynamic", "Auto-Vol",
                                     "Tube Echo", "Tape Echo", "Multi-Head", "Sweep", "Analog", "Analog w/ Mod", "Lo Res Delay" };
            for (int i = 0; i < 30; ++i)
                (i < 15 ? mk2 : legacy).actions.push_back ({ delays[i], "CC 1=" + n (i), clears, i < 15 ? 3 : 1 });
            const char* verbs[] = { "Room", "Searchlights", "Particle Verb", "Double Tank", "Octo", "Tile", "Ducking", "Plateaux", "Cave", "Plate",
                                    "Ganymede", "Chamber", "Hot Springs", "Hall", "Glitz", "Reverb off" };
            for (int i = 0; i < 16; ++i)
                reverbs.actions.push_back ({ verbs[i], "CC 2=" + n (i), clears, i == 15 ? 9 : 6 });
            p.models = { mk2, legacy, reverbs };
            p.pedals = { { "Expression", 3 }, { "Delay time", 11 }, { "Repeats", 13 }, { "Tweak", 14 }, { "Tweez", 15 }, { "Mix", 16 },
                         { "Reverb decay", 17 }, { "Reverb predelay", 18 }, { "Reverb mix", 20 } };
            p.pedalNote = "Expression (CC#3) moves what the preset assigns to the expression pedal: make that assignment first with a pedal or "
                          "footswitch on the EXP PEDAL jack. The others move that knob like turning it (in Classic Looper mode, time, repeats, "
                          "tweak, tweez and mix set the looper's echo).";
            p.testMessage = "PC 0";   // preset A
            p.channelHint = "Must match the DL4 MkII: Global Settings > MIDI Channel (1 out of the box).";
            p.usbToThru = 2; p.usbThruSetting = "MIDI THRU on (Global Settings; off out of the box)";
            p.notes = "Presets: Program Change 0-5 = presets A-F (the footswitches), 6-127 = presets 7-128, which you can only reach over MIDI.\n\n"
                      "Controls: the note value (CC#12), preset on / bypass (CC#4), tap (CC#64), Classic Looper mode (CC#9) and the "
                      "reverb-delay routing (CC#19). Models: delay model CC#1 (MkII 0-14, Legacy 15-29) and reverb model CC#2 (0-14, 15 = "
                      "off) of the loaded preset; changing a model clears the preset's pedal assignments.\n\nLooper: CC#60-66 (record/overdub, "
                      "play/stop, play once, undo/redo, reverse, half speed); it answers even outside Classic Looper mode.\n\n"
                      "Connection: 5-pin MIDI In, MIDI Out/Thru and USB MIDI. MIDI THRU is off out of the box: turn it on (Global Settings) "
                      "to pass MIDI on to another pedal.";
            v.push_back (p);
        }
        {
            Profile p;
            p.id = "line6.hx-one"; p.brand = "Line 6"; p.model = "HX One"; p.shortName = "HX One";
            p.aliases = "hx one hxone effect stompbox flux"; p.colour = line6Colour;
            p.manual = "HX One Owner's Manual (Rev C, firmware 3.70)";
            p.pedal = true; p.scheme = Scheme::hxOne;
            p.sceneWord = "Scene"; p.sceneCount = 0;
            p.mainTitle = "Switches";
            p.switchesTitle = "Footswitches";
            p.switches = { { "ON", 1, 127 }, { "FLUX", 2, 127 } };
            p.switchesNote = "ON (CC#1) turns the effect on if it's off and off if it's on. FLUX (CC#2) is like pressing FLUX, in any mode.";
            p.utilities = { { "Engage", "CC 4=127", "Turns the effect on (CC#4 = 64-127), whatever it was.", 3 },
                            { "Bypass", "CC 4=0", "Bypasses the effect (CC#4 = 0-63), whatever it was.", 9 },
                            { "Tap", "CC 93=127", oneTap + " The manual's table lists CC#93 for tap.", 1 },
                            { "Home", "CC 5=0", "Shows the Home view (CC#5 = 0).", 8 },
                            { "Preset list", "CC 5=1", "Shows the Preset List (CC#5 = 1).", 8 },
                            { "Tuner", "CC 5=2", "Shows the tuner (CC#5 = 2).", 5 } };
            p.looper = helixLooper (60, 61, 62, 63, 65, 66, 0);
            p.pedals = { { "Expression", 3 } };
            for (int i = 1; i <= 24; ++i)
                p.pedals.push_back ({ "Parameter " + n (i), i <= 11 ? 20 + i : 21 + i });   // CC#21-31, 33-45 (32 is reserved)
            p.pedals.push_back ({ "FLUX on time", 46 });
            p.pedals.push_back ({ "FLUX off time", 48 });
            p.pedalNote = "Expression (CC#3) works when Settings > Pedal Jack is ExpFS4. Parameters 1-24 are the loaded effect's parameters "
                          "in order (CC#21-45, no 32).";
            p.testMessage = "PC 0";   // preset 000
            p.channelHint = "Must match the HX One: Settings View > MIDI Channel (1 out of the box).";
            p.usbToThru = 1;
            p.notes = "Presets: Program Change 0-127 = presets 000-127 (bank select is ignored; MIDI PC Rx must be on, as it is out of the box).\n\n"
                      "Switches: ON (CC#1, any value toggles) and FLUX (CC#2). Engage / Bypass (CC#4) set it whatever it was. Views (CC#5): "
                      "Home, Preset List, Tuner. Tap: CC#93, as in the manual's table (a note elsewhere says TAP uses CC#64; tell us which "
                      "your unit answers).\n\nLooper (Simple Looper models): CC#60-66.\n\nExpression: CC#3 (Pedal Jack = ExpFS4), "
                      "parameters 1-24 = CC#21-31 and 33-45, FLUX times CC#46 and 48.\n\nConnection: 5-pin MIDI In, MIDI Out/Thru (Thru on "
                      "out of the box) and USB MIDI.";
            v.push_back (p);
        }

        // Fractal Audio (factory default CCs)
        {
            auto p = fractalBase ("fractal.axe-fx-2", "Axe-Fx II / XL / XL+", "Axe-Fx II", "axe fx axefx 2 ii xl plus",
                                  "Axe-Fx II Owner's Manual (Doc Q7.0)", Scheme::axeFx2);
            p.switches = { { "Amp 1", 37, 127 }, { "Amp 2", 38, 127 }, { "Cab 1", 39, 127 }, { "Drive 1", 49, 127 }, { "Drive 2", 50, 127 },
                           { "Delay 1", 47, 127 }, { "Reverb 1", 83, 127 }, { "Chorus 1", 41, 127 }, { "Comp 1", 43, 127 }, { "Wah 1", 97, 127 } };
            p.utilities = { { "Tuner on", "CC 15=127", {}, 5 }, { "Tuner off", "CC 15=0", {}, 9 }, { "Tap", "CC 14=127", oneTap, 1 },
                            { "Next scene", "CC 123=127", {}, 9 }, { "Previous scene", "CC 124=127", {}, 9 } };
            p.pedals = { { "External 1", 16 }, { "External 2", 17 }, { "External 3", 18 }, { "External 4", 19 } };
            p.usbToThru = 2; p.usbThruSetting = "USB Adapter Mode (I/O > MIDI)";
            p.notes = "These are the factory default CCs (default table p.194). If you changed them on the unit (I/O > CTRL), use a custom MIDI device "
                      "instead.\n\nPresets: banks A-F of 128 (A-C on the Mark I/II), selected with CC#0 and then the Program Change.\n\n"
                      "\"Ignore Redundant PC\" is off by default, so loading the preset that's already loaded reloads it.\n\n"
                      "USB: MIDI over USB reaches the 5-pin MIDI Out only with USB Adapter Mode on.";
            v.push_back (p);
        }
        {
            auto p = fractalBase ("fractal.ax8", "AX8", "AX8", "ax8 ax 8", "AX8 Owner's Manual (default CCs p.99)", Scheme::ax8);
            p.switches = { { "Drive 1", 49, 127 }, { "Drive 2", 50, 127 }, { "Delay 1", 47, 127 }, { "Delay 2", 48, 127 }, { "Reverb", 83, 127 },
                           { "Chorus", 41, 127 }, { "Comp", 43, 127 }, { "Wah", 97, 127 }, { "Pitch", 77, 127 }, { "Flanger", 56, 127 } };
            p.switchesNote += " The AX8 manual doesn't state the 0-63 / 64-127 rule; it's the Axe-Fx II's. No default CC for Amp or Cab bypass.";
            p.utilities = { { "Tuner", "CC 15=127", "Enters or exits the tuner (CC#15).", 5 }, { "Tap", "CC 14=127", oneTap, 1 },
                            { "Next scene", "CC 123=127", {}, 9 }, { "Previous scene", "CC 124=127", {}, 9 } };
            p.pedals = { { "External 5", 20 }, { "External 6", 21 }, { "External 7", 22 }, { "External 8", 23 } };
            p.pedalNote += " Externals 1-4 default to the pedal jacks, so these use 5-8.";
            p.tunerOff = "CC 15=127";
            p.notes = "These are the factory default CCs (p.99). If you changed them on the unit, use a custom MIDI device instead.\n\n"
                      "Presets: 512 in 64 banks of 8, shown 01:1-64:8 (CC#0 = 0 for banks 01-16, 1 for 17-32, ...; then the Program Change).\n\n"
                      "No default CC for Amp or Cab bypass.";
            v.push_back (p);
        }
        {
            auto p = fractalBase ("fractal.fx8", "FX8 (Mark I / II)", "FX8", "fx8 fx 8", "FX8 Owner's Manual (default CC table)", Scheme::fx8);
            p.switches = { { "Drive 1", 49, 127 }, { "Drive 2", 50, 127 }, { "Delay 1", 47, 127 }, { "Delay 2", 48, 127 }, { "Reverb 1", 83, 127 },
                           { "Chorus 1", 41, 127 }, { "Comp 1", 43, 127 }, { "Wah 1", 97, 127 }, { "Pitch", 77, 127 }, { "Phaser 1", 75, 127 } };
            p.switchesNote += " The FX8 manual doesn't state the 0-63 / 64-127 rule; it's the Axe-Fx II's.";
            p.utilities = { { "Tuner", "CC 15=127", "Enters or exits the tuner (CC#15).", 5 }, { "Tap", "CC 14=127", oneTap, 1 },
                            { "Bypass (unit)", "CC 13=127", "The front-panel Bypass (CC#13).", 9 },
                            { "Next scene", "CC 123=127", {}, 9 }, { "Previous scene", "CC 124=127", {}, 9 } };
            p.pedals = { { "External 1", 16 }, { "External 2", 17 }, { "External 3", 18 }, { "External 4", 19 } };
            p.tunerOff = "CC 15=127";
            p.notes = "These are the factory default CCs. If you changed them on the unit, use a custom MIDI device instead. Mark I needs firmware 3.0 "
                      "or newer.\n\nPresets: 128, A1-P8 = Program Change 0-127. The FX8 doesn't respond to bank select.";
            v.push_back (p);
        }
        // Effect pedals with fixed MIDI charts, one file per brand group (the Effects & Pedals tab).
        addStrymon (v);
        addBossPedals (v);
        addBoutiquePedals (v);
        return v;
    }();
    return list;
}

const Profile* find (const juce::String& id)
{
    for (const auto& p : all())
        if (p.id == id)
            return &p;
    return nullptr;
}

//==============================================================================
bool hasSetlists (const Profile& p)
{
    return p.scheme == Scheme::helix || p.scheme == Scheme::podGo || p.scheme == Scheme::stadium;
}

juce::StringArray setlistNames (const Profile& p)
{
    switch (p.scheme)
    {
        case Scheme::helix:   return { "FACTORY 1", "FACTORY 2", "USER 1", "USER 2", "USER 3", "USER 4", "USER 5", "TEMPLATES" };
        case Scheme::podGo:   return { "Factory", "User" };
        case Scheme::stadium:
        {
            juce::StringArray s { "FACTORY PRESETS", "USER 1A-32D", "USER 33A-64D", "USER 65A-96D", "USER 97A-128D" };
            for (int i = 1; i <= 16; ++i)
                s.add ("Your setlist " + n (i));
            return s;
        }
        case Scheme::hxStomp: case Scheme::hxFour: case Scheme::axeFx2: case Scheme::ax8: case Scheme::fx8:
        case Scheme::headrush: case Scheme::headrushOld: case Scheme::nano: case Scheme::darkglassAmp: case Scheme::dl4: case Scheme::hxOne:
        case Scheme::strymonAB: case Scheme::strymonABC: case Scheme::strymonMX: case Scheme::numbered: case Scheme::boss500: break;
    }
    return {};
}

int slotsPerBank (const Profile& p)
{
    switch (p.scheme)
    {
        case Scheme::hxStomp: return 3;
        case Scheme::axeFx2: case Scheme::headrush: case Scheme::headrushOld: case Scheme::dl4: case Scheme::hxOne: return 128;
        case Scheme::strymonAB: case Scheme::strymonMX: return 2;
        case Scheme::strymonABC: case Scheme::boss500: return 3;
        case Scheme::numbered: return juce::jmax (1, p.presetCount);
        case Scheme::nano:    return 64;
        case Scheme::darkglassAmp: return 5;
        case Scheme::ax8: case Scheme::fx8: return 8;
        case Scheme::helix: case Scheme::podGo: case Scheme::stadium: case Scheme::hxFour: break;
    }
    return 4;
}

int presetsPerSetlist (const Profile& p, int)
{
    switch (p.scheme)
    {
        case Scheme::hxStomp: return 126;
        case Scheme::nano:    return 64;
        case Scheme::darkglassAmp: return 5;
        case Scheme::axeFx2:  return 768;
        case Scheme::strymonAB: return 200;
        case Scheme::strymonABC: case Scheme::strymonMX: return 300;
        case Scheme::numbered: return juce::jmax (1, p.presetCount);
        case Scheme::boss500: return 297;
        case Scheme::ax8:     return 512;
        case Scheme::helix: case Scheme::podGo: case Scheme::stadium: case Scheme::hxFour: case Scheme::fx8:
        case Scheme::headrush: case Scheme::headrushOld: case Scheme::dl4: case Scheme::hxOne: break;
    }
    return 128;
}

juce::StringArray bankNames (const Profile& p, int setlist)
{
    juce::StringArray b;
    if (p.scheme == Scheme::headrush || p.scheme == Scheme::headrushOld)
        return { "MIDI PROG" };   // one list, no banks
    if (p.scheme == Scheme::nano)
        return { "ALL PRESETS" };
    if (p.scheme == Scheme::darkglassAmp || p.scheme == Scheme::dl4 || p.scheme == Scheme::hxOne || p.scheme == Scheme::numbered)
        return { "PRESETS" };
    if (p.scheme == Scheme::boss500)
    {
        for (int i = 1; i <= 99; ++i)
            b.add (juce::String (i).paddedLeft ('0', 2));
        return b;
    }
    if (p.scheme == Scheme::strymonAB || p.scheme == Scheme::strymonABC || p.scheme == Scheme::strymonMX)
    {
        const auto digits = p.scheme == Scheme::strymonMX ? 3 : 2;
        for (int i = 0; i < presetsPerSetlist (p, setlist) / slotsPerBank (p); ++i)
            b.add (juce::String (i).paddedLeft ('0', digits));   // 00-99, 000-149: the bank numbers start at 0
        return b;
    }
    const auto banks = presetsPerSetlist (p, setlist) / slotsPerBank (p);
    const auto letters = p.scheme == Scheme::axeFx2 || p.scheme == Scheme::fx8;
    // Stadium's USER PRESETS groups continue the bank numbers: group 2 is 33A-64D.
    const auto first = p.scheme == Scheme::stadium && setlist >= 1 && setlist <= 4 ? (setlist - 1) * 32 + 1 : 1;
    for (int i = 0; i < banks; ++i)
        b.add (letters ? juce::String::charToString ((juce::juce_wchar) ('A' + i)) : juce::String (first + i).paddedLeft ('0', 2));
    return b;
}

juce::StringArray slotNames (const Profile& p)
{
    juce::StringArray s;
    const auto count = slotsPerBank (p);
    for (int i = 0; i < count; ++i)
        s.add (p.scheme == Scheme::numbered ? n (i + p.labelFrom)
               : p.scheme == Scheme::dl4 ? (i < 6 ? juce::String::charToString ((juce::juce_wchar) ('A' + i)) : n (i + 1))
               : p.scheme == Scheme::hxOne ? juce::String (i).paddedLeft ('0', 3)
               : (p.scheme == Scheme::headrush || p.scheme == Scheme::darkglassAmp) ? n (i + 1) : (p.scheme == Scheme::headrushOld || p.scheme == Scheme::nano) ? n (i)
               : p.scheme == Scheme::axeFx2 ? juce::String (i).paddedLeft ('0', 3)
               : (p.scheme == Scheme::ax8 || p.scheme == Scheme::fx8) ? n (i + 1)
               : juce::String::charToString ((juce::juce_wchar) ('A' + i)));
    return s;
}

juce::String presetLabel (const Profile& p, int setlist, int index)
{
    if (p.scheme == Scheme::headrush || p.scheme == Scheme::headrushOld)
        return "Prog " + slotNames (p)[juce::jlimit (0, 127, index)];
    if (p.scheme == Scheme::nano)
        return "PC " + n (juce::jlimit (0, 63, index));
    if (p.scheme == Scheme::darkglassAmp)
        return "Preset " + n (juce::jlimit (0, 4, index) + 1);
    if (p.scheme == Scheme::dl4 || p.scheme == Scheme::hxOne)
        return "Preset " + slotNames (p)[juce::jlimit (0, 127, index)];
    if (p.scheme == Scheme::numbered)
    {
        index = juce::jlimit (0, juce::jmax (0, p.presetCount - 1), index);
        for (const auto& [reserved, name] : p.reservedPrograms)
            if (reserved == index)
                return name;   // "Manual mode", "Bypass"...
        return "Preset " + n (index + p.labelFrom);
    }
    const auto per = slotsPerBank (p);
    const auto banks = bankNames (p, setlist);
    const auto bank = juce::jlimit (0, juce::jmax (0, banks.size() - 1), index / per);
    return banks[bank] + (p.scheme == Scheme::ax8 ? ":" : "") + slotNames (p)[index % per];
}

juce::String slotTitle (const Profile& p)
{
    return p.scheme == Scheme::headrush || p.scheme == Scheme::headrushOld ? "MIDI PROG (as set on the rig)"
         : p.scheme == Scheme::nano ? "Program Change (PC/CC view in the app)" : "Preset";
}

juce::String sceneLabel (const Profile& p, int sceneIndex)
{
    return p.sceneLetters ? juce::String::charToString ((juce::juce_wchar) ('A' + juce::jlimit (0, 25, sceneIndex))) : n (sceneIndex + 1);
}

juce::String sceneCcs (const Profile& p)
{
    if (p.scenePerCc && p.sceneCount > 1)
        return "CC#" + n (p.sceneCc) + "-" + n (p.sceneCc + p.sceneCount - 1);
    return "CC#" + n (p.sceneCc);
}

static juce::MidiMessage sceneMessage (const Profile& p, int channel, int sceneIndex)
{
    const auto s = juce::jlimit (0, juce::jmax (0, p.sceneCount - 1), sceneIndex);
    return p.scenePerCc ? juce::MidiMessage::controllerEvent (juce::jlimit (1, 16, channel), p.sceneCc + s, 127)
                        : juce::MidiMessage::controllerEvent (juce::jlimit (1, 16, channel), p.sceneCc, juce::jlimit (0, 127, s + p.sceneValueBase));
}

juce::String setlistLabel (const Profile& p, int setlist)
{
    return hasSetlists (p) ? setlistNames (p)[setlist] : juce::String();
}

int defaultSetlist (const Profile& p)
{
    return p.scheme == Scheme::helix ? 2 : (p.scheme == Scheme::podGo || p.scheme == Scheme::stadium) ? 1 : -1;
}

//==============================================================================
static int channelOf (int channel) { return juce::jlimit (1, 16, channel); }

void addPresetLoad (cues::Cue& c, const Profile& p, int channel, int setlist, int index, bool sendSetlist, double beat)
{
    const auto ch = channelOf (channel);
    index = juce::jlimit (0, presetsPerSetlist (p, setlist) - 1, index);
    if (hasSetlists (p) && sendSetlist && setlist >= 0)
        c.add (beat, juce::MidiMessage::controllerEvent (ch, 32, juce::jlimit (0, 127, setlist)));
    // Bank select on CC#0 (Strymon: always, so a clip never relies on the bank the pedal happens to be in).
    if (p.scheme == Scheme::axeFx2 || p.scheme == Scheme::ax8 || p.scheme == Scheme::strymonAB || p.scheme == Scheme::strymonABC
        || p.scheme == Scheme::strymonMX || p.scheme == Scheme::boss500 || (p.scheme == Scheme::numbered && p.sendBankCc0))
        c.add (beat, juce::MidiMessage::controllerEvent (ch, 0, index / 128));
    c.add (beat, juce::MidiMessage::programChange (ch, juce::jlimit (0, 127, index % 128 + p.pcOffset)));
}

cues::Cue preset (const Profile& p, int channel, int setlist, int index, bool sendSetlist, const juce::String& name)
{
    cues::Cue c;
    c.name = p.shortName + " " + (name.isNotEmpty() ? name : presetLabel (p, setlist, index));
    c.cc0IsControl = p.cc0IsControl;
    c.padCc = p.padCc;
    addPresetLoad (c, p, channel, setlist, index, sendSetlist, 0.0);
    return c;
}

cues::Cue scene (const Profile& p, int channel, int sceneIndex, const juce::String& name)
{
    cues::Cue c;
    c.name = p.shortName + " " + p.sceneWord + " " + sceneLabel (p, sceneIndex) + (name.isNotEmpty() ? " - " + name : juce::String());
    c.add (0.0, sceneMessage (p, channel, sceneIndex));
    c.padCc = p.padCc;
    return c;
}

cues::Cue sceneAfterPreset (const Profile& p, int channel, int setlist, int index, bool sendSetlist,
                            const juce::String& presetName, int sceneIndex, const juce::String& sceneName)
{
    cues::Cue c;
    c.name = p.shortName + " " + presetName + " > " + sceneLabel (p, sceneIndex) + (sceneName.isNotEmpty() ? " - " + sceneName : juce::String());
    addPresetLoad (c, p, channel, setlist, index, sendSetlist, 0.0);
    c.add (p.sceneBuffered ? 0.0 : fractalGap, sceneMessage (p, channel, sceneIndex));
    return c;
}

cues::Cue switchCue (const Profile& p, int channel, int switchIndex, bool on, const juce::String& name)
{
    const auto& s = p.switches[(size_t) juce::jlimit (0, (int) p.switches.size() - 1, switchIndex)];
    cues::Cue c;
    const auto label = name.isNotEmpty() ? name : s.name;
    c.name = p.shortName + " " + label + (p.switchesOnOff ? (on ? " On" : " Off") : juce::String());
    c.add (0.0, juce::MidiMessage::controllerEvent (channelOf (channel), s.cc, p.switchesOnOff ? (on ? s.value : s.offValue) : s.value));
    if (s.press)   // a footswitch: down, then up 1/16 later, like a foot
        c.add (0.25, juce::MidiMessage::controllerEvent (channelOf (channel), s.cc, s.offValue));
    c.toggles = ! p.switchesOnOff || s.press;   // a press or a toggle
    c.cc0IsControl = p.cc0IsControl;
    c.padCc = p.padCc;
    return c;
}

cues::Cue switchAfterPreset (const Profile& p, int channel, int setlist, int index, bool sendSetlist,
                             const juce::String& presetName, int switchIndex, bool on, const juce::String& name)
{
    auto single = switchCue (p, channel, switchIndex, on, name);
    cues::Cue c;
    c.name = p.shortName + " " + presetName + " > " + single.name.fromFirstOccurrenceOf (p.shortName + " ", false, false);
    addPresetLoad (c, p, channel, setlist, index, sendSetlist, 0.0);
    c.add (fractalGap, single.events.front().second);
    return c;
}

cues::Cue action (const Profile& p, int channel, const Action& a)
{
    // Tuner, tap, next scene, looper...: most are presses or toggles, so never pad the clip with a second one.
    auto c = cues::custom::cue (channel, p.shortName + " " + a.name, a.messages, 0);
    c.toggles = true;
    c.cc0IsControl = p.cc0IsControl;
    c.padCc = p.padCc;
    return c;
}
}
