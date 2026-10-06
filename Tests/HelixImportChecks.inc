// Import from HX Edit: synthetic .hlx / .hls / .hlb files (included in CueModelTests.cpp, inside the modellers block).
{
    auto nthOfTypeTest = [] (const juce::ValueTree& parent, const juce::Identifier& type, int n)
    {
        for (auto c : parent)
            if (c.hasType (type) && n-- == 0)
                return c;
        return juce::ValueTree();
    };
    // A preset object as HX Edit writes it inside a setlist: meta.name, tone.snapshotN { @name, @ledcolor }, blocks in dsp0.
    auto presetJson = [] (const juce::String& name, int snapshots, bool blocks, int firstColour)
    {
        juce::String js = "{\"meta\":{\"name\":" + juce::JSON::toString (name) + "},\"tone\":{";
        js << "\"dsp0\":{\"inputA\":{}" << (blocks ? ",\"block0\":{\"@model\":\"HD2_DistScream808\"}" : "") << "},";
        for (int i = 0; i < snapshots; ++i)
            js << "\"snapshot" << i << "\":{\"@name\":\"SNAP " << (i + 1) << "\",\"@ledcolor\":" << (i == 0 ? firstColour : i % 12)
              << ",\"@valid\":true},";
        js << "\"global\":{\"@current_snapshot\":0}}}";
        return js;
    };
    auto setlistArray = [&presetJson] (int count, int snapshots)
    {
        juce::StringArray items;
        for (int i = 0; i < count; ++i)
            items.add (i == 3 ? presetJson ("New Preset", snapshots, false, 0)
                              : presetJson ("Tone " + juce::String (i), snapshots, true, i == 0 ? 2 : 0));
        return "[" + items.joinIntoString (",") + "]";
    };
    // The .hls / .hlb wrapper: encoded_data = base64 (zlib (JSON)).
    auto wrap = [] (const juce::String& name, const juce::String& payload)
    {
        juce::MemoryOutputStream zipped;
        {
            juce::GZIPCompressorOutputStream z (zipped, 9);   // default window bits: zlib format, as HX Edit writes
            z.writeText (payload, false, false, nullptr);
            z.flush();
        }
        return "{\"meta\":{\"name\":" + juce::JSON::toString (name) + "},\"compression\":{\"decompressed_size\":"
               + juce::String ((int) payload.getNumBytesAsUTF8()) + "},\"encoded_data\":\""
               + juce::Base64::toBase64 (zipped.getData(), zipped.getDataSize()) + "\"}";
    };

    // .hlx: one preset, plain JSON.
    const auto hlx = ::helix::readJson ("{\"version\":6,\"schema\":\"L6Preset\",\"data\":" + presetJson ("Lead Boost", 8, true, 8) + "}");
    CHECK (hlx.ok() && hlx.kind == "preset" && hlx.setlists.size() == 1 && hlx.setlists[0].presets.size() == 1);
    CHECK (hlx.setlists[0].presets[0].name == "Lead Boost" && hlx.setlists[0].presets[0].snapshots.size() == 8
           && hlx.setlists[0].presets[0].snapshots[0].ledColour == 8 && ! hlx.setlists[0].presets[0].empty);

    // .hls: a setlist of 128, the name from the wrapper.
    const auto hls = ::helix::readJson (wrap ("Gig 2026", setlistArray (128, 8)));
    CHECK (hls.ok() && hls.kind == "setlist" && hls.setlists.size() == 1 && hls.setlists[0].name == "Gig 2026");
    CHECK (hls.setlists[0].presets.size() == 128 && hls.setlists[0].presets[5].slot == 5 && hls.setlists[0].presets[5].name == "Tone 5");
    CHECK (hls.setlists[0].presets[3].empty && ! hls.setlists[0].presets[4].empty);
    CHECK (hls.setlists[0].presets[0].snapshots[1].name == "SNAP 2" && hls.setlists[0].presets[0].snapshots[0].ledColour == 2);

    // Colours: 2 Red, 8 Blue, 0 Auto / unknown -> none (palette), 11 Off -> grey.
    CHECK (::helix::snapshotColour (2) == juce::Colour (0xffe5484d) && ::helix::snapshotColour (8) == juce::Colour (0xff0090ff));
    CHECK (::helix::snapshotColour (0).isTransparent() && ::helix::snapshotColour (-1).isTransparent()
           && ::helix::snapshotColour (12).isTransparent() && ! ::helix::snapshotColour (11).isTransparent());

    // .hlb: a bundle of setlists, each with its name.
    const auto bundlePayload = "{\"setlists\":[{\"meta\":{\"name\":\"USER 1\"},\"presets\":" + setlistArray (128, 8) + "},"
                               "{\"meta\":{\"name\":\"USER 2\"},\"presets\":[null," + presetJson ("Second", 8, true, 6) + "]}]}";
    const auto hlb = ::helix::readJson (wrap ("Bundle", bundlePayload));
    CHECK (hlb.ok() && hlb.kind == "bundle" && hlb.setlists.size() == 2 && hlb.setlists[1].name == "USER 2");
    CHECK (hlb.setlists[1].presets.size() == 1 && hlb.setlists[1].presets[0].slot == 1 && hlb.setlists[1].presets[0].name == "Second");

    // Which pages.
    CHECK (::helix::supports (*find ("line6.helix-floor")) && ::helix::supports (*find ("line6.hx-stomp"))
           && ::helix::supports (*find ("line6.hx-effects")) && ! ::helix::supports (*find ("line6.pod-go"))
           && ! ::helix::supports (*find ("line6.helix-stadium")) && ! ::helix::supports (*find ("fractal.ax8")));

    // apply on a Helix Floor: USER 1 (CC#32 = 2), the empty one skipped, other setlists and footswitch names kept.
    {
        auto root = state::createDefault();
        auto m = state::modeller (root, "line6.helix-floor");
        const auto& floor = *find ("line6.helix-floor");
        m.addChild (state::createModPreset (floor.id, "Keep me", 3, 0, juce::Colours::red), -1, nullptr);   // USER 2
        int switchesBefore = 0;
        for (auto c : m) switchesBefore += c.hasType (IDs::ModSwitch) ? 1 : 0;

        CHECK (::helix::apply (m, floor, hls, 0, 2, true) == 127);
        int user1 = 0, user2 = 0, switchesAfter = 0;
        juce::ValueTree first, fifth;
        for (auto c : m)
        {
            switchesAfter += c.hasType (IDs::ModSwitch) ? 1 : 0;
            if (! c.hasType (IDs::ModPreset)) continue;
            if ((int) c[IDs::setlist] == 2) { if (user1 == 0) first = c; if ((int) c[IDs::presetIndex] == 5) fifth = c; ++user1; }
            if ((int) c[IDs::setlist] == 3) ++user2;
        }
        CHECK (user1 == 127 && user2 == 1 && switchesAfter == switchesBefore && switchesBefore > 0);
        CHECK (first[IDs::name].toString() == "Tone 0" && (int) first[IDs::presetIndex] == 0
               && state::colourOf (first) == juce::Colour (0xffe5484d));                      // snapshot 1 red
        CHECK (fifth[IDs::name].toString() == "Tone 5");
        int scenes = 0;
        for (auto c : fifth) scenes += c.hasType (IDs::Scene) ? 1 : 0;
        CHECK (scenes == 8 && nthOfTypeTest (fifth, IDs::Scene, 1)[IDs::name].toString() == "SNAP 2");
        CHECK (state::colourOf (nthOfTypeTest (fifth, IDs::Scene, 1)) == ::helix::snapshotColour (1));   // @ledcolor 1 White
        for (auto c : m)
            if (c.hasType (IDs::ModPreset) && (int) c[IDs::setlist] == 2)
                CHECK ((int) c[IDs::presetIndex] != 3);                                       // "New Preset" skipped

        // Again without skipping: 128, and the old ones are gone, not doubled.
        CHECK (::helix::apply (m, floor, hls, 0, 2, false) == 128);
        user1 = 0;
        for (auto c : m) user1 += (c.hasType (IDs::ModPreset) && (int) c[IDs::setlist] == 2) ? 1 : 0;
        CHECK (user1 == 128);

        // A single .hlx replaces only its slot.
        auto one = hlx;
        one.setlists[0].presets[0].slot = 7;
        CHECK (::helix::apply (m, floor, one, 0, 2, true) == 1);
        user1 = 0;
        juce::ValueTree seventh;
        for (auto c : m)
            if (c.hasType (IDs::ModPreset) && (int) c[IDs::setlist] == 2)
            {
                ++user1;
                if ((int) c[IDs::presetIndex] == 7) seventh = c;
            }
        CHECK (user1 == 128 && seventh[IDs::name].toString() == "Lead Boost"
               && state::colourOf (seventh) == juce::Colour (0xff0090ff));

        // Bundle setlist 2 into USER 2: replaces "Keep me".
        CHECK (::helix::apply (m, floor, hlb, 1, 3, true) == 1);
        bool kept = false;
        for (auto c : m)
            kept = kept || c[IDs::name].toString() == "Keep me";
        CHECK (! kept);

        // Out-of-range setlists and indices write nothing.
        CHECK (::helix::apply (m, floor, hlb, 5, 2, true) == 0 && ::helix::apply (m, floor, hlb, 0, 99, true) == 0);
    }

    // HX Stomp: no setlists, 126 presets, 3 snapshots.
    {
        auto root = state::createDefault();
        auto m = state::modeller (root, "line6.hx-stomp");
        const auto& stompProfile = *find ("line6.hx-stomp");
        CHECK (::helix::apply (m, stompProfile, hls, 0, 5, false) == 126);                                  // slots 126, 127 dropped
        int presets = 0;
        juce::ValueTree p0;
        for (auto c : m)
            if (c.hasType (IDs::ModPreset))
            {
                if (presets++ == 0) p0 = c;
                CHECK ((int) c[IDs::setlist] == -1);
            }
        int scenes = 0;
        for (auto c : p0) scenes += c.hasType (IDs::Scene) ? 1 : 0;
        CHECK (presets == 126 && scenes == 3 && nthOfTypeTest (p0, IDs::Scene, 2)[IDs::name].toString() == "SNAP 3");
    }

    // Everything skipped: the page keeps what it had.
    {
        auto root = state::createDefault();
        auto m = state::modeller (root, "line6.helix-floor");
        const auto before = m.createCopy();
        const auto empties = ::helix::readJson (wrap ("Empty", "[" + presetJson ("New Preset", 8, false, 0) + "]"));
        CHECK (empties.ok() && empties.setlists[0].presets[0].empty);
        CHECK (::helix::apply (m, *find ("line6.helix-floor"), empties, 0, 2, true) == 0 && m.isEquivalentTo (before));
    }

    // Garbage never crashes and says why.
    for (const auto& bad : { juce::String(), juce::String ("not json"), juce::String ("[1,2,3]"), juce::String ("{}"),
                             juce::String ("{\"encoded_data\":\"!!!not base64!!!\"}"),
                             juce::String ("{\"encoded_data\":\"") + juce::Base64::toBase64 ("plain text, not zlib", 20) + "\"}",
                             juce::String ("{\"encoded_data\":42}"), juce::String ("{\"data\":{\"meta\":\"x\"}}"),
                             juce::String ("{\"setlists\":[1,null,\"x\"]}") })
    {
        const auto r = ::helix::readJson (bad);
        CHECK (! r.ok() && r.error.isNotEmpty());
        auto root = state::createDefault();
        CHECK (::helix::apply (state::modeller (root, "line6.helix-floor"), *find ("line6.helix-floor"), r, 0, 2, true) == 0);
    }
    CHECK (! ::helix::readFile (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("no-such-file.hls")).ok());
}
