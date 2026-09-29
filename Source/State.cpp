#include "State.h"
#include "CueModel.h"

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
    setDefault (root, IDs::whChannel, 2);
    setDefault (root, IDs::whPcBase, 1);
    setDefault (root, IDs::sendSetlist, false);
    setDefault (root, IDs::selectedPreset, 0);
    setDefault (root, IDs::comboPresetScene, false);
    setDefault (root, IDs::stompOn, true);
    setDefault (root, IDs::whChords, false);
    setDefault (root, IDs::whBypass, false);
    setDefault (root, IDs::whHeelFirst, true);
    setDefault (root, IDs::sweepBeats, 4.0);
    setDefault (root, IDs::sweepCurve, 1.0);
    setDefault (root, IDs::sweepReset, true);

    auto qc = root.getOrCreateChildWithName (IDs::QC, nullptr);
    for (int i = qc.getNumChildren(); --i >= 0;)
        if (! qc.getChild (i).hasType (IDs::Preset))
            qc.removeChild (i, nullptr);

    if (qc.getNumChildren() == 0)
        qc.appendChild (createPreset ("Preset 1A", 1, 1, 0, paletteColour (5)), nullptr);

    for (auto p : qc)
        sanitisePreset (p);

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

bool saveLibrary (const juce::ValueTree& root, const juce::File& file)
{
    juce::ValueTree lib (IDs::PedalCues);
    lib.appendChild (root.getChildWithName (IDs::QC).createCopy(), nullptr);
    lib.appendChild (root.getChildWithName (IDs::Whammy).createCopy(), nullptr);

    if (auto xml = lib.createXml())
        return file.getParentDirectory().createDirectory() && xml->writeTo (file);

    return false;
}

bool loadLibrary (juce::ValueTree& root, const juce::File& file)
{
    const auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr)
        return false;

    const auto lib = juce::ValueTree::fromXml (*xml);
    if (! lib.hasType (IDs::PedalCues))
        return false;

    for (const auto* id : { &IDs::QC, &IDs::Whammy })
    {
        const auto src = lib.getChildWithName (*id);
        if (! src.isValid())
            continue;

        root.removeChild (root.getChildWithName (*id), nullptr);
        root.appendChild (src.createCopy(), nullptr);
    }

    root.setProperty (IDs::selectedPreset, 0, nullptr);
    sanitise (root);
    return true;
}

} // namespace state
