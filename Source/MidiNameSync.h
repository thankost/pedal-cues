#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_data_structures/juce_data_structures.h>

#include <array>
#include <map>
#include <optional>
#include <set>
#include <vector>

// "Read names from the unit (beta)": reads preset names over MIDI SysEx, read-only.
//
// READ-ONLY RULE (like the Quad Cortex USB sync): the only messages this code ever builds are the read requests below,
// all in the "Allowed requests" section, and the dialog sends nothing that namesync::isAllowedRequest rejects:
//   - Roland / Boss: RQ1 (command 0x11) only. Never DT1 (0x12): that writes into the unit's memory.
//   - Fractal Axe-Fx III family: QUERY PATCH NAME (0x0D) and QUERY SCENE NAME (0x0E) only. No scene / bypass / channel
//     sets, no looper, tap, tuner or tempo.
//   - Strymon gen 1 (TimeLine, Mobius, BigSky): request single preset (0x63; 0x67 "fast fetch" allowed by the whitelist but
//     not sent). Never 0x62 (write), 0x64 (save), 0x65 (factory write), 0x1B / 0x1F (reset), 0x20, 0x7A / 0x7B (flash).
// No preset loads, no tuner / tap / bypass. Nothing on the unit changes.
//
// Sources (see each function): Boss GT-1000 / DD-500 / RV-500 / MD-500 MIDI Implementations, Fractal "Axe-Fx III MIDI for
// 3rd Party Devices" rev 1.4, Strymon's open-source Preset Librarian (spl, DcMidiDevDefs.h). None of it is tested on hardware.
namespace namesync
{
enum class Protocol
{
    bossGt1000,   // Roland RQ1, model 00 00 00 4F, 250 user patches + the PROGRAM MAP
    boss500,      // Roland RQ1, model byte per pedal (DD-500 4D, MD-500 43, RV-500 42), 297 patches 01A-99C
    fractalGen3,  // Fractal 0x0D / 0x0E, model byte (Axe-Fx III 10, FM3 11, FM9 12, VP4 14)
    strymonGen1   // Strymon 0x63, product byte (TimeLine 01, Mobius 02, BigSky 03)
};

struct Target
{
    Protocol protocol = Protocol::boss500;
    juce::String id;            // the modellers::Profile id or the device template id
    juce::String deviceName;    // "GT-1000", "Axe-Fx III"
    int model = 0;              // Roland model ID's last byte / Fractal model byte / Strymon product byte
    int presetCount = 0;        // how many presets are read
    bool customDevice = false;  // true: a template's custom device (tiles); false: a page (ModPreset nodes)
    bool sceneNames = false;    // Fractal: can also read the loaded preset's scene names
    int sceneCount = 0;         // Fractal: 8 (VP4: 4)
};

// The protocol for a page profile id (boss.dd-500, strymon.timeline...) or a template id (boss.gt-1000, fractal.fm9...).
std::optional<Target> targetFor (const juce::String& profileOrTemplateId);

//==============================================================================
// Roland / Boss. Addresses and sizes are 4 bytes of 7 bits; here they're one "linear" int (a<<21 | b<<14 | c<<7 | d),
// so adding offsets carries at 0x80 the way the unit counts.
namespace roland
{
    constexpr juce::uint8 manufacturer = 0x41, rq1 = 0x11, dt1 = 0x12;
    constexpr int defaultDeviceId = 0x10;   // device ID 17, the usual factory setting
    constexpr int broadcastDeviceId = 0x7F; // "7FH = Broadcast" in the MIDI Implementations
    constexpr int gt1000Model = 0x4F, dd500Model = 0x4D, md500Model = 0x43, rv500Model = 0x42;
    constexpr int nameLength = 16;          // Patch Name1-16, ASCII 32-126
    constexpr int gt1000UserPatches = 250;  // U01-1 .. U50-5
    constexpr int boss500Patches = 297;     // 01A .. 99C

    int linear (juce::uint8 a, juce::uint8 b, juce::uint8 c, juce::uint8 d);
    std::array<juce::uint8, 4> bytes (int linearValue);
    // The standard Roland checksum: (128 - (sum of the bytes % 128)) % 128. The four PDFs only name the 'sum' field: the formula
    // is Roland's usual one, to confirm against a reply.
    juce::uint8 checksum (const juce::uint8* data, int size);

    // F0 41 dev 00 00 00 model 11 aaaa ssss sum F7 (address and size as linear values).
    juce::MidiMessage requestData (int model, int deviceId, int address, int size);

    struct Data { int deviceId = 0, model = 0, address = 0; std::vector<juce::uint8> data; bool checksumOk = false; };
    // A DT1 reply (F0 41 dev 00 00 00 model 12 aaaa data sum F7), raw bytes from F0 to F7. Nothing for anything else.
    std::optional<Data> parseData (const juce::uint8* raw, int size);

    int gt1000PatchNameAddress (int patch);       // patch 0-249 (U01-1 = 0): 20 00 00 00, ..., 21 79 00 00
    int gt1000ProgramMapAddress (int bank, int pc); // bank 0-3 (BANK1-4), pc 0-127: 00 10 00 00 + bank * 4 * 128 + pc * 4
    constexpr int gt1000ProgramMapEntrySize = 4;  // 4 nibble bytes per PC: value 0-499
    juce::String gt1000PatchLabel (int patch);    // "U01-1".."U50-5"
    int boss500PatchNameAddress (int index);      // index 0-296 (01A = 0): bank N = index / 3 + 1, patch A-C
}

//==============================================================================
// Fractal Audio Axe-Fx III family: F0 00 01 74 <model> <function> <data> <cs> F7.
namespace fractal
{
    constexpr int axeFx3Model = 0x10, fm3Model = 0x11, fm9Model = 0x12, vp4Model = 0x14;
    constexpr juce::uint8 queryPatchNameFunction = 0x0D, querySceneNameFunction = 0x0E;
    constexpr int nameLength = 32;
    constexpr int current = -1;   // 7F 7F / 7F: the loaded preset / scene

    // XOR of every byte from F0 through the last data byte, & 0x7F. The PDF only says "XOR checksum"; this is Fractal's usual
    // convention (it matches the forum's Axe-Fx II example F0 00 01 74 03 0F 09 F7).
    juce::uint8 checksum (const juce::uint8* fromF0, int size);
    juce::MidiMessage queryPatchName (int model, int preset);   // preset 0-16383 (LS 7 bits first) or current
    juce::MidiMessage querySceneName (int model, int scene);    // scene 0-7 or current

    struct Name { bool scene = false; int number = 0; juce::String name; bool checksumOk = false; };
    std::optional<Name> parseName (const juce::uint8* raw, int size, int model);
}

//==============================================================================
// Strymon TimeLine / Mobius / BigSky: F0 00 01 55 12 <product> <opcode> ... F7.
namespace strymon
{
    constexpr int timeLine = 0x01, mobius = 0x02, bigSky = 0x03;
    constexpr juce::uint8 requestPresetOpcode = 0x63, fastFetchOpcode = 0x67, presetDumpOpcode = 0x62, nakOpcode = 0x47;
    constexpr int replySize = 650, numberOffset = 7, nameOffset = 632, nameLength = 16, checksumOffset = 648;

    // F0 00 01 55 12 pp 63 hi lo F7. The hi / lo split (n >> 7, n & 0x7F) is inferred from spl's getHi / getLo.
    juce::MidiMessage requestPreset (int product, int preset);

    struct Preset { int number = 0; juce::String name; };
    std::optional<Preset> parsePreset (const juce::uint8* raw, int size, int product);   // a 650-byte dump (opcode 62 or 67)
    bool isNak (const juce::uint8* raw, int size, int product);
}

//==============================================================================
// The whitelist: true only for the read requests above. The dialog refuses to send anything else.
bool isAllowedRequest (const juce::MidiMessage&);

//==============================================================================
// The read as a state machine, so it can be tested with a simulated unit: the dialog sends current(), passes every SysEx it
// receives to accept(), and calls timedOut() when a request gets no answer in time.
struct Results
{
    std::map<int, juce::String> presetNames;   // preset index (as the target counts) -> name (may be empty)
    std::map<int, int> programMap;             // GT-1000: bank * 128 + pc -> PROGRAM MAP value (0-249 user, 250-499 preset)
    std::array<juce::String, 8> sceneNames;    // Fractal: the loaded preset's scenes
    bool hasSceneNames = false;
    int numberMismatches = 0;                  // replies numbered differently from the request (kept, for hardware testing)
    int checksumMismatches = 0;                // replies whose checksum didn't match the formula (kept)
    int naks = 0;                              // Strymon: "no such preset"
};

class Reader
{
public:
    Reader (Target, bool withSceneNames);

    bool finished() const;
    bool anyAnswer() const { return answered > 0; }
    // No answer to the first maxConsecutiveTimeouts requests: the read stopped (wrong port, channel-less SysEx off, ...).
    bool gaveUp() const { return stopped; }
    // The unit answered, then stopped answering preset names (e.g. an Axe-Fx III Mk I has 512, not 1024): the first index that
    // got no answer, or -1. The read goes on with the next kind of request (PROGRAM MAP, scene names).
    int presetsEndedAt() const { return endedAt; }
    int done() const { return (int) position; }
    int total() const { return (int) requests.size(); }
    juce::MidiMessage current() const;
    int timeoutMs() const;                     // how long to wait for an answer to current()
    static constexpr int gapMs = 50;           // between requests
    static constexpr int maxConsecutiveTimeouts = 5;

    bool accept (const juce::uint8* raw, int size);   // true when it answered current() (then the next one is current)
    void timedOut();

    const Results& results() const { return res; }
    const Target& target() const { return tgt; }
    int deviceId() const { return rolandDevice; }

private:
    enum class Kind { presetName, programMap, sceneName };
    struct Request { Kind kind; int index; int address = 0, size = 0; };

    void advance();
    bool collectRoland (const roland::Data&);

    Target tgt;
    std::vector<Request> requests;
    size_t position = 0;
    int rolandDevice = roland::defaultDeviceId;
    bool triedBroadcast = false;
    int consecutiveTimeouts = 0, answered = 0, endedAt = -1;
    bool stopped = false;
    std::set<int> missed;                       // preset names that timed out: a late reply still fills them in
    std::map<int, juce::uint8> memory;          // Roland: bytes received, by linear address (replies may come in pieces)
    Results res;
};

//==============================================================================
// Applying the names.
bool isPlaceholderName (const juce::String&);   // empty, spaces, "<EMPTY>"

struct Applied { int added = 0, renamed = 0; };

// A page (Boss 500, Strymon): a ModPreset per named preset (renames the one with the same index, else adds one).
// replace: removes the page's presets first (only if there's at least one name to put back).
Applied applyToModeller (juce::ValueTree& root, const Target&, const Results&, bool onlyNamed, bool replace);

// A template's custom device (GT-1000, Fractal): a tile per named preset in its "Presets" group, with the template's message
// pattern (GT-1000: "bank k, CC 32=0, PC n" following the PROGRAM MAP; Fractal "bank b, PC p"; VP4 "PC p"). A tile that already
// sends those messages is renamed.
Applied applyToCustomUnit (juce::ValueTree unit, const Target&, const Results&, bool onlyNamed);

// What a preset is called on the unit: "U01-1", "01A", "00B", "Preset 012".
juce::String unitLabel (const Target&, int index);

// The dialog (MidiNameSyncDialog.cpp): pick MIDI Out / In, read, review, Apply. customUnit is invalid for pages.
void showMidiNameSync (juce::ValueTree state, const juce::String& profileOrTemplateId, juce::ValueTree customUnit);
}
