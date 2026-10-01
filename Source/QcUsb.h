#pragma once

#include <juce_core/juce_core.h>

#include <array>
#include <atomic>
#include <functional>
#include <optional>
#include <vector>

// Read-only access to a Quad Cortex over USB, to fill the library with its setlists and preset names.
// The QC speaks a private USB-HID protocol (the one Cortex Control uses); it isn't documented by Neural DSP.
// This follows the community reverse-engineering in pyquadcortex (MIT) and qc-mcp, and only ever READS:
// it never recalls, saves or changes anything on the pedal. MIDI cues don't use any of this.
namespace qcusb
{
constexpr int vendorId = 0x152A;
constexpr int productIdQc = 0x880A, productIdMini = 0x892F;
constexpr int reportSize = 129;   // report ID + 128 data bytes
using Report = std::array<juce::uint8, reportSize>;

// Message types (CortexMessageType) used here.
enum MessageType : juce::uint32
{
    typeFile = 4, typeVersion = 10, typeScene = 13, typeMode = 14, typeMasterVolume = 17, typeUndoRedo = 21,
    typeSetlistPosition = 2, typeIOSettings = 3, typeGeneralSettings = 9, typeKeepAlive = 32, typeGlobalTempo = 33,
    typePresetDirty = 34, typeModuleStats = 35, typeGlobalEQ = 38, typeConnection = 49, typeModelRepo = 51,
    typeResetCommsBuffers = 52
};

//==============================================================================
// Minimal protobuf encoding/decoding: just what these few messages need.
struct PbWriter
{
    std::vector<juce::uint8> bytes;
    void varint (juce::uint64 v);
    void fieldVarint (int field, juce::uint64 v);
    void fieldString (int field, const juce::String& s);
};

struct PbField
{
    int number = 0, wireType = 0;
    juce::uint64 value = 0;                 // varint / fixed
    const juce::uint8* data = nullptr;      // length-delimited
    size_t size = 0;
    juce::String text() const { return juce::String::fromUTF8 ((const char*) data, (int) size); }
};

// Calls f for each field; returns false on malformed input.
bool forEachField (const juce::uint8* data, size_t size, const std::function<void (const PbField&)>& f);

//==============================================================================
// Framing: a message is [protobuf payload][type LE32][4 zero bytes], split into 126-byte chunks.
std::vector<Report> encodeMessage (juce::uint32 type, const std::vector<juce::uint8>& payload);

struct Message
{
    juce::uint32 type = 0;
    bool encrypted = false;
    juce::MemoryBlock payload;   // gunzipped if it was compressed
};

class Reassembler
{
public:
    // Feed one input report (with its report ID byte). Returns a message when its last report arrives.
    std::optional<Message> feed (const juce::uint8* report, int size);

private:
    juce::MemoryBlock partial;
    bool active = false;
};

//==============================================================================
struct Preset { int position = 0; juce::String name; };   // position 0..255 = (bank-1)*8 + slot

struct Folder
{
    juce::String key, name;
    bool isFactory = false;
    std::vector<Preset> presets;    // named slots only
    int fileCount = 0;              // entries in the push (incl. empty slots)
};

// A File push (type 4, action UPDATE) describing one folder, or nothing.
std::optional<Folder> parseFolder (const Message&);

// True for real setlists: "My Presets", the player's own setlists and the Factory Library.
bool isSetlist (const Folder&);

inline int bankOf (int position)              { return position / 8 + 1; }
inline int slotOf (int position)              { return position % 8; }

//==============================================================================
struct Result
{
    bool ok = false;
    juce::String error;                       // shown to the user when !ok
    juce::String corosVersion;
    bool isMini = false;
    std::vector<Folder> setlists;
};

// Connects over USB, lists every setlist and preset name, disconnects. Blocking (run it off the message thread).
// progress() is called from that thread with short status texts. Set cancel to stop early.
Result readSetlists (const std::function<void (const juce::String&)>& progress, std::atomic<bool>& cancel);
} // namespace qcusb
