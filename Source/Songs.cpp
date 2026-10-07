#include "Songs.h"
#include "State.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace songs
{
namespace
{
constexpr int ticksPerQuarter = 960;

juce::String newUid() { return juce::Uuid().toString(); }

int clampDen (int den)
{
    for (int d : { 1, 2, 4, 8, 16, 32 })
        if (den <= d)
            return d;
    return 32;
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
    s.setProperty (IDs::bars, juce::jlimit (1, 999, bars), nullptr);
    s.setProperty (IDs::timeNum, juce::jlimit (1, 32, timeNum), nullptr);
    s.setProperty (IDs::timeDen, clampDen (timeDen), nullptr);
    s.setProperty (IDs::bpm, juce::jlimit (20.0, 400.0, bpm), nullptr);
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
    const auto num = juce::jlimit (1, 32, (int) section.getProperty (IDs::timeNum, 4));
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

double songSeconds (const juce::ValueTree& song)
{
    double seconds = 0.0;
    for (const auto& s : song)
        if (s.hasType (IDs::SongSection))
            seconds += sectionLengthBeats (s) * 60.0 / juce::jlimit (20.0, 400.0, (double) s.getProperty (IDs::bpm, 120.0));
    return seconds;
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
    double start = 0.0, seconds = 0.0;
    for (const auto& s : song)
    {
        if (! s.hasType (IDs::SongSection))
            continue;
        const auto length = sectionLengthBeats (s);
        const auto spb = 60.0 / juce::jlimit (20.0, 400.0, (double) s.getProperty (IDs::bpm, 120.0));
        if (beat <= start + length)
            return seconds + (beat - start) * spb;
        seconds += length * spb;
        start += length;
    }
    return seconds;
}

double secondsToBeat (const juce::ValueTree& song, double seconds)
{
    double start = 0.0, elapsed = 0.0;
    for (const auto& s : song)
    {
        if (! s.hasType (IDs::SongSection))
            continue;
        const auto length = sectionLengthBeats (s);
        const auto spb = 60.0 / juce::jlimit (20.0, 400.0, (double) s.getProperty (IDs::bpm, 120.0));
        if (seconds <= elapsed + length * spb)
            return start + (seconds - elapsed) / spb;
        elapsed += length * spb;
        start += length;
    }
    return start;
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
        const auto bpm = juce::jlimit (20.0, 400.0, (double) s.getProperty (IDs::bpm, 120.0));
        const auto num = juce::jlimit (1, 32, (int) s.getProperty (IDs::timeNum, 4));
        const auto den = clampDen ((int) s.getProperty (IDs::timeDen, 4));
        if (std::abs (bpm - lastBpm) > 1.0e-6)
            conductor.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / bpm)), toTicks (beat));
        if (num != lastNum || den != lastDen)
            conductor.addEvent (juce::MidiMessage::timeSignatureMetaEvent (num, den), toTicks (beat));
        conductor.addEvent (juce::MidiMessage::textMetaEvent (6, s[IDs::name].toString()), toTicks (beat));
        lastBpm = bpm; lastNum = num; lastDen = den;
        beat += sectionLengthBeats (s);
    }

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
                tempos[tick] = 60.0 / juce::jmax (1.0e-6, m.getTempoSecondsPerQuarterNote());
            else if (m.isTimeSignatureMetaEvent())
            {
                int num = 4, den = 4;
                m.getTimeSignatureInfo (num, den);
                meters[tick] = { juce::jlimit (1, 32, num), clampDen (den) };
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

    std::vector<double> markerTicks { 0.0 };
    for (const auto& [tick, name] : markers)
        markerTicks.push_back (tick);
    markerTicks.push_back (std::numeric_limits<double>::max());
    for (size_t k = 0; k + 1 < markerTicks.size(); ++k)
    {
        std::vector<double> inside;
        for (const auto& [tick, bpm] : tempos)
            if (tick > markerTicks[k] + 0.5 && tick < markerTicks[k + 1] - 0.5)
                inside.push_back (tick);
        if (inside.size() > 4)
        {
            result.warnings.add ("A tempo ramp (" + juce::String ((int) inside.size()) + " tempo changes) was kept as the tempo where "
                                 "its section starts.");
            continue;
        }
        for (auto tick : inside)
            if (starts.find (tick) == starts.end())
                starts[tick] = {};
    }

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

        int bars = juce::roundToInt (lengthBeats / bb);
        if (lengthBeats <= 1.0e-6)
        {
            bars = 4;   // the last marker with nothing after it
            result.warnings.add ("\"" + name + "\" is the last marker and nothing follows it: it got 4 bars.");
        }
        else if (std::abs (lengthBeats / bb - bars) > 0.05 || bars < 1)
        {
            bars = juce::jmax (1, bars);
            result.warnings.add ("\"" + name + "\" doesn't end on a bar line: rounded to " + juce::String (bars)
                                 + (bars == 1 ? " bar." : " bars."));
        }
        if (it == starts.begin() && it->second.isEmpty() && lengthBeats <= 1.0e-6)
            continue;   // a marker at the very start: no empty "Start" section

        song.appendChild (createSection (name, bars, meter.first, meter.second, bpm), nullptr);
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
