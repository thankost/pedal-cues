#include "EditorCommon.h"

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
                                   "bank and slot in whatever setlist the QC is on. Click to turn it on (MIDI Setup > Advanced), then drag "
                                   "preset clips you made before into your DAW again.");
        setlistWarning.onClick = [this] { state.setProperty (IDs::sendSetlist, true, nullptr); };
        addChildComponent (setlistWarning);

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
        for (auto* b : { &scenesViewButton, &expressionViewButton })
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
        expressionViewButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
        scenesViewButton.setTooltip ("Scene and footswitch tiles for the open preset");
        expressionViewButton.setTooltip ("Expression pedal moves (CC#1 / CC#2): swells, fades, wah, or draw your own. "
                                         "They act on the preset the QC has loaded.");
        scenesViewButton.onClick     = [this] { if (scenesViewButton.getToggleState())     state.setProperty (IDs::qcExpressionView, false, nullptr); };
        expressionViewButton.onClick = [this] { if (expressionViewButton.getToggleState()) state.setProperty (IDs::qcExpressionView, true, nullptr); };

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
            t->title = scene[IDs::name].toString();
            t->badge = cues::qc::letter (s);
            t->subtitle = combo ? shortLocation (preset) + " > Scene " + cues::qc::letter (s)
                                : "Scene " + cues::qc::letter (s) + " - current preset";
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
            t->title = stomp[IDs::name].toString();
            t->subtitle = "FS " + cues::qc::letter (f) + (stompOn ? "  ON" : "  OFF");
            t->active = stompOn;
            t->setTooltip ("Drag to switch footswitch " + cues::qc::letter (f) + (stompOn ? " on" : " off")
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
        addUtil ("Gig View On", "", "CC#46 = 127", juce::Colour (0xff2ec4b6), [this] { return cues::qc::gigView (qcChannel(), true); });
        addUtil ("Gig View Off", "", "CC#46 = 0",   juce::Colour (0xff2ec4b6).darker (0.5f), [this] { return cues::qc::gigView (qcChannel(), false); });
        addUtil ("Preset Mode", "", "CC#47 = 0",   juce::Colour (0xff8e7cf0), [this] { return cues::qc::gigMode (qcChannel(), 0); });
        addUtil ("Stomp Mode", "", "CC#47 = 1",   juce::Colour (0xff8e7cf0), [this] { return cues::qc::gigMode (qcChannel(), 2); });
        addUtil ("Scene Mode", "", "CC#47 = 2",   juce::Colour (0xff8e7cf0), [this] { return cues::qc::gigMode (qcChannel(), 1); });

        const auto showExpression = (bool) state[IDs::qcExpressionView];
        (showExpression ? expressionViewButton : scenesViewButton).setToggleState (true, juce::dontSendNotification);
        for (auto* c : std::initializer_list<juce::Component*> { &scenesSection, &stompsSection, &loadFirstToggle, &stompOnToggle })
            c->setVisible (! showExpression);
        for (auto* t : sceneTiles) t->setVisible (! showExpression);
        for (auto* t : stompTiles) t->setVisible (! showExpression);
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
            presetView.setBounds (content);
        }

        const auto listWidth = presetView.getWidth() - presetView.getScrollBarThickness() - 2;
        constexpr int rowHeight = 50;
        presetList.setSize (listWidth, rowHeight * presetTiles.size());
        for (int i = 0; i < presetTiles.size(); ++i)
            presetTiles[i]->setBounds (0, i * rowHeight, listWidth, rowHeight);

        if (screen != nullptr)
            screen->setBounds (r.removeFromTop (104).expanded (3));
        r.removeFromTop (10);

        viewChoice.setBounds (r.removeFromTop (30).removeFromLeft (320));
        {
            auto c = viewChoice.getLocalBounds();
            scenesViewButton.setBounds (c.removeFromLeft (c.getWidth() / 2));
            expressionViewButton.setBounds (c);
        }
        r.removeFromTop (10);

        utilsSection.setBounds (r.removeFromBottom (Section::headerHeight + 60));
        r.removeFromBottom (12);
        expression->setBounds (r);
        stompsSection.setBounds (r.removeFromBottom (Section::headerHeight + 64));
        r.removeFromBottom (12);
        scenesSection.setBounds (r);

        {
            auto hdr = scenesSection.headerArea().withSizeKeepingCentre (scenesSection.headerArea().getWidth(), 28);
            loadFirstToggle.setBounds (hdr.removeFromRight (170));
        }
        stompOnToggle.setBounds (stompsSection.headerArea().removeFromRight (160));

        // QC display layout: scenes A-D on the top row, E-H on the bottom row.
        auto grid = scenesSection.contentArea().expanded (3);
        const auto rowH = grid.getHeight() / 2;
        const auto cellW = grid.getWidth() / 4;
        for (int s = 0; s < sceneTiles.size(); ++s)
        {
            const auto row = s < 4 ? 0 : 1;
            sceneTiles[s]->setBounds (grid.getX() + (s % 4) * cellW, grid.getY() + row * rowH, cellW, rowH);
        }

        layoutStrip (stompTiles, stompsSection.contentArea().expanded (3));
        layoutStrip (utilTiles, utilsSection.contentArea().expanded (3));
    }

private:
    static void layoutStrip (juce::OwnedArray<Tile>& tiles, juce::Rectangle<int> area)
    {
        layoutGrid (tiles, area, juce::jmax (1, tiles.size()), 0);
    }

    juce::ValueTree qcTree() const { return state.getChildWithName (IDs::QC); }
    int qcChannel() const          { return (int) state[IDs::qcChannel]; }

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
        const auto sceneName = nthOfType (p, IDs::Scene, sceneIndex)[IDs::name].toString();

        if (! (bool) state[IDs::comboPresetScene])
            return cues::qc::scene (qcChannel(), sceneIndex, sceneName);

        cues::Cue c;
        c.name = "QC " + p[IDs::name].toString() + " > " + cues::qc::letter (sceneIndex) + " - " + sceneName;
        cues::qc::addPresetLoad (c, qcChannel(), (int) p[IDs::setlist], (int) p[IDs::bank], (int) p[IDs::slot],
                                 (bool) state[IDs::sendSetlist], 0.0);
        cues::qc::addScene (c, qcChannel(), sceneIndex, 0.25);
        return c;
    }

    cues::Cue stompCue (int presetIndex, int footswitch) const
    {
        const auto p = qcTree().getChild (presetIndex);
        const auto stompName = nthOfType (p, IDs::Stomp, footswitch)[IDs::name].toString();
        const auto on = (bool) state[IDs::stompOn];
        auto c = cues::qc::stomp (qcChannel(), footswitch, on, stompName);

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
    Section utilsSection   { "qc.utils", "Utilities", "tuner, gig view, footswitch mode" };

    juce::TextButton addButton { "+ Preset" };
    juce::TextButton syncButton { "Sync from QC (USB)" };
    juce::TextButton setlistWarning;
    juce::Viewport presetView;
    juce::Component presetList;
    juce::ToggleButton loadFirstToggle;
    juce::ToggleButton stompOnToggle { "Tiles switch ON" };
    juce::Component viewChoice;
    juce::TextButton scenesViewButton { "Scenes & Stomps" }, expressionViewButton { "Expression" };
    std::unique_ptr<Page> expression { makeQcExpression (proc) };

    juce::OwnedArray<Tile> presetTiles, sceneTiles, stompTiles, utilTiles;
    std::unique_ptr<Tile> screen;
};
} // namespace

std::unique_ptr<Page> makeQcPage (PedalCuesProcessor& p)
{
    return std::make_unique<QcPage> (p);
}

juce::String ampUnitName (int unit)
{
    return unit == 1 ? "Kemper Profiler" : unit == 2 ? "Kemper Player" : "Quad Cortex";
}

AmpInfo ampInfo (const juce::ValueTree& state)
{
    AmpInfo a;
    const auto unit = (int) state[IDs::ampUnit];
    if (unit == state::customAmpUnit)
    {
        if (const auto u = state::customUnit (state); u.isValid())
        {
            a.kind = AmpInfo::Kind::custom;
            a.name = a.box = a.shortName = u[IDs::name].toString();
            a.colour = state::colourOf (u, juce::Colour (0xff8e7cf0));
        }
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
// The first tab: the Quad Cortex or Kemper page, for the unit picked with the ▾ on the tab (or in MIDI Setup).
class AmpPage final : public Page
{
public:
    explicit AmpPage (PedalCuesProcessor& p)
        : state (p.state), qc (makeQcPage (p)), kemper (makeKemperPage (p)), custom (makeCustomPage (p))
    {
        addChildComponent (*qc);
        addChildComponent (*kemper);
        addChildComponent (*custom);
        refresh();
    }

    void refresh() override
    {
        const auto kind = ampInfo (state).kind;
        auto* shown = kind == AmpInfo::Kind::custom ? custom.get() : kind == AmpInfo::Kind::kemper ? kemper.get() : qc.get();
        for (auto* page : { qc.get(), kemper.get(), custom.get() })
            page->setVisible (page == shown);
        shown->refresh();
    }

    void resized() override
    {
        for (auto* page : { qc.get(), kemper.get(), custom.get() })
            page->setBounds (getLocalBounds());
    }

private:
    juce::ValueTree state;
    std::unique_ptr<Page> qc, kemper, custom;
};
} // namespace

std::unique_ptr<Page> makeAmpPage (PedalCuesProcessor& p)
{
    return std::make_unique<AmpPage> (p);
}
} // namespace ui
