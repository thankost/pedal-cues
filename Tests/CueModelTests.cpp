#include "../Source/CueModel.h"
#include "../Source/Update.h"
#include "../Source/State.h"
#include "../Source/QcUsb.h"

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
    CHECK (p2.events.size() == 3 && isCC (p2.events[0].second, 1, 0, 1) && isCC (p2.events[1].second, 1, 32, 3));
    CHECK (p2.events[2].second.getProgramChangeNumber() == 24);

    CHECK (isCC (qc::tuner (1, true).events[0].second, 1, 45, 127));
    CHECK (isCC (qc::stomp (1, 7, false, {}).events[0].second, 1, 42, 0));
    CHECK (isCC (qc::gigMode (1, 1).events[0].second, 1, 47, 2)); // scene
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

    std::printf (failures == 0 ? "All tests passed\n" : "%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
