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
        // The page shows at most three Models groups (ModellerPage's modelSections).
        CHECK (bp->models.size() <= 3);
        // The padding CC is one the pedal ignores: no tile uses it.
        if (bp->padCc >= 0)
        {
            for (const auto& act : allActions)
                for (const auto& step : custom::parse (act.messages, 0).steps)
                    CHECK (! (step.kind == custom::Step::Kind::controller && step.number == bp->padCc));
            for (const auto& c : bp->switches)
                CHECK (c.cc != bp->padCc);
            for (const auto& c : bp->pedals)
                CHECK (c.cc != bp->padCc);
        }
        for (const auto& c : bp->pedals)
            CHECK (c.max >= 1 && c.max <= 127);
        // One Expression entry per CC, and no tile name twice in a list.
        for (size_t i = 0; i < bp->pedals.size(); ++i)
            for (size_t j = i + 1; j < bp->pedals.size(); ++j)
                CHECK (bp->pedals[i].cc != bp->pedals[j].cc && bp->pedals[i].name != bp->pedals[j].name);
        for (size_t i = 0; i < bp->switches.size(); ++i)
            for (size_t j = i + 1; j < bp->switches.size(); ++j)
                CHECK (bp->switches[i].name != bp->switches[j].name);
        for (size_t i = 0; i < allActions.size(); ++i)
            for (size_t j = i + 1; j < allActions.size(); ++j)
                CHECK (allActions[i].name != allActions[j].name);   // clip names stay unique
        // Presses: down, then up 1/16 later; never repeated by the padding.
        for (int s = 0; s < (int) bp->switches.size(); ++s)
            if (bp->switches[(size_t) s].press)
            {
                const auto cue = switchCue (*bp, 1, s, true, {});
                CHECK (cue.events.size() == 2 && cue.toggles && cue.events[1].first == 0.25
                       && isCC (cue.events[0].second, 1, bp->switches[(size_t) s].cc, bp->switches[(size_t) s].value)
                       && isCC (cue.events[1].second, 1, bp->switches[(size_t) s].cc, bp->switches[(size_t) s].offValue));
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
    CHECK (lvx.tunerOn == "CC 117=127" && lvx.padCc == 106 && lvx.looper.size() == 7 && isCC (action (lvx, 1, lvx.looper[0]).events[0].second, 1, 100, 127));
    CHECK (isCC (lengthPadding (preset (lvx, 1, -1, 0, false, {})), 1, 106, 0));
    const auto& mx = *find ("meris.mercuryx");
    CHECK (mx.models[0].hint.startsWith ("CC#32") && isCC (action (mx, 1, mx.models[0].actions[1]).events[0].second, 1, 32, 23));   // Cathedra 16-31
    for (const auto& u : mx.utilities)
        CHECK (! u.name.startsWith ("Tap"));   // MercuryX has no tap CC
    // Hold modifier presses (MercuryX / Enzo X document the release; LVX / Ottobit X don't, so they have none).
    const auto holdIndex = [] (const Profile& pr)
    {
        for (int i = 0; i < (int) pr.switches.size(); ++i)
            if (pr.switches[(size_t) i].cc == 118)
                return pr.switches[(size_t) i].press ? i : -2;
        return -1;
    };
    CHECK (holdIndex (mx) >= 0 && holdIndex (*find ("meris.enzo-x")) >= 0 && holdIndex (lvx) == -1 && holdIndex (*find ("meris.ottobit-x")) == -1);
    {
        const auto hold = switchCue (mx, 1, holdIndex (mx), true, {});
        CHECK (hold.events.size() == 2 && isCC (hold.events[0].second, 1, 118, 127) && isCC (hold.events[1].second, 1, 118, 0) && hold.events[1].first == 0.25);
    }
    // Every X pedal now shows its block types and locations within the three Models groups.
    CHECK (lvx.models.size() == 3 && lvx.models[1].actions.size() == 31 && lvx.models[2].actions.size() == 25);
    CHECK (mx.models[2].actions.size() == 25 && find ("meris.ottobit-x")->models[2].actions.size() == 24);
    const auto pedalCc = [] (const char* id, const juce::String& name, int cc, int max)
    {
        for (const auto& c : find (id)->pedals)
            if (c.name == name)
                return c.cc == cc && c.max == max;
        return false;
    };
    CHECK (pedalCc ("meris.lvx", "Delay param 40", 61, 127) && pedalCc ("meris.lvx", "Delay damping", 120, 127)
           && pedalCc ("meris.mercuryx", "Gate decay", 45, 127) && pedalCc ("meris.enzo-x", "Amp release", 59, 127)
           && pedalCc ("meris.ottobit-x", "Glitch step 8", 77, 127) && pedalCc ("meris.polymoon", "Tempo", 15, 120)
           && pedalCc ("meris.enzo", "Tempo", 15, 120) && pedalCc ("meris.hedra", "Tempo", 15, 127));
    CHECK (sends ("meris.lvx", "Looper: Post Mix", 94, 115) && sends ("meris.lvx", "Preamp: Feedback", 6, 79)
           && sends ("meris.mercuryx", "Pitch: Pre Tank", 71, 89) && sends ("meris.mercuryx", "79 Chorus", 86, 32)
           && sends ("meris.enzo-x", "Mod: Post Amb", 87, 111) && sends ("meris.enzo-x", "Env down", 52, 95)
           && sends ("meris.ottobit-x", "Dust heavy", 20, 106) && sends ("meris.ottobit-x", "Crush: Dry", 25, 47)
           && sends ("meris.ottobit-x", "Knob: glitch", 27, 95));
    CHECK (sends ("meris.polymoon", "Early: FM 24 Hz", 22, 52) && sends ("meris.polymoon", "Late: Oct dn / Oct up", 25, 124)
           && sends ("meris.enzo", "Unison", 16, 63) && sends ("meris.enzo", "+24", 16, 127)
           && sends ("meris.hedra", "V3 1", 27, 126) && sends ("meris.hedra", "V1 micro", 25, 7));
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
    CHECK (presetLabel (blooper, -1, 0) == "Loop 1" && preset (blooper, 2, -1, 0, false, {}).events[0].second.getProgramChangeNumber() == 0
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
    // Only the engine numbers the MIDI implementation and the ADT guide agree on.
    CHECK (nem.models[0].actions.size() == 19 && isCC (action (nem, 1, nem.models[0].actions[18]).events[0].second, 1, 1, 22));
    for (const auto& a : nem.models[0].actions)
        for (int v : { 5, 9, 10, 12, 23 })
            CHECK (a.messages != "CC 1=" + juce::String (v));
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
    // Chase Bliss dips, hidden / ALT menus, MOOD clock divisions.
    const auto switchNamed = [] (const char* id, const juce::String& name) -> const Control*
    {
        for (const auto& c : find (id)->switches)
            if (c.name == name)
                return &c;
        return nullptr;
    };
    const auto switchIs = [&switchNamed] (const char* id, const juce::String& name, int cc, int on, int off)
    {
        const auto* c = switchNamed (id, name);
        return c != nullptr && c->cc == cc && c->value == on && c->offValue == off && ! c->press;
    };
    CHECK (switchIs ("chasebliss.mood-mkii", "Dip Time", 61, 127, 0) && switchIs ("chasebliss.mood-mkii", "Dip Smooth", 78, 127, 0)
           && switchIs ("chasebliss.mood-mkii", "Hidden menu", 104, 127, 0));
    CHECK (switchIs ("chasebliss.gen-loss-mkii", "Dip Random", 67, 127, 0) && switchIs ("chasebliss.gen-loss-mkii", "Dip Polarity R", 71, 127, 0)
           && switchIs ("chasebliss.gen-loss-mkii", "ALT menu", 104, 0, 127) && switchIs ("chasebliss.gen-loss-mkii", "Dip Hum byp", 78, 127, 0));
    CHECK (switchIs ("chasebliss.lossy", "Dip All wet", 78, 127, 0) && switchIs ("chasebliss.lossy", "ALT menu", 104, 127, 0)
           && switchIs ("chasebliss.lossy", "Gate hold", 106, 127, 0));
    CHECK (switchIs ("chasebliss.brothers-am", "Dip Master", 77, 127, 0) && switchNamed ("chasebliss.brothers-am", "Dip ") == nullptr);
    for (const auto& c : find ("chasebliss.brothers-am")->switches)
        CHECK (c.cc != 78);
    CHECK (switchIs ("chasebliss.onward", "Dip Manual", 78, 127, 0) && switchIs ("chasebliss.clean", "Dip Dusty", 78, 127, 0)
           && switchIs ("chasebliss.clean", "Dip Sweep T", 67, 127, 0));
    for (const char* id : { "chasebliss.blooper", "chasebliss.dark-world", "chasebliss.thermae", "chasebliss.habit", "chasebliss.cxm-1978",
                            "chasebliss.preamp-mkii" })
        for (const auto& c : find (id)->switches)
            CHECK (c.cc < 61 || c.cc > 78);   // no dip CCs in these charts
    CHECK (find ("chasebliss.mood-mkii")->models.size() == 2 && sends ("chasebliss.mood-mkii", "Drop Dotted 1/8", 53, 4)
           && sends ("chasebliss.mood-mkii", "Exit synth", 59, 127) && sends ("chasebliss.mood-mkii", "True bypass", 55, 127)
           && sends ("chasebliss.mood-mkii", "Synth: ADSR", 58, 2) && sends ("chasebliss.mood-mkii", "Clock ignore", 51, 0));
    CHECK (find ("chasebliss.mood-mkii")->models[1].actions[7].messages == "CC 54=7");
    CHECK (switchIs ("chasebliss.blooper", "Hold B", 8, 127, 0) && switchIs ("chasebliss.blooper", "One-shot dub", 9, 127, 0)
           && sends ("chasebliss.cxm-1978", "EOM unlock", 101, 127));
    for (const auto& a : find ("chasebliss.blooper")->looper)
        CHECK (a.messages != "CC 11=7" && ! a.messages.startsWith ("CC 7="));   // erase

    // Walrus: ranged program / division controls, numbered cabs, reverse mode 0 / 1.
    CHECK (pedalCc ("walrus.mako-d1", "Program", 24, 5) && pedalCc ("walrus.mako-d1", "Feedback mode", 32, 2)
           && pedalCc ("walrus.mako-d1", "Grain pitch", 34, 4) && switchIs ("walrus.mako-d1", "Reverse mode", 35, 1, 0));
    CHECK (pedalCc ("walrus.mako-r1", "Program", 28, 5) && pedalCc ("walrus.mako-m1", "Division", 16, 4)
           && switchIs ("walrus.mako-m1", "Skip", 87, 127, 0) && sends ("walrus.mako-m1", "Type 3", 17, 2));
    CHECK (sends ("walrus.mako-d1", "Program 5", 24, 5) && sends ("walrus.mako-d1", "Div R 5", 31, 5) && sends ("walrus.mako-d1", "Shape 0", 33, 0)
           && sends ("walrus.mako-r1", "Program 5", 28, 5) && sends ("walrus.mako-m1", "Program 2", 18, 2) && sends ("walrus.mako-acs1", "Amp R 5", 29, 5));
    CHECK (sends ("walrus.mako-acs1", "L 12 1960 421+M160", 26, 11) && sends ("walrus.mako-acs1", "R 1 Deluxe 57", 27, 0)
           && find ("walrus.mako-acs1")->models[0].hint == "CC#26, CC#27"
           && sends ("walrus.mako-acs1", "Spring", 102, 2) && pedalCc ("walrus.mako-acs1", "Amp right", 29, 5));

    // Source Audio: 0 / 1 switches, knob settings, Ventris engines 12-13 and low cut, Collider divisions.
    CHECK (switchIs ("sourceaudio.nemesis", "LFO lock", 32, 1, 0) && switchIs ("sourceaudio.nemesis", "Merge stereo", 37, 1, 0)
           && pedalCc ("sourceaudio.nemesis", "Pitch shift", 24, 125) && pedalCc ("sourceaudio.nemesis", "Input low pass", 30, 127)
           && sends ("sourceaudio.nemesis", "Fb max 2x", 31, 3) && sends ("sourceaudio.nemesis", "Rate: tape speed", 41, 1));
    CHECK (find ("sourceaudio.ventris")->models[0].actions.size() == 14 && sends ("sourceaudio.ventris", "A Metal Box", 1, 13) && sends ("sourceaudio.ventris", "B Room", 25, 0)
           && sends ("sourceaudio.ventris", "B cut 250 Hz", 38, 6) && pedalCc ("sourceaudio.ventris", "B special", 44, 127));
    CHECK (sends ("sourceaudio.collider", "Reverb 1/16", 23, 5) && sends ("sourceaudio.collider", "Delay golden", 9, 2)
           && pedalCc ("sourceaudio.collider", "Reverb control 2", 21, 127));

    // EHX: POG3 function Program Changes one lower than listed, pans; Abyss blocks, EQ 0-24.
    {
        const auto& pg = *find ("ehx.pog3");
        bool on = false, up = false;
        for (const auto& u : pg.utilities)
        {
            on = on || (u.name == "Effect on" && u.messages == "PC 125");
            up = up || (u.name == "Preset up" && u.messages == "PC 123");
        }
        CHECK (on && up && pedalCc ("ehx.pog3", "+2 oct pan", 107, 127));
        for (const auto& c : pg.pedals)
            CHECK (c.cc != 119 && c.cc != 80);   // CC#119 duplicates the detune slider; CC#80 saves
    }
    CHECK (pedalCc ("ehx.oceans-abyss", "Rev B inf level", 55, 127) && pedalCc ("ehx.oceans-abyss", "EQ 6.4k", 117, 24)
           && sends ("ehx.oceans-abyss", "R only", 78, 5) && sends ("ehx.oceans-abyss", "LFO fall", 105, 4)
           && sends ("ehx.oceans-abyss", "B chord min7", 57, 4) && sends ("ehx.oceans-abyss", "Rev B mode 2", 44, 127)
           && switchIs ("ehx.oceans-abyss", "Channel blocks", 35, 23, 22));
    CHECK (find ("sourceaudio.ventris")->models[1].hint == "CC#25" && sends ("sourceaudio.ventris", "A+B cascade", 50, 3)
           && find ("sourceaudio.ventris")->pedals.front().cc == 100);
}
