// Strymon pedal pages: checks against the MIDI charts (included in CueModelTests.cpp, inside the modellers block).
{
    int strymonCount = 0;
    for (const auto& p : all())
        if (p.brand == "Strymon")
        {
            ++strymonCount;
            CHECK (p.pedal && p.sceneCount == 0 && p.padCc == -1 && p.models.size() <= 3 && custom::parse (p.testMessage, 0).ok());
            CHECK (! p.pedals.empty() && p.channelHint.isNotEmpty() && p.manual.isNotEmpty());
            // Every preset tile sends its bank on CC#0 first, then the Program Change.
            const auto last = preset (p, 1, -1, presetsPerSetlist (p, -1) - 1, false, {});
            CHECK (last.events.size() == 2 && last.events[0].second.isController() && last.events[0].second.getControllerNumber() == 0);
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
    CHECK (! tl.usbMidi && tl.hasDin && tl.models.empty() && tl.pedals.front().cc == 100);

    // BigSky: 00A-99C, bank boundaries 42B | 42C and 85A | 85B.
    const auto& bs = *find ("strymon.bigsky");
    CHECK (presetLabel (bs, -1, 2) == "00C" && presetLabel (bs, -1, 127) == "42B" && presetLabel (bs, -1, 128) == "42C"
           && presetLabel (bs, -1, 255) == "85A" && presetLabel (bs, -1, 256) == "85B" && presetLabel (bs, -1, 299) == "99C");
    auto bsp = preset (bs, 1, -1, 128, false, {});
    CHECK (isCC (bsp.events[0].second, 1, 0, 1) && bsp.events[1].second.getProgramChangeNumber() == 0);
    CHECK (isCC (preset (bs, 1, -1, 256, false, {}).events[0].second, 1, 0, 2));
    CHECK (bs.looper.empty() && bs.models.empty());

    const auto& mob = *find ("strymon.mobius");
    CHECK (presetLabel (mob, -1, 199) == "99B" && mob.pedals.size() == 6 && mob.pedals[1].cc == 17);

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
    const auto& bmx = *find ("strymon.bigsky-mx");
    CHECK (isCC (action (bmx, 1, bmx.models[0].actions[2]).events[0].second, 1, 99, 2) && isCC (action (bmx, 1, bmx.models[2].actions[0]).events[0].second, 1, 18, 0));
    CHECK (bmx.pedals.size() == 15 && bmx.pedals[1].cc == 3 && bmx.pedals[2].cc == 4);

    // Volante: 0-299, bank 0's PC 127 = Manual mode; types 1-3.
    const auto& vol = *find ("strymon.volante");
    CHECK (presetLabel (vol, -1, 0) == "Preset 0" && presetLabel (vol, -1, 127) == "Manual mode" && presetLabel (vol, -1, 255) == "Preset 255");
    auto volp = preset (vol, 1, -1, 200, false, {});
    CHECK (isCC (volp.events[0].second, 1, 0, 1) && volp.events[1].second.getProgramChangeNumber() == 72);
    CHECK (isCC (action (vol, 1, vol.models[0].actions[0]).events[0].second, 1, 11, 1) && isCC (action (vol, 1, vol.models[2].actions[2]).events[0].second, 1, 18, 94));
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
    CHECK (isCC (switchCue (dig, 1, 2, true, {}).events[0].second, 1, 19, 2) && isCC (action (dig, 1, dig.models[2].actions[1]).events[0].second, 1, 22, 1));
    const auto& lex = *find ("strymon.lex-v2");
    CHECK (isCC (action (lex, 1, lex.utilities[2]).events[0].second, 1, 22, 127) && isCC (action (lex, 1, lex.utilities[3]).events[0].second, 1, 22, 0));
    const auto& zel = *find ("strymon.zelzah");
    CHECK (isCC (action (zel, 1, zel.utilities[4]).events[0].second, 1, 17, 127) && isCC (action (zel, 1, zel.models[2].actions[2]).events[0].second, 1, 23, 2));
    const auto& brig = *find ("strymon.brig");
    CHECK (isCC (action (brig, 1, brig.models[1].actions[3]).events[0].second, 1, 17, 3) && isCC (action (brig, 1, brig.utilities[2]).events[0].second, 1, 93, 127));
}
