#include "EditorCommon.h"
#include "Modellers.h"
#include "MovesPanel.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace theme;

namespace ui
{
namespace
{
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
        faceplate.setComponentID ("wh.faceplate");   // the tour frames the whole red bar

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

        // Early by (in the Treadle moves header): for a Whammy that answers a little late.
        earlyLabel.setText ("EARLY BY", juce::dontSendNotification);
        earlyLabel.setFont (font (11.5f, true));
        earlyLabel.setColour (juce::Label::textColourId, dim);
        earlyLabel.setJustificationType (juce::Justification::centredRight);
        for (int ms : { 0, 10, 20, 30, 40, 50 })
            earlyBox.addItem (ms == 0 ? juce::String ("Off") : juce::String (ms) + " ms", ms + 1);
        earlyBox.setComponentID ("wh.movesEarly");
        const juce::String earlyTip ("Plays every treadle move this much earlier inside its clip, for a Whammy that answers a little "
                                     "late: you still drop the move on the beat. The clip's first point stays at its start, and mode "
                                     "changes aren't moved. Applies to moves you drag from now on.");
        earlyBox.setTooltip (earlyTip);
        earlyLabel.setTooltip (earlyTip);
        earlyBox.onChange = [this] { state.setProperty (IDs::whMovesEarly, earlyBox.getSelectedId() - 1, nullptr); };
        moves.extraHeader().addAndMakeVisible (earlyLabel);
        moves.extraHeader().addAndMakeVisible (earlyBox);

        // Program numbering: rarely needed, so it's in the Modes header's "..." menu rather than among the options.
        moreButton.setComponentID ("wh.more");
        moreButton.setColour (juce::TextButton::buttonColourId, raised);
        moreButton.setTooltip ("Program numbering: only change it if every mode lands one position off on your Whammy.");
        moreButton.onClick = [this] { showMoreMenu(); };
        addAndMakeVisible (moreButton);

        // Whammy V or DT: two devices in the Effects & Pedals list (whModel), so the faceplate only names it.

        // Whammy DT: the Modes card switches between the Whammy side and the Drop Tune side of the pedal.
        for (auto* b : { &whammyViewButton, &dropTuneViewButton })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4305);
            b->setColour (juce::TextButton::buttonColourId, surface);
            b->setColour (juce::TextButton::buttonOnColourId, whammyRed);
            b->setColour (juce::TextButton::textColourOffId, dim);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            addChildComponent (b);
        }
        whammyViewButton.setConnectedEdges (juce::Button::ConnectedOnRight);
        dropTuneViewButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
        dropTuneViewButton.setComponentID ("wh.dropTuneView");
        whammyViewButton.setTooltip ("The Whammy, Harmony and Detune modes (the pedal's left knob)");
        dropTuneViewButton.setTooltip ("Shift Up and Shift Down: drop or raise your tuning (the pedal's right knob)");
        whammyViewButton.onClick   = [this] { if (whammyViewButton.getToggleState())   state.setProperty (IDs::whDropTuneView, false, nullptr); };
        dropTuneViewButton.onClick = [this] { if (dropTuneViewButton.getToggleState()) state.setProperty (IDs::whDropTuneView, true, nullptr); };

        refresh();
    }

    void refresh() override
    {
        const auto dt = isDt();
        const auto dropView = dt && (bool) state[IDs::whDropTuneView];
        const auto chords = (bool) state[IDs::whChords] && ! dt;   // on the DT, the Chords numbers are Drop Tune
        const auto bypass = (bool) state[IDs::whBypass];

        faceplate.model = dt ? "WHAMMY DT" : "WHAMMY V";
        faceplate.tagline = dt ? "MIDI MODE  +  DROP TUNE  +  TREADLE AUTOMATION" : "MIDI MODE  +  TREADLE AUTOMATION";
        faceplate.repaint();
        chordsToggle.setVisible (! dt);
        whammyViewButton.setVisible (dt);
        dropTuneViewButton.setVisible (dt);
        (dropView ? dropTuneViewButton : whammyViewButton).setToggleState (true, juce::dontSendNotification);
        modesSection.hint = dropView ? juce::String ("Program Change - shifts everything you play; lit LED = on, dark = loads bypassed")
                                     : juce::String ("Program Change - lit LED = engaged, dark = loads bypassed");
        modesSection.repaint();

        chordsToggle.setToggleState (chords, juce::dontSendNotification);
        bypassToggle.setToggleState (bypass, juce::dontSendNotification);
        heelToggle.setToggleState ((bool) state[IDs::whHeelFirst], juce::dontSendNotification);
        earlyBox.setSelectedId (juce::jlimit (0, 50, (int) state[IDs::whMovesEarly]) / 10 * 10 + 1, juce::dontSendNotification);

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
                return cues::whammy::effect (state::pedalChannel (state), i, node[IDs::name].toString(),
                                             (bool) state[IDs::whChords] && ! isDt(), (bool) state[IDs::whBypass],
                                             (int) state[IDs::whPcBase], (bool) state[IDs::whHeelFirst]);
            };
            t->onDoubleClick = [node] { renameNode (node, "Rename Whammy mode"); };
            t->onContextMenu = [this, node, i] { modeMenu (node, i); };
            addChildComponent (t);
            t->setVisible (! dropView);
        }

        // Whammy DT Drop Tune: Shift Up on top, Shift Down below, like the pedal's right knob.
        dropTiles.clear();
        for (const auto up : { true, false })
            for (int step = 0; step < cues::whammy::numShifts; ++step)
            {
                auto* t = dropTiles.add (new Tile (proc, Tile::Look::whammyMode));
                t->title = juce::String (up ? "+" : "-") + cues::whammy::shiftName (step);
                t->subtitle = "PC " + juce::String (cues::whammy::dropTuneProgram (up, step, bypass));
                t->colour = up ? shiftUpColour : shiftDownColour;
                t->active = ! bypass;
                t->setTooltip (cues::whammy::shiftDescription (up, step) + ". Drag onto the timeline where the new tuning starts.");
                t->makeCue = [this, up, step]
                {
                    return cues::whammy::dropTune (state::pedalChannel (state), up, step, (bool) state[IDs::whBypass],
                                                   (int) state[IDs::whPcBase]);
                };
                addChildComponent (t);
                t->setVisible (dropView);
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

        if (! dropNote.isEmpty())
        {
            g.setColour (dim);
            g.setFont (font (12.5f));
            g.drawFittedText ("Drop Tune can be combined with a Whammy, Harmony or Detune mode, like on the pedal: for example, "
                              "tune down a semitone and still bend with the treadle.",
                              dropNote, juce::Justification::topLeft, 2);
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);

        auto plate = r.removeFromTop (92);
        faceplate.setBounds (plate);
        options.setBounds (plate.removeFromRight (juce::jmin (760, plate.getWidth() - 440)).reduced (14, 22));
        {
            auto o = options.getLocalBounds().reduced (12, 0);
            // Widths shared out by label length: Chords is short, "Load bypassed" the longest.
            const auto total = (float) o.getWidth();
            const auto share = isDt() ? std::array<float, 3> { 0.0f, 0.55f, 0.45f } : std::array<float, 3> { 0.27f, 0.40f, 0.33f };
            if (! isDt())
                chordsToggle.setBounds (o.removeFromLeft (juce::roundToInt (total * share[0])));
            bypassToggle.setBounds (o.removeFromLeft (juce::roundToInt (total * share[1])));
            heelToggle.setBounds (o);
        }
        r.removeFromTop (12);

        moves.setBounds (r.removeFromBottom (Section::headerHeight + MovesPanel::controlsHeight + 124));
        {
            auto hdr = moves.extraHeader().getLocalBounds();
            earlyBox.setBounds (hdr.removeFromRight (100));
            earlyLabel.setBounds (hdr.withTrimmedRight (6));
        }
        r.removeFromBottom (12);
        modesSection.setBounds (r);
        {
            auto hdr = modesSection.headerArea().withSizeKeepingCentre (modesSection.headerArea().getWidth(), 28);
            moreButton.setBounds (hdr.removeFromRight (40));
            hdr.removeFromRight (8);
            dropTuneViewButton.setBounds (hdr.removeFromRight (100));
            whammyViewButton.setBounds (hdr.removeFromRight (100));
        }

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
        dropNote = {};
        if (isDt() && (bool) state[IDs::whDropTuneView])
        {
            // Two rows of Drop Tune tiles, as tall as the mode rows above would be.
            const auto dropTop1 = top1;
            const auto dropTop2 = dropTop1 + rowH + rowGap + captionH;
            for (int i = 0; i < dropTiles.size(); ++i)
                dropTiles[i]->setBounds (cell (i % cues::whammy::numShifts, i < cues::whammy::numShifts ? dropTop1 : dropTop2));
            groups.push_back ({ "SHIFT UP  -  raise your tuning", shiftUpColour, caption (0, 9, dropTop1) });
            groups.push_back ({ "SHIFT DOWN  -  drop your tuning", shiftDownColour, caption (0, 9, dropTop2) });
            dropNote = juce::Rectangle<int> (m.getX() + 3, dropTop2 + rowH + rowGap + 8, m.getWidth() - 6, 40);
            repaint();
            return;
        }
        groups.push_back ({ "WHAMMY", cues::whammy::colour (0), caption (0, 10, top1) });
        groups.push_back ({ "HARMONY", cues::whammy::colour (12), caption (0, 9, top2) });
        groups.push_back ({ "DETUNE", cues::whammy::colour (10), caption (0, 2, top3) });

        repaint();
    }

private:
    bool isDt() const { return (int) state[IDs::whModel] == 1; }

    const juce::Colour shiftUpColour { 0xfff5a623 }, shiftDownColour { 0xff8e7cf0 };
    juce::Rectangle<int> dropNote;   // a short note under the Drop Tune rows

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
        c.drawnTooltip = "Your drawn treadle move. Drag onto the timeline where the move should start.";
        c.shapesTooltip = "Ready-made treadle moves";
        c.drawTooltip = "Draw your own treadle move with the mouse";
        c.numShapes = cues::whammy::numShapes;
        c.extraHeaderWidth = 190;   // Early by
        c.shapeName        = [] (int s) { return cues::whammy::shapeName ((cues::whammy::Shape) s); };
        c.shapeDescription = [] (int s) { return cues::whammy::shapeDescription ((cues::whammy::Shape) s); };
        c.shapeHolds       = [] (int s) { return s == (int) cues::whammy::Shape::toe || s == (int) cues::whammy::Shape::heel; };
        c.makeShape = [this] (int s)
        {
            return early (cues::whammy::sweep (state::pedalChannel (state), (cues::whammy::Shape) s, (double) state[IDs::sweepBeats],
                                               (double) state[IDs::sweepCurve], (bool) state[IDs::sweepReset]));
        };
        c.controller = [] { return 11; };   // the treadle
        c.makeDrawn = [this] (const std::vector<float>& points, const juce::String& name)
        {
            return early (cues::whammy::drawn (state::pedalChannel (state), points, (double) state[IDs::sweepBeats],
                                               (bool) state[IDs::sweepReset], name));
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
    juce::Label earlyLabel;
    juce::ComboBox earlyBox;
    MovesPanel moves { proc, treadleMoves() };

    std::vector<Group> groups;
    juce::OwnedArray<Tile> modeTiles, dropTiles;
    juce::TextButton whammyViewButton { "Whammy" }, dropTuneViewButton { "Drop Tune" };
    juce::TextButton moreButton { "..." };

    // Early by: the move plays that much earlier inside its clip, at the tempo it's dragged at.
    cues::Cue early (cues::Cue cue) const
    {
        cues::whammy::moveEarly (cue, cues::whammy::msToBeats ((int) state[IDs::whMovesEarly], proc.getHostBpm()));
        return cue;
    }

    void showMoreMenu()
    {
        const auto base = (int) state[IDs::whPcBase];
        juce::PopupMenu m;
        m.addSectionHeader ("Program numbering");
        m.addItem (1, "As printed in the manual (1 = first)", true, base == 1);
        m.addItem (2, "Zero-based (0 = first)", true, base != 1);
        m.addSeparator();
        m.addItem (3, "Only change this if every mode lands one position off.", false, false);
        juce::Component::SafePointer<WhammyPage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton), [safe] (int result)
        {
            if (safe != nullptr && (result == 1 || result == 2))
                safe->state.setProperty (IDs::whPcBase, result == 1 ? 1 : 0, nullptr);
        });
    }
};
} // namespace

std::unique_ptr<Page> makeWhammyPage (PedalCuesProcessor& p)
{
    return std::make_unique<WhammyPage> (p);
}

PedalInfo pedalInfo (const juce::ValueTree& state)
{
    PedalInfo info;
    info.channel = state::pedalChannel (state);
    info.name = (int) state[IDs::whModel] == 1 ? "Whammy DT" : "Whammy V";
    if ((int) state[IDs::fxUnit] == state::fxNone)
    {
        info.isNone = true;
        info.name = "No pedal";
        info.shortName = "pedal";
        info.colour = theme::raised.brighter (0.3f);
        return info;
    }
    if ((int) state[IDs::fxUnit] == state::fxModeller)
        if (const auto* m = modellers::find (state[IDs::fxProfile].toString()))
        {
            info.page = m;
            info.name = m->brand + " " + m->model;
            info.shortName = m->shortName;
            info.colour = m->colour;
            info.channel = state::fxModellerChannel (state);
            return info;
        }
    if (const auto u = state::fxCustomUnit (state); u.isValid())
    {
        info.isCustom = true;
        info.name = info.shortName = u[IDs::name].toString();
        info.colour = state::colourOf (u, juce::Colour (0xff8e7cf0));
        info.channel = state::channelFor (state, u);
    }
    return info;
}

namespace
{
// Effects & Pedals with no pedal picked: what the tab is for, and how to add one.
class NoPedalPage final : public Page
{
public:
    explicit NoPedalPage (juce::ValueTree s) : state (std::move (s))
    {
        title.setText ("No effect or pedal yet", juce::dontSendNotification);
        title.setFont (font (22.0f, true));
        title.setJustificationType (juce::Justification::centred);
        body.setText ("Add an effect or pedal: a Whammy, a delay, a looper or any MIDI device. Pick one, or make your own, and its "
                      "tiles appear here. Not using one? Leave this tab empty.",
                      juce::dontSendNotification);
        body.setFont (font (14.0f));
        body.setColour (juce::Label::textColourId, dim);
        body.setJustificationType (juce::Justification::centredTop);
        chooseButton.setColour (juce::TextButton::buttonColourId, accent);
        chooseButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        chooseButton.onClick = [this] { showUnitPicker (state, chooseButton, {}, true); };
        for (auto* c : std::initializer_list<juce::Component*> { &title, &body, &chooseButton })
            addAndMakeVisible (c);
    }

    void refresh() override {}

    void resized() override
    {
        auto r = getLocalBounds().withSizeKeepingCentre (juce::jmin (560, getWidth() - 40), 200);
        title.setBounds (r.removeFromTop (40));
        body.setBounds (r.removeFromTop (70));
        r.removeFromTop (12);
        chooseButton.setBounds (r.removeFromTop (40).withSizeKeepingCentre (220, 40));
    }

private:
    juce::ValueTree state;
    juce::Label title, body;
    juce::TextButton chooseButton { "Choose a pedal" };
};

// The Effects & Pedals tab: the Whammy V / DT page, a pedal page (DL4 MkII, HX One), a custom MIDI device, or nothing yet.
class PedalPage final : public Page
{
public:
    explicit PedalPage (PedalCuesProcessor& p)
        : state (p.state), whammy (makeWhammyPage (p)), custom (makeCustomPage (p, true)), none (std::make_unique<NoPedalPage> (p.state)),
          modeller (makeModellerPage (p, true))
    {
        addChildComponent (*modeller);
        addChildComponent (*whammy);
        addChildComponent (*custom);
        addChildComponent (*none);
        refresh();
    }

    void refresh() override
    {
        const auto info = pedalInfo (state);
        auto* shown = info.isNone ? none.get() : info.isCustom ? custom.get() : info.page != nullptr ? modeller.get() : whammy.get();
        for (auto* page : { whammy.get(), custom.get(), none.get(), modeller.get() })
            page->setVisible (page == shown);
        shown->refresh();
    }

    void resized() override
    {
        for (auto* page : { whammy.get(), custom.get(), none.get(), modeller.get() })
            page->setBounds (getLocalBounds());
    }

private:
    juce::ValueTree state;
    std::unique_ptr<Page> whammy, custom, none, modeller;
};
} // namespace

std::unique_ptr<Page> makePedalPage (PedalCuesProcessor& p)
{
    return std::make_unique<PedalPage> (p);
}
} // namespace ui
