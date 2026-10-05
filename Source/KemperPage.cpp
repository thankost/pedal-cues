#include "EditorCommon.h"
#include "MovesPanel.h"

using namespace theme;

namespace ui
{
namespace
{
const juce::Colour kemperLine { 0xff5fe08f };

bool isPlayer (const juce::ValueTree& state)                { return (int) state[IDs::ampUnit] == 2; }
cues::kemper::Unit unitOf (const juce::ValueTree& state)     { return isPlayer (state) ? cues::kemper::Unit::player : cues::kemper::Unit::profiler; }

// Kemper Pedals view: Wah / Pitch / Volume / Morph pedal moves, with "Set to" tiles, shapes and Draw.
class KemperPedals final : public Page
{
public:
    explicit KemperPedals (PedalCuesProcessor& p) : proc (p), state (p.state)
    {
        addAndMakeVisible (moves);
        for (int i = 0; i < cues::kemper::numPedals; ++i)
            pedalBox.addItem (cues::kemper::pedalName (i) + " pedal", i + 1);
        pedalBox.setTooltip ("Which Kemper pedal the tiles move: Wah (CC#1), Pitch (CC#4), Volume (CC#7) or Morph (CC#11). "
                             "It moves what that pedal controls in the Kemper's rig.");
        pedalBox.onChange = [this] { state.setProperty (IDs::kemperPedal, pedalBox.getSelectedId() - 1, nullptr); };
        moves.extraHeader().addAndMakeVisible (pedalBox);
        moves.extraHeader().setComponentID ("km.pedalChoice");

        styleCaption (setLabel, "SET TO");
        moves.extraRow().addAndMakeVisible (setLabel);
        refresh();
    }

    void refresh() override
    {
        const auto pedal = pedalIndex();
        pedalBox.setSelectedId (pedal + 1, juce::dontSendNotification);
        moves.setHint ("CC#" + juce::String (cues::kemper::pedalController (pedal)) + "  -  the loaded slot");

        setTiles.clear();
        static const std::pair<float, const char*> positions[] = {
            { 0.0f, "Heel" }, { 0.25f, "25%" }, { 0.5f, "Half" }, { 0.75f, "75%" }, { 1.0f, "Toe" }
        };
        for (const auto& position : positions)
        {
            const auto pos = position.first;
            const juce::String label (position.second);
            const auto value = juce::roundToInt (pos * 127.0f);
            auto* t = setTiles.add (new Tile (proc, Tile::Look::utility));
            t->title = label;
            t->subtitle = "CC#" + juce::String (cues::kemper::pedalController (pedal)) + " = " + juce::String (value);
            t->colour = kemperGreen.interpolatedWith (raised, 0.6f - 0.6f * pos);
            t->setTooltip ("Drag onto the timeline to put the " + cues::kemper::pedalName (pedal).toLowerCase() + " pedal at "
                           + label.toLowerCase() + ".");
            t->makeCue = [this, pos]
            {
                cues::Cue c;
                const auto v = juce::roundToInt (pos * 127.0f);
                c.name = "Kemper " + cues::kemper::pedalName (pedalIndex()) + " " + (v == 0 ? juce::String ("Heel") : v == 127 ? juce::String ("Toe")
                                                                                        : juce::String (juce::roundToInt (pos * 100.0f)) + "%");
                c.add (0.0, juce::MidiMessage::controllerEvent (juce::jlimit (1, 16, state::ampChannel (state)),
                                                                cues::kemper::pedalController (pedalIndex()), v));
                return c;
            };
            moves.extraRow().addAndMakeVisible (t);
        }

        moves.refresh();
        resized();
    }

    void resized() override
    {
        moves.setBounds (getLocalBounds());
        pedalBox.setBounds (moves.extraHeader().getLocalBounds());
        auto row = moves.extraRow().getLocalBounds();
        setLabel.setBounds (row.removeFromLeft (64));
        layoutGrid (setTiles, row.expanded (3, 0), setTiles.size(), 0);
    }

private:
    int pedalIndex() const { return juce::jlimit (0, cues::kemper::numPedals - 1, (int) state[IDs::kemperPedal]); }

    MovesConfig pedalMoves()
    {
        MovesConfig c;
        c.idPrefix = "km.ped";
        c.title = "Pedals";
        c.hint = "CC#1";
        c.colour = kemperGreen;
        c.line = kemperLine;
        c.beatsId = IDs::kpBeats;  c.curveId = IDs::kpCurve;  c.resetId = IDs::kpReset;
        c.drawId = IDs::kpDraw;    c.drawingId = IDs::kpDrawing;  c.drawingNameId = IDs::kpDrawingName;
        c.resetText = "Back to heel after move";
        c.resetTooltip = "After a move, put the pedal back to heel (0). Leave it off for a swell that should stay up.";
        c.padHint = "Drag here to draw a pedal move";
        c.padTooltip = "Drag to draw the pedal move: bottom = heel, top = toe. Hold Shift to snap to quarter steps.";
        c.drawnTooltip = "Your drawn pedal move. Drag onto the timeline where the move should start.";
        c.shapesTooltip = "Ready-made pedal moves";
        c.drawTooltip = "Draw your own pedal move with the mouse";
        c.tileNote = " It moves what the Kemper's rig assigns to that pedal.";
        c.numShapes = cues::qc::numExpShapes;
        c.shapeRows = 2;
        c.extraHeaderWidth = 150;
        c.extraRowHeight = 44;
        c.shapeName        = [] (int s) { return cues::qc::expShapeName ((cues::qc::ExpShape) s); };
        c.shapeDescription = [] (int s) { return cues::qc::expShapeDescription ((cues::qc::ExpShape) s); };
        c.shapeHolds       = [] (int s) { return s == (int) cues::qc::ExpShape::toe || s == (int) cues::qc::ExpShape::heel; };
        c.makeShape = [this] (int s)
        {
            return cues::qc::shapedMove ("Kemper " + cues::kemper::pedalName (pedalIndex()) + " ", state::ampChannel (state),
                                         cues::kemper::pedalController (pedalIndex()), (cues::qc::ExpShape) s,
                                         (double) state[IDs::kpBeats], (double) state[IDs::kpCurve], (bool) state[IDs::kpReset]);
        };
        c.makeDrawn = [this] (const std::vector<float>& points, const juce::String& name)
        {
            return cues::qc::drawnMove ("Kemper " + cues::kemper::pedalName (pedalIndex()) + " " + (name.isNotEmpty() ? name : juce::String ("Drawn")),
                                        state::ampChannel (state), cues::kemper::pedalController (pedalIndex()), points,
                                        (double) state[IDs::kpBeats], (bool) state[IDs::kpReset]);
        };
        return c;
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;
    juce::ComboBox pedalBox;
    juce::Label setLabel;
    juce::OwnedArray<Tile> setTiles;
    MovesPanel moves { proc, pedalMoves() };
};

//==============================================================================
class KemperPage final : public Page
{
public:
    explicit KemperPage (PedalCuesProcessor& p) : proc (p), state (p.state)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &performancesSection, &slotsSection, &effectsSection, &utilsSection })
            addAndMakeVisible (c);

        addButton.setComponentID ("km.addPerformance");
        addButton.setTooltip ("Add a performance (double-click or right-click one to edit, recolour, reorder)");
        addButton.setColour (juce::TextButton::buttonColourId, accent);
        addButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        addButton.onClick = [this] { addPerformance(); };
        addAndMakeVisible (addButton);

        search.setComponentID ("km.search");
        search.onSearch = [this] { layoutRows(); listView.setViewPosition (0, 0); };
        search.onSubmit = [this]
        {
            if (! shownRows.empty())
                state.setProperty (IDs::selectedPerformance, shownRows.front(), nullptr);
        };
        addAndMakeVisible (search);

        listView.setViewedComponent (&list, false);
        listView.setScrollBarsShown (true, false);
        listView.setScrollBarThickness (8);
        addAndMakeVisible (listView);

        loadFirstToggle.setComponentID ("km.target");
        loadFirstToggle.setColour (juce::ToggleButton::tickColourId, kemperGreen);
        loadFirstToggle.setTooltip ("On: slot tiles load their performance and slot from anywhere (bank select + Program Change). "
                                    "Off: they load that slot of the performance the Kemper has loaded (CC#50-54).");
        loadFirstToggle.onClick = [this] { state.setProperty (IDs::kemperSlotFirst, loadFirstToggle.getToggleState(), nullptr); };
        addAndMakeVisible (loadFirstToggle);

        effectOnToggle.setColour (juce::ToggleButton::tickColourId, ledGreen);
        effectOnToggle.setTooltip ("Whether effect tiles switch the module on or off");
        effectOnToggle.onClick = [this] { state.setProperty (IDs::kemperEffectOn, effectOnToggle.getToggleState(), nullptr); };
        addAndMakeVisible (effectOnToggle);
        tailsToggle.setColour (juce::ToggleButton::tickColourId, ledGreen);
        tailsToggle.setTooltip ("Delay and Reverb: let the echoes and reverb tail ring out when switched off (spillover)");
        tailsToggle.onClick = [this] { state.setProperty (IDs::kemperKeepTails, tailsToggle.getToggleState(), nullptr); };
        addAndMakeVisible (tailsToggle);

        viewChoice.setComponentID ("km.view");
        viewChoice.setInterceptsMouseClicks (false, true);
        addAndMakeVisible (viewChoice);
        for (auto* b : { &slotsViewButton, &pedalsViewButton })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4308);
            b->setColour (juce::TextButton::buttonColourId, surface);
            b->setColour (juce::TextButton::buttonOnColourId, kemperGreen);
            b->setColour (juce::TextButton::textColourOffId, dim);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            viewChoice.addAndMakeVisible (b);
        }
        slotsViewButton.setConnectedEdges (juce::Button::ConnectedOnRight);
        pedalsViewButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
        slotsViewButton.setTooltip ("Slot and effect tiles for the open performance");
        pedalsViewButton.setTooltip ("Wah, pitch, volume and morph pedal moves: swells, fades, or draw your own");
        slotsViewButton.onClick  = [this] { if (slotsViewButton.getToggleState())  state.setProperty (IDs::kemperPedalsView, false, nullptr); };
        pedalsViewButton.onClick = [this] { if (pedalsViewButton.getToggleState()) state.setProperty (IDs::kemperPedalsView, true, nullptr); };

        addChildComponent (pedals);
        refresh();
    }

    void refresh() override
    {
        const auto tree = kemperTree();
        const auto sel = selectedIndex();
        const auto perf = performance (sel);
        const auto loadFirst = (bool) state[IDs::kemperSlotFirst];
        const auto effectOn = (bool) state[IDs::kemperEffectOn];
        const auto player = isPlayer (state);

        performancesSection.title = player ? "Banks" : "Performances";
        search.setTextToShowWhenEmpty (player ? "Search banks" : "Search performances", dim);
        performancesSection.repaint();
        addButton.setButtonText (player ? "+ Bank" : "+ Perf.");
        loadFirstToggle.setButtonText ("Load " + shortName (perf) + " first");
        loadFirstToggle.setToggleState (loadFirst, juce::dontSendNotification);
        slotsSection.hint = loadFirst ? (player ? juce::String ("Program Change - loads ") + shortName (perf) + " and its slot"
                                                : "bank + Program Change - loads " + shortName (perf) + " and its slot")
                                      : juce::String ("CC#50-54 - a slot of the performance the Kemper has loaded");
        slotsSection.repaint();
        effectOnToggle.setToggleState (effectOn, juce::dontSendNotification);
        effectOnToggle.setButtonText (effectOn ? "Tiles switch ON" : "Tiles switch OFF");
        tailsToggle.setToggleState ((bool) state[IDs::kemperKeepTails], juce::dontSendNotification);

        rows.clear();
        int index = 0;
        for (auto p : tree)
        {
            if (! p.hasType (IDs::Performance))
                continue;
            auto* t = rows.add (new Tile (proc, Tile::Look::row));
            const auto i = index++;
            t->title = p[IDs::name].toString();
            t->subtitle = shortName (p);
            t->colour = state::colourOf (p);
            t->highlighted = (i == sel);
            t->setTooltip ("Click to show its slots, drag onto the timeline to load slot 1, double-click to edit.");
            t->makeCue = [this, i] { return slotCue (i, 0, true); };
            t->onClick = [this, i] { state.setProperty (IDs::selectedPerformance, i, nullptr); };
            t->onDoubleClick = [this, i] { editPerformance (performance (i)); };
            t->onContextMenu = [this, i] { performanceMenu (i); };
            list.addAndMakeVisible (t);
        }

        screen = std::make_unique<Tile> (proc, Tile::Look::screen);
        screen->setComponentID ("km.screen");
        screen->screenHeading = player ? "LOADED BANK" : "LOADED PERFORMANCE";
        screen->chipsHeading = "SLOTS";
        screen->numberedChips = true;
        screen->screenHint = "Drag this onto the timeline to load slot 1";
        screen->title = perf[IDs::name].toString();
        screen->subtitle = shortName (perf);
        screen->colour = state::colourOf (perf);
        for (int s = 0; s < cues::kemper::slotsPerPerformance; ++s)
            screen->chips.push_back (state::colourOf (nthOfType (perf, IDs::KemperSlot, s)));
        screen->makeCue = [this, sel] { return slotCue (sel, 0, true); };
        screen->onDoubleClick = [this, sel] { editPerformance (performance (sel)); };
        screen->onContextMenu = [this, sel] { performanceMenu (sel); };
        addAndMakeVisible (*screen);

        slotTiles.clear();
        for (int s = 0; s < cues::kemper::slotsPerPerformance; ++s)
        {
            const auto slot = nthOfType (perf, IDs::KemperSlot, s);
            auto* t = slotTiles.add (new Tile (proc, Tile::Look::footswitch));
            t->title = slot[IDs::name].toString();
            t->badge = juce::String (s + 1);
            t->subtitle = loadFirst ? shortName (perf) + " > Slot " + juce::String (s + 1) : "Slot " + juce::String (s + 1) + " - current";
            t->colour = state::colourOf (slot);
            t->setTooltip ("Drag onto the timeline to load this slot. Double-click to rename, right-click for colour.");
            t->makeCue = [this, sel, s] { return slotCue (sel, s, (bool) state[IDs::kemperSlotFirst]); };
            t->onDoubleClick = [slot] { renameNode (slot, "Rename slot"); };
            t->onContextMenu = [this, slot, sel, s] { nodeMenu (slot, true, [this, sel, s] { return slotCue (sel, s, (bool) state[IDs::kemperSlotFirst]); }); };
            addAndMakeVisible (t);
        }

        effectTiles.clear();
        for (int e = 0; e < cues::kemper::numEffects; ++e)
        {
            const auto node = nthOfType (tree, IDs::KemperEffect, e);
            auto* t = effectTiles.add (new Tile (proc, Tile::Look::stomp));
            t->title = node[IDs::name].toString();
            t->subtitle = shortModule (e) + (effectOn ? "  ON" : "  OFF");
            t->active = effectOn;
            t->setTooltip ("Drag to switch the " + cues::kemper::effectName (e) + " module " + (effectOn ? "on" : "off")
                           + " (CC#" + juce::String (cues::kemper::effectController (e, (bool) state[IDs::kemperKeepTails])) + "). Double-click to rename.");
            t->makeCue = [this, e, node]
            {
                return cues::kemper::effect (channel(), e, (bool) state[IDs::kemperEffectOn], (bool) state[IDs::kemperKeepTails],
                                             node[IDs::name].toString());
            };
            t->onDoubleClick = [node] { renameNode (node, "Rename effect"); };
            t->onContextMenu = [this, node] { nodeMenu (node, false, {}); };
            addAndMakeVisible (t);
        }

        utilTiles.clear();
        auto addUtil = [this] (const juce::String& title, const juce::String& sub, juce::Colour c, std::function<cues::Cue()> make)
        {
            auto* t = utilTiles.add (new Tile (proc, Tile::Look::utility));
            t->title = title;
            t->subtitle = sub;
            t->colour = c;
            t->makeCue = std::move (make);
            t->setTooltip ("Drag onto the timeline (" + sub + ")");
            addAndMakeVisible (t);
        };
        const auto violet = juce::Colour (0xff8e7cf0);
        addUtil ("Tuner On",     "CC#31 = 1", kemperGreen,            [this] { return cues::kemper::tuner (channel(), true); });
        addUtil ("Tuner Off",    "CC#31 = 0", raised.brighter (0.2f), [this] { return cues::kemper::tuner (channel(), false); });
        addUtil ("Tap x4",       "CC#30, 4 beats", accent,            [this] { return cues::kemper::tapTempo (channel(), 4); });
        addUtil ("Morph On",     "CC#80 = 1", violet,                 [this] { return cues::kemper::morph (channel(), true); });
        addUtil ("Morph Off",    "CC#80 = 0", violet.darker (0.4f),   [this] { return cues::kemper::morph (channel(), false); });
        addUtil ("Rotary Fast",  "CC#33 = 1", qcBlue,                 [this] { return cues::kemper::rotary (channel(), true); });
        addUtil ("Rotary Slow",  "CC#33 = 0", qcBlue.darker (0.4f),   [this] { return cues::kemper::rotary (channel(), false); });

        const auto showPedals = (bool) state[IDs::kemperPedalsView];
        (showPedals ? pedalsViewButton : slotsViewButton).setToggleState (true, juce::dontSendNotification);
        for (auto* c : std::initializer_list<juce::Component*> { &slotsSection, &effectsSection, &loadFirstToggle, &effectOnToggle, &tailsToggle })
            c->setVisible (! showPedals);
        for (auto* t : slotTiles)   t->setVisible (! showPedals);
        for (auto* t : effectTiles) t->setVisible (! showPedals);
        pedals.setVisible (showPedals);
        pedals.refresh();

        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);

        performancesSection.setBounds (r.removeFromLeft (juce::jlimit (230, 300, r.getWidth() / 4)));
        r.removeFromLeft (12);
        addButton.setBounds (performancesSection.headerArea().removeFromRight (86));
        {
            auto content = performancesSection.contentArea();
            search.setBounds (content.removeFromTop (32).reduced (2, 0));
            content.removeFromTop (6);
            listView.setBounds (content);
        }
        layoutRows();

        if (screen != nullptr)
            screen->setBounds (r.removeFromTop (104).expanded (3));
        r.removeFromTop (10);

        viewChoice.setBounds (r.removeFromTop (30).removeFromLeft (320));
        {
            auto c = viewChoice.getLocalBounds();
            slotsViewButton.setBounds (c.removeFromLeft (c.getWidth() / 2));
            pedalsViewButton.setBounds (c);
        }
        r.removeFromTop (10);

        utilsSection.setBounds (r.removeFromBottom (Section::headerHeight + 60));
        r.removeFromBottom (12);
        pedals.setBounds (r);
        effectsSection.setBounds (r.removeFromBottom (Section::headerHeight + 64));
        r.removeFromBottom (12);
        slotsSection.setBounds (r);

        {
            auto hdr = slotsSection.headerArea().withSizeKeepingCentre (slotsSection.headerArea().getWidth(), 28);
            loadFirstToggle.setBounds (hdr.removeFromRight (170));
        }
        {
            auto hdr = effectsSection.headerArea();
            tailsToggle.setBounds (hdr.removeFromRight (130));
            effectOnToggle.setBounds (hdr.removeFromRight (160));
        }

        layoutGrid (slotTiles, slotsSection.contentArea().expanded (3), cues::kemper::slotsPerPerformance, 0);
        layoutGrid (effectTiles, effectsSection.contentArea().expanded (3), cues::kemper::numEffects, 0);
        layoutGrid (utilTiles, utilsSection.contentArea().expanded (3), utilTiles.size(), 0);
    }

private:
    juce::ValueTree kemperTree() const { return state.getChildWithName (IDs::Kemper); }
    int channel() const                { return state::ampChannel (state); }

    int numPerformances() const
    {
        int n = 0;
        for (auto c : kemperTree())
            if (c.hasType (IDs::Performance))
                ++n;
        return n;
    }

    juce::ValueTree performance (int index) const { return nthOfType (kemperTree(), IDs::Performance, index); }

    int selectedIndex() const
    {
        return juce::jlimit (0, juce::jmax (0, numPerformances() - 1), (int) state[IDs::selectedPerformance]);
    }

    juce::String shortName (const juce::ValueTree& p) const
    {
        return (isPlayer (state) ? "Bank " : "P") + juce::String ((int) p[IDs::number]);
    }

    static juce::String shortModule (int e)
    {
        static const char* names[cues::kemper::numEffects] = { "A", "B", "C", "D", "X", "MOD", "DLY", "REV" };
        return names[juce::jlimit (0, cues::kemper::numEffects - 1, e)];
    }

    cues::Cue slotCue (int perfIndex, int s, bool fromAnywhere) const
    {
        const auto p = performance (perfIndex);
        const auto slotName = nthOfType (p, IDs::KemperSlot, s)[IDs::name].toString();
        if (! fromAnywhere)
            return cues::kemper::slotOfCurrent (channel(), s, slotName);

        auto c = cues::kemper::slot (channel(), unitOf (state), (int) p[IDs::number], s, slotName);
        c.name = "Kemper " + p[IDs::name].toString() + " > " + juce::String (s + 1) + " - " + slotName;
        return c;
    }

    void addPerformance()
    {
        auto tree = kemperTree();
        const auto count = numPerformances();
        int next = 1;
        for (auto c : tree)
            if (c.hasType (IDs::Performance))
                next = juce::jmax (next, (int) c[IDs::number] + 1);
        next = juce::jmin (next, cues::kemper::numPerformances (unitOf (state)));

        auto p = state::createPerformance ((isPlayer (state) ? "Bank " : "Performance ") + juce::String (next), next,
                                           paletteColour (count + 4));
        tree.addChild (p, count, nullptr);   // performances stay before the effect names
        state.setProperty (IDs::selectedPerformance, count, nullptr);
        editPerformance (p);
    }

    void editPerformance (juce::ValueTree p)
    {
        const auto player = isPlayer (state);
        const auto maxNumber = cues::kemper::numPerformances (unitOf (state));
        auto* w = new juce::AlertWindow (player ? "Edit bank" : "Edit performance",
                                         player ? "Name it and give its bank number on the Profiler Player (1-10)."
                                                : "Name it and give its performance number on the Kemper (1-125).",
                                         juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", p[IDs::name].toString(), "Name");
        w->addTextEditor ("number", p[IDs::number].toString(), player ? "Bank (1-10)" : "Performance (1-125)");
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([w, p, maxNumber] (int result) mutable
        {
            if (result != 1)
                return;
            const auto name = w->getTextEditorContents ("name").trim();
            if (name.isNotEmpty())
                p.setProperty (IDs::name, name, nullptr);
            p.setProperty (IDs::number, juce::jlimit (1, maxNumber, w->getTextEditorContents ("number").getIntValue()), nullptr);
        }), true);
    }

    void performanceMenu (int index)
    {
        auto p = performance (index);
        const auto count = numPerformances();

        juce::PopupMenu m;
        m.addItem (1, "Edit name / number...");
        m.addSubMenu ("Colour", colourMenu (state::colourOf (p), 100));
        m.addSeparator();
        m.addItem (2, "Duplicate");
        m.addItem (3, "Move up", index > 0);
        m.addItem (4, "Move down", index < count - 1);
        m.addItem (5, "Delete", count > 1);
        m.addSeparator();
        m.addItem (6, "Send to pedal now");

        juce::Component::SafePointer<KemperPage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, index, p] (int result) mutable
        {
            if (safe == nullptr || result == 0)
                return;
            auto tree = safe->kemperTree();
            const auto at = tree.indexOf (p);

            if (result >= 100)
                p.setProperty (IDs::colour, paletteColour (result - 100).toString(), nullptr);
            else if (result == 1)
                safe->editPerformance (p);
            else if (result == 2)
                tree.addChild (p.createCopy(), at + 1, nullptr);
            else if (result == 3)
                tree.moveChild (at, at - 1, nullptr);
            else if (result == 4)
                tree.moveChild (at, at + 1, nullptr);
            else if (result == 5)
            {
                tree.removeChild (p, nullptr);
                safe->state.setProperty (IDs::selectedPerformance, juce::jmax (0, index - 1), nullptr);
            }
            else if (result == 6)
                safe->proc.preview (safe->slotCue (index, 0, true));
        });
    }

    void nodeMenu (juce::ValueTree node, bool withColour, std::function<cues::Cue()> send)
    {
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        if (withColour)
            m.addSubMenu ("Colour", colourMenu (state::colourOf (node), 100));
        if (send)
            m.addItem (2, "Send to pedal now");

        juce::Component::SafePointer<KemperPage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, node, send] (int result) mutable
        {
            if (safe == nullptr || result == 0)
                return;
            if (result >= 100)
                node.setProperty (IDs::colour, paletteColour (result - 100).toString(), nullptr);
            else if (result == 1)
                renameNode (node, "Rename");
            else if (result == 2 && send)
                safe->proc.preview (send());
        });
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;

    Section performancesSection { "km.performances", "Performances", "" };
    Section slotsSection   { "km.slots", "Slots", "", kemperGreen };
    Section effectsSection { "km.effects", "Effects", "CC#17-29", ledGreen };
    Section utilsSection   { "km.utils", "Utilities", "tuner, tap, morph" };

    juce::TextButton addButton { "+ Perf." };
    SearchBox search { "Search performances" };
    std::vector<int> shownRows;

    void layoutRows()
    {
        shownRows = layoutFilteredRows (rows, list, listView.getWidth() - listView.getScrollBarThickness() - 2, 50, search.getText());
    }
    juce::Viewport listView;
    juce::Component list;
    juce::ToggleButton loadFirstToggle, effectOnToggle { "Tiles switch ON" }, tailsToggle { "Keep tails" };
    juce::Component viewChoice;
    juce::TextButton slotsViewButton { "Slots & Effects" }, pedalsViewButton { "Pedals" };
    KemperPedals pedals { proc };

    juce::OwnedArray<Tile> rows, slotTiles, effectTiles, utilTiles;
    std::unique_ptr<Tile> screen;
};
} // namespace

std::unique_ptr<Page> makeKemperPage (PedalCuesProcessor& p)
{
    return std::make_unique<KemperPage> (p);
}
} // namespace ui
