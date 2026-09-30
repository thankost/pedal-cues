#include "../Source/CueModel.h"
#include "../Source/Update.h"

#include <cstdio>

static int failures = 0;

#define CHECK(cond) do { if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); } } while (0)

static bool isCC (const juce::MidiMessage& m, int ch, int cc, int v)
{
    return m.isController() && m.getChannel() == ch && m.getControllerNumber() == cc && m.getControllerValue() == v;
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
