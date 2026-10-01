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
                if (m->type == typeFile || m->type == typeVersion || m->type == typeResetCommsBuffers)
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

juce::String randomHex32()
{
    juce::Random r;
    juce::String s;
    for (int i = 0; i < 32; ++i)
        s << juce::String::charToString ("0123456789abcdef"[r.nextInt (16)]);
    return s;
}
} // namespace

Result readSetlists (const std::function<void (const juce::String&)>& progress, std::atomic<bool>& cancel)
{
    Result result;
    auto fail = [&result] (const juce::String& why) { result.ok = false; result.error = why; return result; };

    if (hid_init() != 0)
        return fail ("USB access isn't available on this computer.");

    // Find the QC's HID interface (the only one, interface 5).
    juce::String path;
    bool mini = false;
    if (auto* list = hid_enumerate ((unsigned short) vendorId, 0))
    {
        for (auto* d = list; d != nullptr; d = d->next)
            if ((d->product_id == productIdQc || d->product_id == productIdMini) && (d->interface_number == 5 || d->usage_page == 0x0001))
            {
                path = d->path;
                mini = d->product_id == productIdMini;
                break;
            }
        hid_free_enumeration (list);
    }
    if (path.isEmpty())
        return fail ("No Quad Cortex found on USB. Connect the QC's USB port to this computer, switch it on and try again.");

   #if JUCE_MAC
    hid_darwin_set_open_exclusive (0);   // don't lock the pedal away from other apps
   #endif
    auto* dev = hid_open_path (path.toRawUTF8());
    if (dev == nullptr)
        return fail ("Couldn't open the Quad Cortex over USB. Quit Cortex Control (it keeps the USB connection to itself) and try again.");

    result.isMini = mini;
    progress ("Connecting to the Quad Cortex...");
    Session session (dev);

    const auto start = juce::Time::getMillisecondCounter();
    auto elapsed = [start] { return (int) (juce::Time::getMillisecondCounter() - start); };
    auto lostOrCancelled = [&] { return cancel.load() || session.isLost(); };

    // 1. Identity: ask for the version until the full reply (device type + CorOS version) arrives.
    while (result.corosVersion.isEmpty() && elapsed() < 30000 && ! lostOrCancelled())
    {
        session.send (typeVersion, readRequest());
        if (auto v = session.waitFor ([] (const Message& m) { return m.type == typeVersion; }, 5000))
            forEachField (static_cast<const juce::uint8*> (v->payload.getData()), v->payload.getSize(), [&] (const PbField& f)
            {
                if (f.number == 4 && f.wireType == 2) result.corosVersion = f.text();
                if (f.number == 12 && f.wireType == 0) result.isMini = result.isMini || f.value == 1;
            });
    }
    if (cancel) return fail ("Cancelled.");
    if (result.corosVersion.isEmpty())
        return fail ("The Quad Cortex didn't answer over USB. Quit Cortex Control, wait until the QC has fully started, and try again.");

    // 2. Session start: reset the comms buffers, announce ourselves the way Cortex Control does, subscribe.
    PbWriter reset;
    reset.fieldVarint (1, 0);
    reset.fieldString (2, randomHex32());
    bool resetAcked = false;
    while (! resetAcked && elapsed() < 60000 && ! lostOrCancelled())
    {
        session.send (typeResetCommsBuffers, reset.bytes);
        resetAcked = session.waitFor ([] (const Message& m) { return m.type == typeResetCommsBuffers; }, 5000).has_value();
    }
    if (cancel) return fail ("Cancelled.");
    if (! resetAcked)
        return fail ("The Quad Cortex didn't start a USB session. Unplug the USB cable, plug it back in and try again.");

    PbWriter announce;
    announce.fieldVarint (1, 1);                       // UPDATE
    announce.fieldString (11, result.corosVersion);    // cortex_control_version: mirror the unit's version
    session.send (typeVersion, announce.bytes);

    PbWriter connect;
    connect.fieldVarint (2, 1);                        // connected: true
    session.send (typeConnection, connect.bytes);

    for (const auto type : { typeModelRepo, typeModuleStats, typeUndoRedo, typeIOSettings, typeGeneralSettings, typeMode,
                             typeGlobalEQ, typeMasterVolume, typeGlobalTempo, typeScene, typePresetDirty, typeSetlistPosition })
        session.send (type, readRequest());            // all READs: nothing here changes the pedal

    std::this_thread::sleep_for (std::chrono::seconds (2));
    session.take (typeFile);                           // drop anything pushed before our listing request

    // 3. Listing: the QC pushes one File message per folder; it's lazy, so re-ask if nothing comes.
    progress ("Reading your setlists and presets...");
    std::map<juce::String, Folder> folders;
    juce::uint32 lastSetlistAt = 0;

    for (int attempt = 0; attempt < 3 && folders.empty() && ! lostOrCancelled(); ++attempt)
    {
        PbWriter list;
        list.fieldVarint (1, 3);                       // READ
        list.fieldVarint (2, (juce::uint64) (100 + attempt));
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

    PbWriter goodbye;
    goodbye.fieldVarint (2, 0);                        // connected: false
    session.send (typeConnection, goodbye.bytes);

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
} // namespace qcusb
