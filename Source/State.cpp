#include "State.h"

#include <algorithm>
#include "CueModel.h"
#include "Modellers.h"

#include <array>
#include <iterator>

namespace state
{

const juce::Array<NamedColour>& palette()
{
    static const NamedColour raw[] = {
        { "Red",    0xffe5484d }, { "Orange", 0xfff76b15 }, { "Yellow", 0xffffc53d },
        { "Green",  0xff46a758 }, { "Teal",   0xff12a594 }, { "Blue",   0xff0090ff },
        { "Purple", 0xff8e4ec6 }, { "Pink",   0xffd6409f }, { "White",  0xffe8e8e8 },
        { "Grey",   0xff6f6f6f }
    };
    static const juce::Array<NamedColour> colours (raw, (int) std::size (raw));
    return colours;
}

static juce::Colour paletteColour (int i)
{
    const auto& p = palette();
    return juce::Colour (p.getReference (((i % p.size()) + p.size()) % p.size()).argb);
}

juce::Colour colourOf (const juce::ValueTree& node, juce::Colour fallback)
{
    const auto s = node[IDs::colour].toString();
    return s.isEmpty() ? fallback : juce::Colour::fromString (s);
}

static void setDefault (juce::ValueTree& t, const juce::Identifier& id, const juce::var& v)
{
    if (! t.hasProperty (id))
        t.setProperty (id, v, nullptr);
}

static void sanitisePreset (juce::ValueTree& p)
{
    setDefault (p, IDs::name, "New Preset");
    setDefault (p, IDs::setlist, 1);
    setDefault (p, IDs::bank, 1);
    setDefault (p, IDs::slot, 0);
    setDefault (p, IDs::colour, paletteColour (5).toString());

    int scenes = 0, stomps = 0;
    for (auto c : p)
    {
        if (c.hasType (IDs::Scene)) ++scenes;
        if (c.hasType (IDs::Stomp)) ++stomps;
    }

    for (int i = scenes; i < 8; ++i)
    {
        juce::ValueTree s (IDs::Scene);
        s.setProperty (IDs::name, "Scene " + cues::qc::letter (i), nullptr);
        s.setProperty (IDs::colour, paletteColour (i).toString(), nullptr);
        p.appendChild (s, nullptr);
    }

    for (int i = stomps; i < 8; ++i)
    {
        juce::ValueTree s (IDs::Stomp);
        s.setProperty (IDs::name, "Stomp " + cues::qc::letter (i), nullptr);
        p.appendChild (s, nullptr);
    }
}

static void sanitiseCustomUnit (juce::ValueTree& u)
{
    setDefault (u, IDs::name, "My device");
    setDefault (u, IDs::colour, juce::Colour (0xff8e7cf0).toString());
    setDefault (u, IDs::notes, juce::String());
    setDefault (u, IDs::programBase, 0);
    setDefault (u, IDs::expCc, 11);
    for (int i = u.getNumChildren(); --i >= 0;)
        if (! u.getChild (i).hasType (IDs::Group))
            u.removeChild (i, nullptr);
    for (auto g : u)
    {
        setDefault (g, IDs::name, "Group");
        for (int i = g.getNumChildren(); --i >= 0;)
            if (! g.getChild (i).hasType (IDs::CueTile))
                g.removeChild (i, nullptr);
        for (auto t : g)
        {
            setDefault (t, IDs::name, "Tile");
            setDefault (t, IDs::colour, paletteColour (5).toString());
            setDefault (t, IDs::note, juce::String());
            setDefault (t, IDs::messages, juce::String());
        }
    }
}

static juce::ValueTree customTile (const juce::String& name, const juce::String& messages, const juce::String& note, int colour)
{
    juce::ValueTree t (IDs::CueTile);
    t.setProperty (IDs::name, name, nullptr);
    t.setProperty (IDs::messages, messages, nullptr);
    t.setProperty (IDs::note, note, nullptr);
    t.setProperty (IDs::colour, paletteColour (colour).toString(), nullptr);
    return t;
}

bool usesCc0AsControl (const juce::ValueTree& unit)
{
    if ((bool) unit[IDs::padRepeat])
        return true;
    for (auto group : unit)
        for (auto tile : group)
        {
            const auto steps = cues::custom::parse (tile[IDs::messages].toString(), (int) unit[IDs::programBase]).steps;
            for (size_t i = 0; i < steps.size(); ++i)
                if (steps[i].kind == cues::custom::Step::Kind::controller && steps[i].number == 0
                    && std::none_of (steps.begin() + (long) i, steps.end(), [] (const auto& st) { return st.kind == cues::custom::Step::Kind::program; }))
                    return true;
        }
    return false;
}

juce::ValueTree createCustomUnit (const juce::String& name)
{
    juce::ValueTree u (IDs::Unit);
    u.setProperty (IDs::name, name, nullptr);
    u.setProperty (IDs::notes, "Notes: which manual page the numbers come from, and why it's set up this way.", nullptr);

    // Examples to edit: Program Changes work the same on every device; CC numbers are each device's own.
    juce::ValueTree presets (IDs::Group);
    presets.setProperty (IDs::name, "Presets", nullptr);
    presets.appendChild (customTile ("Preset 1", "PC 0", "Example: the first preset. Check how your manual counts presets.", 5), nullptr);
    presets.appendChild (customTile ("Preset 2", "PC 1", {}, 4), nullptr);
    u.appendChild (presets, nullptr);

    juce::ValueTree effects (IDs::Group);
    effects.setProperty (IDs::name, "Switches", nullptr);
    effects.appendChild (customTile ("Switch on", "CC 50=127", "Example: set the CC number and value from your device's MIDI chart.", 0), nullptr);
    effects.appendChild (customTile ("Switch off", "CC 50=0", {}, 9), nullptr);
    u.appendChild (effects, nullptr);

    sanitiseCustomUnit (u);
    return u;
}

juce::ValueTree customUnit (const juce::ValueTree& root)
{
    const auto units = root.getChildWithName (IDs::CustomUnits);
    if (units.getNumChildren() == 0)
        return {};
    return units.getChild (juce::jlimit (0, units.getNumChildren() - 1, (int) root[IDs::selectedCustomUnit]));
}

juce::ValueTree fxCustomUnit (const juce::ValueTree& root)
{
    const auto units = root.getChildWithName (IDs::CustomUnits);
    if ((int) root[IDs::fxUnit] != fxCustom || units.getNumChildren() == 0)
        return {};
    return units.getChild (juce::jlimit (0, units.getNumChildren() - 1, (int) root[IDs::fxCustomUnit]));
}

juce::ValueTree fxModellerData (juce::ValueTree& root)
{
    if ((int) root[IDs::fxUnit] != fxModeller)
        return {};
    return modeller (root, root[IDs::fxProfile].toString());
}

int fxModellerChannel (const juce::ValueTree& root)
{
    auto r = root;
    return juce::jlimit (1, 16, (int) fxModellerData (r).getProperty (IDs::channel, 3));
}

bool isPedal (const juce::ValueTree& unit)
{
    return unit[IDs::category].toString() == "pedal";
}

int channelFor (const juce::ValueTree&, const juce::ValueTree& unit)
{
    return juce::jlimit (1, 16, (int) unit.getProperty (IDs::channel, 1));
}

// Where the first tab's device keeps its channel.
static juce::ValueTree ampChannelNode (juce::ValueTree& root, juce::Identifier& id)
{
    id = IDs::channel;
    switch ((int) root[IDs::ampUnit])
    {
        case 1: id = IDs::kemperChannel; return root;
        case 2: id = IDs::kemperPlayerChannel; return root;
        case customAmpUnit: return customUnit (root);
        case modellerAmpUnit: return modeller (root, root[IDs::modellerProfile].toString());
        case qcMiniAmpUnit: id = IDs::qcMiniChannel; return root;
        default: id = IDs::qcChannel; return root;
    }
}

int ampChannel (const juce::ValueTree& root)
{
    auto r = root;
    juce::Identifier id;
    const auto node = ampChannelNode (r, id);
    return juce::jlimit (1, 16, (int) node.getProperty (id, 1));
}

void setAmpChannel (juce::ValueTree& root, int channel)
{
    juce::Identifier id;
    if (auto node = ampChannelNode (root, id); node.isValid())
        node.setProperty (id, juce::jlimit (1, 16, channel), nullptr);
}

int pedalChannel (const juce::ValueTree& root)
{
    if (const auto u = fxCustomUnit (root); u.isValid())
        return channelFor (root, u);
    if ((int) root[IDs::fxUnit] == fxModeller)
        return fxModellerChannel (root);
    return juce::jlimit (1, 16, (int) root[(int) root[IDs::whModel] == 1 ? IDs::whDtChannel : IDs::whChannel]);
}

void setPedalChannel (juce::ValueTree& root, int channel)
{
    channel = juce::jlimit (1, 16, channel);
    if (auto u = fxCustomUnit (root); u.isValid())
        u.setProperty (IDs::channel, channel, nullptr);
    else if (auto m = fxModellerData (root); m.isValid())
        m.setProperty (IDs::channel, channel, nullptr);
    else
        root.setProperty ((int) root[IDs::whModel] == 1 ? IDs::whDtChannel : IDs::whChannel, channel, nullptr);
}

void removeCustomUnit (juce::ValueTree& root, const juce::ValueTree& unit)
{
    auto units = root.getChildWithName (IDs::CustomUnits);
    const auto index = units.indexOf (unit);
    if (index < 0)
        return;
    const auto onFirstTab = (int) root[IDs::ampUnit] == customAmpUnit && (int) root[IDs::selectedCustomUnit] == index;
    const auto onPedalsTab = (int) root[IDs::fxUnit] == fxCustom && (int) root[IDs::fxCustomUnit] == index;
    units.removeChild (unit, nullptr);
    // Later devices move up one; the deleted one's tab falls back to the default device (the Quad Cortex / the Whammy V).
    for (const auto* id : { &IDs::selectedCustomUnit, &IDs::fxCustomUnit })
        if ((int) root[*id] > index)
            root.setProperty (*id, (int) root[*id] - 1, nullptr);
    if (onFirstTab || units.getNumChildren() == 0)
    {
        root.setProperty (IDs::selectedCustomUnit, 0, nullptr);
        if ((int) root[IDs::ampUnit] == customAmpUnit)
            root.setProperty (IDs::ampUnit, 0, nullptr);
    }
    if (onPedalsTab || units.getNumChildren() == 0)
    {
        root.setProperty (IDs::fxCustomUnit, 0, nullptr);
        if ((int) root[IDs::fxUnit] == fxCustom)
            root.setProperty (IDs::fxUnit, fxWhammy, nullptr);   // the default device takes its place
    }
}

void showCustomUnitOn (juce::ValueTree& root, int index, bool pedalsTab)
{
    auto unit = root.getChildWithName (IDs::CustomUnits).getChild (index);
    if (! unit.isValid())
        return;
    unit.setProperty (IDs::category, pedalsTab ? "pedal" : "amp", nullptr);
    if (pedalsTab)
    {
        if ((int) root[IDs::ampUnit] == customAmpUnit && (int) root[IDs::selectedCustomUnit] == index)
            root.setProperty (IDs::ampUnit, 0, nullptr);
        root.setProperty (IDs::fxCustomUnit, index, nullptr);
        root.setProperty (IDs::fxUnit, fxCustom, nullptr);
    }
    else
    {
        if ((int) root[IDs::fxUnit] == fxCustom && (int) root[IDs::fxCustomUnit] == index)
            root.setProperty (IDs::fxUnit, fxWhammy, nullptr);   // it moved to the first tab: the default device takes its place
        root.setProperty (IDs::selectedCustomUnit, index, nullptr);
        root.setProperty (IDs::ampUnit, customAmpUnit, nullptr);
    }
}

juce::ValueTree addCustomUnit (juce::ValueTree& root, juce::ValueTree unit, bool onPedalsTab)
{
    auto units = root.getOrCreateChildWithName (IDs::CustomUnits, nullptr);
    const auto base = unit[IDs::name].toString().trim().isNotEmpty() ? unit[IDs::name].toString().trim() : juce::String ("My device");
    auto name = base;
    for (int n = 2;; ++n)
    {
        bool taken = false;
        for (auto u : units)
            taken = taken || u[IDs::name].toString() == name;
        if (! taken)
            break;
        name = base + " (" + juce::String (n) + ")";
    }
    unit.setProperty (IDs::name, name, nullptr);
    if (! unit.hasProperty (IDs::channel))
        unit.setProperty (IDs::channel, onPedalsTab ? 3 : 1, nullptr);   // an amp modeller starts on 1, a pedal on 3
    sanitiseCustomUnit (unit);
    unit.setProperty (IDs::category, onPedalsTab ? "pedal" : "amp", nullptr);
    units.appendChild (unit, nullptr);
    if (onPedalsTab)
    {
        root.setProperty (IDs::fxCustomUnit, units.getNumChildren() - 1, nullptr);
        root.setProperty (IDs::fxUnit, fxCustom, nullptr);
    }
    else
        root.setProperty (IDs::selectedCustomUnit, units.getNumChildren() - 1, nullptr);
    return unit;
}

bool saveUnit (const juce::ValueTree& unit, const juce::File& file)
{
    juce::ValueTree doc (IDs::PedalCuesUnit);
    doc.setProperty ("format", 1, nullptr);
    doc.appendChild (unit.createCopy(), nullptr);
    if (auto xml = doc.createXml())
        return file.getParentDirectory().createDirectory() && xml->writeTo (file);
    return false;
}

juce::ValueTree loadUnit (const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return {};
    const auto doc = juce::ValueTree::fromXml (*xml);
    auto unit = doc.hasType (IDs::PedalCuesUnit) ? doc.getChildWithName (IDs::Unit).createCopy() : juce::ValueTree();
    if (unit.isValid())
        sanitiseCustomUnit (unit);
    return unit;
}

//==============================================================================
static void sanitiseModPreset (juce::ValueTree& p, const modellers::Profile& profile)
{
    setDefault (p, IDs::name, "Preset");
    setDefault (p, IDs::setlist, modellers::defaultSetlist (profile));
    setDefault (p, IDs::presetIndex, 0);
    setDefault (p, IDs::colour, paletteColour (5).toString());
    int scenes = 0;
    for (auto c : p)
        if (c.hasType (IDs::Scene))
            ++scenes;
    for (int i = scenes; i < profile.sceneCount; ++i)
    {
        juce::ValueTree s (IDs::Scene);
        s.setProperty (IDs::name, profile.sceneWord + " " + juce::String (i + 1), nullptr);
        s.setProperty (IDs::colour, paletteColour (i).toString(), nullptr);
        p.appendChild (s, nullptr);
    }
}

static void sanitiseModeller (juce::ValueTree& m, const modellers::Profile& profile)
{
    setDefault (m, IDs::selectedPreset, 0);
    // Its own channel: amp modellers 1, pedals 3 (the units' factory 1 would clash with the amp modeller).
    setDefault (m, IDs::channel, profile.pedal ? 3 : 1);
    for (int i = m.getNumChildren(); --i >= 0;)
        if (! m.getChild (i).hasType (IDs::ModPreset) && ! m.getChild (i).hasType (IDs::ModSwitch))
            m.removeChild (i, nullptr);
    if (m.getChildWithName (IDs::ModPreset) == juce::ValueTree())
        m.addChild (createModPreset (profile.id, "Preset " + modellers::presetLabel (profile, modellers::defaultSetlist (profile), 0),
                                     modellers::defaultSetlist (profile), 0, paletteColour (5)), 0, nullptr);
    for (auto c : m)
        if (c.hasType (IDs::ModPreset))
            sanitiseModPreset (c, profile);
    int switches = 0;
    for (auto c : m)
        if (c.hasType (IDs::ModSwitch))
            ++switches;
    for (int i = switches; i < (int) profile.switches.size(); ++i)
    {
        juce::ValueTree s (IDs::ModSwitch);
        s.setProperty (IDs::name, profile.switches[(size_t) i].name, nullptr);
        m.appendChild (s, nullptr);
    }
}

juce::ValueTree createModPreset (const juce::String& profileId, const juce::String& name, int setlist, int index, juce::Colour colour)
{
    juce::ValueTree p (IDs::ModPreset);
    p.setProperty (IDs::name, name, nullptr);
    p.setProperty (IDs::setlist, setlist, nullptr);
    p.setProperty (IDs::presetIndex, index, nullptr);
    p.setProperty (IDs::colour, colour.toString(), nullptr);
    if (const auto* profile = modellers::find (profileId))
        sanitiseModPreset (p, *profile);
    return p;
}

juce::ValueTree modeller (juce::ValueTree& root, const juce::String& profileId)
{
    auto all = root.getOrCreateChildWithName (IDs::Modellers, nullptr);
    for (auto m : all)
        if (m[IDs::profile].toString() == profileId)
            return m;
    const auto* profile = modellers::find (profileId);
    if (profile == nullptr)
        return {};
    juce::ValueTree m (IDs::Modeller);
    m.setProperty (IDs::profile, profileId, nullptr);
    sanitiseModeller (m, *profile);
    all.appendChild (m, nullptr);
    return m;
}

static void sanitisePerformance (juce::ValueTree& p)
{
    setDefault (p, IDs::name, "New Performance");
    setDefault (p, IDs::number, 1);
    setDefault (p, IDs::colour, paletteColour (5).toString());

    int slots = 0;
    for (auto c : p)
        if (c.hasType (IDs::KemperSlot))
            ++slots;

    for (int i = slots; i < cues::kemper::slotsPerPerformance; ++i)
    {
        juce::ValueTree s (IDs::KemperSlot);
        s.setProperty (IDs::name, "Slot " + juce::String (i + 1), nullptr);
        s.setProperty (IDs::colour, paletteColour (i).toString(), nullptr);
        p.appendChild (s, nullptr);
    }
}

juce::ValueTree createPerformance (const juce::String& name, int number, juce::Colour colour)
{
    juce::ValueTree p (IDs::Performance);
    p.setProperty (IDs::name, name, nullptr);
    p.setProperty (IDs::number, number, nullptr);
    p.setProperty (IDs::colour, colour.toString(), nullptr);
    sanitisePerformance (p);
    return p;
}

juce::ValueTree createPreset (const juce::String& name, int setlist, int bank, int slot, juce::Colour colour)
{
    juce::ValueTree p (IDs::Preset);
    p.setProperty (IDs::name, name, nullptr);
    p.setProperty (IDs::setlist, setlist, nullptr);
    p.setProperty (IDs::bank, bank, nullptr);
    p.setProperty (IDs::slot, slot, nullptr);
    p.setProperty (IDs::colour, colour.toString(), nullptr);
    sanitisePreset (p);
    return p;
}

void sanitise (juce::ValueTree& root)
{
    setDefault (root, IDs::qcChannel, 1);
    setDefault (root, IDs::ampUnit, 0);
    setDefault (root, IDs::selectedCustomUnit, 0);
    setDefault (root, IDs::fxUnit, fxWhammy);
    setDefault (root, IDs::fxCustomUnit, 0);
    setDefault (root, IDs::modellerProfile, juce::String());
    setDefault (root, IDs::fxProfile, juce::String());
    setDefault (root, IDs::fxView, 0);
    setDefault (root, IDs::fxPedal, 0);
    setDefault (root, IDs::fxBeats, 4.0);
    setDefault (root, IDs::fxCurve, 1.0);
    setDefault (root, IDs::fxReset, false);
    setDefault (root, IDs::fxDraw, false);
    setDefault (root, IDs::fxDrawing, cues::whammy::encodeDrawing (cues::whammy::defaultDrawing()));
    setDefault (root, IDs::fxDrawingName, juce::String());
    setDefault (root, IDs::mdLoadFirst, true);
    setDefault (root, IDs::mdSendSetlist, false);   // like the QC: only when the setlist numbers are known to be right
    setDefault (root, IDs::mdSwitchOn, true);
    setDefault (root, IDs::mdView, 0);
    setDefault (root, IDs::mdPedal, 0);
    setDefault (root, IDs::mdBeats, 4.0);
    setDefault (root, IDs::mdCurve, 1.0);
    setDefault (root, IDs::mdReset, false);
    setDefault (root, IDs::mdDraw, false);
    setDefault (root, IDs::mdDrawing, cues::whammy::encodeDrawing (cues::whammy::defaultDrawing()));
    setDefault (root, IDs::mdDrawingName, juce::String());
    setDefault (root, IDs::cuExpressionView, false);
    setDefault (root, IDs::cuBeats, 4.0);
    setDefault (root, IDs::cuCurve, 1.0);
    setDefault (root, IDs::cuReset, false);
    setDefault (root, IDs::cuDraw, false);
    setDefault (root, IDs::cuDrawing, cues::whammy::encodeDrawing (cues::whammy::defaultDrawing()));
    setDefault (root, IDs::cuDrawingName, juce::String());
    setDefault (root, IDs::selectedPerformance, 0);
    setDefault (root, IDs::kemperSlotFirst, true);
    setDefault (root, IDs::kemperEffectOn, true);
    setDefault (root, IDs::kemperKeepTails, true);
    setDefault (root, IDs::kemperPedalsView, false);
    setDefault (root, IDs::kemperPedal, 0);
    setDefault (root, IDs::kpBeats, 4.0);
    setDefault (root, IDs::kpCurve, 1.0);
    setDefault (root, IDs::kpReset, false);
    setDefault (root, IDs::kpDraw, false);
    setDefault (root, IDs::kpDrawing, cues::whammy::encodeDrawing (cues::whammy::defaultDrawing()));
    setDefault (root, IDs::kpDrawingName, juce::String());
    setDefault (root, IDs::whChannel, 2);
    // Before v0.9.0 the Kemper, QC Mini, modeller pages and custom devices on the first tab all shared qcChannel, and both Whammys
    // whChannel: each starts from the channel it was using.
    setDefault (root, IDs::qcMiniChannel, root[IDs::qcChannel]);
    setDefault (root, IDs::kemperChannel, root[IDs::qcChannel]);
    setDefault (root, IDs::kemperPlayerChannel, root[IDs::qcChannel]);
    setDefault (root, IDs::whDtChannel, root[IDs::whChannel]);
    setDefault (root, IDs::whPcBase, 1);
    setDefault (root, IDs::sendSetlist, false);
    setDefault (root, IDs::selectedPreset, 0);
    setDefault (root, IDs::comboPresetScene, true);
    setDefault (root, IDs::stompOn, true);
    setDefault (root, IDs::whModel, 0);
    setDefault (root, IDs::whDropTuneView, false);
    setDefault (root, IDs::whChords, false);
    setDefault (root, IDs::whBypass, false);
    setDefault (root, IDs::whHeelFirst, true);
    setDefault (root, IDs::whMovesEarly, 0);
    setDefault (root, IDs::sweepBeats, 4.0);
    setDefault (root, IDs::sweepCurve, 1.0);
    setDefault (root, IDs::sweepReset, true);
    setDefault (root, IDs::sweepDraw, false);
    setDefault (root, IDs::sweepDrawing, cues::whammy::encodeDrawing (cues::whammy::defaultDrawing()));
    setDefault (root, IDs::qcExpressionView, false);   // Quad Cortex page: Scenes & Stomps, Looper or Expression
    setDefault (root, IDs::qcLooperView, false);
    setDefault (root, IDs::expPedal, 1);
    setDefault (root, IDs::expLoadFirst, false);   // opt-in: a preset reload can cut the sound
    setDefault (root, IDs::expBeats, 4.0);
    setDefault (root, IDs::expCurve, 1.0);
    setDefault (root, IDs::expReset, false);   // a swell usually stays where it ends
    setDefault (root, IDs::expDraw, false);
    setDefault (root, IDs::expDrawing, cues::whammy::encodeDrawing (cues::whammy::defaultDrawing()));
    setDefault (root, IDs::sweepDrawingName, juce::String());   // the saved drawing loaded in the pad, if any
    setDefault (root, IDs::expDrawingName, juce::String());

    root.removeChild (root.getChildWithName (IDs::Drawings), nullptr);   // My drawings live on the computer, not in projects

    auto qc = root.getOrCreateChildWithName (IDs::QC, nullptr);
    for (int i = qc.getNumChildren(); --i >= 0;)
        if (! qc.getChild (i).hasType (IDs::Preset))
            qc.removeChild (i, nullptr);

    if (qc.getNumChildren() == 0)
        qc.appendChild (createPreset ("Preset 1A", 1, 1, 0, paletteColour (5)), nullptr);

    for (auto p : qc)
        sanitisePreset (p);

    // Kemper: performances with five slots each, and the names of its eight effect modules.
    auto kemper = root.getOrCreateChildWithName (IDs::Kemper, nullptr);
    for (int i = kemper.getNumChildren(); --i >= 0;)
        if (! kemper.getChild (i).hasType (IDs::Performance) && ! kemper.getChild (i).hasType (IDs::KemperEffect))
            kemper.removeChild (i, nullptr);
    if (kemper.getChildWithName (IDs::Performance) == juce::ValueTree())
        kemper.addChild (createPerformance ("Performance 1", 1, paletteColour (4)), 0, nullptr);
    for (auto p : kemper)
        if (p.hasType (IDs::Performance))
            sanitisePerformance (p);
    int effects = 0;
    for (auto c : kemper)
        if (c.hasType (IDs::KemperEffect))
            ++effects;
    for (int i = effects; i < cues::kemper::numEffects; ++i)
    {
        juce::ValueTree e (IDs::KemperEffect);
        e.setProperty (IDs::name, cues::kemper::effectName (i), nullptr);
        kemper.appendChild (e, nullptr);
    }

    // Custom units (beta). Picking "custom" with none left falls back to the Quad Cortex.
    auto units = root.getOrCreateChildWithName (IDs::CustomUnits, nullptr);
    for (int i = units.getNumChildren(); --i >= 0;)
        if (! units.getChild (i).hasType (IDs::Unit))
            units.removeChild (i, nullptr);
    for (auto u : units)
        sanitiseCustomUnit (u);
    if ((int) root[IDs::ampUnit] == customAmpUnit && units.getNumChildren() == 0)
        root.setProperty (IDs::ampUnit, 0, nullptr);
    // The Effects & Pedals tab: the Whammy, no pedal, a custom device that exists or a known pedal page (anything else: the Whammy).
    if ((int) root[IDs::fxUnit] == fxNone)
        root.setProperty (IDs::fxUnit, fxWhammy, nullptr);   // v0.9.0 started new setups empty; since v0.9.1 the tab always shows a device
    const auto fx = (int) root[IDs::fxUnit];
    const auto* fxPage = modellers::find (root[IDs::fxProfile].toString());
    if ((fx != fxWhammy && fx != fxNone && fx != fxCustom && fx != fxModeller)
        || (fx == fxCustom && ! juce::isPositiveAndBelow ((int) root[IDs::fxCustomUnit], units.getNumChildren()))
        || (fx == fxModeller && (fxPage == nullptr || ! fxPage->pedal)))
        root.setProperty (IDs::fxUnit, fxWhammy, nullptr);
    // Each device belongs to one tab. Devices from before v0.8.8: the one on the pedals tab is a pedal, the rest amps.
    for (int i = 0; i < units.getNumChildren(); ++i)
    {
        auto u = units.getChild (i);
        if (u[IDs::category].toString() != "amp" && u[IDs::category].toString() != "pedal")
            u.setProperty (IDs::category, ((int) root[IDs::fxUnit] == fxCustom && (int) root[IDs::fxCustomUnit] == i) ? "pedal" : "amp", nullptr);
    }
    if ((int) root[IDs::fxUnit] == fxCustom && (int) root[IDs::ampUnit] == customAmpUnit
        && (int) root[IDs::fxCustomUnit] == (int) root[IDs::selectedCustomUnit])
        root.setProperty (IDs::fxUnit, fxWhammy, nullptr);   // one device can't be on both tabs
    if (auto u = fxCustomUnit (root); u.isValid())
        u.setProperty (IDs::category, "pedal", nullptr);   // the device on each tab belongs to it
    if ((int) root[IDs::ampUnit] == customAmpUnit)
        if (auto u = customUnit (root); u.isValid())
            u.setProperty (IDs::category, "amp", nullptr);

    // Fractal / Line 6 / HeadRush pages: one subtree per model, so switching units keeps each one's presets.
    auto mods = root.getOrCreateChildWithName (IDs::Modellers, nullptr);
    for (int i = mods.getNumChildren(); --i >= 0;)
    {
        auto m = mods.getChild (i);
        const auto* profile = m.hasType (IDs::Modeller) ? modellers::find (m[IDs::profile].toString()) : nullptr;
        if (profile == nullptr)
            mods.removeChild (i, nullptr);
        else
            sanitiseModeller (m, *profile);
    }

    for (auto u : units)
        setDefault (u, IDs::channel, isPedal (u) ? 3 : 1);   // a device without one yet: an amp modeller 1, a pedal 3
    // Once, for state from before v0.9.0: the first tab's custom devices and pages used the shared qcChannel.
    if (! (bool) root[IDs::perDeviceChannels])
    {
        for (auto u : units)
            if (! isPedal (u))
                u.setProperty (IDs::channel, root[IDs::qcChannel], nullptr);
        for (auto m : mods)
            if (const auto* profile = modellers::find (m[IDs::profile].toString()); profile != nullptr && ! profile->pedal)
                m.setProperty (IDs::channel, root[IDs::qcChannel], nullptr);
        root.setProperty (IDs::perDeviceChannels, true, nullptr);
    }
    if ((int) root[IDs::fxUnit] == fxModeller)
        fxModellerData (root);   // created on first use, with its channel
    if ((int) root[IDs::ampUnit] == modellerAmpUnit)
    {
        if (const auto* m = modellers::find (root[IDs::modellerProfile].toString()); m == nullptr || m->pedal)
            root.setProperty (IDs::ampUnit, 0, nullptr);   // unknown model: back to the Quad Cortex
        else
            modeller (root, root[IDs::modellerProfile].toString());
    }
    if (! juce::isPositiveAndNotGreaterThan ((int) root[IDs::ampUnit], qcMiniAmpUnit))
        root.setProperty (IDs::ampUnit, 0, nullptr);   // from a newer PedalCues: show the Quad Cortex

    auto wh = root.getOrCreateChildWithName (IDs::Whammy, nullptr);
    for (int i = wh.getNumChildren(); i < cues::whammy::numEffects; ++i)
    {
        juce::ValueTree e (IDs::Effect);
        e.setProperty (IDs::name, cues::whammy::defaultName (i), nullptr);
        wh.appendChild (e, nullptr);
    }
}

juce::ValueTree createDefault()
{
    juce::ValueTree root (IDs::PedalCues);
    sanitise (root);
    return root;
}

juce::File defaultLibraryFile()
{
   #if JUCE_MAC
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Application Support");
   #else
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #endif
    return base.getChildFile ("PedalCues").getChildFile ("library.xml");
}

static juce::File settingsFile()
{
    return defaultLibraryFile().getSiblingFile ("settings.xml");
}

juce::String getSetting (const juce::String& name)
{
    if (auto xml = juce::XmlDocument::parse (settingsFile()))
        return xml->getStringAttribute (name);
    return {};
}

void setSetting (const juce::String& name, const juce::String& value)
{
    auto xml = juce::XmlDocument::parse (settingsFile());
    if (xml == nullptr)
        xml = std::make_unique<juce::XmlElement> ("PedalCuesSettings");

    xml->setAttribute (name, value);
    settingsFile().getParentDirectory().createDirectory();
    xml->writeTo (settingsFile());
}

bool getFlag (const juce::String& name)
{
    if (auto xml = juce::XmlDocument::parse (settingsFile()))
        return xml->getBoolAttribute (name);
    return false;
}

void setFlag (const juce::String& name, bool value)
{
    setSetting (name, value ? "1" : "0");
}

// What a setup carries besides the names: the MIDI settings and the playing preferences
// (Whammy Chords / Load bypassed / Heel first, return to heel after moves, Expression's Load 1A first).
// Length and curve change per song, so they stay in the project.
static const std::array<const juce::Identifier*, 32>& setupProperties()
{
    static const std::array<const juce::Identifier*, 32> ids { &IDs::ampUnit, &IDs::selectedCustomUnit, &IDs::fxUnit, &IDs::fxCustomUnit, &IDs::modellerProfile,
                                                               &IDs::fxProfile, &IDs::fxReset,
                                                               &IDs::qcMiniChannel, &IDs::kemperChannel, &IDs::kemperPlayerChannel, &IDs::whDtChannel,
                                                               &IDs::perDeviceChannels,
                                                               &IDs::mdLoadFirst, &IDs::mdSendSetlist, &IDs::mdReset, &IDs::cuReset, &IDs::kemperSlotFirst, &IDs::kemperKeepTails,
                                                               &IDs::kpReset, &IDs::qcChannel, &IDs::whChannel, &IDs::whModel, &IDs::whPcBase,
                                                               &IDs::sendSetlist, &IDs::comboPresetScene,
                                                               &IDs::whChords, &IDs::whBypass, &IDs::whHeelFirst, &IDs::whMovesEarly,
                                                               &IDs::sweepReset, &IDs::expReset, &IDs::expLoadFirst };
    return ids;
}

static juce::File& myDrawingsFile()
{
    static juce::File file = defaultLibraryFile().getSiblingFile ("drawings.xml");
    return file;
}

static void tidyDrawings (juce::ValueTree list)
{
    for (int i = list.getNumChildren(); --i >= 0;)
        if (! list.getChild (i).hasType (IDs::Drawing) || list.getChild (i)[IDs::name].toString().isEmpty())
            list.removeChild (i, nullptr);
}

static juce::ValueTree& myDrawingsTree()
{
    static juce::ValueTree list = []
    {
        juce::ValueTree t (IDs::Drawings);
        if (auto xml = juce::XmlDocument::parse (myDrawingsFile()))
            mergeDrawings (t, juce::ValueTree::fromXml (*xml));
        return t;
    }();
    return list;
}

juce::ValueTree myDrawings()
{
    return myDrawingsTree();
}

void storeMyDrawings()
{
    if (auto xml = myDrawingsTree().createXml())
        if (myDrawingsFile().getParentDirectory().createDirectory())
            xml->writeTo (myDrawingsFile());
}

void useMyDrawingsFile (const juce::File& file)
{
    myDrawingsFile() = file;
    auto list = myDrawingsTree();
    list.removeAllChildren (nullptr);
    if (auto xml = juce::XmlDocument::parse (file))
        mergeDrawings (list, juce::ValueTree::fromXml (*xml));
}

juce::ValueTree findDrawing (const juce::ValueTree& list, const juce::String& name)
{
    if (name.isNotEmpty())
        for (auto d : list)
            if (d.hasType (IDs::Drawing) && d[IDs::name].toString() == name)
                return d;
    return {};
}

void saveDrawing (juce::ValueTree list, const juce::String& name, const juce::String& points)
{
    if (name.isEmpty() || ! list.isValid())
        return;

    auto d = findDrawing (list, name);
    if (! d.isValid())
    {
        d = juce::ValueTree (IDs::Drawing);
        d.setProperty (IDs::name, name, nullptr);
        list.appendChild (d, nullptr);
    }
    d.setProperty (IDs::points, points, nullptr);
}

bool renameDrawing (juce::ValueTree list, const juce::String& from, const juce::String& to)
{
    auto d = findDrawing (list, from);
    if (! d.isValid() || to.isEmpty() || (to != from && findDrawing (list, to).isValid()))
        return false;
    d.setProperty (IDs::name, to, nullptr);
    return true;
}

void deleteDrawing (juce::ValueTree list, const juce::String& name)
{
    const auto d = findDrawing (list, name);
    if (d.isValid())
        list.removeChild (d, nullptr);
}

void mergeDrawings (juce::ValueTree into, const juce::ValueTree& from)
{
    for (const auto d : from)
        if (d.hasType (IDs::Drawing))
            saveDrawing (into, d[IDs::name].toString(), d[IDs::points].toString());
    tidyDrawings (into);
}

bool saveLibrary (const juce::ValueTree& root, const juce::File& file, const juce::ValueTree* drawings)
{
    juce::ValueTree lib (IDs::PedalCues);
    lib.appendChild (root.getChildWithName (IDs::QC).createCopy(), nullptr);
    lib.appendChild (root.getChildWithName (IDs::Whammy).createCopy(), nullptr);
    lib.appendChild (root.getChildWithName (IDs::Kemper).createCopy(), nullptr);
    lib.appendChild (root.getChildWithName (IDs::CustomUnits).createCopy(), nullptr);
    lib.appendChild (root.getChildWithName (IDs::Modellers).createCopy(), nullptr);
    if (drawings != nullptr)
        lib.appendChild (drawings->createCopy(), nullptr);

    // The MIDI setup travels with the names, so a default/exported setup is ready to use.
    for (const auto* id : setupProperties())
        if (root.hasProperty (*id))
            lib.setProperty (*id, root[*id], nullptr);
    if (root.hasProperty (IDs::setupViaQcChain))
        lib.setProperty (IDs::setupViaQcChain, root[IDs::setupViaQcChain], nullptr);

    if (auto xml = lib.createXml())
        return file.getParentDirectory().createDirectory() && xml->writeTo (file);

    return false;
}

bool loadLibrary (juce::ValueTree& root, const juce::File& file, juce::ValueTree* drawingsInto)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return false;

    const auto lib = juce::ValueTree::fromXml (*xml);
    if (! lib.hasType (IDs::PedalCues))
        return false;

    if (drawingsInto != nullptr)
        mergeDrawings (*drawingsInto, lib.getChildWithName (IDs::Drawings));

    for (const auto* id : { &IDs::QC, &IDs::Whammy, &IDs::Kemper, &IDs::CustomUnits, &IDs::Modellers })
    {
        const auto src = lib.getChildWithName (*id);
        if (! src.isValid())
            continue;

        root.removeChild (root.getChildWithName (*id), nullptr);
        root.appendChild (src.createCopy(), nullptr);
    }

    if (! lib.hasProperty (IDs::fxUnit))
        root.setProperty (IDs::fxUnit, fxWhammy, nullptr);   // a setup from before the Effects & Pedals tab: the Whammy
    // A setup from before v0.9.0 has one shared amp channel: sanitise gives every device that one again.
    if (! lib.hasProperty (IDs::perDeviceChannels))
        for (const auto* id : { &IDs::perDeviceChannels, &IDs::qcMiniChannel, &IDs::kemperChannel, &IDs::kemperPlayerChannel, &IDs::whDtChannel })
            root.removeProperty (*id, nullptr);
    for (const auto* id : setupProperties())
        if (lib.hasProperty (*id))
            root.setProperty (*id, lib[*id], nullptr);
    if (lib.hasProperty (IDs::setupViaQcChain))
        root.setProperty (IDs::setupViaQcChain, lib[IDs::setupViaQcChain], nullptr);   // the caller applies and removes it

    root.setProperty (IDs::selectedPreset, 0, nullptr);
    root.setProperty (IDs::selectedPerformance, 0, nullptr);
    sanitise (root);
    return true;
}

} // namespace state
