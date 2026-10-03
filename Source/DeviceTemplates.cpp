#include "DeviceTemplates.h"
#include "State.h"

#include <functional>

// Every number below comes from the manufacturer's manual named in the template's notes (Line 6 Owner's Manuals for
// firmware 3.80, POD Go 2.50, the Helix Stadium online manual Rev D; Fractal Audio Owner's Manuals). Message text uses
// the custom-device syntax (cues::custom::parse): "PC n", "CC n=v", "bank n" (CC#0). Programs count from 0, as sent.
namespace templates
{
namespace
{
const juce::Colour fractalColour { 0xff4e9bd8 };

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
    return "From the " + t.brand + " " + t.model + " manual. Not tested on hardware: check the numbers on your unit.";
}

juce::ValueTree createUnit (const Template& t)
{
    // A short name for the tab: "Axe-Fx III (Mk I / Mk II / Turbo)" -> "Axe-Fx III", "POD Go / POD Go Wireless" -> "POD Go".
    auto unit = state::createCustomUnit (t.model.upToFirstOccurrenceOf (" (", false, false).upToFirstOccurrenceOf (" /", false, false).trim());
    unit.removeAllChildren (nullptr);
    unit.setProperty (IDs::templateId, t.id, nullptr);
    unit.setProperty (IDs::colour, t.colour.toString(), nullptr);
    unit.setProperty (IDs::programBase, 0, nullptr);
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
