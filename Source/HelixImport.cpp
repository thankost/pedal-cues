#include "HelixImport.h"
#include "State.h"

namespace helix
{
namespace
{
constexpr juce::int64 maxFileSize = 64 * 1024 * 1024;       // a full Helix bundle is a few MB
constexpr juce::int64 maxDecompressed = 64 * 1024 * 1024;   // guards against a zip bomb in encoded_data

const juce::var& member (const juce::var& v, const char* name)
{
    static const juce::var none;
    if (auto* o = v.getDynamicObject())
        if (o->hasProperty (name))
            return o->getProperty (name);
    return none;
}

juce::String text (const juce::var& v)
{
    return v.isString() ? v.toString().trim() : juce::String();
}

int number (const juce::var& v, int fallback)
{
    if (v.isInt() || v.isInt64() || v.isDouble())
        return (int) v;
    return fallback;
}

// base64 (zlib (JSON)) -> JSON text; empty on any failure.
juce::String decode (const juce::String& encoded, juce::String& error)
{
    juce::String clean;
    clean.preallocateBytes ((size_t) encoded.length());
    for (auto c : encoded)
        if (! juce::CharacterFunctions::isWhitespace (c))
            clean << juce::String::charToString (c);

    juce::MemoryOutputStream compressed;
    if (clean.isEmpty() || ! juce::Base64::convertFromBase64 (compressed, clean))
    {
        error = "The file's data isn't readable (not valid base64).";
        return {};
    }

    auto* in = new juce::MemoryInputStream (compressed.getData(), compressed.getDataSize(), false);
    juce::GZIPDecompressorInputStream unzip (in, true, juce::GZIPDecompressorInputStream::zlibFormat);
    juce::MemoryOutputStream out;
    char buffer[16384];
    for (;;)
    {
        const auto n = unzip.read (buffer, (int) sizeof (buffer));
        if (n <= 0)
            break;
        out.write (buffer, (size_t) n);
        if ((juce::int64) out.getDataSize() > maxDecompressed)
        {
            error = "The file's data is too large to be an HX Edit export.";
            return {};
        }
    }
    if (out.getDataSize() == 0)
    {
        error = "The file's data isn't readable (it doesn't unpack).";
        return {};
    }
    return out.toUTF8();
}

bool hasBlocks (const juce::var& tone)
{
    if (auto* o = tone.getDynamicObject())
        for (const auto& dsp : o->getProperties())
            if (dsp.name.toString().startsWith ("dsp"))
                if (auto* d = dsp.value.getDynamicObject())
                    for (const auto& block : d->getProperties())
                        if (block.name.toString().startsWith ("block"))
                            return true;
    return false;
}

// One preset object: { meta, tone } or { data: { meta, tone } }.
Preset readPreset (const juce::var& v, int slot)
{
    const auto& data = member (v, "data").isObject() ? member (v, "data") : v;
    Preset p;
    p.slot = slot;
    p.name = text (member (member (data, "meta"), "name"));
    const auto& tone = member (data, "tone");
    for (int i = 0; i < 8; ++i)
    {
        const auto& s = member (tone, ("snapshot" + juce::String (i)).toRawUTF8());
        if (! s.isObject())
            break;
        p.snapshots.push_back ({ text (member (s, "@name")), number (member (s, "@ledcolor"), -1) });
    }
    // HX Edit fills unused slots with "New Preset" and an empty signal path. Not confirmed by any source: a guess the
    // dialog's "Skip empty presets" switch can turn off.
    p.empty = p.name.isEmpty() || (p.name == "New Preset" && ! hasBlocks (tone));
    return p;
}

std::vector<Preset> readPresets (const juce::var& list)
{
    std::vector<Preset> presets;
    if (auto* a = list.getArray())
        for (int i = 0; i < a->size(); ++i)
            if (a->getReference (i).isObject())   // a null entry keeps its slot number for the ones after it
                presets.push_back (readPreset (a->getReference (i), i));
    return presets;
}

Result fail (const juce::String& error)
{
    Result r;
    r.error = error;
    return r;
}
} // namespace

Result readJson (const juce::String& json)
{
    juce::var root;
    if (json.isEmpty() || juce::JSON::parse (json, root).failed() || ! (root.isObject() || root.isArray()))
        return fail ("This isn't an HX Edit file (.hlx, .hls or .hlb).");

    Result r;
    auto payload = root;
    const auto wrapperName = text (member (member (root, "meta"), "name"));

    if (member (root, "encoded_data").isString())
    {
        juce::String error;
        const auto decoded = decode (member (root, "encoded_data").toString(), error);
        if (decoded.isEmpty())
            return fail (error);
        if (juce::JSON::parse (decoded, payload).failed())
            return fail ("The file's data isn't readable (not JSON inside).");
    }

    if (payload.isArray())                                       // .hls: the setlist's presets
    {
        r.kind = "setlist";
        r.setlists.push_back ({ wrapperName, readPresets (payload) });
    }
    else if (member (payload, "setlists").isArray())             // .hlb: every setlist
    {
        r.kind = "bundle";
        for (const auto& s : *member (payload, "setlists").getArray())
            if (s.isObject())
                r.setlists.push_back ({ text (member (member (s, "meta"), "name")), readPresets (member (s, "presets")) });
    }
    else if (member (payload, "presets").isArray())              // a setlist as an object
    {
        r.kind = "setlist";
        auto name = text (member (member (payload, "meta"), "name"));
        r.setlists.push_back ({ name.isNotEmpty() ? name : wrapperName, readPresets (member (payload, "presets")) });
    }
    else if (member (member (payload, "data"), "tone").isObject() || member (member (payload, "data"), "meta").isObject())   // .hlx
    {
        r.kind = "preset";
        r.setlists.push_back ({ {}, { readPreset (payload, 0) } });
    }
    else
    {
        return fail ("This isn't an HX Edit file (.hlx, .hls or .hlb).");
    }

    int presets = 0;
    for (const auto& s : r.setlists)
        presets += (int) s.presets.size();
    if (presets == 0)
        return fail ("The file has no presets in it.");
    return r;
}

Result readFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return fail ("The file isn't there any more.");
    if (file.getSize() > maxFileSize)
        return fail ("The file is too large to be an HX Edit export.");
    auto r = readJson (file.loadFileAsString());
    if (r.ok())
    {
        // A preset file without a name: use the file's.
        if (r.kind == "preset" && r.setlists.front().presets.front().name.isEmpty())
            r.setlists.front().presets.front().name = file.getFileNameWithoutExtension();
        if (r.kind == "setlist" && r.setlists.front().name.isEmpty())
            r.setlists.front().name = file.getFileNameWithoutExtension();
    }
    return r;
}

juce::Colour snapshotColour (int ledColour)
{
    // Our tile palette where it has the colour (State.cpp), so imported tiles match hand-made ones.
    switch (ledColour)
    {
        case 1:  return juce::Colour (0xffe8e8e8);   // White
        case 2:  return juce::Colour (0xffe5484d);   // Red
        case 3:  return juce::Colour (0xfff76b15);   // Dark orange
        case 4:  return juce::Colour (0xffffa057);   // Light orange
        case 5:  return juce::Colour (0xffffc53d);   // Yellow
        case 6:  return juce::Colour (0xff46a758);   // Green
        case 7:  return juce::Colour (0xff12a594);   // Turquoise
        case 8:  return juce::Colour (0xff0090ff);   // Blue
        case 9:  return juce::Colour (0xff8e4ec6);   // Violet
        case 10: return juce::Colour (0xffd6409f);   // Pink
        case 11: return juce::Colour (0xff6f6f6f);   // Off
        default: return {};                          // 0 Auto, missing, unknown
    }
}

bool supports (const modellers::Profile& p)
{
    static const juce::StringArray ids { "line6.helix-floor", "line6.helix-lt", "line6.helix-rack",
                                         "line6.hx-stomp", "line6.hx-stomp-xl", "line6.hx-effects" };
    return ids.contains (p.id);
}

int apply (juce::ValueTree m, const modellers::Profile& profile, const Result& result, int setlistIndexInFile, int targetSetlist, bool skipEmpty)
{
    if (! m.isValid() || ! result.ok() || ! juce::isPositiveAndBelow (setlistIndexInFile, (int) result.setlists.size()))
        return 0;
    if (! modellers::hasSetlists (profile))
        targetSetlist = -1;
    else if (! juce::isPositiveAndBelow (targetSetlist, modellers::setlistNames (profile).size()))
        return 0;

    const auto& palette = state::palette();
    const auto paletteAt = [&palette] (int i) { return juce::Colour (palette.getReference (((i % palette.size()) + palette.size()) % palette.size()).argb); };
    const auto limit = modellers::presetsPerSetlist (profile, targetSetlist);
    const auto single = result.kind == "preset";

    std::vector<juce::ValueTree> made;
    for (const auto& p : result.setlists[(size_t) setlistIndexInFile].presets)
    {
        if ((skipEmpty && p.empty) || ! juce::isPositiveAndBelow (p.slot, limit))
            continue;
        const auto first = p.snapshots.empty() ? juce::Colour() : snapshotColour (p.snapshots.front().ledColour);
        const auto name = p.name.isNotEmpty() ? p.name : "Preset " + modellers::presetLabel (profile, targetSetlist, p.slot);
        auto node = state::createModPreset (profile.id, name, targetSetlist, p.slot,
                                            first.isTransparent() ? paletteAt (p.slot) : first);
        int s = 0;
        for (auto scene : node)
        {
            if (! scene.hasType (IDs::Scene))
                continue;
            if (s >= profile.sceneCount || s >= (int) p.snapshots.size())
                break;
            const auto& snap = p.snapshots[(size_t) s];
            if (snap.name.isNotEmpty())
                scene.setProperty (IDs::name, snap.name, nullptr);
            const auto c = snapshotColour (snap.ledColour);
            if (! c.isTransparent())
                scene.setProperty (IDs::colour, c.toString(), nullptr);
            ++s;
        }
        made.push_back (node);
    }
    if (made.empty())
        return 0;

    // Remove what the file replaces, remembering where it was.
    int insertAt = -1, lastPreset = -1;
    for (int i = m.getNumChildren(); --i >= 0;)
    {
        const auto c = m.getChild (i);
        if (! c.hasType (IDs::ModPreset))
            continue;
        const auto sameSetlist = ! modellers::hasSetlists (profile) || (int) c[IDs::setlist] == targetSetlist;
        const auto replaced = sameSetlist && (! single || (int) c[IDs::presetIndex] == (int) made.front()[IDs::presetIndex]);
        if (replaced)
        {
            m.removeChild (i, nullptr);
            insertAt = i;
            if (lastPreset >= 0)
                --lastPreset;
        }
        else if (lastPreset < 0)
        {
            lastPreset = i;
        }
    }
    if (insertAt < 0)
        insertAt = lastPreset + 1;   // after the last preset (0 when there is none)

    for (size_t i = 0; i < made.size(); ++i)
        m.addChild (made[i], insertAt + (int) i, nullptr);

    int presetNumber = 0;   // selectedPreset counts ModPresets only
    for (int i = 0; i < insertAt; ++i)
        presetNumber += m.getChild (i).hasType (IDs::ModPreset) ? 1 : 0;
    m.setProperty (IDs::selectedPreset, presetNumber, nullptr);
    return (int) made.size();
}
}
