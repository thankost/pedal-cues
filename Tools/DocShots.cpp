// Renders the images used by docs/GUIDE.md from the real UI components.
// Usage: DocShots <outputDirectory> [iconPngPath]

#include "../Source/PluginEditor.h"
#include "../Source/PluginProcessor.h"

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
        link (t, i, accent, "USB");
        link (i, q, grey, "MIDI");
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
        link (t1, o1, accent, "USB");
        link (o1, q, grey, "MIDI");
        link (t2, o2, accent, "USB");
        link (o2, wh, grey, "MIDI");
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
        link (t1, q, accent, "USB");
        link (t2, o, accent, "USB");
        link (o, wh, grey, "MIDI");
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
        link (t, q, grey, "USB");
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

    row (220.0f, "CURRENT QC PRESET", "Tiles only switch the scene or footswitch on the preset already loaded. No reload, no audio gap.",
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

    {
        PedalCuesEditor editor (proc, false);
        editor.setSize (1120, 760);
        editor.refreshNow();

        editor.showPage (0);
        save (snapshot (editor), outDir.getChildFile ("quad-cortex.png"));
        editor.showPage (1);
        save (snapshot (editor), outDir.getChildFile ("whammy.png"));
        proc.state.setProperty (IDs::sweepDraw, true, nullptr);
        editor.refreshNow();
        save (snapshot (editor), outDir.getChildFile ("whammy-draw.png"));
        proc.state.setProperty (IDs::sweepDraw, false, nullptr);
        editor.refreshNow();

        const std::pair<int, const char*> tourShots[] = {
            { 0, "tour-welcome.png" }, { 3, "tour-scenes.png" }, { 4, "tour-target.png" }, { 7, "tour-whammy.png" }, { 9, "tour-treadle.png" }
        };
        for (const auto& [step, name] : tourShots)
        {
            editor.startTour (step);
            save (snapshot (editor), outDir.getChildFile (name));
        }
    }

    {
        // Settings page in both setup modes; the user's own choice is restored afterwards.
        const auto previous = state::getFlag ("setupViaQcChain");
        for (const auto& [viaQc, name] : { std::pair<bool, const char*> { false, "settings.png" }, { true, "settings-qc-chain.png" } })
        {
            state::setFlag ("setupViaQcChain", viaQc);
            PedalCuesEditor editor (proc, false);
            editor.setSize (1120, 760);
            editor.refreshNow();
            editor.showPage (2);
            save (snapshot (editor), outDir.getChildFile (name));
        }
        state::setFlag ("setupViaQcChain", previous);
    }

    {
        auto guide = ui::makeWiringGuide();
        save (guide->createComponentSnapshot (guide->getLocalBounds(), true, scale), outDir.getChildFile ("wiring-guide.png"));
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

    save (drawTimeline (proc), outDir.getChildFile ("timeline.png"));
    save (drawRouting(), outDir.getChildFile ("routing.png"));
    save (drawPresetTarget(), outDir.getChildFile ("preset-target.png"));
    save (drawIcon (1024), outDir.getChildFile ("icon.png"));
    if (argc > 2)
        save (drawIcon (1024), juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    return 0;
}
