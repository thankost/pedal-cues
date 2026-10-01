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
    bool viaInterface = false;   // true = separate outputs, false = daisy chain through the QC
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
            paintFlow (g, r.removeFromTop (juce::jmin (r.getHeight(), viaInterface ? 150 : 130)), viaInterface);
    }

private:
    struct Node { const char* title; const char* sub; juce::Colour colour; };

    static void paintFlow (juce::Graphics& g, juce::Rectangle<int> area, bool viaInterface)
    {
        g.setColour (dim);
        g.setFont (font (11.0f, true));
        g.drawText ("SIGNAL FLOW", area.removeFromTop (20), juce::Justification::centredLeft);

        if (! viaInterface)
        {
            // The QC only passes on MIDI from its 5-pin MIDI In, not from USB, so the chain starts at an interface.
            const Node nodes[] = { { "Cue tracks", "QC + Whammy", accent },
                                   { "Interface", "MIDI Out", juce::Colour (0xff9aa0ac) },
                                   { "Quad Cortex", "In + Thru", qcBlue },
                                   { "Whammy V", "5-pin MIDI In", whammyRed } };
            const char* links[] = { "USB", "MIDI", "MIDI" };
            paintChain (g, area.withSizeKeepingCentre (area.getWidth(), 66), nodes, links, 4);
            return;
        }

        const auto rowH = (area.getHeight() - 8) / 2;
        const Node qc[] = { { "QC Cues track", "PedalCues, Quad Cortex tab", accent },
                            { "Quad Cortex", "USB, or MIDI Out 1", qcBlue } };
        const char* qcLinks[] = { "USB/MIDI" };
        const Node wh[] = { { "Whammy Cues track", "PedalCues, Whammy tab", accent },
                            { "Audio interface", "MIDI Out 2", juce::Colour (0xff9aa0ac) },
                            { "Whammy V", "5-pin MIDI In", whammyRed } };
        const char* whLinks[] = { "USB", "MIDI" };
        auto top = area.removeFromTop (rowH);
        area.removeFromTop (8);
        paintChain (g, top.removeFromLeft ((top.getWidth() * 2 - 56) / 3), qc, qcLinks, 2);   // same box width as the 3-box row
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

// Help > Wiring guide: which cables go where, for both setups, plus the one that doesn't work.
class WiringGuide final : public juce::Component
{
public:
    WiringGuide()
    {
        daisy.viaInterface = false;
        daisy.rowHeight = 44;
        daisy.steps = { "A MIDI cable from your interface's MIDI Out to the QC's MIDI In.",
                        "A MIDI cable from the QC's MIDI Out/Thru to the Whammy's MIDI In. Turn MIDI Thru on in the QC." };
        separate.viaInterface = true;
        separate.rowHeight = 44;
        separate.steps = { "Whammy: a MIDI cable from your interface's MIDI Out 2 to the Whammy's MIDI In.",
                           "Quad Cortex: USB to the computer, or a MIDI cable from MIDI Out 1 to the QC's MIDI In." };
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
        heading (daisyTitle, "DAISY CHAIN VIA QC", "Both Reaper tracks send to the same interface MIDI Out. The pedals share one cable.");
        heading (separateTitle, "SEPARATE OUTPUTS", "Each Reaper track sends to the output its pedal is on.");

        g.setColour (whammyRed);
        g.setFont (font (12.5f, true));
        g.drawFittedText ("Doesn't work: the QC on USB only, with the Whammy on the QC's MIDI Thru. "
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
    StepsList daisy, separate;
    juce::Rectangle<int> daisyTitle, separateTitle, warning;
    juce::TextButton guideButton { "Open the user guide" };
};

class SettingsPage final : public Page
{
public:
    SettingsPage (PedalCuesProcessor& p, std::function<void()> tour)
        : proc (p), state (p.state), startTour (std::move (tour))
    {
        for (auto* c : std::initializer_list<juce::Component*> { &pedalsSection, &tracksSection })
            addAndMakeVisible (c);
        addChildComponent (testSection);

        // Your pedals: the channels every cue is sent on.
        styleCaption (qcChannelLabel, "Quad Cortex channel");
        styleCaption (whChannelLabel, "Whammy V channel");
        styleCaption (pcBaseLabel, "Whammy program numbering");
        styleHint (qcHint, "Must match the QC: Settings > MIDI Settings > MIDI Channel (not Omni).");
        styleHint (whHint, "Must match the Whammy's MIDI channel (see its manual). Use a different channel from the QC.");

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

        // My wiring (in the Reaper tracks header): the steps below depend on it. The cable details are in Help > Wiring guide.
        for (auto* b : { &viaQcButton, &viaInterfaceButton })
        {
            b->setClickingTogglesState (true);
            b->setRadioGroupId (4201);
            b->setColour (juce::TextButton::buttonOnColourId, qcBlue);
            b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        }
        viaQcButton.setTooltip ("Interface MIDI Out > QC 5-pin MIDI In > QC MIDI Thru > Whammy. "
                                "MIDI Thru does not forward USB MIDI, so this needs a 5-pin MIDI Out.");
        viaInterfaceButton.setTooltip ("Each pedal on its own output: the QC over USB (or MIDI Out 1), the Whammy on MIDI Out 2");
        viaQcButton.onClick = [this] { if (viaQcButton.getToggleState()) setSetupMode (false); };
        viaInterfaceButton.onClick = [this] { if (viaInterfaceButton.getToggleState()) setSetupMode (true); };

        // Reaper tracks: the two cue tracks and their outputs. Standalone only: Test (send to a port, test each pedal).
        tracksSteps.showFlow = false;
        wiringLink.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        wiringLink.setColour (juce::TextButton::textColourOffId, qcBlue);
        wiringLink.onClick = [] { showWiringGuide(); };
        styleCaption (wiringLabel, "My wiring:");

        testOutBox.onChange = [this]
        {
            const auto i = testOutBox.getSelectedItemIndex();
            proc.setDirectMidiOutput (juce::isPositiveAndBelow (i - 1, testDevices.size()) ? testDevices[i - 1].identifier : juce::String());
            updateTestState();
        };
        testQcButton.setTooltip ("Opens the QC tuner for 1.5 seconds: a quick check that the QC gets MIDI on its channel.");
        testWhButton.setTooltip ("Selects 2 Oct Up on the Whammy: a quick check that it gets MIDI on its channel.");
        testQcButton.onClick = [this]
        {
            cues::Cue c;
            const auto ch = (int) state[IDs::qcChannel];
            c.add (0.0, cues::qc::tuner (ch, true).events.front().second);
            c.add (proc.getHostBpm() / 40.0, cues::qc::tuner (ch, false).events.front().second);   // ~1.5 s later
            proc.preview (c);
            setTestStatus ("Sent tuner on/off to " + currentPortName() + " on channel " + juce::String (ch)
                           + ". Did the QC's tuner open? If not, check the cable direction, the QC's channel, and MIDI Thru for the daisy chain.");
        };
        testWhButton.onClick = [this]
        {
            const auto wh = state.getChildWithName (IDs::Whammy);
            proc.preview (cues::whammy::effect ((int) state[IDs::whChannel], 0, wh.getChild (0)[IDs::name].toString(),
                                                (bool) state[IDs::whChords], (bool) state[IDs::whBypass],
                                                (int) state[IDs::whPcBase], false));
            if (currentPortName().containsIgnoreCase ("Quad Cortex") || currentPortName().containsIgnoreCase ("QC"))
                setTestStatus ("Sent 2 Oct Up to " + currentPortName() + ", but that's the Quad Cortex's USB port: the QC's MIDI Thru "
                               "doesn't pass USB MIDI on, so a Whammy on its Thru won't react. Send to your interface's MIDI Out instead.");
            else
                setTestStatus ("Sent 2 Oct Up to " + currentPortName() + " on channel " + juce::String ((int) state[IDs::whChannel])
                               + ". Did the Whammy's mode LED move? If not, check the cable direction, the Whammy's channel, and MIDI Thru for the daisy chain.");
        };
        for (auto* b : { &testQcButton, &testWhButton })
            b->setColour (juce::TextButton::buttonColourId, raised);

        tourButton.setColour (juce::TextButton::buttonColourId, accent);
        tourButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        tourButton.onClick = [this] { if (startTour) startTour(); };
        guideButton.onClick = [] { juce::URL (guideUrl).launchInDefaultBrowser(); };

        for (auto* c : std::initializer_list<juce::Component*> { &qcChannelLabel, &whChannelLabel, &qcChannelBox, &whChannelBox,
                                                                 &qcHint, &whHint, &advancedButton, &tracksSteps, &viaQcButton,
                                                                 &viaInterfaceButton, &wiringLabel, &wiringLink, &tourButton, &guideButton })
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
        refreshTestDevices();
    }

    void visibilityChanged() override
    {
        if (isVisible())
            refreshTestDevices();
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        auto left = r.removeFromLeft (juce::jmax (380, r.getWidth() * 2 / 5));
        r.removeFromLeft (12);

        // Left: Your pedals, then (standalone) Test, then the tour/guide buttons at the bottom.
        auto buttons = left.removeFromBottom (36);
        tourButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 5));
        buttons.removeFromLeft (10);
        guideButton.setBounds (buttons);
        left.removeFromBottom (12);

        const auto pedalsH = Section::headerHeight + 2 * 84 + 34 + (showAdvanced ? 62 + 40 : 0) + 8;
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
            field (qcChannelLabel, qcChannelBox, &qcHint);
            field (whChannelLabel, whChannelBox, &whHint);
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

        // Right: Reaper tracks, with the wiring choice in its header and a link to the wiring guide.
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
            tracksSteps.setBounds (t);
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

        testDevices = juce::MidiOutput::getAvailableDevices();
        testOutBox.clear (juce::dontSendNotification);
        testOutBox.addItem ("None", 1);
        int selected = 0;
        for (int i = 0; i < testDevices.size(); ++i)
        {
            testOutBox.addItem (testDevices[i].name, i + 2);
            if (testDevices[i].identifier == proc.getDirectMidiOutput())
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
        for (const auto& d : testDevices)
            if (d.identifier == proc.getDirectMidiOutput())
                return d.name;
        return "the MIDI port";
    }

    // The test buttons only make sense once a port is picked.
    void updateTestState()
    {
        const auto hasPort = proc.getDirectMidiOutput().isNotEmpty();
        testQcButton.setEnabled (hasPort);
        testWhButton.setEnabled (hasPort);
        if (! hasPort)
            setTestStatus (testDevices.isEmpty() ? "No MIDI devices found. Connect your audio interface (for its MIDI Out), or the Quad Cortex over USB "
                                                   "to test only the QC (its MIDI Thru doesn't pass USB MIDI on to the Whammy). Then come back to this tab."
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
                "Reaper > Preferences > MIDI Devices: enable the outputs your pedals are on.",
                "Track 'QC Cues': insert PedalCues. I/O > MIDI Hardware Output > Quad Cortex (USB) or MIDI Out 1.",
                "Track 'Whammy Cues': insert PedalCues. I/O > MIDI Hardware Output > MIDI Out 2.",
                "Leave both on 'Send to original channels'. Drag QC tiles onto QC Cues and Whammy tiles onto Whammy Cues."
            };
        }
        else
        {
            tracksSteps.steps = {
                "Reaper > Preferences > MIDI Devices: enable your interface's MIDI output.",
                "Tracks 'QC Cues' and 'Whammy Cues': insert PedalCues on each.",
                "On both tracks: I/O > MIDI Hardware Output > the interface MIDI Out, 'Send to original channels'.",
                "Drag QC tiles onto QC Cues and Whammy tiles onto Whammy Cues."
            };
        }
        tracksSteps.repaint();
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;
    std::function<void()> startTour;
    bool showAdvanced = false;

    Section pedalsSection { "set.pedals", "Your pedals", "set once, must match the pedals" };
    Section testSection   { "set.test", "Test your pedals", "sends on the channels above", ledGreen };
    Section tracksSection { "set.tracks", "Reaper tracks", "two cue tracks, once", qcBlue };

    juce::Label qcChannelLabel, whChannelLabel, pcBaseLabel, qcHint, whHint, testHint;
    juce::ComboBox qcChannelBox, whChannelBox, pcBaseBox, testOutBox;
    juce::ToggleButton setlistToggle { "Send setlist (CC#32) with preset changes" };
    juce::TextButton advancedButton;

    StepsList tracksSteps;
    juce::Label wiringLabel;
    juce::TextButton wiringLink { "How should I wire my pedals? >" };
    juce::TextButton viaQcButton { "Daisy chain via QC" };
    juce::TextButton viaInterfaceButton { "Separate outputs" };

    juce::Array<juce::MidiDeviceInfo> testDevices;
    juce::TextButton testQcButton { "Test QC" }, testWhButton { "Test Whammy" };

    juce::TextButton tourButton { "Show quick tour" };
    juce::TextButton guideButton { "Open user guide" };
};
} // namespace

std::unique_ptr<juce::Component> makeWiringGuide()
{
    return std::make_unique<WiringGuide>();
}

void showWiringGuide()
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (makeWiringGuide().release());
    o.dialogTitle = "Wiring guide";
    o.dialogBackgroundColour = background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}

std::unique_ptr<Page> makeSettingsPage (PedalCuesProcessor& p, std::function<void()> startTour)
{
    return std::make_unique<SettingsPage> (p, std::move (startTour));
}
} // namespace ui
