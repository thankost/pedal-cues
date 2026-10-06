// Amp modeller pages (Modellers.cpp): checks against the manuals' MIDI charts (included in CueModelTests.cpp, inside the modellers block).
{
    const auto ccOf = [] (const Action& a) { const auto st = custom::parse (a.messages, 0).steps; return st.size() == 1 ? st[0].number : -1; };
    const auto valueOf = [] (const Action& a) { const auto st = custom::parse (a.messages, 0).steps; return st.size() == 1 ? st[0].value : -1; };
    const auto hasAction = [&] (const std::vector<Action>& list, int cc, int value)
    {
        for (const auto& a : list)
            if (ccOf (a) == cc && valueOf (a) == value)
                return true;
        return false;
    };
    const auto usesCc = [&] (const Profile& p, int cc)
    {
        std::vector<const std::vector<Action>*> lists { &p.utilities, &p.looper };
        for (const auto& g : p.models)
            lists.push_back (&g.actions);
        for (const auto* list : lists)
            for (const auto& a : *list)
                for (const auto& st : custom::parse (a.messages, 0).steps)
                    if (st.kind == custom::Step::Kind::controller && st.number == cc)
                        return true;
        return false;
    };

    // Line 6 Helix family: the earlier tiles keep their places (switch names and the chosen pedal are stored by index), then the
    // added globals: next / previous snapshot CC#69 = 8 / 9, MODE CC#71, Page CC#81 0 / 127, Parameter Knobs from CC#75.
    for (const auto* id : { "line6.helix-floor", "line6.helix-lt", "line6.helix-rack" })
    {
        const auto& h = *find (id);
        CHECK (h.switches.size() == 10 && h.switches[0].cc == 49 && h.switches[5].cc == 54 && ! h.switches[0].press);   // any value = one press
        CHECK (h.pedals[0].cc == 1 && h.pedals[1].cc == 2 && h.pedals.back().cc == 80 && h.pedals.back().name == "Knob 6");
        CHECK (hasAction (h.utilities, 69, 8) && hasAction (h.utilities, 69, 9) && hasAction (h.utilities, 71, 127)
               && hasAction (h.utilities, 81, 0) && hasAction (h.utilities, 81, 127) && ccOf (h.utilities[0]) == 68);
        CHECK (hasAction (h.utilities, 73, 127) == (juce::String (id) == "line6.helix-lt"));   // Play/Edit view: LT only
    }
    CHECK (find ("line6.helix-floor")->pedals.size() == 9 && find ("line6.helix-lt")->pedals.size() == 8);   // EXP 1-3 / 1-2 + knobs 1-6
    for (const auto* id : { "line6.hx-stomp", "line6.hx-stomp-xl", "line6.hx-effects" })
    {
        const auto& h = *find (id);
        CHECK (h.pedals.size() == 5 && h.pedals[2].cc == 75 && h.pedals[4].cc == 77);
        CHECK (hasAction (h.utilities, 69, 9) && hasAction (h.utilities, 70, 0) && hasAction (h.utilities, 81, 127));
        CHECK (hasAction (h.utilities, 73, 127) == (juce::String (id) != "line6.hx-effects"));
    }
    CHECK (find ("line6.hx-stomp")->sceneCount == 3 && find ("line6.hx-stomp")->notes.contains ("midi.guide"));   // kept at three
    const auto& podGo = *find ("line6.pod-go");
    CHECK (podGo.pedals.size() == 2 && podGo.utilities.size() == 4 && hasAction (podGo.utilities, 69, 8) && ! usesCc (podGo, 71));
    for (const auto& p : all())
        CHECK (! usesCc (p, 128));   // the manuals' joke "CC#128" lines are never tiles

    // Helix Stadium: tuner stays first; CC#9 buttons without Save screen (3) or Undo / Redo (10 / 11); no Clear Loop (CC#52).
    const auto& st = *find ("line6.helix-stadium");
    CHECK (ccOf (st.utilities[0]) == 9 && valueOf (st.utilities[0]) == 34 && hasAction (st.utilities, 9, 14) && hasAction (st.utilities, 69, 8));
    CHECK (! hasAction (st.utilities, 9, 3) && ! hasAction (st.utilities, 9, 10) && ! hasAction (st.utilities, 9, 11) && ! usesCc (st, 52));
    CHECK (hasAction (st.looper, 47, 127) && hasAction (st.looper, 49, 127) && hasAction (st.looper, 48, 127) && ! hasAction (st.looper, 48, 0));
    CHECK (st.pedals.size() == 12 && st.pedals[2].cc == 7 && st.pedals[3].cc == 8 && st.pedals[4].cc == 38 && st.pedals[11].cc == 45);

    // HX One: FLUX curves CC#47 / 49 = 0-10 on the Models view (Slow 5 ... Linear ... Fast 5); the pedals list is unchanged.
    const auto& one = *find ("line6.hx-one");
    CHECK (one.models.size() == 2 && one.models[0].actions.size() == 11 && one.models[1].actions.size() == 11);
    CHECK (one.models[0].actions[0].name == "On: Slow 5" && one.models[0].actions[5].name == "On: Linear" && valueOf (one.models[0].actions[10]) == 10
           && ccOf (one.models[0].actions[10]) == 47 && ccOf (one.models[1].actions[0]) == 49 && one.models[1].actions[10].name == "Off: Fast 5");

    // Fractal: the first ten blocks keep their places, then more; X/Y on the Models view (64-127 = X, 0-63 = Y);
    // Volume Incr / Decr (CC#35 / 36) save the preset, so never a tile.
    for (const auto* id : { "fractal.axe-fx-2", "fractal.ax8", "fractal.fx8" })
    {
        const auto& f = *find (id);
        CHECK (f.switches.size() == 21 && f.switchesOnOff && ! usesCc (f, 35) && ! usesCc (f, 36));
        CHECK (hasAction (f.utilities, 122, 127) && hasAction (f.utilities, 122, 0) && hasAction (f.looper, 33, 127) && hasAction (f.looper, 33, 0));
        CHECK (f.models.size() == 2 && f.notes.contains ("Volume Incr"));
        for (const auto& g : f.models)
            for (size_t i = 0; i + 1 < g.actions.size(); i += 2)
                CHECK (ccOf (g.actions[i]) == ccOf (g.actions[i + 1]) && valueOf (g.actions[i]) == 127 && valueOf (g.actions[i + 1]) == 0
                       && g.actions[i].name.endsWith (" X") && g.actions[i + 1].name.endsWith (" Y"));
    }
    const auto& axe = *find ("fractal.axe-fx-2");
    CHECK (axe.switches[0].cc == 37 && axe.switches[9].cc == 97 && axe.switches[10].cc == 40 && axe.switches[20].cc == 59);
    CHECK (axe.pedals.size() == 15 && axe.pedals[0].cc == 16 && axe.pedals[11].cc == 27 && axe.pedals[12].cc == 10);
    CHECK (hasAction (axe.utilities, 13, 127) && hasAction (axe.utilities, 13, 0) && ccOf (axe.models[0].actions[0]) == 100
           && ccOf (axe.models[1].actions.back()) == 119);
    const auto& ax8 = *find ("fractal.ax8");
    CHECK (ax8.switches[0].cc == 49 && ax8.switches[10].cc == 75 && ax8.pedals[0].cc == 20 && ax8.pedals[7].cc == 27 && ax8.pedals.size() == 11);
    const auto& fx8 = *find ("fractal.fx8");
    CHECK (fx8.switches[9].cc == 75 && fx8.switches.back().cc == 126 && ! usesCc (fx8, 125)                 // Relay 1 / Rotary X/Y clash on 125
           && fx8.pedals.size() == 16 && fx8.pedals[14].cc == 9);

    // HeadRush Core / Prime / Flex Prime: footswitch press tiles after the 14 blocks (127, then 0 1/16 later); the older units
    // don't document the footswitch values, so they keep blocks only.
    for (const auto& [id, footswitches] : { std::pair<const char*, int> { "headrush.core", 5 }, { "headrush.prime", 12 }, { "headrush.flex-prime", 3 } })
    {
        const auto& h = *find (id);
        CHECK ((int) h.switches.size() == 14 + footswitches && h.switches[13].cc == 88 && ! h.switches[13].press);
        const auto& fs = h.switches[14];
        CHECK (fs.press && fs.cc == 49 && fs.value == 127 && fs.offValue == 0 && h.switches.back().cc == 48 + footswitches);
        const auto c = switchCue (h, 1, 14, true, {});
        CHECK (c.events.size() == 2 && isCC (c.events[0].second, 1, 49, 127) && isCC (c.events[1].second, 1, 49, 0) && c.toggles);
        CHECK (h.utilities.size() <= 16 && ccOf (h.utilities[0]) == 92 && hasAction (h.utilities, 12, 127) && hasAction (h.utilities, 19, 127));
        CHECK (h.looperTitle == "Looper & drums" && hasAction (h.looper, 42, 127) && hasAction (h.looper, 93, 127));
        for (int cc = 102; cc <= 117; ++cc)
            CHECK (! usesCc (h, cc));   // Practice Tool: left out
    }
    CHECK (hasAction (find ("headrush.prime")->looper, 123, 127) && ! hasAction (find ("headrush.core")->looper, 123, 127));
    CHECK (hasAction (find ("headrush.core")->utilities, 98, 127) && hasAction (find ("headrush.prime")->utilities, 15, 127)
           && ! usesCc (*find ("headrush.flex-prime"), 89));                                        // Flex Prime: no Mic dry
    CHECK (find ("headrush.prime")->pedals.size() == 5 && find ("headrush.prime")->pedals[4].cc == 63);
    for (const auto* id : { "headrush.pedalboard", "headrush.gigboard", "headrush.mx5" })
        for (const auto& sw : find (id)->switches)
            CHECK (! sw.press && sw.cc >= 75 && sw.cc <= 85);

    // Darkglass Anagram: no Clear Slot (CC#113); default scene CC#107 = 127, bank preview CC#103 / 104, slots CC#117 / 118.
    const auto& ana = *find ("darkglass.anagram");
    CHECK (! usesCc (ana, 113) && hasAction (ana.utilities, 107, 127) && hasAction (ana.utilities, 103, 127)
           && hasAction (ana.looper, 117, 127) && hasAction (ana.looper, 118, 127) && ana.pedals.front().cc == 89);
    // Nano Cortex, Darkglass amps: the manuals' charts were already complete.
    CHECK (find ("neural.nano-cortex")->switches.size() == 8 && find ("neural.nano-cortex")->notes.contains ("midi.guide"));
    CHECK (find ("darkglass.infinity-500-combo")->pedals.size() == 16 && find ("darkglass.exponent-500")->pedals.size() == 6);
}
