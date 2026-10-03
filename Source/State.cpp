#include "State.h"
#include "CueModel.h"

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
    setDefault (root, IDs::sweepBeats, 4.0);
    setDefault (root, IDs::sweepCurve, 1.0);
    setDefault (root, IDs::sweepReset, true);
    setDefault (root, IDs::sweepDraw, false);
    setDefault (root, IDs::sweepDrawing, cues::whammy::encodeDrawing (cues::whammy::defaultDrawing()));
    setDefault (root, IDs::qcExpressionView, false);   // Quad Cortex page: Scenes & Stomps or Expression
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
static const std::array<const juce::Identifier*, 16>& setupProperties()
{
    static const std::array<const juce::Identifier*, 16> ids { &IDs::ampUnit, &IDs::kemperSlotFirst, &IDs::kemperKeepTails,
                                                               &IDs::kpReset, &IDs::qcChannel, &IDs::whChannel, &IDs::whModel, &IDs::whPcBase,
                                                               &IDs::sendSetlist, &IDs::comboPresetScene,
                                                               &IDs::whChords, &IDs::whBypass, &IDs::whHeelFirst,
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

    for (const auto* id : { &IDs::QC, &IDs::Whammy, &IDs::Kemper })
    {
        const auto src = lib.getChildWithName (*id);
        if (! src.isValid())
            continue;

        root.removeChild (root.getChildWithName (*id), nullptr);
        root.appendChild (src.createCopy(), nullptr);
    }

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
