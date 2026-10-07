#pragma once

#include "CueModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

// Song Builder (beta): a song is a list of sections (name, bars, time signature, tempo) and tracks the user names (one per
// device, or however they like), with cues dropped onto the tracks from the tabs' tiles. Cues are snapshots of the MIDI
// the tile sent when it was dropped, positioned relative to their section, so changing a section's length moves everything
// after it. Positions are in beats (quarter notes).
//
// State: Songs > Song { name, uid } > SongSection { name, uid, bars, timeNum, timeDen, bpm, colour }
//                                   > SongTrack { name, uid, colour, include }
//                                   > SongCue { track (uid), section (uid), beat, name, colour, lengthBeats, toggles,
//                                               cc0IsControl, padCc, events }
namespace songs
{
juce::ValueTree songsNode (juce::ValueTree root);                 // created when missing
juce::ValueTree createSong (const juce::String& name);            // one 8-bar "Intro" at 120 BPM, 4/4, no tracks yet
juce::ValueTree createSection (const juce::String& name, int bars, int timeNum, int timeDen, double bpm);
juce::ValueTree selectedSong (juce::ValueTree root);              // the one the Song Builder shows (or the first)

// The time map.
double barBeats (const juce::ValueTree& section);                 // one bar in quarter notes: 4/4 = 4, 7/8 = 3.5, 6/8 = 3
double sectionLengthBeats (const juce::ValueTree& section);
double sectionStartBeat (const juce::ValueTree& song, const juce::ValueTree& section);
double songLengthBeats (const juce::ValueTree& song);
int songBars (const juce::ValueTree& song);
double songSeconds (const juce::ValueTree& song);
juce::ValueTree sectionAt (const juce::ValueTree& song, double beat);   // the section that holds this beat (or the last)
juce::ValueTree sectionByUid (const juce::ValueTree& song, const juce::String& uid);
juce::String barLabel (const juce::ValueTree& song, double beat);       // "Bar 9" or "Bar 9, beat 3"
double beatToSeconds (const juce::ValueTree& song, double beat);         // through each section's tempo
double secondsToBeat (const juce::ValueTree& song, double seconds);

// Play from a beat: the chosen tracks' messages (with their channels) at seconds from that beat, through the tempo map.
std::vector<std::pair<double, juce::MidiMessage>> playbackEvents (const juce::ValueTree& song, const juce::StringArray& trackUids,
                                                                  double fromBeat);

// Tracks, in the order they're shown and exported.
juce::ValueTree addTrack (juce::ValueTree song, const juce::String& name, juce::Colour);
std::vector<juce::ValueTree> tracks (const juce::ValueTree& song);
juce::ValueTree trackByUid (const juce::ValueTree& song, const juce::String& uid);
void moveTrack (juce::ValueTree song, juce::ValueTree track, int delta);   // -1 up, +1 down
void removeTrack (juce::ValueTree song, juce::ValueTree track);           // and its cues
juce::StringArray includedTracks (const juce::ValueTree& song);           // the uids ticked for export
juce::ValueTree duplicateTrack (juce::ValueTree song, const juce::ValueTree& track);   // with its cues, right after it
// A track's channel: 0 = each cue keeps the channel it was dropped with; 1-16 = the track's cues are on that channel.
// Setting one rewrites the cues already on the track, and every cue dropped, moved or pasted onto it later
// (one set of cues for two players: duplicate the track, give the copy the other player's channel).
int trackChannel (const juce::ValueTree& track);
void setTrackChannel (juce::ValueTree song, juce::ValueTree track, int channel);
void setCueChannel (juce::ValueTree cue, int channel);   // every channel message of the cue (meta and SysEx stay)
juce::Array<int> channelsUsed (const juce::ValueTree& song, const juce::ValueTree& track);   // by its cues, sorted

// Cues.
juce::ValueTree cueToTree (const cues::Cue&);
cues::Cue cueFromTree (const juce::ValueTree&);
cues::Cue cueFromMidiFile (const juce::MidiFile&, const juce::String& name);   // a .mid dropped from the DAW or Finder
juce::ValueTree placeCue (juce::ValueTree song, const juce::ValueTree& track, double beat, const cues::Cue&, juce::Colour);
void moveCue (juce::ValueTree song, juce::ValueTree cue, const juce::ValueTree& track, double beat);
double cueBeat (const juce::ValueTree& song, const juce::ValueTree& cue);   // absolute
void removeSection (juce::ValueTree song, juce::ValueTree section);         // and its cues

// Copy and paste: copied cues keep their spacing (offsets from the earliest) and their track. Pasting cues from one track
// puts them on `track`; cues from several tracks go back on their own tracks when the song has them, else on `track`.
struct ClipCue { juce::ValueTree cue; double offset = 0.0; juce::String track; };
std::vector<ClipCue> copyCues (const juce::ValueTree& song, const std::vector<juce::ValueTree>& cues);
std::vector<juce::ValueTree> pasteCues (juce::ValueTree song, const std::vector<ClipCue>&, double beat, const juce::ValueTree& track);
double clipLength (const std::vector<ClipCue>&);   // from the first cue's start to the last one's end, in beats

// The song as a Standard MIDI File: the tempo map, time signatures and a marker per section in the first track, then one
// track per exported track (trackUids, in the song's order), named after it. singleTrack puts everything in one track
// (some backing-track players read only one; every message keeps its channel).
juce::MidiFile songMidi (const juce::ValueTree& song, const juce::StringArray& trackUids, bool singleTrack = false);
juce::File writeSongFile (const juce::ValueTree& song, const juce::StringArray& trackUids, bool singleTrack = false,
                          const juce::String& fileName = {});

// Song files (.pedalcues-song): one song with its sections, tracks and cues, to keep or share. Opening one gives it new
// ids (copySong), so the same file can be opened twice or into a project that already has it.
inline const juce::String songFileExtension { ".pedalcues-song" };
juce::ValueTree copySong (const juce::ValueTree& song, const juce::String& name);
bool saveSongFile (const juce::ValueTree& song, const juce::File&);
juce::ValueTree loadSongFile (const juce::File&, juce::String& error);   // invalid on error

// Import a DAW's map from a MIDI file: markers become sections, with the tempo and time signature in force there.
// A tempo or time signature change between markers starts a new section ("Verse (2)"); markers off the bar line are
// rounded to the nearest bar. Without markers, every time signature change starts a section.
struct MapImport
{
    juce::ValueTree song;          // invalid when nothing could be read
    juce::StringArray warnings;    // rounded markers, ramps simplified...
    juce::String error;
};
// withCues: each MIDI track with channel messages also becomes a song track (named after the MIDI track), its messages
// grouped into cues: messages less than half a beat apart are one cue (a preset load and its scene, a pedal sweep).
MapImport importMap (const juce::MidiFile&, const juce::String& songName, bool withCues = false);
} // namespace songs
