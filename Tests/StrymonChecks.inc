// Strymon pedal pages: checks against the MIDI charts (included in CueModelTests.cpp, inside the modellers block).
{
    int strymonCount = 0;
    for (const auto& p : all())
        if (p.brand == "Strymon")
        {
            ++strymonCount;
            CHECK (p.pedal && p.sceneCount == 0 && p.padCc == -1 && p.models.size() <= (size_t) modellers::maxModelGroups && custom::parse (p.testMessage, 0).ok());
            CHECK (! p.pedals.empty() && p.channelHint.isNotEmpty() && p.manual.isNotEmpty());
            // Every preset tile sends its bank on CC#0 first, then the Program Change.
            const auto last = preset (p, 1, -1, presetsPerSetlist (p, -1) - 1, false, {});
            CHECK (last.events.size() == 2 && last.events[0].second.isController() && last.events[0].second.getControllerNumber() == 0);
            // Footswitches are presses: down, then up 1/16 later, never repeated by the padding.
            CHECK (p.switches.empty() || (! p.switchesOnOff && p.switchesTitle == "Footswitches"));
            for (int si = 0; si < (int) p.switches.size(); ++si)
            {
                const auto& sw = p.switches[(size_t) si];
                const auto press = switchCue (p, 1, si, true, {});
                CHECK (sw.press && press.toggles && press.events.size() == 2 && isCC (press.events[0].second, 1, sw.cc, sw.value)
                       && isCC (press.events[1].second, 1, sw.cc, sw.offValue) && std::abs (press.events[1].first - 0.25) < 1.0e-9 && sw.value != sw.offValue);
            }
            // Controls stay within four rows of eight, the models within three groups.
            // Controls: at most four rows of utilities, never eight a row (ModellerPage lays 15-16 and 22-24 out eight wide).
            const auto count = (int) p.utilities.size();
            CHECK (count <= 28 && count != 15 && count != 16 && ! (count >= 22 && count <= 24) && p.switches.size() <= 6);
            for (const auto& c : p.pedals)
                CHECK (c.max >= 1 && c.max <= 127 && c.cc >= 1 && c.cc <= 127);
            for (const auto& u : p.utilities)
                CHECK (custom::parse (u.messages, 0).ok() && u.name.length() <= 12);
            for (const auto& g : p.models)
                for (const auto& m : g.actions)
                    CHECK (custom::parse (m.messages, 0).ok());
        }
    CHECK (strymonCount == 21);

    // TimeLine: 00A-99B, CC#0 = 1 from 64A; bypass 0 / engage 127; looper on any value.
    const auto& tl = *find ("strymon.timeline");
    CHECK (presetLabel (tl, -1, 0) == "00A" && presetLabel (tl, -1, 127) == "63B" && presetLabel (tl, -1, 130) == "65A"
           && presetLabel (tl, -1, 199) == "99B");
    auto tlp = preset (tl, 2, -1, 130, false, {});
    CHECK (isCC (tlp.events[0].second, 2, 0, 1) && tlp.events[1].second.getProgramChangeNumber() == 2 && tlp.name == "TimeLine 65A");
    CHECK (isCC (lengthPadding (tlp), 2, 0, 1));                                    // a preset repeats its bank CC
    CHECK (isCC (action (tl, 1, tl.utilities[0]).events[0].second, 1, 102, 127) && isCC (action (tl, 1, tl.utilities[1]).events[0].second, 1, 102, 0));
    CHECK (isCC (action (tl, 1, tl.utilities[2]).events[0].second, 1, 93, 127) && tl.looper.size() == 8);
    CHECK (isCC (action (tl, 1, tl.looper[0]).events[0].second, 1, 87, 127) && isCC (lengthPadding (action (tl, 1, tl.looper[0])), 1, 0, 0));
    CHECK (! tl.usbMidi && tl.hasDin && tl.models.size() == 4 && tl.pedals.front().cc == 100);
    // TimeLine footswitches: down = 0, up = 127 (TAP too, as the skill says; the manual prints it off = 0 / on = 127). Ranges.
    CHECK (tl.switches.size() == 3 && tl.switches[0].name == "A" && tl.switches[0].cc == 80 && tl.switches[0].value == 0
           && tl.switches[0].offValue == 127 && tl.switches[1].name == "Tap" && tl.switches[1].cc == 81 && tl.switches[1].value == 127 && tl.switches[1].offValue == 0
           && tl.utilities[2].name == "Remote tap"
           && tl.switches[2].name == "B" && tl.switches[2].cc == 82);
    auto tlPedal = [&tl] (const juce::String& name) { for (const auto& c : tl.pedals) if (c.name == name) return c; return Control {}; };
    CHECK (tlPedal ("Boost").cc == 23 && tlPedal ("Boost").max == 60 && tlPedal ("Smear").max == 18 && tlPedal ("High pass").cc == 47
           && tlPedal ("dTape Low end").cc == 59 && tlPedal ("dTape Low end").max == 20 && tlPedal ("Dual Time 2").max == 26
           && tlPedal ("Trem Speed").cc == 61 && tlPedal ("Trem Speed").max == 34 && tlPedal ("Looper level").max == 127);
    auto tlUtil = [&tl] (const juce::String& name) { for (const auto& u : tl.utilities) if (u.name == name) return u; return modellers::Action {}; };
    CHECK (isCC (action (tl, 1, tlUtil ("16th")).events[0].second, 1, 21, 4) && isCC (action (tl, 1, tlUtil ("Ice: Long")).events[0].second, 1, 46, 2)
           && isCC (action (tl, 1, tlUtil ("Exp on")).events[0].second, 1, 60, 1) && isCC (action (tl, 1, tlUtil ("Persist off")).events[0].second, 1, 22, 0)
           && tl.utilities.size() == 18);
    CHECK (tl.models[1].actions.size() == 12 && isCC (action (tl, 1, tl.models[1].actions[1]).events[0].second, 1, 58, 1)
           && tl.models[1].actions[7].name == "Dual: Parallel" && tl.models[1].actions[7].messages == "CC 36=1"
           && isCC (action (tl, 1, tl.models[2].actions[1]).events[0].second, 1, 29, 1) && tl.models[2].actions[1].name == "Trem: Square"
           && isCC (action (tl, 1, tl.models[2].actions[15]).events[0].second, 1, 28, 10) && tl.models[3].actions.back().name == "Lo-Fi: Intercom"
           && isCC (action (tl, 1, tl.models[3].actions.back()).events[0].second, 1, 53, 8));
    CHECK (tlPedal ("Ice Interval").cc == 30 && tlPedal ("Ice Interval").max == 29);

    // BigSky: 00A-99C, bank boundaries 42B | 42C and 85A | 85B.
    const auto& bs = *find ("strymon.bigsky");
    CHECK (presetLabel (bs, -1, 2) == "00C" && presetLabel (bs, -1, 127) == "42B" && presetLabel (bs, -1, 128) == "42C"
           && presetLabel (bs, -1, 255) == "85A" && presetLabel (bs, -1, 256) == "85B" && presetLabel (bs, -1, 299) == "99C");
    auto bsp = preset (bs, 1, -1, 128, false, {});
    CHECK (isCC (bsp.events[0].second, 1, 0, 1) && bsp.events[1].second.getProgramChangeNumber() == 0);
    CHECK (isCC (preset (bs, 1, -1, 256, false, {}).events[0].second, 1, 0, 2));
    CHECK (bs.looper.empty() && bs.models.size() == 4);
    // BigSky footswitches: A 80, B 82, C 81, down = 0 / up = 127; per-machine CCs.
    CHECK (bs.switches.size() == 3 && bs.switches[1].name == "C" && bs.switches[1].cc == 81 && bs.switches[2].cc == 82 && bs.switches[2].value == 0);
    CHECK (bs.pedals[8].name == "Boost" && bs.pedals[8].cc == 23 && bs.pedals[8].max == 60 && bs.pedals.back().cc == 49 && bs.pedals.back().max == 6);
    CHECK (isCC (action (bs, 1, bs.models[3].actions[6]).events[0].second, 1, 33, 6) && isCC (action (bs, 1, bs.models[2].actions[9]).events[0].second, 1, 57, 2)
           && isCC (action (bs, 1, bs.models[1].actions[0]).events[0].second, 1, 59, 0) && bs.utilities.size() == 13
           && isCC (action (bs, 1, bs.utilities[6]).events[0].second, 1, 70, 1));

    const auto& mob = *find ("strymon.mobius");
    CHECK (presetLabel (mob, -1, 199) == "99B" && mob.pedals[1].cc == 17 && mob.models.size() == 1 && mob.switches.size() == 3);
    CHECK (mob.pedals[6].name == "Chorus Mix" && mob.pedals[6].cc == 29 && mob.pedals[6].max == 17 && mob.pedals.back().cc == 55
           && mob.pedals.back().max == 20 && isCC (action (mob, 1, mob.utilities.back()).events[0].second, 1, 60, 0));

    // MX: 000A-149B, CC#0 0-2; TimeLine MX delay types 0-11 on CC#1 / CC#2.
    const auto& tmx = *find ("strymon.timeline-mx");
    CHECK (presetLabel (tmx, -1, 0) == "000A" && presetLabel (tmx, -1, 299) == "149B" && presetLabel (tmx, -1, 128) == "064A");
    auto tmxp = preset (tmx, 1, -1, 299, false, {});
    CHECK (isCC (tmxp.events[0].second, 1, 0, 2) && tmxp.events[1].second.getProgramChangeNumber() == 43);
    CHECK (tmx.models.size() == 3 && tmx.models[0].actions.size() == 12 && tmx.models[1].actions.size() == 12);
    CHECK (isCC (action (tmx, 1, tmx.models[1].actions[11]).events[0].second, 1, 2, 11) && tmx.models[0].actions[0].name == "Spectral");
    CHECK (isCC (action (tmx, 1, tmx.models[2].actions[5]).events[0].second, 1, 124, 5));
    CHECK (isCC (action (tmx, 1, tmx.looper[0]).events[0].second, 1, 119, 127) && isCC (action (tmx, 1, tmx.looper[7]).events[0].second, 1, 99, 127));
    CHECK (tmx.usbMidi && tmx.hasThru && tmx.usbToThru == 0);
    // MX footswitches: 0 = press, 127 = release, TAP on CC#82. Tap divisions 0-7 per delay; ranged parameters in pairs.
    CHECK (tmx.switches.size() == 3 && tmx.switches[2].name == "Tap" && tmx.switches[2].cc == 82 && tmx.switches[2].value == 0
           && tmx.switches[2].offValue == 127);
    CHECK (tmx.utilities.size() == 27 && isCC (action (tmx, 1, tmx.utilities.back()).events[0].second, 1, 18, 7)
           && isCC (action (tmx, 1, tmx.utilities[11]).events[0].second, 1, 17, 0) && tmx.utilities[12].name == "D1: 1/8 dot");
    auto tmxPedal = [&tmx] (const juce::String& name) { for (const auto& c : tmx.pedals) if (c.name == name) return c; return Control {}; };
    CHECK (tmxPedal ("Boost").cc == 122 && tmxPedal ("Boost").max == 60 && tmxPedal ("Looper level").cc == 127 && tmxPedal ("Looper level").max == 100
           && tmxPedal ("Output level 2").cc == 8 && tmxPedal ("High pass 1").max == 19 && tmxPedal ("Ice Interval 2").cc == 54
           && tmxPedal ("Ice Interval 2").max == 29 && tmxPedal ("dTape Low contour 1").max == 21);
    const auto& bmx = *find ("strymon.bigsky-mx");
    CHECK (isCC (action (bmx, 1, bmx.models[2].actions[2]).events[0].second, 1, 99, 2) && isCC (action (bmx, 1, bmx.models[4].actions[0]).events[0].second, 1, 18, 0));
    CHECK (bmx.pedals[1].cc == 3 && bmx.pedals[2].cc == 4 && bmx.switches.size() == 3 && bmx.switches[1].cc == 81 && bmx.switches[2].name == "Infinite");
    auto bmxPedal = [&bmx] (const juce::String& name) { for (const auto& c : bmx.pedals) if (c.name == name) return c; return Control {}; };
    CHECK (bmxPedal ("Boost").cc == 79 && bmxPedal ("Boost").max == 60 && bmxPedal ("Output level 1").max == 16 && bmxPedal ("Low end 2").cc == 24
           && bmxPedal ("Chorale Vowel 2").cc == 94 && bmxPedal ("Chorale Vowel 2").max == 6 && bmxPedal ("Magneto Heads 1").max == 4);
    for (const auto& c : bmx.pedals)
        CHECK (c.cc != 72 && c.cc != 71 && c.cc != 73 && c.cc != 1 && c.cc != 2);   // shimmer shifts (CC#72 printed twice) and the type are left out

    // Volante: 0-299, bank 0's PC 127 = Manual mode; types 1-3.
    const auto& vol = *find ("strymon.volante");
    CHECK (presetLabel (vol, -1, 0) == "Preset 0" && presetLabel (vol, -1, 127) == "Manual mode" && presetLabel (vol, -1, 255) == "Preset 255");
    auto volp = preset (vol, 1, -1, 200, false, {});
    CHECK (isCC (volp.events[0].second, 1, 0, 1) && volp.events[1].second.getProgramChangeNumber() == 72);
    CHECK (isCC (action (vol, 1, vol.models[0].actions[0]).events[0].second, 1, 11, 1) && isCC (action (vol, 1, vol.models[1].actions[2]).events[0].second, 1, 18, 94));
    CHECK (isCC (action (vol, 1, vol.models[0].actions[5]).events[0].second, 1, 19, 3) && vol.models[2].actions.size() == 16
           && isCC (action (vol, 1, vol.models[2].actions[14]).events[0].second, 1, 37, 127));
    CHECK (vol.switches.size() == 3 && vol.switches[0].name == "On" && vol.switches[0].cc == 80 && vol.switches[0].value == 127
           && vol.switches[0].offValue == 0 && vol.switches[2].cc == 82);
    CHECK (vol.hasThru && vol.usbMidi && vol.usbToThru == 3 && vol.looperTitle == "SOS looper" && vol.testMessage == "CC 0=0, PC 0");

    // cloudburst, Brig, Olivera, Ultraviolet: Manual mode at PC 127 in every bank; PC 0-3 Favorite / MultiSwitch.
    for (const auto* id : { "strymon.cloudburst", "strymon.brig", "strymon.olivera", "strymon.ultraviolet" })
    {
        const auto& p = *find (id);
        CHECK (presetLabel (p, -1, 0) == "Favorite" && presetLabel (p, -1, 3) == "MultiSwitch 3" && presetLabel (p, -1, 4) == "Preset 4"
               && presetLabel (p, -1, 127) == "Manual mode" && presetLabel (p, -1, 255) == "Manual mode" && presetLabel (p, -1, 254) == "Preset 254"
               && presetLabel (p, -1, 299) == "Preset 299" && p.testMessage == "CC 0=0, PC 4");
    }
    // The v2 pedals and Zelzah: Manual mode only in bank 0.
    for (const auto* id : { "strymon.flint-v2", "strymon.el-capistan-v2", "strymon.deco-v2", "strymon.dig-v2", "strymon.lex-v2",
                            "strymon.bluesky-v2", "strymon.zelzah" })
    {
        const auto& p = *find (id);
        CHECK (presetLabel (p, -1, 0) == "Favorite" && presetLabel (p, -1, 127) == "Manual mode" && presetLabel (p, -1, 255) == "Preset 255"
               && p.usbMidi && ! p.hasThru && p.midiIn.startsWith ("TRS") && p.pedals.front().cc == 100);
    }
    for (const auto* id : { "strymon.sunset", "strymon.riverside", "strymon.compadre", "strymon.iridium", "strymon.cloudburst" })
        CHECK (! find (id)->usbMidi && find (id)->hasDin && ! find (id)->hasThru);

    const auto& cb = *find ("strymon.cloudburst");
    auto cbp = preset (cb, 4, -1, 299, false, {});
    CHECK (isCC (cbp.events[0].second, 4, 0, 2) && cbp.events[1].second.getProgramChangeNumber() == 43);
    CHECK (isCC (action (cb, 1, cb.utilities[2]).events[0].second, 1, 97, 127) && isCC (action (cb, 1, cb.utilities[4]).events[0].second, 1, 98, 127));

    const auto& sun = *find ("strymon.sunset");
    CHECK (isCC (action (sun, 1, sun.utilities[0]).events[0].second, 1, 33, 127) && isCC (action (sun, 1, sun.utilities[2]).events[0].second, 1, 10, 127)
           && isCC (action (sun, 1, sun.utilities[5]).events[0].second, 1, 15, 0));
    CHECK (isCC (action (sun, 1, sun.models[2].actions[2]).events[0].second, 1, 20, 3) && presetLabel (sun, -1, 127) == "Manual mode");

    const auto& iri = *find ("strymon.iridium");
    CHECK (presetLabel (iri, -1, 0) == "Favorite" && presetLabel (iri, -1, 1) == "Preset 1" && iri.models[1].actions.size() == 9
           && isCC (action (iri, 1, iri.models[1].actions[8]).events[0].second, 1, 20, 8) && isCC (action (iri, 1, iri.utilities[3]).events[0].second, 1, 21, 127));

    const auto& cmp = *find ("strymon.compadre");
    CHECK (presetLabel (cmp, -1, 1) == "MultiSwitch 1" && presetLabel (cmp, -1, 255) == "Preset 255"
           && isCC (action (cmp, 1, cmp.utilities[0]).events[0].second, 1, 13, 127) && cmp.pedals.front().cc == 12);

    const auto& flint = *find ("strymon.flint-v2");
    CHECK (preset (flint, 1, -1, 5, false, {}).name == "Flint Preset 5" && isCC (action (flint, 1, flint.models[2].actions[1]).events[0].second, 1, 23, 127));
    const auto& dig = *find ("strymon.dig-v2");
    CHECK (dig.switches.empty() && isCC (action (dig, 1, dig.models[2].actions[2]).events[0].second, 1, 19, 2)
           && isCC (action (dig, 1, dig.models[1].actions[1]).events[0].second, 1, 22, 1) && isCC (action (dig, 1, dig.models[0].actions[5]).events[0].second, 1, 11, 3) && dig.pedals.back().cc == 23 && dig.pedals.back().max == 126);
    CHECK (isCC (action (dig, 1, dig.utilities.back()).events[0].second, 1, 25, 6) && dig.utilities.back().name == "Clk 1/4");
    const auto& deco = *find ("strymon.deco-v2");
    CHECK (isCC (action (deco, 1, deco.utilities.back()).events[0].second, 1, 25, 6) && deco.utilities.back().name == "Clk x8");
    const auto& elcap = *find ("strymon.el-capistan-v2");
    CHECK (elcap.models.size() == 3 && elcap.models[2].actions.size() == 7 && isCC (action (elcap, 1, elcap.models[2].actions[0]).events[0].second, 1, 25, 0)
           && elcap.models[2].actions[0].name == "Clk x4" && elcap.switches.size() == 2 && elcap.switches[0].cc == 27);
    CHECK (flint.switches.size() == 2 && flint.switches[1].cc == 28 && flint.utilities.back().name == "Clk x4");
    const auto& lex = *find ("strymon.lex-v2");
    CHECK (isCC (action (lex, 1, lex.utilities[2]).events[0].second, 1, 22, 127) && isCC (action (lex, 1, lex.utilities[3]).events[0].second, 1, 22, 0));
    const auto& zel = *find ("strymon.zelzah");
    CHECK (isCC (action (zel, 1, zel.utilities[4]).events[0].second, 1, 17, 127) && isCC (action (zel, 1, zel.models[2].actions[2]).events[0].second, 1, 23, 2));
    CHECK (isCC (action (zel, 1, zel.utilities.back()).events[0].second, 1, 15, 1) && zel.switches.size() == 2);
    CHECK (isCC (action (lex, 1, lex.utilities[11]).events[0].second, 1, 20, 1));
    // Single-footswitch pedals; the Iridium's Fav / On.
    for (const auto* id : { "strymon.cloudburst", "strymon.brig", "strymon.olivera", "strymon.ultraviolet" })
        CHECK (find (id)->switches.size() == 1 && find (id)->switches[0].cc == 27 && find (id)->switches[0].value == 127);
    CHECK (iri.switches.size() == 2 && iri.switches[1].name == "On" && iri.switches[1].cc == 28);
    // MIDI expression on / off (CC#60) wherever the table lists it; never on the Compadre or Brig.
    for (const auto& prof : all())
        if (prof.brand == "Strymon")
        {
            const auto hasExp = std::any_of (prof.utilities.begin(), prof.utilities.end(),
                                             [] (const modellers::Action& u) { return u.messages.startsWith ("CC 60="); });
            CHECK (hasExp == (prof.id != "strymon.compadre" && prof.id != "strymon.brig" && ! prof.id.contains ("-mx")));
        }
    const auto& brig = *find ("strymon.brig");
    CHECK (isCC (action (brig, 1, brig.models[1].actions[3]).events[0].second, 1, 17, 3) && isCC (action (brig, 1, brig.utilities[2]).events[0].second, 1, 93, 127));
    // Type encoders: numbered tiles 0-11 with the amber warning (the manuals give no order); never more groups than the page shows.
    for (const auto& [id, number, group] : { std::tuple<const char*, int, int> { "strymon.timeline", 19, 0 }, { "strymon.bigsky", 19, 0 },
                                             { "strymon.mobius", 19, 0 }, { "strymon.bigsky-mx", 1, 0 }, { "strymon.bigsky-mx", 2, 1 } })
    {
        const auto& g = find (id)->models[(size_t) group];
        CHECK (g.actions.size() == 12 && g.warning.isNotEmpty() && g.actions[11].name == "Type 11"
               && isCC (action (*find (id), 1, g.actions[7]).events[0].second, 1, number, 7));
    }
    for (const auto& prof : all())
        CHECK (prof.models.size() <= (size_t) modellers::maxModelGroups);
    // The third view says "Knobs" exactly where the chart has no expression CC (Compadre, Walrus MAKO, Nemesis).
    for (const auto& prof : all())
        if (prof.pedal && ! prof.pedals.empty())
        {
            const auto hasExp = std::any_of (prof.pedals.begin(), prof.pedals.end(),
                                             [] (const modellers::Control& c) { return c.name == "Expression"; });
            CHECK ((prof.pedalsTitle == "Knobs") == ! hasExp && (hasExp ? prof.pedalsTitle == "Expression" : true));
        }
    CHECK (find ("strymon.compadre")->pedalsTitle == "Knobs" && find ("walrus.mako-acs1")->pedalsTitle == "Knobs"
           && find ("sourceaudio.nemesis")->pedalsTitle == "Knobs" && find ("sourceaudio.ventris")->pedalsTitle == "Expression");
}
