#include "EditorCommon.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace theme;

namespace ui
{
namespace
{
// The red Whammy V faceplate banner.
class Faceplate final : public juce::Component
{
public:
    Faceplate() { setInterceptsMouseClicks (false, false); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        juce::ColourGradient grad (juce::Colour (0xffe0303f), b.getTopLeft(), juce::Colour (0xff9e1426), b.getBottomRight(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b, 14.0f);

        // Brushed highlights.
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (b.withHeight (b.getHeight() * 0.45f).reduced (2.0f, 2.0f), 12.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);

        // Screws.
        for (auto p : { juce::Point<float> (12.0f, 12.0f), { 12.0f, b.getBottom() - 12.0f } })
        {
            g.setColour (juce::Colour (0xff5c0b15));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (p));
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawLine (p.x - 2.5f, p.y, p.x + 2.5f, p.y, 1.0f);
        }

        auto area = getLocalBounds().reduced (28, 10);
        auto logo = area.removeFromLeft (360).toFloat();

        // Italic, heavy "WHAMMY" logotype drawn as a skewed glyph run.
        juce::GlyphArrangement ga;
        ga.addLineOfText (juce::Font (font (logo.getHeight() * 0.58f, true)).withHorizontalScale (1.1f),
                          "WHAMMY", logo.getX(), logo.getCentreY() + logo.getHeight() * 0.17f);
        juce::Path text;
        ga.createPath (text);
        text.applyTransform (juce::AffineTransform::shear (-0.22f, 0.0f)
                                 .translated (0.22f * (logo.getCentreY()), 0.0f));

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillPath (text, juce::AffineTransform::translation (2.0f, 3.0f));
        g.setColour (juce::Colours::white);
        g.fillPath (text);

        const auto tb = text.getBounds();
        const juce::Rectangle<float> five (tb.getRight() + 12.0f, tb.getY() + 2.0f, tb.getHeight() - 4.0f, tb.getHeight() - 4.0f);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.fillRoundedRectangle (five, 6.0f);
        g.setColour (juce::Colours::white);
        g.setFont (font (five.getHeight() * 0.7f, true));
        g.drawText ("V", five, juce::Justification::centred);

        g.setColour (juce::Colours::white.withAlpha (0.8f));
        g.setFont (font (11.0f, true));
        g.drawText ("MIDI MODE  +  TREADLE AUTOMATION", juce::Rectangle<float> (tb.getX(), tb.getBottom() + 4.0f, 400.0f, 14.0f),
                    juce::Justification::centredLeft);
    }
};

class OptionsPanel final : public juce::Component
{
public:
    OptionsPanel()
    {
        setComponentID ("wh.options");
        setInterceptsMouseClicks (false, true);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 10.0f);
    }
};

// Freehand treadle canvas: drag to draw heel (bottom) to toe (top) across the move's length.
class DrawPad final : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    DrawPad()
    {
        setComponentID ("wh.draw");
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        setTooltip ("Drag to draw the treadle move: bottom = heel, top = toe. Hold Shift to snap to quarter steps.");
    }

    std::vector<float> points;
    double beats = 4.0;
    std::function<void()> onChange, onCommit;

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (background);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

        const auto plot = plotArea();

        for (auto frac : { 0.25f, 0.5f, 0.75f })
        {
            g.setColour (outline.withAlpha (frac == 0.5f ? 1.0f : 0.6f));
            g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * frac), plot.getX(), plot.getRight());
        }

        // Beat and bar lines (4/4).
        const auto wholeBeats = (int) std::floor (beats);
        if (wholeBeats >= 1 && wholeBeats <= 64)
        {
            for (int k = 1; k < (int) std::ceil (beats); ++k)
            {
                const auto x = plot.getX() + plot.getWidth() * (float) (k / beats);
                const auto bar = k % 4 == 0;
                if (! bar && beats > 16.0)
                    continue;
                g.setColour (bar ? dim.withAlpha (0.45f) : outline.withAlpha (0.7f));
                g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            }
        }

        g.setColour (dim.withAlpha (0.8f));
        g.setFont (font (10.0f, true));
        g.drawText ("TOE", b.getX() + 6.0f, plot.getY() - 6.0f, 36.0f, 12.0f, juce::Justification::centredLeft);
        g.drawText ("HALF", b.getX() + 6.0f, plot.getCentreY() - 6.0f, 36.0f, 12.0f, juce::Justification::centredLeft);
        g.drawText ("HEEL", b.getX() + 6.0f, plot.getBottom() - 6.0f, 36.0f, 12.0f, juce::Justification::centredLeft);

        if (points.size() < 2)
            return;

        juce::Path line, fill;
        for (size_t i = 0; i < points.size(); ++i)
        {
            const auto x = plot.getX() + plot.getWidth() * (float) i / (float) (points.size() - 1);
            const auto y = plot.getBottom() - plot.getHeight() * points[i];
            if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, plot.getBottom()); }
            else          line.lineTo (x, y);
            fill.lineTo (x, y);
        }
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (whammyRed.withAlpha (0.35f), 0.0f, plot.getY(),
                                                 whammyRed.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (juce::Colour (0xffff5a6a));
        g.strokePath (line, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (std::all_of (points.begin(), points.end(), [] (float v) { return v <= 0.0f; }))
        {
            g.setColour (dim);
            g.setFont (font (13.0f, true));
            g.drawText ("Drag here to draw a treadle move", plot, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override  { lastIndex = -1; apply (e); }
    void mouseDrag (const juce::MouseEvent& e) override  { apply (e); }
    void mouseUp (const juce::MouseEvent&) override      { if (onCommit) onCommit(); }

private:
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (10.0f, 12.0f).withTrimmedLeft (34.0f); }

    void apply (const juce::MouseEvent& e)
    {
        if (points.size() < 2)
            return;

        const auto plot = plotArea();
        const auto n = (int) points.size();
        const auto index = juce::jlimit (0, n - 1, juce::roundToInt ((e.position.x - plot.getX()) / plot.getWidth() * (float) (n - 1)));
        auto value = juce::jlimit (0.0f, 1.0f, (plot.getBottom() - e.position.y) / plot.getHeight());
        if (e.mods.isShiftDown())
            value = std::round (value * 4.0f) / 4.0f;

        if (lastIndex < 0 || lastIndex == index)
        {
            points[(size_t) index] = value;
        }
        else
        {
            const auto step = index > lastIndex ? 1 : -1;
            for (int i = lastIndex; i != index + step; i += step)
            {
                const auto t = (float) (i - lastIndex) / (float) (index - lastIndex);
                points[(size_t) i] = lastValue + (value - lastValue) * t;
            }
        }

        lastIndex = index;
        lastValue = value;
        repaint();
        if (onChange)
            onChange();
    }

    int lastIndex = -1;
    float lastValue = 0.0f;
};

class WhammyPage final : public Page
{
public:
    explicit WhammyPage (PedalCuesProcessor& p) : proc (p), state (p.state)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &faceplate, &options, &modesSection, &sweepsSection, &sweepControls })
            addAndMakeVisible (c);

        sweepControls.setComponentID ("wh.sweepControls");
        sweepControls.setInterceptsMouseClicks (false, true);

        for (auto* t : { &chordsToggle, &bypassToggle, &heelToggle })
        {
            t->setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff5a6a));
            options.addAndMakeVisible (t);
        }

        chordsToggle.setTooltip ("Use the Chords (polyphonic) program range instead of Classic");
        bypassToggle.setTooltip ("Select the mode but leave the Whammy bypassed (LED off)");
        heelToggle.setTooltip ("Send CC#11 = 0 before the mode change so the new mode starts at heel position");
        resetToggle.setTooltip ("After a treadle move, return the treadle to heel (CC#11 = 0)");

        chordsToggle.onClick = [this] { state.setProperty (IDs::whChords, chordsToggle.getToggleState(), nullptr); };
        bypassToggle.onClick = [this] { state.setProperty (IDs::whBypass, bypassToggle.getToggleState(), nullptr); };
        heelToggle.onClick   = [this] { state.setProperty (IDs::whHeelFirst, heelToggle.getToggleState(), nullptr); };
        resetToggle.onClick  = [this] { state.setProperty (IDs::sweepReset, resetToggle.getToggleState(), nullptr); };

        for (size_t i = 0; i < lengths.size(); ++i)
            lengthBox.addItem (cues::formatBeats (lengths[i]), (int) i + 1);

        lengthBox.onChange = [this]
        {
            const auto i = lengthBox.getSelectedItemIndex();
            if (i >= 0)
                state.setProperty (IDs::sweepBeats, lengths[(size_t) i], nullptr);
        };

        curveSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        curveSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
        curveSlider.setRange (0.25, 4.0, 0.01);
        curveSlider.setSkewFactorFromMidPoint (1.0);
        curveSlider.setColour (juce::Slider::trackColourId, whammyRed);
        curveSlider.setTooltip ("1 = linear, below 1 = fast start, above 1 = slow start");
        curveSlider.onValueChange = [this] { state.setProperty (IDs::sweepCurve, curveSlider.getValue(), nullptr); };

        styleCaption (lengthLabel, "LENGTH");
        styleCaption (curveLabel, "CURVE");
        resetToggle.setColour (juce::ToggleButton::tickColourId, whammyRed);

        for (auto* c : std::initializer_list<juce::Component*> { &lengthLabel, &lengthBox, &curveLabel, &curveSlider, &resetToggle })
            addAndMakeVisible (c);

        // Shapes / Draw switch in the Treadle moves header.
        for (auto* b : { &shapesButton, &drawButton })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4301);
            b->setColour (juce::TextButton::buttonColourId, surface);
            b->setColour (juce::TextButton::buttonOnColourId, whammyRed);
            b->setColour (juce::TextButton::textColourOffId, dim);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            addAndMakeVisible (b);
        }
        shapesButton.setConnectedEdges (juce::Button::ConnectedOnRight);
        drawButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
        drawButton.setComponentID ("wh.drawMode");
        shapesButton.setTooltip ("Ready-made treadle moves");
        drawButton.setTooltip ("Draw your own treadle move with the mouse");
        shapesButton.onClick = [this] { if (shapesButton.getToggleState()) state.setProperty (IDs::sweepDraw, false, nullptr); };
        drawButton.onClick   = [this] { if (drawButton.getToggleState())   state.setProperty (IDs::sweepDraw, true, nullptr); };

        for (auto* b : { &clearButton, &smoothButton })
        {
            b->setColour (juce::TextButton::buttonColourId, surface);
            b->setColour (juce::TextButton::textColourOffId, text);
            addChildComponent (b);
        }
        clearButton.setTooltip ("Reset the drawing to heel");
        smoothButton.setTooltip ("Round off sharp edges in the drawing");
        clearButton.onClick = [this]
        {
            std::fill (pad.points.begin(), pad.points.end(), 0.0f);
            commitDrawing();
        };
        smoothButton.onClick = [this]
        {
            auto& v = pad.points;
            for (int pass = 0; pass < 2 && v.size() > 2; ++pass)
            {
                const auto copy = v;
                for (size_t i = 1; i + 1 < v.size(); ++i)
                    v[i] = (copy[i - 1] + 2.0f * copy[i] + copy[i + 1]) * 0.25f;
            }
            commitDrawing();
        };

        pad.onChange = [this] { updateDrawTile(); };
        pad.onCommit = [this] { commitDrawing(); };
        addChildComponent (pad);

        refresh();
    }

    void refresh() override
    {
        const auto chords = (bool) state[IDs::whChords];
        const auto bypass = (bool) state[IDs::whBypass];
        const auto beats = (double) state[IDs::sweepBeats];

        chordsToggle.setToggleState (chords, juce::dontSendNotification);
        bypassToggle.setToggleState (bypass, juce::dontSendNotification);
        heelToggle.setToggleState ((bool) state[IDs::whHeelFirst], juce::dontSendNotification);
        resetToggle.setToggleState ((bool) state[IDs::sweepReset], juce::dontSendNotification);
        curveSlider.setValue ((double) state[IDs::sweepCurve], juce::dontSendNotification);

        int bestLength = 0;
        for (size_t i = 0; i < lengths.size(); ++i)
            if (std::abs (lengths[i] - beats) < std::abs (lengths[(size_t) bestLength] - beats))
                bestLength = (int) i;
        lengthBox.setSelectedItemIndex (bestLength, juce::dontSendNotification);

        const auto wh = state.getChildWithName (IDs::Whammy);

        modeTiles.clear();
        for (int i = 0; i < cues::whammy::numEffects; ++i)
        {
            const auto node = wh.getChild (i);
            auto* t = modeTiles.add (new Tile (proc, Tile::Look::whammyMode));
            t->title = node[IDs::name].toString();
            t->subtitle = "PC " + juce::String (cues::whammy::programNumber (i, chords, bypass));
            t->colour = cues::whammy::colour (i);
            t->active = ! bypass;
            t->setTooltip ("Drag onto the timeline to switch the Whammy to this mode. Double-click to rename.");
            t->makeCue = [this, i, node]
            {
                return cues::whammy::effect ((int) state[IDs::whChannel], i, node[IDs::name].toString(),
                                             (bool) state[IDs::whChords], (bool) state[IDs::whBypass],
                                             (int) state[IDs::whPcBase], (bool) state[IDs::whHeelFirst]);
            };
            t->onDoubleClick = [node] { renameNode (node, "Rename Whammy mode"); };
            t->onContextMenu = [this, node, i] { modeMenu (node, i); };
            addAndMakeVisible (t);
        }

        const auto drawMode = (bool) state[IDs::sweepDraw];
        (drawMode ? drawButton : shapesButton).setToggleState (true, juce::dontSendNotification);
        curveSlider.setEnabled (! drawMode);
        curveLabel.setAlpha (drawMode ? 0.4f : 1.0f);
        curveSlider.setAlpha (drawMode ? 0.4f : 1.0f);

        pad.points = cues::whammy::decodeDrawing (state[IDs::sweepDrawing].toString());
        pad.beats = beats;
        pad.setVisible (drawMode);
        clearButton.setVisible (drawMode);
        smoothButton.setVisible (drawMode);
        pad.repaint();

        drawTile.reset (new Tile (proc, Tile::Look::sweep));
        drawTile->title = "Drawn move";
        drawTile->colour = juce::Colour (0xffff5a6a);
        drawTile->setTooltip ("Your drawn treadle move. Drag onto the timeline where the move should start.");
        drawTile->makeCue = [this]
        {
            return cues::whammy::drawn ((int) state[IDs::whChannel], pad.points, (double) state[IDs::sweepBeats],
                                        (bool) state[IDs::sweepReset]);
        };
        addChildComponent (drawTile.get());
        drawTile->setVisible (drawMode);
        updateDrawTile();

        shapeTiles.clear();
        for (int s = 0; s < cues::whammy::numShapes; ++s)
        {
            const auto shape = (cues::whammy::Shape) s;
            auto* t = shapeTiles.add (new Tile (proc, Tile::Look::sweep));
            t->makeCue = [this, shape]
            {
                return cues::whammy::sweep ((int) state[IDs::whChannel], shape, (double) state[IDs::sweepBeats],
                                            (double) state[IDs::sweepCurve], (bool) state[IDs::sweepReset]);
            };

            const auto cue = t->makeCue();
            for (const auto& [beat, msg] : cue.events)
                if (msg.isController())
                    t->curve.push_back ({ (float) (beat / cue.lengthBeats), (float) msg.getControllerValue() / 127.0f });

            t->title = cues::whammy::shapeName (shape);
            t->subtitle = (shape == cues::whammy::Shape::toe || shape == cues::whammy::Shape::heel ? "hold " : "")
                        + cues::formatBeats (beats);
            t->colour = juce::Colour (0xffff5a6a);
            t->setTooltip (cues::whammy::shapeDescription (shape) + ". Drag onto the timeline where the move should start.");
            addChildComponent (t);
            t->setVisible (! drawMode);
        }

        resized();
    }

    void paintOverChildren (juce::Graphics& g) override
    {
        for (const auto& grp : groups)
        {
            g.setColour (grp.colour);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) grp.caption.getX() + 3.0f, (float) grp.caption.getCentreY() }));
            g.setColour (dim);
            g.setFont (font (11.0f, true));
            g.drawText (grp.name, grp.caption.withTrimmedLeft (12), juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);

        auto plate = r.removeFromTop (92);
        faceplate.setBounds (plate);
        options.setBounds (plate.removeFromRight (juce::jmin (560, plate.getWidth() / 2)).reduced (14, 22));
        {
            auto o = options.getLocalBounds().reduced (12, 0);
            const auto w = o.getWidth() / 3;
            chordsToggle.setBounds (o.removeFromLeft (w));
            bypassToggle.setBounds (o.removeFromLeft (w));
            heelToggle.setBounds (o);
        }
        r.removeFromTop (12);

        sweepsSection.setBounds (r.removeFromBottom (Section::headerHeight + 42 + 124));
        r.removeFromBottom (12);
        modesSection.setBounds (r);

        // Modes, as on the pedal panel (manual p.10): each Whammy mode sits above the Harmony mode on the
        // same panel row (2 Oct Up over Oct Down/Oct Up ... 2 Oct Down over 2nd Up/3rd Up), Dive Bomb has
        // no partner, and Detune (Shallow, Deep) gets its own row below.
        auto m = modesSection.contentArea().expanded (3, 0);
        constexpr int captionH = 18, rowGap = 6, cols = 10;
        const auto rowH = (m.getHeight() - 3 * captionH - 2 * rowGap) / 3;
        const auto unit = m.getWidth() / cols;
        const auto top1 = m.getY() + captionH;
        const auto top2 = top1 + rowH + rowGap + captionH;
        const auto top3 = top2 + rowH + rowGap + captionH;

        auto cell = [&] (int col, int top) { return juce::Rectangle<int> (m.getX() + col * unit, top - 3, unit, rowH + 6); };
        auto place = [&] (int index, int col, int top)
        {
            if (juce::isPositiveAndBelow (index, modeTiles.size()))
                modeTiles[index]->setBounds (cell (col, top));
        };
        auto caption = [&] (int col, int span, int top) { return juce::Rectangle<int> (m.getX() + col * unit + 3, top - captionH, span * unit, captionH); };

        for (int i = 0; i < 10; ++i)
            place (i, i, top1);                  // Whammy: 2 Oct Up .. Dive Bomb
        for (int i = 0; i < 9; ++i)
            place (20 - i, i, top2);             // Harmony: Oct Down/Oct Up .. 2nd Up/3rd Up
        place (11, 0, top3);                     // Shallow Detune
        place (10, 1, top3);                     // Deep Detune

        groups.clear();
        groups.push_back ({ "WHAMMY", cues::whammy::colour (0), caption (0, 10, top1) });
        groups.push_back ({ "HARMONY", cues::whammy::colour (12), caption (0, 9, top2) });
        groups.push_back ({ "DETUNE", cues::whammy::colour (10), caption (0, 2, top3) });

        auto controlsRow = sweepsSection.contentArea().removeFromTop (34);
        sweepControls.setBounds (controlsRow);
        lengthLabel.setBounds (controlsRow.removeFromLeft (58));
        lengthBox.setBounds (controlsRow.removeFromLeft (110).reduced (0, 3));
        controlsRow.removeFromLeft (24);
        curveLabel.setBounds (controlsRow.removeFromLeft (52));
        curveSlider.setBounds (controlsRow.removeFromLeft (220));
        controlsRow.removeFromLeft (24);
        resetToggle.setBounds (controlsRow.removeFromLeft (240));

        {
            auto hdr = sweepsSection.headerArea().withSizeKeepingCentre (sweepsSection.headerArea().getWidth(), 28);
            drawButton.setBounds (hdr.removeFromRight (80));
            shapesButton.setBounds (hdr.removeFromRight (80));
        }

        auto tiles = sweepsSection.contentArea().withTrimmedTop (42).expanded (3);
        layoutGrid (shapeTiles, tiles, cues::whammy::numShapes, 0);

        {
            auto area = tiles;
            const auto tileW = area.getWidth() / cues::whammy::numShapes;
            if (drawTile != nullptr)
                drawTile->setBounds (area.removeFromRight (tileW));
            area.removeFromRight (6);
            auto buttons = area.removeFromRight (96).reduced (3, 0).withSizeKeepingCentre (90, 78);
            clearButton.setBounds (buttons.removeFromTop (36));
            buttons.removeFromTop (6);
            smoothButton.setBounds (buttons.removeFromTop (36));
            area.removeFromRight (6);
            pad.setBounds (area.reduced (3));
        }
        repaint();
    }

private:
    void updateDrawTile()
    {
        if (drawTile == nullptr)
            return;

        const auto beats = (double) state[IDs::sweepBeats];
        const auto cue = drawTile->makeCue();
        drawTile->curve.clear();
        for (const auto& [beat, msg] : cue.events)
            if (msg.isController())
                drawTile->curve.push_back ({ (float) (beat / cue.lengthBeats), (float) msg.getControllerValue() / 127.0f });
        drawTile->subtitle = cues::formatBeats (beats);
        drawTile->repaint();
    }

    void commitDrawing()
    {
        pad.repaint();
        updateDrawTile();
        state.setProperty (IDs::sweepDrawing, cues::whammy::encodeDrawing (pad.points), nullptr);
    }

    void modeMenu (juce::ValueTree node, int index)
    {
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        m.addItem (2, "Reset name");
        m.addItem (3, "Send to pedal now");

        juce::Component::SafePointer<WhammyPage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, node, index] (int result) mutable
        {
            if (safe == nullptr)
                return;

            if (result == 1)
                renameNode (node, "Rename Whammy mode");
            else if (result == 2)
                node.setProperty (IDs::name, cues::whammy::defaultName (index), nullptr);
            else if (result == 3 && juce::isPositiveAndBelow (index, safe->modeTiles.size()))
                safe->modeTiles[index]->sendNow();
        });
    }

    struct Group
    {
        juce::String name;
        juce::Colour colour;
        juce::Rectangle<int> caption;
    };

    static constexpr std::array<double, 10> lengths { 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0 };

    PedalCuesProcessor& proc;
    juce::ValueTree state;

    Faceplate faceplate;
    OptionsPanel options;
    Section modesSection  { "wh.modes", "Modes", "Program Change - lit LED = engaged, dark = loads bypassed", whammyRed };
    Section sweepsSection { "wh.sweeps", "Treadle moves", "CC#11, tempo-synced", whammyRed };
    juce::Component sweepControls;

    juce::ToggleButton chordsToggle { "Chords" };
    juce::ToggleButton bypassToggle { "Load bypassed" };
    juce::ToggleButton heelToggle   { "Heel first" };
    juce::ToggleButton resetToggle  { "Return to heel after move" };
    juce::TextButton shapesButton { "Shapes" }, drawButton { "Draw" };
    juce::TextButton clearButton { "Clear" }, smoothButton { "Smooth" };
    DrawPad pad;
    std::unique_ptr<Tile> drawTile;
    juce::Label lengthLabel, curveLabel;
    juce::ComboBox lengthBox;
    juce::Slider curveSlider;

    std::vector<Group> groups;
    juce::OwnedArray<Tile> modeTiles, shapeTiles;
};
} // namespace

std::unique_ptr<Page> makeWhammyPage (PedalCuesProcessor& p)
{
    return std::make_unique<WhammyPage> (p);
}
} // namespace ui
