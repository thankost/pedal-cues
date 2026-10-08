#include "Songs.h"
#include "State.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <map>

namespace songs
{
namespace
{
constexpr int ticksPerQuarter = 960;

juce::String newUid() { return juce::Uuid().toString(); }

int clampDen (int den)
{
    for (int d : { 1, 2, 4, 8, 16, 32, 64 })
        if (den <= d)
            return d;
    return 64;
}

// Events as text: "beat:hexbytes" separated by spaces, e.g. "0:b00000 0:c005 0.25:b02b01".
juce::String eventsToText (const cues::Cue& cue)
{
    juce::StringArray parts;
    for (const auto& [beat, m] : cue.events)
        parts.add (juce::String (beat, 6).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + ":"
                   + juce::String::toHexString (m.getRawData(), m.getRawDataSize(), 0));
    return parts.joinIntoString (" ");
}

void eventsFromText (const juce::String& text, cues::Cue& cue)
{
    for (const auto& part : juce::StringArray::fromTokens (text, " ", ""))
    {
        const auto colon = part.indexOfChar (':');
        if (colon <= 0)
            continue;
        const auto beat = part.substring (0, colon).getDoubleValue();
        const auto hex = part.substring (colon + 1);
        juce::MemoryBlock bytes;
        bytes.loadFromHexString (hex);
        if (bytes.getSize() == 0 || ! std::isfinite (beat) || beat < 0.0)
            continue;
        cue.add (beat, juce::MidiMessage (bytes.getData(), (int) bytes.getSize()));
    }
}
} // namespace

//==============================================================================
juce::ValueTree songsNode (juce::ValueTree root)
{
    auto node = root.getChildWithName (IDs::Songs);
    if (! node.isValid())
    {
        node = juce::ValueTree (IDs::Songs);
        root.appendChild (node, nullptr);
    }
    return node;
}

juce::ValueTree createSection (const juce::String& name, int bars, int timeNum, int timeDen, double bpm)
{
    juce::ValueTree s (IDs::SongSection);
    s.setProperty (IDs::name, name, nullptr);
    s.setProperty (IDs::uid, newUid(), nullptr);
    s.setProperty (IDs::bars, juce::jlimit (1, maxBars, bars), nullptr);
    s.setProperty (IDs::timeNum, juce::jlimit (1, maxBeatsPerBar, timeNum), nullptr);
    s.setProperty (IDs::timeDen, clampDen (timeDen), nullptr);
    s.setProperty (IDs::bpm, juce::jlimit (minBpm, maxBpm, bpm), nullptr);
    return s;
}

juce::ValueTree createSong (const juce::String& name)
{
    juce::ValueTree song (IDs::Song);
    song.setProperty (IDs::name, name, nullptr);
    song.setProperty (IDs::uid, newUid(), nullptr);
    song.appendChild (createSection ("Intro", 8, 4, 4, 120.0), nullptr);
    return song;
}

juce::ValueTree selectedSong (juce::ValueTree root)
{
    auto list = root.getChildWithName (IDs::Songs);
    if (! list.isValid() || list.getNumChildren() == 0)
        return {};
    const auto uid = root[IDs::selectedSong].toString();
    for (auto song : list)
        if (song[IDs::uid].toString() == uid)
            return song;
    return list.getChild (0);
}

//==============================================================================
double barBeats (const juce::ValueTree& section)
{
    const auto num = juce::jlimit (1, maxBeatsPerBar, (int) section.getProperty (IDs::timeNum, 4));
    const auto den = clampDen ((int) section.getProperty (IDs::timeDen, 4));
    return num * 4.0 / den;
}

double sectionLengthBeats (const juce::ValueTree& section)
{
    return juce::jmax (1, (int) section.getProperty (IDs::bars, 1)) * barBeats (section);
}

double sectionStartBeat (const juce::ValueTree& song, const juce::ValueTree& section)
{
    double beat = 0.0;
    for (const auto& s : song)
    {
        if (! s.hasType (IDs::SongSection))
            continue;
        if (s == section)
            return beat;
        beat += sectionLengthBeats (s);
    }
    return beat;
}

double songLengthBeats (const juce::ValueTree& song)
{
    double beat = 0.0;
    for (const auto& s : song)
        if (s.hasType (IDs::SongSection))
            beat += sectionLengthBeats (s);
    return beat;
}

int songBars (const juce::ValueTree& song)
{
    int bars = 0;
    for (const auto& s : song)
        if (s.hasType (IDs::SongSection))
            bars += juce::jmax (1, (int) s.getProperty (IDs::bars, 1));
    return bars;
}

static double sectionBpm (const juce::ValueTree& s) { return juce::jlimit (minBpm, maxBpm, (double) s.getProperty (IDs::bpm, 120.0)); }

std::vector<juce::ValueTree> tempoChanges (const juce::ValueTree& section)
{
    std::vector<juce::ValueTree> list;
    const auto length = sectionLengthBeats (section);
    for (const auto& c : section)
        if (c.hasType (IDs::SongTempo) && (double) c[IDs::beat] > 1.0e-9 && (double) c[IDs::beat] < length - 1.0e-9)
            list.push_back (c);
    std::stable_sort (list.begin(), list.end(), [] (const auto& a, const auto& b) { return (double) a[IDs::beat] < (double) b[IDs::beat]; });
    return list;
}

juce::ValueTree addTempoChange (juce::ValueTree section, double beat, double bpm, bool gradual)
{
    juce::ValueTree t (IDs::SongTempo);
    t.setProperty (IDs::beat, juce::jlimit (0.0, sectionLengthBeats (section), beat), nullptr);
    t.setProperty (IDs::bpm, juce::jlimit (minBpm, maxBpm, bpm), nullptr);
    if (gradual)
        t.setProperty (IDs::tempoRamp, true, nullptr);
    section.appendChild (t, nullptr);
    return t;
}

std::vector<TempoSegment> tempoSegments (const juce::ValueTree& song)
{
    // Every tempo point in song order: a section's start, then its changes.
    struct Point { double beat, bpm; bool glide; };
    std::vector<Point> points;
    double start = 0.0;
    for (const auto& s : song)
    {
        if (! s.hasType (IDs::SongSection))
            continue;
        points.push_back ({ start, sectionBpm (s), (bool) s.getProperty (IDs::tempoRamp, false) });
        for (const auto& c : tempoChanges (s))
            points.push_back ({ start + (double) c[IDs::beat], juce::jlimit (minBpm, maxBpm, (double) c[IDs::bpm]),
                                (bool) c.getProperty (IDs::tempoRamp, false) });
        start += sectionLengthBeats (s);
    }
    std::vector<TempoSegment> segments;
    for (size_t i = 0; i < points.size(); ++i)
    {
        const auto to = i + 1 < points.size() ? points[i + 1].beat : start;
        if (to <= points[i].beat + 1.0e-12)
            continue;
        const auto end = points[i].glide && i + 1 < points.size() ? points[i + 1].bpm : points[i].bpm;
        segments.push_back ({ points[i].beat, to, points[i].bpm, end });
    }
    return segments;
}

static juce::ValueTree nextSection (const juce::ValueTree& song, const juce::ValueTree& section)
{
    for (int i = song.indexOf (section) + 1; i < song.getNumChildren(); ++i)
        if (song.getChild (i).hasType (IDs::SongSection))
            return song.getChild (i);
    return {};
}

bool rampsTempo (const juce::ValueTree& song, const juce::ValueTree& section)
{
    return (bool) section.getProperty (IDs::tempoRamp, false) && (! tempoChanges (section).empty() || nextSection (song, section).isValid());
}

// Seconds from a segment's start to `x` beats into it, its tempo going evenly per beat from b0 to b1 over `length` beats
// (bpm(x) = b0 + (b1 - b0) x / length, so the time is a logarithm), and the way back.
static double secondsInto (double length, double b0, double b1, double x)
{
    if (std::abs (b1 - b0) < 1.0e-9 || length <= 0.0)
        return x * 60.0 / b0;
    const auto k = (b1 - b0) / length;
    return 60.0 / k * std::log ((b0 + k * x) / b0);
}

static double beatsInto (double length, double b0, double b1, double seconds)
{
    if (std::abs (b1 - b0) < 1.0e-9 || length <= 0.0)
        return seconds * b0 / 60.0;
    const auto k = (b1 - b0) / length;
    return b0 / k * (std::exp (seconds * k / 60.0) - 1.0);
}

static double segmentSeconds (const TempoSegment& g, double beat)
{
    return secondsInto (g.to - g.from, g.bpm0, g.bpm1, juce::jlimit (0.0, g.to - g.from, beat - g.from));
}

double songSeconds (const juce::ValueTree& song)
{
    double seconds = 0.0;
    for (const auto& g : tempoSegments (song))
        seconds += segmentSeconds (g, g.to);
    return seconds;
}

double sectionEndBpm (const juce::ValueTree& song, const juce::ValueTree& section)
{
    const auto end = sectionStartBeat (song, section) + sectionLengthBeats (section);
    for (const auto& g : tempoSegments (song))
        if (std::abs (g.to - end) < 1.0e-9)
            return g.bpm1;
    return sectionBpm (section);
}

double tempoAt (const juce::ValueTree& song, double beat)
{
    const auto segments = tempoSegments (song);
    for (const auto& g : segments)
        if (beat < g.to - 1.0e-9)
            return g.bpm0 + (g.bpm1 - g.bpm0) * juce::jlimit (0.0, 1.0, (beat - g.from) / (g.to - g.from));
    return segments.empty() ? 120.0 : segments.back().bpm1;
}

juce::ValueTree sectionAt (const juce::ValueTree& song, double beat)
{
    double start = 0.0;
    juce::ValueTree last;
    for (const auto& s : song)
    {
        if (! s.hasType (IDs::SongSection))
            continue;
        last = s;
        const auto end = start + sectionLengthBeats (s);
        if (beat < end - 1.0e-9)
            return s;
        start = end;
    }
    return last;
}

juce::ValueTree sectionByUid (const juce::ValueTree& song, const juce::String& uid)
{
    for (const auto& s : song)
        if (s.hasType (IDs::SongSection) && s[IDs::uid].toString() == uid)
            return s;
    return {};
}

juce::String barLabel (const juce::ValueTree& song, double beat)
{
    int barsBefore = 0;
    double start = 0.0;
    for (const auto& s : song)
    {
        if (! s.hasType (IDs::SongSection))
            continue;
        const auto length = sectionLengthBeats (s);
        if (beat < start + length - 1.0e-9 || s == sectionAt (song, beat))
        {
            const auto bb = barBeats (s);
            const auto offset = juce::jmax (0.0, beat - start);
            const auto bar = (int) std::floor (offset / bb + 1.0e-9);
            const auto unit = 4.0 / clampDen ((int) s.getProperty (IDs::timeDen, 4));
            const auto inBar = offset - bar * bb;
            const auto beatInBar = (int) std::floor (inBar / unit + 1.0e-9) + 1;
            auto label = "Bar " + juce::String (barsBefore + bar + 1);
            if (beatInBar > 1 || std::abs (inBar - (beatInBar - 1) * unit) > 1.0e-6)
                label << ", beat " << beatInBar;
            return label;
        }
        barsBefore += juce::jmax (1, (int) s.getProperty (IDs::bars, 1));
        start += length;
    }
    return "Bar 1";
}

double beatToSeconds (const juce::ValueTree& song, double beat)
{
    double seconds = 0.0;
    for (const auto& g : tempoSegments (song))
    {
        if (beat <= g.to)
            return seconds + segmentSeconds (g, beat);
        seconds += segmentSeconds (g, g.to);
    }
    return seconds;
}

double secondsToBeat (const juce::ValueTree& song, double seconds)
{
    double elapsed = 0.0, end = 0.0;
    for (const auto& g : tempoSegments (song))
    {
        const auto whole = segmentSeconds (g, g.to);
        if (seconds <= elapsed + whole)
            return g.from + juce::jlimit (0.0, g.to - g.from, beatsInto (g.to - g.from, g.bpm0, g.bpm1, seconds - elapsed));
        elapsed += whole;
        end = g.to;
    }
    return end;
}

//==============================================================================
juce::ValueTree cueToTree (const cues::Cue& cue)
{
    juce::ValueTree t (IDs::SongCue);
    t.setProperty (IDs::name, cue.name, nullptr);
    t.setProperty (IDs::lengthBeats, cue.lengthBeats, nullptr);
    t.setProperty (IDs::events, eventsToText (cue), nullptr);
    if (cue.toggles)      t.setProperty (IDs::toggles, true, nullptr);
    if (cue.cc0IsControl) t.setProperty (IDs::cc0IsControl, true, nullptr);
    if (cue.padCc >= 0)   t.setProperty (IDs::padCc, cue.padCc, nullptr);
    return t;
}

cues::Cue cueFromTree (const juce::ValueTree& t)
{
    cues::Cue cue;
    cue.name = t[IDs::name].toString();
    cue.lengthBeats = juce::jmax (0.0, (double) t.getProperty (IDs::lengthBeats, 1.0));
    cue.toggles = (bool) t.getProperty (IDs::toggles, false);
    cue.cc0IsControl = (bool) t.getProperty (IDs::cc0IsControl, false);
    cue.padCc = (int) t.getProperty (IDs::padCc, -1);
    eventsFromText (t[IDs::events].toString(), cue);
    return cue;
}

//==============================================================================
juce::ValueTree addTrack (juce::ValueTree song, const juce::String& name, juce::Colour colour)
{
    juce::ValueTree t (IDs::SongTrack);
    t.setProperty (IDs::name, name, nullptr);
    t.setProperty (IDs::uid, newUid(), nullptr);
    t.setProperty (IDs::colour, colour.toString(), nullptr);
    t.setProperty (IDs::include, true, nullptr);
    song.appendChild (t, nullptr);
    return t;
}

std::vector<juce::ValueTree> tracks (const juce::ValueTree& song)
{
    std::vector<juce::ValueTree> list;
    for (const auto& c : song)
        if (c.hasType (IDs::SongTrack))
            list.push_back (c);
    return list;
}

juce::ValueTree trackByUid (const juce::ValueTree& song, const juce::String& uid)
{
    for (const auto& c : song)
        if (c.hasType (IDs::SongTrack) && c[IDs::uid].toString() == uid)
            return c;
    return {};
}

void moveTrack (juce::ValueTree song, juce::ValueTree track, int delta)
{
    const auto list = tracks (song);
    const auto it = std::find (list.begin(), list.end(), track);
    if (it == list.end())
        return;
    const auto at = (int) (it - list.begin()) + delta;
    if (at < 0 || at >= (int) list.size())
        return;
    song.moveChild (song.indexOf (track), song.indexOf (list[(size_t) at]), nullptr);   // swap with the neighbouring track
}

void removeTrack (juce::ValueTree song, juce::ValueTree track)
{
    const auto uid = track[IDs::uid].toString();
    for (int i = song.getNumChildren(); --i >= 0;)
        if (song.getChild (i).hasType (IDs::SongCue) && song.getChild (i)[IDs::track].toString() == uid)
            song.removeChild (i, nullptr);
    song.removeChild (track, nullptr);
}

juce::StringArray includedTracks (const juce::ValueTree& song)
{
    juce::StringArray uids;
    for (const auto& t : tracks (song))
        if ((bool) t.getProperty (IDs::include, true))
            uids.add (t[IDs::uid].toString());
    return uids;
}

juce::ValueTree duplicateTrack (juce::ValueTree song, const juce::ValueTree& track)
{
    auto copy = track.createCopy();
    copy.setProperty (IDs::uid, newUid(), nullptr);
    copy.setProperty (IDs::name, track[IDs::name].toString() + " (copy)", nullptr);
    song.addChild (copy, song.indexOf (track) + 1, nullptr);
    for (int i = 0, n = song.getNumChildren(); i < n; ++i)
    {
        const auto c = song.getChild (i);
        if (c.hasType (IDs::SongCue) && c[IDs::track] == track[IDs::uid])
        {
            auto cue = c.createCopy();
            cue.setProperty (IDs::track, copy[IDs::uid], nullptr);
            song.appendChild (cue, nullptr);
        }
    }
    return copy;
}

void setCueChannel (juce::ValueTree cue, int channel)
{
    if (channel < 1 || channel > 16)
        return;
    auto c = cueFromTree (cue);
    for (auto& [b, m] : c.events)
        if (m.getChannel() > 0)
            m.setChannel (channel);
    cue.setProperty (IDs::events, eventsToText (c), nullptr);
}

void setTrackChannel (juce::ValueTree song, juce::ValueTree track, int channel)
{
    channel = juce::jlimit (0, 16, channel);
    track.setProperty (IDs::channel, channel, nullptr);
    if (channel == 0)
        return;   // own channels: the cues stay as they are
    for (auto c : song)
        if (c.hasType (IDs::SongCue) && c[IDs::track] == track[IDs::uid])
            setCueChannel (c, channel);
}

int trackChannel (const juce::ValueTree& track)
{
    return juce::jlimit (0, 16, (int) track.getProperty (IDs::channel, 0));
}

juce::Array<int> channelsUsed (const juce::ValueTree& song, const juce::ValueTree& track)
{
    juce::Array<int> used;
    for (const auto& c : song)
        if (c.hasType (IDs::SongCue) && c[IDs::track] == track[IDs::uid])
            for (const auto& [b, m] : cueFromTree (c).events)
                if (m.getChannel() > 0)
                    used.addIfNotAlreadyThere (m.getChannel());
    used.sort();
    return used;
}

cues::Cue cueFromMidiFile (const juce::MidiFile& file, const juce::String& name)
{
    cues::Cue cue;
    cue.name = name;
    const auto tpq = (double) file.getTimeFormat();
    if (tpq <= 0.0)
        return cue;
    double last = 0.0;
    for (int t = 0; t < file.getNumTracks(); ++t)
    {
        const auto& seq = *file.getTrack (t);
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            const auto& m = seq.getEventPointer (i)->message;
            const auto beat = m.getTimeStamp() / tpq;
            if (m.isEndOfTrackMetaEvent())
                last = juce::jmax (last, beat);
            if (m.isMetaEvent() || m.isSysEx() || m.isMidiClock() || m.isActiveSense())
                continue;
            cue.add (beat, m);
            last = juce::jmax (last, beat);
        }
    }
    std::stable_sort (cue.events.begin(), cue.events.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    cue.lengthBeats = juce::jmax (0.25, last);
    return cue;
}

static void setCuePosition (juce::ValueTree song, juce::ValueTree cue, const juce::ValueTree& track, double beat)
{
    beat = juce::jlimit (0.0, juce::jmax (0.0, songLengthBeats (song) - 0.25), beat);
    const auto section = sectionAt (song, beat);
    cue.setProperty (IDs::track, track[IDs::uid].toString(), nullptr);
    cue.setProperty (IDs::section, section[IDs::uid].toString(), nullptr);
    cue.setProperty (IDs::beat, juce::jmax (0.0, beat - sectionStartBeat (song, section)), nullptr);
    setCueChannel (cue, trackChannel (track));   // a track with its own channel takes every cue onto it
}

juce::ValueTree placeCue (juce::ValueTree song, const juce::ValueTree& track, double beat, const cues::Cue& cue, juce::Colour colour)
{
    auto t = cueToTree (cue);
    t.setProperty (IDs::colour, colour.toString(), nullptr);
    setCuePosition (song, t, track, beat);
    song.appendChild (t, nullptr);
    return t;
}

void moveCue (juce::ValueTree song, juce::ValueTree cue, const juce::ValueTree& track, double beat)
{
    setCuePosition (song, cue, track, beat);
}

double cueBeat (const juce::ValueTree& song, const juce::ValueTree& cue)
{
    const auto section = sectionByUid (song, cue[IDs::section].toString());
    if (! section.isValid())
        return -1.0;
    return sectionStartBeat (song, section) + juce::jmax (0.0, (double) cue.getProperty (IDs::beat, 0.0));
}

std::vector<ClipCue> copyCues (const juce::ValueTree& song, const std::vector<juce::ValueTree>& cues)
{
    std::vector<ClipCue> clip;
    double first = std::numeric_limits<double>::max();
    for (const auto& c : cues)
        if (cueBeat (song, c) >= 0.0)
            first = juce::jmin (first, cueBeat (song, c));
    for (const auto& c : cues)
        if (const auto at = cueBeat (song, c); at >= 0.0)
            clip.push_back ({ c.createCopy(), at - first, c[IDs::track].toString() });
    std::sort (clip.begin(), clip.end(), [] (const auto& a, const auto& b) { return a.offset < b.offset; });
    return clip;
}

std::vector<juce::ValueTree> pasteCues (juce::ValueTree song, const std::vector<ClipCue>& clip, double beat, const juce::ValueTree& track)
{
    juce::StringArray fromTracks;
    for (const auto& item : clip)
        fromTracks.addIfNotAlreadyThere (item.track);
    std::vector<juce::ValueTree> pasted;
    for (const auto& item : clip)
    {
        auto target = track;
        if (fromTracks.size() > 1 || ! target.isValid())
            if (const auto own = trackByUid (song, item.track); own.isValid())
                target = own;
        if (! target.isValid())
            continue;
        auto copy = item.cue.createCopy();
        setCuePosition (song, copy, target, beat + item.offset);
        song.appendChild (copy, nullptr);
        pasted.push_back (copy);
    }
    return pasted;
}

double clipLength (const std::vector<ClipCue>& clip)
{
    double end = 0.0;
    for (const auto& item : clip)
        end = juce::jmax (end, item.offset + juce::jmax (1.0, (double) item.cue.getProperty (IDs::lengthBeats, 1.0)));
    return end;
}

void removeSection (juce::ValueTree song, juce::ValueTree section)
{
    const auto uid = section[IDs::uid].toString();
    for (int i = song.getNumChildren(); --i >= 0;)
        if (song.getChild (i).hasType (IDs::SongCue) && song.getChild (i)[IDs::section].toString() == uid)
            song.removeChild (i, nullptr);
    song.removeChild (section, nullptr);
}

//==============================================================================
juce::MidiFile songMidi (const juce::ValueTree& song, const juce::StringArray& trackUids, bool singleTrack)
{
    const auto toTicks = [] (double beat) { return beat * ticksPerQuarter; };
    const auto end = toTicks (juce::jmax (1.0, songLengthBeats (song)));

    juce::MidiMessageSequence conductor;
    conductor.addEvent (juce::MidiMessage::textMetaEvent (3, song[IDs::name].toString()), 0.0);
    double beat = 0.0, lastBpm = -1.0;
    int lastNum = -1, lastDen = -1;
    for (const auto& s : song)
    {
        if (! s.hasType (IDs::SongSection))
            continue;
        const auto num = juce::jlimit (1, maxBeatsPerBar, (int) s.getProperty (IDs::timeNum, 4));
        const auto den = clampDen ((int) s.getProperty (IDs::timeDen, 4));
        const auto length = sectionLengthBeats (s);
        if (num != lastNum || den != lastDen)
            conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (num, den), toTicks (beat));
        conductor.addEvent (juce::MidiMessage::textMetaEvent (6, s[IDs::name].toString()), toTicks (beat));
        lastNum = num; lastDen = den;
        beat += length;
    }
    // The tempo map: a tempo at each steady change; a glide as a tempo every 1/16 (a MIDI file holds steps), each lasting
    // exactly as long as the glide does there.
    for (const auto& g : tempoSegments (song))
    {
        const auto write = [&] (double at, double bpmNow)
        {
            if (std::abs (bpmNow - lastBpm) > 1.0e-6)
                conductor.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / bpmNow)), toTicks (at));
            lastBpm = bpmNow;
        };
        if (std::abs (g.bpm1 - g.bpm0) < 1.0e-9)
        {
            write (g.from, g.bpm0);
            continue;
        }
        constexpr double step = 0.25;
        const auto length = g.to - g.from;
        for (double x = 0.0; x < length - 1.0e-9; x += step)
        {
            const auto d = juce::jmin (step, length - x);
            write (g.from + x, d * 60.0 / (secondsInto (length, g.bpm0, g.bpm1, x + d) - secondsInto (length, g.bpm0, g.bpm1, x)));
        }
    }
    conductor.sort();

    // One sequence per exported track, in the song's order.
    std::vector<juce::ValueTree> exported;
    for (const auto& t : tracks (song))
        if (trackUids.contains (t[IDs::uid].toString()))
            exported.push_back (t);
    std::vector<juce::MidiMessageSequence> rows (exported.size());
    for (size_t r = 0; r < exported.size(); ++r)
        rows[r].addEvent (juce::MidiMessage::textMetaEvent (3, exported[r][IDs::name].toString()), 0.0);
    for (const auto& c : song)
    {
        if (! c.hasType (IDs::SongCue))
            continue;
        const auto uid = c[IDs::track].toString();
        const auto it = std::find_if (exported.begin(), exported.end(), [&] (const auto& t) { return t[IDs::uid].toString() == uid; });
        const auto at = cueBeat (song, c);
        if (it == exported.end() || at < 0.0)
            continue;
        auto& target = singleTrack ? conductor : rows[(size_t) (it - exported.begin())];
        const auto channel = trackChannel (*it);
        for (auto [b, m] : cueFromTree (c).events)
        {
            if (channel > 0 && m.getChannel() > 0)
                m.setChannel (channel);   // the track's channel wins: the same cues for another player
            target.addEvent (m, toTicks (at + b));
        }
    }

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerQuarter);
    conductor.addEvent (juce::MidiMessage::endOfTrack(), end);
    conductor.sort();
    conductor.updateMatchedPairs();
    file.addTrack (conductor);
    if (! singleTrack)
        for (auto& seq : rows)
        {
            seq.addEvent (juce::MidiMessage::endOfTrack(), end);
            seq.sort();
            file.addTrack (seq);
        }
    return file;
}

double countInSeconds (const juce::ValueTree& song, double fromBeat, int countInBars)
{
    const auto section = sectionAt (song, fromBeat);
    if (! section.isValid() || countInBars <= 0)
        return 0.0;
    return countInBars * barBeats (section) * 60.0 / tempoAt (song, fromBeat);
}

juce::StringArray clickDivNames()
{
    return { "Beat", "1/4", "1/8", "1/16", "1/8 T", "1/16 T", "Bars only", "Off" };
}

double clickStepBeats (const juce::ValueTree& section)
{
    const auto beat = 4.0 / juce::jmax (1, (int) section.getProperty (IDs::timeDen, 4));
    switch ((int) section.getProperty (IDs::clickDiv, 0))
    {
        case 1: return 1.0;
        case 2: return 0.5;
        case 3: return 0.25;
        case 4: return 1.0 / 3.0;
        case 5: return 1.0 / 6.0;
        case 6: return barBeats (section);
        case 7: return 0.0;
        default: return beat;
    }
}

std::vector<Click> metronomeClicks (const juce::ValueTree& song, double fromBeat, int countInBars)
{
    std::vector<Click> clicks;
    const auto lead = countInSeconds (song, fromBeat, countInBars);
    if (lead > 0.0)
    {
        const auto section = sectionAt (song, fromBeat);
        const auto unit = 4.0 / juce::jmax (1, (int) section.getProperty (IDs::timeDen, 4));
        const auto perBar = juce::roundToInt (barBeats (section) / unit);
        const auto spb = 60.0 / tempoAt (song, fromBeat);
        for (int i = 0; i < countInBars * perBar; ++i)
            clicks.push_back ({ i * unit * spb, i % perBar == 0, false });
    }
    const auto startSeconds = beatToSeconds (song, fromBeat);
    double start = 0.0;
    for (const auto& s : song)
    {
        if (! s.hasType (IDs::SongSection))
            continue;
        const auto beatUnit = 4.0 / juce::jmax (1, (int) s.getProperty (IDs::timeDen, 4));
        const auto step = clickStepBeats (s);
        const auto bb = barBeats (s);
        const auto length = sectionLengthBeats (s);
        if (step > 0.0)
            for (int k = 0; k * step < length - 1.0e-9; ++k)
            {
                const auto b = k * step;
                const auto beat = start + b;
                if (beat < fromBeat - 1.0e-9)
                    continue;
                const auto inBar = std::fmod (b, bb);
                const auto onBar = inBar < 1.0e-6 || bb - inBar < 1.0e-6;
                const auto inBeat = std::fmod (inBar, beatUnit);
                const auto onBeat = inBeat < 1.0e-6 || beatUnit - inBeat < 1.0e-6;
                clicks.push_back ({ lead + beatToSeconds (song, beat) - startSeconds, onBar, ! onBeat });
            }
        start += length;
    }
    return clicks;
}

juce::StringArray gridStepNames()
{
    return { "Beat", "Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4 T", "1/8 T", "1/16 T" };
}

double gridStepBeats (int step, const juce::ValueTree& section)
{
    switch (step)
    {
        case 1: return barBeats (section);
        case 2: return 2.0;
        case 3: return 1.0;
        case 4: return 0.5;
        case 5: return 0.25;
        case 6: return 0.125;
        case 7: return 2.0 / 3.0;
        case 8: return 1.0 / 3.0;
        case 9: return 1.0 / 6.0;
        default: return 4.0 / juce::jmax (1, (int) section.getProperty (IDs::timeDen, 4));
    }
}

double snapBeat (const juce::ValueTree& song, double beat, int step)
{
    const auto section = sectionAt (song, beat);
    if (! section.isValid())
        return juce::jmax (0.0, beat);
    const auto start = sectionStartBeat (song, section);
    const auto unit = gridStepBeats (step, section);
    return juce::jmax (0.0, start + std::round ((beat - start) / unit) * unit);
}

std::vector<std::pair<double, juce::MidiMessage>> playbackEvents (const juce::ValueTree& song, const juce::StringArray& trackUids,
                                                                  double fromBeat)
{
    // The same messages as the exported file (track channels applied), minus the meta events.
    const auto file = songMidi (song, trackUids);
    const auto offset = beatToSeconds (song, fromBeat);
    std::vector<std::pair<double, juce::MidiMessage>> events;
    for (int t = 0; t < file.getNumTracks(); ++t)
    {
        const auto& seq = *file.getTrack (t);
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            const auto& m = seq.getEventPointer (i)->message;
            const auto beat = m.getTimeStamp() / ticksPerQuarter;
            if (m.isMetaEvent() || beat < fromBeat - 1.0e-9)
                continue;
            events.emplace_back (beatToSeconds (song, beat) - offset, m);
        }
    }
    std::stable_sort (events.begin(), events.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    return events;
}

juce::File writeSongFile (const juce::ValueTree& song, const juce::StringArray& trackUids, bool singleTrack, const juce::String& fileName)
{
    const auto file = songMidi (song, trackUids, singleTrack);
    juce::MemoryOutputStream data;
    if (! file.writeTo (data, 1))
        return {};

    // One folder per unique content, like the tiles: DAWs that reference the file keep working.
    const auto hash = juce::String::toHexString (data.getMemoryBlock().toBase64Encoding().hashCode64());
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("PedalCues").getChildFile (hash);
    if (! dir.createDirectory())
        return {};
    auto name = juce::File::createLegalFileName (fileName.isNotEmpty() ? fileName : song[IDs::name].toString()).trim();
    if (name.isEmpty())
        name = "Song";
    const auto out = dir.getChildFile (name + ".mid");
    return out.replaceWithData (data.getData(), data.getDataSize()) ? out : juce::File();
}

//==============================================================================
juce::ValueTree withCountIn (const juce::ValueTree& song, int bars)
{
    auto copy = song.createCopy();
    const auto first = copy.getChildWithName (IDs::SongSection);
    if (bars <= 0 || ! first.isValid())
        return copy;
    copy.addChild (createSection ("Count-in", bars, first[IDs::timeNum], first[IDs::timeDen], first[IDs::bpm]), copy.indexOf (first), nullptr);
    return copy;
}

juce::StringArray clickSoundNames() { return { "Beep", "Click", "Wood block", "Cowbell", "Custom samples" }; }

ClickSound clickSettings (const juce::ValueTree& node)
{
    ClickSound c;
    c.sound = juce::jlimit (0, 4, (int) node.getProperty (IDs::clickSound, 0));
    c.accent = (bool) node.getProperty (IDs::clickAccent, true);
    c.gain = juce::jlimit (0.0f, 1.0f, (float) (double) node.getProperty (IDs::clickGain, 0.7));
    return c;
}

double clickLength (const ClickSound& c)
{
    if (c.sound != 4)
        return clickSeconds;
    double longest = clickSeconds;
    for (const auto& b : { c.accentSample, c.beatSample })
        if (b != nullptr)
            longest = juce::jmax (longest, b->getNumSamples() / c.sampleRate);
    return juce::jmin (2.0, longest);
}

float clickSample (const ClickSound& c, bool accented, double t, juce::Random* noise, bool sub)
{
    const auto accent = accented && c.accent;
    const auto level = sub ? 0.35 : (accent ? 0.95 : 0.65);
    if (c.sound == 4)   // custom samples: the accent one on bar starts (else the beat one), the beat one for the rest
    {
        const auto& sample = accent && c.accentSample != nullptr ? c.accentSample : c.beatSample != nullptr ? c.beatSample : c.accentSample;
        if (sample == nullptr || t < 0.0)
            return 0.0f;
        const auto i = (int) (t * c.sampleRate);
        return i < sample->getNumSamples() ? sample->getSample (0, i) * c.gain * (float) (sub ? 0.5 : 1.0) : 0.0f;
    }
    if (t < 0.0 || t >= clickSeconds)
        return 0.0f;
    const auto twoPi = juce::MathConstants<double>::twoPi;
    double v = 0.0;
    switch (c.sound)
    {
        case 1:   // a short noise click
            v = (noise != nullptr ? noise->nextFloat() * 2.0 - 1.0 : std::sin (twoPi * 3000.0 * t)) * std::exp (-t * 400.0);
            break;
        case 2:   // a wood block: two tones, quick decay
            v = (0.7 * std::sin (twoPi * (accent ? 1250.0 : 950.0) * t) + 0.3 * std::sin (twoPi * (accent ? 2700.0 : 2100.0) * t))
                * std::exp (-t * 180.0);
            break;
        case 3:   // a cowbell: two detuned squares-ish tones
            v = (std::sin (twoPi * (accent ? 800.0 : 560.0) * t) + std::sin (twoPi * (accent ? 1200.0 : 845.0) * t)) * 0.5
                * std::exp (-t * 60.0);
            break;
        default:  // a beep
            v = std::sin (twoPi * (accent ? 1760.0 : 1320.0) * t) * std::exp (-t * 120.0);
            break;
    }
    return (float) (v * c.gain * level);
}

juce::AudioBuffer<float> renderClickTrack (const juce::ValueTree& song, int countInBars, double sampleRate, const ClickSound& c,
                                           double tailSeconds)
{
    const auto lead = countInSeconds (song, 0.0, countInBars);
    const auto total = lead + songSeconds (song) + juce::jmax (0.0, tailSeconds);
    juce::AudioBuffer<float> out (1, juce::jmax (1, (int) std::ceil (total * sampleRate)));
    out.clear();
    juce::Random noise (1234);
    const auto len = (int) (clickLength (c) * sampleRate);
    for (const auto& k : metronomeClicks (song, 0.0, countInBars))
    {
        const auto at = (int) std::round (k.seconds * sampleRate);
        for (int i = 0; i < len && at + i < out.getNumSamples(); ++i)
            out.addSample (0, at + i, clickSample (c, k.accent, i / sampleRate, &noise, k.sub));
    }
    return out;
}

double barStartBeat (const juce::ValueTree& song, int bar)
{
    double beat = 0.0;
    int n = 1;
    for (const auto& s : song)
    {
        if (! s.hasType (IDs::SongSection))
            continue;
        const auto bars = juce::jmax (1, (int) s.getProperty (IDs::bars, 1));
        if (bar < n + bars)
            return beat + (bar - n) * barBeats (s);
        beat += sectionLengthBeats (s);
        n += bars;
    }
    return beat;
}

int barNumberAt (const juce::ValueTree& song, double beat)
{
    double start = 0.0;
    int n = 1;
    for (const auto& s : song)
    {
        if (! s.hasType (IDs::SongSection))
            continue;
        const auto length = sectionLengthBeats (s);
        if (beat < start + length - 1.0e-9)
            return n + (int) std::floor ((beat - start) / barBeats (s) + 1.0e-9);
        start += length;
        n += juce::jmax (1, (int) s.getProperty (IDs::bars, 1));
    }
    return n;
}

bool fitTempoToAudio (juce::ValueTree song, int bar, double fileSeconds)
{
    const auto offset = (double) song.getProperty (IDs::audioOffset, 0.0);
    const auto wanted = fileSeconds - offset;   // seconds from bar 1 to that bar, in the audio
    const auto now = beatToSeconds (song, barStartBeat (song, bar));
    if (bar <= 1 || wanted <= 0.05 || now <= 0.0)
        return false;
    const auto factor = now / wanted;           // faster when the audio gets there sooner
    const auto scale = [factor] (juce::ValueTree t)
    {
        t.setProperty (IDs::bpm, juce::jlimit (minBpm, maxBpm, std::round ((double) t.getProperty (IDs::bpm, 120.0) * factor * 100.0) / 100.0), nullptr);
    };
    for (auto s : song)
        if (s.hasType (IDs::SongSection))
        {
            scale (s);
            for (auto c : s)   // its tempo changes too
                if (c.hasType (IDs::SongTempo))
                    scale (c);
        }
    return true;
}

int barsToCoverAudio (const juce::ValueTree& song, double audioSeconds)
{
    juce::ValueTree last;
    for (const auto& s : song)
        if (s.hasType (IDs::SongSection))
            last = s;
    if (! last.isValid())
        return 0;
    const auto past = audioSeconds - (double) song.getProperty (IDs::audioOffset, 0.0) - songSeconds (song);
    if (past <= 0.05)
        return 0;
    const auto beats = past * sectionEndBpm (song, last) / 60.0;
    return (int) std::ceil (beats / barBeats (last) - 1.0e-6);
}

void extendLastSection (juce::ValueTree song, int bars)
{
    juce::ValueTree last;
    for (const auto& s : song)
        if (s.hasType (IDs::SongSection))
            last = s;
    if (last.isValid() && bars > 0)
        last.setProperty (IDs::bars, juce::jlimit (1, maxBars, juce::jmax (1, (int) last.getProperty (IDs::bars, 1)) + bars), nullptr);
}

double tempoFromTaps (const std::vector<double>& taps)
{
    if (taps.size() < 2)
        return 0.0;
    std::vector<double> gaps;
    for (size_t i = 1; i < taps.size(); ++i)
        if (taps[i] > taps[i - 1])
            gaps.push_back (taps[i] - taps[i - 1]);
    if (gaps.empty())
        return 0.0;
    std::sort (gaps.begin(), gaps.end());
    const auto median = gaps[gaps.size() / 2];
    return juce::jlimit (minBpm, maxBpm, std::round (60.0 / median * 10.0) / 10.0);
}

juce::StringArray playedTracks (const juce::ValueTree& song)
{
    const auto list = tracks (song);
    const auto anySolo = std::any_of (list.begin(), list.end(), [] (const auto& t) { return (bool) t.getProperty (IDs::soloed, false); });
    juce::StringArray uids;
    for (const auto& t : list)
        if ((bool) t.getProperty (IDs::include, true) && (anySolo ? (bool) t.getProperty (IDs::soloed, false)
                                                                   : ! (bool) t.getProperty (IDs::muted, false)))
            uids.add (t[IDs::uid].toString());
    return uids;
}

bool loopRange (const juce::ValueTree& song, double& start, double& end)
{
    const auto section = sectionByUid (song, song[IDs::loopSection].toString());
    if (! section.isValid())
        return false;
    start = sectionStartBeat (song, section);
    end = start + sectionLengthBeats (section);
    return true;
}

//==============================================================================
juce::ValueTree copySong (const juce::ValueTree& song, const juce::String& name)
{
    auto copy = song.createCopy();
    copy.setProperty (IDs::name, name, nullptr);
    copy.setProperty (IDs::uid, newUid(), nullptr);
    for (auto child : copy)   // new ids for sections and tracks, and the cues follow them
        for (const auto* type : { &IDs::SongSection, &IDs::SongTrack })
            if (child.hasType (*type))
            {
                const auto old = child[IDs::uid].toString(), fresh = newUid();
                const auto& ref = *type == IDs::SongSection ? IDs::section : IDs::track;
                child.setProperty (IDs::uid, fresh, nullptr);
                for (auto cue : copy)
                    if (cue.hasType (IDs::SongCue) && cue[ref].toString() == old)
                        cue.setProperty (ref, fresh, nullptr);
            }
    return copy;
}

bool saveSongFile (const juce::ValueTree& song, const juce::File& file)
{
    juce::ValueTree root ("PedalCuesSong");
    root.setProperty ("version", JucePlugin_VersionString, nullptr);
    root.appendChild (song.createCopy(), nullptr);
    if (const auto xml = root.createXml())
        return xml->writeTo (file);
    return false;
}

juce::ValueTree loadSongFile (const juce::File& file, juce::String& error)
{
    const auto xml = juce::XmlDocument::parse (file);
    const auto root = xml != nullptr ? juce::ValueTree::fromXml (*xml) : juce::ValueTree();
    const auto song = root.hasType ("PedalCuesSong") ? root.getChildWithName (IDs::Song) : juce::ValueTree();
    if (! song.isValid() || ! song.getChildWithName (IDs::SongSection).isValid())
    {
        error = file.getFileName() + " isn't a PedalCues song file.";
        return {};
    }
    // Drop what the song doesn't know (a newer version's extras stay harmless) and cues whose section or track is gone.
    auto copy = copySong (song, song[IDs::name].toString().isNotEmpty() ? song[IDs::name].toString() : file.getFileNameWithoutExtension());
    for (int i = copy.getNumChildren(); --i >= 0;)
    {
        const auto c = copy.getChild (i);
        const auto known = c.hasType (IDs::SongSection) || c.hasType (IDs::SongTrack) || c.hasType (IDs::SongCue);
        const auto orphan = c.hasType (IDs::SongCue) && (! sectionByUid (copy, c[IDs::section].toString()).isValid()
                                                         || ! trackByUid (copy, c[IDs::track].toString()).isValid());
        if (! known || orphan)
            copy.removeChild (i, nullptr);
    }
    return copy;
}

//==============================================================================
// A cue's name from its messages: "PC 5", "CC#43 = 2", "CC#11 move" (many values), "Note C3".
static juce::String nameFor (const std::vector<std::pair<double, juce::MidiMessage>>& events)
{
    int ccCount = 0, ccNumber = -1;
    for (const auto& [b, m] : events)
        if (m.isProgramChange())
            return "PC " + juce::String (m.getProgramChangeNumber());
    for (const auto& [b, m] : events)
        if (m.isController() && m.getControllerNumber() != 0 && m.getControllerNumber() != 32)
        {
            ++ccCount;
            if (ccNumber < 0)
                ccNumber = m.getControllerNumber();
        }
    for (const auto& [b, m] : events)
        if (m.isController() && m.getControllerNumber() == ccNumber)
            return ccCount > 3 ? "CC#" + juce::String (ccNumber) + " move"
                               : "CC#" + juce::String (ccNumber) + " = " + juce::String (m.getControllerValue());
    for (const auto& [b, m] : events)
        if (m.isNoteOn())
            return "Note " + juce::MidiMessage::getMidiNoteName (m.getNoteNumber(), true, true, 3);
    return "Cue";
}

// A bar of `quarters` quarter notes as a time signature: the smallest note value that counts it in whole beats
// (3/8 for 1.5, 7/16 for 1.75), else the nearest 1/64.
static std::pair<int, int> shortBar (double quarters)
{
    for (int den = 4; den <= 64; den *= 2)
    {
        const auto num = quarters * den / 4.0;
        if (std::abs (num - std::round (num)) < 1.0e-3 && std::round (num) >= 1.0 && std::round (num) <= maxBeatsPerBar)
            return { (int) std::round (num), den };
    }
    return { juce::jlimit (1, maxBeatsPerBar, juce::roundToInt (quarters * 16.0)), 64 };
}

MapImport importMap (const juce::MidiFile& file, const juce::String& songName, bool withCues)
{
    MapImport result;
    const auto tpq = (int) file.getTimeFormat();
    if (tpq <= 0)
    {
        result.error = "This file counts time in SMPTE frames, not beats, so it has no bars to read. Export it from your DAW "
                       "with a tempo map (in beats) instead.";
        return result;
    }

    std::map<double, double> tempos;                    // tick -> BPM
    std::map<double, std::pair<int, int>> meters;       // tick -> num / den
    std::vector<std::pair<double, juce::String>> markers;
    double endTick = 0.0;
    for (int t = 0; t < file.getNumTracks(); ++t)
    {
        const auto& seq = *file.getTrack (t);
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            const auto& m = seq.getEventPointer (i)->message;
            const auto tick = m.getTimeStamp();
            endTick = juce::jmax (endTick, tick);
            if (m.isTempoMetaEvent())
                // A file stores whole microseconds a beat (225 BPM = 266,666 us, which reads back as 225.0006): round to 0.01.
                tempos[tick] = std::round (6000.0 / juce::jmax (1.0e-6, m.getTempoSecondsPerQuarterNote())) / 100.0;
            else if (m.isTimeSignatureMetaEvent())
            {
                int num = 4, den = 4;
                m.getTimeSignatureInfo (num, den);
                meters[tick] = { juce::jlimit (1, maxBeatsPerBar, num), clampDen (den) };
            }
            else if (m.isTextMetaEvent() && (m.getMetaEventType() == 6 || m.getMetaEventType() == 7))   // marker, cue point
            {
                const auto text = m.getTextFromTextMetaEvent().trim();
                if (text.isNotEmpty())
                    markers.emplace_back (tick, text);
            }
        }
    }

    bool hasMessages = false;
    for (int t = 0; t < file.getNumTracks() && ! hasMessages; ++t)
        for (int i = 0; i < file.getTrack (t)->getNumEvents() && ! hasMessages; ++i)
            hasMessages = file.getTrack (t)->getEventPointer (i)->message.getChannel() > 0;
    if (tempos.empty() && meters.empty() && markers.empty() && ! (withCues && hasMessages))
    {
        result.error = "No markers, tempo or time signatures in this file. In your DAW, export the project (or the arrangement) "
                       "as a MIDI file with the tempo map and markers included.";
        return result;
    }

    std::sort (markers.begin(), markers.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    markers.erase (std::unique (markers.begin(), markers.end(), [] (const auto& a, const auto& b) { return a.first == b.first; }),
                   markers.end());

    const auto valueAt = [] (const auto& map, double tick, auto fallback)
    {
        auto v = fallback;
        for (const auto& [t, value] : map)
        {
            if (t > tick + 0.5)
                break;
            v = value;
        }
        return v;
    };

    // Where a section starts: every marker, every time signature change, and tempo changes (a ramp, with more than
    // four steps between two markers, is kept as the tempo at the marker).
    std::map<double, juce::String> starts;   // tick -> name ("" = continues the section before)
    starts[0.0] = {};
    for (const auto& [tick, name] : markers)
        starts[tick] = name;
    for (const auto& [tick, meter] : meters)
        if (starts.find (tick) == starts.end())
            starts[tick] = {};

    // Tempo changes don't start sections: like Reaper's tempo markers, each one lands inside its section at its exact
    // beat, so a change partway through a bar (Guitar Pro writes them) keeps the bars where they are.

    if (endTick <= starts.rbegin()->first)
        endTick = starts.rbegin()->first;   // the last section gets a default length below

    auto song = createSong (songName.isNotEmpty() ? songName : juce::String ("Imported song"));
    song.removeAllChildren (nullptr);

    juce::String lastName;
    std::map<juce::String, int> repeats;
    int partNumber = 0;
    for (auto it = starts.begin(); it != starts.end(); ++it)
    {
        const auto tick = it->first;
        const auto next = std::next (it) != starts.end() ? std::next (it)->first : endTick;
        const auto meter = valueAt (meters, tick, std::pair<int, int> { 4, 4 });
        const auto bpm = valueAt (tempos, tick, 120.0);
        // The file's tempo changes inside this stretch, as changes in the section(s) made for it.
        const auto addChanges = [&] (juce::ValueTree section, double fromTick, double toTick)
        {
            for (const auto& [t, value] : tempos)
                if (t > fromTick + 0.5 && t < toTick - 0.5 && std::abs (value - valueAt (tempos, t - 1.0, value)) > 1.0e-6)
                    addTempoChange (section, (t - fromTick) / tpq, value);
        };
        const auto bb = meter.first * 4.0 / meter.second;
        const auto lengthBeats = (next - tick) / tpq;

        juce::String name = it->second;
        if (name.isEmpty())
        {
            if (lastName.isEmpty())
                name = markers.empty() ? "Part " + juce::String (++partNumber) : juce::String ("Start");
            else
                name = lastName + " (" + juce::String (++repeats[lastName] + 1) + ")";
        }
        else
            lastName = name;

        if (it == starts.begin() && it->second.isEmpty() && lengthBeats <= 1.0e-6)
            continue;   // a marker at the very start: no empty "Start" section
        const auto isLast = std::next (it) == starts.end();
        if (lengthBeats <= 1.0e-6)
        {
            result.warnings.add ("\"" + name + "\" is the last marker and nothing follows it: it got 4 bars.");
            song.appendChild (createSection (name, 4, meter.first, meter.second, bpm), nullptr);
            continue;
        }

        // Whole bars, then, when the section stops partway through a bar (a tempo or time signature change mid-bar, a
        // pickup or cut-off bar), one short bar in a time signature that fits exactly, so nothing after it moves. The
        // song's last section is just filled up to its last bar line instead.
        auto bars = (int) std::floor (lengthBeats / bb + 1.0e-6);
        const auto rest = lengthBeats - bars * bb;
        if (rest > 1.0e-3 && isLast)
            ++bars;
        else if (rest > 1.0e-3)
        {
            const auto fit = shortBar (rest);
            const auto split = tick + bars * bb * tpq;
            if (bars > 0)
            {
                auto whole = createSection (name, bars, meter.first, meter.second, bpm);
                addChanges (whole, tick, split);
                song.appendChild (whole, nullptr);
            }
            auto last = createSection (bars > 0 ? name + " (end)" : name, 1, fit.first, fit.second, valueAt (tempos, split, bpm));
            addChanges (last, split, next);
            song.appendChild (last, nullptr);
            if (std::abs (fit.first * 4.0 / fit.second - rest) > 1.0e-3)
                result.warnings.add ("\"" + name + "\" ends partway through a bar: its last bar is " + juce::String (fit.first) + "/"
                                     + juce::String (fit.second) + ", as near as a time signature gets.");
            continue;
        }
        auto section = createSection (name, juce::jmax (1, bars), meter.first, meter.second, bpm);
        addChanges (section, tick, next);
        song.appendChild (section, nullptr);
    }

    // Drop a zero-length "Start" when the first marker sits at tick 0 (std::map merged it, so nothing to do) and
    // make sure there's at least one section.
    if (song.getNumChildren() == 0)
        song.appendChild (createSection ("Intro", 8, 4, 4, 120.0), nullptr);

    if (withCues)
    {
        int made = 0;
        for (int t = 0; t < file.getNumTracks(); ++t)
        {
            const auto& seq = *file.getTrack (t);
            juce::String name;
            std::vector<std::pair<double, juce::MidiMessage>> events;   // beats, channel messages only
            for (int i = 0; i < seq.getNumEvents(); ++i)
            {
                const auto& m = seq.getEventPointer (i)->message;
                if (m.isTextMetaEvent() && m.getMetaEventType() == 3 && name.isEmpty())
                    name = m.getTextFromTextMetaEvent().trim();
                else if (m.getChannel() > 0 && ! m.isNoteOff())   // note-offs follow their note-on in the same cue
                    events.emplace_back (m.getTimeStamp() / tpq, m);
                else if (m.isNoteOff() && ! events.empty())
                    events.emplace_back (m.getTimeStamp() / tpq, m);
            }
            if (events.empty())
                continue;
            std::stable_sort (events.begin(), events.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

            auto track = addTrack (song, name.isNotEmpty() ? name : "Track " + juce::String (made + 1), juce::Colour (0xff9b87f5));
            ++made;
            // Group: a cue runs while the next message is less than half a beat after the previous one.
            size_t start = 0;
            for (size_t i = 1; i <= events.size(); ++i)
            {
                if (i < events.size() && events[i].first - events[i - 1].first < 0.5)
                    continue;
                cues::Cue cue;
                const auto first = events[start].first;
                for (size_t k = start; k < i; ++k)
                    cue.add (events[k].first - first, events[k].second);
                cue.lengthBeats = juce::jmax (0.25, events[i - 1].first - first);
                cue.name = nameFor (cue.events);
                placeCue (song, track, first, cue, juce::Colour (0xff9b87f5));
                start = i;
            }
        }
        if (made == 0)
            result.warnings.add ("No MIDI messages in this file: only its map (sections, tempo, time signatures) came in.");
    }

    result.song = song;
    return result;
}
} // namespace songs
