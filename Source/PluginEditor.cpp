#include "PluginEditor.h"
#include "Tile.h"

#include <array>
#include <cmath>

namespace
{
const juce::Colour background (0xff15161a);
const juce::Colour panel      (0xff202228);
const juce::Colour accent     (0xffffb020);
const juce::Colour dimText    (0xffa0a4ad);

juce::ValueTree nthOfType (const juce::ValueTree& parent, const juce::Identifier& type, int n)
{
    int count = 0;
    for (auto child : parent)
        if (child.hasType (type) && count++ == n)
            return child;
    return {};
}

void styleHeader (juce::Label& l, const juce::String& text)
{
    l.setText (text, juce::dontSendNotification);
    l.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    l.setColour (juce::Label::textColourId, dimText);
}

void layoutGrid (juce::OwnedArray<Tile>& tiles, juce::Rectangle<int> area, int columns)
{
    if (tiles.isEmpty())
        return;

    const auto rows = (tiles.size() + columns - 1) / columns;
    const auto cellW = area.getWidth() / columns;
    const auto cellH = area.getHeight() / rows;

    for (int i = 0; i < tiles.size(); ++i)
        tiles[i]->setBounds (area.getX() + (i % columns) * cellW, area.getY() + (i / columns) * cellH, cellW, cellH);
}

juce::PopupMenu colourMenu (juce::Colour current, int baseId)
{
    juce::PopupMenu m;
    const auto& p = state::palette();
    for (int i = 0; i < p.size(); ++i)
    {
        const juce::Colour c (p.getReference (i).argb);
        m.addColouredItem (baseId + i, p.getReference (i).name, c, true, c == current);
    }
    return m;
}

juce::Colour paletteColour (int index)
{
    return juce::Colour (state::palette().getReference (index).argb);
}

void askText (juce::Component* owner, const juce::String& title, const juce::String& current,
              std::function<void (const juce::String&)> done)
{
    auto* w = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon, owner);
    w->addTextEditor ("text", current);
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, done] (int result)
    {
        if (result == 1)
            done (w->getTextEditorContents ("text").trim());
    }), true);
}

void renameNode (juce::Component* owner, juce::ValueTree node, const juce::String& title)
{
    askText (owner, title, node[IDs::name].toString(), [node] (const juce::String& text) mutable
    {
        if (text.isNotEmpty())
            node.setProperty (IDs::name, text, nullptr);
    });
}

void editPresetDialog (juce::Component* owner, juce::ValueTree preset)
{
    auto* w = new juce::AlertWindow ("Edit preset",
                                     "Name it, and tell PedalCues where it lives on the Quad Cortex.",
                                     juce::MessageBoxIconType::NoIcon, owner);
    w->addTextEditor ("name", preset[IDs::name].toString(), "Name");
    w->addTextEditor ("setlist", preset[IDs::setlist].toString(), "Setlist number");
    w->addTextEditor ("bank", preset[IDs::bank].toString(), "Bank (1-32)");

    juce::StringArray slots;
    for (int i = 0; i < 8; ++i)
        slots.add (cues::qc::letter (i));

    w->addComboBox ("slot", slots, "Slot");
    w->getComboBoxComponent ("slot")->setSelectedItemIndex ((int) preset[IDs::slot], juce::dontSendNotification);

    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, preset] (int result) mutable
    {
        if (result != 1)
            return;

        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isNotEmpty())
            preset.setProperty (IDs::name, name, nullptr);

        preset.setProperty (IDs::setlist, juce::jlimit (1, 128, w->getTextEditorContents ("setlist").getIntValue()), nullptr);
        preset.setProperty (IDs::bank,    juce::jlimit (1, 32,  w->getTextEditorContents ("bank").getIntValue()), nullptr);
        preset.setProperty (IDs::slot,    juce::jmax (0, w->getComboBoxComponent ("slot")->getSelectedItemIndex()), nullptr);
    }), true);
}

//==============================================================================
class QcPage final : public PedalCuesEditor::Page
{
public:
    explicit QcPage (PedalCuesProcessor& p) : proc (p), state (p.state)
    {
        styleHeader (presetsLabel, "PRESETS");
        styleHeader (scenesLabel, "SCENES  (CC#43)");
        styleHeader (stompsLabel, "FOOTSWITCH STOMPS  (CC#35-42)");
        styleHeader (utilLabel, "UTILITIES");

        for (auto* c : std::initializer_list<juce::Component*> { &presetsLabel, &scenesLabel, &stompsLabel, &utilLabel,
                                                                 &addButton, &presetView, &comboToggle, &stompOnToggle })
            addAndMakeVisible (c);

        addButton.setTooltip ("Add a preset tile (right-click tiles to edit, recolour, reorder)");
        addButton.onClick = [this] { addPreset(); };

        presetView.setViewedComponent (&presetList, false);
        presetView.setScrollBarsShown (true, false);

        comboToggle.setTooltip ("Scene tiles also load their preset first (scene is sent 1/4 beat later)");
        comboToggle.onClick = [this] { state.setProperty (IDs::comboPresetScene, comboToggle.getToggleState(), nullptr); };

        stompOnToggle.setTooltip ("Whether stomp tiles engage (on) or bypass (off) the footswitch");
        stompOnToggle.onClick = [this] { state.setProperty (IDs::stompOn, stompOnToggle.getToggleState(), nullptr); };

        refresh();
    }

    void refresh() override
    {
        const auto qc = qcTree();
        const auto sel = selectedIndex();
        const auto preset = qc.getChild (sel);
        const auto combo = (bool) state[IDs::comboPresetScene];
        const auto stompOn = (bool) state[IDs::stompOn];

        comboToggle.setToggleState (combo, juce::dontSendNotification);
        stompOnToggle.setToggleState (stompOn, juce::dontSendNotification);
        stompOnToggle.setButtonText (stompOn ? "Stomp tiles send ON" : "Stomp tiles send OFF");

        presetTiles.clear();
        for (int i = 0; i < qc.getNumChildren(); ++i)
        {
            auto* t = presetTiles.add (new Tile (proc));
            describePreset (*t, i);
            t->highlighted = (i == sel);
            t->onClick = [this, i] { state.setProperty (IDs::selectedPreset, i, nullptr); };
            t->onDoubleClick = [this, i] { editPresetDialog (this, qcTree().getChild (i)); };
            t->onContextMenu = [this, i] { presetMenu (i); };
            presetList.addAndMakeVisible (t);
        }

        stripTile = std::make_unique<Tile> (proc);
        describePreset (*stripTile, sel);
        stripTile->title = "Loaded preset:  " + stripTile->title;
        stripTile->onDoubleClick = [this, sel] { editPresetDialog (this, qcTree().getChild (sel)); };
        stripTile->onContextMenu = [this, sel] { presetMenu (sel); };
        addAndMakeVisible (*stripTile);

        sceneTiles.clear();
        for (int s = 0; s < 8; ++s)
        {
            const auto scene = nthOfType (preset, IDs::Scene, s);
            auto* t = sceneTiles.add (new Tile (proc));
            t->title = scene[IDs::name].toString();
            t->subtitle = "Scene " + cues::qc::letter (s) + (combo ? "  + load " + presetLocation (preset) : juce::String());
            t->colour = state::colourOf (scene);
            t->setTooltip ("Drag onto the timeline. Right-click to rename / recolour.");
            t->makeCue = [this, sel, s] { return sceneCue (sel, s); };
            t->onDoubleClick = [this, scene] { renameNode (this, scene, "Rename scene"); };
            t->onContextMenu = [this, scene, sel, s] { nodeMenu (scene, true, [this, sel, s] { return sceneCue (sel, s); }); };
            addAndMakeVisible (t);
        }

        stompTiles.clear();
        for (int f = 0; f < 8; ++f)
        {
            const auto stomp = nthOfType (preset, IDs::Stomp, f);
            auto* t = stompTiles.add (new Tile (proc));
            t->title = stomp[IDs::name].toString();
            t->subtitle = "FS " + cues::qc::letter (f) + (stompOn ? "  ON" : "  OFF");
            t->colour = stompOn ? juce::Colour (0xff3d4450) : juce::Colour (0xff2a2d33);
            t->makeCue = [this, stomp, f] { return cues::qc::stomp (qcChannel(), f, (bool) state[IDs::stompOn], stomp[IDs::name].toString()); };
            t->onDoubleClick = [this, stomp] { renameNode (this, stomp, "Rename footswitch"); };
            t->onContextMenu = [this, stomp] { nodeMenu (stomp, false, {}); };
            addAndMakeVisible (t);
        }

        utilTiles.clear();
        auto addUtil = [this] (const juce::String& title, const juce::String& sub, juce::Colour c, std::function<cues::Cue()> make)
        {
            auto* t = utilTiles.add (new Tile (proc));
            t->title = title;
            t->subtitle = sub;
            t->colour = c;
            t->makeCue = std::move (make);
            addAndMakeVisible (t);
        };

        addUtil ("Tuner On",  "CC#45 = 127", juce::Colour (0xffe8e8e8), [this] { return cues::qc::tuner (qcChannel(), true); });
        addUtil ("Tuner Off", "CC#45 = 0",   juce::Colour (0xff9a9a9a), [this] { return cues::qc::tuner (qcChannel(), false); });
        addUtil ("Preset Mode", "CC#47 = 0", juce::Colour (0xff4a4f5a), [this] { return cues::qc::gigMode (qcChannel(), 0); });
        addUtil ("Scene Mode",  "CC#47 = 1", juce::Colour (0xff4a4f5a), [this] { return cues::qc::gigMode (qcChannel(), 1); });
        addUtil ("Stomp Mode",  "CC#47 = 2", juce::Colour (0xff4a4f5a), [this] { return cues::qc::gigMode (qcChannel(), 2); });

        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);

        auto left = r.removeFromLeft (230);
        r.removeFromLeft (12);

        auto header = left.removeFromTop (26);
        addButton.setBounds (header.removeFromRight (84).reduced (0, 2));
        presetsLabel.setBounds (header);
        presetView.setBounds (left);

        const auto listWidth = presetView.getWidth() - presetView.getScrollBarThickness();
        constexpr int tileHeight = 54;
        presetList.setSize (listWidth, tileHeight * presetTiles.size());
        for (int i = 0; i < presetTiles.size(); ++i)
            presetTiles[i]->setBounds (0, i * tileHeight, listWidth, tileHeight);

        if (stripTile != nullptr)
            stripTile->setBounds (r.removeFromTop (46));

        r.removeFromTop (8);
        auto sceneHeader = r.removeFromTop (24);
        comboToggle.setBounds (sceneHeader.removeFromRight (280));
        scenesLabel.setBounds (sceneHeader);
        layoutGrid (sceneTiles, r.removeFromTop (juce::jmax (120, r.getHeight() / 3)), 4);

        r.removeFromTop (8);
        auto stompHeader = r.removeFromTop (24);
        stompOnToggle.setBounds (stompHeader.removeFromRight (280));
        stompsLabel.setBounds (stompHeader);
        layoutGrid (stompTiles, r.removeFromTop (58), 8);

        r.removeFromTop (8);
        utilLabel.setBounds (r.removeFromTop (24));
        layoutGrid (utilTiles, r.removeFromTop (54), 5);
    }

private:
    juce::ValueTree qcTree() const { return state.getChildWithName (IDs::QC); }
    int qcChannel() const          { return (int) state[IDs::qcChannel]; }

    int selectedIndex() const
    {
        return juce::jlimit (0, juce::jmax (0, qcTree().getNumChildren() - 1), (int) state[IDs::selectedPreset]);
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

    void addPreset()
    {
        auto qc = qcTree();
        auto setlist = 1, bank = 1, slot = 0;

        if (qc.getNumChildren() > 0)
        {
            const auto last = qc.getChild (qc.getNumChildren() - 1);
            setlist = (int) last[IDs::setlist];
            slot = (int) last[IDs::slot] + 1;
            bank = (int) last[IDs::bank] + slot / 8;
            slot %= 8;
            bank = juce::jlimit (1, 32, bank);
        }

        auto preset = state::createPreset ("Preset " + juce::String (bank) + cues::qc::letter (slot),
                                           setlist, bank, slot, paletteColour (qc.getNumChildren()));
        qc.appendChild (preset, nullptr);
        state.setProperty (IDs::selectedPreset, qc.getNumChildren() - 1, nullptr);
        editPresetDialog (this, preset);
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
                editPresetDialog (&page, preset);
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
                renameNode (safe.getComponent(), node, "Rename");
            else if (result == 2 && send)
                safe->proc.preview (send());
        });
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;

    juce::Label presetsLabel, scenesLabel, stompsLabel, utilLabel;
    juce::TextButton addButton { "+ Preset" };
    juce::Viewport presetView;
    juce::Component presetList;
    juce::ToggleButton comboToggle { "Scene tiles also load the preset" };
    juce::ToggleButton stompOnToggle { "Stomp tiles send ON" };

    juce::OwnedArray<Tile> presetTiles, sceneTiles, stompTiles, utilTiles;
    std::unique_ptr<Tile> stripTile;
};

//==============================================================================
class WhammyPage final : public PedalCuesEditor::Page
{
public:
    explicit WhammyPage (PedalCuesProcessor& p) : proc (p), state (p.state)
    {
        styleHeader (modesLabel, "MODES  (Program Change)");
        styleHeader (sweepsLabel, "TREADLE MOVES  (CC#11, tempo-synced)");
        styleHeader (lengthLabel, "Length");
        styleHeader (curveLabel, "Curve");

        for (auto* c : std::initializer_list<juce::Component*> { &modesLabel, &sweepsLabel, &lengthLabel, &curveLabel,
                                                                 &chordsToggle, &bypassToggle, &heelToggle, &resetToggle,
                                                                 &lengthBox, &curveSlider })
            addAndMakeVisible (c);

        chordsToggle.setTooltip ("Use the Chords (polyphonic) program range instead of Classic");
        bypassToggle.setTooltip ("Select the mode but leave the Whammy bypassed");
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
        curveSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
        curveSlider.setRange (0.25, 4.0, 0.01);
        curveSlider.setSkewFactorFromMidPoint (1.0);
        curveSlider.setTooltip ("1 = linear, <1 = fast start, >1 = slow start");
        curveSlider.onValueChange = [this] { state.setProperty (IDs::sweepCurve, curveSlider.getValue(), nullptr); };

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
            const auto category = cues::whammy::category (i);
            const juce::String group = category == cues::whammy::Category::whammy ? "WHAMMY"
                                     : category == cues::whammy::Category::detune ? "DETUNE" : "HARMONY";

            auto* t = modeTiles.add (new Tile (proc));
            t->title = node[IDs::name].toString();
            t->subtitle = group + "   PC " + juce::String (cues::whammy::programNumber (i, chords, bypass));
            t->colour = cues::whammy::colour (i);
            if (bypass)
                t->colour = t->colour.withSaturation (0.25f).darker (0.3f);
            t->setTooltip ("Drag onto the timeline to switch the Whammy to this mode. Right-click to rename.");
            t->makeCue = [this, i, node]
            {
                return cues::whammy::effect ((int) state[IDs::whChannel], i, node[IDs::name].toString(),
                                             (bool) state[IDs::whChords], (bool) state[IDs::whBypass],
                                             (int) state[IDs::whPcBase], (bool) state[IDs::whHeelFirst]);
            };
            t->onDoubleClick = [this, node] { renameNode (this, node, "Rename Whammy mode"); };
            t->onContextMenu = [this, node, i] { modeMenu (node, i); };
            addAndMakeVisible (t);
        }

        shapeTiles.clear();
        for (int s = 0; s < cues::whammy::numShapes; ++s)
        {
            const auto shape = (cues::whammy::Shape) s;
            auto* t = shapeTiles.add (new Tile (proc));
            t->title = cues::whammy::shapeName (shape);
            t->subtitle = (shape == cues::whammy::Shape::toe || shape == cues::whammy::Shape::heel)
                              ? "hold " + cues::formatBeats (beats)
                              : cues::formatBeats (beats);
            t->colour = juce::Colour (0xffc7801a).withRotatedHue (0.02f * (float) s);
            t->setTooltip (cues::whammy::shapeDescription (shape));
            t->makeCue = [this, shape]
            {
                return cues::whammy::sweep ((int) state[IDs::whChannel], shape, (double) state[IDs::sweepBeats],
                                            (double) state[IDs::sweepCurve], (bool) state[IDs::sweepReset]);
            };
            addAndMakeVisible (t);
        }

        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);

        auto toggles = r.removeFromTop (26);
        const auto third = toggles.getWidth() / 3;
        chordsToggle.setBounds (toggles.removeFromLeft (third));
        bypassToggle.setBounds (toggles.removeFromLeft (third));
        heelToggle.setBounds (toggles);

        r.removeFromTop (6);
        modesLabel.setBounds (r.removeFromTop (22));
        layoutGrid (modeTiles, r.removeFromTop (juce::jmax (180, r.getHeight() - 150)), 7);

        r.removeFromTop (10);
        sweepsLabel.setBounds (r.removeFromTop (22));

        auto controls = r.removeFromTop (28);
        lengthLabel.setBounds (controls.removeFromLeft (52));
        lengthBox.setBounds (controls.removeFromLeft (120).reduced (0, 2));
        controls.removeFromLeft (16);
        curveLabel.setBounds (controls.removeFromLeft (46));
        curveSlider.setBounds (controls.removeFromLeft (220));
        controls.removeFromLeft (16);
        resetToggle.setBounds (controls);

        r.removeFromTop (6);
        layoutGrid (shapeTiles, r.removeFromTop (60), cues::whammy::numShapes);
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
                renameNode (safe.getComponent(), node, "Rename Whammy mode");
            else if (result == 2)
                node.setProperty (IDs::name, cues::whammy::defaultName (index), nullptr);
            else if (result == 3 && juce::isPositiveAndBelow (index, safe->modeTiles.size()))
                safe->modeTiles[index]->sendNow();
        });
    }

    static constexpr std::array<double, 10> lengths { 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0 };

    PedalCuesProcessor& proc;
    juce::ValueTree state;

    juce::Label modesLabel, sweepsLabel, lengthLabel, curveLabel;
    juce::ToggleButton chordsToggle { "Chords mode" };
    juce::ToggleButton bypassToggle { "Load bypassed" };
    juce::ToggleButton heelToggle   { "Heel before mode change" };
    juce::ToggleButton resetToggle  { "Return to heel after move" };
    juce::ComboBox lengthBox;
    juce::Slider curveSlider;

    juce::OwnedArray<Tile> modeTiles, shapeTiles;
};

//==============================================================================
class SettingsPage final : public PedalCuesEditor::Page
{
public:
    explicit SettingsPage (PedalCuesProcessor& p) : state (p.state)
    {
        styleHeader (qcChannelLabel, "Quad Cortex MIDI channel");
        styleHeader (whChannelLabel, "Whammy V MIDI channel");
        styleHeader (pcBaseLabel, "Whammy program numbering");
        styleHeader (libraryLabel, "LIBRARY  (presets, scenes, stomps and Whammy names)");

        for (int ch = 1; ch <= 16; ++ch)
        {
            qcChannelBox.addItem ("Channel " + juce::String (ch), ch);
            whChannelBox.addItem ("Channel " + juce::String (ch), ch);
        }

        pcBaseBox.addItem ("As printed in the manual (1 = first program)", 1);
        pcBaseBox.addItem ("Zero-based (0 = first program)", 2);

        qcChannelBox.onChange = [this] { state.setProperty (IDs::qcChannel, qcChannelBox.getSelectedId(), nullptr); };
        whChannelBox.onChange = [this] { state.setProperty (IDs::whChannel, whChannelBox.getSelectedId(), nullptr); };
        pcBaseBox.onChange    = [this] { state.setProperty (IDs::whPcBase, pcBaseBox.getSelectedId() == 1 ? 1 : 0, nullptr); };
        setlistToggle.onClick = [this] { state.setProperty (IDs::sendSetlist, setlistToggle.getToggleState(), nullptr); };
        setlistToggle.setTooltip ("Also send CC#32 (setlist) before each preset change. Leave off if all presets are in the active setlist.");

        saveDefaultButton.onClick = [this]
        {
            setStatus (state::saveLibrary (state, state::defaultLibraryFile())
                           ? "Saved. New PedalCues instances will start with this library."
                           : "Could not save the default library.");
        };

        loadDefaultButton.onClick = [this]
        {
            setStatus (state::loadLibrary (state, state::defaultLibraryFile())
                           ? "Default library loaded."
                           : "No default library saved yet.");
        };

        exportButton.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Export PedalCues library",
                                                           juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                               .getChildFile ("PedalCues Library.xml"),
                                                           "*.xml");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto file = fc.getResult();
                                      if (file != juce::File())
                                          setStatus (state::saveLibrary (state, file.withFileExtension ("xml"))
                                                         ? "Exported to " + file.getFileName()
                                                         : "Export failed.");
                                  });
        };

        importButton.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Import PedalCues library",
                                                           juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                           "*.xml");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto file = fc.getResult();
                                      if (file.existsAsFile())
                                          setStatus (state::loadLibrary (state, file) ? "Imported " + file.getFileName()
                                                                                      : "Not a PedalCues library file.");
                                  });
        };

        statusLabel.setColour (juce::Label::textColourId, accent);

        help.setMultiLine (true);
        help.setReadOnly (true);
        help.setScrollbarsShown (true);
        help.setCaretVisible (false);
        help.setColour (juce::TextEditor::backgroundColourId, panel);
        help.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        help.setFont (juce::FontOptions (13.0f));
        help.setText (
            "HOW TO USE (Reaper)\n"
            "1. Put PedalCues on a track (e.g. 'Pedal Cues'). In the track's routing, add a MIDI Hardware Output\n"
            "   to the Quad Cortex USB MIDI port. Audio of this track is irrelevant.\n"
            "2. Set the channels above to match the pedals: QC Settings > MIDI Settings > MIDI Channel,\n"
            "   and on the Whammy hold the Whammy footswitch while powering up and turn the knob.\n"
            "3. Whammy via the QC: connect QC MIDI Out -> Whammy MIDI In and enable MIDI Thru on the QC.\n"
            "   Check that your CorOS version forwards USB MIDI to the 5-pin MIDI Out; if not, use a USB MIDI\n"
            "   interface for the Whammy instead. Use different channels for QC and Whammy.\n"
            "4. Drag any tile onto the arrangement: it becomes a named MIDI item at the drop position\n"
            "   (enable snapping to land exactly on bars). Click the play corner of a tile to test it live.\n"
            "5. Treadle moves are written in beats, so they follow the project tempo.\n\n"
            "NOTES\n"
            "- Scenes: CC#43 (0-7). Presets: CC#0 (page) [+ CC#32 setlist] + Program Change.\n"
            "- Whammy V programs: Classic 1-21 on / 22-42 bypassed, Chords 43-63 on / 64-84 bypassed.\n"
            "  If modes arrive one step off, switch 'Whammy program numbering'.\n"
            "- Save a default library once your presets and scenes are named: every new project starts with it.");

        for (auto* c : std::initializer_list<juce::Component*> { &qcChannelLabel, &whChannelLabel, &pcBaseLabel, &libraryLabel,
                                                                 &qcChannelBox, &whChannelBox, &pcBaseBox, &setlistToggle,
                                                                 &saveDefaultButton, &loadDefaultButton, &exportButton,
                                                                 &importButton, &statusLabel, &help })
            addAndMakeVisible (c);

        refresh();
    }

    void refresh() override
    {
        qcChannelBox.setSelectedId ((int) state[IDs::qcChannel], juce::dontSendNotification);
        whChannelBox.setSelectedId ((int) state[IDs::whChannel], juce::dontSendNotification);
        pcBaseBox.setSelectedId ((int) state[IDs::whPcBase] == 1 ? 1 : 2, juce::dontSendNotification);
        setlistToggle.setToggleState ((bool) state[IDs::sendSetlist], juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);

        auto row = [&r] { auto x = r.removeFromTop (28); r.removeFromTop (6); return x; };

        auto a = row();
        qcChannelLabel.setBounds (a.removeFromLeft (220));
        qcChannelBox.setBounds (a.removeFromLeft (160));

        auto b = row();
        whChannelLabel.setBounds (b.removeFromLeft (220));
        whChannelBox.setBounds (b.removeFromLeft (160));

        auto c = row();
        pcBaseLabel.setBounds (c.removeFromLeft (220));
        pcBaseBox.setBounds (c.removeFromLeft (360));

        setlistToggle.setBounds (row().removeFromLeft (420));

        r.removeFromTop (8);
        libraryLabel.setBounds (row());
        auto buttons = row();
        for (auto* btn : { &saveDefaultButton, &loadDefaultButton, &exportButton, &importButton })
        {
            btn->setBounds (buttons.removeFromLeft (170));
            buttons.removeFromLeft (8);
        }
        statusLabel.setBounds (row());

        r.removeFromTop (6);
        help.setBounds (r);
    }

private:
    void setStatus (const juce::String& text) { statusLabel.setText (text, juce::dontSendNotification); }

    juce::ValueTree state;

    juce::Label qcChannelLabel, whChannelLabel, pcBaseLabel, libraryLabel, statusLabel;
    juce::ComboBox qcChannelBox, whChannelBox, pcBaseBox;
    juce::ToggleButton setlistToggle { "Send setlist (CC#32) with preset changes" };
    juce::TextButton saveDefaultButton { "Save as default" };
    juce::TextButton loadDefaultButton { "Load default" };
    juce::TextButton exportButton { "Export..." };
    juce::TextButton importButton { "Import..." };
    juce::TextEditor help;
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace

//==============================================================================
PedalCuesEditor::PedalCuesEditor (PedalCuesProcessor& p)
    : AudioProcessorEditor (p), pedalProcessor (p), state (p.state)
{
    titleLabel.setText ("PedalCues", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (21.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, accent);
    addAndMakeVisible (titleLabel);

    tempoLabel.setJustificationType (juce::Justification::centredRight);
    tempoLabel.setColour (juce::Label::textColourId, dimText);
    addAndMakeVisible (tempoLabel);

    const char* names[] = { "Quad Cortex", "Whammy V", "Settings" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = tabButtons.add (new juce::TextButton (names[i]));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->setColour (juce::TextButton::buttonColourId, panel);
        b->setColour (juce::TextButton::buttonOnColourId, accent.darker (0.2f));
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        b->onClick = [this, i] { showPage (i); };
        addAndMakeVisible (b);
    }

    pages.push_back (std::make_unique<QcPage> (p));
    pages.push_back (std::make_unique<WhammyPage> (p));
    pages.push_back (std::make_unique<SettingsPage> (p));
    for (auto& page : pages)
        addChildComponent (*page);

    state.addListener (this);

    setResizable (true, true);
    setResizeLimits (860, 600, 2000, 1400);
    setSize (980, 660);

    showPage (0);
    timerCallback();
    startTimerHz (2);
}

PedalCuesEditor::~PedalCuesEditor()
{
    state.removeListener (this);
}

void PedalCuesEditor::showPage (int index)
{
    currentPage = index;
    for (int i = 0; i < (int) pages.size(); ++i)
    {
        pages[(size_t) i]->setVisible (i == index);
        tabButtons[i]->setToggleState (i == index, juce::dontSendNotification);
    }
}

void PedalCuesEditor::handleAsyncUpdate()
{
    for (auto& page : pages)
        page->refresh();
}

void PedalCuesEditor::timerCallback()
{
    tempoLabel.setText ("Host tempo " + juce::String (pedalProcessor.getHostBpm(), 1) + " BPM   |   drag tiles onto the timeline",
                        juce::dontSendNotification);
}

void PedalCuesEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    g.setColour (panel);
    g.fillRect (getLocalBounds().removeFromTop (48));
}

void PedalCuesEditor::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (48).reduced (12, 8);

    titleLabel.setBounds (header.removeFromLeft (130));
    for (auto* b : tabButtons)
    {
        b->setBounds (header.removeFromLeft (120));
        header.removeFromLeft (4);
    }
    tempoLabel.setBounds (header);

    for (auto& page : pages)
        page->setBounds (r);
}
