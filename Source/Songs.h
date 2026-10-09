#pragma once

#include "CueModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <memory>
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
// What a section can be. The time signature follows what a MIDI file can store: up to 255 beats in a bar, and a note value
// that's a power of two (1-64). The tempo and length ranges are just generous.
constexpr int maxBeatsPerBar = 255, maxBars = 9999;
constexpr double minBpm = 10.0, maxBpm = 960.0;
inline bool isNoteValue (int den) { return den >= 1 && den <= 64 && (den & (den - 1)) == 0; }

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
// Past the song's end it carries on at the end tempo (secondsToBeat stops at the end): for a backing track that runs longer.
double secondsToBeatOn (const juce::ValueTree& song, double seconds);
// The tempo map, like Reaper's tempo markers: each section starts at its own bpm, and can hold tempo changes at any beat
// inside it (SongTempo children, `beat` in quarters from the section's start), so a change partway through a bar never
// moves a bar line. The section's start and every change can glide (tempoRamp), evenly per beat, to the next change or
// the next section's tempo, like Reaper's "gradually transition to next marker". The song's very end stays steady.
struct TempoSegment { double from, to, bpm0, bpm1; };   // song beats; bpm1 == bpm0 when steady
std::vector<TempoSegment> tempoSegments (const juce::ValueTree& song);
std::vector<juce::ValueTree> tempoChanges (const juce::ValueTree& section);            // inside it, by beat
juce::ValueTree addTempoChange (juce::ValueTree section, double beat, double bpm, bool gradual = false);
bool rampsTempo (const juce::ValueTree& song, const juce::ValueTree& section);          // its start glides
double sectionEndBpm (const juce::ValueTree& song, const juce::ValueTree& section);   // the tempo where it ends
double tempoAt (const juce::ValueTree& song, double beat);

// The metronome from a beat: a click on every beat of each section's time signature (the eighth in 7/8), accented on bar
// starts, through the tempo map; countInBars bars of clicks first (in the starting section's time signature and tempo).
// Times are seconds from pressing Play; countInSeconds is how long the count-in lasts (the song's MIDI starts after it).
struct Click { double seconds; bool accent; bool sub = false; };   // accent: a bar's first beat; sub: between the beats
double countInSeconds (const juce::ValueTree& song, double fromBeat, int countInBars);
std::vector<Click> metronomeClicks (const juce::ValueTree& song, double fromBeat, int countInBars);

// How a section's click counts (SongSection clickDiv): its beat, quarters, eighths, sixteenths, triplets, only bar starts,
// or nothing. The count-in always counts the beat.
juce::StringArray clickDivNames();   // "Beat", "1/4", "1/8", "1/16", "1/8 T", "1/16 T", "Bars only", "Off"
double clickStepBeats (const juce::ValueTree& section);   // 0: no click in it

// Grid steps for snapping (quarter notes): 0 = the section's beat, then bar, 1/2, 1/4, 1/8, 1/16, 1/32, 1/4T, 1/8T, 1/16T.
juce::StringArray gridStepNames();
double gridStepBeats (int step, const juce::ValueTree& section);
double snapBeat (const juce::ValueTree& song, double beat, int step);   // to the grid, counted from its section's start

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

// ---- Backing-track creator --------------------------------------------------------------------------------------
// A copy of the song with `bars` bars of count-in in front (the first section's time signature and tempo, named
// "Count-in"), for exports that start with it; cues keep their places.
juce::ValueTree withCountIn (const juce::ValueTree& song, int bars);

// The click: its sound (0 beep, 1 click, 2 wood block, 3 cowbell), an accented first beat, volume.
struct ClickSound
{
    int sound = 0;              // 0 beep, 1 click, 2 wood block, 3 cowbell, 4 custom samples
    bool accent = true;
    float gain = 0.7f;
    // Custom samples (sound 4), mono at sampleRate: the accent and the beat (the beat one also plays the subdivisions, softer).
    std::shared_ptr<juce::AudioBuffer<float>> accentSample, beatSample;
    double sampleRate = 48000.0;
};
juce::StringArray clickSoundNames();
ClickSound clickSettings (const juce::ValueTree& songsNode);
constexpr double clickSeconds = 0.05;
float clickSample (const ClickSound&, bool accented, double t, juce::Random* noise = nullptr, bool sub = false);   // t: seconds in
double clickLength (const ClickSound&);   // seconds: clickSeconds, or the longest custom sample (up to 2 s)
// The whole song's click as audio (mono), from the count-in (if any) to the end plus tailSeconds of silence.
juce::AudioBuffer<float> renderClickTrack (const juce::ValueTree& song, int countInBars, double sampleRate, const ClickSound&,
                                           double tailSeconds = 2.0);

// Fit the tempo to the audio: make bar `bar` (1 = the first) start at `fileSeconds` in the backing track, bar 1 staying at
// the song's audio offset, by scaling every section's tempo by the same factor. False when it can't (bar 1, or before it).
bool fitTempoToAudio (juce::ValueTree song, int bar, double fileSeconds);
// How many bars the last section needs to cover a backing track `audioSeconds` long (bar 1 at the song's audioOffset),
// at the tempo it ends on; 0 when the song already reaches the audio's end. extendLastSection adds them.
int barsToCoverAudio (const juce::ValueTree& song, double audioSeconds);
void extendLastSection (juce::ValueTree song, int bars);
double barStartBeat (const juce::ValueTree& song, int bar);   // 1-based, through the time signatures
int barNumberAt (const juce::ValueTree& song, double beat);   // 1-based
double tempoFromTaps (const std::vector<double>& tapSeconds); // BPM from the median gap (0 below 2 taps)

// Play: the tracks it plays (ticked, not muted; only the soloed ones when any is soloed).
juce::StringArray playedTracks (const juce::ValueTree& song);
// The section Play loops, if any (its start and end beat).
bool loopRange (const juce::ValueTree& song, double& start, double& end);

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
