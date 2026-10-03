#include "../Source/CueModel.h"
#include "../Source/Update.h"
#include "../Source/State.h"
#include "../Source/QcUsb.h"
#include "../Source/Fuzzy.h"
#include "../Source/DeviceTemplates.h"
#include "../Source/Modellers.h"

#include <cstdio>

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static bool isCC (const juce::MidiMessage& m, int ch, int cc, int v)
{
    return m.isController() && m.getChannel() == ch && m.getControllerNumber() == cc && m.getControllerValue() == v;
}


// Reads a fixture of hex reports (one per line) captured from a real Quad Cortex.
static std::vector<std::vector<juce::uint8>> readHexReports (const char* name)
{
    std::vector<std::vector<juce::uint8>> reports;
    juce::StringArray lines;
    juce::File (PEDALCUES_TEST_FIXTURES).getChildFile (name).readLines (lines);
    for (const auto& line : lines)
    {
        if (line.trim().isEmpty())
            continue;
        std::vector<juce::uint8> r;
        for (int i = 0; i + 1 < line.length(); i += 2)
            r.push_back ((juce::uint8) line.substring (i, i + 2).getHexValue32());
        reports.push_back (r);
    }
    return reports;
}

static std::optional<qcusb::Message> reassemble (const std::vector<std::vector<juce::uint8>>& reports)
{
    qcusb::Reassembler r;
    std::optional<qcusb::Message> out;
    for (const auto& rep : reports)
        if (auto m = r.feed (rep.data(), (int) rep.size()))
            out = m;
    return out;
}

int main (int argc, char** argv)
{
    // Optional online check: PedalCuesTests --online asks GitHub for the latest release.
    if (argc > 1 && juce::String (argv[1]) == "--online")
    {
        const auto info = update::fetchLatest();
        std::printf ("status=%d latest=%s download=%s\n", (int) info.status, info.latest.toRawUTF8(), info.downloadUrl.toRawUTF8());
        return info.status == update::Info::Status::failed ? 1 : 0;
    }

    // Hardware diagnostics: PedalCuesTests --qc-setlists lists the QC's setlists in the order it sends them,
    // with any folder fields PedalCues doesn't decode (to find how the QC numbers its setlists). Quit Cortex Control first.
    if (argc > 1 && juce::String (argv[1]) == "--qc-setlists")
    {
        std::atomic<bool> cancel { false };
        const auto r = qcusb::readSetlists ([] (const juce::String& s) { std::printf ("  %s\n", s.toRawUTF8()); }, cancel);
        if (! r.ok)
        {
            std::printf ("failed: %s\n", r.error.toRawUTF8());
            return 1;
        }
        auto byArrival = r.setlists;
        std::sort (byArrival.begin(), byArrival.end(), [] (const auto& a, const auto& b) { return a.arrival < b.arrival; });
        std::printf ("CorOS %s, %d setlists, in the order the QC sent them:\n", r.corosVersion.toRawUTF8(), (int) byArrival.size());
        for (const auto& f : byArrival)
            std::printf ("  #%d  %-24s %3d presets  key=%s%s  %s\n", f.arrival, f.name.toRawUTF8(), (int) f.presets.size(),
                         f.key.toRawUTF8(), f.isFactory ? "  (factory)" : "", f.otherFields.joinIntoString (" ").toRawUTF8());
        return 0;
    }

    using namespace cues;

    // Quad Cortex scene C on channel 1 -> CC43 = 2
    auto s = qc::scene (1, 2, "Chorus");
    CHECK (s.events.size() == 1 && isCC (s.events[0].second, 1, 43, 2));
    CHECK (s.name == "QC Scene C - Chorus");

    // Preset bank 3 slot B (index 17) -> CC0 = 0, PC 17; bank 20 slot A (index 152) -> CC0 = 1, PC 24
    auto p = qc::preset (1, 1, 3, 1, false, "Lead");
    CHECK (p.events.size() == 2 && isCC (p.events[0].second, 1, 0, 0));
    CHECK (p.events[1].second.isProgramChange() && p.events[1].second.getProgramChangeNumber() == 17);
    auto p2 = qc::preset (1, 4, 20, 0, true, {});
    CHECK (p2.events.size() == 3 && isCC (p2.events[0].second, 1, 0, 1) && isCC (p2.events[1].second, 1, 32, 4));   // setlist 4 -> CC#32 = 4
    CHECK (isCC (qc::preset (1, 0, 1, 0, true, {}).events[1].second, 1, 32, 0));   // 0 = Factory Presets
    CHECK (qc::location (0, 2, 3) == "Factory | 2D" && qc::location (3, 2, 3) == "SL3 | 2D");
    CHECK (p2.events[2].second.getProgramChangeNumber() == 24);

    CHECK (isCC (qc::tuner (1, true).events[0].second, 1, 45, 127));
    CHECK (isCC (qc::stomp (1, 7, false, {}).events[0].second, 1, 42, 0));
    CHECK (isCC (qc::gigMode (1, 1).events[0].second, 1, 47, 2)); // scene
    CHECK (isCC (qc::gigView (1, true).events[0].second, 1, 46, 127) && isCC (qc::gigView (3, false).events[0].second, 3, 46, 0));
    CHECK (qc::gigView (1, true).name == "QC Gig View On");

    // Clips with every event on the first tick are padded for Ableton, never with a second Program Change.
    {
        CHECK (isCC (lengthPadding (qc::scene (1, 2, {})), 1, 43, 2));                                  // a CC is repeated as is
        CHECK (isCC (lengthPadding (qc::preset (1, 3, 2, 1, true, {})), 1, 32, 3));                    // QC preset: its setlist CC
        CHECK (isCC (lengthPadding (qc::preset (1, 3, 2, 1, false, {})), 1, 0, 0));                    // ... or its bank CC
        CHECK (isCC (lengthPadding (custom::cue (2, "x", "PC 5", 0)), 2, 0, 0));                       // PC only: a bank select of 0
    }

    // Fuzzy search for preset lists.
    {
        const juce::StringArray names { "Clean Rig", "Plexi Crunch", "Drop C Heavy", "Lead Rig", "Ambient Pads" };
        const juce::StringArray where { "SL1 | 1A", "SL1 | 1B", "SL1 | 2B", "SL2 | 1C", "SL1 | 2A" };
        CHECK (fuzzy::order ({}, names, where).size() == 5 && fuzzy::order ({}, names, where)[0] == 0);   // empty: everything, in order
        CHECK (fuzzy::order ("drop", names, where) == std::vector<int> { 2 });
        CHECK (fuzzy::order ("drpc", names, where).front() == 2);                              // letters in order
        CHECK (fuzzy::order ("hevy", names, where) == std::vector<int> { 2 });                 // one typo
        CHECK (fuzzy::order ("crnch", names, where) == std::vector<int> { 1 });
        CHECK (fuzzy::order ("rig", names, where).size() == 2);                                // Clean Rig, Lead Rig
        CHECK (fuzzy::order ("lead rig", names, where) == std::vector<int> { 3 });             // every word must match
        CHECK (fuzzy::order ("sl2", names, where) == std::vector<int> { 3 } && fuzzy::order ("2b", names, where) == std::vector<int> { 2 });   // location, as typed
        CHECK (fuzzy::order ("cl", names, where) == std::vector<int> { 0 });                   // no scattered letters from "SL1"
        CHECK (fuzzy::order ("xyzzy", names, where).empty());
        CHECK (fuzzy::score ("cr", "Crunch") > fuzzy::score ("cr", "Clean Rig"));               // unbroken run beats scattered letters
    }

    // Custom units (beta): typed message lists.
    {
        auto pr = custom::parse ("PC 5, CC 34=2; bank 1\nCC#11 127, program 3", 0);
        CHECK (pr.ok() && pr.steps.size() == 5);
        CHECK (custom::describe (pr, 0) == "PC 5, CC#34 = 2, bank 1, CC#11 = 127, PC 3");
        CHECK (custom::parse (custom::describe (pr, 0), 0).steps.size() == 5);   // what the editor saves reads back
        auto c = custom::cue (3, "Rig Verse", "bank 1, PC 5, CC 34=2", 0);
        CHECK (c.name == "Rig Verse" && c.events.size() == 3);
        CHECK (isCC (c.events[0].second, 3, 0, 1) && c.events[0].first == 0.0);
        CHECK (c.events[1].second.isProgramChange() && c.events[1].second.getProgramChangeNumber() == 5);
        CHECK (isCC (c.events[2].second, 3, 34, 2) && c.events[2].first == 0.0);
        // Counting from 1: PC 1 is program 0, and PC 0 is out of range.
        CHECK (custom::cue (1, "x", "PC 1", 1).events[0].second.getProgramChangeNumber() == 0);
        CHECK (custom::describe (custom::parse ("PC 1", 1), 1) == "PC 1");
        CHECK (! custom::parse ("PC 0", 1).ok() && custom::parse ("PC 0", 1).error.contains ("1 to 128"));
        CHECK (! custom::parse ("CC 128=1", 0).ok() && ! custom::parse ("hello", 0).ok() && ! custom::parse ("", 0).ok());
        CHECK (! custom::parse ("CC 34", 0).ok() && ! custom::parse ("wait", 0).ok());
    }
    CHECK (isCC (qc::gigMode (1, 2).events[0].second, 1, 47, 1)); // stomp

    // Whammy V program numbers (manual, 1-based)
    CHECK (whammy::programNumber (1, false, false) == 2);   // Oct Up
    CHECK (whammy::programNumber (1, false, true)  == 23);  // Oct Up bypassed
    CHECK (whammy::programNumber (8, true, false)  == 51);  // 2 Oct Down, Chords
    CHECK (whammy::programNumber (8, true, true)   == 72);
    CHECK (whammy::programNumber (20, true, true)  == 84);

    // A saved setup carries the MIDI settings along with the names.
    {
        auto root = state::createDefault();
        root.setProperty (IDs::qcChannel, 5, nullptr);
        root.setProperty (IDs::whChannel, 7, nullptr);
        root.setProperty (IDs::sendSetlist, true, nullptr);
        const auto file = juce::File::createTempFile (".xml");
        CHECK (state::saveLibrary (root, file));

        auto fresh = state::createDefault();
        CHECK (state::loadLibrary (fresh, file));
        CHECK ((int) fresh[IDs::qcChannel] == 5 && (int) fresh[IDs::whChannel] == 7 && (bool) fresh[IDs::sendSetlist]);
        file.deleteFile();
    }

    // Custom units (beta): kept in the setup, exported and imported as their own file, names kept unique.
    {
        auto root = state::createDefault();
        CHECK (! state::customUnit (root).isValid());
        root.setProperty (IDs::ampUnit, state::customAmpUnit, nullptr);
        state::sanitise (root);
        CHECK ((int) root[IDs::ampUnit] == 0);   // custom with no unit falls back to the Quad Cortex

        auto unit = state::addCustomUnit (root, state::createCustomUnit ("Axe-Fx II"));
        unit.setProperty (IDs::notes, "Scenes use CC 34 on my unit.", nullptr);
        unit.getChild (0).getChild (0).setProperty (IDs::messages, "bank 1, PC 7", nullptr);
        root.setProperty (IDs::ampUnit, state::customAmpUnit, nullptr);
        CHECK (state::customUnit (root) == unit && unit.getNumChildren() == 2);

        const auto again = state::addCustomUnit (root, state::createCustomUnit ("Axe-Fx II"));
        CHECK (again[IDs::name].toString() == "Axe-Fx II (2)" && (int) root[IDs::selectedCustomUnit] == 1);
        root.setProperty (IDs::selectedCustomUnit, 0, nullptr);

        const auto file = juce::File::createTempFile (".xml");
        CHECK (state::saveLibrary (root, file));
        auto fresh = state::createDefault();
        CHECK (state::loadLibrary (fresh, file));
        CHECK ((int) fresh[IDs::ampUnit] == state::customAmpUnit && state::customUnit (fresh)[IDs::notes].toString() == "Scenes use CC 34 on my unit.");
        file.deleteFile();

        const auto unitFile = juce::File::createTempFile (".pedalcues-unit");
        CHECK (state::saveUnit (unit, unitFile));
        const auto loaded = state::loadUnit (unitFile);
        CHECK (loaded.isValid() && loaded[IDs::name].toString() == "Axe-Fx II"
               && loaded.getChild (0).getChild (0)[IDs::messages].toString() == "bank 1, PC 7");
        CHECK (! state::loadUnit (juce::File::createTempFile (".xml")).isValid());
        unitFile.deleteFile();
    }

    // Fractal / Line 6 pages: preset numbering, timing and numbers from the manuals.
    {
        using namespace modellers;
        CHECK (all().size() == 11);
        for (const auto& p : all())
        {
            for (const auto& list : { p.utilities, p.looper })
                for (const auto& a : list)
                    CHECK (custom::parse (a.messages, 0).ok());
            CHECK (custom::parse (p.tunerOn, 0).ok() && custom::parse (p.tunerOff, 0).ok() && ! p.notes.isEmpty() && find (p.id) == &p);
            const auto last = presetsPerSetlist (p, defaultSetlist (p)) - 1;
            const auto c = preset (p, 1, defaultSetlist (p), last, true, {});
            CHECK (c.events.back().second.isProgramChange());
        }
        const auto& helix = *find ("line6.helix-floor");
        CHECK (presetLabel (helix, 2, 0) == "01A" && presetLabel (helix, 2, 5) == "02B" && presetLabel (helix, 2, 127) == "32D");
        CHECK (setlistLabel (helix, 2) == "USER 1" && setlistLabel (helix, 7) == "TEMPLATES");
        auto h = preset (helix, 1, 2, 5, true, "Clean");
        CHECK (h.name == "Helix Floor Clean" && h.events.size() == 2 && isCC (h.events[0].second, 1, 32, 2) && h.events[1].second.getProgramChangeNumber() == 5);
        CHECK (preset (helix, 1, 2, 5, false, {}).events.size() == 1);                                   // setlist only when switched on
        auto hs = sceneAfterPreset (helix, 1, 2, 5, false, "Clean", 1, "Verse");
        CHECK (hs.events.size() == 2 && isCC (hs.events[1].second, 1, 69, 1) && hs.events[1].first == 0.0);   // Line 6: same tick
        CHECK (isCC (switchCue (helix, 1, 5, true, {}).events[0].second, 1, 54, 127));                    // 6th entry = FS7
        const auto& stomp = *find ("line6.hx-stomp");
        CHECK (stomp.sceneCount == 3 && presetLabel (stomp, -1, 125) == "42C" && presetsPerSetlist (stomp, -1) == 126);
        const auto& stadium = *find ("line6.helix-stadium");
        CHECK (presetLabel (stadium, 2, 0) == "33A" && presetLabel (stadium, 1, 0) == "01A");
        CHECK (custom::parse (stadium.utilities[0].messages, 0).steps[0].number == 9 && custom::parse (stadium.utilities[0].messages, 0).steps[0].value == 34);
        CHECK (! find ("line6.pod-go")->hasDin);
        const auto& axe = *find ("fractal.axe-fx-2");
        CHECK (presetLabel (axe, -1, 0) == "A000" && presetLabel (axe, -1, 130) == "B002");
        auto a = sceneAfterPreset (axe, 3, -1, 130, true, "Lead", 2, {});
        CHECK (a.events.size() == 3 && isCC (a.events[0].second, 3, 0, 1) && a.events[1].second.getProgramChangeNumber() == 2
               && isCC (a.events[2].second, 3, 34, 2) && a.events[2].first == fractalGap);              // Fractal: 1/16 later
        CHECK (isCC (switchCue (axe, 1, 0, false, {}).events[0].second, 1, 37, 0) && switchCue (axe, 1, 0, true, {}).name == "Axe-Fx II Amp 1 On");
        const auto& ax8 = *find ("fractal.ax8");
        CHECK (presetLabel (ax8, -1, 128) == "17:1" && isCC (preset (ax8, 1, -1, 128, false, {}).events[0].second, 1, 0, 1));
        const auto& fx8 = *find ("fractal.fx8");
        CHECK (presetLabel (fx8, -1, 9) == "B2" && preset (fx8, 1, -1, 9, true, {}).events.size() == 1);   // no bank select
    }

    // Fractal / Line 6 page data: one subtree per model, kept in the setup, unknown models fall back to the QC.
    {
        auto root = state::createDefault();
        auto helix = state::modeller (root, "line6.helix-floor");
        CHECK (helix.isValid() && helix.getChildWithName (IDs::ModPreset).isValid());
        const auto first = helix.getChildWithName (IDs::ModPreset);
        CHECK ((int) first[IDs::setlist] == 2 && first.getChildWithName (IDs::Scene).isValid()
               && first[IDs::name].toString() == "Preset 01A");
        int scenes = 0, switches = 0;
        for (auto c : first) scenes += c.hasType (IDs::Scene) ? 1 : 0;
        for (auto c : helix) switches += c.hasType (IDs::ModSwitch) ? 1 : 0;
        CHECK (scenes == 8 && switches == 10);
        CHECK (state::modeller (root, "line6.helix-floor") == helix);                          // the same one again
        CHECK (! state::modeller (root, "nope").isValid());
        first.getChildWithName (IDs::Scene).setProperty (IDs::name, "Verse", nullptr);
        root.setProperty (IDs::ampUnit, state::modellerAmpUnit, nullptr);
        root.setProperty (IDs::modellerProfile, "line6.helix-floor", nullptr);
        const auto file = juce::File::createTempFile (".xml");
        CHECK (state::saveLibrary (root, file));
        auto fresh = state::createDefault();
        CHECK (state::loadLibrary (fresh, file));
        CHECK ((int) fresh[IDs::ampUnit] == state::modellerAmpUnit
               && state::modeller (fresh, "line6.helix-floor").getChildWithName (IDs::ModPreset).getChildWithName (IDs::Scene)[IDs::name].toString() == "Verse");
        file.deleteFile();
        fresh.setProperty (IDs::modellerProfile, "gone.model", nullptr);
        state::sanitise (fresh);
        CHECK ((int) fresh[IDs::ampUnit] == 0);
    }

    // Device templates (Fractal, Line 6): every tile's messages read back, names are unique in each group,
    // and a few numbers straight from the manuals.
    {
        CHECK (templates::all().size() == 4);   // Axe-Fx III, FM9, FM3, VP4: no default CCs, so editable devices
        juce::StringArray ids;
        for (const auto& t : templates::all())
        {
            CHECK (! ids.contains (t.id) && t.notes.contains ("Not tested on hardware"));
            ids.add (t.id);
            const auto unit = templates::createUnit (t);
            CHECK (unit[IDs::templateId].toString() == t.id && unit.getNumChildren() == (int) t.groups.size());
            for (auto g : unit)
            {
                juce::StringArray names;
                for (auto tile : g)
                {
                    const auto parsed = custom::parse (tile[IDs::messages].toString(), 0);
                    if (! parsed.ok())
                        std::printf ("  %s / %s: %s\n", t.id.toRawUTF8(), tile[IDs::name].toString().toRawUTF8(), parsed.error.toRawUTF8());
                    CHECK (parsed.ok() && ! names.contains (tile[IDs::name].toString()));
                    names.add (tile[IDs::name].toString());
                }
            }
        }
        auto tileOf = [] (const juce::String& id, const juce::String& group, const juce::String& name)
        {
            for (auto g : templates::createUnit (*templates::find (id)))
                if (g[IDs::name].toString() == group)
                    for (auto t : g)
                        if (t[IDs::name].toString() == name)
                            return custom::cue (1, name, t[IDs::messages].toString(), 0);
            return Cue {};
        };
        CHECK (isCC (tileOf ("fractal.axe-fx-3", "Scenes", "Scene 3").events[0].second, 1, 34, 2));
        const auto axe3Preset = tileOf ("fractal.axe-fx-3", "Presets", "Preset 128");
        CHECK (isCC (axe3Preset.events[0].second, 1, 0, 1) && axe3Preset.events[1].second.getProgramChangeNumber() == 0);
        CHECK (tileOf ("fractal.vp4", "Presets", "A1").events.size() == 1);                                 // VP4: no bank select
        CHECK (templates::find ("fractal.fm3")->notes.contains ("can't be controlled over USB"));
        CHECK (templates::find ("fractal.axe-fx-3")->notes.startsWith ("Why this is an editable device"));

        // The template id survives export and import, so the disclaimer follows the device.
        const auto file = juce::File::createTempFile (".pedalcues-device");
        CHECK (state::saveUnit (templates::createUnit (*templates::find ("fractal.fm3")), file));
        CHECK (state::loadUnit (file)[IDs::templateId].toString() == "fractal.fm3");
        file.deleteFile();
    }

    // Quad Cortex USB (read-only sync): framing and decoding, checked against real captures.
    {
        // Encoding matches Cortex Control's Version READ byte for byte.
        qcusb::PbWriter w;
        w.fieldVarint (1, 3);
        const auto reports = qcusb::encodeMessage (qcusb::typeVersion, w.bytes);
        const auto captured = readHexReports ("qc_version_read.hex");
        CHECK (reports.size() == 1 && captured.size() == 1 && captured[0].size() == 129);
        CHECK (std::equal (reports[0].begin(), reports[0].end(), captured[0].begin()));

        // A multi-report reply reassembles; the CorOS version is field 4.
        const auto version = reassemble (readHexReports ("qc_version_reply.hex"));
        CHECK (version.has_value() && version->type == qcusb::typeVersion);
        juce::String coros;
        if (version)
            qcusb::forEachField (static_cast<const juce::uint8*> (version->payload.getData()), version->payload.getSize(),
                                 [&] (const qcusb::PbField& f) { if (f.number == 4 && f.wireType == 2) coros = f.text(); });
        CHECK (coros == "4.0.1");

        // An empty Downloads folder push: parsed, but not a setlist.
        const auto plain = reassemble (readHexReports ("qc_file_reply_plain.hex"));
        CHECK (plain.has_value());
        const auto downloads = plain ? qcusb::parseFolder (*plain) : std::nullopt;
        CHECK (downloads.has_value() && downloads->key == "cloud-0-1" && downloads->fileCount == 0 && ! qcusb::isSetlist (*downloads));

        // A gzipped folder push over 33 reports (the IR folder): gunzipped and decoded, not a setlist.
        const auto gz = reassemble (readHexReports ("qc_file_reply_gzip.hex"));
        const auto irs = gz ? qcusb::parseFolder (*gz) : std::nullopt;
        CHECK (irs.has_value() && irs->key == "/opt/neuraldsp/impulse_responses" && irs->fileCount == 588 && ! qcusb::isSetlist (*irs));

        // Encrypted messages are flagged so they can be skipped.
        const auto licence = reassemble (readHexReports ("qc_license_encrypted.hex"));
        CHECK (licence.has_value() && licence->encrypted && ! qcusb::parseFolder (*licence).has_value());

        // A setlist push (synthetic): slots by index, empty slots dropped, round-tripped through the framing.
        qcusb::PbWriter preset1, emptySlot, preset2, folder, file;
        preset1.fieldVarint (2, 218); preset1.fieldString (3, "Lead");
        emptySlot.fieldVarint (2, 5);
        preset2.fieldVarint (2, 0); preset2.fieldString (3, "Clean Rig");
        folder.fieldString (1, "/media/p4/Presets/My Presets");
        folder.fieldString (3, "My Presets");
        for (auto* p : { &preset1, &emptySlot, &preset2 })
        {
            folder.varint ((7 << 3) | 2);
            folder.varint (p->bytes.size());
            folder.bytes.insert (folder.bytes.end(), p->bytes.begin(), p->bytes.end());
        }
        file.fieldVarint (1, 1);
        file.varint ((4 << 3) | 2);
        file.varint (folder.bytes.size());
        file.bytes.insert (file.bytes.end(), folder.bytes.begin(), folder.bytes.end());

        qcusb::Reassembler r;
        std::optional<qcusb::Message> m;
        for (auto rep : qcusb::encodeMessage (qcusb::typeFile, file.bytes))
        {
            rep[0] = 0x01;   // as if it came from the QC
            if (auto got = r.feed (rep.data(), (int) rep.size()))
                m = got;
        }
        const auto setlist = m ? qcusb::parseFolder (*m) : std::nullopt;
        CHECK (setlist.has_value() && qcusb::isSetlist (*setlist) && setlist->name == "My Presets" && setlist->fileCount == 3);
        CHECK (setlist && setlist->presets.size() == 2 && setlist->presets[0].position == 218 && setlist->presets[0].name == "Lead");
        CHECK (qcusb::bankOf (218) == 28 && qcusb::slotOf (218) == 2);   // "28C"
        CHECK (qcusb::isSetlist (qcusb::Folder { "/opt/neuraldsp/Factory Library/", "Factory Library", true, {}, 1 }));

        // A real preset from a QC: name, 8 scene labels and their ARGB colours.
        juce::MemoryBlock presetBytes;
        juce::File (PEDALCUES_TEST_FIXTURES).getChildFile ("qc_scene_preset.bin").loadFileAsData (presetBytes);
        const auto real = qcusb::parsePresetDetails (static_cast<const juce::uint8*> (presetBytes.getData()), presetBytes.getSize());
        CHECK (real.has_value() && real->name == "Scene Fixture" && real->sceneCount == 8);
        CHECK (real && real->sceneNames[0] == "Scene A" && real->sceneNames[7] == "Scene H");
        CHECK (real && real->sceneColours[0] == 0xFFFF2727u && real->sceneColours[3] == 0xFFFF02C2u && real->sceneColours[7] == 0xFF00FFDDu);

        // A loaded-preset reply (synthetic, from the spec): unlabelled scene, stomp labels, single-block label wins.
        const char* hex = "08011007" "1a39" "12044c656164" "3801" "7a05436c65616e" "7a0120" "da010410031804"
                          "fa010aa7cefcff0fe0e9a9f80f" "8202070804120344 6c79" "92020608001202 4f44" "2000";
        std::vector<juce::uint8> bytes;
        const auto clean = juce::String (hex).removeCharacters (" ");
        for (int i = 0; i + 1 < clean.length(); i += 2)
            bytes.push_back ((juce::uint8) clean.substring (i, i + 2).getHexValue32());
        qcusb::Message recall;
        recall.type = qcusb::typeRecallPreset;
        recall.payload.append (bytes.data(), bytes.size());
        const auto lead = qcusb::parseRecallPreset (recall);
        CHECK (lead.has_value() && lead->name == "Lead" && lead->sceneCount == 2);
        CHECK (lead && lead->sceneNames[0] == "Clean" && lead->sceneNames[1].isEmpty());
        CHECK (lead && lead->sceneColours[1] == 0xFF0A74E0u && lead->stompNames[4] == "Dly" && lead->stompNames[0] == "OD");
    }

    // Update check: version comparison
    CHECK (update::isNewer ("0.4.12", "0.4.11"));
    CHECK (update::isNewer ("v0.5.0", "0.4.12"));
    CHECK (update::isNewer ("0.4.10", "0.4.9"));
    CHECK (! update::isNewer ("0.4.11", "0.4.11"));
    CHECK (! update::isNewer ("0.4.9", "0.4.10"));
    CHECK (update::isNewer ("1.0", "0.9.9"));

    auto e = whammy::effect (2, 1, "Oct Up", false, false, 1, true);
    CHECK (e.events.size() == 2 && isCC (e.events[0].second, 2, 11, 0));
    CHECK (e.events[1].second.getChannel() == 2 && e.events[1].second.getProgramChangeNumber() == 1);

    // Kemper (MIDI Parameter Documentation OS 11 + main manual). Performance 1 Slot 1 = bank 0 + PC 0,
    // Performance 26 Slot 3 = bank 0 + PC 127, Performance 26 Slot 4 = bank 1 + PC 0. Player: PC only, 50 slots.
    {
        using kemper::Unit;
        auto first = kemper::slot (1, Unit::profiler, 1, 0, "Clean");
        CHECK (first.name == "Kemper P1.1 - Clean" && first.events.size() == 2);
        CHECK (isCC (first.events[0].second, 1, 32, 0) && first.events[1].second.getProgramChangeNumber() == 0);
        auto p26s3 = kemper::slot (1, Unit::profiler, 26, 2, {});
        CHECK (isCC (p26s3.events[0].second, 1, 32, 0) && p26s3.events[1].second.getProgramChangeNumber() == 127);
        auto p26s4 = kemper::slot (1, Unit::profiler, 26, 3, {});
        CHECK (isCC (p26s4.events[0].second, 1, 32, 1) && p26s4.events[1].second.getProgramChangeNumber() == 0);
        auto last = kemper::slot (1, Unit::profiler, 125, 4, {});
        CHECK (isCC (last.events[0].second, 1, 32, 4) && last.events[1].second.getProgramChangeNumber() == 624 - 512);
        auto player = kemper::slot (3, Unit::player, 10, 4, {});
        CHECK (player.events.size() == 1 && player.events[0].second.getChannel() == 3 && player.events[0].second.getProgramChangeNumber() == 49);

        CHECK (isCC (kemper::slotOfCurrent (1, 2, {}).events[0].second, 1, 52, 1));
        CHECK (isCC (kemper::effect (1, 0, true, false, {}).events[0].second, 1, 17, 1));
        CHECK (isCC (kemper::effect (1, 4, false, false, {}).events[0].second, 1, 22, 0));
        CHECK (kemper::effectController (6, false) == 26 && kemper::effectController (6, true) == 27);
        CHECK (kemper::effectController (7, false) == 28 && kemper::effectController (7, true) == 29);
        CHECK (isCC (kemper::tuner (1, true).events[0].second, 1, 31, 1) && isCC (kemper::morph (1, false).events[0].second, 1, 80, 0));
        auto taps = kemper::tapTempo (1, 4);
        CHECK (taps.events.size() == 4 && taps.events[3].first == 3.0 && isCC (taps.events[0].second, 1, 30, 0));
        CHECK (kemper::pedalController (0) == 1 && kemper::pedalController (3) == 11);

        auto chained = kemper::withSlotFirst (kemper::effect (1, 6, true, true, {}), 1, Unit::profiler, 2, 1, "Live");
        CHECK (chained.name == "Kemper Live 2 > Delay On" && chained.events.size() == 3);
        CHECK (chained.events[1].second.getProgramChangeNumber() == 6 && isCC (chained.events[2].second, 1, 27, 1));
        CHECK (std::abs (chained.events[2].first - 0.25) < 1.0e-9);

        auto wah = qc::shapedMove ("Kemper Wah ", 1, kemper::pedalController (0), qc::ExpShape::swellIn, 4.0, 1.0, false);
        CHECK (wah.name == "Kemper Wah Swell In 1 bar" && isCC (wah.events.back().second, 1, 1, 127));
    }

    // Whammy DT Drop Tune (manual page 13): Shift Up 1 = 43 ... Oct+Dry = 51, Shift Down 1 = 60 ... Oct+Dry = 52,
    // bypassed = +18. Sent 1-based minus the numbering base (Program Change 42 for "43" by default).
    CHECK (whammy::dropTuneProgram (true, 0, false) == 43 && whammy::dropTuneProgram (true, 8, false) == 51);
    CHECK (whammy::dropTuneProgram (true, 0, true) == 61 && whammy::dropTuneProgram (true, 8, true) == 69);
    CHECK (whammy::dropTuneProgram (false, 0, false) == 60 && whammy::dropTuneProgram (false, 8, false) == 52);
    CHECK (whammy::dropTuneProgram (false, 0, true) == 78 && whammy::dropTuneProgram (false, 8, true) == 70);
    {
        auto d = whammy::dropTune (2, false, 1, false, 1);
        CHECK (d.name == "Whammy DT Shift Down 2" && d.events.size() == 1);
        CHECK (d.events[0].second.getChannel() == 2 && d.events[0].second.getProgramChangeNumber() == 58);
        CHECK (whammy::dropTune (2, true, 8, true, 1).name == "Whammy DT Shift Up Oct + Dry (Bypass)");
        CHECK (whammy::shiftName (7) == "Oct" && whammy::shiftName (2) == "3");
    }

    // Sweeps
    auto up = whammy::sweep (2, whammy::Shape::rampUp, 4.0, 1.0, true);
    CHECK (isCC (up.events.front().second, 2, 11, 0));
    bool reachedToe = false, monotonic = true;
    int last = -1;
    for (auto& [beat, m] : up.events)
    {
        if (m.getControllerValue() == 127) reachedToe = true;
        if (beat <= 4.0 && m.getControllerValue() < last) monotonic = false;
        if (beat <= 4.0) last = m.getControllerValue();
    }
    CHECK (reachedToe && monotonic);
    CHECK (isCC (up.events.back().second, 2, 11, 0) && up.events.back().first > 4.0);
    CHECK (up.lengthBeats > 4.0);

    auto trill = whammy::sweep (2, whammy::Shape::trill, 1.0, 1.0, false);
    CHECK (trill.events.size() == 4); // 127,0,127,0 on 1/16s

    auto bend = whammy::sweep (2, whammy::Shape::bendToBar, 4.0, 1.0, false);
    CHECK (bend.events.front().second.getControllerValue() == 0);
    CHECK (bend.events[1].first >= 3.0);

    // Drawn moves
    {
        std::vector<float> pts { 0.0f, 1.0f, 0.0f };
        auto d = whammy::drawn (3, pts, 2.0, false);
        CHECK (d.name == "Whammy Drawn 2 beats");
        CHECK (isCC (d.events.front().second, 3, 11, 0) && d.events.front().first == 0.0);
        int peak = 0; double peakBeat = 0.0;
        for (auto& [beat, m] : d.events)
            if (m.getControllerValue() > peak) { peak = m.getControllerValue(); peakBeat = beat; }
        CHECK (peak == 127 && std::abs (peakBeat - 1.0) < 0.05);
        CHECK (d.events.back().second.getControllerValue() == 0);

        auto held = whammy::drawn (3, { 1.0f, 1.0f }, 1.0, true);
        CHECK (held.events.size() == 2);   // toe once, then back to heel
        CHECK (isCC (held.events.back().second, 3, 11, 0) && held.events.back().first > 1.0);

        auto enc = whammy::encodeDrawing (whammy::defaultDrawing());
        auto dec = whammy::decodeDrawing (enc);
        CHECK ((int) dec.size() == whammy::drawPoints);
        CHECK (std::abs (dec[20] - whammy::defaultDrawing()[20]) < 0.002f);
        CHECK ((int) whammy::decodeDrawing ("0,1000").size() == whammy::drawPoints);
        CHECK (whammy::decodeDrawing ("0,1000").back() == 1.0f);
        CHECK ((int) whammy::decodeDrawing ("garbage").size() == whammy::drawPoints);
    }

    // QC expression pedals (CC#1 / CC#2)
    {
        auto swell = qc::expressionMove (1, 1, qc::ExpShape::swellIn, 4.0, 1.0, false);
        CHECK (swell.name == "QC Exp 1 Swell In 1 bar");
        CHECK (isCC (swell.events.front().second, 1, 1, 0) && isCC (swell.events.back().second, 1, 1, 127));
        CHECK (swell.events.back().first <= 4.0);   // no reset: the swell stays up

        auto exp2 = qc::expressionMove (3, 2, qc::ExpShape::fadeOut, 2.0, 1.0, true);
        CHECK (isCC (exp2.events.front().second, 3, 2, 127) && isCC (exp2.events.back().second, 3, 2, 0));

        auto wah = qc::expressionMove (1, 1, qc::ExpShape::wahRhythm, 2.0, 1.0, false);
        int toes = 0;
        for (auto& [beat, m] : wah.events)
            if (m.getControllerValue() == 127) ++toes;
        CHECK (toes == 2);   // toe once per beat

        auto rise = qc::expressionMove (1, 1, qc::ExpShape::riseToBar, 4.0, 1.0, false);
        CHECK (rise.events[1].first >= 3.0 && rise.events.back().second.getControllerValue() == 127);

        auto half = qc::expressionSet (5, 2, 0.5f);
        CHECK (half.name == "QC Exp 2 50%" && half.events.size() == 1 && isCC (half.events[0].second, 5, 2, 64));
        CHECK (qc::expressionSet (1, 1, 0.0f).name == "QC Exp 1 Heel" && qc::expressionSet (1, 1, 1.0f).name == "QC Exp 1 Toe");

        auto first = qc::withPresetFirst (swell, 1, 1, 1, 2, false, "Clean Rig");
        CHECK (first.name == "QC Clean Rig > Exp 1 Swell In 1 bar");
        CHECK (first.events[1].second.isProgramChange() && first.events[1].second.getProgramChangeNumber() == 2);
        CHECK (isCC (first.events[2].second, 1, 1, 0) && std::abs (first.events[2].first - 0.25) < 1.0e-9);
        CHECK (std::abs (first.lengthBeats - (swell.lengthBeats + 0.25)) < 1.0e-9);

        auto drawnExp = qc::expressionDrawn (1, 2, { 0.0f, 1.0f }, 1.0, false);
        CHECK (drawnExp.name == "QC Exp 2 Drawn 1 beat" && isCC (drawnExp.events.back().second, 1, 2, 127));
    }

    // My drawings: named drawn moves, saved in the state and the setup library
    {
        CHECK (whammy::drawn (2, { 0.0f, 1.0f }, 4.0, false, "Big Bend").name == "Whammy Big Bend 1 bar");
        CHECK (qc::expressionDrawn (1, 1, { 0.0f, 1.0f }, 2.0, false, "Slow Swell").name == "QC Exp 1 Slow Swell 2 beats");

        juce::ValueTree list (IDs::Drawings);
        state::saveDrawing (list, "Big Bend", "0,1000");
        state::saveDrawing (list, "Swell", "0,500,1000");
        state::saveDrawing (list, "Big Bend", "1000,0");   // overwrite, no duplicate
        CHECK (list.getNumChildren() == 2);
        CHECK (state::findDrawing (list, "Big Bend")[IDs::points].toString() == "1000,0");
        CHECK (! state::renameDrawing (list, "Big Bend", "Swell"));   // name taken
        CHECK (state::renameDrawing (list, "Big Bend", "Dive"));
        CHECK (! state::findDrawing (list, "Big Bend").isValid() && state::findDrawing (list, "Dive").isValid());

        // Projects don't carry drawings; an exported setup does, and importing adds them without removing any.
        auto root = state::createDefault();
        CHECK (! root.getChildWithName (IDs::Drawings).isValid());
        root.setProperty (IDs::whChords, true, nullptr);          // playing preferences travel with the setup
        root.setProperty (IDs::expLoadFirst, true, nullptr);
        root.setProperty (IDs::sweepReset, false, nullptr);
        root.setProperty (IDs::sweepBeats, 16.0, nullptr);        // per song: stays in the project
        root.setProperty (IDs::setupViaQcChain, true, nullptr);   // export adds the wiring choice like this
        const auto file = juce::File::createTempFile (".xml");
        CHECK (state::saveLibrary (root, file, &list));
        juce::ValueTree mine (IDs::Drawings);
        state::saveDrawing (mine, "Mine", "0,1000");
        auto other = state::createDefault();
        CHECK (state::loadLibrary (other, file, &mine));
        CHECK (mine.getNumChildren() == 3 && state::findDrawing (mine, "Dive")[IDs::points].toString() == "1000,0");
        CHECK (! other.getChildWithName (IDs::Drawings).isValid());
        CHECK ((bool) other[IDs::whChords] && (bool) other[IDs::expLoadFirst] && ! (bool) other[IDs::sweepReset]);
        CHECK ((double) other[IDs::sweepBeats] == 4.0);
        CHECK ((bool) other[IDs::setupViaQcChain]);
        file.deleteFile();

        state::deleteDrawing (list, "Dive");
        CHECK (list.getNumChildren() == 1);
    }

    // MIDI file round trip
    auto file = writeMidiFile (up, 90.0);
    CHECK (file.existsAsFile() && file.getFileName() == "Whammy Ramp Up 1 bar.mid");
    juce::FileInputStream in (file);
    juce::MidiFile mf;
    CHECK (mf.readFrom (in));
    CHECK (mf.getNumTracks() == 1 && mf.getTimeFormat() == 960);
    int ccCount = 0;
    for (auto* ev : *mf.getTrack (0))
        if (ev->message.isController()) ++ccCount;
    CHECK (ccCount == (int) up.events.size());

    CHECK (formatBeats (0.25) == "1/16" && formatBeats (8.0) == "2 bars" && formatBeats (3.0) == "3 beats");

    // Clips whose events all sit on the first tick (a lone scene, the tuner) repeat their last message 1/16 later,
    // so DAWs that size a clip by its last event (Ableton Live) accept them. Spread-out clips are written as they are.
    {
        auto readEvents = [] (const juce::File& f)
        {
            std::vector<std::pair<double, juce::MidiMessage>> out;
            juce::FileInputStream stream (f);
            juce::MidiFile midi;
            if (midi.readFrom (stream))
                for (auto* ev : *midi.getTrack (0))
                    if (! ev->message.isMetaEvent())
                        out.emplace_back (ev->message.getTimeStamp(), ev->message);
            return out;
        };
        const auto lone = readEvents (writeMidiFile (qc::scene (1, 1, "Verse"), 120.0));
        CHECK (lone.size() == 2 && isCC (lone[0].second, 1, 43, 1) && lone[0].first == 0.0);
        CHECK (isCC (lone[1].second, 1, 43, 1) && std::abs (lone[1].first - 240.0) < 1.0e-6);
        const auto loaded = readEvents (writeMidiFile (qc::withPresetFirst (qc::scene (1, 1, {}), 1, 1, 1, 0, false, "Clean"), 120.0));
        CHECK (loaded.size() == 3);   // preset (CC#0, PC) + the scene 1/16 later: nothing added

    }

    std::printf (failures == 0 ? "All tests passed\n" : "%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
