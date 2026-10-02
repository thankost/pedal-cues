#include "EditorCommon.h"
#include "MovesPanel.h"

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

        // Italic, heavy "WHAMMY V" logotype drawn as a skewed glyph run.
        juce::GlyphArrangement ga;
        ga.addLineOfText (juce::Font (font (logo.getHeight() * 0.58f, true)).withHorizontalScale (1.1f),
                          "WHAMMY V", logo.getX(), logo.getCentreY() + logo.getHeight() * 0.17f);
        juce::Path text;
        ga.createPath (text);
        text.applyTransform (juce::AffineTransform::shear (-0.22f, 0.0f)
                                 .translated (0.22f * (logo.getCentreY()), 0.0f));

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillPath (text, juce::AffineTransform::translation (2.0f, 3.0f));
        g.setColour (juce::Colours::white);
        g.fillPath (text);

        const auto tb = text.getBounds();

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

class WhammyPage final : public Page
{
public:
    explicit WhammyPage (PedalCuesProcessor& p) : proc (p), state (p.state)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &faceplate, &options, &modesSection, &moves })
            addAndMakeVisible (c);

        for (auto* t : { &chordsToggle, &bypassToggle, &heelToggle })
        {
            t->setColour (juce::ToggleButton::tickColourId, juce::Colour (0xffff5a6a));
            options.addAndMakeVisible (t);
        }

        chordsToggle.setTooltip ("Use the Chords (polyphonic) program range instead of Classic");
        bypassToggle.setTooltip ("Select the mode but leave the Whammy bypassed (LED off)");
        heelToggle.setTooltip ("Send CC#11 = 0 before the mode change so the new mode starts at heel position");

        chordsToggle.onClick = [this] { state.setProperty (IDs::whChords, chordsToggle.getToggleState(), nullptr); };
        bypassToggle.onClick = [this] { state.setProperty (IDs::whBypass, bypassToggle.getToggleState(), nullptr); };
        heelToggle.onClick   = [this] { state.setProperty (IDs::whHeelFirst, heelToggle.getToggleState(), nullptr); };

        refresh();
    }

    void refresh() override
    {
        const auto chords = (bool) state[IDs::whChords];
        const auto bypass = (bool) state[IDs::whBypass];

        chordsToggle.setToggleState (chords, juce::dontSendNotification);
        bypassToggle.setToggleState (bypass, juce::dontSendNotification);
        heelToggle.setToggleState ((bool) state[IDs::whHeelFirst], juce::dontSendNotification);

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

        moves.refresh();
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

        moves.setBounds (r.removeFromBottom (Section::headerHeight + MovesPanel::controlsHeight + 124));
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

        repaint();
    }

private:
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

    MovesConfig treadleMoves()
    {
        MovesConfig c;
        c.idPrefix = "wh";
        c.title = "Treadle moves";
        c.hint = "CC#11, tempo-synced";
        c.colour = whammyRed;
        c.line = juce::Colour (0xffff5a6a);
        c.beatsId = IDs::sweepBeats;  c.curveId = IDs::sweepCurve;  c.resetId = IDs::sweepReset;
        c.drawId = IDs::sweepDraw;    c.drawingId = IDs::sweepDrawing;  c.drawingNameId = IDs::sweepDrawingName;
        c.resetText = "Return to heel after move";
        c.resetTooltip = "After a treadle move, return the treadle to heel (CC#11 = 0)";
        c.padHint = "Drag here to draw a treadle move";
        c.padTooltip = "Drag to draw the treadle move: bottom = heel, top = toe. Hold Shift to snap to quarter steps.";
        c.drawnTooltip = "Your drawn treadle move. Drag onto the timeline where the move should start.";
        c.shapesTooltip = "Ready-made treadle moves";
        c.drawTooltip = "Draw your own treadle move with the mouse";
        c.numShapes = cues::whammy::numShapes;
        c.shapeName        = [] (int s) { return cues::whammy::shapeName ((cues::whammy::Shape) s); };
        c.shapeDescription = [] (int s) { return cues::whammy::shapeDescription ((cues::whammy::Shape) s); };
        c.shapeHolds       = [] (int s) { return s == (int) cues::whammy::Shape::toe || s == (int) cues::whammy::Shape::heel; };
        c.makeShape = [this] (int s)
        {
            return cues::whammy::sweep ((int) state[IDs::whChannel], (cues::whammy::Shape) s, (double) state[IDs::sweepBeats],
                                        (double) state[IDs::sweepCurve], (bool) state[IDs::sweepReset]);
        };
        c.makeDrawn = [this] (const std::vector<float>& points, const juce::String& name)
        {
            return cues::whammy::drawn ((int) state[IDs::whChannel], points, (double) state[IDs::sweepBeats],
                                        (bool) state[IDs::sweepReset], name);
        };
        return c;
    }

    struct Group
    {
        juce::String name;
        juce::Colour colour;
        juce::Rectangle<int> caption;
    };

    PedalCuesProcessor& proc;
    juce::ValueTree state;

    Faceplate faceplate;
    OptionsPanel options;
    Section modesSection  { "wh.modes", "Modes", "Program Change - lit LED = engaged, dark = loads bypassed", whammyRed };

    juce::ToggleButton chordsToggle { "Chords" };
    juce::ToggleButton bypassToggle { "Load bypassed" };
    juce::ToggleButton heelToggle   { "Heel first" };
    MovesPanel moves { proc, treadleMoves() };

    std::vector<Group> groups;
    juce::OwnedArray<Tile> modeTiles;
};
} // namespace

std::unique_ptr<Page> makeWhammyPage (PedalCuesProcessor& p)
{
    return std::make_unique<WhammyPage> (p);
}
} // namespace ui
