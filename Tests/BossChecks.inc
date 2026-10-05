// Boss pedal pages: checks against the MIDI charts (included in CueModelTests.cpp, inside the modellers block).
{
    // DD-500 / RV-500 / MD-500: 297 patches 01A-99C, bank select CC#0 = index / 128 then PC = index % 128 (the transmit table).
    for (const auto* id : { "boss.dd-500", "boss.rv-500", "boss.md-500" })
    {
        const auto* b = find (id);
        CHECK (b != nullptr && b->pedal && b->scheme == Scheme::boss500 && b->sceneCount == 0 && b->padCc == -1);
        if (b == nullptr)
            continue;
        CHECK (presetsPerSetlist (*b, -1) == 297 && presetLabel (*b, -1, 0) == "01A" && presetLabel (*b, -1, 1) == "01B"
               && presetLabel (*b, -1, 127) == "43B" && presetLabel (*b, -1, 128) == "43C" && presetLabel (*b, -1, 296) == "99C");
        const auto bankAndPc = [&] (int index, int bank, int pc)
        {
            const auto c = preset (*b, 1, -1, index, false, {});
            return c.events.size() == 2 && isCC (c.events[0].second, 1, 0, bank) && c.events[1].second.isProgramChange()
                   && c.events[1].second.getProgramChangeNumber() == pc;
        };
        CHECK (bankAndPc (0, 0, 0) && bankAndPc (127, 0, 127) && bankAndPc (128, 1, 0) && bankAndPc (255, 1, 127)   // 01A, 43B, 43C, 86A
               && bankAndPc (256, 2, 0) && bankAndPc (296, 2, 40));                                                 // 86B, 99C
        CHECK (b->pedals.size() >= 5 && b->pedals[0].name == "Expression" && b->pedals[0].cc == 16 && b->pedals[1].cc == 17);
        CHECK (b->tunerOn.isEmpty() && b->switches.empty() && b->notes.contains ("MIDI PC MAP") && b->notes.contains ("MIDI menu"));
        // No footswitch-emulation CCs (A/B/TAP-CTL 82-84, CTL 1/2 80-81: 127 pressed, 0 released) in any tile.
        for (const auto& bossList : { b->utilities, b->looper })
            for (const auto& bossAction : bossList)
                for (const auto& st : custom::parse (bossAction.messages, 0).steps)
                    CHECK (st.number < 80 || st.number > 84);
    }
    const auto& dd = *find ("boss.dd-500");
    CHECK (dd.utilities.size() == 2 && isCC (action (dd, 1, dd.utilities[0]).events[0].second, 1, 21, 127)
           && isCC (action (dd, 1, dd.utilities[1]).events[0].second, 1, 21, 0));
    CHECK (dd.looper.size() == 5 && isCC (action (dd, 1, dd.looper[2]).events[0].second, 1, 23, 127)
           && isCC (action (dd, 1, dd.looper[4]).events[0].second, 1, 25, 127) && dd.pedals.size() == 5 && dd.pedals[4].cc == 20);
    for (const auto& ddLoop : dd.looper)
        CHECK (! ddLoop.messages.contains ("26"));                                  // CC#26 clears the phrase: no tile
    const auto& rv = *find ("boss.rv-500");
    CHECK (rv.looper.empty() && rv.pedals.size() == 11 && rv.pedals[6].cc == 22 && rv.pedals[10].cc == 26
           && isCC (action (rv, 1, rv.utilities[0]).events[0].second, 1, 27, 127) && isCC (action (rv, 1, rv.utilities[5]).events[0].second, 1, 29, 0));
    const auto& md = *find ("boss.md-500");
    CHECK (md.looper.empty() && md.pedals.size() == 6 && md.pedals[5].cc == 21
           && isCC (action (md, 1, md.utilities[2]).events[0].second, 1, 28, 127));
    // Utility clips are padded with CC#0 = 0 (a bank number the next preset tile overrides), never a repeat.
    const auto ddOn = action (dd, 1, dd.utilities[0]);
    CHECK (ddOn.toggles && ddOn.padCc == -1);
}

// Templates: Eventide H9 / H90, Boss RC-500 / RC-600 (no default CCs). The first preset tile's Program Change on the wire.
{
    const auto firstTile = [] (const juce::String& id, int group, int tile)
    {
        const auto* t = templates::find (id);
        if (t == nullptr)
            return cues::Cue {};
        const auto unit = templates::createUnit (*t);
        const auto tileTree = unit.getChild (group).getChild (tile);
        return custom::cue (1, "x", tileTree[IDs::messages].toString(), (int) unit[IDs::programBase]);
    };
    const auto firstPc = [&] (const juce::String& id, int tile)
    {
        const auto c = firstTile (id, 0, tile);
        return c.events.size() == 1 && c.events[0].second.isProgramChange() ? c.events[0].second.getProgramChangeNumber() : -1;
    };
    for (const auto* id : { "eventide.h9", "eventide.h90", "boss.rc-500", "boss.rc-600" })
    {
        const auto* t = templates::find (id);
        CHECK (t != nullptr && t->pedal && t->notes.startsWith ("Why this is a template") && t->notes.contains ("Not tested on hardware")
               && t->groups.size() >= 2 && t->groups[0].tiles.size() == 8);
    }
    CHECK (firstPc ("eventide.h9", 0) == 1 && firstPc ("eventide.h9", 7) == 8);      // P01 = PC 1 (default RCV.MAP)
    CHECK (firstPc ("eventide.h90", 0) == 0 && firstPc ("eventide.h90", 7) == 7);    // Program 1 = PC 1 in its 1-100 numbering
    CHECK (firstPc ("boss.rc-500", 0) == 0 && firstPc ("boss.rc-500", 7) == 7);      // memory 01 = PC 01 (1-128 numbering)
    CHECK (firstPc ("boss.rc-600", 0) == 0 && firstPc ("boss.rc-600", 7) == 7);      // memory 01 = PC 0
    CHECK (templates::find ("eventide.h9")->expCc == 22);                             // KB0 = C22 by default
    CHECK (templates::find ("boss.rc-500")->notes.contains ("OMNI is ON"));
    // Suggested CCs stay inside what each pedal can assign: H9 C0-C99 away from KB0-KB9 (C22-C31); RC sources CC#1-31 / 64-95.
    for (const auto* id : { "eventide.h9", "boss.rc-500", "boss.rc-600" })
        for (const auto& g : templates::find (id)->groups)
            for (const auto& tile : g.tiles)
                for (const auto& st : custom::parse (tile.messages, templates::find (id)->programBase).steps)
                    if (st.kind != custom::Step::Kind::program)
                        CHECK ((st.number >= 1 && st.number <= 31 && juce::String (id) != "eventide.h9") || (st.number >= 64 && st.number <= 95));
    const auto h90On = firstTile ("eventide.h90", 1, 2), h90Off = firstTile ("eventide.h90", 1, 3);   // P Act/Byp (M): >= 64 active
    CHECK (isCC (h90On.events[0].second, 1, 82, 127) && isCC (h90Off.events[0].second, 1, 82, 0));
}
