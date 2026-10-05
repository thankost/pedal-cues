#include "EditorCommon.h"
#include "DeviceTemplates.h"
#include "MovesPanel.h"

using namespace theme;

namespace ui
{
namespace
{
const juce::Colour customViolet { 0xff8e7cf0 };
constexpr int tileColumns = 4, tileHeight = 58, tileGap = 6, groupGap = 12;

std::unique_ptr<juce::FileChooser> unitChooser;   // one Export / Import unit dialog at a time

// One message in the tile editor: its type, then the numbers that type needs.
class MessageRow final : public juce::Component
{
public:
    using Step = cues::custom::Step;
    enum Type { program = 1, controller, bank };

    MessageRow (int base) : programBase (base)
    {
        typeBox.addItem ("Program Change", program);
        typeBox.addItem ("Control Change", controller);
        typeBox.addItem ("Bank select (CC#0)", bank);
        typeBox.setTooltip ("Program Change loads a preset. Control Change switches scenes, snapshots, effects or other settings "
                            "(see your device's MIDI chart). Bank select picks the bank before a Program Change.");
        typeBox.onChange = [this] { changed (true); };
        addAndMakeVisible (typeBox);

        for (auto* sl : { &numberSlider, &valueSlider })
        {
            sl->setSliderStyle (juce::Slider::IncDecButtons);
            sl->setTextBoxStyle (juce::Slider::TextBoxLeft, false, 52, 28);
            sl->setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
            sl->onValueChange = [this] { changed (false); };
            addAndMakeVisible (sl);
        }
        for (auto* l : { &numberLabel, &valueLabel })
        {
            styleCaption (*l, {});
            l->setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (l);
        }
        for (auto* b : { &onButton, &offButton })
        {
            b->setColour (juce::TextButton::buttonColourId, raised);
            addAndMakeVisible (b);
        }
        onButton.setTooltip ("Value 127: what most devices read as on");
        offButton.setTooltip ("Value 0: off");
        onButton.onClick  = [this] { valueSlider.setValue (127); };
        offButton.onClick = [this] { valueSlider.setValue (0); };

        removeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        removeButton.setColour (juce::TextButton::textColourOffId, dim);
        removeButton.setTooltip ("Remove this message");
        addAndMakeVisible (removeButton);
    }

    void setStep (const Step& st)
    {
        const auto type = st.kind == Step::Kind::program ? program : st.bank ? bank : controller;
        typeBox.setSelectedId (type, juce::dontSendNotification);
        configure();
        numberSlider.setValue (st.kind == Step::Kind::program ? st.number + programBase : st.bank ? st.value : st.number,
                               juce::dontSendNotification);
        valueSlider.setValue (st.value, juce::dontSendNotification);
    }

    Step step() const
    {
        Step st;
        const auto n = (int) numberSlider.getValue();
        switch (typeBox.getSelectedId())
        {
            case controller: st.kind = Step::Kind::controller; st.number = n; st.value = (int) valueSlider.getValue(); break;
            case bank:       st.kind = Step::Kind::controller; st.number = 0; st.value = n; st.bank = true; break;
            default:         st.kind = Step::Kind::program; st.number = n - programBase; break;
        }
        return st;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (0, 4);
        removeButton.setBounds (r.removeFromRight (30));
        r.removeFromRight (6);
        typeBox.setBounds (r.removeFromLeft (190));
        r.removeFromLeft (8);
        numberLabel.setBounds (r.removeFromLeft (70));
        r.removeFromLeft (6);
        numberSlider.setBounds (r.removeFromLeft (110));
        r.removeFromLeft (12);
        valueLabel.setBounds (r.removeFromLeft (46));
        r.removeFromLeft (6);
        valueSlider.setBounds (r.removeFromLeft (110));
        r.removeFromLeft (8);
        onButton.setBounds (r.removeFromLeft (44));
        r.removeFromLeft (4);
        offButton.setBounds (r.removeFromLeft (44));
    }

    std::function<void()> onChange;
    juce::TextButton removeButton { "x" };

private:
    void configure()
    {
        const auto type = typeBox.getSelectedId();
        const auto isCc = type == controller;
        valueSlider.setVisible (isCc);
        valueLabel.setVisible (isCc);
        onButton.setVisible (isCc);
        offButton.setVisible (isCc);
        valueLabel.setText ("Value", juce::dontSendNotification);

        if (type == program)
        {
            numberLabel.setText ("Program", juce::dontSendNotification);
            numberSlider.setRange (programBase, 127 + programBase, 1);
        }
        else if (type == controller)
        {
            numberLabel.setText ("CC#", juce::dontSendNotification);
            numberSlider.setRange (0, 127, 1);
            valueSlider.setRange (0, 127, 1);
        }
        else
        {
            numberLabel.setText ("Bank", juce::dontSendNotification);
            numberSlider.setRange (0, 127, 1);
        }
    }

    void changed (bool typeChanged)
    {
        if (typeChanged)
            configure();
        if (onChange)
            onChange();
    }

    const int programBase;
    juce::ComboBox typeBox;
    juce::Slider numberSlider, valueSlider;
    juce::Label numberLabel, valueLabel;
    juce::TextButton onButton { "On" }, offButton { "Off" };
};

// The tile editor, set up like a MIDI mapping window: a name, the messages one per row (with ready-made
// starting points), a note, a plain-words preview and a Test button.
class TileEditor final : public juce::Component
{
public:
    TileEditor (PedalCuesProcessor& p, juce::ValueTree t, bool newTile)
        : proc (p), root (p.state), tile (t), isNew (newTile), programBase ((int) t.getParent().getParent()[IDs::programBase])
    {
        styleCaption (nameLabel, "NAME");
        styleCaption (startLabel, "START FROM");
        styleCaption (messagesLabel, "MIDI MESSAGES, SENT IN ORDER");
        styleCaption (noteLabel, "NOTE (OPTIONAL)");
        for (auto* l : { &nameLabel, &startLabel, &messagesLabel, &noteLabel })
            addAndMakeVisible (l);

        nameEditor.setText (tile[IDs::name].toString());
        nameEditor.setTextToShowWhenEmpty ("Verse, Clean, Delay on...", dim);
        noteEditor.setText (tile[IDs::note].toString());
        noteEditor.setTextToShowWhenEmpty ("Why it's set up this way, for you and anyone you share the device with", dim);
        for (auto* e : { &nameEditor, &noteEditor })
        {
            e->setFont (font (14.0f));
            addAndMakeVisible (e);
        }

        // Ready-made starting points: they replace the messages below.
        struct Start { const char* label; const char* tip; const char* messages; };
        const Start starts[] = {
            { "Preset",         "Load a preset (also called a patch, program or rig): one Program Change", "PC #" },
            { "Bank + preset",  "Select the bank, then the preset: for devices with more than 128 presets", "bank 0, PC #" },
            { "Switch on/off",  "Switch something on or off (an effect, a scene, a loop, a mute...): one Control Change, "
                                "127 = on, 0 = off", "CC 50=127" },
            { "Set a value",    "Set a parameter or pick an option (a scene, a level, a mode...): one Control Change "
                                "with the value from the device's MIDI chart", "CC 34=0" },
        };
        for (const auto& st : starts)
        {
            auto* b = startButtons.add (new juce::TextButton (st.label));
            b->setTooltip (juce::String (st.tip) + ". Then set the numbers from your device's MIDI chart.");
            b->setColour (juce::TextButton::buttonColourId, raised);
            const auto start = juce::String (st.messages).replace ("#", juce::String (programBase));
            b->onClick = [this, start] { setSteps (cues::custom::parse (start, programBase).steps); };
            addAndMakeVisible (b);
        }

        addButton.setColour (juce::TextButton::buttonColourId, raised);
        addButton.onClick = [this]
        {
            auto steps = currentSteps();
            cues::custom::Step st;
            st.kind = cues::custom::Step::Kind::controller;
            steps.push_back (st);
            setSteps (steps);
        };
        addAndMakeVisible (addButton);

        preview.setFont (font (13.0f));
        preview.setColour (juce::Label::textColourId, text);
        preview.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (preview);

        testButton.setColour (juce::TextButton::buttonColourId, raised);
        testButton.setTooltip ("Send these messages to the device now");
        testButton.onClick = [this]
        {
            proc.preview (cues::custom::cue (state::channelFor (root, tile.getParent().getParent()), nameEditor.getText(), messagesText(), programBase));
        };
        saveButton.setColour (juce::TextButton::buttonColourId, accent);
        saveButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        saveButton.onClick = [this] { save(); };
        cancelButton.setColour (juce::TextButton::buttonColourId, raised);
        cancelButton.onClick = [this] { close(); };
        for (auto* b : { &testButton, &saveButton, &cancelButton })
            addAndMakeVisible (b);

        setSteps (cues::custom::parse (tile[IDs::messages].toString(), programBase).steps);
        setSize (760, 600);
    }

    ~TileEditor() override
    {
        if (isNew && ! saved)   // cancelled a new tile: don't leave an empty one behind
            tile.getParent().removeChild (tile, nullptr);
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (22, 18);
        nameLabel.setBounds (r.removeFromTop (20));
        nameEditor.setBounds (r.removeFromTop (32).withWidth (360));
        r.removeFromTop (14);

        startLabel.setBounds (r.removeFromTop (20));
        {
            auto row = r.removeFromTop (32);
            const auto w = (row.getWidth() - 6 * (startButtons.size() - 1)) / startButtons.size();
            for (auto* b : startButtons)
            {
                b->setBounds (row.removeFromLeft (w));
                row.removeFromLeft (6);
            }
        }
        r.removeFromTop (14);

        auto buttons = r.removeFromBottom (34);
        cancelButton.setBounds (buttons.removeFromRight (100));
        buttons.removeFromRight (8);
        saveButton.setBounds (buttons.removeFromRight (100));
        testButton.setBounds (buttons.removeFromLeft (150));
        r.removeFromBottom (12);

        auto noteArea = r.removeFromBottom (54);
        noteLabel.setBounds (noteArea.removeFromTop (20));
        noteEditor.setBounds (noteArea.removeFromTop (32));
        r.removeFromBottom (10);
        preview.setBounds (r.removeFromBottom (44));
        r.removeFromBottom (6);

        messagesLabel.setBounds (r.removeFromTop (20));
        for (auto* row : rows)
            row->setBounds (r.removeFromTop (40));
        addButton.setBounds (r.removeFromTop (34).withWidth (150).translated (0, 4));
        addButton.setEnabled (rows.size() < maxRows);
    }

private:
    static constexpr int maxRows = 8;

    std::vector<cues::custom::Step> currentSteps() const
    {
        std::vector<cues::custom::Step> steps;
        for (auto* row : rows)
            steps.push_back (row->step());
        return steps;
    }

    juce::String messagesText() const
    {
        cues::custom::Parsed p;
        p.steps = currentSteps();
        return cues::custom::describe (p, programBase);
    }

    void setSteps (std::vector<cues::custom::Step> steps)
    {
        if (steps.size() > (size_t) maxRows)
            steps.resize ((size_t) maxRows);
        rows.clear();
        for (const auto& st : steps)
        {
            auto* row = rows.add (new MessageRow (programBase));
            row->setStep (st);
            row->onChange = [this] { updatePreview(); };
            row->removeButton.onClick = [this, row]
            {
                juce::Component::SafePointer<TileEditor> safe (this);   // the row (and this button) go away in setSteps
                juce::MessageManager::callAsync ([safe, row]
                {
                    if (safe == nullptr)
                        return;
                    auto steps = safe->currentSteps();
                    steps.erase (steps.begin() + safe->rows.indexOf (row));
                    safe->setSteps (steps);
                });
            };
            addAndMakeVisible (row);
        }
        updatePreview();
        resized();
    }

    // The messages in plain words, so it's clear what the tile does.
    void updatePreview()
    {
        const auto steps = currentSteps();
        juce::StringArray parts;
        for (const auto& st : steps)
        {
            using K = cues::custom::Step::Kind;
            parts.add (st.kind == K::program ? "load program " + juce::String (st.number + programBase)
                       : st.bank ? "select bank " + juce::String (st.value)
                                 : "CC#" + juce::String (st.number) + " to " + juce::String (st.value));
        }
        preview.setText (steps.empty() ? "No messages yet: pick a starting point above, or + Add message."
                                       : "On channel " + juce::String (state::channelFor (root, tile.getParent().getParent())) + ": " + parts.joinIntoString (", then ") + ".",
                         juce::dontSendNotification);
        testButton.setEnabled (! steps.empty());
        saveButton.setEnabled (! steps.empty());
    }

    void save()
    {
        tile.setProperty (IDs::name, nameEditor.getText().trim().isNotEmpty() ? nameEditor.getText().trim() : juce::String ("Tile"), nullptr);
        tile.setProperty (IDs::messages, messagesText(), nullptr);
        tile.setProperty (IDs::note, noteEditor.getText().trim(), nullptr);
        saved = true;
        close();
    }

    void close()
    {
        if (auto* w = findParentComponentOfClass<juce::DialogWindow>())
            w->exitModalState (0);
    }

    PedalCuesProcessor& proc;
    juce::ValueTree root, tile;
    const bool isNew;
    const int programBase;
    bool saved = false;

    juce::Label nameLabel, startLabel, messagesLabel, noteLabel, preview;
    juce::TextEditor nameEditor, noteEditor;
    juce::OwnedArray<juce::TextButton> startButtons;
    juce::OwnedArray<MessageRow> rows;
    juce::TextButton addButton { "+ Add message" };
    juce::TextButton testButton { "Test on the device" }, saveButton { "Save" }, cancelButton { "Cancel" };
};

void showTileEditor (PedalCuesProcessor& proc, juce::ValueTree tile, bool isNew)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (makeTileEditor (proc, tile, isNew).release());
    o.dialogTitle = isNew ? "New tile" : "Edit tile";
    o.dialogBackgroundColour = background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

//==============================================================================
// Expression moves for a custom device: the same shapes, Set to tiles and Draw as the other pages, on the CC the
// device's expression pedal (or whatever it assigns) listens to. The CC is part of the device (exported with it).
class CustomExpression final : public Page
{
public:
    CustomExpression (PedalCuesProcessor& p, bool pedalsTab) : proc (p), state (p.state), fx (pedalsTab)
    {
        addAndMakeVisible (moves);
        for (int cc = 0; cc < 128; ++cc)
            ccBox.addItem ("CC#" + juce::String (cc), cc + 1);   // ids start at 1; CC#0 is a control on some units (Exponent 500 Quick-Pot A)   // plain numbers: what a CC does is up to the device
        ccBox.setTooltip ("The CC this device listens to for the move: the one you assigned on the device (see its manual). "
                          "CC#11 is the default because it's the MIDI standard for expression, but any CC works.");
        ccBox.onChange = [this]
        {
            auto u = unit();
            if (u.isValid() && ccBox.getSelectedId() > 0)
                u.setProperty (IDs::expCc, ccBox.getSelectedId() - 1, nullptr);
        };
        moves.extraHeader().addAndMakeVisible (ccBox);
        styleCaption (setLabel, "SET TO");
        moves.extraRow().addAndMakeVisible (setLabel);
        refresh();
    }

    void refresh() override
    {
        ccBox.setSelectedId (controller() + 1, juce::dontSendNotification);
        moves.setHint ("CC#" + juce::String (controller()) + "  -  what the device assigns to it");

        setTiles.clear();
        static const std::pair<float, const char*> positions[] = { { 0.0f, "Heel" }, { 0.25f, "25%" }, { 0.5f, "Half" }, { 0.75f, "75%" }, { 1.0f, "Toe" } };
        for (const auto& [pos, label] : positions)
        {
            const auto value = juce::roundToInt (pos * 127.0f);
            auto* t = setTiles.add (new Tile (proc, Tile::Look::utility));
            t->title = label;
            t->subtitle = "CC#" + juce::String (controller()) + " = " + juce::String (value);
            t->colour = customViolet.interpolatedWith (raised, 0.6f - 0.6f * pos);
            t->setTooltip ("Puts the pedal at " + juce::String (label).toLowerCase() + ".");
            const auto text = juce::String (label);
            t->makeCue = [this, value, text]
            {
                cues::Cue c;
                c.name = prefix() + text;
                c.add (0.0, juce::MidiMessage::controllerEvent (juce::jlimit (1, 16, channel()), controller(), value));
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
        auto h = moves.extraHeader().getLocalBounds();
        ccBox.setBounds (h.removeFromLeft (190).reduced (0, 2));
        auto row = moves.extraRow().getLocalBounds();
        setLabel.setBounds (row.removeFromLeft (64));
        layoutGrid (setTiles, row.expanded (3, 0), setTiles.size(), 0);
    }

private:
    int controller() const
    {
        const auto u = unit();
        return u.isValid() ? juce::jlimit (0, 127, (int) u.getProperty (IDs::expCc, 11)) : 11;
    }

    juce::String prefix() const
    {
        const auto u = unit();
        return (u.isValid() ? u[IDs::name].toString() : juce::String ("Device")) + " Exp ";
    }

    MovesConfig config()
    {
        MovesConfig c;
        c.idPrefix = "cu.exp";
        c.title = "Expression";
        c.hint = "CC#11";
        c.colour = customViolet;
        c.line = customViolet.brighter (0.3f);
        c.beatsId = IDs::cuBeats;  c.curveId = IDs::cuCurve;  c.resetId = IDs::cuReset;
        c.drawId = IDs::cuDraw;    c.drawingId = IDs::cuDrawing;  c.drawingNameId = IDs::cuDrawingName;
        c.resetText = "Back to heel after move";
        c.resetTooltip = "After a move, put the pedal back to heel (0). Leave it off for a swell that should stay up.";
        c.padHint = "Drag here to draw a pedal move";
        c.padTooltip = "Drag to draw the pedal move: bottom = heel, top = toe. Hold Shift to snap to quarter steps.";
        c.drawnTooltip = "Your drawn pedal move. Drag onto the timeline where the move should start.";
        c.shapesTooltip = "Ready-made pedal moves";
        c.drawTooltip = "Draw your own pedal move with the mouse";
        c.tileNote = " It moves what the device assigns to this CC.";
        c.numShapes = cues::qc::numExpShapes;
        c.shapeRows = 2;
        c.extraHeaderWidth = 200;
        c.extraRowHeight = 44;
        c.shapeName        = [] (int s) { return cues::qc::expShapeName ((cues::qc::ExpShape) s); };
        c.shapeDescription = [] (int s) { return cues::qc::expShapeDescription ((cues::qc::ExpShape) s); };
        c.shapeHolds       = [] (int s) { return s == (int) cues::qc::ExpShape::toe || s == (int) cues::qc::ExpShape::heel; };
        c.makeShape = [this] (int s)
        {
            return cues::qc::shapedMove (prefix(), channel(), controller(), (cues::qc::ExpShape) s,
                                         (double) state[IDs::cuBeats], (double) state[IDs::cuCurve], (bool) state[IDs::cuReset]);
        };
        c.controller = [this] { return controller(); };
        c.makeDrawn = [this] (const std::vector<float>& points, const juce::String& name)
        {
            return cues::qc::drawnMove (prefix() + (name.isNotEmpty() ? name : juce::String ("Drawn")), channel(), controller(),
                                        points, (double) state[IDs::cuBeats], (bool) state[IDs::cuReset]);
        };
        return c;
    }

    juce::ValueTree unit() const { return fx ? state::fxCustomUnit (state) : state::customUnit (state); }
    int channel() const          { return state::channelFor (state, unit()); }

    PedalCuesProcessor& proc;
    juce::ValueTree state;
    const bool fx;
    juce::ComboBox ccBox;
    juce::Label setLabel;
    juce::OwnedArray<Tile> setTiles;
    MovesPanel moves { proc, config() };
};

// A custom unit (beta): your own groups of tiles, each a short list of MIDI messages, plus notes.
class CustomPage final : public Page
{
public:
    CustomPage (PedalCuesProcessor& p, bool pedalsTab) : proc (p), state (p.state), fx (pedalsTab)
    {
        addAndMakeVisible (unitSection);

        unitMenuButton.setComponentID ("cu.unitMenu");
        unitMenuButton.setTooltip ("Rename, recolour, duplicate or delete this device; export it to share, or import one");
        unitMenuButton.onClick = [this] { unitMenu(); };
        addAndMakeVisible (unitMenuButton);

        nameLabel.setFont (font (22.0f, true));
        nameLabel.setColour (juce::Label::textColourId, text);
        nameLabel.setTooltip ("Double-click to rename");
        nameLabel.setEditable (false, true, false);
        nameLabel.onTextChange = [this]
        {
            if (nameLabel.getText().trim().isNotEmpty())
                unit().setProperty (IDs::name, nameLabel.getText().trim(), nullptr);
        };
        addAndMakeVisible (nameLabel);

        styleCaption (channelLabel, {});
        addAndMakeVisible (channelLabel);

        // Made from a Fractal / Line 6 template: say where the numbers come from, and that they're untested.
        disclaimerLabel.setFont (font (12.0f, true));
        disclaimerLabel.setColour (juce::Label::textColourId, accent);
        disclaimerLabel.setJustificationType (juce::Justification::topLeft);
        addChildComponent (disclaimerLabel);
        aboutButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        aboutButton.setColour (juce::TextButton::textColourOffId, qcBlue);
        aboutButton.setTooltip ("What the numbers come from and what to set on your unit. Kept by PedalCues (updates keep it current); "
                                "write your own notes below.");
        aboutButton.onClick = [this]
        {
            if (const auto* t = templates::find (unit()[IDs::templateId].toString()))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, t->model, t->notes);
        };
        addChildComponent (aboutButton);

        programBox.addItem ("Programs count from 0 (PC 0 = first)", 1);
        programBox.addItem ("Programs count from 1 (PC 1 = first)", 2);
        programBox.setTooltip ("How your device's manual numbers programs. PC numbers in your tiles use the same counting.");
        programBox.onChange = [this] { unit().setProperty (IDs::programBase, programBox.getSelectedId() == 2 ? 1 : 0, nullptr); };
        addAndMakeVisible (programBox);

        styleCaption (notesLabel, "NOTES");
        addAndMakeVisible (notesLabel);
        notesEditor.setComponentID ("cu.notes");
        notesEditor.setMultiLine (true, true);
        notesEditor.setReturnKeyStartsNewLine (true);
        notesEditor.setScrollbarsShown (true);
        notesEditor.setFont (font (13.0f));
        notesEditor.setTextToShowWhenEmpty ("Why it's set up this way: manual pages, parameters, values...", dim);
        notesEditor.onTextChange = [this] { unit().setProperty (IDs::notes, notesEditor.getText(), nullptr); };
        addAndMakeVisible (notesEditor);

        for (auto* b : { &exportButton, &importButton })
        {
            b->setColour (juce::TextButton::buttonColourId, raised);
            addAndMakeVisible (b);
        }
        exportButton.setTooltip ("Save this device (groups, tiles and notes) as a file to share or back up");
        importButton.setTooltip ("Add a device from a .pedalcues-device file");
        exportButton.onClick = [this] { exportCustomUnit (unit()); };
        importButton.onClick = [this] { importCustomUnit (state, fx); };

        groupsView.setViewedComponent (&groupsContent, false);
        groupsView.setScrollBarsShown (true, false);
        groupsView.setScrollBarThickness (8);
        addAndMakeVisible (groupsView);

        search.setComponentID ("cu.search");
        search.onSearch = [this] { resized(); groupsView.setViewPosition (0, 0); };
        addAndMakeVisible (search);

        addGroupButton.setComponentID ("cu.addGroup");
        addGroupButton.setColour (juce::TextButton::buttonColourId, raised);
        addGroupButton.setTooltip ("Add a group of tiles, for example Presets, Scenes, Snapshots or Effects");
        addGroupButton.onClick = [this] { addGroup(); };
        groupsContent.addAndMakeVisible (addGroupButton);

        // Tiles or Expression on the right.
        const char* views[] = { "Tiles", "Expression" };
        for (int i = 0; i < 2; ++i)
        {
            auto* b = viewButtons.add (new juce::TextButton (views[i]));
            b->setComponentID (i == 0 ? "cu.viewTiles" : "cu.viewExpression");
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4501);
            b->setColour (juce::TextButton::buttonColourId, surface);
            b->setColour (juce::TextButton::buttonOnColourId, customViolet);
            b->setColour (juce::TextButton::textColourOffId, dim);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            b->setConnectedEdges (i == 0 ? juce::Button::ConnectedOnRight : juce::Button::ConnectedOnLeft);
            b->onClick = [this, i, b] { if (b->getToggleState()) state.setProperty (IDs::cuExpressionView, i == 1, nullptr); };
            addAndMakeVisible (b);
        }
        viewButtons[1]->setTooltip ("Pedal moves for this device: swells, fades, wah, Set to tiles, or draw your own, on the CC you pick");
        addChildComponent (expression);

        refresh();
    }

    void refresh() override
    {
        const auto u = unit();
        if (! u.isValid())
            return;
        const auto base = (int) u[IDs::programBase];
        const auto name = u[IDs::name].toString();

        unitSection.accentColour = state::colourOf (u, customViolet);
        unitSection.repaint();
        if (! nameLabel.isBeingEdited())
            nameLabel.setText (name, juce::dontSendNotification);
        channelLabel.setText ("Sends on channel " + juce::String (state::channelFor (state, u)) + " (set it at the top of this tab)", juce::dontSendNotification);
        const auto* fromTemplate = templates::find (u[IDs::templateId].toString());
        disclaimerLabel.setText (fromTemplate != nullptr ? templates::disclaimer (*fromTemplate) : juce::String(), juce::dontSendNotification);
        disclaimerLabel.setVisible (fromTemplate != nullptr);
        aboutButton.setVisible (fromTemplate != nullptr);
        aboutButton.setButtonText (fromTemplate != nullptr && fromTemplate->source.isNotEmpty() ? juce::String ("About this unit (where the numbers come from) >")
                                                                                                : juce::String ("About this unit (from the manual) >"));
        notesLabel.setText (fromTemplate != nullptr ? "YOUR NOTES" : "NOTES", juce::dontSendNotification);
        notesEditor.setTextToShowWhenEmpty (fromTemplate != nullptr ? "What you set on your unit, what you changed..."
                                                                    : "Why it's set up this way: manual pages, parameters, values...", dim);
        programBox.setSelectedId (base == 1 ? 2 : 1, juce::dontSendNotification);
        for (auto* b : viewButtons)
            b->setColour (juce::TextButton::buttonOnColourId, unitSection.accentColour);   // the device's colour, like its tab
        const auto showExpression = (bool) state[IDs::cuExpressionView];
        viewButtons[showExpression ? 1 : 0]->setToggleState (true, juce::dontSendNotification);
        expression.setVisible (showExpression);
        search.setVisible (! showExpression);
        groupsView.setVisible (! showExpression);
        if (showExpression)
            expression.refresh();
        if (! notesEditor.hasKeyboardFocus (true) && notesEditor.getText() != u[IDs::notes].toString())
            notesEditor.setText (u[IDs::notes].toString(), false);

        groupSections.clear();
        groupButtons.clear();
        tiles.clear();
        tileGroup.clear();
        tileText.clear();
        tileExact.clear();

        for (int gi = 0; gi < u.getNumChildren(); ++gi)
        {
            const auto group = u.getChild (gi);
            const auto count = group.getNumChildren();
            auto* section = groupSections.add (new Section ("cu.group" + juce::String (gi), group[IDs::name].toString(),
                                                            count == 0 ? juce::String ("empty: click + Tile")
                                                                       : juce::String (count) + (count == 1 ? " tile" : " tiles"),
                                                            unitSection.accentColour));
            groupsContent.addAndMakeVisible (section);
            section->toBack();

            auto* add = groupButtons.add (new juce::TextButton ("+ Tile"));
            add->setColour (juce::TextButton::buttonColourId, accent);
            add->setColour (juce::TextButton::textColourOffId, juce::Colours::black);
            add->setTooltip ("Add a tile to " + group[IDs::name].toString());
            add->onClick = [this, group] { addTile (group); };
            groupsContent.addAndMakeVisible (add);

            auto* more = groupButtons.add (new juce::TextButton ("..."));
            more->setColour (juce::TextButton::buttonColourId, raised);
            more->setTooltip ("Rename, move or delete this group");
            more->onClick = [this, group] { groupMenu (group); };
            groupsContent.addAndMakeVisible (more);

            for (int ti = 0; ti < count; ++ti)
            {
                const auto tile = group.getChild (ti);
                const auto messages = tile[IDs::messages].toString();
                const auto parsed = cues::custom::parse (messages, base);
                const auto note = tile[IDs::note].toString();

                auto* t = tiles.add (new Tile (proc, Tile::Look::utility));
                tileGroup.add (gi);
                tileText.add (tile[IDs::name].toString());
                tileExact.add ((parsed.ok() ? cues::custom::describe (parsed, base) : juce::String())
                               + "  " + note + "  " + group[IDs::name].toString());
                t->title = tile[IDs::name].toString();
                t->subtitle = parsed.ok() ? cues::custom::describe (parsed, base) : juce::String ("Check the messages");
                t->colour = state::colourOf (tile);
                t->active = parsed.ok();
                t->setTooltip ((note.isNotEmpty() ? note + "\n\n" : juce::String())
                               + (parsed.ok() ? cues::custom::describe (parsed, base) : parsed.error)
                               + "\nDrag onto the timeline. Double-click to edit, right-click for more.");
                if (parsed.ok())
                    t->makeCue = [this, tile] { return tileCue (tile); };
                t->onDoubleClick = [this, tile] { showTileEditor (proc, tile, false); };
                t->onContextMenu = [this, tile] { tileMenu (tile); };
                groupsContent.addAndMakeVisible (t);
            }
        }

        resized();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);

        // Left: the unit card (name, numbering, notes, export / import).
        unitSection.setBounds (r.removeFromLeft (juce::jlimit (260, 330, r.getWidth() / 4)));
        r.removeFromLeft (12);
        unitMenuButton.setBounds (unitSection.headerArea().removeFromRight (44));
        {
            auto c = unitSection.contentArea().reduced (4, 2);
            nameLabel.setBounds (c.removeFromTop (34));
            channelLabel.setBounds (c.removeFromTop (20));
            if (disclaimerLabel.isVisible())
            {
                disclaimerLabel.setBounds (c.removeFromTop (48).withTrimmedTop (6));
                aboutButton.setBounds (c.removeFromTop (26).withTrimmedLeft (-6).withWidth (260));
            }
            c.removeFromTop (8);
            programBox.setBounds (c.removeFromTop (30));
            c.removeFromTop (12);
            auto buttons = c.removeFromBottom (32);
            exportButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
            buttons.removeFromLeft (8);
            importButton.setBounds (buttons);
            c.removeFromBottom (10);
            notesLabel.setBounds (c.removeFromTop (20));
            notesEditor.setBounds (c);
        }

        // Right: a search, then the groups, stacked and scrollable. While searching, only matching tiles show,
        // and groups with none are hidden.
        {
            auto top = r.removeFromTop (32);
            auto views = top.removeFromLeft (240);
            viewButtons[0]->setBounds (views.removeFromLeft (120));
            viewButtons[1]->setBounds (views);
            top.removeFromLeft (12);
            search.setBounds (top.withWidth (juce::jmin (top.getWidth(), 360)));
        }
        r.removeFromTop (10);
        expression.setBounds (r);
        groupsView.setBounds (r);
        const auto query = search.getText().trim();
        for (int ti = 0; ti < tiles.size(); ++ti)
            tiles[ti]->setVisible (fuzzy::score (query, tileText[ti], tileExact[ti]) >= 0);

        const auto width = r.getWidth() - groupsView.getScrollBarThickness() - 4;
        int y = 0;
        for (int gi = 0; gi < groupSections.size(); ++gi)
        {
            int count = 0;
            for (int ti = 0; ti < tiles.size(); ++ti)
                count += (tileGroup[ti] == gi && tiles[ti]->isVisible()) ? 1 : 0;
            const auto showGroup = query.isEmpty() || count > 0;
            groupSections[gi]->setVisible (showGroup);
            groupButtons[gi * 2]->setVisible (showGroup);
            groupButtons[gi * 2 + 1]->setVisible (showGroup);
            if (! showGroup)
                continue;
            const auto rows = juce::jmax (1, (count + tileColumns - 1) / tileColumns);
            const auto height = Section::headerHeight + rows * (tileHeight + tileGap) - tileGap + Section::padding;
            auto* section = groupSections[gi];
            section->setBounds (0, y, width, height);

            auto header = section->headerArea();
            groupButtons[gi * 2 + 1]->setBounds (header.removeFromRight (40));
            header.removeFromRight (6);
            groupButtons[gi * 2]->setBounds (header.removeFromRight (80));

            const auto area = section->contentArea().expanded (3);
            const auto cellW = (area.getWidth() + tileGap) / tileColumns;
            int i = 0;
            for (int ti = 0; ti < tiles.size(); ++ti)
                if (tileGroup[ti] == gi && tiles[ti]->isVisible())
                {
                    tiles[ti]->setBounds (area.getX() + (i % tileColumns) * cellW, area.getY() + (i / tileColumns) * (tileHeight + tileGap),
                                          cellW - tileGap, tileHeight);
                    ++i;
                }
            y += height + groupGap;
        }
        addGroupButton.setVisible (query.isEmpty());
        addGroupButton.setBounds (0, y, 140, 32);
        groupsContent.setSize (width, y + 40);
    }

private:
    juce::ValueTree unit() const { return fx ? state::fxCustomUnit (state) : state::customUnit (state); }

    cues::Cue tileCue (const juce::ValueTree& tile) const
    {
        const auto u = unit();
        auto c = cues::custom::cue (state::channelFor (state, u), u[IDs::name].toString() + " " + tile[IDs::name].toString(),
                                    tile[IDs::messages].toString(), (int) u[IDs::programBase]);
        if (state::usesCc0AsControl (u))
        {
            c.toggles = false;       // CC#0 is a control on this device (Infinity: Compression): repeat instead of sending CC#0 = 0
            c.cc0IsControl = true;   // and a Program Change-only tile is padded with CC#32 = 0
        }
        return c;
    }

    void addGroup()
    {
        askText ("New group", "Scenes", [this] (const juce::String& name)
        {
            juce::ValueTree g (IDs::Group);
            g.setProperty (IDs::name, name.isNotEmpty() ? name : juce::String ("Group"), nullptr);
            unit().appendChild (g, nullptr);
        });
    }

    void addTile (juce::ValueTree group)
    {
        juce::ValueTree t (IDs::CueTile);
        t.setProperty (IDs::name, "New tile", nullptr);
        t.setProperty (IDs::messages, "PC " + juce::String ((int) unit()[IDs::programBase]), nullptr);
        t.setProperty (IDs::note, juce::String(), nullptr);
        t.setProperty (IDs::colour, paletteColour (group.getNumChildren() + 4).toString(), nullptr);
        group.appendChild (t, nullptr);
        showTileEditor (proc, t, true);
    }

    void tileMenu (juce::ValueTree tile)
    {
        auto group = tile.getParent();
        const auto at = group.indexOf (tile);
        juce::PopupMenu m;
        m.addItem (1, "Edit...");
        m.addSubMenu ("Colour", colourMenu (state::colourOf (tile), 100));
        m.addSeparator();
        m.addItem (2, "Duplicate");
        m.addItem (3, "Move left", at > 0);
        m.addItem (4, "Move right", at < group.getNumChildren() - 1);
        m.addItem (5, "Delete");
        m.addSeparator();
        m.addItem (6, "Send to device now", cues::custom::parse (tile[IDs::messages].toString(), (int) unit()[IDs::programBase]).ok());

        juce::Component::SafePointer<CustomPage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options(), [safe, tile, group, at] (int result) mutable
        {
            if (safe == nullptr || result == 0)
                return;
            if (result >= 100)
                tile.setProperty (IDs::colour, paletteColour (result - 100).toString(), nullptr);
            else if (result == 1)
                showTileEditor (safe->proc, tile, false);
            else if (result == 2)
                group.addChild (tile.createCopy(), at + 1, nullptr);
            else if (result == 3)
                group.moveChild (at, at - 1, nullptr);
            else if (result == 4)
                group.moveChild (at, at + 1, nullptr);
            else if (result == 5)
                group.removeChild (tile, nullptr);
            else if (result == 6)
                safe->proc.preview (safe->tileCue (tile));
        });
    }

    void groupMenu (juce::ValueTree group)
    {
        auto u = unit();
        const auto at = u.indexOf (group);
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        m.addItem (2, "Move up", at > 0);
        m.addItem (3, "Move down", at < u.getNumChildren() - 1);
        m.addSeparator();
        m.addItem (4, group.getNumChildren() > 0 ? "Delete group and its tiles..." : "Delete group");

        m.showMenuAsync (juce::PopupMenu::Options(), [group, u, at] (int result) mutable
        {
            if (result == 1)
                renameNode (group, "Rename group");
            else if (result == 2)
                u.moveChild (at, at - 1, nullptr);
            else if (result == 3)
                u.moveChild (at, at + 1, nullptr);
            else if (result == 4 && group.getNumChildren() == 0)
                u.removeChild (group, nullptr);
            else if (result == 4)
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::NoIcon, "Delete group",
                                                    "Delete \"" + group[IDs::name].toString() + "\" and its "
                                                        + juce::String (group.getNumChildren()) + " tiles?",
                                                    "Delete", "Cancel", nullptr,
                                                    juce::ModalCallbackFunction::create ([group, u] (int ok) mutable
                                                    {
                                                        if (ok == 1)
                                                            u.removeChild (group, nullptr);
                                                    }));
        });
    }

    void unitMenu()
    {
        auto u = unit();
        juce::PopupMenu m;
        m.addItem (1, "Rename...");
        m.addSubMenu ("Colour", colourMenu (state::colourOf (u, customViolet), 100));
        m.addItem (2, "Duplicate device");
        m.addItem (3, "Delete device...");
        m.addSeparator();
        m.addItem (4, "Export device...");
        m.addItem (5, "Import device...");
        m.addItem (6, "New MIDI device...");
        m.addSeparator();
        m.addItem (7, fx ? "Move to Amps & Modellers" : "Move to Effects & Pedals");

        juce::Component::SafePointer<CustomPage> safe (this);
        const auto pedals = fx;
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&unitMenuButton), [safe, u, pedals] (int result) mutable
        {
            if (safe == nullptr || result == 0)
                return;
            auto root = safe->state;
            if (result >= 100)
                u.setProperty (IDs::colour, paletteColour (result - 100).toString(), nullptr);
            else if (result == 1)
                renameNode (u, "Rename device");
            else if (result == 2)
                state::addCustomUnit (root, u.createCopy(), pedals);
            else if (result == 3)
                juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::NoIcon, "Delete device",
                                                    "Delete \"" + u[IDs::name].toString() + "\" with all its tiles and notes? "
                                                    "Export it first if you might want it back.",
                                                    "Delete", "Cancel", nullptr,
                                                    juce::ModalCallbackFunction::create ([root, u] (int ok) mutable
                                                    {
                                                        if (ok != 1)
                                                            return;
                                                        state::removeCustomUnit (root, u);   // its tab falls back to the Quad Cortex / the Whammy
                                                    }));
            else if (result == 4)
                exportCustomUnit (u);
            else if (result == 5)
                importCustomUnit (root, pedals);
            else if (result == 6)
                newCustomUnit (root, pedals);
            else if (result == 7)
            {
                // The device changes tab: it's shown there, and this tab goes back to the Quad Cortex / the Whammy.
                state::showCustomUnitOn (root, root.getChildWithName (IDs::CustomUnits).indexOf (u), ! pedals);
            }
        });
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;

    Section unitSection { "cu.device", "MIDI device", "beta", customViolet };
    juce::TextButton unitMenuButton { "..." };
    juce::Label nameLabel, channelLabel, notesLabel, disclaimerLabel;
    juce::TextButton aboutButton;
    juce::ComboBox programBox;
    juce::TextEditor notesEditor;
    juce::TextButton exportButton { "Export device..." }, importButton { "Import device..." };

    juce::Viewport groupsView;
    juce::Component groupsContent;
    juce::TextButton addGroupButton { "+ Group" };
    juce::OwnedArray<Section> groupSections;
    juce::OwnedArray<juce::TextButton> groupButtons;   // per group: + Tile, ...
    juce::OwnedArray<Tile> tiles;
    juce::Array<int> tileGroup;                        // which group each tile belongs to
    juce::StringArray tileText, tileExact;             // what the search looks in: the name (fuzzy); messages, note, group (plain)
    SearchBox search { "Search tiles" };
    juce::OwnedArray<juce::TextButton> viewButtons;   // Tiles | Expression
    const bool fx;   // on the pedals tab (second tab) rather than the first tab
    CustomExpression expression { proc, fx };
};
} // namespace

std::unique_ptr<juce::Component> makeTileEditor (PedalCuesProcessor& p, juce::ValueTree tile, bool isNew)
{
    return std::make_unique<TileEditor> (p, tile, isNew);
}

std::unique_ptr<Page> makeCustomPage (PedalCuesProcessor& p, bool pedalsTab)
{
    return std::make_unique<CustomPage> (p, pedalsTab);
}

void newCustomUnit (juce::ValueTree state, bool pedalsTab)
{
    auto unit = state::addCustomUnit (state, state::createCustomUnit ("My device"), pedalsTab);
    if (! pedalsTab)
        state.setProperty (IDs::ampUnit, state::customAmpUnit, nullptr);
    askText ("Name your device", unit[IDs::name].toString(), [unit] (const juce::String& name) mutable
    {
        if (name.isNotEmpty())
            unit.setProperty (IDs::name, name, nullptr);
    });
}

void exportCustomUnit (juce::ValueTree unit)
{
    if (! unit.isValid())
        return;
    const auto fileName = juce::File::createLegalFileName (unit[IDs::name].toString()) + ".pedalcues-device";
    unitChooser = std::make_unique<juce::FileChooser> ("Export device",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (fileName),
                                                       "*.pedalcues-device");
    unitChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [unit] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();
                                  if (file != juce::File() && ! state::saveUnit (unit, file.withFileExtension ("pedalcues-device")))
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Export failed",
                                                                              "PedalCues couldn't write " + file.getFileName() + ".");
                              });
}

void importCustomUnit (juce::ValueTree state, bool pedalsTab)
{
    unitChooser = std::make_unique<juce::FileChooser> ("Import device", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                       "*.pedalcues-device");
    unitChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [state, pedalsTab] (const juce::FileChooser& fc) mutable
                              {
                                  const auto file = fc.getResult();
                                  if (! file.existsAsFile())
                                      return;
                                  const auto unit = state::loadUnit (file);
                                  if (! unit.isValid())
                                  {
                                      juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Not a PedalCues device",
                                                                              file.getFileName() + " isn't a PedalCues device file.");
                                      return;
                                  }
                                  state::addCustomUnit (state, unit, pedalsTab);
                                  if (! pedalsTab)
                                      state.setProperty (IDs::ampUnit, state::customAmpUnit, nullptr);
                              });
}
} // namespace ui
