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
        { "Clean Verse",   1, 0, 4, { "Intro", "Verse", "Pre", "Chorus", "Bridge", "Solo", "Breakdown", "Outro" } },
        { "Crunch Chorus", 1, 1, 1, { "Riff", "Verse", "Build", "Chorus", "Half-time", "Lead", "Stop", "Ring out" } },
        { "Lead Solo",     1, 2, 0, { "Rhythm", "Solo", "Octave", "Harmony", "Dry", "Wet", "Feedback", "Tail" } },
        { "Ambient Intro", 2, 0, 6, { "Pad", "Swell", "Shimmer", "Clean", "Freeze", "Delay", "Verb", "Silence" } },
        { "Heavy Drop",    2, 1, 2, { "Chug", "Verse", "Drop", "Chorus", "Breakdown", "Solo", "Gallop", "End" } },
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
    g.addTransform (juce::AffineTransform::scale ((float) size / 1024.0f));

    // macOS-style squircle: 824 px body on a 1024 canvas.
    const juce::Rectangle<float> body (100.0f, 100.0f, 824.0f, 824.0f);
    juce::Path shape;
    shape.addRoundedRectangle (body, 185.0f);

    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillPath (shape, juce::AffineTransform::translation (0.0f, 8.0f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b303b), body.getX(), body.getY(),
                                             juce::Colour (0xff0c0d10), body.getX(), body.getBottom(), false));
    g.fillPath (shape);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.strokePath (shape, juce::PathStrokeType (4.0f));

    g.saveState();
    g.reduceClipRegion (shape);

    // Timeline lane
    const juce::Rectangle<float> lane (100.0f, 640.0f, 824.0f, 170.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (lane);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    for (float x = 170.0f; x < 924.0f; x += 131.0f)
        g.fillRect (x, lane.getY(), 3.0f, lane.getHeight());

    auto clip = [&g] (juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c.withAlpha (0.35f));
        g.fillRoundedRectangle (r, 18.0f);
        g.setColour (c);
        g.fillRoundedRectangle (r.withHeight (46.0f), 18.0f);
        g.fillRect (r.withHeight (46.0f).withTrimmedTop (23.0f));
        g.drawRoundedRectangle (r, 18.0f, 6.0f);
    };
    clip ({ 150.0f, 665.0f, 220.0f, 120.0f }, qcBlue);
    clip ({ 654.0f, 665.0f, 220.0f, 120.0f }, whammyRed);

    // Drop target
    const juce::Rectangle<float> target (402.0f, 665.0f, 220.0f, 120.0f);
    g.setColour (accent.withAlpha (0.18f));
    g.fillRoundedRectangle (target, 18.0f);
    juce::Path outline, dashed;
    outline.addRoundedRectangle (target, 18.0f);
    const float dashes[] = { 22.0f, 14.0f };
    juce::PathStrokeType (6.0f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (accent);
    g.fillPath (dashed);

    // Playhead
    g.setColour (accent);
    g.fillRect (512.0f - 3.0f, 600.0f, 6.0f, 324.0f);
    g.restoreState();

    // The tile being dropped: an amber footswitch cue, slightly tilted.
    const juce::Rectangle<float> tile (322.0f, 170.0f, 380.0f, 300.0f);
    const auto tilt = juce::AffineTransform::rotation (-0.10f, tile.getCentreX(), tile.getCentreY());
    juce::Path tilePath;
    tilePath.addRoundedRectangle (tile, 44.0f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (tilePath, tilt.translated (10.0f, 22.0f));
    g.setGradientFill (juce::ColourGradient (accent.brighter (0.25f), tile.getX(), tile.getY(),
                                             accent.darker (0.35f), tile.getX(), tile.getBottom(), false));
    g.fillPath (tilePath, tilt);

    g.saveState();
    g.addTransform (tilt);
    const auto knob = juce::Rectangle<float> (0.0f, 0.0f, 170.0f, 170.0f).withCentre (tile.getCentre().translated (0.0f, 12.0f));
    g.setColour (juce::Colour (0xff15171c));
    g.fillEllipse (knob.expanded (18.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8ebf0), knob.getX(), knob.getY(),
                                             juce::Colour (0xff7d838f), knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (juce::Colour (0xff15171c).withAlpha (0.35f));
    g.drawEllipse (knob.reduced (26.0f), 6.0f);
    // LED
    const auto led = juce::Rectangle<float> (0.0f, 0.0f, 34.0f, 34.0f).withCentre ({ tile.getCentreX(), tile.getY() + 42.0f });
    g.setColour (ledGreen.withAlpha (0.35f));
    g.fillEllipse (led.expanded (14.0f));
    g.setColour (ledGreen.brighter (0.3f));
    g.fillEllipse (led);
    g.restoreState();

    // Arrow into the gap
    juce::Path arrow;
    arrow.startNewSubPath (512.0f, 500.0f);
    arrow.lineTo (512.0f, 585.0f);
    g.setColour (juce::Colours::white);
    g.strokePath (arrow, juce::PathStrokeType (22.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path head;
    head.addTriangle (462.0f, 575.0f, 562.0f, 575.0f, 512.0f, 635.0f);
    g.fillPath (head);

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
        g.drawText (t == 0 ? "audio" : (t == 1 ? "MIDI > Quad Cortex" : "MIDI > QC > Whammy"), juce::Rectangle<float> (18.0f, y + 26.0f, headerW - 30.0f, 16.0f),
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

    drawItem (g, item (1.0f, 1.9f, qcY), ui::paletteColour (4), "QC Clean Verse");
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
    constexpr int w = 1120, h = 430;
    juce::Image img (juce::Image::ARGB, (int) (w * scale), (int) (h * scale), true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (scale));
    g.fillAll (background);

    const juce::Rectangle<float> pc (40.0f, 60.0f, 280.0f, 110.0f);
    const juce::Rectangle<float> qc (420.0f, 60.0f, 280.0f, 110.0f);
    const juce::Rectangle<float> wh (800.0f, 60.0f, 280.0f, 110.0f);
    const juce::Rectangle<float> iface (420.0f, 270.0f, 280.0f, 110.0f);

    g.setColour (dim);
    g.setFont (font (12.0f, true));
    g.drawText ("RECOMMENDED", juce::Rectangle<float> (40.0f, 22.0f, 400.0f, 20.0f), juce::Justification::centredLeft);
    g.drawText ("ALTERNATIVE", juce::Rectangle<float> (40.0f, 340.0f, 360.0f, 20.0f), juce::Justification::centredLeft);
    g.setFont (font (12.0f));
    g.drawText ("if your QC does not forward USB MIDI to its MIDI Out", juce::Rectangle<float> (40.0f, 360.0f, 360.0f, 20.0f),
                juce::Justification::centredLeft);

    box (g, pc, accent, "Reaper + PedalCues", "MIDI items on the timeline, track output = QC MIDI");
    box (g, qc, qcBlue, "Quad Cortex / Mini", "MIDI channel 1 (Settings > MIDI). MIDI Thru on.");
    box (g, wh, whammyRed, "Whammy V", "MIDI channel 2 (hold footswitch at power-up)");
    box (g, iface, dim, "USB MIDI interface", "Any class-compliant interface with a 5-pin DIN out");

    arrow (g, { pc.getRight(), pc.getCentreY() }, { qc.getX(), qc.getCentreY() }, accent, "USB", false);
    arrow (g, { qc.getRight(), qc.getCentreY() }, { wh.getX(), wh.getCentreY() }, qcBlue, "MIDI Out > MIDI In", false);
    arrow (g, { pc.getCentreX(), pc.getBottom() }, { iface.getX(), iface.getCentreY() }, dim, "USB", true);
    arrow (g, { iface.getRight(), iface.getCentreY() }, { wh.getCentreX(), wh.getBottom() }, dim, "5-pin MIDI", true);

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
        editor.showPage (2);
        save (snapshot (editor), outDir.getChildFile ("settings.png"));

        const std::pair<int, const char*> tourShots[] = {
            { 0, "tour-welcome.png" }, { 3, "tour-scenes.png" }, { 6, "tour-whammy.png" }, { 8, "tour-treadle.png" }
        };
        for (const auto& [step, name] : tourShots)
        {
            editor.startTour (step);
            save (snapshot (editor), outDir.getChildFile (name));
        }
    }

    save (drawTimeline (proc), outDir.getChildFile ("timeline.png"));
    save (drawRouting(), outDir.getChildFile ("routing.png"));
    save (drawIcon (1024), outDir.getChildFile ("icon.png"));
    if (argc > 2)
        save (drawIcon (1024), juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    return 0;
}
