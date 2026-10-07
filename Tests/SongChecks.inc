// Song builder (beta, Songs.cpp): the time map, cue snapshots, the song MIDI file and the DAW map import
// (included in CueModelTests.cpp).
{
    using namespace songs;
    const auto near = [] (double a, double b) { return std::abs (a - b) < 1.0e-6; };

    // The time map: 8 bars of 4/4 at 120, 4 bars of 7/8 at 140, 2 bars of 6/8.
    auto song = createSong ("Test");
    auto intro = song.getChild (0);
    song.appendChild (createSection ("Bridge", 4, 7, 8, 140.0), nullptr);
    song.appendChild (createSection ("Outro", 2, 6, 8, 140.0), nullptr);
    auto bridge = song.getChild (1);
    CHECK (near (barBeats (intro), 4.0) && near (barBeats (bridge), 3.5) && near (barBeats (song.getChild (2)), 3.0));
    CHECK (near (sectionStartBeat (song, bridge), 32.0) && near (songLengthBeats (song), 32.0 + 14.0 + 6.0) && songBars (song) == 14);
    CHECK (near (songSeconds (song), 32.0 * 0.5 + 20.0 * 60.0 / 140.0));
    CHECK (sectionAt (song, 31.99) == intro && sectionAt (song, 32.0) == bridge && barLabel (song, 32.0) == "Bar 9");
    CHECK (barLabel (song, 32.0 + 3.5 + 0.5) == "Bar 10, beat 2");   // 7/8: the beat is an eighth

    // A cue keeps its MIDI as placed (snapshot), sits in its section and moves with it.
    cues::Cue cue;
    cue.name = "Lead Rig";
    cue.lengthBeats = 1.0;
    cue.add (0.0, juce::MidiMessage::controllerEvent (1, 0, 0));
    cue.add (0.0, juce::MidiMessage::programChange (1, 5));
    cue.add (0.25, juce::MidiMessage::controllerEvent (1, 43, 2));
    cue.toggles = true;
    cue.padCc = 40;
    const auto back = cueFromTree (cueToTree (cue));
    CHECK (back.name == "Lead Rig" && back.events.size() == 3 && back.toggles && back.padCc == 40 && ! back.cc0IsControl
           && back.events[1].second.isProgramChange() && back.events[1].second.getProgramChangeNumber() == 5
           && near (back.events[2].first, 0.25) && back.events[2].second.getControllerValue() == 2);

    auto qcTrack = addTrack (song, "Quad Cortex", juce::Colours::blue);
    auto whTrack = addTrack (song, "Whammy", juce::Colours::red);
    CHECK (tracks (song).size() == 2 && tracks (song)[0] == qcTrack && includedTracks (song).size() == 2);
    auto placed = placeCue (song, whTrack, 34.0, cue, juce::Colours::red);
    CHECK (placed[IDs::section].toString() == bridge[IDs::uid].toString() && near ((double) placed[IDs::beat], 2.0)
           && placed[IDs::track].toString() == whTrack[IDs::uid].toString() && near (cueBeat (song, placed), 34.0));
    intro.setProperty (IDs::bars, 4, nullptr);            // shorter intro: the bridge and its cue move up 16 beats
    CHECK (near (cueBeat (song, placed), 18.0));
    moveCue (song, placed, qcTrack, 1.0);
    CHECK (placed[IDs::section].toString() == intro[IDs::uid].toString() && placed[IDs::track].toString() == qcTrack[IDs::uid].toString());
    moveCue (song, placed, qcTrack, 999.0);               // clamped inside the song
    CHECK (cueBeat (song, placed) < songLengthBeats (song));
    moveCue (song, placed, whTrack, 20.0);
    auto other = placeCue (song, qcTrack, 0.0, cue, juce::Colours::blue);
    moveTrack (song, whTrack, -1);                        // the Whammy track first
    CHECK (tracks (song)[0] == whTrack && tracks (song)[1] == qcTrack);
    moveTrack (song, whTrack, -1);                        // already first: no change
    CHECK (tracks (song)[0] == whTrack);
    removeSection (song, bridge);                         // its cue goes with it, the other stays
    CHECK (! song.getChildWithProperty (IDs::uid, bridge[IDs::uid]).isValid() && other.getParent() == song && ! placed.getParent().isValid());

    // A track goes with its cues.
    auto spare = addTrack (song, "Spare", juce::Colours::green);
    auto spareCue = placeCue (song, spare, 0.0, cue, juce::Colours::green);
    removeTrack (song, spare);
    CHECK (tracks (song).size() == 2 && ! spareCue.getParent().isValid());

    // The song as a MIDI file: tempo / time signature / markers in track 0, then the chosen tracks, named, events at their beats.
    auto s2 = createSong ("Breathe");
    s2.appendChild (createSection ("Verse", 4, 7, 8, 140.0), nullptr);
    auto qc2 = addTrack (s2, "QC Cues", juce::Colours::blue);
    auto wh2 = addTrack (s2, "Whammy Cues", juce::Colours::red);
    placeCue (s2, qc2, 32.0, cue, juce::Colours::red);    // the Verse's first beat
    placeCue (s2, wh2, 4.0, cue, juce::Colours::red);
    const auto midi = songMidi (s2, includedTracks (s2));
    CHECK (midi.getNumTracks() == 3 && midi.getTimeFormat() == 960);
    const auto& conductor = *midi.getTrack (0);
    int tempos = 0, meters = 0, markers = 0;
    bool verseAt32 = false, sevenEight = false;
    for (int i = 0; i < conductor.getNumEvents(); ++i)
    {
        const auto& m = conductor.getEventPointer (i)->message;
        if (m.isTempoMetaEvent()) ++tempos;
        if (m.isTimeSignatureMetaEvent()) { ++meters; int n = 0, d = 0; m.getTimeSignatureInfo (n, d); sevenEight |= (n == 7 && d == 8 && near (m.getTimeStamp(), 32.0 * 960)); }
        if (m.isTextMetaEvent() && m.getMetaEventType() == 6) { ++markers; verseAt32 |= (m.getTextFromTextMetaEvent() == "Verse" && near (m.getTimeStamp(), 32.0 * 960)); }
    }
    CHECK (tempos == 2 && meters == 2 && markers == 2 && verseAt32 && sevenEight);
    const auto& qcSeq = *midi.getTrack (1);
    bool pcAt32 = false, sceneAfter = false, named = false;
    for (int i = 0; i < qcSeq.getNumEvents(); ++i)
    {
        const auto& m = qcSeq.getEventPointer (i)->message;
        pcAt32 |= m.isProgramChange() && near (m.getTimeStamp(), 32.0 * 960);
        sceneAfter |= m.isController() && m.getControllerNumber() == 43 && near (m.getTimeStamp(), 32.25 * 960);
        named |= m.isTextMetaEvent() && m.getMetaEventType() == 3 && m.getTextFromTextMetaEvent() == "QC Cues";
    }
    CHECK (pcAt32 && sceneAfter && named);
    CHECK (songMidi (s2, includedTracks (s2), true).getNumTracks() == 1);   // one track for players that read only one
    wh2.setProperty (IDs::include, false, nullptr);                          // untick the Whammy track: only the QC's
    CHECK (includedTracks (s2).size() == 1 && songMidi (s2, includedTracks (s2)).getNumTracks() == 2);
    const auto justWhammy = songMidi (s2, { wh2[IDs::uid].toString() });     // one track dragged on its own
    CHECK (justWhammy.getNumTracks() == 2 && justWhammy.getTrack (1)->getNumEvents() > 3);

    // Same cues, another player: a duplicated track on channel 3 sends everything on 3; the original keeps its own.
    wh2.setProperty (IDs::include, true, nullptr);
    CHECK (channelsUsed (s2, qc2) == juce::Array<int> { 1 } && trackChannel (qc2) == 0);
    auto player2 = duplicateTrack (s2, qc2);
    CHECK (tracks (s2).size() == 3 && tracks (s2)[1] == player2 && channelsUsed (s2, player2) == juce::Array<int> { 1 });
    player2.setProperty (IDs::channel, 3, nullptr);
    const auto twoPlayers = songMidi (s2, { qc2[IDs::uid].toString(), player2[IDs::uid].toString() });
    const auto channelsIn = [] (const juce::MidiMessageSequence& seq)
    {
        juce::Array<int> ch;
        for (int i = 0; i < seq.getNumEvents(); ++i)
            if (seq.getEventPointer (i)->message.getChannel() > 0)
                ch.addIfNotAlreadyThere (seq.getEventPointer (i)->message.getChannel());
        return ch;
    };
    CHECK (twoPlayers.getNumTracks() == 3 && channelsIn (*twoPlayers.getTrack (1)) == juce::Array<int> { 1 }
           && channelsIn (*twoPlayers.getTrack (2)) == juce::Array<int> { 3 });
    CHECK (channelsUsed (s2, player2) == juce::Array<int> { 1 });   // the stored cues are untouched: only the export changes

    // Copy and paste: spacing kept; one track's cues land on the target track; several tracks' go back to their own.
    {
        auto s3 = createSong ("Paste");
        s3.getChild (0).setProperty (IDs::bars, 16, nullptr);
        auto a = addTrack (s3, "A", juce::Colours::red), b = addTrack (s3, "B", juce::Colours::blue);
        auto c1 = placeCue (s3, a, 4.0, cue, juce::Colours::red);
        auto c2 = placeCue (s3, a, 6.0, cue, juce::Colours::red);
        auto c3 = placeCue (s3, b, 5.0, cue, juce::Colours::blue);
        const auto oneTrackClip = copyCues (s3, { c2, c1 });
        CHECK (oneTrackClip.size() == 2 && near (oneTrackClip[0].offset, 0.0) && near (oneTrackClip[1].offset, 2.0)
               && near (clipLength (oneTrackClip), 3.0));
        const auto pasted = pasteCues (s3, oneTrackClip, 16.0, b);
        CHECK (pasted.size() == 2 && near (cueBeat (s3, pasted[0]), 16.0) && near (cueBeat (s3, pasted[1]), 18.0)
               && pasted[0][IDs::track] == b[IDs::uid] && pasted[0][IDs::name].toString() == "Lead Rig");
        const auto both = pasteCues (s3, copyCues (s3, { c1, c3 }), 32.0, b);   // two tracks: each to its own
        CHECK (both.size() == 2 && both[0][IDs::track] == a[IDs::uid] && both[1][IDs::track] == b[IDs::uid]
               && near (cueBeat (s3, both[1]), 33.0));
        CHECK (pasteCues (s3, oneTrackClip, 999.0, a).size() == 2);   // past the end: clamped inside the song
    }

    // A .mid clip dropped from the DAW becomes a cue: its notes / CCs / PCs in beats, no meta events.
    juce::MidiMessageSequence clip;
    clip.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);
    clip.addEvent (juce::MidiMessage::programChange (3, 7), 0.0);
    clip.addEvent (juce::MidiMessage::controllerEvent (3, 11, 64), 480.0);
    clip.addEvent (juce::MidiMessage::endOfTrack(), 960.0);
    juce::MidiFile clipFile;
    clipFile.setTicksPerQuarterNote (480);
    clipFile.addTrack (clip);
    const auto fromClip = cueFromMidiFile (clipFile, "Clip");
    CHECK (fromClip.events.size() == 2 && near (fromClip.events[1].first, 1.0) && near (fromClip.lengthBeats, 2.0) && fromClip.name == "Clip");

    // Import a DAW's map: markers, a 7/8 bridge and a tempo change; a marker off the bar line is rounded.
    juce::MidiMessageSequence map;
    map.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);                // 120
    map.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    map.addEvent (juce::MidiMessage::textMetaEvent (6, "Intro"), 0.0);
    map.addEvent (juce::MidiMessage::textMetaEvent (6, "Verse"), 16.0 * 480);      // bar 5
    map.addEvent (juce::MidiMessage::tempoMetaEvent (428571), 48.0 * 480);         // 140, at the bridge
    map.addEvent (juce::MidiMessage::timeSignatureMetaEvent (7, 8), 48.0 * 480);
    map.addEvent (juce::MidiMessage::textMetaEvent (6, "Bridge"), 48.0 * 480);     // bar 13
    map.addEvent (juce::MidiMessage::textMetaEvent (6, "Outro"), (48.0 + 14.0 + 1.0) * 480);   // 4 bars of 7/8 + a quarter
    map.addEvent (juce::MidiMessage::endOfTrack(), (48.0 + 14.0 + 1.0 + 7.0) * 480);
    juce::MidiFile daw;
    daw.setTicksPerQuarterNote (480);
    daw.addTrack (map);
    const auto imported = importMap (daw, "Imported");
    CHECK (imported.error.isEmpty() && imported.song.isValid() && imported.song.getNumChildren() == 4);
    if (imported.song.getNumChildren() == 4)
    {
        const auto& is = imported.song;
        CHECK (is.getChild (0)[IDs::name].toString() == "Intro" && (int) is.getChild (0)[IDs::bars] == 4);
        CHECK (is.getChild (1)[IDs::name].toString() == "Verse" && (int) is.getChild (1)[IDs::bars] == 8);
        CHECK (is.getChild (2)[IDs::name].toString() == "Bridge" && (int) is.getChild (2)[IDs::timeNum] == 7
               && (int) is.getChild (2)[IDs::timeDen] == 8 && std::abs ((double) is.getChild (2)[IDs::bpm] - 140.0) < 0.01
               && (int) is.getChild (2)[IDs::bars] == 4);                       // 15 beats of 7/8 = 4.29 bars: rounded
        CHECK (is.getChild (3)[IDs::name].toString() == "Outro" && (int) is.getChild (3)[IDs::bars] == 2);   // 7 beats of 7/8
        CHECK (imported.warnings.size() == 1 && imported.warnings[0].contains ("Bridge"));
    }
    juce::MidiFile empty;
    empty.setTicksPerQuarterNote (480);
    juce::MidiMessageSequence notes;
    notes.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0.0);
    empty.addTrack (notes);
    CHECK (importMap (empty, "x").error.isNotEmpty());   // nothing to read: an explanation, no song
    juce::MidiFile smpte;
    smpte.setSmpteTimeFormat (25, 40);
    smpte.addTrack (map);
    CHECK (importMap (smpte, "x").error.contains ("SMPTE"));

    // Changes between markers start a section of their own.
    juce::MidiMessageSequence noMarkers;
    noMarkers.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
    noMarkers.addEvent (juce::MidiMessage::timeSignatureMetaEvent (3, 4), 8.0 * 480);
    noMarkers.addEvent (juce::MidiMessage::endOfTrack(), 14.0 * 480);
    juce::MidiFile nm;
    nm.setTicksPerQuarterNote (480);
    nm.addTrack (noMarkers);
    const auto parts = importMap (nm, "Parts");
    CHECK (parts.song.getNumChildren() == 2 && parts.song.getChild (0)[IDs::name].toString() == "Part 1"
           && (int) parts.song.getChild (1)[IDs::timeNum] == 3 && (int) parts.song.getChild (1)[IDs::bars] == 2);

    // Songs survive a project load (sanitise) and loading a setup leaves them alone.
    juce::ValueTree root (IDs::PedalCues);
    songsNode (root).appendChild (createSong ("Kept"), nullptr);
    root.setProperty (IDs::selectedSong, songsNode (root).getChild (0)[IDs::uid], nullptr);
    state::sanitise (root);
    CHECK (root.getChildWithName (IDs::Songs).getNumChildren() == 1 && selectedSong (root)[IDs::name].toString() == "Kept");

    // A song file round trip: sections, tracks (with their channel) and cues come back, with new ids.
    {
        const auto file = juce::File::createTempFile (songFileExtension);
        CHECK (saveSongFile (s2, file));
        juce::String error;
        const auto back2 = loadSongFile (file, error);
        CHECK (error.isEmpty() && back2.isValid() && back2[IDs::name].toString() == "Breathe" && back2[IDs::uid] != s2[IDs::uid]
               && tracks (back2).size() == tracks (s2).size() && near (songLengthBeats (back2), songLengthBeats (s2))
               && songMidi (back2, includedTracks (back2)).getNumTracks() == songMidi (s2, includedTracks (s2)).getNumTracks());
        int cuesBack = 0, cuesThere = 0;
        for (const auto& c : back2) cuesBack += c.hasType (IDs::SongCue) ? 1 : 0;
        for (const auto& c : s2) cuesThere += c.hasType (IDs::SongCue) ? 1 : 0;
        CHECK (cuesBack == cuesThere && trackChannel (tracks (back2)[1]) == 3);
        file.replaceWithText ("<NotASong/>");
        CHECK (! loadSongFile (file, error).isValid() && error.isNotEmpty());
        file.deleteFile();
    }

    // Playback timing through the tempo map: Intro 8 bars at 120 (16 s), then a 7/8 Verse at 140.
    CHECK (near (beatToSeconds (s2, 32.0), 16.0) && near (beatToSeconds (s2, 32.0 + 3.5), 16.0 + 1.5) && near (secondsToBeat (s2, 17.5), 35.5));
    const auto fromVerse = playbackEvents (s2, { qc2[IDs::uid].toString() }, 32.0);   // the QC track from the Verse
    CHECK (fromVerse.size() == 3 && near (fromVerse[0].first, 0.0) && near (fromVerse[2].first, 0.25 * 60.0 / 140.0)
           && fromVerse[2].second.isController());
    CHECK (playbackEvents (s2, { qc2[IDs::uid].toString() }, 0.0).size() == 3);       // nothing earlier on that track

    // A track's channel rewrites its cues: the ones there, and the ones dropped, moved or pasted onto it later.
    {
        auto s4 = createSong ("Channels");
        auto t1 = addTrack (s4, "QC", juce::Colours::blue), t2 = addTrack (s4, "QC 2", juce::Colours::red);
        auto onThree = cue;   // channel 1 in the snapshot
        auto a = placeCue (s4, t1, 0.0, onThree, juce::Colours::blue);
        setTrackChannel (s4, t1, 3);
        CHECK (channelsUsed (s4, t1) == juce::Array<int> { 3 } && trackChannel (t1) == 3);
        auto b = placeCue (s4, t1, 4.0, cue, juce::Colours::blue);          // dropped later: on 3 too
        CHECK (channelsUsed (s4, t1) == juce::Array<int> { 3 });
        setTrackChannel (s4, t2, 5);
        moveCue (s4, b, t2, 4.0);                                             // moved to the other player's track: 5
        CHECK (channelsUsed (s4, t2) == juce::Array<int> { 5 } && channelsUsed (s4, t1) == juce::Array<int> { 3 });
        pasteCues (s4, copyCues (s4, { a }), 8.0, t2);                        // pasted there: 5
        CHECK (channelsUsed (s4, t2) == juce::Array<int> { 5 });
        setTrackChannel (s4, t1, 0);                                          // own channels: left as they are
        CHECK (channelsUsed (s4, t1) == juce::Array<int> { 3 } && trackChannel (t1) == 0);
        juce::ValueTree meta (IDs::SongCue);
        meta.setProperty (IDs::events, "0:ff0304414243 0:b00000", nullptr);   // a track name and a CC: only the CC moves
        setCueChannel (meta, 7);
        const auto mc = cueFromTree (meta);
        CHECK (mc.events.size() == 2 && mc.events[0].second.isMetaEvent() && mc.events[1].second.getChannel() == 7);
    }

    // Import a whole song from a MIDI file: the map, and each MIDI track's messages as cues (grouped by timing).
    {
        auto src = createSong ("Round trip");
        src.appendChild (createSection ("Verse", 4, 7, 8, 140.0), nullptr);
        auto qcT = addTrack (src, "Quad Cortex", juce::Colours::blue);
        auto whT = addTrack (src, "Whammy V", juce::Colours::red);
        placeCue (src, qcT, 0.0, cue, juce::Colours::blue);     // CC0, PC 5 and the scene 1/16 later: one cue
        placeCue (src, qcT, 32.0, cue, juce::Colours::blue);
        cues::Cue sweep;
        for (int k = 0; k < 16; ++k)
            sweep.add (k * 0.25, juce::MidiMessage::controllerEvent (2, 11, k * 8));
        sweep.lengthBeats = 4.0;
        placeCue (src, whT, 8.0, sweep, juce::Colours::red);
        const auto back = importMap (songMidi (src, includedTracks (src)), "Round trip", true);
        CHECK (back.error.isEmpty() && back.song.isValid() && tracks (back.song).size() == 2
               && tracks (back.song)[0][IDs::name].toString() == "Quad Cortex" && tracks (back.song)[1][IDs::name].toString() == "Whammy V");
        int qcCues = 0, whCues = 0;
        juce::ValueTree sweepCue;
        for (const auto& c : back.song)
            if (c.hasType (IDs::SongCue))
            {
                if (c[IDs::track] == tracks (back.song)[0][IDs::uid]) ++qcCues;
                else { ++whCues; sweepCue = c; }
            }
        CHECK (qcCues == 2 && whCues == 1 && near (cueBeat (back.song, sweepCue), 8.0) && sweepCue[IDs::name].toString() == "CC#11 move"
               && cueFromTree (sweepCue).events.size() == 16);
        const auto firstQc = std::find_if (back.song.begin(), back.song.end(), [&] (const juce::ValueTree& c)
                                           { return c.hasType (IDs::SongCue) && c[IDs::track] == tracks (back.song)[0][IDs::uid]; });
        CHECK ((*firstQc)[IDs::name].toString() == "PC 5" && cueFromTree (*firstQc).events.size() == 3);
        CHECK ((int) back.song.getChild (1)[IDs::timeNum] == 7);   // the map came too
        // A file with only messages (no map) still comes in, as one section at 120.
        juce::MidiFile bare;
        bare.setTicksPerQuarterNote (480);
        juce::MidiMessageSequence one;
        one.addEvent (juce::MidiMessage::programChange (1, 3), 0.0);
        one.addEvent (juce::MidiMessage::programChange (1, 4), 16.0 * 480);
        one.addEvent (juce::MidiMessage::endOfTrack(), 32.0 * 480);
        bare.addTrack (one);
        const auto bareSong = importMap (bare, "Bare", true);
        CHECK (bareSong.error.isEmpty() && tracks (bareSong.song).size() == 1 && importMap (bare, "Bare", false).error.isNotEmpty());
    }
}
