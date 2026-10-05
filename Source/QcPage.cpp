#include "EditorCommon.h"
#include "Modellers.h"

using namespace theme;

namespace ui
{
namespace
{
class QcPage final : public Page
{
public:
    explicit QcPage (PedalCuesProcessor& p) : proc (p), state (p.state)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &presetsSection, &scenesSection, &stompsSection, &utilsSection })
            addAndMakeVisible (c);

        // QC Mini: each row of scenes, and each half of the stomps, is one footswitch page.
        for (int i = 0; i < 4; ++i)
        {
            auto& l = pageLabels[i];
            l.setText (i % 2 == 0 ? "PAGE\nI" : "PAGE\nII", juce::dontSendNotification);
            l.setFont (font (11.5f, true));
            l.setColour (juce::Label::textColourId, dim);
            l.setJustificationType (juce::Justification::centred);
            l.setTooltip (i % 2 == 0 ? "Footswitch Page I on the QC Mini" : "Footswitch Page II on the QC Mini (the Quad Cortex's E-H)");
            addChildComponent (l);
        }

        addButton.setComponentID ("qc.addPreset");
        addButton.setTooltip ("Add a preset (double-click or right-click a preset to edit, recolour, reorder)");
        addButton.setColour (juce::TextButton::buttonColourId, accent);
        addButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        addButton.onClick = [this] { addPreset(); };
        addAndMakeVisible (addButton);

        syncButton.setComponentID ("qc.sync");
        syncButton.setTooltip ("Read your setlists and preset names from the Quad Cortex over USB (quit Cortex Control first). "
                               "Only reads: nothing on the pedal changes.");
        syncButton.setColour (juce::TextButton::buttonColourId, raised);
        syncButton.onClick = [this] { showQcSyncDialog (state); };
        addAndMakeVisible (syncButton);

        // Presets in several setlists only load right if the setlist is sent too (CC#32, off by default).
        setlistWarning.setComponentID ("qc.setlistWarning");
        setlistWarning.setColour (juce::TextButton::buttonColourId, accent.withAlpha (0.18f));
        setlistWarning.setColour (juce::TextButton::textColourOffId, accent);
        setlistWarning.setTooltip ("Your presets are in more than one setlist, but Send setlist (CC#32) is off, so a preset tile loads its "
                                   "bank and slot in whatever setlist the QC is on. Click to turn it on (it's \"Switch to the preset's setlist\" just above), then drag "
                                   "preset clips you made before into your DAW again.");
        setlistWarning.onClick = [this] { state.setProperty (IDs::sendSetlist, true, nullptr); };
        addChildComponent (setlistWarning);

        setlistToggle.setButtonText ("Switch to the preset's setlist");
        setlistToggle.setTooltip (setlistSwitchTooltip ("QC"));
        setlistToggle.onClick = [this] { state.setProperty (IDs::sendSetlist, setlistToggle.getToggleState(), nullptr); };
        addAndMakeVisible (setlistToggle);

        search.setComponentID ("qc.search");
        search.onSearch = [this] { layoutPresets(); presetView.setViewPosition (0, 0); };
        search.onSubmit = [this]
        {
            if (! shownPresets.empty())
                state.setProperty (IDs::selectedPreset, shownPresets.front(), nullptr);
        };
        addAndMakeVisible (search);

        presetView.setViewedComponent (&presetList, false);
        presetView.setScrollBarsShown (true, false);
        presetView.setScrollBarThickness (8);
        addAndMakeVisible (presetView);

        // What scene and stomp tiles act on: their own preset (loaded first) or whatever the QC has loaded.
        loadFirstToggle.setComponentID ("qc.target");
        loadFirstToggle.setColour (juce::ToggleButton::tickColourId, qcBlue);
        loadFirstToggle.setTooltip ("On: scene and stomp tiles load this preset first, then switch the scene or footswitch 1/16 later, "
                                    "so they work whatever preset the QC is on. Off: they only switch the scene or footswitch on "
                                    "the preset the QC has loaded (no preset reload, so no audio gap).");
        loadFirstToggle.onClick = [this] { state.setProperty (IDs::comboPresetScene, loadFirstToggle.getToggleState(), nullptr); };
        addAndMakeVisible (loadFirstToggle);

        stompOnToggle.setColour (juce::ToggleButton::tickColourId, ledGreen);
        stompOnToggle.setTooltip ("Whether stomp tiles engage (ON) or bypass (OFF) the footswitch");
        stompOnToggle.onClick = [this] { state.setProperty (IDs::stompOn, stompOnToggle.getToggleState(), nullptr); };
        addAndMakeVisible (stompOnToggle);

        // Scenes & Stomps or Expression in the lower part of the page.
        viewChoice.setComponentID ("qc.view");
        viewChoice.setInterceptsMouseClicks (false, true);
        addAndMakeVisible (viewChoice);
        for (auto* b : { &scenesViewButton, &looperViewButton, &expressionViewButton })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4304);
            b->setColour (juce::TextButton::buttonColourId, surface);
            b->setColour (juce::TextButton::buttonOnColourId, qcBlue);
            b->setColour (juce::TextButton::textColourOffId, dim);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            viewChoice.addAndMakeVisible (b);
        }
        scenesViewButton.setConnectedEdges (juce::Button::ConnectedOnRight);
        looperViewButton.setConnectedEdges (juce::Button::ConnectedOnLeft | juce::Button::ConnectedOnRight);
        expressionViewButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
        looperViewButton.setComponentID ("qc.viewLooper");
        looperViewButton.setTooltip ("Looper X: record, play, overdub, undo, reverse, half speed and its settings (CC#48-60). "
                                     "The preset needs a Looper X block on the grid.");
        scenesViewButton.setTooltip ("Scene and footswitch tiles for the open preset");
        expressionViewButton.setTooltip ("Expression pedal moves (CC#1 / CC#2): swells, fades, wah, or draw your own. "
                                         "They act on the preset the QC has loaded.");
        auto setView = [this] (bool looper, bool expression)
        {
            state.setProperty (IDs::qcLooperView, looper, nullptr);
            state.setProperty (IDs::qcExpressionView, expression, nullptr);
        };
        scenesViewButton.onClick     = [this, setView] { if (scenesViewButton.getToggleState())     setView (false, false); };
        looperViewButton.onClick     = [this, setView] { if (looperViewButton.getToggleState())     setView (true, false); };
        expressionViewButton.onClick = [this, setView] { if (expressionViewButton.getToggleState()) setView (false, true); };

        for (auto* c : std::initializer_list<juce::Component*> { &looperSection, &looperSettingsSection })
            addChildComponent (c);

        addChildComponent (*expression);

        refresh();
    }

    void refresh() override
    {
        const auto qc = qcTree();
        const auto sel = selectedIndex();
        const auto preset = qc.getChild (sel);
        const auto combo = (bool) state[IDs::comboPresetScene];
        const auto stompOn = (bool) state[IDs::stompOn];

        juce::SortedSet<int> setlists;
        for (auto p : qc)
            setlists.add ((int) p[IDs::setlist]);
        setlistWarning.setButtonText ("Setlists not sent: turn on");
        setlistWarning.setVisible (setlists.size() > 1 && ! (bool) state[IDs::sendSetlist]);
        setlistToggle.setToggleState ((bool) state[IDs::sendSetlist], juce::dontSendNotification);

        loadFirstToggle.setButtonText (preset.isValid() ? "Load " + shortLocation (preset) + " first" : "Load preset first");
        loadFirstToggle.setToggleState (combo, juce::dontSendNotification);
        scenesSection.hint = "CC#43  -  " + (combo ? "scenes & stomps load " + shortLocation (preset) + " first"
                                                   : juce::String ("scenes & stomps act on the current QC preset"));
        scenesSection.repaint();
        stompsSection.hint = "CC#35-42  -  " + (combo ? "after loading " + shortLocation (preset) : juce::String ("on the current QC preset"));
        stompsSection.repaint();
        stompOnToggle.setToggleState (stompOn, juce::dontSendNotification);
        stompOnToggle.setButtonText (stompOn ? "Tiles switch ON" : "Tiles switch OFF");

        presetTiles.clear();
        for (int i = 0; i < qc.getNumChildren(); ++i)
        {
            auto* t = presetTiles.add (new Tile (proc, Tile::Look::row));
            describePreset (*t, i);
            t->highlighted = (i == sel);
            t->onClick = [this, i] { state.setProperty (IDs::selectedPreset, i, nullptr); };
            t->onDoubleClick = [this, i] { editPresetDialog (qcTree().getChild (i)); };
            t->onContextMenu = [this, i] { presetMenu (i); };
            presetList.addAndMakeVisible (t);
        }

        screen = std::make_unique<Tile> (proc, Tile::Look::screen);
        screen->setComponentID ("qc.screen");
        describePreset (*screen, sel);
        screen->chipLetters = mini() ? 4 : 8;
        screen->chipsHeading = mini() ? "SCENES  PAGE I / II" : "SCENES";
        for (int s = 0; s < 8; ++s)
            screen->chips.push_back (state::colourOf (nthOfType (preset, IDs::Scene, s)));
        screen->onDoubleClick = [this, sel] { editPresetDialog (qcTree().getChild (sel)); };
        screen->onContextMenu = [this, sel] { presetMenu (sel); };
        addAndMakeVisible (*screen);

        sceneTiles.clear();
        for (int s = 0; s < 8; ++s)
        {
            const auto scene = nthOfType (preset, IDs::Scene, s);
            auto* t = sceneTiles.add (new Tile (proc, Tile::Look::footswitch));
            t->title = nodeName (scene, "Scene ", s);
            t->badge = cues::qc::letter (mini() ? s % 4 : s);
            t->subtitle = combo ? shortLocation (preset) + " > Scene " + cues::qc::letter (mini() ? s % 4 : s)
                                : "Scene " + cues::qc::letter (mini() ? s % 4 : s) + " - current preset";   // the Mini's page is the row label
            t->colour = state::colourOf (scene);
            t->setTooltip ("Drag onto the timeline to switch to this scene. Double-click to rename, right-click for colour.");
            t->makeCue = [this, sel, s] { return sceneCue (sel, s); };
            t->onDoubleClick = [scene] { renameNode (scene, "Rename scene"); };
            t->onContextMenu = [this, scene, sel, s] { nodeMenu (scene, true, [this, sel, s] { return sceneCue (sel, s); }); };
            addAndMakeVisible (t);
        }

        stompTiles.clear();
        for (int f = 0; f < 8; ++f)
        {
            const auto stomp = nthOfType (preset, IDs::Stomp, f);
            auto* t = stompTiles.add (new Tile (proc, Tile::Look::stomp));
            t->title = nodeName (stomp, "Stomp ", f);
            t->subtitle = "FS " + cues::qc::letter (mini() ? f % 4 : f) + (stompOn ? "  ON" : "  OFF");
            t->active = stompOn;
            t->setTooltip ("Drag to switch footswitch " + label (f) + (stompOn ? " on" : " off")
                           + (combo ? " (loads " + presetLocation (preset) + " first)" : juce::String()) + ". Double-click to rename.");
            t->makeCue = [this, sel, f] { return stompCue (sel, f); };
            t->onDoubleClick = [stomp] { renameNode (stomp, "Rename footswitch"); };
            t->onContextMenu = [this, stomp] { nodeMenu (stomp, false, {}); };
            addAndMakeVisible (t);
        }

        utilTiles.clear();
        auto addUtil = [this] (const juce::String& title, const juce::String& badge, const juce::String& sub,
                               juce::Colour c, std::function<cues::Cue()> make)
        {
            auto* t = utilTiles.add (new Tile (proc, Tile::Look::utility));
            t->title = title;
            t->badge = badge;
            t->subtitle = sub;
            t->colour = c;
            t->makeCue = std::move (make);
            t->setTooltip ("Drag onto the timeline (" + sub + ")");
            addAndMakeVisible (t);
        };

        addUtil ("Tuner On", "", "CC#45 = 127", qcBlue,         [this] { return cues::qc::tuner (qcChannel(), true); });
        addUtil ("Tuner Off", "", "CC#45 = 0",   raised.brighter (0.2f), [this] { return cues::qc::tuner (qcChannel(), false); });
        addUtil ("Tap", "", "CC#44 = 127", juce::Colour (0xfff5a623), [this] { return cues::qc::tap (qcChannel()); });
        utilTiles.getLast()->setTooltip ("One Tap Tempo press (CC#44). Tempo needs several taps a beat apart: drop the clip on each beat, "
                                         "or set the tempo in the preset.");
        addUtil ("Gig View On", "", "CC#46 = 127", juce::Colour (0xff2ec4b6), [this] { return cues::qc::gigView (qcChannel(), true); });
        addUtil ("Gig View Off", "", "CC#46 = 0",   juce::Colour (0xff2ec4b6).darker (0.5f), [this] { return cues::qc::gigView (qcChannel(), false); });
        addUtil ("Preset Mode", "", "CC#47 = 0",   juce::Colour (0xff8e7cf0), [this] { return cues::qc::gigMode (qcChannel(), 0); });
        addUtil ("Stomp Mode", "", "CC#47 = 1",   juce::Colour (0xff8e7cf0), [this] { return cues::qc::gigMode (qcChannel(), 2); });
        addUtil ("Scene Mode", "", "CC#47 = 2",   juce::Colour (0xff8e7cf0), [this] { return cues::qc::gigMode (qcChannel(), 1); });
        if (mini())
        {
            // The Mini's four footswitches have two pages (Page II = the QC's E-H).
            addUtil ("Page I", "", "CC#64 = 0",     juce::Colour (0xff5b8def), [this] { return cues::qc::footswitchPage (qcChannel(), 1); });
            utilTiles.getLast()->setTooltip ("Show footswitch Page I on the QC Mini (CC#64): scenes and stomps A-D");
            addUtil ("Page II", "", "CC#64 = 127",  juce::Colour (0xff5b8def), [this] { return cues::qc::footswitchPage (qcChannel(), 2); });
            utilTiles.getLast()->setTooltip ("Show footswitch Page II on the QC Mini (CC#64): the second set of scenes and stomps A-D");
        }
        utilsSection.hint = mini() ? "tuner, tap, gig view, footswitch mode, page" : "tuner, tap, gig view, footswitch mode";
        utilsSection.repaint();

        // Looper X (both QC manuals, CorOS 4.1.1): the actions toggle with any value 64-127; the settings take a value.
        looperTiles.clear();
        looperSettingTiles.clear();
        struct LooperDef { const char* name; int cc, value; bool press; juce::uint32 colour; const char* note; };
        static const LooperDef actions[] = {
            { "Record / Overdub", 53, 127, true, 0xffe74c3c, "Starts recording; pressed again it overdubs (like the footswitch)." },
            { "Play / Stop", 54, 127, true, 0xff2ecc71, {} }, { "Undo / Redo", 56, 127, true, 0xff9b59b6, {} },
            { "One Shot", 50, 127, true, 0xfff5a623, "Plays the loop once." }, { "Reverse", 55, 127, true, 0xff3498db, {} },
            { "Half Speed", 51, 127, true, 0xff1abc9c, {} }, { "Duplicate", 49, 127, true, 0xffe67e22, "Doubles the loop length." },
            { "Punch In / Out", 52, 127, true, 0xffc0392b, {} } };
        static const LooperDef settings[] = {
            { "Looper open", 48, 0, false, 0xff2ec4b6, "Opens the Looper X screen (Perform mode)." },
            { "Looper close", 48, 127, false, 0xff1e8c84, "Closes the Looper X screen." },
            { "Quantize off", 58, 0, false, 0xff8e7cf0, {} }, { "Quantize 4", 58, 4, false, 0xff8e7cf0, "Quantize to 4 beats." },
            { "Quantize 8", 58, 8, false, 0xff8e7cf0, "Quantize to 8 beats." }, { "Quantize 16", 58, 9, false, 0xff8e7cf0, "Quantize to 16 beats (value 9)." },
            { "Duplicate Free", 57, 0, false, 0xffe67e22, "Duplicate mode: Free." }, { "Duplicate Sync", 57, 1, false, 0xffe67e22, "Duplicate mode: Sync." },
            { "Clock start Free", 59, 0, false, 0xff5b8def, "MIDI Clock Start: Free." }, { "Clock start Sync", 59, 1, false, 0xff5b8def, "MIDI Clock Start: Sync." },
            { "Perform view", 60, 0, false, 0xff7f8c8d, "The Looper X Perform view." }, { "Parameters view", 60, 1, false, 0xff7f8c8d, "The Looper X Parameters view." } };
        auto addLooper = [this] (juce::OwnedArray<Tile>& tiles, const LooperDef& d)
        {
            auto* t = tiles.add (new Tile (proc, Tile::Look::utility));
            t->title = d.name;
            t->subtitle = "CC#" + juce::String (d.cc) + " = " + juce::String (d.value);
            t->colour = juce::Colour (d.colour);
            t->setTooltip ((d.note != nullptr ? juce::String (d.note) + " " : juce::String())
                           + (d.press ? "Each clip toggles it, like a footswitch press. " : juce::String())
                           + "Needs a Looper X block in the loaded preset. Drag onto the timeline.");
            const auto name = juce::String (d.name);
            const auto cc = d.cc, value = d.value;
            const auto press = d.press;
            t->makeCue = [this, name, cc, value, press] { return cues::qc::looper (qcChannel(), name, cc, value, press); };
            addChildComponent (t);
        };
        for (const auto& d : actions)  addLooper (looperTiles, d);
        for (const auto& d : settings) addLooper (looperSettingTiles, d);

        const auto showExpression = (bool) state[IDs::qcExpressionView];
        const auto showLooper = ! showExpression && (bool) state[IDs::qcLooperView];
        const auto showScenes = ! showExpression && ! showLooper;
        (showExpression ? expressionViewButton : showLooper ? looperViewButton : scenesViewButton).setToggleState (true, juce::dontSendNotification);
        for (auto* c : std::initializer_list<juce::Component*> { &scenesSection, &stompsSection, &loadFirstToggle, &stompOnToggle })
            c->setVisible (showScenes);
        for (auto* t : sceneTiles) t->setVisible (showScenes);
        for (auto* t : stompTiles) t->setVisible (showScenes);
        for (auto& l : pageLabels) l.setVisible (mini() && showScenes);
        for (auto* c : std::initializer_list<juce::Component*> { &looperSection, &looperSettingsSection })
            c->setVisible (showLooper);
        for (auto* t : looperTiles)        t->setVisible (showLooper);
        for (auto* t : looperSettingTiles) t->setVisible (showLooper);
        expression->setVisible (showExpression);
        expression->refresh();

        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);

        presetsSection.setBounds (r.removeFromLeft (juce::jlimit (230, 300, r.getWidth() / 4)));
        r.removeFromLeft (12);

        addButton.setBounds (presetsSection.headerArea().removeFromRight (86));
        {
            auto content = presetsSection.contentArea();
            syncButton.setBounds (content.removeFromBottom (34).reduced (2, 0));
            content.removeFromBottom (8);
            if (setlistWarning.isVisible())
            {
                setlistWarning.setBounds (content.removeFromBottom (30).reduced (2, 0));
                content.removeFromBottom (6);
            }
            setlistToggle.setBounds (content.removeFromBottom (30).reduced (2, 0));
            content.removeFromBottom (4);
            search.setBounds (content.removeFromTop (32).reduced (2, 0));
            content.removeFromTop (6);
            presetView.setBounds (content);
        }
        layoutPresets();

        if (screen != nullptr)
            screen->setBounds (r.removeFromTop (104).expanded (3));
        r.removeFromTop (10);

        viewChoice.setBounds (r.removeFromTop (30).removeFromLeft (480));
        {
            auto c = viewChoice.getLocalBounds();
            scenesViewButton.setBounds (c.removeFromLeft (c.getWidth() / 3));
            looperViewButton.setBounds (c.removeFromLeft (c.getWidth() / 2));
            expressionViewButton.setBounds (c);
        }
        r.removeFromTop (10);

        utilsSection.setBounds (r.removeFromBottom (Section::headerHeight + 2 * 60 + 6));   // two rows: of four, or five on the Mini
        r.removeFromBottom (12);
        expression->setBounds (r);
        {
            // Looper view: the actions (two rows of four), then the settings (two rows of six).
            auto area = r;
            looperSection.setBounds (area.removeFromTop (juce::jmin (area.getHeight() / 2, Section::headerHeight + 2 * 64 + 6)));
            area.removeFromTop (12);
            looperSettingsSection.setBounds (area.removeFromTop (juce::jmin (area.getHeight(), Section::headerHeight + 2 * 60 + 6)));
            layoutGrid (looperTiles, looperSection.contentArea().expanded (3), 4, 6);
            layoutGrid (looperSettingTiles, looperSettingsSection.contentArea().expanded (3), 6, 6);
        }
        stompsSection.setBounds (r.removeFromBottom (Section::headerHeight + 64));
        r.removeFromBottom (12);
        scenesSection.setBounds (r);

        {
            auto hdr = scenesSection.headerArea().withSizeKeepingCentre (scenesSection.headerArea().getWidth(), 28);
            loadFirstToggle.setBounds (hdr.removeFromRight (170));
        }
        stompOnToggle.setBounds (stompsSection.headerArea().removeFromRight (160));

        // QC display layout: scenes A-D on the top row, E-H on the bottom row (the Mini: Page I, Page II, labelled).
        auto grid = scenesSection.contentArea().expanded (3);
        if (mini())
        {
            auto labels = grid.removeFromLeft (pageLabelWidth);
            pageLabels[0].setBounds (labels.removeFromTop (grid.getHeight() / 2));
            pageLabels[1].setBounds (labels);
        }
        const auto rowH = grid.getHeight() / 2;
        const auto cellW = grid.getWidth() / 4;
        for (int s = 0; s < sceneTiles.size(); ++s)
        {
            const auto row = s < 4 ? 0 : 1;
            sceneTiles[s]->setBounds (grid.getX() + (s % 4) * cellW, grid.getY() + row * rowH, cellW, rowH);
        }

        if (mini() && stompTiles.size() == 8)
        {
            // Page I and Page II halves, each with its label.
            auto strip = stompsSection.contentArea().expanded (3);
            const auto cellW = (strip.getWidth() - 2 * pageLabelWidth) / 8;
            for (int half = 0; half < 2; ++half)
            {
                pageLabels[2 + half].setBounds (strip.removeFromLeft (pageLabelWidth));
                for (int f = half * 4; f < half * 4 + 4; ++f)
                    stompTiles[f]->setBounds (strip.removeFromLeft (cellW));
            }
        }
        else
            layoutStrip (stompTiles, stompsSection.contentArea().expanded (3));
        layoutGrid (utilTiles, utilsSection.contentArea().expanded (3), utilTiles.size() > 8 ? 5 : 4, 6);
    }

private:
    static void layoutStrip (juce::OwnedArray<Tile>& tiles, juce::Rectangle<int> area)
    {
        layoutGrid (tiles, area, juce::jmax (1, tiles.size()), 0);
    }

    juce::ValueTree qcTree() const { return state.getChildWithName (IDs::QC); }
    int qcChannel() const          { return state::ampChannel (state); }
    bool mini() const              { return (int) state[IDs::ampUnit] == state::qcMiniAmpUnit; }

    // Scene / footswitch 0-7 as the unit shows it: A-H on the Quad Cortex, "A (I)" .. "D (II)" on the Mini.
    juce::String label (int index) const { return mini() ? cues::qc::miniLabel (index) : cues::qc::letter (index); }

    // A scene or stomp name, with the default "Scene E" shown as "Scene A (II)" on the Mini.
    juce::String nodeName (const juce::ValueTree& node, const juce::String& prefix, int index) const
    {
        const auto name = node[IDs::name].toString();
        return mini() && name == prefix + cues::qc::letter (index) ? prefix + label (index) : name;
    }

    int selectedIndex() const
    {
        return juce::jlimit (0, juce::jmax (0, qcTree().getNumChildren() - 1), (int) state[IDs::selectedPreset]);
    }

    // Bank and slot as on the QC's preset grid, e.g. "1A".
    static juce::String shortLocation (const juce::ValueTree& p)
    {
        return juce::String ((int) p[IDs::bank]) + cues::qc::letter ((int) p[IDs::slot]);
    }

    static juce::String presetLocation (const juce::ValueTree& p)
    {
        return cues::qc::location ((int) p[IDs::setlist], (int) p[IDs::bank], (int) p[IDs::slot]);
    }

    void describePreset (Tile& t, int index)
    {
        const auto p = qcTree().getChild (index);
        t.title = p[IDs::name].toString();
        t.subtitle = presetLocation (p);
        t.colour = state::colourOf (p);
        t.setTooltip ("Click to show its scenes, drag onto the timeline to load it, double-click to edit.");
        t.makeCue = [this, index] { return presetCue (index); };
    }

    cues::Cue presetCue (int index) const
    {
        const auto p = qcTree().getChild (index);
        return cues::qc::preset (qcChannel(), (int) p[IDs::setlist], (int) p[IDs::bank], (int) p[IDs::slot],
                                 (bool) state[IDs::sendSetlist], p[IDs::name].toString());
    }

    cues::Cue sceneCue (int presetIndex, int sceneIndex) const
    {
        const auto p = qcTree().getChild (presetIndex);
        const auto sceneName = nodeName (nthOfType (p, IDs::Scene, sceneIndex), "Scene ", sceneIndex);

        if (! (bool) state[IDs::comboPresetScene])
            return cues::qc::scene (qcChannel(), sceneIndex, sceneName, mini());

        cues::Cue c;
        c.name = "QC " + p[IDs::name].toString() + " > " + label (sceneIndex) + " - " + sceneName;
        cues::qc::addPresetLoad (c, qcChannel(), (int) p[IDs::setlist], (int) p[IDs::bank], (int) p[IDs::slot],
                                 (bool) state[IDs::sendSetlist], 0.0);
        cues::qc::addScene (c, qcChannel(), sceneIndex, 0.25);
        return c;
    }

    cues::Cue stompCue (int presetIndex, int footswitch) const
    {
        const auto p = qcTree().getChild (presetIndex);
        const auto stompName = nodeName (nthOfType (p, IDs::Stomp, footswitch), "Stomp ", footswitch);
        const auto on = (bool) state[IDs::stompOn];
        auto c = cues::qc::stomp (qcChannel(), footswitch, on, stompName, mini());

        if (! (bool) state[IDs::comboPresetScene])
            return c;

        const auto stompMessage = c.events.front().second;
        c.events.clear();
        c.name = "QC " + p[IDs::name].toString() + " > " + c.name.fromFirstOccurrenceOf ("QC ", false, false);
        cues::qc::addPresetLoad (c, qcChannel(), (int) p[IDs::setlist], (int) p[IDs::bank], (int) p[IDs::slot],
                                 (bool) state[IDs::sendSetlist], 0.0);
        c.add (0.25, stompMessage);
        return c;
    }

    void addPreset()
    {
        auto qc = qcTree();
        auto setlist = 1, bank = 1, slot = 0;

        if (qc.getNumChildren() > 0)
        {
            const auto last = qc.getChild (qc.getNumChildren() - 1);
            setlist = (int) last[IDs::setlist];
            slot = (int) last[IDs::slot] + 1;
            bank = juce::jlimit (1, 32, (int) last[IDs::bank] + slot / 8);
            slot %= 8;
        }

        auto preset = state::createPreset ("Preset " + juce::String (bank) + cues::qc::letter (slot),
                                           setlist, bank, slot, paletteColour (qc.getNumChildren()));
        qc.appendChild (preset, nullptr);
        state.setProperty (IDs::selectedPreset, qc.getNumChildren() - 1, nullptr);
        editPresetDialog (preset);
    }

    void presetMenu (int index)
    {
        auto qc = qcTree();
        auto preset = qc.getChild (index);

        juce::PopupMenu m;
        m.addItem (1, "Edit name / location...");
        m.addSubMenu ("Colour", colourMenu (state::colourOf (preset), 100));
        m.addItem (7, "Apply colour to all its scenes");
        m.addSeparator();
        m.addItem (2, "Duplicate");
        m.addItem (3, "Move up", index > 0);
        m.addItem (4, "Move down", index < qc.getNumChildren() - 1);
        m.addItem (5, "Delete", qc.getNumChildren() > 1);
        m.addSeparator();
        m.addItem (6, "Send to pedal now");

        juce::Component::SafePointer<QcPage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, index, preset] (int result) mutable
        {
            if (safe == nullptr || result == 0)
                return;

            auto& page = *safe;
            auto qcNode = page.qcTree();

            if (result >= 100)
                preset.setProperty (IDs::colour, paletteColour (result - 100).toString(), nullptr);
            else if (result == 1)
                editPresetDialog (preset);
            else if (result == 2)
                qcNode.addChild (preset.createCopy(), index + 1, nullptr);
            else if (result == 3)
                qcNode.moveChild (index, index - 1, nullptr);
            else if (result == 4)
                qcNode.moveChild (index, index + 1, nullptr);
            else if (result == 5)
            {
                qcNode.removeChild (index, nullptr);
                page.state.setProperty (IDs::selectedPreset, juce::jmax (0, index - 1), nullptr);
            }
            else if (result == 6)
                page.proc.preview (page.presetCue (index));
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
        if (send)
            m.addItem (2, "Send to pedal now");

        juce::Component::SafePointer<QcPage> safe (this);
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

    Section presetsSection { "qc.presetList", "Presets", "click to open" };
    Section scenesSection  { "qc.scenes", "Scenes", "CC#43", qcBlue };
    Section stompsSection  { "qc.stomps", "Stomps", "CC#35-42", ledGreen };
    Section utilsSection   { "qc.utils", "Utilities", "tuner, tap, gig view, footswitch mode" };
    Section looperSection  { "qc.looper", "Looper X", "CC#49-56  -  needs a Looper X block in the preset", accent };
    Section looperSettingsSection { "qc.looperSettings", "Looper X settings", "CC#48, 57-60", accent };

    juce::TextButton addButton { "+ Preset" };
    juce::TextButton syncButton { "Sync from QC (USB)" };
    SearchBox search { "Search presets" };
    std::vector<int> shownPresets;

    // The preset rows that match the search, best first.
    void layoutPresets()
    {
        shownPresets = layoutFilteredRows (presetTiles, presetList, presetView.getWidth() - presetView.getScrollBarThickness() - 2,
                                           50, search.getText());
    }
    juce::TextButton setlistWarning;
    juce::ToggleButton setlistToggle;
    juce::Viewport presetView;
    juce::Component presetList;
    juce::ToggleButton loadFirstToggle;
    juce::ToggleButton stompOnToggle { "Tiles switch ON" };
    juce::Component viewChoice;
    juce::TextButton scenesViewButton { "Scenes & Stomps" }, looperViewButton { "Looper" }, expressionViewButton { "Expression" };
    juce::OwnedArray<Tile> looperTiles, looperSettingTiles;
    std::unique_ptr<Page> expression { makeQcExpression (proc) };

    juce::OwnedArray<Tile> presetTiles, sceneTiles, stompTiles, utilTiles;
    juce::Label pageLabels[4];   // QC Mini: scenes Page I / II, stomps Page I / II
    static constexpr int pageLabelWidth = 52;
    std::unique_ptr<Tile> screen;
};
} // namespace

std::unique_ptr<Page> makeQcPage (PedalCuesProcessor& p)
{
    return std::make_unique<QcPage> (p);
}

juce::String ampUnitName (int unit)
{
    return unit == 1 ? "Kemper Profiler" : unit == 2 ? "Kemper Player" : unit == state::qcMiniAmpUnit ? "Quad Cortex Mini" : "Quad Cortex";
}

AmpInfo ampInfo (const juce::ValueTree& state)
{
    AmpInfo a;
    const auto unit = (int) state[IDs::ampUnit];
    if (unit == state::modellerAmpUnit)
    {
        if (const auto* p = modellers::find (state[IDs::modellerProfile].toString()))
        {
            a.kind = AmpInfo::Kind::modeller;
            a.name = a.box = a.shortName = p->shortName;
            a.colour = p->colour;
            a.hasDin = p->hasDin;
            a.hasThru = p->hasThru;
            a.midiIn = p->midiIn;
            a.usbMidi = p->usbMidi;
            a.usbToThru = p->usbToThru;
            a.usbThruSetting = p->usbThruSetting;
            a.channelHint = p->channelHint;
        }
    }
    else if (unit == state::customAmpUnit)
    {
        if (const auto u = state::customUnit (state); u.isValid())
        {
            a.kind = AmpInfo::Kind::custom;
            a.name = a.box = a.shortName = u[IDs::name].toString();
            a.colour = state::colourOf (u, juce::Colour (0xff8e7cf0));
        }
    }
    else if (unit == state::qcMiniAmpUnit)
    {
        // Same page and MIDI as the Quad Cortex. Its MIDI In and Out / Thru are 3.5 mm TRS jacks (Type A), and its
        // manual doesn't say whether USB MIDI reaches the Thru.
        a.name = ampUnitName (unit);
        a.box = "QC Mini";
        a.midiIn = "TRS MIDI In";
        a.usbToThru = 3;
    }
    else if (unit == 1 || unit == 2)
    {
        a.kind = AmpInfo::Kind::kemper;
        a.name = ampUnitName (unit);
        a.box = a.shortName = "Kemper";
        a.colour = theme::kemperGreen;
    }
    return a;
}

namespace
{
// The first tab: the Quad Cortex or Kemper page, for the unit picked with the ▾ on the tab (or in How to connect).
class AmpPage final : public Page
{
public:
    explicit AmpPage (PedalCuesProcessor& p)
        : state (p.state), qc (makeQcPage (p)), kemper (makeKemperPage (p)), custom (makeCustomPage (p, false)), modeller (makeModellerPage (p))
    {
        addChildComponent (*qc);
        addChildComponent (*kemper);
        addChildComponent (*custom);
        addChildComponent (*modeller);
        refresh();
    }

    void refresh() override
    {
        const auto kind = ampInfo (state).kind;
        auto* shown = kind == AmpInfo::Kind::custom ? custom.get() : kind == AmpInfo::Kind::kemper ? kemper.get()
                    : kind == AmpInfo::Kind::modeller ? modeller.get() : qc.get();
        for (auto* page : { qc.get(), kemper.get(), custom.get(), modeller.get() })
            page->setVisible (page == shown);
        shown->refresh();
    }

    void resized() override
    {
        for (auto* page : { qc.get(), kemper.get(), custom.get(), modeller.get() })
            page->setBounds (getLocalBounds());
    }

private:
    juce::ValueTree state;
    std::unique_ptr<Page> qc, kemper, custom, modeller;
};
} // namespace

std::unique_ptr<Page> makeAmpPage (PedalCuesProcessor& p)
{
    return std::make_unique<AmpPage> (p);
}
} // namespace ui
