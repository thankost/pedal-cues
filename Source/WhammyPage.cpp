#include "EditorCommon.h"

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
            addAndMakeVisible (t);
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

        // Modes: Whammy (10) | Detune (2) | Harmony (9)
        auto m = modesSection.contentArea();
        constexpr int gap = 14;
        const auto unit = (m.getWidth() - 2 * gap) / 11;
        const std::array<std::tuple<int, int, int, juce::String, juce::Colour>, 3> spec { {
            { 0, 10, 5, "WHAMMY", cues::whammy::colour (0) },
            { 10, 2, 1, "DETUNE", cues::whammy::colour (10) },
            { 12, 9, 5, "HARMONY", cues::whammy::colour (12) } } };

        groups.clear();
        for (const auto& [first, count, cols, name, colour] : spec)
        {
            auto col = m.removeFromLeft (unit * cols);
            m.removeFromLeft (gap);
            groups.push_back ({ name, colour, col.removeFromTop (18) });
            layoutGrid (modeTiles, col.expanded (3, 3), cols, 0, first, count);
        }

        auto controlsRow = sweepsSection.contentArea().removeFromTop (34);
        sweepControls.setBounds (controlsRow);
        lengthLabel.setBounds (controlsRow.removeFromLeft (58));
        lengthBox.setBounds (controlsRow.removeFromLeft (110).reduced (0, 3));
        controlsRow.removeFromLeft (24);
        curveLabel.setBounds (controlsRow.removeFromLeft (52));
        curveSlider.setBounds (controlsRow.removeFromLeft (220));
        controlsRow.removeFromLeft (24);
        resetToggle.setBounds (controlsRow.removeFromLeft (240));

        auto tiles = sweepsSection.contentArea().withTrimmedTop (42).expanded (3);
        layoutGrid (shapeTiles, tiles, cues::whammy::numShapes, 0);
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
