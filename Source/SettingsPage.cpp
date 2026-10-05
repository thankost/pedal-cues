#include "EditorCommon.h"
#include "Update.h"
#include "Modellers.h"

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
            const Node usb[] = { { "Amp modeller track", "e.g. " + amp.shortName + " Cues", accent },
                                 { "Amp modeller", "e.g. " + amp.box + " (USB)", amp.colour } };
            const char* usbLinks[] = { "USB" };
            const Node din[] = { { "Amp modeller track", "e.g. " + amp.shortName + " Cues", accent },
                                 { "MIDI interface", "MIDI Out", grey },
                                 { "Amp modeller", "e.g. " + amp.box + (amp.isTrs() ? juce::String (", TRS In") : juce::String()), amp.colour } };
            const char* dinLinks[] = { "USB", "MIDI cable" };
            if (! amp.usbMidi)   // no MIDI over USB from a computer (HeadRush): the MIDI cable only
            {
                paintChain (g, area.withSizeKeepingCentre (area.getWidth(), 66), din, dinLinks, 3);
                return;
            }
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
            const Node nodes[] = { { "Cue tracks", "one per device", accent },
                                   { "Interface", "MIDI Out", grey },
                                   { "Amp modeller", "e.g. " + amp.box, amp.colour },
                                   { "Second device", "e.g. a Whammy", second } };
            const char* links[] = { "USB", "MIDI cable", "Thru" };
            paintChain (g, area.withSizeKeepingCentre (area.getWidth(), 66), nodes, links, 4);
            return;
        }

        const Node ampRow[] = { { "Amp modeller track", "e.g. " + amp.shortName + " Cues", accent },
                                { "Amp modeller", "e.g. " + amp.box + " (USB)", amp.colour } };
        const char* ampLinks[] = { "USB" };
        const Node other[] = { { "Second device track", "e.g. Whammy Cues", accent },
                               { "MIDI interface", "MIDI Out 2", grey },
                               { "Second device", "e.g. a Whammy (5-pin MIDI In)", second } };
        const char* otherLinks[] = { "USB", "MIDI cable" };
        auto top = area.removeFromTop (rowH);
        area.removeFromTop (8);
        if (! amp.usbMidi)
        {
            const Node cableRow[] = { { "Amp modeller track", "e.g. " + amp.shortName + " Cues", accent },
                                      { "MIDI interface", "MIDI Out 1", grey },
                                      { "Amp modeller", "e.g. " + amp.box + (amp.isTrs() ? juce::String (", TRS In") : juce::String()), amp.colour } };
            paintChain (g, top, cableRow, otherLinks, 3);
        }
        else
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
        const auto trsNote = amp.isTrs() ? " Its MIDI jacks are 3.5 mm TRS (Type A): use a TRS MIDI cable or a 5-pin to TRS adapter."
                                         : juce::String();
        oneDevice.oneDevice = true;
        oneDevice.steps = { "USB from the " + d + " to the computer" + (amp.isCustom() ? juce::String (", if it has USB MIDI.") : juce::String (".")),
                            "Or a MIDI cable from your interface's MIDI Out to the " + d + "'s MIDI In." + trsNote,
                            "Set the " + d + "'s MIDI channel in MIDI Setup, the same as on the device (not Omni)." };
        if (! amp.hasDin)
            oneDevice.steps = { "USB from the " + d + " to the computer. It has no 5-pin MIDI, so USB is the only way.",
                                "Set the " + d + "'s MIDI channel in MIDI Setup, the same as on the device (not Omni)." };
        else if (! amp.usbMidi)
            oneDevice.steps = { "A MIDI cable from your interface's MIDI Out to the " + d + "'s MIDI In." + trsNote
                                    + " Its manual doesn't mention MIDI over USB from a computer, so use a MIDI cable.",
                                "Set the " + d + "'s MIDI channel in MIDI Setup, the same as on the device (not Omni)." };

        const auto thruNote = amp.kind == AmpInfo::Kind::quadCortex ? juce::String (" Turn MIDI Thru on in the QC.")
                            : amp.isKemper() ? juce::String (" If your Kemper shares one jack for MIDI Out and Thru, set it to Thru.")
                            : amp.isModeller() ? juce::String (" Turn MIDI Thru on in its MIDI settings.")
                                             : juce::String (" See its manual for the Thru setting.");
        daisy.steps = { "A MIDI cable from your interface's MIDI Out to the " + d + "'s MIDI In." + trsNote,
                        "A MIDI cable from the " + d + "'s MIDI Thru to the second device's MIDI In (for example a Whammy)." + thruNote,
                        "Give the two devices different MIDI channels, so each only reacts to its own cues." };
        if (! amp.hasDin)
        {
            daisy.steps = { "The " + d + " has no 5-pin MIDI, so it can't pass MIDI on to a second device: there's no daisy chain.",
                            "Use Separate outputs instead: the " + d + " on USB, the second device on your interface's MIDI Out." };
            daisy.showFlow = false;
        }
        else if (! amp.hasThru)
        {
            daisy.steps = { "The " + d + " has no MIDI Out, so it can't pass MIDI on to a second device: there's no daisy chain.",
                            "Use Separate outputs instead: the " + d + " on USB (or its " + amp.midiIn + "), the second device on your "
                            "interface's MIDI Out." };
            daisy.showFlow = false;
        }

        separate.viaInterface = true;
        separate.steps = { ! amp.usbMidi ? d + ": a MIDI cable from MIDI Out 1 to its MIDI In." + trsNote
                           : d + ": USB to the computer" + (amp.isCustom() ? juce::String (" (if it has USB MIDI)") : juce::String())
                             + (amp.hasDin ? ", or a MIDI cable from MIDI Out 1 to its MIDI In." : " (it has no 5-pin MIDI)."),
                           "Second device (for example a Whammy): a MIDI cable from MIDI Out 2 to its MIDI In. "
                           "The Whammy has no USB MIDI, so it always needs a MIDI cable.",
                           "Each DAW track sends to the output its device is on." };

        // Generic roles, with the picked unit as the example: "the amp modeller (e.g. Quad Cortex)" the first time, then "the amp modeller".
        for (auto* list : { &oneDevice, &daisy, &separate })
        {
            bool named = false;
            for (auto& step : list->steps)
            {
                const auto example = named ? juce::String() : " (e.g. " + d + ")";
                if (step.startsWith (d + ": "))
                    step = "Amp modeller" + example + ": " + step.fromFirstOccurrenceOf (d + ": ", false, false);
                else if (step.contains ("the " + d) || step.contains ("The " + d))
                {
                    const auto first = juce::jmax (step.indexOf ("the " + d), step.indexOf ("The " + d));
                    const auto prefixUpper = step.substring (first, first + 1) == "T";
                    const auto rest = step.substring (first + 4 + d.length());
                    const auto possessive = rest.startsWith ("'s");
                    // "the amp modeller's MIDI In (e.g. Quad Cortex)": the example goes after the noun, at the end of the sentence.
                    step = step.substring (0, first) + (prefixUpper ? "The amp modeller" : "the amp modeller") + (possessive ? "'s" + rest.substring (2) : example + rest);
                    if (possessive && example.isNotEmpty())
                    {
                        const auto stop = step.indexOf (". ") >= 0 ? step.indexOf (". ") : step.lastIndexOf (".");
                        step = stop >= 0 ? step.substring (0, stop) + example + step.substring (stop) : step + example;
                    }
                    step = step.replace ("the " + d, "the amp modeller").replace ("The " + d, "The amp modeller");
                }
                else
                    continue;
                named = true;
            }
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
        for (int i = 0; i < viewButtons.size(); ++i)   // exactly one lit, whatever was clicked before
            viewButtons[i]->setToggleState (i == current, juce::dontSendNotification);
        oneDevice.setVisible (current == 0);
        daisy.setVisible (current == 1);
        separate.setVisible (current == 2);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (background);
        const juce::String titles[] = { "ONE DEVICE, E.G. A " + amp.box.toUpperCase(),
                                        "DAISY CHAIN THROUGH THE AMP MODELLER (E.G. " + amp.box.toUpperCase() + ")",
                                        "SEPARATE OUTPUTS" };
        const juce::String subs[] = { "Just your amp modeller (shown for the " + amp.box + " picked in MIDI Setup): one cue track, sent over "
                                          + (amp.usbMidi ? "USB or " : "") + "a MIDI cable.",
                                      "A second device on the amp modeller's MIDI Thru. Both tracks send to the same interface MIDI Out.",
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
        // What happens to USB MIDI at the unit's Thru: from its manual where it says, otherwise the general rule.
        juce::String warn;
        if (! amp.hasDin)
            warn = "The " + amp.box + " has no 5-pin MIDI: it can't pass MIDI on to a second device. Give the second device its own MIDI output.";
        else if (! amp.hasThru)
            warn = "The " + amp.box + " has no MIDI Out: it can't pass MIDI on to a second device. Give the second device its own MIDI output.";
        else if (! amp.usbMidi)
            warn = {};   // MIDI cables only (HeadRush): the steps say so, and its Thru passes on its MIDI In
        else if (amp.usbToThru == 2)
            warn = "On USB only, a second device on the " + amp.box + "'s Thru gets MIDI only with " + amp.usbThruSetting
                 + " on (see the manual). Without it, use the " + amp.midiIn + ".";
        else if (amp.usbToThru == 3)
            warn = "Usually doesn't work: the " + amp.box + " on USB only, with a second device on its MIDI Thru. Its manual doesn't say whether "
                   "USB MIDI is passed on, so send to its " + amp.midiIn + " to be safe.";
        else
            warn = "Doesn't work: the " + amp.box + " on USB only, with a second device on its MIDI Thru. A MIDI Thru only passes on MIDI from the "
                   "5-pin MIDI In. "
                 + (amp.kind == AmpInfo::Kind::quadCortex ? juce::String ("The Quad Cortex never forwards USB MIDI to its Thru.")
                    : amp.isKemper() ? juce::String ("Kemper confirms USB MIDI has no MIDI Thru.")
                    : amp.isModeller() ? juce::String ("Its manual says so too.")
                                       : juce::String ("That's true for most devices; check its manual."));
        g.setColour (whammyRed);
        g.setFont (font (12.5f, true));
        g.drawFittedText (warn, warning, juce::Justification::centredLeft, 2);
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

// Looks like a drop-down, opens the device list (the same searchable picker as the ▾ on the first tab).
class DeviceButton final : public juce::Button
{
public:
    DeviceButton() : juce::Button ("Device") {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto b = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (down ? surfaceHi.brighter (0.05f) : over ? surfaceHi : raised);
        g.fillRoundedRectangle (b, 8.0f);
        auto r = getLocalBounds().reduced (14, 0);
        const auto arrow = r.removeFromRight (16).toFloat().withSizeKeepingCentre (10.0f, 6.0f);
        juce::Path p;
        p.startNewSubPath (arrow.getX(), arrow.getY());
        p.lineTo (arrow.getCentreX(), arrow.getBottom());
        p.lineTo (arrow.getRight(), arrow.getY());
        g.setColour (dim);
        g.strokePath (p, juce::PathStrokeType (1.6f));
        g.setColour (theme::text);
        g.setFont (font (15.0f));
        g.drawFittedText (getButtonText(), r, juce::Justification::centredLeft, 1, 0.85f);
    }
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
        // One row per tab: the device (opens that tab's list), its channel and, in the standalone app, a Test tick box.
        styleCaption (ampLabel, "Amps & Modellers");
        styleCaption (whChannelLabel, "Effects & Pedals");
        styleCaption (qcChannelLabel, "Channel");
        pedalBox.setTooltip ("The pedal on the Effects & Pedals tab: a Whammy V or DT, a DL4 MkII, one of your MIDI devices, or none. Opens the same list as that tab's arrow.");
        pedalBox.onClick = [this] { showUnitPicker (state, pedalBox, {}, true); };
        ampBox.setTooltip ("Quad Cortex, Nano Cortex, Kemper, a Fractal, Line 6, HeadRush or Darkglass unit, or your own MIDI device: the first tab and the channel below "
                           "follow it. Opens the same searchable list as the arrow on the first tab.");
        ampBox.onClick = [this] { showUnitPicker (state, ampBox, {}); };
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

        qcChannelBox.onChange = [this] { state.setProperty (IDs::qcChannel, qcChannelBox.getSelectedId(), nullptr); };
        whChannelBox.onChange = [this]
        {
            // The pedal's own channel: the Whammy's, or the custom device's (kept with the device).
            if (auto u = state::fxCustomUnit (state); u.isValid())
                u.setProperty (IDs::channel, whChannelBox.getSelectedId(), nullptr);
            else if (auto m = state::fxModellerData (state); m.isValid())
                m.setProperty (IDs::channel, whChannelBox.getSelectedId(), nullptr);
            else
                state.setProperty (IDs::whChannel, whChannelBox.getSelectedId(), nullptr);
        };


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
        styleCaption (wiringLabel, "How are they connected?");
        chainNote.setFont (font (12.5f));
        chainNote.setColour (juce::Label::textColourId, accent);
        chainNote.setJustificationType (juce::Justification::topLeft);
        chainNote.setMinimumHorizontalScale (1.0f);
        addChildComponent (chainNote);
        styleCaption (dawLabel, "Your DAW");
        styleHint (dawHint, {});
        for (auto* d : { "Reaper", "Ableton Live", "Cubase / Nuendo", "Logic Pro", "Other" })
            dawBox.addItem (d, dawBox.getNumItems() + 1);
        dawBox.setSelectedItemIndex (juce::jlimit (0, 4, state::getSetting ("daw").getIntValue()), juce::dontSendNotification);
        dawBox.onChange = [this] { state::setSetting ("daw", juce::String (dawBox.getSelectedItemIndex())); updateDawHint(); };
        updateDawHint();
        styleHint (examplesNote, {});
        examplesNote.setColour (juce::Label::textColourId, accent);

        testOutBox.onChange = [this]
        {
            const auto i = testOutBox.getSelectedItemIndex();
            proc.setDirectMidiOutput (juce::isPositiveAndBelow (i - 1, testDevices.size()) ? testDevices[i - 1].identifier : juce::String());
            updateTestState();
        };
        testButton.setTooltip ("Sends a quick test to every device ticked above, on its channel: a check that each one gets MIDI.");
        testButton.onClick = [this]
        {
            juce::StringArray results;
            if (testAmpToggle.getToggleState())
                results.add (testAmp());
            if (testPedalToggle.getToggleState())
                results.add (testPedal());
            setTestStatus (results.isEmpty() ? juce::String ("Tick Test next to the devices you want to check, then click Test selected.")
                                             : results.joinIntoString ("\n\n"));
        };
        for (auto* t : { &testAmpToggle, &testPedalToggle })
        {
            t->setButtonText ("Test");
            t->setToggleState (true, juce::dontSendNotification);
            t->setTooltip ("Include this device in Test selected");
        }
        testButton.setColour (juce::TextButton::buttonColourId, accent);
        testButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);

        for (auto* c : std::initializer_list<juce::Component*> { &ampLabel, &ampBox, &pedalBox, &qcChannelLabel, &whChannelLabel, &qcChannelBox, &whChannelBox,
                                                                 &qcHint, &whHint, &confirmButton, &confirmHint, &tracksSteps, &viaQcButton,
                                                                 &viaInterfaceButton, &wiringLabel, &wiringLink, &dawHint, &dawLabel, &dawBox,
                                                                 &examplesNote })
            addAndMakeVisible (c);
        for (auto* c : std::initializer_list<juce::Component*> { &testOutBox, &testButton, &testAmpToggle, &testPedalToggle, &testHint })
            addChildComponent (c);

        const auto standalone = PedalCuesProcessor::isStandalone();
        for (auto* c : std::initializer_list<juce::Component*> { &testSection, &testOutBox, &testButton, &testAmpToggle, &testPedalToggle, &testHint })
            c->setVisible (standalone);

        setSetupMode (! state::getFlag ("setupViaQcChain"));
        refresh();
    }

    void refresh() override
    {
        qcChannelBox.setSelectedId ((int) state[IDs::qcChannel], juce::dontSendNotification);
        const auto pedal = pedalInfo (state);
        whChannelBox.setSelectedId (pedal.channel, juce::dontSendNotification);
        whChannelBox.setEnabled (! pedal.isNone);
        pedalBox.setButtonText (pedal.isNone ? juce::String ("No pedal (only one device)") : pedal.name);
        testPedalToggle.setVisible (PedalCuesProcessor::isStandalone() && ! pedal.isNone);
        // Until the channels are confirmed, say the devices shown are just a start.
        examplesNote.setText (pedal.isNone ? ampInfo (state).name + " is an example: click it to pick your amp modeller or MIDI device."
                                           : ampInfo (state).name + " and " + pedal.name + " are examples: click a device to pick yours "
                                             "(or No pedal if you only have one).", juce::dontSendNotification);
        examplesNote.setVisible (! state::getFlag (ui::channelsConfirmedFlag));
        tracksSection.hint = pedal.isNone ? "one cue track, once" : "two cue tracks, once";
        tracksSection.repaint();

        // The first channel belongs to the amp unit (picked here or on the first tab).
        const auto amp = ampInfo (state);
        ampBox.setButtonText (amp.isModeller() ? modellers::find (state[IDs::modellerProfile].toString())->model : amp.name);
        qcChannelLabel.setText ("Channel", juce::dontSendNotification);   // the column: each row's device channel
        qcHint.setText (amp.isModeller() ? amp.channelHint
                        : amp.isCustom() ? "Set the same channel on your " + amp.name + " (see its manual; not Omni)."
                        : isKemper() ? "Set the same channel on your Kemper: System Settings > MIDI > MIDI Global Channel (not OMNI)."
                                     : "Set the same channel on your QC: Settings > MIDI Settings > MIDI Channel (not Omni).", juce::dontSendNotification);
        whHint.setText (pedal.isNone ? juce::String ("Nothing on the Effects & Pedals tab: only one device to set up. Add a pedal with that tab's arrow.")
                        : pedal.page != nullptr ? pedal.page->channelHint + " Use a different channel from the " + ampShort() + "."
                        : pedal.isCustom ? "Set the same channel on your " + pedal.name + " (see its manual). Use a different channel from the "
                                         + ampShort() + "."
                                       : "Set the same channel on your Whammy (see its manual). Use a different channel from the " + ampShort() + ".",
                        juce::dontSendNotification);
        testAmpToggle.setTooltip ("Test selected includes the " + ampShort() + (amp.isCustom() ? juce::String (": it sends your first tile.") : juce::String (": its tuner on and off (or preset 1).")));
        testPedalToggle.setTooltip ("Test selected includes the " + pedal.shortName + (pedal.isCustom ? juce::String (": it sends your first tile.")
                                                                                      : pedal.page != nullptr ? juce::String (": it loads its first preset.")
                                                                                      : juce::String (": it steps through three modes.")));
        viaQcButton.setButtonText ("Daisy chain via " + ampShort());
        setSetupMode (! state::getFlag ("setupViaQcChain"));
        updateConfirm();
        refreshTestDevices();
    }

    void updateConfirm()
    {
        confirmButton.setToggleState (state::getFlag (ui::channelsConfirmedFlag), juce::dontSendNotification);
        confirmButton.setButtonText (juce::String (juce::CharPointer_UTF8 (confirmButton.getToggleState()
                                                                               ? "\xe2\x9c\x93  Done: my devices use these channels"
                                                                               : "Done: my devices use these channels")));
        examplesNote.setVisible (! confirmButton.getToggleState());
    }

    // The first tab's device: its tuner on and off, a first tile, preset 1 or a tap, whatever it has.
    juce::String testAmp()
        {
            juce::String status;
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
                            status = ("Sent your first tile, \"" + t[IDs::name].toString() + "\", to " + currentPortName() + " on channel "
                                           + juce::String (ch) + ". Did the " + ampShort() + " react? If not, check the cable direction and its channel.");
                        }
                if (c.events.empty())
                    status = ("Add a tile with MIDI messages on the " + ampShort() + " page first: Test sends your first tile.");
                else
                    proc.preview (c);
                return status;
            }
            const auto* model = ampInfo (state).isModeller() ? modellers::find (state[IDs::modellerProfile].toString()) : nullptr;
            if (model != nullptr && model->tunerOn.isEmpty() && model->testMessage.isNotEmpty())
            {
                // No tuner over MIDI (Darkglass amps): load preset 1.
                proc.preview (cues::custom::cue (ch, {}, model->testMessage, 0));
                status = ("Sent " + model->testMessage + " (preset 1) to " + currentPortName() + " on channel " + juce::String (ch)
                               + ". Did the " + ampShort() + " switch to preset 1? If not, check the cable direction and its channel ("
                               + model->channelHint + ").");
                return status;
            }
            if (model != nullptr && model->tunerOn.isEmpty() && ! model->utilities.empty())
            {
                // No tuner over MIDI (older HeadRush units): one tap instead.
                proc.preview (modellers::action (*model, ch, model->utilities.front()));
                status = ("Sent " + model->utilities.front().name + " to " + currentPortName() + " on channel " + juce::String (ch) + ". Did the "
                               + ampShort() + " react (its tempo LED)? If not, check the cable direction, its channel (" + model->channelHint + ") and MIDI Thru.");
                return status;
            }
            if (model != nullptr)
            {
                // The unit's tuner on, then off ~1.5 s later (Line 6: the same toggle twice).
                for (const auto& [text, beat] : { std::pair<juce::String, double> { model->tunerOn, 0.0 }, { model->tunerOff, proc.getHostBpm() / 40.0 } })
                    for (const auto& [b, m] : cues::custom::cue (ch, {}, text, 0).events)
                        c.add (beat + b, m);
                proc.preview (c);
                status = ("Sent tuner on/off to " + currentPortName() + " on channel " + juce::String (ch) + ". Did the " + ampShort()
                               + "'s tuner open and close? If not, check the cable direction, its channel (" + model->channelHint + ") and MIDI Thru.");
                return status;
            }
            const auto kemper = isKemper();
            c.add (0.0, (kemper ? cues::kemper::tuner (ch, true) : cues::qc::tuner (ch, true)).events.front().second);
            c.add (proc.getHostBpm() / 40.0, (kemper ? cues::kemper::tuner (ch, false) : cues::qc::tuner (ch, false)).events.front().second);   // ~1.5 s later
            proc.preview (c);
            status = ("Sent tuner on/off to " + currentPortName() + " on channel " + juce::String (ch)
                           + ". Did the " + ampShort() + "'s tuner open and close (or close, if it was open)? If not, check the cable direction, the "
                           + ampShort() + "'s channel, and MIDI Thru for the daisy chain.");
            return status;
        }

    // The second tab's pedal: the Whammy steps through three modes; a custom device sends its first tile.
    juce::String testPedal()
        {
            juce::String status;
            const auto pedal = pedalInfo (state);
            if (pedal.isCustom)
            {
                const auto unit = state::fxCustomUnit (state);
                cues::Cue c;
                for (auto g : unit)
                    for (auto t : g)
                        if (c.events.empty() && cues::custom::parse (t[IDs::messages].toString(), (int) unit[IDs::programBase]).ok())
                        {
                            c = cues::custom::cue (pedal.channel, t[IDs::name].toString(), t[IDs::messages].toString(), (int) unit[IDs::programBase]);
                            status = "Sent your first tile, \"" + t[IDs::name].toString() + "\", to " + currentPortName() + " on channel "
                                   + juce::String (pedal.channel) + ". Did the " + pedal.shortName + " react? If not, check the cable direction and its channel.";
                        }
                if (c.events.empty())
                    return "Add a tile with MIDI messages on the " + pedal.shortName + " page first: Test sends your first tile.";
                proc.preview (c);
                return status;
            }
            if (const auto* page = pedal.page; page != nullptr && page->testMessage.isNotEmpty())
            {
                // DL4 MkII, HX One: no tuner screen to open and close, so load the first preset.
                proc.preview (cues::custom::cue (pedal.channel, {}, page->testMessage, 0));
                return "Sent " + page->testMessage + " (" + modellers::presetLabel (*page, -1, 0) + ") to " + currentPortName() + " on channel "
                     + juce::String (pedal.channel) + ". Did the " + pedal.shortName + " switch to it? If not, check the cable direction, its channel ("
                     + page->channelHint + ") and MIDI Thru.";
            }
            return testWhammy();
        }

    juce::String testWhammy()
        {
            juce::String status;
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
                status = ("Sent Oct Up, 5th Up, 2 Oct Up to " + currentPortName() + ", but that's the " + ampShort() + "'s USB port: its MIDI Thru "
                               "doesn't pass USB MIDI on, so a Whammy on its Thru won't react. Send to your interface's MIDI Out instead.");
            else
                status = ("Sent Oct Up, 5th Up, 2 Oct Up to " + currentPortName() + " on channel " + juce::String ((int) state[IDs::whChannel])
                               + ". Did the Whammy's mode LED step along? If not, check the cable direction, the Whammy's channel, and MIDI Thru for the daisy chain.");
            return status;
        }

    bool isKemper() const          { return ampInfo (state).isKemper(); }
    juce::String ampShort() const  { return ampInfo (state).shortName; }
    juce::String pedalShort() const { return pedalInfo (state).shortName; }

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

        const auto pedalsH = Section::headerHeight + 22 + 2 * 92 + 34 + 70 + 8;
        pedalsSection.setBounds (left.removeFromTop (pedalsH));
        {
            // A list: one row per tab, device | channel | (standalone) Test, with the hint below.
            auto m = pedalsSection.contentArea().reduced (6, 2);
            examplesNote.setBounds (m.removeFromTop (examplesNote.isVisible() ? 22 : 0));
            const auto standalone = PedalCuesProcessor::isStandalone();
            auto device = [&m, standalone] (juce::Label& caption, juce::Component& box, juce::ComboBox& channel, juce::ToggleButton& test, juce::Label& hint)
            {
                auto row = m.removeFromTop (92);
                caption.setBounds (row.removeFromTop (22).withTrimmedRight (standalone ? 198 : 128));   // clear of the Channel column
                auto line = row.removeFromTop (32);
                if (standalone)
                    test.setBounds (line.removeFromRight (70).withTrimmedLeft (8));
                channel.setBounds (line.removeFromRight (120));
                line.removeFromRight (8);
                box.setBounds (line);
                hint.setBounds (row.withTrimmedTop (2));
            };
            device (ampLabel, ampBox, qcChannelBox, testAmpToggle, qcHint);
            device (whChannelLabel, pedalBox, whChannelBox, testPedalToggle, whHint);
            confirmButton.setBounds (m.removeFromTop (34).withWidth (260));
            confirmHint.setBounds (m.removeFromTop (34).withTrimmedTop (2));
            qcChannelLabel.setBounds (qcChannelBox.getX(), ampLabel.getY(), qcChannelBox.getWidth(), ampLabel.getHeight());
        }

        if (PedalCuesProcessor::isStandalone())
        {
            left.removeFromTop (12);
            testSection.setBounds (left.removeFromTop (juce::jmin (left.getHeight(), Section::headerHeight + 140)));
            auto t = testSection.contentArea().reduced (6, 4);
            testOutBox.setBounds (t.removeFromTop (32));
            t.removeFromTop (8);
            testButton.setBounds (t.removeFromTop (32).withWidth (200));
            t.removeFromTop (8);
            testHint.setBounds (t);
        }

        // Right: DAW tracks, with the wiring choice in its header and a link to the wiring guide.
        tracksSection.setBounds (r);
        {
            auto t = tracksSection.contentArea().reduced (8, 6);
            wiringLink.setBounds (t.removeFromBottom (30).removeFromRight (260));
            // Two devices: how they're connected, as two choices with a line each. One device: no choice to make.
            if (viaQcButton.isVisible())
            {
                wiringLabel.setBounds (t.removeFromTop (22));
                auto options = t.removeFromTop (58);
                viaInterfaceButton.setBounds (options.removeFromLeft (options.getWidth() / 2 - 4));
                options.removeFromLeft (8);
                viaQcButton.setBounds (options);
                t.removeFromTop (8);
                if (chainNote.isVisible())
                {
                    chainNote.setBounds (t.removeFromTop (36));
                    t.removeFromTop (4);
                }
                else
                    t.removeFromTop (4);
            }
            tracksSteps.setBounds (t.removeFromTop (juce::jmin (t.getHeight() - 120, tracksSteps.steps.size() * tracksSteps.rowHeight)));
            t.removeFromTop (8);
            auto daw = t.removeFromTop (30);
            dawLabel.setBounds (daw.removeFromLeft (90));
            dawBox.setBounds (daw.removeFromLeft (200));
            t.removeFromTop (6);
            dawHint.setBounds (t.removeFromTop (54));
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
        testButton.setEnabled (hasPort);
        if (! hasPort)
            setTestStatus (testDevices.isEmpty() ? "No MIDI devices found. Connect your audio interface (for its MIDI Out), or the " + ampShort() + " over USB "
                                                   "to test only the " + ampShort() + " (its MIDI Thru doesn't pass USB MIDI on to the Whammy). Then come back to this tab."
                                                 : "Pick the MIDI port your pedals are on to test them.");
        else if (testHint.getText().isEmpty() || testHint.getText().startsWith ("Pick") || testHint.getText().startsWith ("No MIDI"))
            setTestStatus ("The app sends straight to this port, no DAW or audio needed. Tick the devices above, click Test selected, then use any tile's play button.");
    }

    void setTestStatus (const juce::String& t)
    {
        styleHint (testHint, t);
    }

    void setSetupMode (bool viaInterface)
    {
        const auto amp = ampInfo (state);
        if (amp.canChain())
            state::setFlag ("setupViaQcChain", ! viaInterface);
        else
            viaInterface = true;   // USB only (POD Go) or no MIDI Out (Nano Cortex): no daisy chain; the saved choice stays for other units
        viaQcButton.setEnabled (amp.canChain());
        viaQcButton.setTooltip (amp.canChain() ? "Interface MIDI Out > " + amp.box + " " + amp.midiIn + " > its MIDI Thru > the second device (e.g. a Whammy). "
                                                 "A MIDI Thru only passes on MIDI from the MIDI In jack, so this needs an interface MIDI Out."
                                : ! amp.hasDin ? "The " + amp.box + " has no 5-pin MIDI, so it can't be in a daisy chain."
                                               : "The " + amp.box + " has no MIDI Out, so it can't be in a daisy chain.");
        viaInterfaceButton.setTooltip ("Each device on its own output: the " + amp.box + (! amp.usbMidi ? juce::String (" on MIDI Out 1")
                                       : " over USB" + (amp.hasDin ? juce::String (" (or MIDI Out 1)") : juce::String()))
                                       + ", the second device (e.g. a Whammy) on a MIDI Out");
        (viaInterface ? viaInterfaceButton : viaQcButton).setToggleState (true, juce::dontSendNotification);
        viaInterfaceButton.setButtonText ("Each device on its own output\n(the simplest)");
        viaQcButton.setButtonText ("One cable through the " + amp.shortName
                                   + (amp.usbToThru == 1 ? juce::String ("\n(daisy chain, via its MIDI Thru)") : juce::String ("\n(daisy chain: MIDI cable in, not USB)")));
        // Picked the daisy chain: say plainly whether this unit passes USB MIDI on to its Thru (most don't).
        chainNote.setText (amp.usbToThru == 1 ? "The " + amp.box + " also passes USB MIDI on to its Thru, so both tracks can send to its USB port instead."
                           : amp.usbToThru == 2 ? "The " + amp.box + " passes USB MIDI on to its Thru only with " + amp.usbThruSetting
                                                  + " on. Without it, send both tracks to your interface's MIDI Out (a cable into its " + amp.midiIn + ")."
                           : amp.usbToThru == 3 ? "The " + amp.box + "'s manual doesn't say whether USB MIDI reaches its Thru: send both tracks to your "
                                                  "interface's MIDI Out (a cable into its " + amp.midiIn + "), not its USB port."
                                                : "The " + amp.box + " doesn't pass USB MIDI on to its Thru: send both tracks to your interface's MIDI Out "
                                                  "(a cable into its " + amp.midiIn + "), not its USB port.",
                           juce::dontSendNotification);

        const auto pedal = pedalInfo (state);
        for (auto* c : std::initializer_list<juce::Component*> { &viaQcButton, &viaInterfaceButton, &wiringLabel })
            c->setVisible (! pedal.isNone);
        chainNote.setVisible (! pedal.isNone && ! viaInterface && amp.usbMidi);   // MIDI cables only (HeadRush): nothing to warn about
        // Generic roles (amp track, pedal track) with this setup's devices as the example names.
        const auto ampTrack = "'" + ampShort() + " Cues'", pedalTrack = "'" + pedalShort() + " Cues'";
        const auto ampPort = ! amp.usbMidi ? juce::String ("MIDI Out 1")
                           : amp.hasDin ? amp.box + " (USB) or MIDI Out 1" : amp.box + " (USB; it has no 5-pin MIDI)";
        if (pedal.isNone)
        {
            // One device: one cue track.
            tracksSteps.steps = {
                "In your DAW, enable the MIDI output your device is on: "
                    + (! amp.usbMidi ? "your interface's MIDI Out (e.g. for the " + amp.box + ", it needs a MIDI cable)."
                       : amp.hasDin ? "its USB port or your interface's MIDI Out (e.g. " + amp.box + ")." : "its USB port (e.g. " + amp.box + ")."),
                "Make one cue track, e.g. " + ampTrack + ": insert PedalCues and set its MIDI output to that port.",
                "Keep the original MIDI channels, and drag the tiles onto that track."
            };
            tracksSteps.repaint();
            resized();
            return;
        }

        if (viaInterface)
        {
            tracksSteps.steps = {
                "In your DAW, enable the MIDI outputs your devices are on.",
                "Amp modeller track, e.g. " + ampTrack + ": insert PedalCues and set its MIDI output to the amp modeller's port (e.g. " + ampPort + ").",
                "Pedal track, e.g. " + pedalTrack + ": insert PedalCues and set its MIDI output to the pedal's port (e.g. MIDI Out 2).",
                "Keep the original MIDI channels. Drag Amps & Modellers tiles onto the amp modeller track, Effects & Pedals tiles onto the pedal track."
            };
        }
        else
        {
            tracksSteps.steps = {
                "In your DAW, enable your interface's MIDI output.",
                "Make two cue tracks, an amp modeller track and a pedal track (e.g. " + ampTrack + " and " + pedalTrack + "), and insert PedalCues on each.",
                "Set both tracks' MIDI output to the interface MIDI Out (it goes into the " + amp.shortName + " and through its Thru to the pedal), "
                    "keeping the original MIDI channels.",
                "Drag Amps & Modellers tiles onto the amp modeller track and Effects & Pedals tiles onto the pedal track."
            };
        }
        tracksSteps.repaint();
        resized();
    }

    // The DAW picker: only that DAW's "where's the MIDI output" line.
    void updateDawHint()
    {
        static const char* hints[] = {
            "Reaper: the track's I/O button > MIDI Hardware Output, and enable the port in Preferences > MIDI Devices (output). Keep "
            "'Send to original channels'.",
            "Ableton Live: the track's MIDI To. Enable the port as Track in Preferences > Link, Tempo & MIDI.",
            "Cubase / Nuendo: the track's MIDI output in the Inspector.",
            "Logic Pro: use an External MIDI track and pick the port. Logic can't send the tiles' play-button tests to hardware: use the "
            "standalone app for those.",
            "Other DAWs: a MIDI track whose output is set to the port your device is on. The clips keep their own MIDI channels." };
        dawHint.setText (hints[juce::jlimit (0, 4, dawBox.getSelectedItemIndex())], juce::dontSendNotification);
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;

    Section pedalsSection { "set.pedals", "Your devices", "set once, must match your devices" };
    Section testSection   { "set.test", "Test your devices", "sends on the channels above", ledGreen };
    Section tracksSection { "set.tracks", "Set up your DAW", "two cue tracks, once", qcBlue };

    juce::Label ampLabel, qcChannelLabel, whChannelLabel, qcHint, whHint, testHint;
    DeviceButton ampBox;
    juce::ComboBox qcChannelBox, whChannelBox, testOutBox;
    juce::TextButton confirmButton;
    juce::Label confirmHint;

    StepsList tracksSteps;
    juce::Label wiringLabel, dawHint, dawLabel, examplesNote;
    juce::Label chainNote;   // the daisy chain: what this unit does with USB MIDI at its Thru
    juce::ComboBox dawBox;
    juce::TextButton wiringLink { "Wiring guide: cables and diagrams >" };
    juce::TextButton viaQcButton { "Daisy chain via QC" };
    juce::TextButton viaInterfaceButton { "Separate outputs" };

    juce::Array<juce::MidiDeviceInfo> testDevices;
    juce::TextButton testButton { "Test selected" };
    juce::ToggleButton testAmpToggle, testPedalToggle;   // standalone: which devices Test selected checks
    DeviceButton pedalBox;                               // the second tab's pedal (opens its device list)

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
