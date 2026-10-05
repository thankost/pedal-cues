// Boutique pedal pages: checks against the MIDI charts (included in CueModelTests.cpp, inside the modellers block).
{
    const char* boutiqueIds[] = { "meris.mercury7", "meris.ottobit-jr", "meris.polymoon", "meris.enzo", "meris.hedra", "meris.lvx", "meris.mercuryx",
                                  "meris.enzo-x", "meris.ottobit-x", "chasebliss.mood-mkii", "chasebliss.blooper", "chasebliss.dark-world",
                                  "chasebliss.thermae", "chasebliss.habit", "chasebliss.cxm-1978", "chasebliss.preamp-mkii",
                                  "chasebliss.gen-loss-mkii", "chasebliss.lossy", "chasebliss.brothers-am", "chasebliss.onward", "chasebliss.clean",
                                  "walrus.mako-d1", "walrus.mako-r1", "walrus.mako-m1", "walrus.mako-acs1", "sourceaudio.nemesis",
                                  "sourceaudio.ventris", "sourceaudio.collider", "ehx.pog3", "ehx.oceans-abyss" };
    for (const auto* id : boutiqueIds)
    {
        const auto* bp = find (id);
        CHECK (bp != nullptr);
        if (bp == nullptr)
            continue;
        CHECK (bp->pedal && bp->beta && bp->scheme == Scheme::numbered && bp->sceneCount == 0 && bp->testMessage.isNotEmpty());
        CHECK (bp->padCc != 0 && bp->padCc != 32 && bp->padCc != 56);   // -1 = CC#0, only where CC#0 isn't a control
        // Never a factory reset, a save or a CC#0 / CC#32 padding on a pedal that uses them; every message parses.
        std::vector<Action> allActions;
        for (const auto& list : { bp->utilities, bp->looper })
            allActions.insert (allActions.end(), list.begin(), list.end());
        for (const auto& g : bp->models)
            allActions.insert (allActions.end(), g.actions.begin(), g.actions.end());
        for (const auto& act : allActions)
        {
            const auto parsed = custom::parse (act.messages, 0);
            CHECK (parsed.ok());
            if (juce::String (id).startsWith ("chasebliss."))
                CHECK (! act.messages.contains ("CC 56=") && ! act.messages.contains ("CC 111=") && ! act.messages.contains ("CC 27="));   // reset / save
            if (juce::String (id) == "ehx.pog3")
                CHECK (! act.messages.contains ("CC 80="));                                                                          // save
        }
        for (const auto& sw : bp->switches)
            CHECK (! (juce::String (id).startsWith ("chasebliss.") && sw.cc == 56));
        for (const auto& c : bp->pedals)
            CHECK (! (juce::String (id).startsWith ("chasebliss.") && (c.cc == 56 || c.cc == 0 || c.cc == 111)));
        if (bp->padCc >= 0)
        {
            CHECK (isCC (lengthPadding (preset (*bp, 4, -1, 0, false, {})), 4, bp->padCc, 0));
            if (! bp->utilities.empty())
                CHECK (isCC (lengthPadding (action (*bp, 4, bp->utilities.front())), 4, bp->padCc, 0));
        }
    }

    // Meris legacy: PC 0 = bypass, presets 1-16 = PC 1-16; CC#14 bypass, CC#4 expression.
    const auto& m7 = *find ("meris.mercury7");
    CHECK (presetLabel (m7, -1, 0) == "Preset 1" && presetLabel (m7, -1, 15) == "Preset 16" && presetsPerSetlist (m7, -1) == 16);
    CHECK (preset (m7, 1, -1, 0, false, {}).events.size() == 1 && preset (m7, 1, -1, 0, false, {}).events[0].second.getProgramChangeNumber() == 1);
    CHECK (preset (m7, 1, -1, 15, false, {}).events[0].second.getProgramChangeNumber() == 16 && m7.padCc == -1);
    CHECK (isCC (lengthPadding (preset (m7, 2, -1, 3, false, {})), 2, 0, 0));
    CHECK (custom::parse (m7.utilities.front().messages, 0).steps[0].number == 0 && m7.utilities.front().messages == "PC 0");
    CHECK (isCC (switchCue (m7, 1, 0, false, {}).events[0].second, 1, 14, 0) && isCC (switchCue (m7, 1, 0, true, {}).events[0].second, 1, 14, 127));
    CHECK (m7.pedals.front().cc == 4 && m7.midiIn == "TRS MIDI In (EXP jack, tip)" && ! m7.usbMidi);
    const auto& hedra = *find ("meris.hedra");
    CHECK (isCC (action (hedra, 1, hedra.models[0].actions[0]).events[0].second, 1, 16, 1));   // key C = 0-2
    CHECK (isCC (action (*find ("meris.ottobit-jr"), 1, find ("meris.ottobit-jr")->utilities[1]).events[0].second, 1, 28, 127));   // tap

    // Meris X: PC 1-99 = presets, PC 100-102 = favourites, CC#117 tuner, padding off CC#32.
    const auto& lvx = *find ("meris.lvx");
    CHECK (presetLabel (lvx, -1, 0) == "Preset 1" && presetLabel (lvx, -1, 98) == "Preset 99" && presetLabel (lvx, -1, 99) == "Favorite 1"
           && presetLabel (lvx, -1, 101) == "Favorite 3");
    CHECK (preset (lvx, 1, -1, 99, false, {}).events[0].second.getProgramChangeNumber() == 100
           && preset (lvx, 1, -1, 101, false, {}).events[0].second.getProgramChangeNumber() == 102);
    CHECK (lvx.tunerOn == "CC 117=127" && lvx.padCc == 106 && lvx.looper.size() == 6 && isCC (action (lvx, 1, lvx.looper[0]).events[0].second, 1, 100, 127));
    CHECK (isCC (lengthPadding (preset (lvx, 1, -1, 0, false, {})), 1, 106, 0));
    const auto& mx = *find ("meris.mercuryx");
    CHECK (mx.models[0].hint == "CC#32" && isCC (action (mx, 1, mx.models[0].actions[1]).events[0].second, 1, 32, 23));   // Cathedra 16-31
    for (const auto& u : mx.utilities)
        CHECK (! u.name.startsWith ("Tap"));   // MercuryX has no tap CC
    CHECK (find ("meris.enzo-x")->padCc == 70 && find ("meris.ottobit-x")->padCc == 85 && mx.padCc == 50);

    // Chase Bliss: PC 0 = Live, slot n = PC n; CC#40 padding (MOOD MKII CC#0 = pitch bend).
    const auto& mood = *find ("chasebliss.mood-mkii");
    CHECK (presetLabel (mood, -1, 0) == "Preset 1" && presetLabel (mood, -1, 121) == "Preset 122" && presetsPerSetlist (mood, -1) == 122);
    CHECK (preset (mood, 2, -1, 0, false, {}).events[0].second.getProgramChangeNumber() == 1
           && preset (mood, 2, -1, 121, false, {}).events[0].second.getProgramChangeNumber() == 122);
    CHECK (mood.utilities.front().messages == "PC 0" && mood.padCc == 40);
    CHECK (isCC (lengthPadding (preset (mood, 2, -1, 0, false, {})), 2, 40, 0));
    CHECK (isCC (switchCue (mood, 2, 0, true, {}).events[0].second, 2, 102, 127) && isCC (switchCue (mood, 2, 1, false, {}).events[0].second, 2, 103, 0));
    CHECK (mood.pedals.front().cc == 100 && ! mood.hasThru && mood.channelHint.contains ("Channel 2"));
    const auto& blooper = *find ("chasebliss.blooper");
    CHECK (presetLabel (blooper, -1, 0) == "Preset 1" && preset (blooper, 2, -1, 0, false, {}).events[0].second.getProgramChangeNumber() == 0
           && preset (blooper, 2, -1, 15, false, {}).events[0].second.getProgramChangeNumber() == 15 && presetsPerSetlist (blooper, -1) == 16);
    CHECK (isCC (action (blooper, 2, blooper.looper[0]).events[0].second, 2, 11, 1) && blooper.looper.size() == 6);   // record, no erase
    CHECK (isCC (action (blooper, 2, blooper.utilities[0]).events[0].second, 2, 30, 127) && action (blooper, 2, blooper.utilities[0]).toggles);
    const auto& cxm = *find ("chasebliss.cxm-1978");
    CHECK (presetLabel (cxm, -1, 4) == "1:4" && presetLabel (cxm, -1, 17) == "2:7" && presetLabel (cxm, -1, 20) == "3:0"
           && preset (cxm, 1, -1, 17, false, {}).events[0].second.getProgramChangeNumber() == 17 && cxm.hasThru && cxm.midiIn == "5-pin MIDI In");
    CHECK (isCC (switchCue (cxm, 1, 1, true, {}).events[0].second, 1, 31, 0));   // sustain on = 0
    const auto& gen = *find ("chasebliss.gen-loss-mkii");
    CHECK (gen.padCc == 40 && isCC (action (gen, 1, gen.models[0].actions[1]).events[0].second, 1, 16, 15));   // CPR-3300 Gen 1
    CHECK (find ("chasebliss.thermae")->channelHint.contains ("TAP + BYPASS"));

    // Walrus MKII: PC 0-8 = A-C red / green / blue, bypass CCs differ per pedal.
    const auto& d1 = *find ("walrus.mako-d1");
    CHECK (presetLabel (d1, -1, 0) == "A Red" && presetLabel (d1, -1, 4) == "B Green" && presetLabel (d1, -1, 8) == "C Blue"
           && presetLabel (d1, -1, 9) == "Preset 9" && presetsPerSetlist (d1, -1) == 128);
    CHECK (preset (d1, 1, -1, 0, false, {}).events[0].second.getProgramChangeNumber() == 0 && d1.padCc == 40);
    CHECK (isCC (switchCue (d1, 1, 0, true, {}).events[0].second, 1, 29, 127) && isCC (switchCue (*find ("walrus.mako-r1"), 1, 0, false, {}).events[0].second, 1, 30, 0)
           && isCC (switchCue (*find ("walrus.mako-m1"), 1, 0, true, {}).events[0].second, 1, 31, 127));
    CHECK (d1.notes.contains ("MKI uses different numbers"));

    // Source Audio: preset 1 = PC 0, bypass CC#101, toggles on any value, CC#60 padding.
    const auto& nem = *find ("sourceaudio.nemesis");
    CHECK (presetLabel (nem, -1, 0) == "Preset 1" && preset (nem, 1, -1, 0, false, {}).events[0].second.getProgramChangeNumber() == 0
           && presetLabel (nem, -1, 127) == "Preset 128" && nem.padCc == 60);
    CHECK (nem.models[0].actions.size() == 24 && isCC (action (nem, 1, nem.models[0].actions[23]).events[0].second, 1, 1, 23));
    CHECK (isCC (switchCue (nem, 1, 0, false, {}).events[0].second, 1, 101, 0) && isCC (action (nem, 1, nem.utilities[0]).events[0].second, 1, 102, 127));
    const auto& col = *find ("sourceaudio.collider");
    CHECK (isCC (switchCue (col, 1, 1, true, {}).events[0].second, 1, 103, 127) && col.models[1].hint == "CC#15");

    // EHX: 1-based PC lists (inferred), POG3 bypass CC#79 (never the CC#80 save), Oceans Abyss blocks on CC#35.
    const auto& pog = *find ("ehx.pog3");
    CHECK (presetLabel (pog, -1, 0) == "Preset 1" && preset (pog, 1, -1, 0, false, {}).events[0].second.getProgramChangeNumber() == 0
           && presetsPerSetlist (pog, -1) == 100);
    CHECK (isCC (switchCue (pog, 1, 0, false, {}).events[0].second, 1, 79, 0) && pog.pedals.front().cc == 4);
    for (const auto& sw : pog.switches)
        CHECK (sw.cc != 80);
    const auto& abyss = *find ("ehx.oceans-abyss");
    CHECK (presetLabel (abyss, -1, 127) == "Preset 128" && preset (abyss, 1, -1, 127, false, {}).events[0].second.getProgramChangeNumber() == 127);
    CHECK (isCC (switchCue (abyss, 1, 0, false, {}).events[0].second, 1, 4, 0) && abyss.pedals.front().cc == 11);
    CHECK (isCC (switchCue (abyss, 1, 5, true, {}).events[0].second, 1, 35, 1) && isCC (switchCue (abyss, 1, 12, false, {}).events[0].second, 1, 35, 14));

    // A few CC numbers per pedal, looked up by tile name.
    const auto sends = [] (const char* id, const juce::String& name, int cc, int value)
    {
        const auto& pr = *find (id);
        std::vector<Action> list (pr.utilities);
        list.insert (list.end(), pr.looper.begin(), pr.looper.end());
        for (const auto& g : pr.models)
            list.insert (list.end(), g.actions.begin(), g.actions.end());
        for (const auto& t : list)
            if (t.name == name)
                return isCC (action (pr, 1, t).events[0].second, 1, cc, value);
        return false;
    };
    const auto switchSends = [] (const char* id, int index, bool on, int cc, int value)
    {
        return isCC (switchCue (*find (id), 1, index, on, {}).events[0].second, 1, cc, value);
    };
    CHECK (switchSends ("meris.polymoon", 1, true, 9, 127) && sends ("meris.polymoon", "Phaser sync slow", 29, 95));
    CHECK (sends ("meris.enzo", "Arp", 29, 95) && sends ("meris.enzo", "Env follower", 9, 64) && sends ("meris.enzo", "Ladder LP", 23, 2));
    CHECK (switchSends ("meris.enzo-x", 1, true, 12, 127) && switchSends ("meris.enzo-x", 2, true, 37, 127) && sends ("meris.enzo-x", "Tap", 99, 127));
    CHECK (switchSends ("meris.ottobit-x", 2, true, 78, 127) && sends ("meris.ottobit-x", "Tape Stop", 62, 101) && sends ("meris.ottobit-x", "Tuner", 117, 127));
    CHECK (sends ("chasebliss.dark-world", "Dark only", 103, 85) && switchSends ("chasebliss.dark-world", 0, true, 102, 127));
    CHECK (switchSends ("chasebliss.thermae", 1, true, 24, 127) && sends ("chasebliss.thermae", "Tap", 93, 127));
    CHECK (switchSends ("chasebliss.habit", 1, true, 24, 127) && switchSends ("chasebliss.habit", 2, false, 25, 0));
    CHECK (sends ("chasebliss.preamp-mkii", "Fuzz gated", 26, 3) && presetLabel (*find ("chasebliss.preamp-mkii"), -1, 29) == "3:9");
    CHECK (switchSends ("chasebliss.lossy", 4, true, 57, 127) && sends ("chasebliss.lossy", "Weighting bright", 33, 3));
    CHECK (switchSends ("chasebliss.brothers-am", 1, true, 103, 127) && sends ("chasebliss.brothers-am", "Half sun", 22, 3));
    CHECK (sends ("chasebliss.onward", "Retrigger freeze", 109, 127) && sends ("chasebliss.onward", "Sustain: freeze", 32, 3)
           && switchSends ("chasebliss.onward", 5, true, 58, 127));
    CHECK (sends ("chasebliss.clean", "ENV shifty", 32, 3) && switchSends ("chasebliss.clean", 1, true, 103, 127));
    CHECK (switchSends ("walrus.mako-acs1", 1, true, 85, 127) && switchSends ("walrus.mako-acs1", 2, true, 86, 127));
    for (const auto& sw : find ("walrus.mako-acs1")->switches)
        CHECK (sw.cc != 30);   // the bypass CC is printed "0-5": left out
    CHECK (find ("sourceaudio.ventris")->models[1].hint == "CC#25" && sends ("sourceaudio.ventris", "A+B cascade", 50, 3)
           && find ("sourceaudio.ventris")->pedals.front().cc == 100);
}
