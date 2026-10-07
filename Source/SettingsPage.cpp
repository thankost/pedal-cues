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
            const Node usb[] = { { "Amp modeler track", "e.g. " + amp.shortName + " Cues", accent },
                                 { "Amp modeler", "e.g. " + amp.box + " (USB)", amp.colour } };
            const char* usbLinks[] = { "USB" };
            const Node din[] = { { "Amp modeler track", "e.g. " + amp.shortName + " Cues", accent },
                                 { "MIDI interface", "MIDI Out", grey },
                                 { "Amp modeler", "e.g. " + amp.box + (amp.isTrs() ? juce::String (", TRS In") : juce::String()), amp.colour } };
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
                                   { "Amp modeler", "e.g. " + amp.box, amp.colour },
                                   { "Second device", "e.g. a Whammy", second } };
            const char* links[] = { "USB", "MIDI cable", "Thru" };
            paintChain (g, area.withSizeKeepingCentre (area.getWidth(), 66), nodes, links, 4);
            return;
        }

        const Node ampRow[] = { { "Amp modeler track", "e.g. " + amp.shortName + " Cues", accent },
                                { "Amp modeler", "e.g. " + amp.box + " (USB)", amp.colour } };
        const char* ampLinks[] = { "USB" };
        const Node other[] = { { "Second device track", "e.g. Whammy Cues", accent },
                               { "MIDI interface", "MIDI Out 2", grey },
                               { "Second device", "e.g. a Whammy (5-pin MIDI In)", second } };
        const char* otherLinks[] = { "USB", "MIDI cable" };
        auto top = area.removeFromTop (rowH);
        area.removeFromTop (8);
        if (! amp.usbMidi)
        {
            const Node cableRow[] = { { "Amp modeler track", "e.g. " + amp.shortName + " Cues", accent },
                                      { "MIDI interface", "MIDI Out 1", grey },
                                      { "Amp modeler", "e.g. " + amp.box + (amp.isTrs() ? juce::String (", TRS In") : juce::String()), amp.colour } };
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
                            "Set the " + d + "'s MIDI channel at the top of its PedalCues tab, the same as on the device (not Omni)." };
        if (! amp.hasDin)
            oneDevice.steps = { "USB from the " + d + " to the computer. It has no 5-pin MIDI, so USB is the only way.",
                                "Set the " + d + "'s MIDI channel at the top of its PedalCues tab, the same as on the device (not Omni)." };
        else if (! amp.usbMidi)
            oneDevice.steps = { "A MIDI cable from your interface's MIDI Out to the " + d + "'s MIDI In." + trsNote
                                    + " Its manual doesn't mention MIDI over USB from a computer, so use a MIDI cable.",
                                "Set the " + d + "'s MIDI channel at the top of its PedalCues tab, the same as on the device (not Omni)." };

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
                    step = "Amp modeler" + example + ": " + step.fromFirstOccurrenceOf (d + ": ", false, false);
                else if (step.contains ("the " + d) || step.contains ("The " + d))
                {
                    const auto first = juce::jmax (step.indexOf ("the " + d), step.indexOf ("The " + d));
                    const auto prefixUpper = step.substring (first, first + 1) == "T";
                    const auto rest = step.substring (first + 4 + d.length());
                    const auto possessive = rest.startsWith ("'s");
                    // "the amp modeller's MIDI In (e.g. Quad Cortex)": the example goes after the noun, at the end of the sentence.
                    step = step.substring (0, first) + (prefixUpper ? "The amp modeler" : "the amp modeler") + (possessive ? "'s" + rest.substring (2) : example + rest);
                    if (possessive && example.isNotEmpty())
                    {
                        const auto stop = step.indexOf (". ") >= 0 ? step.indexOf (". ") : step.lastIndexOf (".");
                        step = stop >= 0 ? step.substring (0, stop) + example + step.substring (stop) : step + example;
                    }
                    step = step.replace ("the " + d, "the amp modeler").replace ("The " + d, "The amp modeler");
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
        show (state::getFlag ("setupViaQcChain") ? 1 : 2);   // the wiring picked in How to connect
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
                                        "DAISY CHAIN THROUGH THE AMP MODELER (E.G. " + amp.box.toUpperCase() + ")",
                                        "SEPARATE OUTPUTS" };
        const juce::String subs[] = { "Just your amp modeler (shown for the " + amp.box + " on the Amps & Modelers tab): one cue track, sent over "
                                          + (amp.usbMidi ? "USB or " : "") + "a MIDI cable.",
                                      "A second device on the amp modeler's MIDI Thru. Both tracks send to the same interface MIDI Out.",
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

// The test each device's ▶ Test sends (its tuner on and off, a first tile, a first preset, the Whammy's modes), and what to check.
// Shared by the MIDI strip on each page and the Connect your rig window.
class DeviceTests
{
public:
    DeviceTests (PedalCuesProcessor& p, juce::ValueTree s) : proc (p), state (std::move (s)) {}

    // The first tab's device: its tuner on and off, a first tile, preset 1 or a tap, whatever it has.
    juce::String testAmp()
        {
            juce::String status;
            cues::Cue c;
            const auto ch = state::ampChannel (state);
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
                return "Sent " + page->testMessage + " (a preset) to " + currentPortName() + " on channel "
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
                const auto cue = cues::whammy::effect (state::pedalChannel (state), mode, {}, (bool) state[IDs::whChords],
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
                status = ("Sent Oct Up, 5th Up, 2 Oct Up to " + currentPortName() + " on channel " + juce::String (state::pedalChannel (state))
                               + ". Did the Whammy's mode LED step along? If not, check the cable direction, the Whammy's channel, and MIDI Thru for the daisy chain.");
            return status;
        }

    juce::String currentPortName() const
    {
        if (PedalCuesProcessor::standaloneLayoutForScreenshots)
            return "USB MIDI Interface";
        if (! PedalCuesProcessor::isStandalone())
            return "this track's MIDI output";
        for (const auto& d : juce::MidiOutput::getAvailableDevices())
            if (d.identifier == proc.getDirectMidiOutput())
                return d.name;
        return "the MIDI port";
    }

private:
    bool isKemper() const          { return ampInfo (state).isKemper(); }
    juce::String ampShort() const  { return ampInfo (state).shortName; }

    PedalCuesProcessor& proc;
    juce::ValueTree state;
};


// Where to set the channel on the device itself (MIDI strip and Connect your rig).
juce::String ampChannelHint (const juce::ValueTree& state)
{
    const auto amp = ampInfo (state);
    return amp.isModeller() ? amp.channelHint
         : amp.isCustom() ? "Set the same channel on your " + amp.name + " (see its manual; not Omni)."
         : amp.isKemper() ? juce::String ("Set the same channel on your Kemper: System Settings > MIDI > MIDI Global Channel (not OMNI).")
                          : juce::String ("Set the same channel on your QC: Settings > MIDI Settings > MIDI Channel (not Omni).");
}

juce::String pedalChannelHint (const juce::ValueTree& state)
{
    const auto pedal = pedalInfo (state);
    const juce::String other (" With several devices, give each its own channel.");
    return pedal.page != nullptr ? pedal.page->channelHint + other
         : pedal.isCustom ? "Set the same channel on your " + pedal.name + " (see its manual)." + other
                          : "Set the same channel on your Whammy (see its manual)." + other;
}

// The MIDI strip above each device page: the device, its MIDI channel (set once, before building songs), a Test and
// How to connect. Until the channels are confirmed it says so in amber, with a Done button.
class MidiStrip final : public Page, private juce::Timer
{
public:
    MidiStrip (PedalCuesProcessor& p, bool pedalTab) : proc (p), state (p.state), pedal (pedalTab), tests (p, p.state), saved (p)
    {
        setComponentID (pedal ? "strip.pedal" : "strip.amp");
        name.setFont (font (14.0f, true));
        styleCaption (channelLabel, "MIDI CHANNEL");
        for (int ch = 1; ch <= 16; ++ch)
            channelBox.addItem ("Channel " + juce::String (ch), ch);
        channelBox.setComponentID (pedal ? "strip.pedal.channel" : "strip.amp.channel");
        channelBox.onChange = [this] { setChannel (channelBox.getSelectedId()); };
        hint.setFont (font (12.0f));
        hint.setJustificationType (juce::Justification::centredLeft);
        hint.setMinimumHorizontalScale (1.0f);

        doneButton.setColour (juce::TextButton::buttonColourId, accent);
        doneButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        doneButton.setTooltip ("Confirm that your devices use these channels. Clips keep the channel they were dragged with, so set them before building songs.");
        doneButton.onClick = [this] { state::setFlag (ui::channelsConfirmedFlag, true); refresh(); };
        testButton.setTooltip ("Sends a quick test to this device on its channel: a check that it gets MIDI.");
        testButton.onClick = [this] { runTest(); };
        connectButton.setComponentID ("strip.connect");
        connectButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        connectButton.setColour (juce::TextButton::textColourOffId, qcBlue);
        connectButton.setTooltip ("Cables, daisy chain or separate outputs, and your DAW tracks, with examples");
        connectButton.onClick = [this] { showConnectDialog (proc); };
        // The save status at the strip's right end, starting where its box starts so the gap before it is the real gap.
        for (auto* c : std::initializer_list<juce::Component*> { &name, &channelLabel, &channelBox, &hint, &doneButton, &testButton, &connectButton, &saved })
            addAndMakeVisible (c);
        refresh();
    }

    void refresh() override
    {
        const auto amp = ampInfo (state);
        const auto info = pedalInfo (state);
        colour = pedal ? info.colour : amp.colour;
        name.setText (pedal ? info.name : amp.name, juce::dontSendNotification);
        name.setColour (juce::Label::textColourId, colour.brighter (0.3f));
        channelBox.setSelectedId (pedal ? state::pedalChannel (state) : state::ampChannel (state), juce::dontSendNotification);
        const auto confirmed = state::getFlag (ui::channelsConfirmedFlag);
        doneButton.setVisible (! confirmed);
        if (! isTimerRunning())
        {
            hint.setColour (juce::Label::textColourId, confirmed ? dim : accent);
            hint.setText (confirmed ? (pedal ? pedalChannelHint (state) : ampChannelHint (state))
                                    : "Set this to the channel your " + (pedal ? info.shortName : amp.shortName)
                                      + " uses, then click Done. Every clip keeps the channel it was dragged with.",
                          juce::dontSendNotification);
            hint.setTooltip (pedal ? pedalChannelHint (state) : ampChannelHint (state));
        }
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (14.0f, 4.0f);
        g.setColour (surface);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (colour);
        g.fillRoundedRectangle (b.removeFromLeft (4.0f), 2.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 4).reduced (14, 6);
        name.setBounds (r.removeFromLeft (juce::jmin (220, juce::GlyphArrangement::getStringWidthInt (name.getFont(), name.getText()) + 16)));
        channelLabel.setBounds (r.removeFromLeft (96));
        channelBox.setBounds (r.removeFromLeft (122).withSizeKeepingCentre (122, 28));
        r.removeFromLeft (12);
        const auto savedWidth = proc.autoSave.writesToDisk() ? 78 : 118;
        saved.setBounds (r.removeFromRight (savedWidth).withSizeKeepingCentre (savedWidth, 20));

        // [hint]  gap  [Done] [Test] [How to connect >]  gap  [Saved]: the buttons sit midway between the end of the
        // hint text and Saved; a long hint wraps to two lines and keeps at least 16 px either side.
        const auto connectWidth = juce::GlyphArrangement::getStringWidthInt (font (15.0f, true), connectButton.getButtonText()) + 20;
        const auto groupWidth = (doneButton.isVisible() ? 84 + 10 : 0) + 84 + 6 + connectWidth;
        const auto natural = juce::GlyphArrangement::getStringWidthInt (hint.getFont(), hint.getText())
                           + hint.getBorderSize().getLeftAndRight() + 4;
        const auto hintWidth = juce::jmax (0, juce::jmin (natural, r.getWidth() - groupWidth - 32));
        const auto gap = juce::jmax (16, (r.getWidth() - hintWidth - groupWidth) / 2);
        hint.setBounds (r.removeFromLeft (hintWidth));
        r.removeFromLeft (gap);
        if (doneButton.isVisible())
        {
            doneButton.setBounds (r.removeFromLeft (84).withSizeKeepingCentre (84, 28));
            r.removeFromLeft (10);
        }
        testButton.setBounds (r.removeFromLeft (84).withSizeKeepingCentre (84, 28));
        r.removeFromLeft (6);
        connectButton.setBounds (r.removeFromLeft (connectWidth));
    }

private:
    void setChannel (int ch)
    {
        auto root = state;
        if (pedal)
            state::setPedalChannel (root, ch);
        else
            state::setAmpChannel (root, ch);
    }

    void runTest()
    {
        juce::String status;
        if (PedalCuesProcessor::isStandalone() && proc.getDirectMidiOutput().isEmpty())
            status = "Pick the MIDI port your devices are on first: How to connect, or Options > MIDI Output.";
        else
            status = pedal ? tests.testPedal() : tests.testAmp();
        hint.setColour (juce::Label::textColourId, text);
        hint.setText (status, juce::dontSendNotification);
        hint.setTooltip (status);
        startTimer (12000);   // then back to the channel hint
    }

    void timerCallback() override
    {
        stopTimer();
        refresh();
    }

    PedalCuesProcessor& proc;
    juce::ValueTree state;
    const bool pedal;
    DeviceTests tests;
    juce::Colour colour;
    juce::Label name, channelLabel, hint;
    juce::ComboBox channelBox;
    juce::TextButton doneButton { "Done" }, testButton { juce::CharPointer_UTF8 ("\xe2\x96\xb6  Test") }, connectButton { "How to connect >" };
    SaveIndicator saved;
};

class SettingsPage final : public Page, private juce::Timer
{
public:
    explicit SettingsPage (PedalCuesProcessor& p)
        : proc (p), state (p.state)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &tracksSection })
            addAndMakeVisible (c);
        addChildComponent (testSection);

        // One row per tab: the device (opens that tab's list), its channel and, in the standalone app, a Test tick box.

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
            results.add (testAmp());
            if (! pedalInfo (state).isNone)
                results.add (testPedal());
            setTestStatus (results.joinIntoString ("\n\n"));
        };
        testButton.setColour (juce::TextButton::buttonColourId, accent);
        testButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);

        // The channels and Done live in each page's MIDI strip now: this window shows the devices and how to connect them.
        for (auto* c : std::initializer_list<juce::Component*> { &tracksSteps, &viaQcButton,
                                                                 &viaInterfaceButton, &wiringLabel, &wiringLink, &dawHint, &dawLabel, &dawBox })
            addAndMakeVisible (c);
        for (auto* c : std::initializer_list<juce::Component*> { &testOutBox, &testButton, &testHint })
            addChildComponent (c);

        const auto standalone = PedalCuesProcessor::isStandalone();
        for (auto* c : std::initializer_list<juce::Component*> { &testSection, &testOutBox, &testButton, &testHint })
            c->setVisible (standalone);

        setSetupMode (! state::getFlag ("setupViaQcChain"));
        refresh();
    }

    void refresh() override
    {
        const auto pedal = pedalInfo (state);
        tracksSection.hint = pedal.isNone ? "one cue track, once" : "a cue track per device, once";
        tracksSection.repaint();

        setSetupMode (! state::getFlag ("setupViaQcChain"));
        refreshTestDevices();
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }   // its own window now

    juce::String testAmp()   { return tests.testAmp(); }
    juce::String testPedal() { return tests.testPedal(); }

    bool isKemper() const          { return ampInfo (state).isKemper(); }
    juce::String ampShort() const  { return ampInfo (state).shortName; }
    juce::String pedalShort() const { return pedalInfo (state).shortName; }

    // Standalone: a port plugged in while the window is open shows up by itself.
    void timerCallback() override
    {
        juce::StringArray now, shown;
        for (const auto& d : juce::MidiOutput::getAvailableDevices())
            now.add (d.identifier);
        for (const auto& d : testDevices)
            shown.add (d.identifier);
        if (now != shown)
            refreshTestDevices();
    }

    void visibilityChanged() override
    {
        if (PedalCuesProcessor::isStandalone() && ! PedalCuesProcessor::standaloneLayoutForScreenshots)
        {
            if (isShowing())
                startTimer (2000);
            else
                stopTimer();
        }
        if (isVisible())
        {
            refreshTestDevices();
        }
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        // Standalone: the MIDI port and Test all in one row on top. Then how the devices connect and the DAW tracks.
        if (PedalCuesProcessor::isStandalone())
        {
            testSection.setBounds (r.removeFromTop (Section::headerHeight + 92));
            auto t = testSection.contentArea().reduced (6, 4);
            auto row = t.removeFromTop (32);
            testOutBox.setBounds (row.removeFromLeft (360));
            row.removeFromLeft (10);
            testButton.setBounds (row.removeFromLeft (160));
            t.removeFromTop (6);
            testHint.setBounds (t);
            r.removeFromTop (12);
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
        testButton.setColour (juce::TextButton::buttonColourId, hasPort ? accent : raised);   // clearly off until a port is picked
        testButton.setColour (juce::TextButton::textColourOffId, hasPort ? juce::Colours::black : dim);
        if (! hasPort)
            setTestStatus (testDevices.isEmpty() ? "No MIDI ports yet. Connect your audio interface (for its MIDI Out) or a device over USB: it appears here."
                                                 : "Pick the MIDI port your devices are on to test them.");
        else if (testHint.getText().isEmpty() || testHint.getText().startsWith ("Pick") || testHint.getText().startsWith ("No MIDI"))
            setTestStatus ("The app sends straight to this port, no DAW or audio needed. Click Test all (or Test at the top of a page), then use any tile's play button.");
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
        // The generic role, with the picked unit as the example (its USB rule is in the note below).
        viaQcButton.setButtonText ("One cable through the amp modeler\n(daisy chain, e.g. via the " + amp.shortName + "'s Thru"
                                   + (amp.usbToThru == 1 ? juce::String (")") : juce::String (": MIDI cable in, not USB)")));
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
                "Make a cue track for each device: insert PedalCues and set its MIDI output to that device's port, e.g. " + ampTrack + " on "
                    + ampPort + ". Add the next one the same way, e.g. " + pedalTrack + " on MIDI Out 2.",
                "Keep the original MIDI channels, and drag each tab's tiles onto its device's track."
            };
        }
        else
        {
            tracksSteps.steps = {
                "In your DAW, enable your interface's MIDI output.",
                "Make a cue track for each device, e.g. " + ampTrack + " and " + pedalTrack + ": insert PedalCues and set its MIDI output to the "
                    "interface MIDI Out (it goes into the " + amp.shortName + " and through its Thru to the next device). Add more the same way.",
                "Keep the original MIDI channels, and drag each tab's tiles onto its device's track."
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
    DeviceTests tests { proc, state };

    Section testSection   { "set.test", "Test your devices", "each on its own channel", ledGreen };
    Section tracksSection { "set.tracks", "Set up your DAW", "two cue tracks, once", qcBlue };

    juce::Label testHint;
    juce::ComboBox testOutBox;

    StepsList tracksSteps;
    juce::Label wiringLabel, dawHint, dawLabel;
    juce::Label chainNote;   // the daisy chain: what this unit does with USB MIDI at its Thru
    juce::ComboBox dawBox;
    juce::TextButton wiringLink { "Wiring guide: cables and diagrams >" };
    juce::TextButton viaQcButton { "Daisy chain via QC" };
    juce::TextButton viaInterfaceButton { "Separate outputs" };

    juce::Array<juce::MidiDeviceInfo> testDevices;
    juce::TextButton testButton { "Test all" };

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

std::unique_ptr<Page> makeMidiStrip (PedalCuesProcessor& p, bool pedalTab)
{
    return std::make_unique<MidiStrip> (p, pedalTab);
}

// How to connect (the MIDI strip, the ☰ / Help menu): the devices, how they're connected, the DAW tracks and, in the
// standalone app, the MIDI port. It used to be the MIDI Setup tab.
void showConnectDialog (PedalCuesProcessor& p)
{
    auto page = std::make_unique<SettingsPage> (p);
    page->setSize (1000, PedalCuesProcessor::isStandalone() ? 760 : 640);
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (page.release());
    o.dialogTitle = "Connect your rig";
    o.dialogBackgroundColour = background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = true;
    o.launchAsync();
}

std::unique_ptr<Page> makeSettingsPage (PedalCuesProcessor& p)
{
    return std::make_unique<SettingsPage> (p);
}
} // namespace ui
