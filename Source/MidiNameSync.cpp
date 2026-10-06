#include "MidiNameSync.h"

#include "CueModel.h"
#include "DeviceTemplates.h"
#include "Modellers.h"
#include "State.h"

#include <set>

namespace namesync
{
namespace
{
juce::MidiMessage sysex (const std::vector<juce::uint8>& fromF0ToF7)
{
    // createSysExMessage takes the payload without F0 / F7.
    return juce::MidiMessage::createSysExMessage (fromF0ToF7.data() + 1, (int) fromF0ToF7.size() - 2);
}

juce::String asciiName (const juce::uint8* p, int n)
{
    juce::String s;
    for (int i = 0; i < n; ++i)
        s += (p[i] >= 32 && p[i] <= 126) ? (juce::juce_wchar) p[i] : (juce::juce_wchar) ' ';
    return s.trimEnd();
}

bool isRolandModel (int m)
{
    return m == roland::gt1000Model || m == roland::dd500Model || m == roland::md500Model || m == roland::rv500Model;
}

bool isFractalModel (int m)
{
    return m == fractal::axeFx3Model || m == fractal::fm3Model || m == fractal::fm9Model || m == fractal::vp4Model;
}

const juce::uint8 strymonHeader[] = { 0xF0, 0x00, 0x01, 0x55, 0x12 };
const juce::uint8 fractalHeader[] = { 0xF0, 0x00, 0x01, 0x74 };

bool startsWith (const juce::uint8* raw, int size, const juce::uint8* header, int n)
{
    return size >= n && std::equal (header, header + n, raw);
}
}

//==============================================================================
std::optional<Target> targetFor (const juce::String& id)
{
    Target t;
    t.id = id;
    if (id == "boss.gt-1000")
    {
        t.protocol = Protocol::bossGt1000; t.deviceName = "GT-1000"; t.model = roland::gt1000Model;
        t.presetCount = roland::gt1000UserPatches; t.customDevice = true;
        return t;
    }
    if (id == "boss.dd-500" || id == "boss.md-500" || id == "boss.rv-500")
    {
        t.protocol = Protocol::boss500;
        t.model = id == "boss.dd-500" ? roland::dd500Model : id == "boss.md-500" ? roland::md500Model : roland::rv500Model;
        t.deviceName = id == "boss.dd-500" ? "DD-500" : id == "boss.md-500" ? "MD-500" : "RV-500";
        t.presetCount = roland::boss500Patches;
        return t;
    }
    struct F { const char* id; const char* name; int model, presets, scenes; };
    for (const auto& f : { F { "fractal.axe-fx-3", "Axe-Fx III", fractal::axeFx3Model, 1024, 8 },   // Mk I: 512, the rest time out
                           F { "fractal.fm9", "FM9", fractal::fm9Model, 512, 8 },
                           F { "fractal.fm3", "FM3", fractal::fm3Model, 512, 8 },
                           F { "fractal.vp4", "VP4", fractal::vp4Model, 104, 4 } })
        if (id == f.id)
        {
            t.protocol = Protocol::fractalGen3; t.deviceName = f.name; t.model = f.model; t.presetCount = f.presets;
            t.customDevice = true; t.sceneNames = true; t.sceneCount = f.scenes;
            return t;
        }
    struct S { const char* id; const char* name; int product, presets; };
    for (const auto& s : { S { "strymon.timeline", "TimeLine", strymon::timeLine, 200 },
                           S { "strymon.mobius", "Mobius", strymon::mobius, 200 },
                           S { "strymon.bigsky", "BigSky", strymon::bigSky, 300 } })
        if (id == s.id)
        {
            t.protocol = Protocol::strymonGen1; t.deviceName = s.name; t.model = s.product; t.presetCount = s.presets;
            return t;
        }
    return std::nullopt;
}

//==============================================================================
// Allowed requests. These builders are the ONLY messages the name sync sends (isAllowedRequest checks them again before
// sending). Do not add anything here that sets, writes, saves or loads.
namespace roland
{
int linear (juce::uint8 a, juce::uint8 b, juce::uint8 c, juce::uint8 d)
{
    return ((a & 0x7F) << 21) | ((b & 0x7F) << 14) | ((c & 0x7F) << 7) | (d & 0x7F);
}

std::array<juce::uint8, 4> bytes (int v)
{
    return { (juce::uint8) ((v >> 21) & 0x7F), (juce::uint8) ((v >> 14) & 0x7F), (juce::uint8) ((v >> 7) & 0x7F), (juce::uint8) (v & 0x7F) };
}

juce::uint8 checksum (const juce::uint8* data, int size)
{
    int sum = 0;
    for (int i = 0; i < size; ++i)
        sum += data[i];
    return (juce::uint8) ((128 - sum % 128) % 128);
}

juce::MidiMessage requestData (int model, int deviceId, int address, int size)
{
    std::vector<juce::uint8> m { 0xF0, manufacturer, (juce::uint8) (deviceId & 0x7F), 0x00, 0x00, 0x00, (juce::uint8) (model & 0x7F), rq1 };
    const auto a = bytes (address), s = bytes (size);
    m.insert (m.end(), a.begin(), a.end());
    m.insert (m.end(), s.begin(), s.end());
    m.push_back (checksum (m.data() + 8, 8));
    m.push_back (0xF7);
    return sysex (m);
}

std::optional<Data> parseData (const juce::uint8* raw, int size)
{
    // F0 41 dev 00 00 00 model 12 a a a a <at least one data byte> sum F7
    if (size < 15 || raw[0] != 0xF0 || raw[1] != manufacturer || raw[3] != 0 || raw[4] != 0 || raw[5] != 0 || raw[7] != dt1
        || raw[size - 1] != 0xF7)
        return std::nullopt;
    Data d;
    d.deviceId = raw[2];
    d.model = raw[6];
    d.address = linear (raw[8], raw[9], raw[10], raw[11]);
    d.data.assign (raw + 12, raw + size - 2);
    d.checksumOk = checksum (raw + 8, size - 10) == raw[size - 2];
    return d;
}

int gt1000PatchNameAddress (int patch)
{
    // "patch 1 (user patch)" at 20 00 00 00 ... patch 250 at 21 79 00 00: the second byte counts patches (7-bit carry into the
    // first). Patch Name1-16 = offsets 00 00 - 00 0F of [PatchCommon] at the patch's start.
    return linear (0x20, 0, 0, 0) + juce::jlimit (0, gt1000UserPatches - 1, patch) * (1 << 14);
}

int gt1000ProgramMapAddress (int bank, int pc)
{
    // [PcmapPc] BANK1 00 10 00 00, BANK2 00 10 04 00, BANK3 00 10 08 00, BANK4 00 10 0C 00; 4 bytes per PC.
    return linear (0x00, 0x10, 0, 0) + juce::jlimit (0, 3, bank) * 4 * 128 + juce::jlimit (0, 127, pc) * gt1000ProgramMapEntrySize;
}

juce::String gt1000PatchLabel (int patch)
{
    return "U" + juce::String (patch / 5 + 1).paddedLeft ('0', 2) + "-" + juce::String (patch % 5 + 1);
}

int boss500PatchNameAddress (int index)
{
    // BANK N at 30 00 00 00 + N * 4 in the second byte (N = 1-99, bank 0 is the temporary one); PATCH A / B / C at 00 10 00 /
    // 00 20 00 / 00 30 00 inside it; Patch Name1-16 at the patch's offsets 00 00 - 00 0F.
    index = juce::jlimit (0, boss500Patches - 1, index);
    const auto bank = index / 3 + 1, patch = index % 3 + 1;
    return linear (0x30, 0, 0, 0) + bank * 4 * (1 << 14) + patch * 0x10 * (1 << 7);
}
}

namespace fractal
{
juce::uint8 checksum (const juce::uint8* fromF0, int size)
{
    juce::uint8 x = 0;
    for (int i = 0; i < size; ++i)
        x ^= fromF0[i];
    return (juce::uint8) (x & 0x7F);
}

static juce::MidiMessage build (int model, juce::uint8 function, std::vector<juce::uint8> data)
{
    std::vector<juce::uint8> m (std::begin (fractalHeader), std::end (fractalHeader));
    m.push_back ((juce::uint8) (model & 0x7F));
    m.push_back (function);
    m.insert (m.end(), data.begin(), data.end());
    m.push_back (checksum (m.data(), (int) m.size()));
    m.push_back (0xF7);
    return sysex (m);
}

juce::MidiMessage queryPatchName (int model, int preset)
{
    if (preset < 0)
        return build (model, queryPatchNameFunction, { 0x7F, 0x7F });
    return build (model, queryPatchNameFunction, { (juce::uint8) (preset & 0x7F), (juce::uint8) ((preset >> 7) & 0x7F) });   // LS first
}

juce::MidiMessage querySceneName (int model, int scene)
{
    return build (model, querySceneNameFunction, { (juce::uint8) (scene < 0 ? 0x7F : scene & 0x07) });
}

std::optional<Name> parseName (const juce::uint8* raw, int size, int model)
{
    // Patch: F0 00 01 74 mm 0D nn nn <32 chars> cs F7. Scene: F0 00 01 74 mm 0E nn <32 chars> cs F7.
    if (! startsWith (raw, size, fractalHeader, 4) || size < 9 || raw[4] != model || raw[size - 1] != 0xF7)
        return std::nullopt;
    Name n;
    int nameStart = 0;
    if (raw[5] == queryPatchNameFunction)
    {
        nameStart = 8;
        n.number = raw[6] | (raw[7] << 7);
    }
    else if (raw[5] == querySceneNameFunction)
    {
        nameStart = 7;
        n.scene = true;
        n.number = raw[6];
    }
    else
    {
        return std::nullopt;
    }
    const auto nameBytes = size - 2 - nameStart;
    if (nameBytes <= 0)
        return std::nullopt;
    n.name = asciiName (raw + nameStart, juce::jmin (nameLength, nameBytes));
    n.checksumOk = checksum (raw, size - 2) == raw[size - 2];
    return n;
}
}

namespace strymon
{
juce::MidiMessage requestPreset (int product, int preset)
{
    std::vector<juce::uint8> m (std::begin (strymonHeader), std::end (strymonHeader));
    m.push_back ((juce::uint8) (product & 0x7F));
    m.push_back (requestPresetOpcode);
    m.push_back ((juce::uint8) ((preset >> 7) & 0x7F));
    m.push_back ((juce::uint8) (preset & 0x7F));
    m.push_back (0xF7);
    return sysex (m);
}

std::optional<Preset> parsePreset (const juce::uint8* raw, int size, int product)
{
    if (size != replySize || ! startsWith (raw, size, strymonHeader, 5) || raw[5] != product
        || (raw[6] != presetDumpOpcode && raw[6] != fastFetchOpcode) || raw[size - 1] != 0xF7)
        return std::nullopt;
    Preset p;
    p.number = (raw[numberOffset] << 7) | raw[numberOffset + 1];
    p.name = asciiName (raw + nameOffset, nameLength);
    return p;
}

bool isNak (const juce::uint8* raw, int size, int product)
{
    return size >= 8 && size <= 12 && startsWith (raw, size, strymonHeader, 5) && raw[5] == product
           && raw[size - 2] == nakOpcode && raw[size - 1] == 0xF7;
}
}

//==============================================================================
bool isAllowedRequest (const juce::MidiMessage& msg)
{
    if (! msg.isSysEx())
        return false;
    const auto* raw = msg.getRawData();
    const auto size = msg.getRawDataSize();
    if (size < 2 || raw[0] != 0xF0 || raw[size - 1] != 0xF7)
        return false;

    // Roland RQ1: exactly 18 bytes, command 11H, a model we read, a valid device ID and checksum.
    if (raw[1] == roland::manufacturer)
        return size == 18 && (raw[2] <= 0x1F || raw[2] == 0x7F) && raw[3] == 0 && raw[4] == 0 && raw[5] == 0
               && isRolandModel (raw[6]) && raw[7] == roland::rq1 && roland::checksum (raw + 8, 8) == raw[16];

    // Fractal: QUERY PATCH NAME (10 bytes) or QUERY SCENE NAME (9 bytes) only.
    if (startsWith (raw, size, fractalHeader, 4))
        return size >= 9 && isFractalModel (raw[4])
               && ((raw[5] == fractal::queryPatchNameFunction && size == 10) || (raw[5] == fractal::querySceneNameFunction && size == 9))
               && fractal::checksum (raw, size - 2) == raw[size - 2];

    // Strymon gen 1: request single preset (63) or its fast-fetch form (67) only.
    if (startsWith (raw, size, strymonHeader, 5))
        return size == 10 && raw[5] >= strymon::timeLine && raw[5] <= strymon::bigSky
               && (raw[6] == strymon::requestPresetOpcode || raw[6] == strymon::fastFetchOpcode);

    return false;
}

//==============================================================================
Reader::Reader (Target t, bool withSceneNames) : tgt (std::move (t))
{
    switch (tgt.protocol)
    {
        case Protocol::bossGt1000:
            for (int i = 0; i < tgt.presetCount; ++i)
                requests.push_back ({ Kind::presetName, i, roland::gt1000PatchNameAddress (i), roland::nameLength });
            // The PROGRAM MAP, 32 PCs (128 bytes) per request: which patch each BANK1-4 PC#1-128 loads.
            for (int bank = 0; bank < 4; ++bank)
                for (int chunk = 0; chunk < 4; ++chunk)
                    requests.push_back ({ Kind::programMap, bank * 128 + chunk * 32, roland::gt1000ProgramMapAddress (bank, chunk * 32),
                                          32 * roland::gt1000ProgramMapEntrySize });
            break;
        case Protocol::boss500:
            for (int i = 0; i < tgt.presetCount; ++i)
                requests.push_back ({ Kind::presetName, i, roland::boss500PatchNameAddress (i), roland::nameLength });
            break;
        case Protocol::fractalGen3:
            for (int i = 0; i < tgt.presetCount; ++i)
                requests.push_back ({ Kind::presetName, i });
            if (withSceneNames && tgt.sceneNames)
                for (int i = 0; i < juce::jmin (8, tgt.sceneCount); ++i)
                    requests.push_back ({ Kind::sceneName, i });
            break;
        case Protocol::strymonGen1:
            for (int i = 0; i < tgt.presetCount; ++i)
                requests.push_back ({ Kind::presetName, i });
            break;
    }
}

bool Reader::finished() const { return stopped || position >= requests.size(); }

int Reader::timeoutMs() const
{
    // A Strymon dump is 650 bytes: about 210 ms on a 5-pin cable before the pedal's own delay.
    return tgt.protocol == Protocol::strymonGen1 ? 1200 : 600;
}

juce::MidiMessage Reader::current() const
{
    if (finished())
        return {};
    const auto& r = requests[position];
    switch (tgt.protocol)
    {
        case Protocol::bossGt1000:
        case Protocol::boss500:    return roland::requestData (tgt.model, rolandDevice, r.address, r.size);
        case Protocol::fractalGen3: return r.kind == Kind::sceneName ? fractal::querySceneName (tgt.model, r.index)
                                                                     : fractal::queryPatchName (tgt.model, r.index);
        case Protocol::strymonGen1: return strymon::requestPreset (tgt.model, r.index);
    }
    return {};
}

void Reader::advance()
{
    ++position;
}

bool Reader::collectRoland (const roland::Data& d)
{
    const auto& r = requests[position];
    const auto end = d.address + (int) d.data.size();
    if (d.model != tgt.model || end <= r.address || d.address >= r.address + r.size)
        return false;   // not (part of) what we asked for
    if (! d.checksumOk)
        ++res.checksumMismatches;
    for (size_t i = 0; i < d.data.size(); ++i)
        memory[d.address + (int) i] = d.data[i];
    for (int a = r.address; a < r.address + r.size; ++a)
        if (memory.count (a) == 0)
            return false;   // more to come

    std::vector<juce::uint8> got;
    for (int a = r.address; a < r.address + r.size; ++a)
        got.push_back (memory[a]);
    if (r.kind == Kind::presetName)
    {
        res.presetNames[r.index] = asciiName (got.data(), (int) got.size());
    }
    else
    {
        for (int i = 0; i + 3 < (int) got.size(); i += 4)
            res.programMap[r.index + i / 4] = ((got[(size_t) i] & 0x0F) << 12) | ((got[(size_t) i + 1] & 0x0F) << 8)
                                              | ((got[(size_t) i + 2] & 0x0F) << 4) | (got[(size_t) i + 3] & 0x0F);
    }
    return true;
}

bool Reader::accept (const juce::uint8* raw, int size)
{
    if (finished() || raw == nullptr || size < 2)
        return false;
    const auto& r = requests[position];
    bool answeredNow = false;

    switch (tgt.protocol)
    {
        case Protocol::bossGt1000:
        case Protocol::boss500:
            if (auto d = roland::parseData (raw, size))
                answeredNow = collectRoland (*d);
            break;
        case Protocol::fractalGen3:
            if (auto n = fractal::parseName (raw, size, tgt.model))
            {
                if (n->scene != (r.kind == Kind::sceneName))
                    break;
                if (! n->scene && n->number != r.index && missed.count (n->number) > 0)
                {
                    res.presetNames[n->number] = n->name;   // a late answer to an earlier request
                    missed.erase (n->number);
                    break;
                }
                if (n->number != r.index)
                    ++res.numberMismatches;
                if (! n->checksumOk)
                    ++res.checksumMismatches;
                if (n->scene)
                {
                    res.sceneNames[(size_t) juce::jlimit (0, 7, r.index)] = n->name;
                    res.hasSceneNames = true;
                }
                else
                {
                    res.presetNames[r.index] = n->name;
                }
                answeredNow = true;
            }
            break;
        case Protocol::strymonGen1:
            if (auto p = strymon::parsePreset (raw, size, tgt.model))
            {
                if (p->number != r.index && missed.count (p->number) > 0)
                {
                    res.presetNames[p->number] = p->name;   // a late answer to an earlier request
                    missed.erase (p->number);
                    break;
                }
                if (p->number != r.index)
                    ++res.numberMismatches;
                res.presetNames[r.index] = p->name;
                answeredNow = true;
            }
            else if (strymon::isNak (raw, size, tgt.model))
            {
                ++res.naks;
                answeredNow = true;
            }
            break;
    }

    if (answeredNow)
    {
        ++answered;
        consecutiveTimeouts = 0;
        advance();
    }
    return answeredNow;
}

void Reader::timedOut()
{
    if (finished())
        return;
    const auto isRoland = tgt.protocol == Protocol::bossGt1000 || tgt.protocol == Protocol::boss500;
    if (isRoland && answered == 0 && position == 0 && ! triedBroadcast)
    {
        // No answer on device ID 17: ask the first one again with the broadcast ID, then keep whichever works.
        triedBroadcast = true;
        rolandDevice = roland::broadcastDeviceId;
        return;
    }
    if (requests[position].kind == Kind::presetName)
        missed.insert (requests[position].index);
    ++consecutiveTimeouts;
    if (consecutiveTimeouts < maxConsecutiveTimeouts)
    {
        advance();
        return;
    }
    if (answered == 0)
    {
        stopped = true;
        return;
    }
    // It answered before: the presets end here. Skip to the next kind of request.
    const auto kind = requests[position].kind;
    if (kind == Kind::presetName && endedAt < 0)
        endedAt = requests[position].index - (maxConsecutiveTimeouts - 1);
    while (position < requests.size() && requests[position].kind == kind)
        ++position;
    consecutiveTimeouts = 0;
}

//==============================================================================
bool isPlaceholderName (const juce::String& name)
{
    const auto t = name.trim();
    return t.isEmpty() || t.equalsIgnoreCase ("<EMPTY>");
}

juce::String unitLabel (const Target& t, int index)
{
    switch (t.protocol)
    {
        case Protocol::bossGt1000: return roland::gt1000PatchLabel (index);
        case Protocol::boss500:
        case Protocol::strymonGen1:
            if (const auto* p = modellers::find (t.id))
                return modellers::presetLabel (*p, -1, index);
            break;
        case Protocol::fractalGen3:
            if (t.model == fractal::vp4Model)   // A1-Z4
                return juce::String::charToString ((juce::juce_wchar) ('A' + index / 4)) + juce::String (index % 4 + 1);
            return "Preset " + juce::String (index).paddedLeft ('0', 3);
    }
    return juce::String (index);
}

static juce::Colour paletteAt (int i)
{
    const auto& p = state::palette();
    return juce::Colour (p.getReference (((i % p.size()) + p.size()) % p.size()).argb);
}

Applied applyToModeller (juce::ValueTree& root, const Target& t, const Results& r, bool onlyNamed, bool replace)
{
    Applied out;
    const auto* profile = modellers::find (t.id);
    if (profile == nullptr || t.customDevice)
        return out;
    auto m = state::modeller (root, t.id);
    if (! m.isValid())
        return out;
    const auto setlist = modellers::defaultSetlist (*profile);

    std::vector<std::pair<int, juce::String>> names;
    for (const auto& [index, name] : r.presetNames)
    {
        if (isPlaceholderName (name) && onlyNamed)
            continue;
        names.emplace_back (index, isPlaceholderName (name) ? "Preset " + unitLabel (t, index) : name.trim());
    }
    if (names.empty())
        return out;

    if (replace)
        for (int i = m.getNumChildren(); --i >= 0;)
            if (m.getChild (i).hasType (IDs::ModPreset))
                m.removeChild (i, nullptr);

    for (const auto& [index, name] : names)
    {
        juce::ValueTree match;
        for (auto c : m)
            if (c.hasType (IDs::ModPreset) && (int) c[IDs::presetIndex] == index && (int) c[IDs::setlist] == setlist)
                match = c;
        if (match.isValid())
        {
            if (match[IDs::name].toString() != name)
            {
                match.setProperty (IDs::name, name, nullptr);
                ++out.renamed;
            }
            continue;
        }
        int presets = 0;
        for (auto c : m)
            if (c.hasType (IDs::ModPreset))
                ++presets;
        // Presets go before the ModSwitch names, like the page adds them.
        int insertAt = 0;
        for (int i = 0; i < m.getNumChildren(); ++i)
            if (m.getChild (i).hasType (IDs::ModPreset))
                insertAt = i + 1;
        m.addChild (state::createModPreset (t.id, name, setlist, index, paletteAt (presets)), insertAt, nullptr);
        ++out.added;
    }
    if (replace)
        m.setProperty (IDs::selectedPreset, 0, nullptr);
    return out;
}

static bool sameSteps (const cues::custom::Parsed& a, const cues::custom::Parsed& b)
{
    if (! a.ok() || ! b.ok() || a.steps.size() != b.steps.size())
        return false;
    for (size_t i = 0; i < a.steps.size(); ++i)
    {
        const auto& x = a.steps[i];
        const auto& y = b.steps[i];
        if (x.kind != y.kind || x.number != y.number || (x.kind == cues::custom::Step::Kind::controller && x.value != y.value))
            return false;
    }
    return true;
}

Applied applyToCustomUnit (juce::ValueTree unit, const Target& t, const Results& r, bool onlyNamed)
{
    Applied out;
    if (! unit.isValid() || ! t.customDevice)
        return out;
    const auto base = (int) unit.getProperty (IDs::programBase, t.protocol == Protocol::bossGt1000 ? 1 : 0) == 1 ? 1 : 0;

    struct Want { juce::String name, messages, note; };
    std::vector<Want> wants;
    const auto nameFor = [&] (int index, juce::String& name)
    {
        const auto it = r.presetNames.find (index);
        name = it != r.presetNames.end() ? it->second.trim() : juce::String();
        if (isPlaceholderName (name))
        {
            if (onlyNamed)
                return false;
            name = unitLabel (t, index);
        }
        return true;
    };

    if (t.protocol == Protocol::bossGt1000)
    {
        // A tile for each user patch the PROGRAM MAP reaches, at its first PC (BANK1 PC#1 first).
        std::set<int> seen;
        for (const auto& [key, value] : r.programMap)
        {
            if (value < 0 || value >= roland::gt1000UserPatches || seen.count (value) > 0)
                continue;   // 250-499: preset patches (P01-1...), whose names aren't read
            juce::String name;
            if (! nameFor (value, name))
                continue;
            seen.insert (value);
            const auto bank = key / 128, pc = key % 128;
            wants.push_back ({ name, "bank " + juce::String (bank) + ", CC 32=0, PC " + juce::String (pc + base),
                               "PROGRAM MAP BANK" + juce::String (bank + 1) + " PC#" + juce::String (pc + 1) + " = "
                                   + roland::gt1000PatchLabel (value) + " on your GT-1000 (name read from the unit)." });
        }
    }
    else if (t.protocol == Protocol::fractalGen3)
    {
        for (const auto& [index, unused] : r.presetNames)
        {
            juce::ignoreUnused (unused);
            juce::String name;
            if (! nameFor (index, name))
                continue;
            const auto messages = t.model == fractal::vp4Model
                                      ? "PC " + juce::String (index + base)
                                      : "bank " + juce::String (index / 128) + ", PC " + juce::String (index % 128 + base);
            wants.push_back ({ name, messages, unitLabel (t, index) + " on your " + t.deviceName + " (name read from the unit)." });
        }
    }
    if (wants.empty())
        return out;

    juce::ValueTree group;
    for (auto g : unit)
        if (g.hasType (IDs::Group) && g[IDs::name].toString().trim().equalsIgnoreCase ("Presets"))
            group = g;
    if (! group.isValid())
    {
        group = juce::ValueTree (IDs::Group);
        group.setProperty (IDs::name, "Presets", nullptr);
        unit.addChild (group, 0, nullptr);
    }

    for (const auto& w : wants)
    {
        const auto parsed = cues::custom::parse (w.messages, base);
        juce::ValueTree match;
        for (auto tile : group)
            if (tile.hasType (IDs::CueTile) && sameSteps (cues::custom::parse (tile[IDs::messages].toString(), base), parsed))
            {
                match = tile;
                break;
            }
        if (match.isValid())
        {
            if (match[IDs::name].toString() != w.name)
            {
                match.setProperty (IDs::name, w.name, nullptr);
                ++out.renamed;
            }
            continue;
        }
        juce::ValueTree tile (IDs::CueTile);
        tile.setProperty (IDs::name, w.name, nullptr);
        tile.setProperty (IDs::messages, w.messages, nullptr);
        tile.setProperty (IDs::note, w.note, nullptr);
        tile.setProperty (IDs::colour, paletteAt (4 + group.getNumChildren() % 6).toString(), nullptr);
        group.appendChild (tile, nullptr);
        ++out.added;
    }
    return out;
}
}
