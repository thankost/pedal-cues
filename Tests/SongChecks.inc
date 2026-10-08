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

    // Import a DAW's map: markers, a 7/8 bridge and a tempo change; a marker off the bar line gets an exact short bar.
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
    CHECK (imported.error.isEmpty() && imported.song.isValid() && imported.song.getNumChildren() == 5);
    if (imported.song.getNumChildren() == 5)
    {
        const auto& is = imported.song;
        CHECK (is.getChild (0)[IDs::name].toString() == "Intro" && (int) is.getChild (0)[IDs::bars] == 4);
        CHECK (is.getChild (1)[IDs::name].toString() == "Verse" && (int) is.getChild (1)[IDs::bars] == 8);
        CHECK (is.getChild (2)[IDs::name].toString() == "Bridge" && (int) is.getChild (2)[IDs::timeNum] == 7
               && (int) is.getChild (2)[IDs::timeDen] == 8 && std::abs ((double) is.getChild (2)[IDs::bpm] - 140.0) < 0.01
               && (int) is.getChild (2)[IDs::bars] == 4);                       // 15 beats of 7/8 = 4 bars and a quarter...
        CHECK (is.getChild (3)[IDs::name].toString() == "Bridge (end)" && (int) is.getChild (3)[IDs::bars] == 1
               && (int) is.getChild (3)[IDs::timeNum] == 1 && (int) is.getChild (3)[IDs::timeDen] == 4);   // ...kept as a 1/4 bar
        CHECK (is.getChild (4)[IDs::name].toString() == "Outro" && (int) is.getChild (4)[IDs::bars] == 2);   // 7 beats of 7/8
        CHECK (near (songLengthBeats (is), 70.0));   // the file's length, to the beat: nothing after the bridge moved
        CHECK (imported.warnings.isEmpty());
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

    // A short bar picks the note value that counts it exactly (1.75 quarters = 7/16); the song's end is filled to a bar.
    {
        juce::MidiMessageSequence odd;
        odd.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        odd.addEvent (juce::MidiMessage::timeSignatureMetaEvent (3, 4), (8.0 + 1.75) * 480);   // 2 bars of 4/4 and 7/16
        odd.addEvent (juce::MidiMessage::endOfTrack(), (8.0 + 1.75 + 4.0) * 480);               // 1 bar of 3/4 and a quarter
        juce::MidiFile of;
        of.setTicksPerQuarterNote (480);
        of.addTrack (odd);
        const auto r = importMap (of, "Odd");
        CHECK (r.song.getNumChildren() == 3 && r.warnings.isEmpty());
        if (r.song.getNumChildren() == 3)
        {
            CHECK ((int) r.song.getChild (0)[IDs::bars] == 2 && (int) r.song.getChild (1)[IDs::timeNum] == 7
                   && (int) r.song.getChild (1)[IDs::timeDen] == 16 && (int) r.song.getChild (1)[IDs::bars] == 1);
            CHECK ((int) r.song.getChild (2)[IDs::timeNum] == 3 && (int) r.song.getChild (2)[IDs::bars] == 2);   // filled up
        }
    }

    // A tempo change partway through a bar (Guitar Pro writes them) stays exactly there, inside its section, like a
    // Reaper tempo marker: no bar moves, no section starts there, nothing to warn about.
    {
        juce::MidiMessageSequence gp;
        gp.addEvent (juce::MidiMessage::tempoMetaEvent (444444), 0.0);                      // 135
        gp.addEvent (juce::MidiMessage::timeSignatureMetaEvent (9, 8), 0.0);
        gp.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 4.5 * 480);          // 1 bar of 9/8
        gp.addEvent (juce::MidiMessage::tempoMetaEvent (250000), 4.5 * 480 + 551);          // 240, 1.15 beats into the bar
        gp.addEvent (juce::MidiMessage::endOfTrack(), (4.5 + 16.0) * 480);
        juce::MidiFile f;
        f.setTicksPerQuarterNote (480);
        f.addTrack (gp);
        const auto r = importMap (f, "GP");
        CHECK (r.song.getNumChildren() == 2 && near (songLengthBeats (r.song), 20.5) && r.warnings.isEmpty());
        if (r.song.getNumChildren() == 2)
        {
            const auto four = r.song.getChild (1);
            const auto changes = tempoChanges (four);
            CHECK ((int) four[IDs::bars] == 4 && std::abs ((double) four[IDs::bpm] - 135.0) < 0.01 && changes.size() == 1);
            if (changes.size() == 1)
                CHECK (near ((double) changes[0][IDs::beat], 551.0 / 480.0) && std::abs ((double) changes[0][IDs::bpm] - 240.0) < 0.01);
            // The time follows it exactly: 4.5 + 1.148 beats at 135, the rest at 240.
            CHECK (std::abs (songSeconds (r.song) - ((4.5 + 551.0 / 480.0) * 60.0 / 135.0 + (16.0 - 551.0 / 480.0) * 60.0 / 240.0)) < 1.0e-3);
        }
    }

    // A gradual tempo change (tempoRamp): even per beat from 120 to 60 over 8 beats, then steady 60. The time is
    // 60 / k * ln (b1 / b0) with k = -7.5 BPM a beat: 5.545 s; seconds and beats convert both ways; the export steps it.
    {
        auto g = createSong ("Ritardando");
        g.removeAllChildren (nullptr);
        auto slow = createSection ("Rit.", 2, 4, 4, 120.0);
        slow.setProperty (IDs::tempoRamp, true, nullptr);
        g.appendChild (slow, nullptr);
        g.appendChild (createSection ("End", 1, 4, 4, 60.0), nullptr);
        CHECK (rampsTempo (g, slow) && ! rampsTempo (g, g.getChild (1)));
        CHECK (std::abs (beatToSeconds (g, 8.0) - 60.0 / -7.5 * std::log (60.0 / 120.0)) < 1.0e-9);
        CHECK (std::abs (songSeconds (g) - (beatToSeconds (g, 8.0) + 4.0)) < 1.0e-9);
        CHECK (std::abs (tempoAt (g, 4.0) - 90.0) < 1.0e-9 && std::abs (tempoAt (g, 9.0) - 60.0) < 1.0e-9);
        for (double b : { 0.5, 3.0, 7.75, 10.0 })
            CHECK (std::abs (secondsToBeat (g, beatToSeconds (g, b)) - b) < 1.0e-6);
        CHECK (beatToSeconds (g, 1.0) < beatToSeconds (g, 8.0) - beatToSeconds (g, 7.0));   // slowing down: later beats are longer
        // The last section has nothing to glide to: steady.
        g.getChild (1).setProperty (IDs::tempoRamp, true, nullptr);
        CHECK (! rampsTempo (g, g.getChild (1)) && std::abs (sectionEndBpm (g, g.getChild (1)) - 60.0) < 1.0e-9);
        // Exported as a tempo every 1/16 whose times add up to the glide's.
        const auto midi = songMidi (g, {});
        int tempoEvents = 0;
        const auto ticksPerQuarter = (double) midi.getTimeFormat();
        double fileSeconds = 0.0, lastTick = 0.0, spq = 0.5;
        const auto& conductor = *midi.getTrack (0);
        for (int i = 0; i < conductor.getNumEvents(); ++i)
        {
            const auto& m = conductor.getEventPointer (i)->message;
            if (! m.isTempoMetaEvent())
                continue;
            ++tempoEvents;
            fileSeconds += (m.getTimeStamp() - lastTick) / ticksPerQuarter * spq;
            lastTick = m.getTimeStamp();
            spq = m.getTempoSecondsPerQuarterNote();
        }
        fileSeconds += (12.0 * ticksPerQuarter - lastTick) / ticksPerQuarter * spq;
        CHECK (tempoEvents == 33 && std::abs (fileSeconds - songSeconds (g)) < 1.0e-3);
    }

    // Import: a ritardando written as a tempo step every bar comes in as those steps, in one section.
    {
        juce::MidiMessageSequence rit;
        rit.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        rit.addEvent (juce::MidiMessage::tempoMetaEvent (500000), 0.0);   // 120 for 4 bars
        int bar = 4;
        for (double bpm : { 110.0, 100.0, 90.0, 80.0 })   // then 110, 100, 90 and 80 a bar each
            rit.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / bpm)), bar++ * 4.0 * 480);
        rit.addEvent (juce::MidiMessage::endOfTrack(), 12.0 * 4.0 * 480);
        juce::MidiFile rf;
        rf.setTicksPerQuarterNote (480);
        rf.addTrack (rit);
        const auto r = importMap (rf, "Rit");
        CHECK (r.song.getNumChildren() == 1 && near (songLengthBeats (r.song), 48.0) && r.warnings.isEmpty());
        if (r.song.getNumChildren() == 1)
            CHECK (tempoChanges (r.song.getChild (0)).size() == 4 && std::abs (tempoAt (r.song, 26.0) - 90.0) < 0.01);
    }

    // Tempo changes inside a section, steady and gliding, and the export writes them where they are.
    {
        auto t = createSong ("Changes");
        t.removeAllChildren (nullptr);
        auto sec = createSection ("A", 4, 4, 4, 120.0);   // 16 beats
        addTempoChange (sec, 6.0, 90.0, true);            // 90 at beat 6, gliding to...
        addTempoChange (sec, 10.0, 150.0);                // ...150 at beat 10, then steady
        t.appendChild (sec, nullptr);
        CHECK (std::abs (tempoAt (t, 3.0) - 120.0) < 1.0e-9 && std::abs (tempoAt (t, 8.0) - 120.0) < 1.0e-9
               && std::abs (tempoAt (t, 12.0) - 150.0) < 1.0e-9);
        const auto glide = 60.0 / 15.0 * std::log (150.0 / 90.0);   // k = 60 BPM over 4 beats
        CHECK (std::abs (songSeconds (t) - (6.0 * 0.5 + glide + 6.0 * 0.4)) < 1.0e-9);
        CHECK (std::abs (secondsToBeat (t, beatToSeconds (t, 8.5)) - 8.5) < 1.0e-6);
        CHECK (std::abs (sectionEndBpm (t, sec) - 150.0) < 1.0e-9);
        const auto midi = songMidi (t, {});
        int tempos = 0;
        for (int i = 0; i < midi.getTrack (0)->getNumEvents(); ++i)
            if (midi.getTrack (0)->getEventPointer (i)->message.isTempoMetaEvent())
                ++tempos;
        CHECK (tempos == 1 + 16 + 1);   // 120, the glide in 1/16 steps, 150
        // Fit the tempo scales the changes too.
        t.setProperty (IDs::audioOffset, 0.0, nullptr);
        CHECK (fitTempoToAudio (t, 2, beatToSeconds (t, 4.0) / 2.0) && std::abs ((double) tempoChanges (sec)[1][IDs::bpm] - 300.0) < 0.01);
    }

    // A tempo the file can only store approximately (225 BPM = 266,666 us a beat) comes in as 225, not 225.0006.
    {
        juce::MidiMessageSequence q;
        q.addEvent (juce::MidiMessage::tempoMetaEvent (266666), 0.0);
        q.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        q.addEvent (juce::MidiMessage::endOfTrack(), 16.0 * 480);
        juce::MidiFile qf;
        qf.setTicksPerQuarterNote (480);
        qf.addTrack (q);
        CHECK ((double) importMap (qf, "Q").song.getChild (0)[IDs::bpm] == 225.0);
    }

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

    // The metronome: every beat (eighths in 7/8) through the tempo map, accents on bar starts, a count-in in the start's bar.
    {
        auto m = createSong ("Click");
        m.getChild (0).setProperty (IDs::bars, 2, nullptr);                      // 2 bars of 4/4 at 120: 8 clicks, 0.5 s apart
        m.appendChild (createSection ("Odd", 1, 7, 8, 140.0), nullptr);          // 1 bar of 7/8 at 140: 7 eighths
        const auto all = metronomeClicks (m, 0.0, 0);
        CHECK (all.size() == 15 && near (all[1].seconds, 0.5) && all[0].accent && ! all[1].accent && all[4].accent
               && all[8].accent && near (all[8].seconds, 4.0) && near (all[9].seconds - all[8].seconds, 0.5 * 60.0 / 140.0));
        const auto counted = metronomeClicks (m, 0.0, 1);                         // one bar of count-in: 4 clicks, 2 s
        CHECK (near (countInSeconds (m, 0.0, 1), 2.0) && counted.size() == 19 && counted[0].accent && near (counted[4].seconds, 2.0)
               && counted[4].accent);
        const auto fromBar2 = metronomeClicks (m, 4.0, 0);
        CHECK (fromBar2.size() == 11 && near (fromBar2[0].seconds, 0.0) && fromBar2[0].accent);
        CHECK (near (countInSeconds (m, 8.0, 2), 2.0 * 3.5 * 60.0 / 140.0));   // counted in the 7/8 bridge's own bar
        // Snapping to the grid, from the section's start.
        CHECK (near (snapBeat (m, 1.1, 3), 1.0) && near (snapBeat (m, 1.1, 5), 1.0) && near (snapBeat (m, 1.2, 5), 1.25)
               && near (snapBeat (m, 9.3, 0), 9.5) && near (snapBeat (m, 2.9, 1), 4.0) && near (snapBeat (m, 0.3, 8), 1.0 / 3.0));
    }

    // Full freedom within what a MIDI file stores: 37/8, 255/64, 10-960 BPM; a note value of 6 isn't one.
    {
        auto odd = createSong ("Odd");
        odd.removeAllChildren (nullptr);
        odd.appendChild (createSection ("A", 2, 37, 8, 960.0), nullptr);
        odd.appendChild (createSection ("B", 1, 255, 64, 10.0), nullptr);
        CHECK ((int) odd.getChild (0)[IDs::timeNum] == 37 && near (barBeats (odd.getChild (0)), 18.5)
               && (int) odd.getChild (1)[IDs::timeNum] == 255 && (int) odd.getChild (1)[IDs::timeDen] == 64
               && near ((double) odd.getChild (0)[IDs::bpm], 960.0) && near ((double) odd.getChild (1)[IDs::bpm], 10.0));
        const auto file = songMidi (odd, {});
        int n = 0, d = 0;
        for (int i = 0; i < file.getTrack (0)->getNumEvents(); ++i)
            if (file.getTrack (0)->getEventPointer (i)->message.isTimeSignatureMetaEvent() && n == 0)
                file.getTrack (0)->getEventPointer (i)->message.getTimeSignatureInfo (n, d);
        CHECK (n == 37 && d == 8);
        CHECK (isNoteValue (64) && isNoteValue (1) && ! isNoteValue (6) && ! isNoteValue (128));
    }

    // Backing-track creator: count-in section, click track, tempo fit, taps, mute / solo, loop.
    {
        auto b = createSong ("Backing");
        b.getChild (0).setProperty (IDs::bars, 4, nullptr);                         // 4 bars of 4/4 at 120 = 8 s
        auto t1 = addTrack (b, "QC", juce::Colours::blue), t2 = addTrack (b, "Whammy", juce::Colours::red);
        cues::Cue pc;
        pc.add (0.0, juce::MidiMessage::programChange (1, 3));
        auto first = placeCue (b, t1, 4.0, pc, juce::Colours::blue);              // bar 2
        const auto counted = withCountIn (b, 1);
        CHECK (counted.getChild (0)[IDs::name].toString() == "Count-in" && near (songLengthBeats (counted), 20.0)
               && near (cueBeat (counted, counted.getChildWithProperty (IDs::uid, first[IDs::uid])), 8.0));   // moved by the bar
        CHECK (songMidi (counted, includedTracks (counted)).getNumTracks() == 3);

        const ClickSound beep;
        const auto click = renderClickTrack (b, 1, 1000.0, beep, 1.0);             // 2 s count-in + 8 s + 1 s tail at 1 kHz
        CHECK (click.getNumSamples() == 11000 && click.getNumChannels() == 1);
        const auto peakAt = [&] (int from) { float m = 0.0f; for (int i = from; i < from + 40; ++i) m = juce::jmax (m, std::abs (click.getSample (0, i))); return m; };
        CHECK (peakAt (0) > 0.1f && peakAt (500) > 0.1f && peakAt (250) < 1.0e-6f && peakAt (10500) < 1.0e-6f);
        CHECK (clickSample (beep, true, 0.001) != 0.0f && clickSample (beep, true, 0.2) == 0.0f && clickSoundNames().size() == 5);

        CHECK (barNumberAt (b, 4.0) == 2 && barNumberAt (b, 3.9) == 1 && near (barStartBeat (b, 3), 8.0));
        b.setProperty (IDs::audioOffset, 1.0, nullptr);                            // bar 1 is 1 s into the file
        CHECK (fitTempoToAudio (b, 3, 1.0 + 3.0));                                 // bar 3 at 4 s: 8 beats in 3 s = 160 BPM
        CHECK (std::abs ((double) b.getChild (0)[IDs::bpm] - 160.0) < 0.01 && ! fitTempoToAudio (b, 1, 2.0));
        CHECK (std::abs (tempoFromTaps ({ 0.0, 0.5, 1.0, 1.5, 2.6 }) - 120.0) < 0.01 && tempoFromTaps ({ 1.0 }) == 0.0);

        CHECK (playedTracks (b).size() == 2);
        t1.setProperty (IDs::muted, true, nullptr);
        CHECK (playedTracks (b) == juce::StringArray { t2[IDs::uid].toString() });
        t1.setProperty (IDs::soloed, true, nullptr);                               // solo wins over mute here: only the soloed
        CHECK (playedTracks (b) == juce::StringArray { t1[IDs::uid].toString() });
        double ls = 0.0, le = 0.0;
        CHECK (! loopRange (b, ls, le));
        b.setProperty (IDs::loopSection, b.getChild (0)[IDs::uid], nullptr);
        CHECK (loopRange (b, ls, le) && near (ls, 0.0) && near (le, 16.0));
    }

    // A section's click can count eighths, sixteenths, triplets, bars only or nothing; custom samples play.
    {
        auto d = createSong ("Div");
        d.getChild (0).setProperty (IDs::bars, 1, nullptr);                         // one 4/4 bar at 120
        d.getChild (0).setProperty (IDs::clickDiv, 2, nullptr);                     // eighths: 8 clicks, the off-beats softer
        auto eighths = metronomeClicks (d, 0.0, 0);
        CHECK (eighths.size() == 8 && eighths[0].accent && ! eighths[0].sub && eighths[1].sub && ! eighths[2].sub && near (eighths[1].seconds, 0.25));
        d.getChild (0).setProperty (IDs::clickDiv, 4, nullptr);                     // eighth triplets: 12
        CHECK (metronomeClicks (d, 0.0, 0).size() == 12);
        d.getChild (0).setProperty (IDs::clickDiv, 6, nullptr);                     // bars only
        CHECK (metronomeClicks (d, 0.0, 0).size() == 1);
        d.getChild (0).setProperty (IDs::clickDiv, 7, nullptr);                     // off; the count-in still counts
        CHECK (metronomeClicks (d, 0.0, 0).empty() && metronomeClicks (d, 0.0, 1).size() == 4);
        CHECK (clickDivNames().size() == 8);

        ClickSound custom;
        custom.sound = 4;
        custom.sampleRate = 1000.0;
        custom.beatSample = std::make_shared<juce::AudioBuffer<float>> (1, 300);    // 0.3 s
        custom.beatSample->clear();
        custom.beatSample->setSample (0, 10, 1.0f);
        CHECK (near (clickLength (custom), 0.3) && clickSample (custom, false, 0.010) != 0.0f && clickSample (custom, true, 0.010) != 0.0f
               && clickSample (custom, false, 0.4) == 0.0f);
    }
}
