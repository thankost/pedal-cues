#include "QcUsb.h"

#include <hidapi.h>
#if JUCE_MAC
 #include <hidapi_darwin.h>
#endif

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <thread>

namespace qcusb
{
//==============================================================================
void PbWriter::varint (juce::uint64 v)
{
    do
    {
        auto b = (juce::uint8) (v & 0x7f);
        v >>= 7;
        bytes.push_back ((juce::uint8) (b | (v != 0 ? 0x80 : 0)));
    }
    while (v != 0);
}

void PbWriter::fieldVarint (int field, juce::uint64 v)
{
    varint ((juce::uint64) (field << 3));
    varint (v);
}

void PbWriter::fieldString (int field, const juce::String& s)
{
    varint ((juce::uint64) ((field << 3) | 2));
    const auto* utf8 = s.toRawUTF8();
    const auto n = (size_t) s.getNumBytesAsUTF8();
    varint (n);
    bytes.insert (bytes.end(), utf8, utf8 + n);
}

static bool readVarint (const juce::uint8*& p, const juce::uint8* end, juce::uint64& out)
{
    out = 0;
    for (int shift = 0; p < end && shift < 64; shift += 7)
    {
        const auto b = *p++;
        out |= (juce::uint64) (b & 0x7f) << shift;
        if ((b & 0x80) == 0)
            return true;
    }
    return false;
}

bool forEachField (const juce::uint8* data, size_t size, const std::function<void (const PbField&)>& f)
{
    const auto* p = data;
    const auto* end = data + size;

    while (p < end)
    {
        juce::uint64 tag = 0;
        if (! readVarint (p, end, tag))
            return false;

        PbField field;
        field.number = (int) (tag >> 3);
        field.wireType = (int) (tag & 7);

        switch (field.wireType)
        {
            case 0: if (! readVarint (p, end, field.value)) return false; break;
            case 1: if (end - p < 8) return false; p += 8; break;
            case 5: if (end - p < 4) return false; p += 4; break;
            case 2:
            {
                juce::uint64 len = 0;
                if (! readVarint (p, end, len) || len > (juce::uint64) (end - p))
                    return false;
                field.data = p;
                field.size = (size_t) len;
                p += len;
                break;
            }
            default: return false;   // groups (3/4) don't occur in this protocol
        }
        f (field);
    }
    return true;
}

//==============================================================================
std::vector<Report> encodeMessage (juce::uint32 type, const std::vector<juce::uint8>& payload)
{
    std::vector<juce::uint8> body (payload);
    for (int i = 0; i < 4; ++i)
        body.push_back ((juce::uint8) ((type >> (8 * i)) & 0xff));
    body.insert (body.end(), 4, 0);   // encrypted, compressed, 2 reserved: the host always sends zeros

    std::vector<Report> reports;
    constexpr size_t chunk = 126;
    for (size_t offset = 0; offset < body.size(); offset += chunk)
    {
        const auto n = std::min (chunk, body.size() - offset);
        Report r {};
        r[0] = 0x02;
        r[1] = (juce::uint8) n;
        r[2] = (juce::uint8) ((offset == 0 ? 0x40 : 0) | (offset + n >= body.size() ? 0x80 : 0));
        std::copy (body.begin() + (std::ptrdiff_t) offset, body.begin() + (std::ptrdiff_t) (offset + n), r.begin() + 3);
        reports.push_back (r);
    }
    return reports;
}

std::optional<Message> Reassembler::feed (const juce::uint8* report, int size)
{
    if (size < 3)
        return {};

    const auto len = juce::jmin ((int) report[1], 126, size - 3);
    const auto flags = report[2];

    if (flags & 0x40)       // a FIRST report while a message is half-built: the device interleaves, start over
    {
        partial.reset();
        active = true;
    }
    if (! active)
        return {};

    partial.append (report + 3, (size_t) len);
    if (partial.getSize() > (1u << 20))
    {
        partial.reset();
        active = false;
        return {};
    }
    if ((flags & 0x80) == 0)
        return {};

    active = false;
    if (partial.getSize() < 8)
        return {};

    const auto* bytes = static_cast<const juce::uint8*> (partial.getData());
    const auto bodySize = partial.getSize() - 8;
    const auto* trailer = bytes + bodySize;

    Message m;
    m.type = (juce::uint32) trailer[0] | ((juce::uint32) trailer[1] << 8) | ((juce::uint32) trailer[2] << 16) | ((juce::uint32) trailer[3] << 24);
    m.encrypted = trailer[4] != 0;

    if (bodySize >= 2 && bytes[0] == 0x1f && bytes[1] == 0x8b)   // gzip stream (checked by its magic bytes)
    {
        juce::MemoryInputStream in (bytes, bodySize, false);
        juce::GZIPDecompressorInputStream gz (&in, false, juce::GZIPDecompressorInputStream::gzipFormat);
        gz.readIntoMemoryBlock (m.payload);
    }
    else
    {
        m.payload.append (bytes, bodySize);
    }

    partial.reset();
    return m;
}

//==============================================================================
std::optional<Folder> parseFolder (const Message& m)
{
    if (m.type != typeFile || m.encrypted)
        return {};

    juce::uint64 action = 0;
    const PbField* folderField = nullptr;
    PbField folderCopy;
    const auto ok = forEachField (static_cast<const juce::uint8*> (m.payload.getData()), m.payload.getSize(), [&] (const PbField& f)
    {
        if (f.number == 1 && f.wireType == 0) action = f.value;
        if (f.number == 4 && f.wireType == 2) { folderCopy = f; folderField = &folderCopy; }
    });
    if (! ok || action != 1 || folderField == nullptr)
        return {};

    Folder folder;
    int arrayPosition = 0;
    const auto folderOk = forEachField (folderField->data, folderField->size, [&] (const PbField& f)
    {
        if (f.number == 1 && f.wireType == 2) folder.key = f.text();
        else if (f.number == 3 && f.wireType == 2) folder.name = f.text();
        else if (f.number == 4 && f.wireType == 0) folder.isFactory = f.value != 0;
        else if (f.number == 7 && f.wireType == 2)
        {
            // ProductData: f2 index (may be missing; then the array position is the slot), f3 name.
            bool hasIndex = false;
            int index = 0;
            juce::String name;
            forEachField (f.data, f.size, [&] (const PbField& p)
            {
                if (p.number == 2 && p.wireType == 0) { hasIndex = true; index = (int) p.value; }
                else if (p.number == 3 && p.wireType == 2) name = p.text();
            });

            const auto position = hasIndex ? index : arrayPosition;
            ++arrayPosition;
            ++folder.fileCount;
            if (name.trim().isNotEmpty() && juce::isPositiveAndBelow (position, 256))
                folder.presets.push_back ({ position, name.trim() });
        }
    });
    if (! folderOk || folder.key.isEmpty())
        return {};

    if (folder.name.isEmpty())
        folder.name = folder.key.trimCharactersAtEnd ("/").fromLastOccurrenceOf ("/", false, false);
    return folder;
}

bool isSetlist (const Folder& f)
{
    const auto key = f.key.trimCharactersAtEnd ("/");
    return key.startsWith ("/media/p4/Presets/") || key == "/opt/neuraldsp/Factory Library";
}

//==============================================================================
std::optional<PresetDetails> parsePresetDetails (const juce::uint8* data, size_t size)
{
    PresetDetails d;
    int scene = 0, colour = 0;
    std::array<juce::String, 8> stompLabels, singleStompLabels;

    auto readMapEntry = [] (const PbField& f, std::array<juce::String, 8>& into)   // map<uint32,string> entry {1:key, 2:value}
    {
        juce::uint64 key = 0;
        juce::String value;
        forEachField (f.data, f.size, [&] (const PbField& e)
        {
            if (e.number == 1 && e.wireType == 0) key = e.value;
            else if (e.number == 2 && e.wireType == 2) value = e.text();
        });
        if (key < 8)
            into[(size_t) key] = value.trim();
    };

    const auto ok = forEachField (data, size, [&] (const PbField& f)
    {
        if (f.number == 2 && f.wireType == 2)
            d.name = f.text().trim();
        else if (f.number == 15 && f.wireType == 2)                       // scene_labels: " " = unlabelled
        {
            if (scene < 8)
                d.sceneNames[(size_t) scene] = f.text().trim();
            ++scene;
        }
        else if (f.number == 31 && f.wireType == 2)                       // scene_colors, packed varints (ARGB)
        {
            const auto* q = f.data;
            const auto* end = f.data + f.size;
            juce::uint64 v = 0;
            while (q < end && readVarint (q, end, v))
                if (colour < 8)
                    d.sceneColours[(size_t) colour++] = (juce::uint32) v;
        }
        else if (f.number == 31 && f.wireType == 0 && colour < 8)         // ... or unpacked
            d.sceneColours[(size_t) colour++] = (juce::uint32) f.value;
        else if (f.number == 32 && f.wireType == 2) readMapEntry (f, stompLabels);
        else if (f.number == 34 && f.wireType == 2) readMapEntry (f, singleStompLabels);
    });
    if (! ok)
        return {};

    d.sceneCount = juce::jmin (scene, 8);
    for (size_t i = 0; i < 8; ++i)                                         // the unit prefers the single-block label
        d.stompNames[i] = singleStompLabels[i].isNotEmpty() ? singleStompLabels[i] : stompLabels[i];
    return d;
}

std::optional<PresetDetails> parseRecallPreset (const Message& m)
{
    if (m.type != typeRecallPreset || m.encrypted)
        return {};
    std::optional<PresetDetails> details;
    forEachField (static_cast<const juce::uint8*> (m.payload.getData()), m.payload.getSize(), [&] (const PbField& f)
    {
        if (f.number == 3 && f.wireType == 2)   // preset: an embedded BinaryPreset
            details = parsePresetDetails (f.data, f.size);
    });
    return details;
}

//==============================================================================
namespace
{
// One USB session: a reader thread reassembling messages, a keep-alive thread, serialized writes.
class Session
{
public:
    explicit Session (hid_device* d) : dev (d)
    {
        reader = std::thread ([this] { readLoop(); });
        keepAlive = std::thread ([this] { keepAliveLoop(); });
    }

    ~Session()
    {
        running = false;
        cv.notify_all();
        if (keepAlive.joinable()) keepAlive.join();
        if (reader.joinable()) reader.join();
        hid_close (dev);
    }

    void send (juce::uint32 type, const std::vector<juce::uint8>& payload)
    {
        const std::lock_guard<std::mutex> lock (writeLock);   // continuation reports carry no header: keep messages whole
        for (const auto& r : encodeMessage (type, payload))
            hid_write (dev, r.data(), r.size());              // the QC stalls every write's status stage: ignore the result
    }

    // Waits for a message matching pred; returns it (and drops it from the queue).
    std::optional<Message> waitFor (const std::function<bool (const Message&)>& pred, int timeoutMs)
    {
        std::unique_lock<std::mutex> lock (queueLock);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds (timeoutMs);
        for (;;)
        {
            for (auto it = queue.begin(); it != queue.end(); ++it)
                if (pred (*it))
                {
                    auto m = std::move (*it);
                    queue.erase (it);
                    return m;
                }
            if (lost || ! running || cv.wait_until (lock, deadline) == std::cv_status::timeout)
                return {};
        }
    }

    // Takes every queued message of a type.
    std::vector<Message> take (juce::uint32 type)
    {
        const std::lock_guard<std::mutex> lock (queueLock);
        std::vector<Message> out;
        for (auto it = queue.begin(); it != queue.end();)
        {
            if (it->type == type) { out.push_back (std::move (*it)); it = queue.erase (it); }
            else ++it;
        }
        return out;
    }

    bool isLost() const { return lost; }

private:
    void readLoop()
    {
        Reassembler reassembler;
        std::array<juce::uint8, 512> buffer {};
        int failures = 0;

        while (running)
        {
            const auto n = hid_read_timeout (dev, buffer.data(), buffer.size(), 200);
            if (n < 0)
            {
                if (++failures >= 2)   // two failed reads in a row = the device is gone
                {
                    lost = true;
                    cv.notify_all();
                    return;
                }
                continue;
            }
            failures = 0;
            if (n == 0)
                continue;

            if (auto m = reassembler.feed (buffer.data(), n))
            {
                // Keep only what this client reads; drop the rest (e.g. the ~47 KB ModelRepo reply).
                if (m->type == typeFile || m->type == typeVersion || m->type == typeResetCommsBuffers || m->type == typeRecallPreset
                    || m->type == typeSetlistPosition || m->type == typePresetDirty || m->type == typeScene)
                {
                    const std::lock_guard<std::mutex> lock (queueLock);
                    queue.push_back (std::move (*m));
                    if (queue.size() > 4000)
                        queue.pop_front();
                }
                cv.notify_all();
            }
        }
    }

    void keepAliveLoop()
    {
        PbWriter ka;
        ka.fieldVarint (1, 1);   // action UPDATE
        ka.fieldVarint (3, 1);   // is_online
        while (running)
        {
            send (typeKeepAlive, ka.bytes);
            std::unique_lock<std::mutex> lock (sleepLock);
            sleepCv.wait_for (lock, std::chrono::seconds (1), [this] { return ! running.load(); });
        }
    }

    hid_device* dev;
    std::atomic<bool> running { true }, lost { false };
    std::thread reader, keepAlive;
    std::mutex writeLock, queueLock, sleepLock;
    std::condition_variable cv, sleepCv;
    std::deque<Message> queue;
};

std::vector<juce::uint8> readRequest()   // { action: READ }
{
    PbWriter w;
    w.fieldVarint (1, 3);
    return w.bytes;
}

std::vector<juce::uint8> readRequest (juce::uint64 rid)   // { action: READ, request_id }
{
    PbWriter w;
    w.fieldVarint (1, 3);
    w.fieldVarint (2, rid);
    return w.bytes;
}

juce::String randomHex32()
{
    juce::Random r;
    juce::String s;
    for (int i = 0; i < 32; ++i)
        s << juce::String::charToString ("0123456789abcdef"[r.nextInt (16)]);
    return s;
}

std::optional<juce::uint64> requestIdOf (const Message& m)   // f2 in every message used here
{
    std::optional<juce::uint64> rid;
    forEachField (static_cast<const juce::uint8*> (m.payload.getData()), m.payload.getSize(), [&] (const PbField& f)
    {
        if (f.number == 2 && f.wireType == 0) rid = f.value;
    });
    return rid;
}

// Waits for the reply of the given type that echoes rid.
std::optional<Message> waitReply (Session& session, juce::uint32 type, juce::uint64 rid, int timeoutMs)
{
    return session.waitFor ([type, rid] (const Message& m) { return m.type == type && requestIdOf (m) == rid; }, timeoutMs);
}

struct Position
{
    juce::String folderKey;
    juce::uint32 position = 0;
    bool isFactory = false, valid = false, special = false;   // special = Downloads / plugin banks (not restorable here)
};

Position parsePosition (const Message& m)
{
    Position p;
    p.valid = true;
    forEachField (static_cast<const juce::uint8*> (m.payload.getData()), m.payload.getSize(), [&] (const PbField& f)
    {
        if (f.number == 3 && f.wireType == 2) p.folderKey = f.text();
        else if (f.number == 4 && f.wireType == 0) p.position = (juce::uint32) f.value;
        else if (f.number == 5 && f.wireType == 0) p.isFactory = f.value != 0;
        else if ((f.number == 6 || f.number == 8) && f.wireType == 0 && f.value != 0) p.special = true;
    });
    return p;
}

// hidapi's macOS device manager is tied to the run loop of the thread that calls hid_init(). Every sync runs on
// its own worker thread, so hidapi is started and stopped around each sync, on that thread: a manager left over
// from an earlier, finished thread made the second sync crash (macOS 27 traps the dead run loop).
// One USB sync at a time, so two PedalCues windows can't stop hidapi under each other.
class UsbScope
{
public:
    UsbScope() : lock (mutex()) { ok = hid_init() == 0; }
    ~UsbScope() { hid_exit(); }
    bool ok = false;

private:
    static std::mutex& mutex() { static std::mutex m; return m; }
    std::lock_guard<std::mutex> lock;
};

struct Connected
{
    std::unique_ptr<Session> session;
    juce::String error, corosVersion;
    bool isMini = false;
};

// Opens the QC and runs the handshake Cortex Control uses. All requests are READs (plus Connection/KeepAlive).
Connected connect (const std::function<void (const juce::String&)>& progress, std::atomic<bool>& cancel)
{
    Connected c;
    if (hid_init() != 0)
    {
        c.error = "USB access isn't available on this computer.";
        return c;
    }

    juce::String path;
    if (auto* list = hid_enumerate ((unsigned short) vendorId, 0))
    {
        for (auto* d = list; d != nullptr; d = d->next)
            if ((d->product_id == productIdQc || d->product_id == productIdMini) && (d->interface_number == 5 || d->usage_page == 0x0001))
            {
                path = d->path;
                c.isMini = d->product_id == productIdMini;
                break;
            }
        hid_free_enumeration (list);
    }
    if (path.isEmpty())
    {
        c.error = "No Quad Cortex found on USB. Connect the QC's USB port to this computer, switch it on and try again.";
        return c;
    }

   #if JUCE_MAC
    hid_darwin_set_open_exclusive (0);   // don't lock the pedal away from other apps
   #endif
    auto* dev = hid_open_path (path.toRawUTF8());
    if (dev == nullptr)
    {
        c.error = "Couldn't open the Quad Cortex over USB. Quit Cortex Control (it keeps the USB connection to itself) and try again.";
        return c;
    }

    progress ("Connecting to the Quad Cortex...");
    auto session = std::make_unique<Session> (dev);
    const auto start = juce::Time::getMillisecondCounter();
    auto elapsed = [start] { return (int) (juce::Time::getMillisecondCounter() - start); };
    auto stop = [&] { return cancel.load() || session->isLost(); };

    // Identity: ask for the version until the full reply (device type + CorOS version) arrives.
    while (c.corosVersion.isEmpty() && elapsed() < 30000 && ! stop())
    {
        session->send (typeVersion, readRequest());
        if (auto v = session->waitFor ([] (const Message& m) { return m.type == typeVersion; }, 5000))
            forEachField (static_cast<const juce::uint8*> (v->payload.getData()), v->payload.getSize(), [&] (const PbField& f)
            {
                if (f.number == 4 && f.wireType == 2) c.corosVersion = f.text();
                if (f.number == 12 && f.wireType == 0) c.isMini = c.isMini || f.value == 1;
            });
    }
    if (cancel) { c.error = "Cancelled."; return c; }
    if (c.corosVersion.isEmpty())
    {
        c.error = "The Quad Cortex didn't answer over USB. Quit Cortex Control, wait until the QC has fully started, and try again.";
        return c;
    }

    // Session start: reset the comms buffers, announce ourselves the way Cortex Control does, subscribe.
    PbWriter reset;
    reset.fieldVarint (1, 0);
    reset.fieldString (2, randomHex32());
    bool acked = false;
    while (! acked && elapsed() < 60000 && ! stop())
    {
        session->send (typeResetCommsBuffers, reset.bytes);
        acked = session->waitFor ([] (const Message& m) { return m.type == typeResetCommsBuffers; }, 5000).has_value();
    }
    if (cancel) { c.error = "Cancelled."; return c; }
    if (! acked)
    {
        c.error = "The Quad Cortex didn't start a USB session. Unplug the USB cable, plug it back in and try again.";
        return c;
    }

    PbWriter announce;
    announce.fieldVarint (1, 1);                       // UPDATE
    announce.fieldString (11, c.corosVersion);         // cortex_control_version: mirror the unit's version
    session->send (typeVersion, announce.bytes);

    PbWriter connectMsg;
    connectMsg.fieldVarint (2, 1);                     // connected: true
    session->send (typeConnection, connectMsg.bytes);

    for (const auto type : { typeModelRepo, typeModuleStats, typeUndoRedo, typeIOSettings, typeGeneralSettings, typeMode,
                             typeGlobalEQ, typeMasterVolume, typeGlobalTempo, typeScene, typePresetDirty, typeSetlistPosition,
                             typeRecallPreset })
        session->send (type, readRequest());           // all READs: nothing here changes the pedal

    std::this_thread::sleep_for (std::chrono::seconds (2));
    c.session = std::move (session);
    return c;
}

void disconnect (Session& session)
{
    PbWriter goodbye;
    goodbye.fieldVarint (2, 0);                        // connected: false
    session.send (typeConnection, goodbye.bytes);
}
} // namespace

Result readSetlists (const std::function<void (const juce::String&)>& progress, std::atomic<bool>& cancel)
{
    Result result;
    auto fail = [&result] (const juce::String& why) { result.ok = false; result.error = why; return result; };

    const UsbScope usb;   // outlives the session below
    auto c = connect (progress, cancel);
    if (c.session == nullptr)
        return fail (c.error);
    auto& session = *c.session;
    result.corosVersion = c.corosVersion;
    result.isMini = c.isMini;
    auto lostOrCancelled = [&] { return cancel.load() || session.isLost(); };
    juce::uint64 rid = 100;

    // The loaded preset: where it is and its scenes/stomps. Pure reads, no side effects.
    {
        const auto posRid = ++rid;
        session.send (typeSetlistPosition, readRequest (posRid));
        if (auto m = waitReply (session, typeSetlistPosition, posRid, 5000))
        {
            const auto pos = parsePosition (*m);
            if (! pos.special)
            {
                result.currentFolderKey = pos.folderKey.trimCharactersAtEnd ("/");
                result.currentPosition = (int) pos.position;
            }
        }
        const auto presetRid = ++rid;
        session.send (typeRecallPreset, readRequest (presetRid));
        if (auto m = waitReply (session, typeRecallPreset, presetRid, 15000))
            result.current = parseRecallPreset (*m);
    }

    // Listing: the QC pushes one File message per folder; it's lazy, so re-ask if nothing comes.
    progress ("Reading your setlists and presets...");
    session.take (typeFile);                           // drop anything pushed before our listing request
    std::map<juce::String, Folder> folders;
    juce::uint32 lastSetlistAt = 0;

    for (int attempt = 0; attempt < 3 && folders.empty() && ! lostOrCancelled(); ++attempt)
    {
        PbWriter list;
        list.fieldVarint (1, 3);                       // READ
        list.fieldVarint (2, ++rid);
        list.fieldVarint (3, 0);                       // presets only
        session.send (typeFile, list.bytes);

        const auto askedAt = juce::Time::getMillisecondCounter();
        while (! lostOrCancelled())
        {
            session.waitFor ([] (const Message&) { return false; }, 250);   // just wait a little
            for (const auto& m : session.take (typeFile))
                if (auto f = parseFolder (m))
                    if (isSetlist (*f) && f->fileCount > 0)
                    {
                        auto& slot = folders[f->key.trimCharactersAtEnd ("/")];
                        if (f->fileCount >= slot.fileCount)
                            slot = *f;
                        lastSetlistAt = juce::Time::getMillisecondCounter();
                    }

            const auto now = juce::Time::getMillisecondCounter();
            if (! folders.empty() && now - lastSetlistAt > 2000)
                break;                                 // 2 s quiet after the last setlist: done
            if (folders.empty() && now - askedAt > 12000)
                break;                                 // nothing yet: ask again
            if (now - askedAt > 30000)
                break;
        }
    }

    disconnect (session);

    if (cancel) return fail ("Cancelled.");
    if (session.isLost()) return fail ("The USB connection to the Quad Cortex was lost.");
    if (folders.empty())
        return fail ("The Quad Cortex didn't send its setlists. Wait a few seconds and try again.");

    // My Presets first, then the player's setlists by name, Factory Library last.
    for (auto& [key, f] : folders)
        result.setlists.push_back (f);
    std::stable_sort (result.setlists.begin(), result.setlists.end(), [] (const Folder& a, const Folder& b)
    {
        auto rank = [] (const Folder& f) { return f.isFactory ? 2 : (f.name == "My Presets" ? 0 : 1); };
        if (rank (a) != rank (b)) return rank (a) < rank (b);
        return a.name.compareIgnoreCase (b.name) < 0;
    });
    for (auto& f : result.setlists)
        std::sort (f.presets.begin(), f.presets.end(), [] (const Preset& a, const Preset& b) { return a.position < b.position; });

    result.ok = true;
    return result;
}

ScanResult scanPresets (const std::vector<ScanTarget>& targets, const std::function<void (const juce::String&, double)>& progress,
                        std::atomic<bool>& cancel)
{
    ScanResult result;
    auto fail = [&result] (const juce::String& why) { result.ok = false; result.error = why; return result; };

    const UsbScope usb;   // outlives the session below
    auto c = connect ([&progress] (const juce::String& t) { progress (t, 0.0); }, cancel);
    if (c.session == nullptr)
        return fail (c.error);
    auto& session = *c.session;
    juce::uint64 rid = 500;

    // Remember what's loaded (preset + scene), and refuse if it has unsaved changes: a load would discard them.
    const auto dirtyRid = ++rid;
    session.send (typePresetDirty, readRequest (dirtyRid));
    if (auto m = waitReply (session, typePresetDirty, dirtyRid, 5000))
    {
        bool dirty = false;
        forEachField (static_cast<const juce::uint8*> (m->payload.getData()), m->payload.getSize(), [&] (const PbField& f)
        {
            if (f.number == 3 && f.wireType == 0) dirty = f.value != 0;
        });
        if (dirty)
        {
            disconnect (session);
            return fail ("The preset loaded on the QC has unsaved changes. Save or discard them on the QC first: "
                         "reading every preset loads each one, which would throw those changes away.");
        }
    }

    const auto posRid = ++rid;
    session.send (typeSetlistPosition, readRequest (posRid));
    const auto original = [&]
    {
        if (auto m = waitReply (session, typeSetlistPosition, posRid, 5000))
            return parsePosition (*m);
        return Position {};
    }();

    int originalScene = -1;
    const auto sceneRid = ++rid;
    session.send (typeScene, readRequest (sceneRid));
    if (auto m = waitReply (session, typeScene, sceneRid, 5000))
        forEachField (static_cast<const juce::uint8*> (m->payload.getData()), m->payload.getSize(), [&] (const PbField& f)
        {
            if (f.number == 3 && f.wireType == 0) originalScene = (int) f.value;
        });

    // Load each preset (the same as choosing it on the QC) and read it back.
    auto recall = [&] (const juce::String& folderKey, bool isFactory, juce::uint32 position) -> std::optional<PresetDetails>
    {
        const auto r = ++rid;
        PbWriter w;
        w.fieldVarint (1, 1);                                          // UPDATE = recall
        w.fieldVarint (2, r);
        const auto key = folderKey.trimCharactersAtEnd ("/") + (isFactory ? "/" : "");   // factory key keeps its trailing slash
        w.fieldString (3, key);
        w.fieldVarint (4, position);
        w.fieldVarint (5, isFactory ? 1 : 0);
        session.send (typeSetlistPosition, w.bytes);
        if (auto m = waitReply (session, typeRecallPreset, r, 40000))
            return parseRecallPreset (*m);
        return {};
    };

    for (size_t i = 0; i < targets.size() && ! cancel && ! session.isLost(); ++i)
    {
        const auto& t = targets[i];
        progress ("Reading " + t.name + " (" + juce::String ((int) i + 1) + " of " + juce::String ((int) targets.size()) + ")...",
                  (double) i / (double) juce::jmax ((size_t) 1, targets.size()));
        if (auto details = recall (t.folderKey, t.isFactory, (juce::uint32) t.position))
            result.presets[{ t.folderKey.trimCharactersAtEnd ("/"), t.position }] = *details;
    }

    // Back to where the player was.
    if (original.valid && ! original.special && original.folderKey.isNotEmpty() && ! session.isLost())
    {
        progress ("Going back to the preset you were on...", 1.0);
        recall (original.folderKey, original.isFactory, original.position);
        if (originalScene >= 0)
        {
            PbWriter w;
            w.fieldVarint (1, 1);                                      // UPDATE: select the scene
            w.fieldVarint (2, ++rid);
            w.fieldVarint (3, (juce::uint64) originalScene);
            session.send (typeScene, w.bytes);
            std::this_thread::sleep_for (std::chrono::milliseconds (300));
        }
    }

    disconnect (session);
    if (session.isLost()) return fail ("The USB connection to the Quad Cortex was lost.");
    result.ok = true;
    return result;
}
} // namespace qcusb
