#include "EditorCommon.h"
#include "Modellers.h"
#include "MovesPanel.h"

using namespace theme;

namespace ui
{
namespace
{
// Which tab the page is on: the first (ampUnit 4, modellerProfile, qcChannel) or Effects & Pedals (fxUnit 3, fxProfile, its own channel).
// Each keeps its own view and expression settings, so a Helix and a DL4 side by side don't flip together.
struct Slot
{
    bool pedalsTab = false;
    juce::Identifier profileId() const { return pedalsTab ? IDs::fxProfile : IDs::modellerProfile; }
    juce::Identifier viewId() const    { return pedalsTab ? IDs::fxView : IDs::mdView; }
    juce::Identifier pedalId() const   { return pedalsTab ? IDs::fxPedal : IDs::mdPedal; }
    const modellers::Profile* profile (const juce::ValueTree& state) const { return modellers::find (state[profileId()].toString()); }
    int channel (const juce::ValueTree& state) const
    {
        return pedalsTab ? state::fxModellerChannel (state) : state::ampChannel (state);
    }
};

// Expression moves on the unit's pedal CCs (Helix EXP 1-3, Fractal External controllers), with "Set to" tiles.
class ModellerPedals final : public Page
{
public:
    ModellerPedals (PedalCuesProcessor& p, Slot s) : proc (p), state (p.state), slot (s)
    {
        addAndMakeVisible (moves);
        pedalBox.onChange = [this] { state.setProperty (slot.pedalId(), pedalBox.getSelectedId() - 1, nullptr); };
        moves.extraHeader().addAndMakeVisible (pedalBox);
        styleCaption (setLabel, "SET TO");
        moves.extraRow().addAndMakeVisible (setLabel);
        refresh();
    }

    void refresh() override
    {
        const auto* p = profile();
        pedalBox.clear (juce::dontSendNotification);
        if (p == nullptr || p->pedals.empty())
        {
            setTiles.clear();
            return;
        }
        for (int i = 0; i < (int) p->pedals.size(); ++i)
            pedalBox.addItem (p->pedals[(size_t) i].name + " (CC#" + juce::String (p->pedals[(size_t) i].cc) + ")", i + 1);
        pedalBox.setSelectedId (pedalIndex() + 1, juce::dontSendNotification);
        pedalBox.setTooltip (p->pedalNote);
        moves.setHint ("CC#" + juce::String (controller()) + "  -  the loaded preset");

        setTiles.clear();
        static const std::pair<float, const char*> positions[] = { { 0.0f, "Heel" }, { 0.25f, "25%" }, { 0.5f, "Half" }, { 0.75f, "75%" }, { 1.0f, "Toe" } };
        for (const auto& [pos, label] : positions)
        {
            const auto value = juce::roundToInt (pos * 127.0f);
            auto* t = setTiles.add (new Tile (proc, Tile::Look::utility));
            t->title = label;
            t->subtitle = "CC#" + juce::String (controller()) + " = " + juce::String (value);
            t->colour = p->colour.interpolatedWith (raised, 0.6f - 0.6f * pos);
            t->setTooltip ("Puts " + p->pedals[(size_t) pedalIndex()].name + " at " + juce::String (label).toLowerCase() + ". " + p->pedalNote);
            const auto text = juce::String (label);
            t->makeCue = [this, value, text]
            {
                cues::Cue c;
                c.name = pedalLabel() + " " + text;
                c.add (0.0, juce::MidiMessage::controllerEvent (slot.channel (state), controller(), value));
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
    const modellers::Profile* profile() const { return slot.profile (state); }
    int pedalIndex() const
    {
        const auto* p = profile();
        return p == nullptr ? 0 : juce::jlimit (0, juce::jmax (0, (int) p->pedals.size() - 1), (int) state[slot.pedalId()]);
    }
    int controller() const
    {
        const auto* p = profile();
        return p == nullptr || p->pedals.empty() ? 1 : p->pedals[(size_t) pedalIndex()].cc;
    }

    // "Helix Floor EXP 1"; units without pedals (older HeadRush) never show this panel.
    juce::String pedalLabel() const
    {
        const auto* p = profile();
        return p != nullptr && ! p->pedals.empty() ? p->shortName + " " + p->pedals[(size_t) pedalIndex()].name : juce::String ("Pedal");
    }

    MovesConfig config()
    {
        MovesConfig c;
        c.idPrefix = slot.pedalsTab ? "fx.ped" : "md.ped";
        c.title = "Expression";
        c.hint = "CC#1";
        c.colour = qcBlue;
        c.line = qcBlue.brighter (0.3f);
        if (slot.pedalsTab)
        {
            c.beatsId = IDs::fxBeats;  c.curveId = IDs::fxCurve;  c.resetId = IDs::fxReset;
            c.drawId = IDs::fxDraw;    c.drawingId = IDs::fxDrawing;  c.drawingNameId = IDs::fxDrawingName;
        }
        else
        {
            c.beatsId = IDs::mdBeats;  c.curveId = IDs::mdCurve;  c.resetId = IDs::mdReset;
            c.drawId = IDs::mdDraw;    c.drawingId = IDs::mdDrawing;  c.drawingNameId = IDs::mdDrawingName;
        }
        c.resetText = "Back to heel after move";
        c.resetTooltip = "After a move, put the pedal back to heel (0). Leave it off for a swell that should stay up.";
        c.padHint = "Drag here to draw a pedal move";
        c.padTooltip = "Drag to draw the pedal move: bottom = heel, top = toe. Hold Shift to snap to quarter steps.";
        c.drawnTooltip = "Your drawn pedal move. Drag onto the timeline where the move should start.";
        c.shapesTooltip = "Ready-made pedal moves";
        c.drawTooltip = "Draw your own pedal move with the mouse";
        c.tileNote = " It moves what the preset assigns to this pedal.";
        c.numShapes = cues::qc::numExpShapes;
        c.shapeRows = 2;
        c.extraHeaderWidth = 170;
        c.extraRowHeight = 44;
        c.shapeName        = [] (int s) { return cues::qc::expShapeName ((cues::qc::ExpShape) s); };
        c.shapeDescription = [] (int s) { return cues::qc::expShapeDescription ((cues::qc::ExpShape) s); };
        c.shapeHolds       = [] (int s) { return s == (int) cues::qc::ExpShape::toe || s == (int) cues::qc::ExpShape::heel; };
        const auto beats = c.beatsId, curve = c.curveId, reset = c.resetId;
        c.makeShape = [this, beats, curve, reset] (int s)
        {
            return cues::qc::shapedMove (pedalLabel() + " ",
                                         slot.channel (state), controller(), (cues::qc::ExpShape) s,
                                         (double) state[beats], (double) state[curve], (bool) state[reset]);
        };
        c.makeDrawn = [this, beats, reset] (const std::vector<float>& points, const juce::String& name)
        {
            return cues::qc::drawnMove (pedalLabel() + " "
                                            + (name.isNotEmpty() ? name : juce::String ("Drawn")),
                                        slot.channel (state), controller(), points, (double) state[beats], (bool) state[reset]);
        };
        return c;
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;
    Slot slot;
    juce::ComboBox pedalBox;
    juce::Label setLabel;
    juce::OwnedArray<Tile> setTiles;
    MovesPanel moves { proc, config() };
};

//==============================================================================
// A Fractal, Line 6 or HeadRush unit with defined MIDI numbers: presets, scenes / snapshots, footswitches or blocks, utilities,
// looper and expression, laid out like the Quad Cortex page and driven by its modellers::Profile.
class ModellerPage final : public Page
{
public:
    ModellerPage (PedalCuesProcessor& p, Slot s) : proc (p), state (p.state), slot (s)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &presetsSection, &scenesSection, &switchesSection, &utilsSection, &looperSection })
            addChildComponent (c);
        for (int g = 0; g < 3; ++g)
            addChildComponent (modelSections.add (new Section ("md.models" + juce::String (g), "Models", "")));
        presetsSection.setVisible (true);

        addButton.setColour (juce::TextButton::buttonColourId, accent);
        addButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        addButton.setTooltip ("Add a preset (double-click or right-click one to edit, recolour, reorder)");
        addButton.onClick = [this] { addPreset(); };
        addAndMakeVisible (addButton);

        search.onSearch = [this] { layoutPresets(); presetView.setViewPosition (0, 0); };
        search.onSubmit = [this] { if (! shown.empty()) select (shown.front()); };
        addAndMakeVisible (search);

        presetView.setViewedComponent (&presetList, false);
        presetView.setScrollBarsShown (true, false);
        presetView.setScrollBarThickness (8);
        addAndMakeVisible (presetView);

        setlistToggle.setButtonText ("Switch to the preset's setlist");
        setlistToggle.onClick = [this] { state.setProperty (IDs::mdSendSetlist, setlistToggle.getToggleState(), nullptr); };
        addChildComponent (setlistToggle);
        setlistWarning.setButtonText ("Setlists not sent: turn on");
        setlistWarning.setColour (juce::TextButton::buttonColourId, accent.withAlpha (0.18f));
        setlistWarning.setColour (juce::TextButton::textColourOffId, accent);
        setlistWarning.onClick = [this] { state.setProperty (IDs::mdSendSetlist, true, nullptr); };
        addChildComponent (setlistWarning);

        loadFirstToggle.onClick = [this] { state.setProperty (IDs::mdLoadFirst, loadFirstToggle.getToggleState(), nullptr); };
        addChildComponent (loadFirstToggle);
        switchOnToggle.onClick = [this] { state.setProperty (IDs::mdSwitchOn, switchOnToggle.getToggleState(), nullptr); };
        addChildComponent (switchOnToggle);

        viewChoice.setInterceptsMouseClicks (false, true);
        addAndMakeVisible (viewChoice);
        const char* views[] = { "Scenes & Switches", "Looper", "Expression", "Models" };   // the first two follow the profile (refresh)
        for (int i = 0; i < 4; ++i)
        {
            auto* b = viewButtons.add (new juce::TextButton (views[i]));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4510);
            b->setColour (juce::TextButton::buttonColourId, surface);
            b->setColour (juce::TextButton::textColourOffId, dim);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            b->onClick = [this, i] { state.setProperty (slot.viewId(), i, nullptr); };
            viewChoice.addAndMakeVisible (b);
        }

        // Built from the manual: say so, and where the details are.
        disclaimer.setFont (font (12.0f, true));
        disclaimer.setColour (juce::Label::textColourId, accent);
        disclaimer.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (disclaimer);
        aboutButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        aboutButton.setColour (juce::TextButton::textColourOffId, qcBlue);
        aboutButton.onClick = [this] { showAbout(); };
        addAndMakeVisible (aboutButton);

        addChildComponent (pedals);
        refresh();
    }

    void refresh() override
    {
        const auto* p = profile();
        if (p == nullptr)
            return;
        auto root = state;
        const auto data = state::modeller (root, p->id);
        const auto sel = selectedIndex();
        const auto preset = modPreset (sel);
        const auto loadFirst = (bool) state[IDs::mdLoadFirst];
        const auto switchOn = (bool) state[IDs::mdSwitchOn];
        const auto view = viewFor (*p);
        const auto setlists = modellers::hasSetlists (*p);
        const auto label = presetLabel (preset);

        for (auto* b : viewButtons)
            b->setColour (juce::TextButton::buttonOnColourId, p->colour);
        viewButtons[view]->setToggleState (true, juce::dontSendNotification);
        viewButtons[1]->setEnabled (! p->looper.empty());   // Nano Cortex: no looper over MIDI
        viewButtons[1]->setButtonText (p->looperTitle);     // Infinity 500 Combo: IR slots
        looperSection.title = p->looperTitle;
        viewButtons[2]->setEnabled (! p->pedals.empty());
        viewButtons[0]->setButtonText (p->mainTitle);       // DL4 MkII: "Controls"
        viewButtons[3]->setVisible (! p->models.empty());   // DL4 MkII: delay and reverb models
        {
            int shownButtons = 0;
            for (auto* b : viewButtons)
                shownButtons += b->isVisible() ? 1 : 0;
            int i = 0;
            for (auto* b : viewButtons)
                if (b->isVisible())
                {
                    b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < shownButtons - 1 ? juce::Button::ConnectedOnRight : 0));
                    ++i;
                }
        }

        disclaimer.setText (p->beta ? "From the " + p->brand + " manual, not tested on hardware" : juce::String ("Not tested on hardware"),
                            juce::dontSendNotification);

        // Setlists: the same rule as the Quad Cortex. Off by default; a reminder when presets span several setlists.
        juce::SortedSet<int> usedSetlists;
        for (auto c : data)
            if (c.hasType (IDs::ModPreset))
                usedSetlists.add ((int) c[IDs::setlist]);
        const auto sendSetlist = (bool) state[IDs::mdSendSetlist];
        setlistToggle.setVisible (setlists);
        setlistToggle.setToggleState (sendSetlist, juce::dontSendNotification);
        setlistToggle.setTooltip (setlistSwitchTooltip (p->shortName));
        setlistWarning.setVisible (setlists && usedSetlists.size() > 1 && ! sendSetlist);
        setlistWarning.setTooltip ("Your presets are in more than one setlist, but they don't select their setlist, so a preset loads in whatever "
                                   "setlist the unit is on. Click to turn it on, then drag preset clips you made before into your DAW again.");

        presetsSection.accentColour = p->colour;
        scenesSection.title = p->sceneWord + "s";
        scenesSection.accentColour = p->colour;
        scenesSection.hint = modellers::sceneCcs (*p) + "  -  "
                           + (loadFirst ? p->sceneWord.toLowerCase() + "s load " + label + " first" : "on the loaded preset");
        loadFirstToggle.setButtonText ("Load " + label + " first");
        loadFirstToggle.setToggleState (loadFirst, juce::dontSendNotification);
        loadFirstToggle.setTooltip ("On: " + p->sceneWord.toLowerCase() + " tiles load this preset first" + (p->switchesOnOff ? " (and so do block tiles)" : "")
                                    + ", so they work whatever preset the unit is on. Off: they act on the preset that's loaded. " + p->sceneNote);
        switchesSection.title = p->switchesTitle;
        switchesSection.hint = switchesHint (*p, loadFirst, label);
        switchOnToggle.setToggleState (switchOn, juce::dontSendNotification);
        switchOnToggle.setButtonText (switchOn ? "Tiles switch ON" : "Tiles switch OFF");
        switchOnToggle.setTooltip ("Whether block tiles switch the block on or off");
        for (auto* s : { &presetsSection, &scenesSection, &switchesSection })
            s->repaint();

        presetTiles.clear();
        int i = 0;
        for (auto c : data)
        {
            if (! c.hasType (IDs::ModPreset))
                continue;
            const auto index = i++;
            auto* t = presetTiles.add (new Tile (proc, Tile::Look::row));
            t->title = c[IDs::name].toString();
            t->subtitle = (setlists ? shortSetlist (c) + " | " : juce::String()) + presetLabel (c);
            t->colour = state::colourOf (c);
            t->highlighted = index == sel;
            t->setTooltip ("Click to show its " + p->sceneWord.toLowerCase() + "s, drag onto the timeline to load it, double-click to edit.");
            t->makeCue = [this, index] { return presetCue (index); };
            t->onClick = [this, index] { select (index); };
            t->onDoubleClick = [this, index] { editPreset (modPreset (index)); };
            t->onContextMenu = [this, index] { presetMenu (index); };
            presetList.addAndMakeVisible (t);
        }

        screen = std::make_unique<Tile> (proc, Tile::Look::screen);
        screen->screenHeading = "LOADED PRESET";
        screen->chipsHeading = p->sceneWord.toUpperCase() + "S";
        screen->numberedChips = ! p->sceneLetters;
        screen->title = preset[IDs::name].toString();
        screen->subtitle = (setlists ? shortSetlist (preset) + " | " : juce::String()) + label;
        screen->colour = state::colourOf (preset);
        for (int s = 0; s < p->sceneCount; ++s)
            screen->chips.push_back (state::colourOf (nthOfType (preset, IDs::Scene, s)));
        screen->makeCue = [this, sel] { return presetCue (sel); };
        screen->onDoubleClick = [this, sel] { editPreset (modPreset (sel)); };
        screen->onContextMenu = [this, sel] { presetMenu (sel); };
        addAndMakeVisible (*screen);

        sceneTiles.clear();
        for (int s = 0; s < p->sceneCount; ++s)
        {
            const auto scene = nthOfType (preset, IDs::Scene, s);
            auto* t = sceneTiles.add (new Tile (proc, Tile::Look::footswitch));
            t->title = scene[IDs::name].toString();
            const auto sl = modellers::sceneLabel (*p, s);
            t->badge = sl;
            t->subtitle = loadFirst ? label + " > " + p->sceneWord + " " + sl : p->sceneWord + " " + sl + " - current";
            t->colour = state::colourOf (scene);
            t->setTooltip ("Drag onto the timeline to switch to this " + p->sceneWord.toLowerCase() + ". Double-click to rename, right-click for colour.");
            t->makeCue = [this, sel, s] { return sceneCue (sel, s); };
            t->onDoubleClick = [scene] { renameNode (scene, "Rename"); };
            t->onContextMenu = [this, scene, sel, s] { nodeMenu (scene, true, [this, sel, s] { return sceneCue (sel, s); }); };
            addChildComponent (t);
        }

        switchTiles.clear();
        for (int s = 0; s < (int) p->switches.size(); ++s)
        {
            const auto node = nthOfType (data, IDs::ModSwitch, s);
            const auto& control = p->switches[(size_t) s];
            auto* t = switchTiles.add (new Tile (proc, Tile::Look::stomp));
            t->title = node[IDs::name].toString();
            const auto unusualValues = control.value != 127 || control.offValue != 0;   // Darkglass amps: ON sends 0 or 1
            t->subtitle = p->switchesOnOff ? "CC#" + juce::String (control.cc)
                                               + (unusualValues ? " = " + juce::String (switchOn ? control.value : control.offValue) : juce::String())
                                               + (switchOn ? "  ON" : "  OFF")
                                           : "CC#" + juce::String (control.cc) + (control.value != 127 ? " = " + juce::String (control.value) : juce::String());
            t->active = ! p->switchesOnOff || switchOn;
            t->setTooltip (p->switchesNote + " Double-click to rename.");
            t->makeCue = [this, sel, s] { return switchCue (sel, s); };
            t->onDoubleClick = [node] { renameNode (node, "Rename"); };
            t->onContextMenu = [this, node, sel, s] { nodeMenu (node, false, [this, sel, s] { return switchCue (sel, s); }); };
            addChildComponent (t);
        }

        auto makeActions = [this, p] (juce::OwnedArray<Tile>& tiles, const std::vector<modellers::Action>& actions)
        {
            tiles.clear();
            const auto& palette = state::palette();
            for (const auto& a : actions)
            {
                auto* t = tiles.add (new Tile (proc, Tile::Look::utility));
                t->title = a.name;
                t->subtitle = cues::custom::describe (cues::custom::parse (a.messages, 0), 0);
                t->colour = juce::Colour (palette.getReference (a.colour % palette.size()).argb);
                t->setTooltip ((a.note.isNotEmpty() ? a.note + "\n" : juce::String()) + "Drag onto the timeline (" + t->subtitle + ").");
                t->makeCue = [this, a] { return modellers::action (*profile(), channel(), a); };
                addChildComponent (t);
            }
        };
        makeActions (utilTiles, p->utilities);
        makeActions (looperTiles, p->looper);
        for (int g = 0; g < modelTiles.size(); ++g)
            modelTiles[g]->clear();
        modelTiles.clear();
        for (int g = 0; g < (int) p->models.size() && g < modelSections.size(); ++g)
        {
            const auto& group = p->models[(size_t) g];
            modelSections[g]->title = group.title;
            modelSections[g]->hint = group.hint + "  -  the loaded preset";
            modelSections[g]->accentColour = p->colour;
            makeActions (*modelTiles.add (new juce::OwnedArray<Tile>()), group.actions);
        }

        const auto scenesView = view == 0, looperView = view == 1, pedalView = view == 2 && ! p->pedals.empty(), modelsView = view == 3;
        const auto hasScenes = p->sceneCount > 0;   // HeadRush Pedalboard, Gigboard, MX5: no scenes over MIDI
        for (auto* c : std::initializer_list<juce::Component*> { &scenesSection, &switchesSection, &loadFirstToggle })
            c->setVisible (scenesView && (hasScenes || c == &switchesSection));
        utilsSection.setVisible (scenesView || looperView);
        switchesSection.setVisible (scenesView && ! p->switches.empty());
        switchOnToggle.setVisible (scenesView && p->switchesOnOff);
        looperSection.setVisible (looperView);
        for (auto* t : sceneTiles)  t->setVisible (scenesView);
        for (auto* t : switchTiles) t->setVisible (scenesView);
        for (auto* t : utilTiles)   t->setVisible (scenesView || looperView);
        for (auto* t : looperTiles) t->setVisible (looperView);
        for (int g = 0; g < modelSections.size(); ++g)
            modelSections[g]->setVisible (modelsView && g < modelTiles.size());
        for (auto* tiles : modelTiles)
            for (auto* t : *tiles)
                t->setVisible (modelsView);
        pedals.setVisible (pedalView);
        if (pedalView)
            pedals.refresh();

        resized();
    }

    void resized() override
    {
        const auto* p = profile();
        if (p == nullptr)
            return;
        auto r = getLocalBounds().reduced (14);

        presetsSection.setBounds (r.removeFromLeft (juce::jlimit (230, 300, r.getWidth() / 4)));
        r.removeFromLeft (12);
        addButton.setBounds (presetsSection.headerArea().removeFromRight (86));
        {
            auto content = presetsSection.contentArea();
            if (setlistWarning.isVisible())
            {
                setlistWarning.setBounds (content.removeFromBottom (30).reduced (2, 0));
                content.removeFromBottom (6);
            }
            if (setlistToggle.isVisible())
            {
                setlistToggle.setBounds (content.removeFromBottom (30).reduced (2, 0));
                content.removeFromBottom (4);
            }
            search.setBounds (content.removeFromTop (32).reduced (2, 0));
            content.removeFromTop (6);
            presetView.setBounds (content);
        }
        layoutPresets();

        if (screen != nullptr)
            screen->setBounds (r.removeFromTop (104).expanded (3));
        r.removeFromTop (10);

        {
            auto row = r.removeFromTop (30);
            int shownButtons = 0;
            for (auto* b : viewButtons)
                shownButtons += b->isVisible() ? 1 : 0;
            const auto buttonWidth = shownButtons > 3 ? 116 : 140;
            viewChoice.setBounds (row.removeFromLeft (buttonWidth * shownButtons));
            auto c = viewChoice.getLocalBounds();
            for (auto* b : viewButtons)
                if (b->isVisible())
                    b->setBounds (c.removeFromLeft (buttonWidth));
            aboutButton.setBounds (row.removeFromRight (140));
            disclaimer.setBounds (row);
            // Narrow windows: the short form, so it's never cut off.
            // Not beta (Nano Cortex): only that it isn't tested yet.
            const auto full = p->beta ? "From the " + p->brand + " manual, not tested on hardware" : juce::String ("Not tested on hardware");
            const juce::String shortForm ("Not tested on hardware");
            const auto room = (float) row.getWidth() - 8.0f;
            disclaimer.setText (juce::GlyphArrangement::getStringWidth (disclaimer.getFont(), full) < room ? full
                                : juce::GlyphArrangement::getStringWidth (disclaimer.getFont(), shortForm) < room ? shortForm : juce::String(),
                                juce::dontSendNotification);
            aboutButton.setButtonText (disclaimer.getText().isEmpty() ? "Not tested: about >" : "About this unit >");
            disclaimer.setTooltip ("Built from the " + p->manual + ". Not tested on hardware yet: check the numbers on your unit (About this unit).");
        }
        r.removeFromTop (10);

        const auto view = viewFor (*p);
        if (view == 2)
        {
            pedals.setBounds (r);
            return;
        }
        if (view == 3)
        {
            // The model groups stacked, each as tall as its rows of six.
            constexpr int columns = 6;
            int rows = 0;
            for (auto* tiles : modelTiles)
                rows += (tiles->size() + columns - 1) / columns;
            const auto gaps = 10 * juce::jmax (0, modelTiles.size() - 1);
            const auto rowHeight = juce::jlimit (34, 60, (r.getHeight() - gaps - Section::headerHeight * modelTiles.size()) / juce::jmax (1, rows));
            for (int g = 0; g < modelTiles.size(); ++g)
            {
                const auto groupRows = (modelTiles[g]->size() + columns - 1) / columns;
                modelSections[g]->setBounds (r.removeFromTop (Section::headerHeight + rowHeight * groupRows));
                r.removeFromTop (10);
                layoutGrid (*modelTiles[g], modelSections[g]->contentArea().expanded (3), columns, 6);
            }
            return;
        }

        // Up to six utilities in a row; more wrap into two rows so the names stay readable.
        const auto utilCount = (int) p->utilities.size();
        const auto utilColumns = utilCount <= 6 ? juce::jmax (4, utilCount) : (utilCount + 1) / 2;   // at least four wide: one Tap isn't a banner
        const auto utilRows = (utilCount + utilColumns - 1) / utilColumns;
        utilsSection.setBounds (r.removeFromBottom (Section::headerHeight + 60 * utilRows));
        r.removeFromBottom (12);
        layoutGrid (utilTiles, utilsSection.contentArea().expanded (3), utilColumns, 6);

        if (view == 1)
        {
            looperSection.setBounds (r);
            layoutGrid (looperTiles, looperSection.contentArea().expanded (3), 4, 6);
            return;
        }

        if (! p->switches.empty())
        {
            // Five switches a row; in a short window (the channel reminder, 14 HeadRush blocks) seven a row, then
            // lower rows, so the scenes keep room for their names.
            const auto count = (int) p->switches.size();
            const auto sceneRows = p->sceneCount > 6 ? 2 : p->sceneCount > 0 ? 1 : 0;
            const auto sceneRoom = sceneRows > 0 ? Section::headerHeight + sceneRows * 62 : 0;
            int columns = juce::jmin (5, count), rowHeight = 64;
            auto height = [&] { return Section::headerHeight + rowHeight * ((count + columns - 1) / columns); };
            if (r.getHeight() - height() - 12 < sceneRoom)
                columns = juce::jmin (7, count);
            if (r.getHeight() - height() - 12 < sceneRoom)
                rowHeight = 52;
            switchesSection.setBounds (p->sceneCount > 0 ? r.removeFromBottom (height()) : r.removeFromTop (height()));
            r.removeFromBottom (12);
            layoutGrid (switchTiles, switchesSection.contentArea().expanded (3), columns, 6);
            if (switchOnToggle.isVisible())
                switchOnToggle.setBounds (switchesSection.headerArea().removeFromRight (160));
        }
        scenesSection.setBounds (r);
        {
            auto hdr = scenesSection.headerArea().withSizeKeepingCentre (scenesSection.headerArea().getWidth(), 28);
            loadFirstToggle.setBounds (hdr.removeFromRight (190));
        }
        // Up to six scenes in one row; more in two (8 as two rows of four, like the units' displays).
        const auto columns = p->sceneCount > 6 ? (p->sceneCount + 1) / 2 : p->sceneCount;
        layoutGrid (sceneTiles, scenesSection.contentArea().expanded (3), juce::jmax (1, columns), 0);
    }

private:
    const modellers::Profile* profile() const { return slot.profile (state); }
    int channel() const { return slot.channel (state); }

    // The saved view, or Scenes & Switches when the unit has no looper / pedals / models for it.
    int viewFor (const modellers::Profile& p) const
    {
        const auto view = juce::jlimit (0, 3, (int) state[slot.viewId()]);
        return (view == 1 && p.looper.empty()) || (view == 2 && p.pedals.empty()) || (view == 3 && p.models.empty()) ? 0 : view;
    }

    juce::ValueTree data() const
    {
        auto root = state;
        return profile() != nullptr ? state::modeller (root, profile()->id) : juce::ValueTree();
    }

    int numPresets() const
    {
        int n = 0;
        for (auto c : data())
            n += c.hasType (IDs::ModPreset) ? 1 : 0;
        return n;
    }

    juce::ValueTree modPreset (int i) const { return nthOfType (data(), IDs::ModPreset, i); }
    int selectedIndex() const { return juce::jlimit (0, juce::jmax (0, numPresets() - 1), (int) data()[IDs::selectedPreset]); }
    void select (int i) { data().setProperty (IDs::selectedPreset, i, nullptr); }

    juce::String presetLabel (const juce::ValueTree& p) const
    {
        return modellers::presetLabel (*profile(), (int) p[IDs::setlist], (int) p[IDs::presetIndex]);
    }

    juce::String shortSetlist (const juce::ValueTree& p) const
    {
        return modellers::setlistLabel (*profile(), (int) p[IDs::setlist]);
    }

    static juce::String switchesHint (const modellers::Profile& p, bool loadFirst, const juce::String& label)
    {
        if (p.switchesOnOff)
            return loadFirst ? "after loading " + label : juce::String ("on the loaded preset");
        if (p.switches.empty())
            return {};
        return "CC#" + juce::String (p.switches.front().cc) + (p.switches.size() > 1 && p.switches.back().cc != p.switches.front().cc
                                                                   ? "-" + juce::String (p.switches.back().cc) : juce::String())
             + "  -  on the loaded preset";
    }

    cues::Cue presetCue (int i) const
    {
        const auto p = modPreset (i);
        return modellers::preset (*profile(), channel(), (int) p[IDs::setlist], (int) p[IDs::presetIndex],
                                  (bool) state[IDs::mdSendSetlist], p[IDs::name].toString());
    }

    cues::Cue sceneCue (int presetIndex, int s) const
    {
        const auto p = modPreset (presetIndex);
        const auto name = nthOfType (p, IDs::Scene, s)[IDs::name].toString();
        if (! (bool) state[IDs::mdLoadFirst])
            return modellers::scene (*profile(), channel(), s, name);
        return modellers::sceneAfterPreset (*profile(), channel(), (int) p[IDs::setlist], (int) p[IDs::presetIndex],
                                            (bool) state[IDs::mdSendSetlist], p[IDs::name].toString(), s, name);
    }

    // Line 6 footswitches always act on the loaded preset (no load-first: whether a press waits for a preset load isn't documented).
    cues::Cue switchCue (int presetIndex, int s) const
    {
        const auto& prof = *profile();
        const auto name = nthOfType (data(), IDs::ModSwitch, s)[IDs::name].toString();
        const auto on = (bool) state[IDs::mdSwitchOn];
        if (! prof.switchesOnOff || ! (bool) state[IDs::mdLoadFirst])
            return modellers::switchCue (prof, channel(), s, on, name);
        const auto p = modPreset (presetIndex);
        return modellers::switchAfterPreset (prof, channel(), (int) p[IDs::setlist], (int) p[IDs::presetIndex],
                                             (bool) state[IDs::mdSendSetlist], p[IDs::name].toString(), s, on, name);
    }

    void layoutPresets()
    {
        shown = layoutFilteredRows (presetTiles, presetList, presetView.getWidth() - presetView.getScrollBarThickness() - 2, 50, search.getText());
    }

    void addPreset()
    {
        const auto& p = *profile();
        auto d = data();
        auto setlist = modellers::defaultSetlist (p), index = 0;
        if (numPresets() > 0)
        {
            const auto last = modPreset (numPresets() - 1);
            setlist = (int) last[IDs::setlist];
            index = juce::jmin ((int) last[IDs::presetIndex] + 1, modellers::presetsPerSetlist (p, setlist) - 1);
        }
        auto preset = state::createModPreset (p.id, "Preset " + modellers::presetLabel (p, setlist, index), setlist, index, paletteColour (numPresets()));
        d.appendChild (preset, nullptr);
        select (numPresets() - 1);
        editPreset (preset);
    }

    // Name, setlist, bank and slot exactly as the unit shows them.
    void editPreset (juce::ValueTree preset)
    {
        const auto& p = *profile();
        const auto setlist = (int) preset[IDs::setlist];
        const auto per = modellers::slotsPerBank (p);
        auto* w = new juce::AlertWindow ("Edit preset", "Name it, and tell PedalCues where it is on your " + p.shortName + " (as the unit shows it).",
                                         juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", preset[IDs::name].toString(), "Name");
        if (modellers::hasSetlists (p))
        {
            w->addComboBox ("setlist", modellers::setlistNames (p), "Setlist");
            w->getComboBoxComponent ("setlist")->setSelectedItemIndex (setlist, juce::dontSendNotification);
        }
        if (modellers::bankNames (p, setlist).size() > 1)   // HeadRush: one list of MIDI PROG numbers
        {
            w->addComboBox ("bank", modellers::bankNames (p, setlist), "Bank");
            w->getComboBoxComponent ("bank")->setSelectedItemIndex ((int) preset[IDs::presetIndex] / per, juce::dontSendNotification);
        }
        w->addComboBox ("slot", modellers::slotNames (p), modellers::slotTitle (p));
        w->getComboBoxComponent ("slot")->setSelectedItemIndex ((int) preset[IDs::presetIndex] % per, juce::dontSendNotification);
        w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        const auto id = p.id;
        w->enterModalState (true, juce::ModalCallbackFunction::create ([w, preset, id, per] (int result) mutable
        {
            const auto* prof = modellers::find (id);
            if (result != 1 || prof == nullptr)
                return;
            const auto name = w->getTextEditorContents ("name").trim();
            if (name.isNotEmpty())
                preset.setProperty (IDs::name, name, nullptr);
            auto newSetlist = (int) preset[IDs::setlist];
            if (auto* s = w->getComboBoxComponent ("setlist"))
                newSetlist = juce::jmax (0, s->getSelectedItemIndex());
            const auto* bank = w->getComboBoxComponent ("bank");
            const auto index = (bank != nullptr ? juce::jmax (0, bank->getSelectedItemIndex()) : 0) * per
                             + juce::jmax (0, w->getComboBoxComponent ("slot")->getSelectedItemIndex());
            preset.setProperty (IDs::setlist, newSetlist, nullptr);
            preset.setProperty (IDs::presetIndex, juce::jlimit (0, modellers::presetsPerSetlist (*prof, newSetlist) - 1, index), nullptr);
        }), true);
    }

    void presetMenu (int index)
    {
        auto d = data();
        auto preset = modPreset (index);
        juce::PopupMenu m;
        m.addItem (1, "Edit name / location...");
        m.addSubMenu ("Colour", colourMenu (state::colourOf (preset), 100));
        if (profile()->sceneCount > 0)
            m.addItem (7, "Apply colour to all its " + profile()->sceneWord.toLowerCase() + "s");
        m.addSeparator();
        m.addItem (2, "Duplicate");
        m.addItem (3, "Move up", index > 0);
        m.addItem (4, "Move down", index < numPresets() - 1);
        m.addItem (5, "Delete", numPresets() > 1);
        m.addSeparator();
        m.addItem (6, "Send to pedal now");

        juce::Component::SafePointer<ModellerPage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, index, preset, d] (int result) mutable
        {
            if (safe == nullptr || result == 0)
                return;
            const auto at = d.indexOf (preset);
            if (result >= 100)
                preset.setProperty (IDs::colour, paletteColour (result - 100).toString(), nullptr);
            else if (result == 1)
                safe->editPreset (preset);
            else if (result == 2)
                d.addChild (preset.createCopy(), at + 1, nullptr);
            else if (result == 3)
                d.moveChild (at, at - 1, nullptr);
            else if (result == 4)
                d.moveChild (at, at + 1, nullptr);
            else if (result == 5)
            {
                d.removeChild (preset, nullptr);
                safe->select (juce::jmax (0, index - 1));
            }
            else if (result == 6)
                safe->proc.preview (safe->presetCue (index));
            else if (result == 7)
                for (auto child : preset)
                    if (child.hasType (IDs::Scene))
                        child.setProperty (IDs::colour, preset[IDs::colour], nullptr);
        });
    }

    void nodeMenu (juce::ValueTree node, bool withColour, std::function<cues::Cue()> send)
    {
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        if (withColour)
            m.addSubMenu ("Colour", colourMenu (state::colourOf (node), 100));
        m.addItem (2, "Send to pedal now");
        juce::Component::SafePointer<ModellerPage> safe (this);
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

    void showAbout()
    {
        const auto& p = *profile();
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, p.model,
                                                "Built from the " + p.manual + ". Not tested on hardware yet: check the numbers on your unit, "
                                                "and please tell us what works.\n\n" + p.notes + "\n\nMIDI channel: " + p.channelHint);
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;
    Slot slot;

    Section presetsSection  { "md.presetList", "Presets", "click to open" };
    Section scenesSection   { "md.scenes", "Scenes", "" };
    Section switchesSection { "md.switches", "Footswitches", "", ledGreen };
    Section utilsSection    { "md.utils", "Utilities", "" };
    Section looperSection   { "md.looper", "Looper", "on the loaded preset", accent };

    juce::TextButton addButton { "+ Preset" };
    SearchBox search { "Search presets" };
    std::vector<int> shown;
    juce::Viewport presetView;
    juce::Component presetList;
    juce::ToggleButton setlistToggle;
    juce::TextButton setlistWarning;
    juce::ToggleButton loadFirstToggle, switchOnToggle;
    juce::Component viewChoice;
    juce::OwnedArray<juce::TextButton> viewButtons;
    juce::Label disclaimer;
    juce::TextButton aboutButton;
    ModellerPedals pedals { proc, slot };
    juce::OwnedArray<Section> modelSections;

    juce::OwnedArray<Tile> presetTiles, sceneTiles, switchTiles, utilTiles, looperTiles;
    juce::OwnedArray<juce::OwnedArray<Tile>> modelTiles;   // one list per Models group
    std::unique_ptr<Tile> screen;
};
} // namespace

std::unique_ptr<Page> makeModellerPage (PedalCuesProcessor& p, bool pedalsTab)
{
    return std::make_unique<ModellerPage> (p, Slot { pedalsTab });
}
} // namespace ui
