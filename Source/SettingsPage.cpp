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
    bool viaInterface = false;   // true = separate outputs, false = daisy chain through the QC / Kemper
    int unit = 0;                // 0 = Quad Cortex, 1/2 = Kemper (Profiler / Player)
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
            paintFlow (g, r.removeFromTop (juce::jmin (r.getHeight(), viaInterface ? 150 : 130)), viaInterface, unit);
    }

private:
    struct Node { juce::String title, sub; juce::Colour colour; };

    static void paintFlow (juce::Graphics& g, juce::Rectangle<int> area, bool viaInterface, int unit)
    {
        g.setColour (dim);
        g.setFont (font (11.0f, true));
        g.drawText ("SIGNAL FLOW", area.removeFromTop (20), juce::Justification::centredLeft);

        if (! viaInterface)
        {
            // A MIDI Thru only passes on MIDI from the 5-pin MIDI In, not from USB, so the chain starts at an interface.
            const auto kemper = unit != 0;
            const Node nodes[] = { { "Cue tracks", kemper ? "Kemper + Whammy" : "QC + Whammy", accent },
                                   { "Interface", "MIDI Out", juce::Colour (0xff9aa0ac) },
                                   { kemper ? "Kemper" : "Quad Cortex", "MIDI In + Thru", kemper ? kemperGreen : qcBlue },
                                   { "Whammy", "5-pin MIDI In", whammyRed } };
            const char* links[] = { "USB", "MIDI cable", "Thru" };
            paintChain (g, area.withSizeKeepingCentre (area.getWidth(), 66), nodes, links, 4);
            return;
        }

        const auto rowH = (area.getHeight() - 8) / 2;
        const auto kemper = unit != 0;
        const Node amp[] = { { kemper ? "Kemper Cues track" : "QC Cues track", kemper ? "PedalCues, Kemper tab" : "PedalCues, Quad Cortex tab", accent },
                             { kemper ? "Kemper" : "Quad Cortex", "USB, or MIDI Out 1", kemper ? kemperGreen : qcBlue } };
        const char* ampLinks[] = { "USB" };
        const Node wh[] = { { "Whammy Cues track", "PedalCues, Whammy tab", accent },
                            { "MIDI interface", "MIDI Out 2", juce::Colour (0xff9aa0ac) },
                            { "Whammy", "5-pin MIDI In (no USB MIDI)", whammyRed } };
        const char* whLinks[] = { "USB", "MIDI cable" };
        auto top = area.removeFromTop (rowH);
        area.removeFromTop (8);
        paintChain (g, top.removeFromLeft ((top.getWidth() * 2 - 56) / 3), amp, ampLinks, 2);   // same box width as the 3-box row
        paintChain (g, area, wh, whLinks, 3);
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

// Help > Wiring guide: which cables go where, for both setups, plus the one that doesn't work. Follows the amp unit.
class WiringGuide final : public juce::Component
{
public:
    explicit WiringGuide (int ampUnit) : kemper (ampUnit != 0)
    {
        daisy.viaInterface = false;
        daisy.unit = ampUnit;
        daisy.rowHeight = 44;
        separate.viaInterface = true;
        separate.unit = ampUnit;
        separate.rowHeight = 44;
        if (kemper)
        {
            daisy.steps = { "A MIDI cable from your interface's MIDI Out to the Kemper's MIDI In.",
                            "A MIDI cable from the Kemper's MIDI Thru to the Whammy's MIDI In (if your Kemper shares one jack for "
                            "MIDI Out and Thru, set it to Thru; see the Kemper manual)." };
            separate.steps = { "Whammy: a MIDI cable from your interface's MIDI Out 2 to the Whammy's MIDI In (the Whammy has no USB MIDI).",
                               "Kemper: USB to the computer, or a MIDI cable from MIDI Out 1 to the Kemper's MIDI In." };
        }
        else
        {
            daisy.steps = { "A MIDI cable from your interface's MIDI Out to the QC's MIDI In.",
                            "A MIDI cable from the QC's MIDI Out/Thru to the Whammy's MIDI In. Turn MIDI Thru on in the QC." };
            separate.steps = { "Whammy: a MIDI cable from your interface's MIDI Out 2 to the Whammy's MIDI In (the Whammy has no USB MIDI).",
                               "Quad Cortex: USB to the computer, or a MIDI cable from MIDI Out 1 to the QC's MIDI In." };
        }
        guideButton.onClick = [] { juce::URL (guideUrl + "#2-connect-your-rig").launchInDefaultBrowser(); };
        for (auto* c : std::initializer_list<juce::Component*> { &daisy, &separate, &guideButton })
            addAndMakeVisible (c);
        setSize (860, 720);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (background);
        auto heading = [&g] (juce::Rectangle<int> r, const juce::String& title, const juce::String& sub)
        {
            g.setColour (text);
            g.setFont (font (14.0f, true));
            g.drawText (title, r.removeFromTop (20), juce::Justification::centredLeft);
            g.setColour (dim);
            g.setFont (font (12.0f));
            g.drawText (sub, r, juce::Justification::topLeft);
        };
        heading (daisyTitle, kemper ? "DAISY CHAIN VIA KEMPER" : "DAISY CHAIN VIA QC",
                 "Both DAW tracks send to the same interface MIDI Out. The pedals share one cable.");
        heading (separateTitle, "SEPARATE OUTPUTS", "Each DAW track sends to the output its pedal is on.");

        g.setColour (whammyRed);
        g.setFont (font (12.5f, true));
        g.drawFittedText (kemper ? "Doesn't work: the Kemper on USB only, with the Whammy on the Kemper's MIDI Thru. "
                                   "USB MIDI has no MIDI Thru, so the Thru only passes on MIDI from the 5-pin MIDI In."
                                 : "Doesn't work: the QC on USB only, with the Whammy on the QC's MIDI Thru. "
                                   "The QC's MIDI Thru never forwards MIDI it receives over USB.",
                          warning, juce::Justification::centredLeft, 2);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (24, 20);
        guideButton.setBounds (r.removeFromBottom (34).removeFromRight (220));
        r.removeFromBottom (10);
        warning = r.removeFromBottom (40);
        r.removeFromBottom (10);

        const auto half = r.getHeight() / 2;
        auto top = r.removeFromTop (half);
        daisyTitle = top.removeFromTop (44);
        daisy.setBounds (top);
        separateTitle = r.removeFromTop (44);
        separate.setBounds (r);
    }

private:
    const bool kemper;
    StepsList daisy, separate;
    juce::Rectangle<int> daisyTitle, separateTitle, warning;
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
        styleCaption (ampLabel, "Amp modeller");
        for (int u = 0; u < 3; ++u)
            ampBox.addItem (ampUnitName (u), u + 1);
        ampBox.setTooltip ("Quad Cortex, Kemper Profiler or Kemper Player: the first tab and the channel below follow it "
                           "(the same as the menu on the first tab)");
        ampBox.onChange = [this] { state.setProperty (IDs::ampUnit, ampBox.getSelectedId() - 1, nullptr); };
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
        setlistToggle.setTooltip ("Also send CC#32 (setlist) before each preset change. Leave off if all presets are in the active setlist.");

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
        wiringLink.onClick = [this] { showWiringGuide ((int) state[IDs::ampUnit]); };
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
        ampBox.setSelectedId ((int) state[IDs::ampUnit] + 1, juce::dontSendNotification);
        qcChannelLabel.setText (ui::ampUnitName ((int) state[IDs::ampUnit]) + " channel", juce::dontSendNotification);
        qcHint.setText (isKemper() ? "Must match the Kemper: System Settings > MIDI > MIDI Global Channel (not OMNI)."
                                   : "Must match the QC: Settings > MIDI Settings > MIDI Channel (not Omni).", juce::dontSendNotification);
        whHint.setText ("Must match the Whammy's MIDI channel (see its manual). Use a different channel from the " + ampShort() + ".",
                        juce::dontSendNotification);
        testQcButton.setButtonText ("Test " + ampShort());
        testQcButton.setTooltip ("Opens the " + ampShort() + "'s tuner for 1.5 seconds: a quick check that it gets MIDI on its channel.");
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

    bool isKemper() const          { return (int) state[IDs::ampUnit] != 0; }
    juce::String ampShort() const  { return isKemper() ? "Kemper" : "QC"; }

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

        const auto pedalsH = Section::headerHeight + 62 + 2 * 84 + 34 + 70 + (showAdvanced ? 62 + 40 : 0) + 8;
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
            field (whChannelLabel, whChannelBox, &whHint);
            confirmButton.setBounds (m.removeFromTop (34).withWidth (260));
            confirmHint.setBounds (m.removeFromTop (34).withTrimmedTop (2));
            m.removeFromTop (2);
            advancedButton.setBounds (m.removeFromTop (30).withWidth (220));
            m.removeFromTop (4);
            if (showAdvanced)
            {
                field (pcBaseLabel, pcBaseBox, nullptr);
                setlistToggle.setBounds (m.removeFromTop (36));
            }
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
        advancedButton.setButtonText (juce::String (showAdvanced ? "v" : ">") + "  Advanced: numbering, setlist");
        for (auto* c : std::initializer_list<juce::Component*> { &pcBaseLabel, &pcBaseBox, &setlistToggle })
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
                isKemper() ? juce::String ("Track 'Kemper Cues': insert PedalCues and set its MIDI output to the Kemper (USB) or MIDI Out 1.")
                           : juce::String ("Track 'QC Cues': insert PedalCues and set its MIDI output to the Quad Cortex (USB) or MIDI Out 1."),
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
    juce::ToggleButton setlistToggle { "Send setlist (CC#32) with preset changes" };
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

std::unique_ptr<juce::Component> makeWiringGuide (int ampUnit)
{
    return std::make_unique<WiringGuide> (ampUnit);
}

void showWiringGuide (int ampUnit)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (makeWiringGuide (ampUnit).release());
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
