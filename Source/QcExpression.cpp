#include "EditorCommon.h"
#include "MovesPanel.h"

using namespace theme;

namespace ui
{
namespace
{
const juce::Colour expLine { 0xff7fd8ff };

// The Quad Cortex page's Expression view: automates whatever is assigned to Expression 1 or 2 on the QC
// (CC#1 / CC#2). Exp 1 / Exp 2 sit in the card header, "set to" tiles above the moves.
class QcExpression final : public Page
{
public:
    explicit QcExpression (PedalCuesProcessor& p) : proc (p), state (p.state)
    {
        addAndMakeVisible (moves);

        for (auto* b : { &exp1Button, &exp2Button })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4303);
            b->setColour (juce::TextButton::buttonColourId, surface);
            b->setColour (juce::TextButton::buttonOnColourId, qcBlue);
            b->setColour (juce::TextButton::textColourOffId, dim);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            moves.extraHeader().addAndMakeVisible (b);
        }
        exp1Button.setConnectedEdges (juce::Button::ConnectedOnRight);
        exp2Button.setConnectedEdges (juce::Button::ConnectedOnLeft);
        moves.extraHeader().setComponentID ("qc.expPedal");
        exp1Button.setTooltip ("Tiles move what's assigned to Expression 1 on the QC (CC#1)");
        exp2Button.setTooltip ("Tiles move what's assigned to Expression 2 on the QC (CC#2)");
        exp1Button.onClick = [this] { if (exp1Button.getToggleState()) state.setProperty (IDs::expPedal, 1, nullptr); };
        exp2Button.onClick = [this] { if (exp2Button.getToggleState()) state.setProperty (IDs::expPedal, 2, nullptr); };

        loadFirstToggle.setColour (juce::ToggleButton::tickColourId, expLine);
        loadFirstToggle.setTooltip ("Each expression clip first loads the preset open on the left, then moves 1/16 later, so it "
                                    "lands on the right preset even if one was changed by accident. The QC may cut the sound "
                                    "for a moment when it reloads a preset.");
        loadFirstToggle.onClick = [this] { state.setProperty (IDs::expLoadFirst, loadFirstToggle.getToggleState(), nullptr); };
        moves.extraRow().addAndMakeVisible (loadFirstToggle);

        styleCaption (setLabel, "SET TO");
        moves.extraRow().addAndMakeVisible (setLabel);
        moves.extraRow().setComponentID ("qc.expSet");

        refresh();
    }

    void refresh() override
    {
        const auto pedal = pedalNumber();
        (pedal == 2 ? exp2Button : exp1Button).setToggleState (true, juce::dontSendNotification);
        const auto loadFirst = (bool) state[IDs::expLoadFirst];
        const auto preset = openPreset();
        const auto location = preset.isValid() ? juce::String ((int) preset[IDs::bank]) + cues::qc::letter ((int) preset[IDs::slot])
                                               : juce::String ("preset");
        loadFirstToggle.setToggleState (loadFirst, juce::dontSendNotification);
        loadFirstToggle.setButtonText ("Load " + location + " first");
        moves.setHint ("CC#" + juce::String (pedal) + "  -  " + (loadFirst ? "loads " + location + " first"
                                                                            : juce::String ("acts on the preset the QC has loaded")));

        setTiles.clear();
        static const std::pair<float, const char*> positions[] = {
            { 0.0f, "Heel" }, { 0.25f, "25%" }, { 0.5f, "Half" }, { 0.75f, "75%" }, { 1.0f, "Toe" }
        };
        for (const auto& position : positions)
        {
            const auto pos = position.first;
            const juce::String label (position.second);
            auto* t = setTiles.add (new Tile (proc, Tile::Look::utility));
            const auto value = cues::qc::expressionSet (1, pedal, pos).events.front().second.getControllerValue();
            t->title = label;
            t->subtitle = "CC#" + juce::String (pedal) + " = " + juce::String (value);
            t->colour = qcBlue.interpolatedWith (raised, 0.6f - 0.6f * pos);
            t->setTooltip ("Drag onto the timeline to put Expression " + juce::String (pedal) + " at " + label.toLowerCase()
                           + " (for example right after a preset loads)."
                           + (loadFirst ? " Loads " + location + " first." : juce::String (" Acts on the preset the QC has loaded.")));
            t->makeCue = [this, pos] { return withPreset (cues::qc::expressionSet (qcChannel(), pedalNumber(), pos)); };
            moves.extraRow().addAndMakeVisible (t);
        }

        moves.refresh();
        resized();
    }

    void resized() override
    {
        moves.setBounds (getLocalBounds());

        auto hdr = moves.extraHeader().getLocalBounds();
        exp2Button.setBounds (hdr.removeFromRight (70));
        exp1Button.setBounds (hdr.removeFromRight (70));

        auto row = moves.extraRow().getLocalBounds();
        setLabel.setBounds (row.removeFromLeft (64));
        loadFirstToggle.setBounds (row.removeFromRight (170));
        row.removeFromRight (10);
        layoutGrid (setTiles, row.expanded (3, 0), setTiles.size(), 0);
    }

private:
    int pedalNumber() const { return (int) state[IDs::expPedal] == 2 ? 2 : 1; }
    int qcChannel() const   { return (int) state[IDs::qcChannel]; }

    // The preset open in the list (what "Load 1A first" loads).
    juce::ValueTree openPreset() const
    {
        const auto qc = state.getChildWithName (IDs::QC);
        return qc.getChild (juce::jlimit (0, juce::jmax (0, qc.getNumChildren() - 1), (int) state[IDs::selectedPreset]));
    }

    cues::Cue withPreset (cues::Cue c) const
    {
        const auto p = openPreset();
        if (! (bool) state[IDs::expLoadFirst] || ! p.isValid())
            return c;
        return cues::qc::withPresetFirst (c, qcChannel(), (int) p[IDs::setlist], (int) p[IDs::bank], (int) p[IDs::slot],
                                          (bool) state[IDs::sendSetlist], p[IDs::name].toString());
    }

    MovesConfig expressionMoves()
    {
        MovesConfig c;
        c.idPrefix = "qc.exp";
        c.title = "Expression";
        c.hint = "CC#1";
        c.colour = qcBlue;
        c.line = expLine;
        c.beatsId = IDs::expBeats;  c.curveId = IDs::expCurve;  c.resetId = IDs::expReset;
        c.drawId = IDs::expDraw;    c.drawingId = IDs::expDrawing;  c.drawingNameId = IDs::expDrawingName;
        c.resetText = "Back to heel after move";
        c.resetTooltip = "After a move, put the expression pedal back to heel (0). Leave it off for a swell that should stay up.";
        c.padHint = "Drag here to draw an expression move";
        c.padTooltip = "Drag to draw the expression move: bottom = heel, top = toe. Hold Shift to snap to quarter steps.";
        c.drawnTooltip = "Your drawn expression move. Drag onto the timeline where the move should start.";
        c.shapesTooltip = "Ready-made expression moves";
        c.drawTooltip = "Draw your own expression move with the mouse";
        c.tileNote = " It moves what the preset assigns to the expression pedal.";
        c.finish = [this] (cues::Cue cue) { return withPreset (std::move (cue)); };
        c.numShapes = cues::qc::numExpShapes;
        c.shapeRows = 2;
        c.extraHeaderWidth = 140;
        c.extraRowHeight = 44;
        c.shapeName        = [] (int s) { return cues::qc::expShapeName ((cues::qc::ExpShape) s); };
        c.shapeDescription = [] (int s) { return cues::qc::expShapeDescription ((cues::qc::ExpShape) s); };
        c.shapeHolds       = [] (int s) { return s == (int) cues::qc::ExpShape::toe || s == (int) cues::qc::ExpShape::heel; };
        c.makeShape = [this] (int s)
        {
            return cues::qc::expressionMove (qcChannel(), pedalNumber(), (cues::qc::ExpShape) s, (double) state[IDs::expBeats],
                                             (double) state[IDs::expCurve], (bool) state[IDs::expReset]);
        };
        c.makeDrawn = [this] (const std::vector<float>& points, const juce::String& name)
        {
            return cues::qc::expressionDrawn (qcChannel(), pedalNumber(), points, (double) state[IDs::expBeats],
                                              (bool) state[IDs::expReset], name);
        };
        return c;
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;

    juce::TextButton exp1Button { "Exp 1" }, exp2Button { "Exp 2" };
    juce::Label setLabel;
    juce::ToggleButton loadFirstToggle;
    juce::OwnedArray<Tile> setTiles;
    MovesPanel moves { proc, expressionMoves() };
};
} // namespace

std::unique_ptr<Page> makeQcExpression (PedalCuesProcessor& p)
{
    return std::make_unique<QcExpression> (p);
}
} // namespace ui
