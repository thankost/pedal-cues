#include "EditorCommon.h"
#include "Update.h"

using namespace theme;

namespace ui
{
namespace
{
// Numbered "how to hook it up" steps, drawn as badges + text.
class StepsList final : public juce::Component
{
public:
    juce::StringArray steps;
    bool viaInterface = false;   // true = separate outputs, false = daisy chain through the first device
    bool oneDevice = false;      // wiring guide: just the first device, no second one
    AmpInfo amp;
    bool showFlow = true;        // draw the signal-flow picture under the steps
    int rowHeight = 58;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        const auto rowH = rowHeight;

        for (int i = 0; i < steps.size(); ++i)
        {
            auto row = r.removeFromTop (rowH);
            const auto badge = row.removeFromLeft (26).removeFromTop (26).toFloat().translated (0.0f, 2.0f);
            g.setColour (accent.withAlpha (0.18f));
            g.fillEllipse (badge);
            g.setColour (accent);
            g.setFont (font (12.5f, true));
            g.drawText (juce::String (i + 1), badge, juce::Justification::centred);

            row.removeFromLeft (12);
            g.setColour (text.withAlpha (0.92f));
            g.setFont (font (13.0f));
            g.drawFittedText (steps[i], row.withTrimmedTop (3), juce::Justification::topLeft, 3, 1.0f);
        }

        r.removeFromTop (14);
        if (showFlow && r.getHeight() >= 110)
            paintFlow (g, r.removeFromTop (juce::jmin (r.getHeight(), viaInterface || oneDevice ? 150 : 130)), viaInterface, oneDevice, amp);
    }

private:
    struct Node { juce::String title, sub; juce::Colour colour; };

    // The signal flow, in general terms: the first device by name, a second one "for example a Whammy".
    static void paintFlow (juce::Graphics& g, juce::Rectangle<int> area, bool viaInterface, bool oneDevice, const AmpInfo& amp)
    {
        g.setColour (dim);
        g.setFont (font (11.0f, true));
        g.drawText ("SIGNAL FLOW", area.removeFromTop (20), juce::Justification::centredLeft);

        const auto grey = juce::Colour (0xff9aa0ac);
        const auto second = juce::Colour (0xffb0b6c2);
        const auto rowH = (area.getHeight() - 8) / 2;

        if (oneDevice)
        {
            // Either USB straight to the device, or an interface's MIDI Out and a MIDI cable.
            const Node usb[] = { { amp.shortName + " Cues track", "PedalCues, " + amp.box + " tab", accent },
                                 { amp.box, "USB MIDI", amp.colour } };
            const char* usbLinks[] = { "USB" };
            const Node din[] = { { amp.shortName + " Cues track", "PedalCues, " + amp.box + " tab", accent },
                                 { "MIDI interface", "MIDI Out", grey },
                                 { amp.box, "5-pin MIDI In", amp.colour } };
            const char* dinLinks[] = { "USB", "MIDI cable" };
            auto top = area.removeFromTop (rowH);
            area.removeFromTop (8);
            paintChain (g, top.removeFromLeft ((top.getWidth() * 2 - 56) / 3), usb, usbLinks, 2);
            g.setColour (dim);
            g.setFont (font (12.0f, true));
            g.drawText ("or", top, juce::Justification::centred);
            paintChain (g, area, din, dinLinks, 3);
            return;
        }

        if (! viaInterface)
        {
            // A MIDI Thru only passes on MIDI from the 5-pin MIDI In, not from USB, so the chain starts at an interface.
            const Node nodes[] = { { "Cue tracks", amp.shortName + " + second device", accent },
                                   { "Interface", "MIDI Out", grey },
                                   { amp.box, "MIDI In + Thru", amp.colour },
                                   { "Second device", "e.g. a Whammy", second } };
            const char* links[] = { "USB", "MIDI cable", "Thru" };
            paintChain (g, area.withSizeKeepingCentre (area.getWidth(), 66), nodes, links, 4);
            return;
        }

        const Node ampRow[] = { { amp.shortName + " Cues track", "PedalCues, " + amp.box + " tab", accent },
                                { amp.box, "USB, or MIDI Out 1", amp.colour } };
        const char* ampLinks[] = { "USB" };
        const Node other[] = { { "Second device track", "e.g. Whammy Cues", accent },
                               { "MIDI interface", "MIDI Out 2", grey },
                               { "Second device", "e.g. a Whammy (5-pin MIDI In)", second } };
        const char* otherLinks[] = { "USB", "MIDI cable" };
        auto top = area.removeFromTop (rowH);
        area.removeFromTop (8);
        paintChain (g, top.removeFromLeft ((top.getWidth() * 2 - 56) / 3), ampRow, ampLinks, 2);   // same box width as the 3-box row
        paintChain (g, area, other, otherLinks, 3);
    }

    static void paintChain (juce::Graphics& g, juce::Rectangle<int> row, const Node* nodes, const char* const* links, int count)
    {
        constexpr int arrowW = 56;
        const auto boxW = (row.getWidth() - (count - 1) * arrowW) / count;

        for (int i = 0; i < count; ++i)
        {
            const auto b = row.removeFromLeft (boxW).toFloat();
            g.setColour (surfaceHi);
            g.fillRoundedRectangle (b, 10.0f);
            g.setColour (nodes[i].colour);
            g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.5f);
            auto inner = b.reduced (12.0f, 6.0f);
            g.setColour (text);
            g.setFont (font (13.5f, true));
            g.drawText (nodes[i].title, inner.removeFromTop (inner.getHeight() / 2 + 2), juce::Justification::centredLeft, true);
            g.setColour (dim);
            g.setFont (font (11.5f));
            g.drawText (nodes[i].sub, inner, juce::Justification::centredLeft, true);

            if (i < count - 1)
            {
                const auto a = row.removeFromLeft (arrowW).toFloat();
                juce::Path arrow;
                arrow.addArrow ({ a.getX() + 8.0f, a.getCentreY(), a.getRight() - 8.0f, a.getCentreY() }, 2.0f, 10.0f, 9.0f);
                g.setColour (nodes[i].colour);
                g.fillPath (arrow);
                g.setFont (font (10.0f, true));
                g.drawText (links[i], a.withTrimmedBottom (a.getHeight() / 2 + 6), juce::Justification::centredBottom);
            }
        }
    }
};

// Help > Wiring guide: one device, a daisy chain to a second device (for example a Whammy) on its MIDI Thru,
// or separate outputs. The first device is the unit on the first tab; the views switch at the top.
class WiringGuide final : public juce::Component
{
public:
    explicit WiringGuide (const AmpInfo& a) : amp (a)
    {
        const auto d = amp.box;
        oneDevice.oneDevice = true;
        oneDevice.steps = { "USB from the " + d + " to the computer" + (amp.isCustom() ? juce::String (", if it has USB MIDI.") : juce::String (".")),
                            "Or a MIDI cable from your interface's MIDI Out to the " + d + "'s MIDI In.",
                            "Set the " + d + "'s MIDI channel in MIDI Setup, the same as on the device (not Omni)." };

        const auto thruNote = amp.kind == AmpInfo::Kind::quadCortex ? juce::String (" Turn MIDI Thru on in the QC.")
                            : amp.isKemper() ? juce::String (" If your Kemper shares one jack for MIDI Out and Thru, set it to Thru.")
                                             : juce::String (" See its manual for the Thru setting.");
        daisy.steps = { "A MIDI cable from your interface's MIDI Out to the " + d + "'s MIDI In.",
                        "A MIDI cable from the " + d + "'s MIDI Thru to the second device's MIDI In (for example a Whammy)." + thruNote,
                        "Give the two devices different MIDI channels, so each only reacts to its own cues." };

        separate.viaInterface = true;
        separate.steps = { d + ": USB to the computer" + (amp.isCustom() ? juce::String (" (if it has USB MIDI)") : juce::String())
                             + ", or a MIDI cable from MIDI Out 1 to its MIDI In.",
                           "Second device (for example a Whammy): a MIDI cable from MIDI Out 2 to its MIDI In. "
                           "The Whammy has no USB MIDI, so it always needs a MIDI cable.",
                           "Each DAW track sends to the output its device is on." };

        for (auto* list : { &oneDevice, &daisy, &separate })
        {
            list->amp = amp;
            list->rowHeight = 40;
            addChildComponent (list);
        }

        const char* names[] = { "One device", "Daisy chain", "Separate outputs" };
        for (int i = 0; i < 3; ++i)
        {
            auto* b = viewButtons.add (new juce::TextButton (names[i]));
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4402);
            b->setColour (juce::TextButton::buttonColourId, surface);
            b->setColour (juce::TextButton::buttonOnColourId, accent);
            b->setColour (juce::TextButton::textColourOffId, dim);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
            b->setConnectedEdges ((i > 0 ? juce::Button::ConnectedOnLeft : 0) | (i < 2 ? juce::Button::ConnectedOnRight : 0));
            b->onClick = [this, i] { show (i); };
            addAndMakeVisible (b);
        }

        guideButton.onClick = [] { juce::URL (guideUrl + "#2-connect-your-rig").launchInDefaultBrowser(); };
        addAndMakeVisible (guideButton);
        setSize (860, 600);
        show (state::getFlag ("setupViaQcChain") ? 1 : 2);   // the wiring picked in MIDI Setup
    }

    void show (int view)
    {
        current = juce::jlimit (0, 2, view);
        viewButtons[current]->setToggleState (true, juce::dontSendNotification);
        oneDevice.setVisible (current == 0);
        daisy.setVisible (current == 1);
        separate.setVisible (current == 2);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (background);
        const juce::String titles[] = { "ONE DEVICE: " + amp.box.toUpperCase(),
                                        "DAISY CHAIN VIA " + amp.box.toUpperCase(),
                                        "SEPARATE OUTPUTS" };
        const juce::String subs[] = { "Just your " + amp.box + ": one cue track, sent over USB or a MIDI cable.",
                                      "A second device on the " + amp.box + "'s MIDI Thru. Both tracks send to the same interface MIDI Out.",
                                      "Each device on its own output. Each DAW track sends to the output its device is on." };
        auto r = title;
        g.setColour (text);
        g.setFont (font (14.0f, true));
        g.drawText (titles[current], r.removeFromTop (20), juce::Justification::centredLeft);
        g.setColour (dim);
        g.setFont (font (12.0f));
        g.drawText (subs[current], r, juce::Justification::topLeft);

        if (current == 0)
            return;
        const auto specific = amp.kind == AmpInfo::Kind::quadCortex ? juce::String ("The Quad Cortex never forwards USB MIDI to its Thru.")
                            : amp.isKemper() ? juce::String ("Kemper confirms USB MIDI has no MIDI Thru.")
                                             : juce::String ("That's true for most devices; check its manual.");
        g.setColour (whammyRed);
        g.setFont (font (12.5f, true));
        g.drawFittedText ("Doesn't work: the " + amp.box + " on USB only, with a second device on its MIDI Thru. A MIDI Thru only "
                          "passes on MIDI from the 5-pin MIDI In. " + specific,
                          warning, juce::Justification::centredLeft, 2);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (24, 20);
        auto bar = r.removeFromTop (32).withWidth (480);
        for (auto* b : viewButtons)
            b->setBounds (bar.removeFromLeft (160));
        r.removeFromTop (18);

        guideButton.setBounds (r.removeFromBottom (34).removeFromRight (220));
        r.removeFromBottom (10);
        warning = r.removeFromBottom (40);
        r.removeFromBottom (10);
        title = r.removeFromTop (44);
        for (auto* list : { &oneDevice, &daisy, &separate })
            list->setBounds (r);
    }

private:
    const AmpInfo amp;
    StepsList oneDevice, daisy, separate;
    juce::OwnedArray<juce::TextButton> viewButtons;
    int current = 2;
    juce::Rectangle<int> title, warning;
    juce::TextButton guideButton { "Open the user guide" };
};

class SettingsPage final : public Page
{
public:
    explicit SettingsPage (PedalCuesProcessor& p)
        : proc (p), state (p.state)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &pedalsSection, &tracksSection })
            addAndMakeVisible (c);
        addChildComponent (testSection);

        // Your pedals: the channels every cue is sent on.
        styleCaption (qcChannelLabel, "Quad Cortex channel");
        styleCaption (whChannelLabel, "Whammy channel");
        styleCaption (ampLabel, "Amp modeller or MIDI device");
        ampBox.setTooltip ("Quad Cortex, Kemper Profiler, Kemper Player or a custom MIDI device (beta): the first tab and the channel below "
                           "follow it (the same as the menu on the first tab)");
        ampBox.onChange = [this]
        {
            const auto id = ampBox.getSelectedId();
            if (id == newCustomId)
                newCustomUnit (state);
            else if (id >= firstCustomId)
            {
                state.setProperty (IDs::selectedCustomUnit, id - firstCustomId, nullptr);
                state.setProperty (IDs::ampUnit, state::customAmpUnit, nullptr);
            }
            else if (id > 0)
                state.setProperty (IDs::ampUnit, id - 1, nullptr);
        };
        styleCaption (pcBaseLabel, "Whammy program numbering");
        styleHint (qcHint, "Must match the QC: Settings > MIDI Settings > MIDI Channel (not Omni).");
        styleHint (whHint, "Must match the Whammy's MIDI channel (see its manual). Use a different channel from the QC.");
        confirmButton.setClickingTogglesState (true);
        confirmButton.setColour (juce::TextButton::buttonColourId, accent);
        confirmButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        confirmButton.setColour (juce::TextButton::buttonOnColourId, raised);
        confirmButton.setColour (juce::TextButton::textColourOnId, ledGreen);
        confirmButton.setTooltip ("Confirm once that your pedals are set to these channels. Clips keep the channel they were dragged with, "
                                  "so set this before building songs.");
        confirmButton.onClick = [this] { state::setFlag (ui::channelsConfirmedFlag, confirmButton.getToggleState()); updateConfirm(); };
        styleHint (confirmHint, "Every clip keeps the channel it was dragged with: set the channels before building songs.");

        for (int ch = 1; ch <= 16; ++ch)
        {
            qcChannelBox.addItem ("Channel " + juce::String (ch), ch);
            whChannelBox.addItem ("Channel " + juce::String (ch), ch);
        }
        pcBaseBox.addItem ("As printed in the manual (1 = first)", 1);
        pcBaseBox.addItem ("Zero-based (0 = first)", 2);

        qcChannelBox.onChange = [this] { state.setProperty (IDs::qcChannel, qcChannelBox.getSelectedId(), nullptr); };
        whChannelBox.onChange = [this] { state.setProperty (IDs::whChannel, whChannelBox.getSelectedId(), nullptr); };
        pcBaseBox.onChange    = [this] { state.setProperty (IDs::whPcBase, pcBaseBox.getSelectedId() == 1 ? 1 : 0, nullptr); };
        setlistToggle.onClick = [this] { state.setProperty (IDs::sendSetlist, setlistToggle.getToggleState(), nullptr); };
        pcBaseBox.setTooltip ("Only change this if every Whammy mode lands one position off.");
        setlistToggle.setTooltip ("Send the setlist (CC#32) before each preset change, so the QC switches to the preset's setlist even when "
                                  "it's on another one. Needed when your presets are in more than one setlist. Clips keep what they were "
                                  "dragged with: drag preset clips in again after changing this.");

        advancedButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        advancedButton.setColour (juce::TextButton::textColourOffId, dim);
        advancedButton.onClick = [this] { showAdvanced = ! showAdvanced; updateAdvanced(); resized(); };

        // My wiring (in the DAW tracks header): the steps below depend on it. The cable details are in Help > Wiring guide.
        for (auto* b : { &viaQcButton, &viaInterfaceButton })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4201);
            b->setColour (juce::TextButton::buttonOnColourId, qcBlue);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        }
        viaQcButton.setTooltip ("Interface MIDI Out > QC / Kemper 5-pin MIDI In > its MIDI Thru > Whammy. "
                                "MIDI Thru does not forward USB MIDI, so this needs a 5-pin MIDI Out.");
        viaInterfaceButton.setTooltip ("Each pedal on its own output: the QC / Kemper over USB (or MIDI Out 1), the Whammy on MIDI Out 2");
        viaQcButton.onClick = [this] { if (viaQcButton.getToggleState()) setSetupMode (false); };
        viaInterfaceButton.onClick = [this] { if (viaInterfaceButton.getToggleState()) setSetupMode (true); };

        // DAW tracks: the two cue tracks and their outputs. Standalone only: Test (send to a port, test each pedal).
        tracksSteps.showFlow = false;
        wiringLink.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        wiringLink.setColour (juce::TextButton::textColourOffId, qcBlue);
        wiringLink.onClick = [this] { showWiringGuide (state); };
        styleCaption (wiringLabel, "My wiring:");
        styleHint (dawHint, "Where the track's MIDI output is: Reaper: I/O > MIDI Hardware Output, and enable the port in "
                            "Preferences > MIDI Devices ('Send to original channels'). Ableton Live: MIDI To. Cubase: the track's MIDI output. "
                            "Logic: use an External MIDI track (Logic can't send the play-button tests to hardware; use the standalone app).");

        testOutBox.onChange = [this]
        {
            const auto i = testOutBox.getSelectedItemIndex();
            proc.setDirectMidiOutput (juce::isPositiveAndBelow (i - 1, testDevices.size()) ? testDevices[i - 1].identifier : juce::String());
            updateTestState();
        };
        testQcButton.setTooltip ("Opens the QC tuner for 1.5 seconds: a quick check that the QC gets MIDI on its channel.");
        testWhButton.setTooltip ("Steps the Whammy through Oct Up, 5th Up and 2 Oct Up: a quick check that it gets MIDI on its channel.");
        testQcButton.onClick = [this]
        {
            cues::Cue c;
            const auto ch = (int) state[IDs::qcChannel];
            if (ampInfo (state).isCustom())
            {
                // A custom unit has no known tuner: send its first tile that has valid messages.
                const auto unit = state::customUnit (state);
                for (auto g : unit)
                    for (auto t : g)
                        if (c.events.empty() && cues::custom::parse (t[IDs::messages].toString(), (int) unit[IDs::programBase]).ok())
                        {
                            c = cues::custom::cue (ch, t[IDs::name].toString(), t[IDs::messages].toString(), (int) unit[IDs::programBase]);
                            setTestStatus ("Sent your first tile, \"" + t[IDs::name].toString() + "\", to " + currentPortName() + " on channel "
                                           + juce::String (ch) + ". Did the " + ampShort() + " react? If not, check the cable direction and its channel.");
                        }
                if (c.events.empty())
                    setTestStatus ("Add a tile with MIDI messages on the " + ampShort() + " page first: Test sends your first tile.");
                else
                    proc.preview (c);
                return;
            }
            const auto kemper = isKemper();
            c.add (0.0, (kemper ? cues::kemper::tuner (ch, true) : cues::qc::tuner (ch, true)).events.front().second);
            c.add (proc.getHostBpm() / 40.0, (kemper ? cues::kemper::tuner (ch, false) : cues::qc::tuner (ch, false)).events.front().second);   // ~1.5 s later
            proc.preview (c);
            setTestStatus ("Sent tuner on/off to " + currentPortName() + " on channel " + juce::String (ch)
                           + ". Did the " + ampShort() + "'s tuner open and close (or close, if it was open)? If not, check the cable direction, the "
                           + ampShort() + "'s channel, and MIDI Thru for the daisy chain.");
        };
        testWhButton.onClick = [this]
        {
            // Three modes half a second apart, so the LED visibly moves whatever mode the Whammy was on.
            cues::Cue c;
            const auto halfSecond = proc.getHostBpm() / 120.0;   // in beats
            int step = 0;
            for (const auto mode : { 1, 2, 0 })   // Oct Up, 5th Up, 2 Oct Up
            {
                const auto cue = cues::whammy::effect ((int) state[IDs::whChannel], mode, {}, (bool) state[IDs::whChords],
                                                       (bool) state[IDs::whBypass], (int) state[IDs::whPcBase], false);
                for (const auto& [beat, message] : cue.events)
                    if (message.isProgramChange())
                        c.add ((double) step * halfSecond, message);
                ++step;
            }
            proc.preview (c);
            if (currentPortName().containsIgnoreCase ("Quad Cortex") || currentPortName().containsIgnoreCase ("QC")
                || currentPortName().containsIgnoreCase ("Kemper") || currentPortName().containsIgnoreCase ("Profiler"))
                setTestStatus ("Sent Oct Up, 5th Up, 2 Oct Up to " + currentPortName() + ", but that's the " + ampShort() + "'s USB port: its MIDI Thru "
                               "doesn't pass USB MIDI on, so a Whammy on its Thru won't react. Send to your interface's MIDI Out instead.");
            else
                setTestStatus ("Sent Oct Up, 5th Up, 2 Oct Up to " + currentPortName() + " on channel " + juce::String ((int) state[IDs::whChannel])
                               + ". Did the Whammy's mode LED step along? If not, check the cable direction, the Whammy's channel, and MIDI Thru for the daisy chain.");
        };
        for (auto* b : { &testQcButton, &testWhButton })
            b->setColour (juce::TextButton::buttonColourId, raised);

        for (auto* c : std::initializer_list<juce::Component*> { &ampLabel, &ampBox, &qcChannelLabel, &whChannelLabel, &qcChannelBox, &whChannelBox,
                                                                 &qcHint, &whHint, &confirmButton, &confirmHint, &advancedButton, &tracksSteps, &viaQcButton,
                                                                 &viaInterfaceButton, &wiringLabel, &wiringLink, &dawHint })
            addAndMakeVisible (c);
        for (auto* c : std::initializer_list<juce::Component*> { &pcBaseLabel, &pcBaseBox, &setlistToggle,
                                                                 &testOutBox, &testQcButton, &testWhButton, &testHint })
            addChildComponent (c);

        const auto standalone = PedalCuesProcessor::isStandalone();
        for (auto* c : std::initializer_list<juce::Component*> { &testSection, &testOutBox, &testQcButton, &testWhButton, &testHint })
            c->setVisible (standalone);

        setSetupMode (! state::getFlag ("setupViaQcChain"));
        updateAdvanced();
        refresh();
    }

    void refresh() override
    {
        qcChannelBox.setSelectedId ((int) state[IDs::qcChannel], juce::dontSendNotification);
        whChannelBox.setSelectedId ((int) state[IDs::whChannel], juce::dontSendNotification);
        pcBaseBox.setSelectedId ((int) state[IDs::whPcBase] == 1 ? 1 : 2, juce::dontSendNotification);
        setlistToggle.setToggleState ((bool) state[IDs::sendSetlist], juce::dontSendNotification);

        // The first channel belongs to the amp unit (picked here or on the first tab).
        ampBox.clear (juce::dontSendNotification);
        for (int u = 0; u < 3; ++u)
            ampBox.addItem (ampUnitName (u), u + 1);
        ampBox.addSectionHeading ("Custom MIDI devices (beta)");
        const auto units = state.getChildWithName (IDs::CustomUnits);
        for (int i = 0; i < units.getNumChildren(); ++i)
            ampBox.addItem (units.getChild (i)[IDs::name].toString(), firstCustomId + i);
        ampBox.addItem ("New MIDI device...", newCustomId);
        const auto amp = ampInfo (state);
        ampBox.setSelectedId (amp.isCustom() ? firstCustomId + (int) state[IDs::selectedCustomUnit] : (int) state[IDs::ampUnit] + 1,
                              juce::dontSendNotification);
        qcChannelLabel.setText (amp.name + " channel", juce::dontSendNotification);
        qcHint.setText (amp.isCustom() ? "Must match the MIDI channel set on the " + amp.name + " (see its manual; not Omni)."
                        : isKemper() ? "Must match the Kemper: System Settings > MIDI > MIDI Global Channel (not OMNI)."
                                     : "Must match the QC: Settings > MIDI Settings > MIDI Channel (not Omni).", juce::dontSendNotification);
        whHint.setText ("Must match the Whammy's MIDI channel (see its manual). Use a different channel from the " + ampShort() + ".",
                        juce::dontSendNotification);
        testQcButton.setButtonText ("Test " + ampShort());
        testQcButton.setTooltip (amp.isCustom() ? "Sends your first tile on the " + ampShort() + " page: a quick check that it gets MIDI on its channel."
                                                : "Opens the " + ampShort() + "'s tuner for 1.5 seconds: a quick check that it gets MIDI on its channel.");
        viaQcButton.setButtonText ("Daisy chain via " + ampShort());
        setSetupMode (! state::getFlag ("setupViaQcChain"));
        updateConfirm();
        refreshTestDevices();
    }

    void updateConfirm()
    {
        confirmButton.setToggleState (state::getFlag (ui::channelsConfirmedFlag), juce::dontSendNotification);
        confirmButton.setButtonText (juce::String (juce::CharPointer_UTF8 (confirmButton.getToggleState()
                                                                               ? "\xe2\x9c\x93  My pedals use these channels"
                                                                               : "My pedals use these channels")));
    }

    bool isKemper() const          { return ampInfo (state).isKemper(); }
    juce::String ampShort() const  { return ampInfo (state).shortName; }
    static constexpr int firstCustomId = 100, newCustomId = 99;

    void visibilityChanged() override
    {
        if (isVisible())
        {
            updateConfirm();   // another PedalCues window may have confirmed the channels
            refreshTestDevices();
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        auto left = r.removeFromLeft (juce::jmax (380, r.getWidth() * 2 / 5));
        r.removeFromLeft (12);

        // Left: Your pedals, then (standalone) Test. The quick tour and the user guide are in the ☰ / Help menu.

        const auto isQc = ampInfo (state).kind == AmpInfo::Kind::quadCortex;
        setlistToggle.setVisible (isQc);   // a Quad Cortex setting: next to its channel
        const auto pedalsH = Section::headerHeight + 62 + 2 * 84 + 34 + 70 + (isQc ? 32 : 0) + (showAdvanced ? 62 : 0) + 8;
        pedalsSection.setBounds (left.removeFromTop (pedalsH));
        {
            auto m = pedalsSection.contentArea().reduced (6, 2);
            auto field = [&m] (juce::Label& l, juce::Component& c, juce::Label* hint)
            {
                auto row = m.removeFromTop (hint != nullptr ? 84 : 62);
                l.setBounds (row.removeFromTop (22));
                c.setBounds (row.removeFromTop (32));
                if (hint != nullptr)
                    hint->setBounds (row.withTrimmedTop (2));
            };
            field (ampLabel, ampBox, nullptr);
            field (qcChannelLabel, qcChannelBox, &qcHint);
            if (isQc)
                setlistToggle.setBounds (m.removeFromTop (32).translated (0, -6));
            field (whChannelLabel, whChannelBox, &whHint);
            confirmButton.setBounds (m.removeFromTop (34).withWidth (260));
            confirmHint.setBounds (m.removeFromTop (34).withTrimmedTop (2));
            m.removeFromTop (2);
            advancedButton.setBounds (m.removeFromTop (30).withWidth (220));
            m.removeFromTop (4);
            if (showAdvanced)
                field (pcBaseLabel, pcBaseBox, nullptr);
        }

        if (PedalCuesProcessor::isStandalone())
        {
            left.removeFromTop (12);
            testSection.setBounds (left.removeFromTop (juce::jmin (left.getHeight(), Section::headerHeight + 140)));
            auto t = testSection.contentArea().reduced (6, 4);
            testOutBox.setBounds (t.removeFromTop (32));
            t.removeFromTop (8);
            auto row = t.removeFromTop (32);
            testQcButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 4));
            row.removeFromLeft (8);
            testWhButton.setBounds (row);
            t.removeFromTop (8);
            testHint.setBounds (t);
        }

        // Right: DAW tracks, with the wiring choice in its header and a link to the wiring guide.
        tracksSection.setBounds (r);
        {
            // The whole title row (headerArea() is only its right half, too narrow for label + two buttons).
            auto hdr = tracksSection.getBounds().removeFromTop (Section::headerHeight).reduced (Section::padding, 8);
            viaInterfaceButton.setBounds (hdr.removeFromRight (140));
            hdr.removeFromRight (6);
            viaQcButton.setBounds (hdr.removeFromRight (150));
            hdr.removeFromRight (6);
            wiringLabel.setBounds (hdr.removeFromRight (78));

            auto t = tracksSection.contentArea().reduced (8, 6);
            wiringLink.setBounds (t.removeFromBottom (30).removeFromRight (260));
            tracksSteps.setBounds (t.removeFromTop (juce::jmin (t.getHeight() - 70, tracksSteps.steps.size() * tracksSteps.rowHeight)));
            t.removeFromTop (6);
            dawHint.setBounds (t.removeFromTop (64));
        }
    }

private:
    static void styleHint (juce::Label& l, const juce::String& t)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (font (11.5f));
        l.setColour (juce::Label::textColourId, dim);
        l.setJustificationType (juce::Justification::topLeft);
    }

    void updateAdvanced()
    {
        advancedButton.setButtonText (juce::String (showAdvanced ? "v" : ">") + "  Advanced: Whammy numbering");
        for (auto* c : std::initializer_list<juce::Component*> { &pcBaseLabel, &pcBaseBox })
            c->setVisible (showAdvanced);
    }

    void refreshTestDevices()
    {
        if (! PedalCuesProcessor::isStandalone())
            return;

        testDevices = PedalCuesProcessor::standaloneLayoutForScreenshots
                          ? juce::Array<juce::MidiDeviceInfo> { juce::MidiDeviceInfo ("USB MIDI Interface", "demo") }
                          : juce::MidiOutput::getAvailableDevices();
        testOutBox.clear (juce::dontSendNotification);
        testOutBox.addItem ("None", 1);
        int selected = 0;
        for (int i = 0; i < testDevices.size(); ++i)
        {
            testOutBox.addItem (testDevices[i].name, i + 2);
            if (testDevices[i].identifier == proc.getDirectMidiOutput() || PedalCuesProcessor::standaloneLayoutForScreenshots)
                selected = i + 1;
        }
        if (testDevices.isEmpty())
        {
            testOutBox.addItem ("No MIDI devices found", 1000);
            testOutBox.setItemEnabled (1000, false);
        }
        testOutBox.setSelectedItemIndex (selected, juce::dontSendNotification);
        updateTestState();
    }

    juce::String currentPortName() const
    {
        if (PedalCuesProcessor::standaloneLayoutForScreenshots && ! testDevices.isEmpty())
            return testDevices[0].name;
        for (const auto& d : testDevices)
            if (d.identifier == proc.getDirectMidiOutput())
                return d.name;
        return "the MIDI port";
    }

    // The test buttons only make sense once a port is picked.
    void updateTestState()
    {
        const auto hasPort = proc.getDirectMidiOutput().isNotEmpty() || PedalCuesProcessor::standaloneLayoutForScreenshots;
        testQcButton.setEnabled (hasPort);
        testWhButton.setEnabled (hasPort);
        if (! hasPort)
            setTestStatus (testDevices.isEmpty() ? "No MIDI devices found. Connect your audio interface (for its MIDI Out), or the " + ampShort() + " over USB "
                                                   "to test only the " + ampShort() + " (its MIDI Thru doesn't pass USB MIDI on to the Whammy). Then come back to this tab."
                                                 : "Pick the MIDI port your pedals are on to test them.");
        else if (testHint.getText().isEmpty() || testHint.getText().startsWith ("Pick") || testHint.getText().startsWith ("No MIDI"))
            setTestStatus ("The app sends straight to this port, no DAW or audio needed. Test each pedal, then use any tile's play button.");
    }

    void setTestStatus (const juce::String& t)
    {
        styleHint (testHint, t);
    }

    void setSetupMode (bool viaInterface)
    {
        state::setFlag ("setupViaQcChain", ! viaInterface);
        (viaInterface ? viaInterfaceButton : viaQcButton).setToggleState (true, juce::dontSendNotification);

        if (viaInterface)
        {
            tracksSteps.steps = {
                "In your DAW, enable the MIDI outputs your pedals are on.",
                "Track '" + ampShort() + " Cues': insert PedalCues and set its MIDI output to the " + ampInfo (state).box + " (USB) or MIDI Out 1.",
                "Track 'Whammy Cues': insert PedalCues and set its MIDI output to MIDI Out 2.",
                "Keep the original MIDI channels. Drag " + ampShort() + " tiles onto " + ampShort() + " Cues and Whammy tiles onto Whammy Cues."
            };
        }
        else
        {
            tracksSteps.steps = {
                "In your DAW, enable your interface's MIDI output.",
                "Tracks '" + ampShort() + " Cues' and 'Whammy Cues': insert PedalCues on each.",
                "Set both tracks' MIDI output to the interface MIDI Out, keeping the original MIDI channels.",
                "Drag " + ampShort() + " tiles onto " + ampShort() + " Cues and Whammy tiles onto Whammy Cues."
            };
        }
        tracksSteps.repaint();
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;
    bool showAdvanced = false;

    Section pedalsSection { "set.pedals", "Your pedals", "set once, must match the pedals" };
    Section testSection   { "set.test", "Test your pedals", "sends on the channels above", ledGreen };
    Section tracksSection { "set.tracks", "DAW tracks", "two cue tracks, once", qcBlue };

    juce::Label ampLabel, qcChannelLabel, whChannelLabel, pcBaseLabel, qcHint, whHint, testHint;
    juce::ComboBox ampBox, qcChannelBox, whChannelBox, pcBaseBox, testOutBox;
    juce::ToggleButton setlistToggle { "Send setlist (CC#32) with preset changes" };   // shown for the Quad Cortex
    juce::TextButton advancedButton, confirmButton;
    juce::Label confirmHint;

    StepsList tracksSteps;
    juce::Label wiringLabel, dawHint;
    juce::TextButton wiringLink { "How should I wire my pedals? >" };
    juce::TextButton viaQcButton { "Daisy chain via QC" };
    juce::TextButton viaInterfaceButton { "Separate outputs" };

    juce::Array<juce::MidiDeviceInfo> testDevices;
    juce::TextButton testQcButton { "Test QC" }, testWhButton { "Test Whammy" };

};
} // namespace

std::unique_ptr<juce::Component> makeWiringGuide (const AmpInfo& amp, int view)
{
    auto guide = std::make_unique<WiringGuide> (amp);
    if (view >= 0)
        guide->show (view);
    return guide;
}

void showWiringGuide (const juce::ValueTree& state)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (makeWiringGuide (ampInfo (state)).release());
    o.dialogTitle = "Wiring guide";
    o.dialogBackgroundColour = background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

std::unique_ptr<Page> makeSettingsPage (PedalCuesProcessor& p)
{
    return std::make_unique<SettingsPage> (p);
}
} // namespace ui
