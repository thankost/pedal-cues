// "Read names from the unit" (MidiNameSync.h): byte-exact requests, checksums, reply parsing, a simulated unit, the
// read-only whitelist. Included in CueModelTests.cpp, at the end of the modellers block. Everything is qualified with namesync::.
{
    using Bytes = std::vector<juce::uint8>;
    const auto raw = [] (const juce::MidiMessage& m) { return Bytes (m.getRawData(), m.getRawData() + m.getRawDataSize()); };
    const auto msgOf = [] (const Bytes& b) { return juce::MidiMessage (b.data(), (int) b.size()); };

    // Targets.
    CHECK (namesync::targetFor ("boss.gt-1000")->protocol == namesync::Protocol::bossGt1000 && namesync::targetFor ("boss.gt-1000")->customDevice
           && namesync::targetFor ("boss.gt-1000")->presetCount == 250 && namesync::targetFor ("boss.gt-1000")->model == 0x4F);
    CHECK (namesync::targetFor ("boss.dd-500")->model == 0x4D && namesync::targetFor ("boss.md-500")->model == 0x43
           && namesync::targetFor ("boss.rv-500")->model == 0x42 && namesync::targetFor ("boss.rv-500")->presetCount == 297
           && ! namesync::targetFor ("boss.rv-500")->customDevice);
    CHECK (namesync::targetFor ("fractal.axe-fx-3")->model == 0x10 && namesync::targetFor ("fractal.fm3")->model == 0x11
           && namesync::targetFor ("fractal.fm9")->model == 0x12 && namesync::targetFor ("fractal.vp4")->model == 0x14
           && namesync::targetFor ("fractal.vp4")->presetCount == 104 && namesync::targetFor ("fractal.fm9")->sceneNames);
    CHECK (namesync::targetFor ("strymon.timeline")->model == 1 && namesync::targetFor ("strymon.mobius")->model == 2
           && namesync::targetFor ("strymon.bigsky")->model == 3 && namesync::targetFor ("strymon.bigsky")->presetCount == 300
           && namesync::targetFor ("strymon.timeline")->presetCount == 200);
    CHECK (! namesync::targetFor ("line6.helix-floor") && ! namesync::targetFor ("strymon.volante") && ! namesync::targetFor ({}));
    // Every page target is a real page, every template target a real template.
    for (const auto* id : { "boss.dd-500", "boss.md-500", "boss.rv-500", "strymon.timeline", "strymon.mobius", "strymon.bigsky" })
        CHECK (modellers::find (id) != nullptr);
    for (const auto* id : { "boss.gt-1000", "fractal.axe-fx-3", "fractal.fm9", "fractal.fm3", "fractal.vp4" })
        CHECK (templates::find (id) != nullptr);

    // Roland 7-bit address arithmetic and checksum.
    CHECK (namesync::roland::bytes (namesync::roland::linear (0x20, 0x7F, 0, 0) + (1 << 14)) == (std::array<juce::uint8, 4> { 0x21, 0, 0, 0 }));
    CHECK (namesync::roland::bytes (namesync::roland::linear (0, 0x10, 0, 0x7C) + 4) == (std::array<juce::uint8, 4> { 0, 0x10, 1, 0 }));
    {
        const juce::uint8 addr8[] = { 0x20, 0, 0, 0, 0, 0, 0, 0x10 };
        CHECK (namesync::roland::checksum (addr8, 8) == 0x50);
    }

    // GT-1000 RQ1: user patch 1 (20 00 00 00) and 250 (21 79 00 00), 16 bytes (the report's frame).
    CHECK (raw (namesync::roland::requestData (0x4F, 0x10, namesync::roland::gt1000PatchNameAddress (0), 16))
           == (Bytes { 0xF0, 0x41, 0x10, 0, 0, 0, 0x4F, 0x11, 0x20, 0, 0, 0, 0, 0, 0, 0x10, 0x50, 0xF7 }));
    CHECK (raw (namesync::roland::requestData (0x4F, 0x10, namesync::roland::gt1000PatchNameAddress (249), 16))
           == (Bytes { 0xF0, 0x41, 0x10, 0, 0, 0, 0x4F, 0x11, 0x21, 0x79, 0, 0, 0, 0, 0, 0x10, 0x56, 0xF7 }));
    CHECK (namesync::roland::bytes (namesync::roland::gt1000PatchNameAddress (127)) == (std::array<juce::uint8, 4> { 0x20, 0x7F, 0, 0 })
           && namesync::roland::bytes (namesync::roland::gt1000PatchNameAddress (128)) == (std::array<juce::uint8, 4> { 0x21, 0, 0, 0 }));
    // PROGRAM MAP: BANK1-4 at 00 10 00 00 / 00 10 04 00 / 00 10 08 00 / 00 10 0C 00, 4 bytes per PC.
    CHECK (namesync::roland::bytes (namesync::roland::gt1000ProgramMapAddress (0, 0)) == (std::array<juce::uint8, 4> { 0, 0x10, 0, 0 })
           && namesync::roland::bytes (namesync::roland::gt1000ProgramMapAddress (1, 0)) == (std::array<juce::uint8, 4> { 0, 0x10, 4, 0 })
           && namesync::roland::bytes (namesync::roland::gt1000ProgramMapAddress (3, 0)) == (std::array<juce::uint8, 4> { 0, 0x10, 0x0C, 0 })
           && namesync::roland::bytes (namesync::roland::gt1000ProgramMapAddress (0, 32)) == (std::array<juce::uint8, 4> { 0, 0x10, 1, 0 }));
    CHECK (namesync::roland::gt1000PatchLabel (0) == "U01-1" && namesync::roland::gt1000PatchLabel (249) == "U50-5"
           && namesync::roland::gt1000PatchLabel (6) == "U02-2");

    // DD-500 / RV-500 / MD-500: bank 1 patch A = 30 04 10 00, bank 99 patch C = 33 0C 30 00.
    CHECK (raw (namesync::roland::requestData (0x4D, 0x10, namesync::roland::boss500PatchNameAddress (0), 16))
           == (Bytes { 0xF0, 0x41, 0x10, 0, 0, 0, 0x4D, 0x11, 0x30, 0x04, 0x10, 0, 0, 0, 0, 0x10, 0x2C, 0xF7 }));
    CHECK (raw (namesync::roland::requestData (0x42, 0x10, namesync::roland::boss500PatchNameAddress (296), 16))
           == (Bytes { 0xF0, 0x41, 0x10, 0, 0, 0, 0x42, 0x11, 0x33, 0x0C, 0x30, 0, 0, 0, 0, 0x10, 0x01, 0xF7 }));
    CHECK (namesync::roland::bytes (namesync::roland::boss500PatchNameAddress (4)) == (std::array<juce::uint8, 4> { 0x30, 0x08, 0x20, 0 }));   // 02B

    // Roland DT1 replies.
    const auto dt1 = [] (int dev, int model, int address, const Bytes& data, bool goodSum)
    {
        Bytes b { 0xF0, 0x41, (juce::uint8) dev, 0, 0, 0, (juce::uint8) model, 0x12 };
        const auto ab = namesync::roland::bytes (address);
        b.insert (b.end(), ab.begin(), ab.end());
        b.insert (b.end(), data.begin(), data.end());
        b.push_back ((juce::uint8) ((namesync::roland::checksum (b.data() + 8, (int) b.size() - 8) + (goodSum ? 0 : 1)) & 0x7F));
        b.push_back (0xF7);
        return b;
    };
    const auto ascii16 = [] (const juce::String& text)
    {
        Bytes b;
        for (int i = 0; i < 16; ++i)
            b.push_back ((juce::uint8) (i < text.length() ? text[i] : ' '));
        return b;
    };
    {
        const auto reply = dt1 (0x10, 0x4F, namesync::roland::gt1000PatchNameAddress (0), ascii16 ("CLEAN AMB"), true);
        const auto d = namesync::roland::parseData (reply.data(), (int) reply.size());
        CHECK (d && d->model == 0x4F && d->deviceId == 0x10 && d->address == namesync::roland::linear (0x20, 0, 0, 0)
               && d->data.size() == 16 && d->checksumOk && d->data[0] == 'C');
        const auto bad = dt1 (0x10, 0x4F, 0, ascii16 ("X"), false);
        CHECK (namesync::roland::parseData (bad.data(), (int) bad.size())->checksumOk == false);
        const auto rq = raw (namesync::roland::requestData (0x4F, 0x10, 0, 16));   // an RQ1 is not a DT1 reply
        CHECK (! namesync::roland::parseData (rq.data(), (int) rq.size()));
    }

    // Fractal: XOR checksum (the forum's Axe-Fx II example F0 00 01 74 03 0F 09 F7), 0x0D / 0x0E queries.
    {
        const juce::uint8 example[] = { 0xF0, 0x00, 0x01, 0x74, 0x03, 0x0F };
        CHECK (namesync::fractal::checksum (example, 6) == 0x09);
    }
    CHECK (raw (namesync::fractal::queryPatchName (0x10, 0)) == (Bytes { 0xF0, 0, 0x01, 0x74, 0x10, 0x0D, 0, 0, 0x18, 0xF7 }));
    CHECK (raw (namesync::fractal::queryPatchName (0x10, 300)) == (Bytes { 0xF0, 0, 0x01, 0x74, 0x10, 0x0D, 0x2C, 0x02, 0x36, 0xF7 }));   // LS first
    CHECK (raw (namesync::fractal::queryPatchName (0x10, namesync::fractal::current))[6] == 0x7F
           && raw (namesync::fractal::queryPatchName (0x10, namesync::fractal::current))[7] == 0x7F);
    CHECK (raw (namesync::fractal::querySceneName (0x12, 3)) == (Bytes { 0xF0, 0, 0x01, 0x74, 0x12, 0x0E, 0x03, 0x1A, 0xF7 }));
    const auto fractalReply = [] (int model, bool scene, int number, const juce::String& name, bool goodSum)
    {
        Bytes b { 0xF0, 0, 0x01, 0x74, (juce::uint8) model, (juce::uint8) (scene ? 0x0E : 0x0D) };
        if (scene)
            b.push_back ((juce::uint8) number);
        else
        {
            b.push_back ((juce::uint8) (number & 0x7F));
            b.push_back ((juce::uint8) (number >> 7));
        }
        for (int i = 0; i < 32; ++i)
            b.push_back ((juce::uint8) (i < name.length() ? name[i] : ' '));
        b.push_back ((juce::uint8) ((namesync::fractal::checksum (b.data(), (int) b.size()) + (goodSum ? 0 : 1)) & 0x7F));
        b.push_back (0xF7);
        return b;
    };
    {
        const auto r = fractalReply (0x10, false, 300, "Plexi Crunch", true);
        const auto n = namesync::fractal::parseName (r.data(), (int) r.size(), 0x10);
        CHECK (r.size() == 42 && n && ! n->scene && n->number == 300 && n->name == "Plexi Crunch" && n->checksumOk);
        const auto sc = fractalReply (0x10, true, 2, "Solo", false);
        const auto sn = namesync::fractal::parseName (sc.data(), (int) sc.size(), 0x10);
        CHECK (sn && sn->scene && sn->number == 2 && sn->name == "Solo" && ! sn->checksumOk);
        CHECK (! namesync::fractal::parseName (r.data(), (int) r.size(), 0x12));   // another model
    }

    // Strymon: request 63 hi lo; the 650-byte dump with the name at 632 (the spl Mobius example's tail "CALL THE COPS   " 55 F7).
    CHECK (raw (namesync::strymon::requestPreset (2, 0)) == (Bytes { 0xF0, 0, 0x01, 0x55, 0x12, 0x02, 0x63, 0, 0, 0xF7 }));
    CHECK (raw (namesync::strymon::requestPreset (3, 150)) == (Bytes { 0xF0, 0, 0x01, 0x55, 0x12, 0x03, 0x63, 0x01, 0x16, 0xF7 }));
    const auto strymonDump = [] (int product, int number, const juce::String& name, juce::uint8 opcode)
    {
        Bytes b (650, 0);
        const juce::uint8 head[] = { 0xF0, 0x00, 0x01, 0x55, 0x12, (juce::uint8) product, opcode, (juce::uint8) (number >> 7), (juce::uint8) (number & 0x7F) };
        std::copy (std::begin (head), std::end (head), b.begin());
        for (int i = 0; i < 16; ++i)
            b[(size_t) (632 + i)] = (juce::uint8) (i < name.length() ? name[i] : ' ');
        b[648] = 0x55;
        b[649] = 0xF7;
        return b;
    };
    {
        const auto d = strymonDump (2, 5, "CALL THE COPS", 0x62);
        const auto dump = namesync::strymon::parsePreset (d.data(), (int) d.size(), 2);
        CHECK (dump && dump->number == 5 && dump->name == "CALL THE COPS" && d[647] == ' ' && d[648] == 0x55);
        const auto fast = strymonDump (2, 5, "FAST", 0x67);
        CHECK (namesync::strymon::parsePreset (fast.data(), (int) fast.size(), 2)->name == "FAST");
        CHECK (! namesync::strymon::parsePreset (d.data(), (int) d.size(), 1));                     // another pedal
        CHECK (! namesync::strymon::parsePreset (d.data(), 649, 2));                                // wrong length
        const Bytes nak { 0xF0, 0, 0x01, 0x55, 0x12, 0x02, 0x47, 0xF7 };
        CHECK (namesync::strymon::isNak (nak.data(), (int) nak.size(), 2) && ! namesync::strymon::isNak (d.data(), (int) d.size(), 2));
    }

    // The whitelist: every builder passes; writes and anything else don't.
    CHECK (namesync::isAllowedRequest (namesync::roland::requestData (0x4F, 0x7F, 0, 16))
           && namesync::isAllowedRequest (namesync::fractal::querySceneName (0x14, namesync::fractal::current))
           && namesync::isAllowedRequest (namesync::strymon::requestPreset (1, 199)));
    {
        auto write = raw (namesync::roland::requestData (0x4F, 0x10, 0, 16));
        write[7] = 0x12;   // DT1
        CHECK (! namesync::isAllowedRequest (msgOf (write)));
        CHECK (! namesync::isAllowedRequest (msgOf (dt1 (0x10, 0x4F, 0, ascii16 ("X"), true))));
        auto badSum = raw (namesync::roland::requestData (0x4F, 0x10, 0, 16));
        badSum[16] ^= 1;
        CHECK (! namesync::isAllowedRequest (msgOf (badSum)));
        Bytes setScene { 0xF0, 0, 0x01, 0x74, 0x10, 0x0C, 0x02 };   // SET SCENE 3
        setScene.push_back (namesync::fractal::checksum (setScene.data(), (int) setScene.size()));
        setScene.push_back (0xF7);
        CHECK (! namesync::isAllowedRequest (msgOf (setScene)));
        for (juce::uint8 op : { (juce::uint8) 0x62, (juce::uint8) 0x64, (juce::uint8) 0x65, (juce::uint8) 0x1B, (juce::uint8) 0x7A })
        {
            auto req = raw (namesync::strymon::requestPreset (2, 0));
            req[6] = op;
            CHECK (! namesync::isAllowedRequest (msgOf (req)));
        }
        CHECK (! namesync::isAllowedRequest (msgOf (strymonDump (2, 0, "X", 0x62))));
        CHECK (! namesync::isAllowedRequest (juce::MidiMessage::programChange (1, 0))
               && ! namesync::isAllowedRequest (juce::MidiMessage::controllerEvent (1, 0, 0)));
    }

    // A simulated unit: runs a Reader to the end, recording every request it sends.
    using Device = std::function<std::vector<Bytes> (const Bytes&)>;
    const auto runReader = [&] (namesync::Reader& reader, const Device& device, std::vector<Bytes>& sent)
    {
        for (int guard = 0; ! reader.finished() && guard < 5000; ++guard)
        {
            const auto request = raw (reader.current());
            sent.push_back (request);
            bool answered = false;
            for (const auto& reply : device (request))
                answered = reader.accept (reply.data(), (int) reply.size()) || answered;
            if (! answered)
                reader.timedOut();
        }
    };
    // Every request any reader sends is whitelisted, and Roland ones are RQ1 (byte 7 = 11H, never 12H).
    const auto allReadOnly = [] (const std::vector<Bytes>& sent)
    {
        for (const auto& one : sent)
            if (! namesync::isAllowedRequest (juce::MidiMessage (one.data(), (int) one.size())) || (one[1] == 0x41 && one[7] != 0x11))
                return false;
        return ! sent.empty();
    };

    // GT-1000 that only answers the broadcast device ID; the PROGRAM MAP comes back in two pieces. BANK1 PC#n -> U patch n-1,
    // BANK2 PC#n -> 128 + n-1 (250+ are preset patches), BANK3-4 -> U01-1.
    {
        const Device gt = [&] (const Bytes& rq) -> std::vector<Bytes>
        {
            if (rq[2] != 0x7F)
                return {};
            const auto address = namesync::roland::linear (rq[8], rq[9], rq[10], rq[11]);
            const auto size = namesync::roland::linear (rq[12], rq[13], rq[14], rq[15]);
            if (address >= namesync::roland::gt1000PatchNameAddress (0))
            {
                const auto patch = (address - namesync::roland::gt1000PatchNameAddress (0)) >> 14;
                return { dt1 (0x10, 0x4F, address, ascii16 (patch % 3 == 2 ? juce::String() : "Patch " + juce::String (patch)), true) };
            }
            Bytes data;
            for (int at = address; at < address + size; at += 4)
            {
                const auto offset = at - namesync::roland::gt1000ProgramMapAddress (0, 0);
                const auto bank = offset / 512, pc = (offset % 512) / 4;
                const auto value = bank < 2 ? bank * 128 + pc : 0;
                for (int shift : { 12, 8, 4, 0 })
                    data.push_back ((juce::uint8) ((value >> shift) & 0x0F));
            }
            const auto half = (int) data.size() / 2;
            return { dt1 (0x10, 0x4F, address, Bytes (data.begin(), data.begin() + half), true),
                     dt1 (0x10, 0x4F, address + half, Bytes (data.begin() + half, data.end()), true) };
        };
        namesync::Reader reader (*namesync::targetFor ("boss.gt-1000"), false);
        std::vector<Bytes> sent;
        runReader (reader, gt, sent);
        const auto& r = reader.results();
        CHECK (reader.finished() && ! reader.gaveUp() && reader.deviceId() == 0x7F && allReadOnly (sent));
        CHECK (sent.size() == 1 + 250 + 16 && sent[0][2] == 0x10 && sent[1][2] == 0x7F);
        CHECK (r.presetNames.size() == 250 && r.presetNames.at (0) == "Patch 0" && r.presetNames.at (2).isEmpty() && r.presetNames.at (249) == "Patch 249");
        CHECK (r.programMap.size() == 512 && r.programMap.at (0) == 0 && r.programMap.at (127) == 127 && r.programMap.at (128) == 128
               && r.programMap.at (128 + 127) == 255 && r.programMap.at (300) == 0);

        auto unit = templates::createUnit (*templates::find ("boss.gt-1000"));
        const auto applied = namesync::applyToCustomUnit (unit, *namesync::targetFor ("boss.gt-1000"), r, true);
        auto presets = unit.getChildWithProperty (IDs::name, "Presets");
        // Named user patches: 250 minus every third (patch % 3 == 2); the 8 BANK1 tiles and BANK2 PC#1 match existing tiles.
        int named = 0;
        for (int patch = 0; patch < 250; ++patch)
            named += patch % 3 == 2 ? 0 : 1;
        CHECK (applied.added + applied.renamed == named);
        CHECK (presets.getChild (0)[IDs::name].toString() == "Patch 0" && presets.getChild (0)[IDs::messages].toString() == "bank 0, CC 32=0, PC 1");
        CHECK (presets.getChild (2)[IDs::name].toString() == "BANK1 PC#3");   // U01-3 has no name: left alone
        bool found = false;
        for (auto tile : presets)
            if (tile[IDs::name].toString() == "Patch 249")
                found = tile[IDs::messages].toString() == "bank 1, CC 32=0, PC 122";
        CHECK (found);
        for (auto tile : presets)
            CHECK (cues::custom::parse (tile[IDs::messages].toString(), 1).ok());
        // Again: everything matches, nothing is added.
        CHECK (namesync::applyToCustomUnit (unit, *namesync::targetFor ("boss.gt-1000"), r, true).added == 0);
    }

    // Axe-Fx III Mk I: answers presets 0-511 only; then the loaded preset's scene names.
    {
        const Device axeDevice = [&] (const Bytes& rq) -> std::vector<Bytes>
        {
            if (rq[5] == 0x0D)
            {
                const auto n = rq[6] | (rq[7] << 7);
                if (n >= 512)
                    return {};
                return { fractalReply (0x10, false, n, n % 2 == 0 ? "Tone " + juce::String (n) : juce::String ("<EMPTY>"), true) };
            }
            return { fractalReply (0x10, true, rq[6], "Scene " + juce::String (rq[6] + 1) + "!", true) };
        };
        namesync::Reader reader (*namesync::targetFor ("fractal.axe-fx-3"), true);
        std::vector<Bytes> sent;
        runReader (reader, axeDevice, sent);
        const auto& r = reader.results();
        CHECK (reader.finished() && ! reader.gaveUp() && reader.presetsEndedAt() == 512 && allReadOnly (sent));
        CHECK (sent.size() == 512 + 5 + 8 && r.presetNames.size() == 512 && r.hasSceneNames && r.sceneNames[7] == "Scene 8!");
        CHECK (r.numberMismatches == 0 && r.checksumMismatches == 0);

        auto unit = templates::createUnit (*templates::find ("fractal.axe-fx-3"));
        const auto applied = namesync::applyToCustomUnit (unit, *namesync::targetFor ("fractal.axe-fx-3"), r, true);
        auto presets = unit.getChildWithProperty (IDs::name, "Presets");
        CHECK (applied.added + applied.renamed == 256);
        CHECK (presets.getChild (0)[IDs::name].toString() == "Tone 0" && presets.getChild (1)[IDs::name].toString() == "Preset 001");
        CHECK (presets.getChild (8)[IDs::name].toString() == "Tone 128");   // the template's "Preset 128" tile (bank 1, PC 0)
        bool found = false;
        for (auto tile : presets)
            if (tile[IDs::name].toString() == "Tone 300")
                found = tile[IDs::messages].toString() == "bank 2, PC 44";
        CHECK (found);
    }

    // VP4: Program Change only.
    {
        namesync::Results r;
        r.presetNames[5] = "Wet";
        auto unit = templates::createUnit (*templates::find ("fractal.vp4"));
        CHECK (namesync::applyToCustomUnit (unit, *namesync::targetFor ("fractal.vp4"), r, true).renamed == 1);
        CHECK (unit.getChildWithProperty (IDs::name, "Presets").getChild (5)[IDs::name].toString() == "Wet");
        CHECK (namesync::unitLabel (*namesync::targetFor ("fractal.vp4"), 5) == "B2");
    }

    // Mobius: a NAK for preset 3, then a page gets ModPreset nodes.
    {
        const Device mobius = [&] (const Bytes& rq) -> std::vector<Bytes>
        {
            const auto n = (rq[7] << 7) | rq[8];
            if (n == 3)
                return { Bytes { 0xF0, 0, 0x01, 0x55, 0x12, 0x02, 0x47, 0xF7 } };
            return { strymonDump (2, n, n == 5 ? juce::String ("CALL THE COPS") : "MOD " + juce::String (n), 0x62) };
        };
        namesync::Reader reader (*namesync::targetFor ("strymon.mobius"), false);
        std::vector<Bytes> sent;
        runReader (reader, mobius, sent);
        const auto& r = reader.results();
        CHECK (reader.finished() && sent.size() == 200 && allReadOnly (sent) && r.naks == 1 && r.presetNames.size() == 199
               && r.presetNames.at (5) == "CALL THE COPS" && reader.timeoutMs() >= 1000);

        auto root = state::createDefault();
        const auto applied = namesync::applyToModeller (root, *namesync::targetFor ("strymon.mobius"), r, true, true);
        auto m = state::modeller (root, "strymon.mobius");
        int count = 0;
        juce::ValueTree five;
        for (auto c : m)
            if (c.hasType (IDs::ModPreset))
            {
                ++count;
                if ((int) c[IDs::presetIndex] == 5)
                    five = c;
            }
        CHECK (applied.added == 199 && count == 199 && five.isValid() && five[IDs::name].toString() == "CALL THE COPS"
               && (int) five[IDs::setlist] == -1);
        CHECK (namesync::unitLabel (*namesync::targetFor ("strymon.mobius"), 5) == "02B");
        // Applying again renames nothing new.
        CHECK (namesync::applyToModeller (root, *namesync::targetFor ("strymon.mobius"), r, true, false).added == 0);
    }

    // A Boss 500 pedal that never answers: the read stops after 5 + 1 (broadcast retry) requests, nothing applied.
    {
        namesync::Reader reader (*namesync::targetFor ("boss.dd-500"), false);
        std::vector<Bytes> sent;
        runReader (reader, [] (const Bytes&) { return std::vector<Bytes> {}; }, sent);
        CHECK (reader.gaveUp() && ! reader.anyAnswer() && sent.size() == 6 && allReadOnly (sent));
        auto root = state::createDefault();
        CHECK (namesync::applyToModeller (root, *namesync::targetFor ("boss.dd-500"), reader.results(), true, true).added == 0);
        CHECK (namesync::unitLabel (*namesync::targetFor ("boss.dd-500"), 296) == "99C");
    }

    // A late Fractal answer (after its request timed out) fills in its own preset, not the current one.
    {
        namesync::Reader reader (*namesync::targetFor ("fractal.fm9"), false);
        reader.timedOut();   // preset 0: no answer in time
        const auto late = fractalReply (0x12, false, 0, "Late", true);
        CHECK (! reader.accept (late.data(), (int) late.size()) && reader.results().presetNames.at (0) == "Late");
        const auto onTime = fractalReply (0x12, false, 1, "On time", true);
        CHECK (reader.accept (onTime.data(), (int) onTime.size()) && reader.results().presetNames.at (1) == "On time");
    }
    CHECK (namesync::isPlaceholderName ("  ") && namesync::isPlaceholderName ("<EMPTY>") && ! namesync::isPlaceholderName ("Lead"));
}
