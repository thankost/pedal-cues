// Renders the images used by docs/GUIDE.md from the real UI components.
// Usage: DocShots <outputDirectory> [iconPngPath]

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"
#include "../Source/DeviceTemplates.h"
#include "../Source/Modellers.h"
#include "../Source/MovesPanel.h"

using namespace theme;

namespace
{
constexpr float scale = 2.0f;

void save (const juce::Image& img, const juce::File& file)
{
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat png;
    if (out.openedOk() && png.writeImageToStream (img, out))
        std::cout << "wrote " << file.getFullPathName() << std::endl;
    else
        std::cerr << "FAILED " << file.getFullPathName() << std::endl;
}

void fillDemoState (juce::ValueTree root)
{
    auto qc = root.getChildWithName (IDs::QC);
    qc.removeAllChildren (nullptr);

    struct Demo { const char* name; int bank, slot, colour; const char* scenes[8]; };
    const Demo demos[] = {
        { "Clean Rig",     1, 0, 4, { "Intro", "Verse", "Pre", "Chorus", "Bridge", "Solo", "Breakdown", "Outro" } },
        { "Plexi Crunch",  1, 1, 1, { "Riff", "Verse", "Build", "Chorus", "Half-time", "Lead", "Stop", "Ring out" } },
        { "Lead Rig",      1, 2, 0, { "Rhythm", "Solo", "Octave", "Harmony", "Dry", "Wet", "Feedback", "Tail" } },
        { "Ambient Pads",  2, 0, 6, { "Pad", "Swell", "Shimmer", "Clean", "Freeze", "Delay", "Verb", "Silence" } },
        { "Drop C Heavy",  2, 1, 2, { "Chug", "Verse", "Drop", "Chorus", "Breakdown", "Solo", "Gallop", "End" } },
    };

    const int sceneColours[8] = { 6, 4, 7, 1, 3, 0, 5, 9 };
    const char* stomps[8] = { "Drive", "Delay", "Reverb", "Chorus", "Boost", "Octaver", "Phaser", "Tremolo" };

    for (const auto& d : demos)
    {
        auto p = state::createPreset (d.name, 1, d.bank, d.slot, ui::paletteColour (d.colour));
        int s = 0, f = 0;
        for (auto child : p)
        {
            if (child.hasType (IDs::Scene) && s < 8)
            {
                child.setProperty (IDs::name, d.scenes[s], nullptr);
                child.setProperty (IDs::colour, ui::paletteColour (sceneColours[s]).toString(), nullptr);
                ++s;
            }
            else if (child.hasType (IDs::Stomp) && f < 8)
            {
                child.setProperty (IDs::name, stomps[f++], nullptr);
            }
        }
        qc.appendChild (p, nullptr);
    }

    root.setProperty (IDs::selectedPreset, 0, nullptr);
    root.setProperty (IDs::stompOn, true, nullptr);
    root.setProperty (IDs::sweepBeats, 4.0, nullptr);

    // Kemper: a few performances with named, coloured slots.
    {
        auto kemper = root.getChildWithName (IDs::Kemper);
        for (int i = kemper.getNumChildren(); --i >= 0;)
            if (kemper.getChild (i).hasType (IDs::Performance))
                kemper.removeChild (i, nullptr);
        struct Perf { const char* name; int number, colour; const char* slots[5]; };
        const Perf perfs[] = {
            { "Opener",      1, 4, { "Clean",  "Crunch", "Lead",   "Ambient", "Heavy" } },
            { "Midnight",    2, 1, { "Intro",  "Verse",  "Chorus", "Solo",    "Outro" } },
            { "Big Riff",    3, 0, { "Riff",   "Verse",  "Drop",   "Lead",    "Clean" } },
            { "Ballad",      4, 6, { "Clean",  "Swell",  "Chorus", "Solo",    "End" } },
        };
        const int slotColours[5] = { 4, 6, 1, 0, 9 };
        for (int i = 0; i < (int) std::size (perfs); ++i)
        {
            auto p = state::createPerformance (perfs[i].name, perfs[i].number, ui::paletteColour (perfs[i].colour));
            for (int s = 0; s < 5; ++s)
            {
                auto slot = ui::nthOfType (p, IDs::KemperSlot, s);
                slot.setProperty (IDs::name, perfs[i].slots[s], nullptr);
                slot.setProperty (IDs::colour, ui::paletteColour (slotColours[s]).toString(), nullptr);
            }
            kemper.addChild (p, i, nullptr);
        }
        const char* effects[8] = { "Green Scream", "Compressor", "Wah", "Chorus", "Boost", "Phaser", "Tap Delay", "Hall" };
        int e = 0;
        for (auto c : kemper)
            if (c.hasType (IDs::KemperEffect) && e < 8)
                c.setProperty (IDs::name, effects[e++], nullptr);
    }

    // My drawings: the default drawing saved as "Big Bend" (loaded on the Whammy pad), plus a slow swell
    // loaded on the expression pad.
    state::useMyDrawingsFile (juce::File::createTempFile (".xml"));   // leave the user's own drawings alone
    state::saveDrawing (state::myDrawings(), "Big Bend", cues::whammy::encodeDrawing (cues::whammy::defaultDrawing()));
    std::vector<float> swell ((size_t) cues::whammy::drawPoints);
    for (size_t i = 0; i < swell.size(); ++i)
        swell[i] = std::pow ((float) i / (float) (swell.size() - 1), 2.0f);
    state::saveDrawing (state::myDrawings(), "Slow Swell", cues::whammy::encodeDrawing (swell));
    root.setProperty (IDs::sweepDrawingName, "Big Bend", nullptr);
    root.setProperty (IDs::expDrawingName, "Slow Swell", nullptr);
    root.setProperty (IDs::expDrawing, cues::whammy::encodeDrawing (swell), nullptr);
    // A custom unit (beta): a made-up rig with example values, as a user would set it up.
    {
        auto unit = state::createCustomUnit ("My Rig");
        unit.removeAllChildren (nullptr);
        unit.setProperty (IDs::notes, "Example values, not a real device's chart.\n\nScenes: my device switches scenes with CC 34 "
                                      "(value 0 = scene 1). On the timeline, drop the preset first and the scene just after it.\n\n"
                                      "Effects use CC 50-53, 127 = on, 0 = off.", nullptr);
        struct T { const char* name; const char* messages; const char* note; int colour; };
        const std::pair<const char*, std::vector<T>> groups[] = {
            { "Presets", { { "Clean", "PC 0", "", 4 }, { "Crunch", "PC 1", "", 1 }, { "Lead", "PC 2", "", 0 }, { "Ambient", "PC 3", "", 6 } } },
            { "Scenes",  { { "Verse", "CC 34=0", "Scene 1 of the loaded preset", 5 }, { "Chorus", "CC 34=1", "", 1 },
                           { "Bridge", "CC 34=2", "", 6 }, { "Solo", "CC 34=3", "", 0 } } },
            { "Effects", { { "Drive on", "CC 50=127", "", 0 }, { "Drive off", "CC 50=0", "", 9 }, { "Delay on", "CC 51=127", "", 5 },
                           { "Tuner", "CC 53=127", "", 3 } } },
        };
        for (const auto& [groupName, items] : groups)
        {
            juce::ValueTree g (IDs::Group);
            g.setProperty (IDs::name, groupName, nullptr);
            for (const auto& t : items)
            {
                juce::ValueTree tile (IDs::CueTile);
                tile.setProperty (IDs::name, t.name, nullptr);
                tile.setProperty (IDs::messages, t.messages, nullptr);
                tile.setProperty (IDs::note, t.note, nullptr);
                tile.setProperty (IDs::colour, ui::paletteColour (t.colour).toString(), nullptr);
                g.appendChild (tile, nullptr);
            }
            unit.appendChild (g, nullptr);
        }
        state::addCustomUnit (root, unit);
    }
}

juce::Image snapshot (juce::Component& c)
{
    return c.createComponentSnapshot (c.getLocalBounds(), true, scale);
}

juce::Image drawIcon (int size)
{
    juce::Image img (juce::Image::ARGB, size, size, true);
    juce::Graphics g (img);
    drawAppIcon (g, { 0.0f, 0.0f, (float) size, (float) size }, true);
    return img;
}
} // namespace

//==============================================================================
// A DAW-style arrangement showing what dropped tiles turn into.
namespace
{
void drawItem (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, const juce::String& name,
               const std::vector<juce::Point<float>>& curve = {})
{
    g.setColour (c.withAlpha (0.35f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (c);
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.2f);
    g.fillRoundedRectangle (r.withHeight (16.0f), 4.0f);
    g.setColour (c.getPerceivedBrightness() > 0.6f ? juce::Colours::black : juce::Colours::white);
    g.setFont (font (11.0f, true));
    g.drawText (name, r.withHeight (16.0f).reduced (5.0f, 0.0f), juce::Justification::centredLeft, true);

    auto body = r.withTrimmedTop (18.0f).reduced (4.0f, 4.0f);
    if (curve.empty())
    {
        g.setColour (c.brighter (0.4f));
        g.fillRect (body.getX(), body.getCentreY() - 2.0f, 3.0f, 4.0f);
        return;
    }

    juce::Path p;
    auto lastY = body.getBottom() - curve.front().y * body.getHeight();
    p.startNewSubPath (body.getX(), lastY);
    for (auto pt : curve)
    {
        const auto x = body.getX() + pt.x * body.getWidth();
        const auto y = body.getBottom() - pt.y * body.getHeight();
        p.lineTo (x, lastY);
        p.lineTo (x, y);
        lastY = y;
    }
    p.lineTo (body.getRight(), lastY);
    g.setColour (c.brighter (0.5f));
    g.strokePath (p, juce::PathStrokeType (1.5f));
}

juce::Image drawTimeline (PedalCuesProcessor& proc)
{
    constexpr int w = 1120, h = 400;
    juce::Image img (juce::Image::ARGB, (int) (w * scale), (int) (h * scale), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));

    g.fillAll (juce::Colour (0xff1b1d22));

    const float headerW = 170.0f, rulerH = 30.0f, trackH = 86.0f, barW = 58.0f;
    const auto x0 = headerW;
    auto barX = [&] (float bar) { return x0 + (bar - 1.0f) * barW; };

    // Ruler.
    g.setColour (juce::Colour (0xff23262d));
    g.fillRect (0.0f, 0.0f, (float) w, rulerH);
    for (int bar = 1; bar <= 17; ++bar)
    {
        const auto x = barX ((float) bar);
        g.setColour (juce::Colour (0xff3a3f49));
        g.drawVerticalLine ((int) x, rulerH, (float) h);
        g.setColour (dim);
        g.setFont (font (11.0f, true));
        g.drawText (juce::String (bar), juce::Rectangle<float> (x + 4.0f, 0.0f, 30.0f, rulerH), juce::Justification::centredLeft);
    }

    const char* tracks[] = { "Backing Track", "Pedal Cues  (QC)", "Pedal Cues  (Whammy)" };
    for (int t = 0; t < 3; ++t)
    {
        const auto y = rulerH + 8.0f + (float) t * (trackH + 8.0f);
        drawCard (g, { 8.0f, y, headerW - 16.0f, trackH }, surfaceHi, 8.0f);
        g.setColour (text);
        g.setFont (font (12.5f, true));
        g.drawText (tracks[t], juce::Rectangle<float> (18.0f, y + 8.0f, headerW - 30.0f, 18.0f), juce::Justification::centredLeft);
        g.setColour (dim);
        g.setFont (font (11.0f));
        g.drawText (t == 0 ? "audio" : (t == 1 ? "MIDI > Quad Cortex" : "MIDI > interface > Whammy"), juce::Rectangle<float> (18.0f, y + 26.0f, headerW - 30.0f, 16.0f),
                    juce::Justification::centredLeft);
    }

    // Backing track waveform.
    {
        const auto y = rulerH + 8.0f;
        const juce::Rectangle<float> r (barX (1.0f), y, barW * 16.0f, trackH);
        g.setColour (juce::Colour (0xff2d5f8a).withAlpha (0.5f));
        g.fillRoundedRectangle (r, 4.0f);
        juce::Random rnd (7);
        g.setColour (juce::Colour (0xff7fb8e6));
        for (float x = r.getX() + 2.0f; x < r.getRight() - 2.0f; x += 2.0f)
        {
            const auto env = 0.35f + 0.6f * std::abs (std::sin ((x - r.getX()) / 140.0f));
            const auto a = env * (0.4f + 0.6f * rnd.nextFloat()) * (trackH * 0.4f);
            g.drawVerticalLine ((int) x, r.getCentreY() - a, r.getCentreY() + a);
        }
    }

    const auto qcY = rulerH + 8.0f + trackH + 8.0f;
    const auto whY = qcY + trackH + 8.0f;
    auto item = [&] (float bar, float bars, float y) { return juce::Rectangle<float> (barX (bar) + 1.0f, y + 6.0f, bars * barW - 2.0f, trackH - 12.0f); };

    drawItem (g, item (1.0f, 1.9f, qcY), ui::paletteColour (4), "QC Clean Rig");
    drawItem (g, item (3.0f, 1.9f, qcY), ui::paletteColour (4), "Verse");
    drawItem (g, item (9.0f, 1.9f, qcY), ui::paletteColour (1), "Chorus");
    drawItem (g, item (13.0f, 1.9f, qcY), ui::paletteColour (0), "Solo");

    auto sweepCurve = [&] (cues::whammy::Shape shape, double beats)
    {
        const auto cue = cues::whammy::sweep (2, shape, beats, 1.0, false);
        std::vector<juce::Point<float>> pts;
        for (const auto& [beat, msg] : cue.events)
            pts.push_back ({ (float) (beat / cue.lengthBeats), (float) msg.getControllerValue() / 127.0f });
        return pts;
    };

    drawItem (g, item (9.0f, 1.9f, whY), whammyRed, "Whammy Oct Up");
    drawItem (g, item (11.0f, 2.0f, whY), juce::Colour (0xffff5a6a), "Whammy Rise & Fall 2 bars", sweepCurve (cues::whammy::Shape::swell, 8.0));
    drawItem (g, item (15.0f, 2.0f, whY), juce::Colour (0xffff5a6a), "Whammy Bend to Bar", sweepCurve (cues::whammy::Shape::bendToBar, 8.0));

    // A scene tile being dragged onto bar 15, with a drop marker.
    Tile tile (proc, Tile::Look::footswitch);
    tile.title = "Pre";
    tile.subtitle = "Scene C";
    tile.badge = "C";
    tile.colour = ui::paletteColour (7);
    tile.setSize (150, 150);
    const auto tileImg = tile.createComponentSnapshot (tile.getLocalBounds(), true, scale);

    const auto dropX = barX (5.0f);
    g.setColour (accent);
    g.drawLine (dropX, rulerH, dropX, (float) h, 2.0f);
    g.fillRect (juce::Rectangle<float> (dropX - 5.0f, rulerH - 6.0f, 10.0f, 6.0f));
    g.setColour (accent.withAlpha (0.25f));
    g.fillRoundedRectangle (item (5.0f, 1.9f, qcY), 4.0f);
    g.setColour (accent);
    g.drawRoundedRectangle (item (5.0f, 1.9f, qcY), 4.0f, 1.5f);

    g.setOpacity (0.92f);
    g.drawImage (tileImg, juce::Rectangle<float> (dropX + 40.0f, qcY + 34.0f, 128.0f, 128.0f));

    // Cursor.
    juce::Path cursor;
    const auto cx = dropX + 44.0f, cy = qcY + 42.0f;
    cursor.startNewSubPath (cx, cy);
    cursor.lineTo (cx, cy + 20.0f);
    cursor.lineTo (cx + 5.0f, cy + 15.0f);
    cursor.lineTo (cx + 9.0f, cy + 23.0f);
    cursor.lineTo (cx + 12.0f, cy + 21.0f);
    cursor.lineTo (cx + 8.0f, cy + 14.0f);
    cursor.lineTo (cx + 14.0f, cy + 14.0f);
    cursor.closeSubPath();
    g.setOpacity (1.0f);
    g.setColour (juce::Colours::white);
    g.fillPath (cursor);
    g.setColour (juce::Colours::black);
    g.strokePath (cursor, juce::PathStrokeType (1.0f));

    return img;
}
} // namespace

//==============================================================================
// Signal/MIDI routing diagram.
namespace
{
void box (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, const juce::String& title, const juce::String& sub)
{
    juce::DropShadow (juce::Colours::black.withAlpha (0.5f), 18, { 0, 6 }).drawForRectangle (g, r.toNearestInt());
    g.setColour (surfaceHi);
    g.fillRoundedRectangle (r, 14.0f);
    g.setColour (c);
    g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.5f);
    g.fillRoundedRectangle (r.getX() + 16.0f, r.getY() + 14.0f, 4.0f, 34.0f, 2.0f);

    g.setColour (text);
    g.setFont (font (17.0f, true));
    g.drawText (title, r.reduced (30.0f, 12.0f).removeFromTop (26.0f), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (font (12.5f));
    g.drawFittedText (sub, r.reduced (30.0f, 12.0f).withTrimmedTop (28.0f).toNearestInt(), juce::Justification::topLeft, 3);
}

void arrow (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b, juce::Colour c, const juce::String& label, bool dashed)
{
    juce::Path p;
    p.startNewSubPath (a);
    p.lineTo (b);

    juce::Path stroke;
    juce::PathStrokeType st (2.5f);
    if (dashed)
    {
        const float dashes[] = { 8.0f, 6.0f };
        st.createDashedStroke (stroke, p, dashes, 2);
    }
    else
        st.createStrokedPath (stroke, p);

    g.setColour (c);
    g.fillPath (stroke);

    juce::Path head;
    head.addArrow ({ b - (b - a) / (b.getDistanceFrom (a)) * 14.0f, b }, 0.0f, 14.0f, 14.0f);
    g.fillPath (head);

    const auto mid = (a + b) * 0.5f;
    const juce::Font f (font (12.0f, true));
    const auto w = juce::GlyphArrangement::getStringWidth (f, label) + 18.0f;
    const auto pill = juce::Rectangle<float> (w, 24.0f).withCentre (mid);
    g.setColour (background);
    g.fillRoundedRectangle (pill, 12.0f);
    g.setColour (c);
    g.drawRoundedRectangle (pill, 12.0f, 1.0f);
    g.setFont (f);
    g.drawText (label, pill, juce::Justification::centred);
}

juce::Image drawRouting()
{
    constexpr int w = 1120, h = 900;
    juce::Image img (juce::Image::ARGB, (int) (w * scale), (int) (h * scale), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.fillAll (background);

    auto heading = [&g] (float y, const juce::String& title, const juce::String& sub)
    {
        g.setColour (text);
        g.setFont (font (13.0f, true));
        g.drawText (title, juce::Rectangle<float> (40.0f, y, 1040.0f, 20.0f), juce::Justification::centredLeft);
        g.setColour (dim);
        g.setFont (font (12.0f));
        g.drawText (sub, juce::Rectangle<float> (40.0f, y + 20.0f, 1040.0f, 18.0f), juce::Justification::centredLeft);
    };
    auto divider = [&g] (float y) { g.setColour (outline); g.fillRect (40.0f, y, 1040.0f, 1.0f); };
    const auto grey = juce::Colour (0xff9aa0ac);
    constexpr float bh = 76.0f;

    // Four boxes in a row, 220 wide with 53 px gaps.
    auto col = [] (int i) { return 40.0f + (float) i * 273.3f; };
    auto rect = [&] (int i, float y) { return juce::Rectangle<float> (col (i), y, 220.0f, bh); };
    auto link = [&] (juce::Rectangle<float> a, juce::Rectangle<float> b, juce::Colour c, const juce::String& label)
    {
        arrow (g, { a.getRight(), a.getCentreY() }, { b.getX(), b.getCentreY() }, c, label, false);
    };

    // 1. Daisy chain.
    heading (20.0f, "DAISY CHAIN  (what we use)", "Both cue tracks send to the same interface MIDI Out. The pedals share one cable; their channels keep the cues apart.");
    {
        const auto y = 66.0f;
        const auto t = rect (0, y), i = rect (1, y), q = rect (2, y), wh = rect (3, y);
        box (g, t, accent, "QC + Whammy Cues", "Both output = interface MIDI Out");
        box (g, i, grey, "Audio interface", "MIDI Out");
        box (g, q, qcBlue, "Quad Cortex", "Ch 1. MIDI In, Thru on");
        box (g, wh, whammyRed, "Whammy V", "Channel 2");
        link (t, i, accent, "to interface");
        link (i, q, grey, "MIDI cable");
        link (q, wh, qcBlue, "Thru");
    }
    divider (166.0f);

    // 2. Separate MIDI cables.
    heading (182.0f, "SEPARATE MIDI CABLES", "Each cue track sends to its own MIDI Out (an interface with two MIDI Outs, or two USB MIDI interfaces).");
    {
        const auto y1 = 228.0f, y2 = y1 + bh + 16.0f;
        const auto t1 = rect (0, y1), o1 = rect (1, y1), q = rect (2, y1);
        const auto t2 = rect (0, y2), o2 = rect (1, y2), wh = rect (2, y2);
        box (g, t1, accent, "QC Cues track", "Output = MIDI Out 1");
        box (g, o1, grey, "Interface", "MIDI Out 1");
        box (g, q, qcBlue, "Quad Cortex", "Channel 1");
        box (g, t2, accent, "Whammy Cues track", "Output = MIDI Out 2");
        box (g, o2, grey, "Interface", "MIDI Out 2");
        box (g, wh, whammyRed, "Whammy V", "Channel 2");
        link (t1, o1, accent, "to interface");
        link (o1, q, grey, "MIDI cable");
        link (t2, o2, accent, "to interface");
        link (o2, wh, grey, "MIDI cable");
    }
    divider (430.0f);

    // 3. QC over USB + interface.
    heading (446.0f, "QC OVER USB + INTERFACE", "The QC Cues track sends over USB, the Whammy Cues track to the interface's MIDI Out.");
    {
        const auto y1 = 492.0f, y2 = y1 + bh + 16.0f;
        const auto t1 = rect (0, y1), q = rect (1, y1);
        const auto t2 = rect (0, y2), o = rect (1, y2), wh = rect (2, y2);
        box (g, t1, accent, "QC Cues track", "Output = Quad Cortex");
        box (g, q, qcBlue, "Quad Cortex", "Channel 1");
        box (g, t2, accent, "Whammy Cues track", "Output = interface");
        box (g, o, grey, "Audio interface", "MIDI Out");
        box (g, wh, whammyRed, "Whammy V", "Channel 2");
        link (t1, q, accent, "QC USB");
        link (t2, o, accent, "to interface");
        link (o, wh, grey, "MIDI cable");
    }
    divider (694.0f);

    // 4. Does not work.
    heading (710.0f, "DOES NOT WORK:  QC OVER USB, WHAMMY ON THE QC'S THRU",
             "Known Quad Cortex limitation: MIDI Thru only passes on 5-pin MIDI, not MIDI received over USB.");
    {
        const auto y = 756.0f;
        const auto t = rect (0, y), q = rect (1, y), wh = rect (2, y);
        box (g, t, grey, "QC + Whammy Cues", "Output = Quad Cortex (USB)");
        box (g, q, grey, "Quad Cortex", "USB, MIDI Thru on");
        box (g, wh, grey, "Whammy V", "Never changes");
        link (t, q, grey, "QC USB");
        arrow (g, { q.getRight(), q.getCentreY() }, { wh.getX(), wh.getCentreY() }, whammyRed, "no USB Thru", true);
    }

    return img;
}

// What a scene tile does in each "scenes & stomps act on" mode, when the QC is on another preset.
juce::Image drawPresetTarget()
{
    constexpr int w = 1120, h = 400;
    juce::Image img (juce::Image::ARGB, (int) (w * scale), (int) (h * scale), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.fillAll (background);

    auto heading = [&g] (float y, const juce::String& title, const juce::String& sub)
    {
        g.setColour (text);
        g.setFont (font (13.0f, true));
        g.drawText (title, juce::Rectangle<float> (40.0f, y, 1040.0f, 20.0f), juce::Justification::centredLeft);
        g.setColour (dim);
        g.setFont (font (12.0f));
        g.drawText (sub, juce::Rectangle<float> (40.0f, y + 20.0f, 1040.0f, 18.0f), juce::Justification::centredLeft);
    };
    const auto grey = juce::Colour (0xff9aa0ac);

    auto row = [&] (float y, const juce::String& title, const juce::String& sub, const juce::String& sends,
                    const juce::String& resultTitle, const juce::String& resultSub, juce::Colour resultColour)
    {
        heading (y, title, sub);
        const juce::Rectangle<float> before (40.0f, y + 50.0f, 250.0f, 96.0f);
        const juce::Rectangle<float> tile (415.0f, y + 50.0f, 250.0f, 96.0f);
        const juce::Rectangle<float> after (830.0f, y + 50.0f, 250.0f, 96.0f);
        box (g, before, grey, "QC is on 2H", "Drop C Heavy, scene A");
        box (g, tile, accent, "Click / play Scene B", "Tile from preset 1A Clean Rig");
        box (g, after, resultColour, resultTitle, resultSub);
        arrow (g, { before.getRight(), before.getCentreY() }, { tile.getX(), tile.getCentreY() }, grey, "then", true);
        arrow (g, { tile.getRight(), tile.getCentreY() }, { after.getX(), after.getCentreY() }, accent, sends, false);
    };

    row (20.0f, "LOAD 1A FIRST  (default)", "Scene and stomp tiles load their own preset, then switch. Works whatever preset the QC is on.",
         "PC 1A + CC#43", "QC on 1A, scene B", "Clean Rig > Verse", ledGreen);

    g.setColour (outline);
    g.fillRect (40.0f, 200.0f, 1040.0f, 1.0f);

    row (220.0f, "LOAD 1A FIRST OFF", "Tiles only switch the scene or footswitch on the preset already loaded. No reload, no audio gap.",
         "CC#43 only", "QC stays on 2H, scene B", "Drop C Heavy > Verse", qcBlue);

    return img;
}
} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    const auto outDir = argc > 1 ? juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])
                                 : juce::File::getCurrentWorkingDirectory().getChildFile ("docs/images");
    outDir.createDirectory();

    // Screenshots show the "up to date" badge without going online.
    update::Info upToDate;
    upToDate.status = update::Info::Status::upToDate;
    upToDate.latest = JucePlugin_VersionString;
    update::setOfflineResult (upToDate);

    PedalCuesProcessor proc;
    proc.state = state::createDefault();
    fillDemoState (proc.state);

    // Screenshots show the channels as confirmed (no reminder banner), except channel-banner.png and settings.png.
    const auto channelsWereConfirmed = state::getFlag (ui::channelsConfirmedFlag);
    state::setFlag (ui::channelsConfirmedFlag, false);
    {
        PedalCuesEditor editor (proc, false);
        editor.setSize (1120, 760);
        editor.refreshNow();
        editor.showPage (0);
        save (snapshot (editor), outDir.getChildFile ("channel-banner.png"));
    }
    state::setFlag (ui::channelsConfirmedFlag, true);

    {
        PedalCuesEditor editor (proc, false);
        editor.setSize (1120, 760);
        editor.refreshNow();

        editor.showPage (0);
        save (snapshot (editor), outDir.getChildFile ("quad-cortex.png"));

        // The preset search in use ("rig": Clean Rig and Lead Rig).
        {
            std::function<juce::Component* (juce::Component&)> find = [&find] (juce::Component& c) -> juce::Component*
            {
                if (c.getComponentID() == "qc.search")
                    return &c;
                for (auto* child : c.getChildren())
                    if (auto* f = find (*child))
                        return f;
                return nullptr;
            };
            if (auto* box = dynamic_cast<juce::TextEditor*> (find (editor)))
            {
                box->setText ("rig", false);
                box->onTextChange();   // the editor reports changes asynchronously; filter now
                save (snapshot (editor), outDir.getChildFile ("qc-search.png"));
                box->setText ({}, false);
                box->onTextChange();
            }
        }
        proc.state.setProperty (IDs::qcLooperView, true, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("qc-looper.png"));
        proc.state.setProperty (IDs::qcLooperView, false, nullptr);
        proc.state.setProperty (IDs::qcExpressionView, true, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("qc-expression.png"));
        proc.state.setProperty (IDs::expDraw, true, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("qc-expression-draw.png"));
        proc.state.setProperty (IDs::expDraw, false, nullptr);
        proc.state.setProperty (IDs::qcExpressionView, false, nullptr);
        editor.refreshNow();
        // QC Mini: the same page, footswitches and scenes on Pages I and II, plus the page tiles.
        proc.state.setProperty (IDs::ampUnit, state::qcMiniAmpUnit, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("qc-mini.png"));
        // Kemper: the same first tab with Kemper Profiler picked in its dropdown.
        proc.state.setProperty (IDs::ampUnit, 1, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("kemper.png"));
        proc.state.setProperty (IDs::kemperPedalsView, true, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("kemper-pedals.png"));
        proc.state.setProperty (IDs::kemperPedalsView, false, nullptr);

        // A custom unit (beta), with example values.
        proc.state.setProperty (IDs::ampUnit, state::customAmpUnit, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("custom-device.png"));
        proc.state.setProperty (IDs::cuExpressionView, true, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("custom-expression.png"));
        proc.state.setProperty (IDs::cuExpressionView, false, nullptr);
        editor.refreshNow();

        // Devices made from the Fractal and Line 6 templates, and the unit picker.
        {
            auto root = proc.state;
            state::addCustomUnit (root, templates::createUnit (*templates::find ("fractal.axe-fx-3")));
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("template-axe-fx-3.png"));
            state::addCustomUnit (root, templates::createUnit (*templates::find ("boss.gt-1000")));
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("template-boss-gt-1000.png"));
            state::addCustomUnit (root, templates::createUnit (*templates::find ("darkglass.microtubes-infinity")));
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("template-microtubes-infinity.png"));
        }

        // Fractal / Line 6 pages with demo presets.
        {
            struct Demo { const char* name; int setlist, index, colour; std::vector<const char*> scenes; };
            auto fill = [&proc] (const char* id, const std::vector<Demo>& demos)
            {
                auto root = proc.state;
                auto m = state::modeller (root, id);
                for (int i = m.getNumChildren(); --i >= 0;)
                    if (m.getChild (i).hasType (IDs::ModPreset))
                        m.removeChild (i, nullptr);
                for (const auto& d : demos)
                {
                    auto preset = state::createModPreset (id, d.name, d.setlist, d.index, ui::paletteColour (d.colour));
                    for (int s = 0; s < (int) d.scenes.size(); ++s)
                    {
                        auto scene = ui::nthOfType (preset, IDs::Scene, s);
                        scene.setProperty (IDs::name, d.scenes[(size_t) s], nullptr);
                        scene.setProperty (IDs::colour, ui::paletteColour (s * 3 + d.colour).toString(), nullptr);
                    }
                    m.appendChild (preset, nullptr);
                }
                proc.state.setProperty (IDs::modellerProfile, id, nullptr);
                proc.state.setProperty (IDs::ampUnit, state::modellerAmpUnit, nullptr);
            };
            fill ("line6.helix-floor", { { "Clean Verse", 2, 0, 4, { "Intro", "Verse", "Pre", "Chorus", "Bridge", "Solo", "Breakdown", "Outro" } },
                                         { "Crunch Rhythm", 2, 1, 1, {} }, { "Lead Boost", 2, 2, 0, {} }, { "Ambient Swells", 2, 4, 6, {} },
                                         { "Drop D Heavy", 3, 0, 2, {} } });
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("helix.png"));
            proc.state.setProperty (IDs::mdView, 1, nullptr);
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("helix-looper.png"));
            proc.state.setProperty (IDs::mdView, 2, nullptr);
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("helix-expression.png"));
            proc.state.setProperty (IDs::mdView, 0, nullptr);

            fill ("fractal.axe-fx-2", { { "Clean Rig", -1, 0, 4, { "Intro", "Verse", "Chorus", "Solo", "Bridge", "Outro", "Ambient", "Heavy" } },
                                        { "Plexi Crunch", -1, 1, 1, {} }, { "Lead Rig", -1, 130, 0, {} } });
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("axe-fx-2.png"));
            fill ("line6.hx-stomp", { { "Stomp Clean", -1, 0, 4, { "Verse", "Chorus", "Solo" } }, { "Stomp Drive", -1, 1, 1, {} } });
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("hx-stomp.png"));
            fill ("headrush.core", { { "Worship Clean", -1, 0, 4, { "Intro", "Verse", "Pre", "Chorus", "Bridge", "Solo", "Tag", "Swell", "Big", "Outro" } },
                                     { "Crunch", -1, 1, 1, {} }, { "Lead", -1, 2, 0, {} }, { "Ambient", -1, 9, 6, {} } });
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("headrush-core.png"));
            if (outDir.getFileName() != "images")   // check by eye: the smallest window, with the channel reminder
            {
                const auto size = editor.getBounds();
                state::setFlag (ui::channelsConfirmedFlag, false);
                editor.setSize (980, 680);
                editor.showPage (0);
                save (snapshot (editor), outDir.getChildFile ("check-headrush-small.png"));
                state::setFlag (ui::channelsConfirmedFlag, true);
                editor.showPage (0);
                editor.setBounds (size);
                editor.refreshNow();
            }
            fill ("headrush.pedalboard", { { "Clean", -1, 0, 4, {} }, { "Rhythm", -1, 1, 1, {} }, { "Lead", -1, 2, 0, {} } });
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("headrush-pedalboard.png"));
            fill ("neural.nano-cortex", { { "Clean", -1, 0, 4, {} }, { "Crunch", -1, 1, 1, {} }, { "Lead", -1, 2, 0, {} }, { "Ambient", -1, 3, 6, {} } });
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("nano-cortex.png"));
            fill ("darkglass.anagram", { { "Verse Clean", -1, 0, 4, { "Verse", "Chorus", "Bridge" } }, { "Fuzz Bass", -1, 1, 1, {} },
                                         { "Octave Lead", -1, 3, 0, {} } });
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("anagram.png"));
            fill ("darkglass.infinity-500-combo", { { "Clean", -1, 0, 4, {} }, { "Growl", -1, 1, 1, {} }, { "Fuzz", -1, 2, 0, {} },
                                                    { "Slap", -1, 3, 6, {} }, { "Solo", -1, 4, 2, {} } });
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("infinity-500.png"));
            fill ("darkglass.exponent-500", { { "Clean", -1, 0, 4, {} }, { "Drive", -1, 1, 1, {} }, { "Lead", -1, 2, 0, {} } });
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("exponent-500.png"));
            proc.state.setProperty (IDs::ampUnit, 0, nullptr);
            editor.refreshNow();
        }
        for (const auto& [query, name] : { std::pair<const char*, const char*> { "", "unit-picker.png" }, { "helix", "unit-picker-search.png" } })
        {
            auto picker = ui::makeUnitPicker (proc.state, query);
            save (picker->createComponentSnapshot (picker->getLocalBounds(), true, scale), outDir.getChildFile (name));
        }
        {
            auto units = proc.state.getChildWithName (IDs::CustomUnits);
            while (units.getNumChildren() > 1)
                units.removeChild (units.getNumChildren() - 1, nullptr);
            proc.state.setProperty (IDs::selectedCustomUnit, 0, nullptr);
        }
        {
            const auto scene = state::customUnit (proc.state).getChild (0).getChild (2);   // Presets > Lead
            auto tileEditor = ui::makeTileEditor (proc, scene, false);
            save (tileEditor->createComponentSnapshot (tileEditor->getLocalBounds(), true, 2.0f), outDir.getChildFile ("custom-tile-editor.png"));
        }
        proc.state.setProperty (IDs::ampUnit, 0, nullptr);
        editor.refreshNow();

        editor.showPage (1);
        save (snapshot (editor), outDir.getChildFile ("whammy.png"));
        proc.state.setProperty (IDs::sweepDraw, true, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("whammy-draw.png"));
        // Draw > Wave...: a generated wave in the pad, and the Wave panel.
        {
            const auto saved = proc.state[IDs::sweepDrawing];
            const auto savedName = proc.state[IDs::sweepDrawingName];
            proc.state.setProperty (IDs::sweepDrawingName, juce::String(), nullptr);
            proc.state.setProperty (IDs::sweepDrawing, cues::whammy::encodeDrawing (cues::whammy::waveDrawing (cues::whammy::Wave::sine, 8.0, 0.0, 0.0, 0.0, 1.0, 0.9, 0.0)), nullptr);
            editor.refreshNow();
            save (snapshot (editor), outDir.getChildFile ("whammy-wave.png"));
            ui::MovesPanel::WaveSettings w;
            w.type = (int) cues::whammy::Wave::sine; w.cycles = 8.0; w.grow = 0.9;
            auto panel = ui::MovesPanel::makeWaveEditor (w, theme::whammyRed);
            save (panel->createComponentSnapshot (panel->getLocalBounds(), true, 2.0f), outDir.getChildFile ("wave-editor.png"));
            proc.state.setProperty (IDs::sweepDrawing, saved, nullptr);
            proc.state.setProperty (IDs::sweepDrawingName, savedName, nullptr);
        }
        proc.state.setProperty (IDs::sweepDraw, false, nullptr);
        editor.refreshNow();

        // Whammy DT: the Drop Tune side of the Modes card.
        proc.state.setProperty (IDs::whModel, 1, nullptr);
        proc.state.setProperty (IDs::whDropTuneView, true, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("whammy-dt-droptune.png"));
        proc.state.setProperty (IDs::whModel, 0, nullptr);
        proc.state.setProperty (IDs::whDropTuneView, false, nullptr);
        editor.refreshNow();

        const std::pair<int, const char*> tourShots[] = {
            { 0, "tour-welcome.png" }, { 4, "tour-scenes.png" }, { 5, "tour-target.png" }, { 9, "tour-whammy.png" }, { 12, "tour-treadle.png" }
        };
        for (const auto& [step, name] : tourShots)
        {
            editor.startTour (step);
            save (snapshot (editor), outDir.getChildFile (name));
        }
        proc.state.setProperty (IDs::qcExpressionView, false, nullptr);   // the tour's expression step switched it
    }

    {
        // Settings page in both setup modes; the user's own choice is restored afterwards.
        const auto previous = state::getFlag ("setupViaQcChain");
        for (const auto& [viaQc, name] : { std::pair<bool, const char*> { false, "settings.png" }, { true, "settings-qc-chain.png" } })
        {
            state::setFlag ("setupViaQcChain", viaQc);
            state::setFlag (ui::channelsConfirmedFlag, viaQc);   // settings.png shows the button before it's clicked
            PedalCuesEditor editor (proc, false);
            editor.setSize (1120, 760);
            editor.refreshNow();
            editor.showPage (2);
            save (snapshot (editor), outDir.getChildFile (name));
        }
        state::setFlag ("setupViaQcChain", previous);
        state::setFlag (ui::channelsConfirmedFlag, true);
    }

    {
        auto guide = ui::makeWiringGuide (ui::ampInfo (proc.state), 1);
        save (guide->createComponentSnapshot (guide->getLocalBounds(), true, scale), outDir.getChildFile ("wiring-guide.png"));
        const std::pair<int, const char*> views[] = { { 0, "wiring-one-device.png" }, { 2, "wiring-separate.png" } };
        for (const auto& [view, name] : views)
        {
            auto v = ui::makeWiringGuide (ui::ampInfo (proc.state), view);
            save (v->createComponentSnapshot (v->getLocalBounds(), true, scale), outDir.getChildFile (name));
        }
        auto kemperState = proc.state.createCopy();
        kemperState.setProperty (IDs::ampUnit, 1, nullptr);
        auto kemperGuide = ui::makeWiringGuide (ui::ampInfo (kemperState), 1);
        save (kemperGuide->createComponentSnapshot (kemperGuide->getLocalBounds(), true, scale), outDir.getChildFile ("wiring-guide-kemper.png"));
        if (outDir.getFileName() != "images")   // a scratch folder: extra shots to check by eye
        {
            // Checks only (not in the docs): a unit without USB MIDI, and the QC Mini's TRS jacks.
            auto extra = proc.state.createCopy();
            extra.setProperty (IDs::ampUnit, state::modellerAmpUnit, nullptr);
            extra.setProperty (IDs::modellerProfile, "headrush.flex-prime", nullptr);
            for (int view = 0; view < 3; ++view)
            {
                auto g = ui::makeWiringGuide (ui::ampInfo (extra), view);
                save (g->createComponentSnapshot (g->getLocalBounds(), true, 1.0f), outDir.getChildFile ("check-headrush-" + juce::String (view) + ".png"));
            }
            extra.setProperty (IDs::ampUnit, state::qcMiniAmpUnit, nullptr);
            auto g = ui::makeWiringGuide (ui::ampInfo (extra), 1);
            save (g->createComponentSnapshot (g->getLocalBounds(), true, 1.0f), outDir.getChildFile ("check-qc-mini-1.png"));
        }
    }
    {
        // The standalone app's MIDI Setup tab, with the "Test your pedals" card (demo port, after a Test QC click).
        PedalCuesProcessor::standaloneLayoutForScreenshots = true;
        {
            PedalCuesEditor editor (proc, false);
            editor.setSize (1120, 760);
            editor.refreshNow();
            editor.setHelpInMenuBar (true);   // like the real app: the ☰ items are in the menu bar
            editor.showPage (2);
            save (snapshot (editor), outDir.getChildFile ("settings-standalone.png"));
        }
        PedalCuesProcessor::standaloneLayoutForScreenshots = false;
    }

    state::setFlag (ui::channelsConfirmedFlag, channelsWereConfirmed);

    save (drawTimeline (proc), outDir.getChildFile ("timeline.png"));
    save (drawRouting(), outDir.getChildFile ("routing.png"));
    save (drawPresetTarget(), outDir.getChildFile ("preset-target.png"));
    save (drawIcon (1024), outDir.getChildFile ("icon.png"));
    if (argc > 2)
        save (drawIcon (1024), juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    return 0;
}
