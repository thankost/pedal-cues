#include "EditorCommon.h"
#include "MidiNameSync.h"
#include "State.h"

#include <juce_audio_devices/juce_audio_devices.h>

#include <deque>

using namespace theme;

// "Read names from the unit (beta)": asks the unit for its preset names over MIDI SysEx, read-only (MidiNameSync.h).
// The plugin normally leaves MIDI ports to the host; this dialog opens the two ports picked here only while a read runs and
// closes them right after, the way the Quad Cortex sync opens USB.
namespace namesync
{
namespace
{
const juce::String outSetting ("nameSyncMidiOut"), inSetting ("nameSyncMidiIn");   // "identifier|name", per computer

// One read: owns the ports while it runs, sends the Reader's requests one at a time and feeds it the SysEx that comes back.
class Job final : public juce::Thread, private juce::MidiInputCallback
{
public:
    Job (const Target& t, bool scenes) : juce::Thread ("PedalCues name sync"), reader (t, scenes) {}

    ~Job() override { stop(); }

    // Opens both ports; an empty string on success, else what went wrong.
    juce::String open (const juce::MidiDeviceInfo& out, const juce::MidiDeviceInfo& in)
    {
        output = juce::MidiOutput::openDevice (out.identifier);
        if (output == nullptr)
            return "Couldn't open the MIDI Out \"" + out.name + "\". If another app (or PedalCues' own MIDI Output) is using it, "
                   "close that and try again.";
        input = juce::MidiInput::openDevice (in.identifier, this);
        if (input == nullptr)
        {
            output.reset();
            return "Couldn't open the MIDI In \"" + in.name + "\". If another app is using it, close that and try again.";
        }
        input->start();
        return {};
    }

    void stop()
    {
        signalThreadShouldExit();
        arrived.signal();
        stopThread (3000);
        if (input != nullptr)
            input->stop();
        input.reset();     // the ports close as soon as the read ends
        output.reset();
    }

    bool isDone() const { return done.load(); }

    // A copy for the window (the thread keeps working on the reader).
    Results results (int& doneCount, int& total, bool& gaveUp, int& endedAt, bool& anyAnswer, int& deviceId) const
    {
        const juce::ScopedLock sl (lock);
        doneCount = reader.done(); total = reader.total(); gaveUp = reader.gaveUp(); endedAt = reader.presetsEndedAt();
        anyAnswer = reader.anyAnswer(); deviceId = reader.deviceId();
        return reader.results();
    }

private:
    void run() override
    {
        for (;;)
        {
            juce::MidiMessage request;
            int timeout = 0;
            {
                const juce::ScopedLock sl (lock);
                if (reader.finished())
                    break;
                request = reader.current();
                timeout = reader.timeoutMs();
            }
            if (threadShouldExit())
                break;
            // The read-only rule: only the whitelisted read requests ever leave this dialog.
            if (! isAllowedRequest (request) || output == nullptr)
            {
                jassertfalse;
                break;
            }
            drain (nullptr);   // anything stale
            output->sendMessageNow (request);

            const auto deadline = juce::Time::getMillisecondCounter() + (juce::uint32) timeout;
            bool answered = false;
            while (! answered && ! threadShouldExit())
            {
                const auto now = juce::Time::getMillisecondCounter();
                if (now >= deadline)
                    break;
                arrived.wait ((int) (deadline - now));
                drain (&answered);
            }
            if (threadShouldExit())
                break;
            if (! answered)
            {
                const juce::ScopedLock sl (lock);
                reader.timedOut();
            }
            wait (Reader::gapMs);   // pacing between requests
        }
        done.store (true);
    }

    // Hands what arrived to the reader; sets *answered when it answered the current request.
    void drain (bool* answered)
    {
        for (;;)
        {
            std::vector<juce::uint8> msg;
            {
                const juce::ScopedLock sl (queueLock);
                if (queue.empty())
                    return;
                msg = std::move (queue.front());
                queue.pop_front();
            }
            const juce::ScopedLock sl (lock);
            const auto ok = reader.accept (msg.data(), (int) msg.size());
            if (answered != nullptr && ok)
                *answered = true;
        }
    }

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override
    {
        if (! m.isSysEx())
            return;
        {
            const juce::ScopedLock sl (queueLock);
            if (queue.size() < 64)
                queue.emplace_back (m.getRawData(), m.getRawData() + m.getRawDataSize());
        }
        arrived.signal();
    }

    Reader reader;
    juce::CriticalSection lock, queueLock;
    std::deque<std::vector<juce::uint8>> queue;
    juce::WaitableEvent arrived;
    std::unique_ptr<juce::MidiOutput> output;
    std::unique_ptr<juce::MidiInput> input;
    std::atomic<bool> done { false };
};

class NameSyncDialog final : public juce::Component, private juce::Timer
{
public:
    NameSyncDialog (juce::ValueTree s, Target t, juce::ValueTree unit)
        : state (std::move (s)), target (std::move (t)), customUnit (std::move (unit))
    {
        title.setText ("Read names from the " + target.deviceName + " (beta)", juce::dontSendNotification);
        title.setFont (font (16.0f, true));
        title.setColour (juce::Label::textColourId, theme::text);
        addAndMakeVisible (title);

        beta.setText ("Reads preset names over MIDI; nothing is changed on your unit. Not tested on hardware yet: tell us if it works.",
                      juce::dontSendNotification);
        beta.setFont (font (12.5f, true));
        beta.setColour (juce::Label::textColourId, accent);
        beta.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (beta);

        how.setText (howText(), juce::dontSendNotification);
        styleNote (how);
        addAndMakeVisible (how);

        for (auto* l : { &outLabel, &inLabel })
        {
            l->setFont (font (13.0f, true));
            l->setColour (juce::Label::textColourId, theme::text);
            addAndMakeVisible (*l);
        }
        outLabel.setText ("MIDI Out (to the unit)", juce::dontSendNotification);
        inLabel.setText ("MIDI In (from the unit)", juce::dontSendNotification);
        addAndMakeVisible (outBox);
        addAndMakeVisible (inBox);
        refreshPorts.onClick = [this] { fillPorts(); };
        addAndMakeVisible (refreshPorts);

        scenesToggle.setButtonText ("Also read the scene names of the loaded preset (shown here, not applied)");
        scenesToggle.setVisible (target.sceneNames);
        addChildComponent (scenesToggle);
        onlyNamedToggle.setButtonText ("Only presets with names");
        onlyNamedToggle.setToggleState (true, juce::dontSendNotification);
        addAndMakeVisible (onlyNamedToggle);
        replaceToggle.setButtonText ("Replace my preset list (otherwise add new presets and rename matching ones)");
        replaceToggle.setVisible (! target.customDevice);
        addChildComponent (replaceToggle);

        status.setFont (font (13.0f, true));
        status.setColour (juce::Label::textColourId, theme::text);
        status.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (status);
        addAndMakeVisible (progressBar);

        list.setMultiLine (true);
        list.setReadOnly (true);
        list.setScrollbarsShown (true);
        list.setCaretVisible (false);
        list.setFont (font (13.0f));
        list.setColour (juce::TextEditor::backgroundColourId, surface);
        list.setColour (juce::TextEditor::textColourId, theme::text);
        list.setColour (juce::TextEditor::outlineColourId, outline);
        addAndMakeVisible (list);

        readButton.setColour (juce::TextButton::buttonColourId, accent);
        readButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        readButton.onClick = [this] { running() ? cancelRead() : startRead(); };
        applyButton.onClick = [this] { apply(); };
        applyButton.setEnabled (false);
        closeButton.onClick = [this] { close(); };
        for (auto* b : { &readButton, &applyButton, &closeButton })
            addAndMakeVisible (*b);

        fillPorts();
        status.setText ("Pick the ports, then Read.", juce::dontSendNotification);
        setSize (640, 620);
    }

    ~NameSyncDialog() override
    {
        stopTimer();
        job.reset();   // stops the thread, then closes the ports
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20, 16);
        title.setBounds (r.removeFromTop (26));
        beta.setBounds (r.removeFromTop (34));
        how.setBounds (r.removeFromTop (48));
        r.removeFromTop (6);

        const auto portRow = [&] (juce::Label& l, juce::ComboBox& box, juce::Component* extra)
        {
            auto row = r.removeFromTop (28);
            l.setBounds (row.removeFromLeft (170));
            if (extra != nullptr)
            {
                extra->setBounds (row.removeFromRight (80));
                row.removeFromRight (8);
            }
            box.setBounds (row);
            r.removeFromTop (6);
        };
        portRow (outLabel, outBox, &refreshPorts);
        portRow (inLabel, inBox, nullptr);

        if (scenesToggle.isVisible())
            scenesToggle.setBounds (r.removeFromTop (26));
        onlyNamedToggle.setBounds (r.removeFromTop (26));
        if (replaceToggle.isVisible())
            replaceToggle.setBounds (r.removeFromTop (26));
        r.removeFromTop (6);

        auto buttons = r.removeFromBottom (34);
        closeButton.setBounds (buttons.removeFromRight (100));
        buttons.removeFromRight (10);
        applyButton.setBounds (buttons.removeFromRight (110));
        buttons.removeFromRight (10);
        readButton.setBounds (buttons.removeFromRight (110));
        r.removeFromBottom (10);

        status.setBounds (r.removeFromTop (40));
        progressBar.setBounds (r.removeFromTop (16));
        r.removeFromTop (8);
        list.setBounds (r);
    }

private:
    static void styleNote (juce::Label& l)
    {
        l.setFont (font (12.0f));
        l.setColour (juce::Label::textColourId, dim);
        l.setJustificationType (juce::Justification::topLeft);
    }

    juce::String howText() const
    {
        juce::String s ("The names come back on the unit's MIDI Out (or USB), so connect both ways: pick the port that goes to the unit "
                        "and the one it answers on. A preset at a time, it takes a minute or two. Nothing is loaded.");
        switch (target.protocol)
        {
            case Protocol::bossGt1000:
            case Protocol::boss500:   s << " Asks with device ID 17, then the broadcast ID if there's no answer."; break;
            case Protocol::fractalGen3:
                if (target.model == fractal::fm3Model)
                    s << " FM3: Fractal says not to control it over USB MIDI; use 5-pin cables if USB doesn't answer.";
                break;
            case Protocol::strymonGen1: break;
        }
        return s;
    }

    static juce::String remembered (const juce::String& setting) { return state::getSetting (setting); }

    static void select (juce::ComboBox& box, const juce::Array<juce::MidiDeviceInfo>& devices, const juce::String& saved)
    {
        const auto id = saved.upToFirstOccurrenceOf ("|", false, false), name = saved.fromFirstOccurrenceOf ("|", false, false);
        int byName = -1;
        for (int i = 0; i < devices.size(); ++i)
        {
            if (devices[i].identifier == id)
            {
                box.setSelectedItemIndex (i, juce::dontSendNotification);
                return;
            }
            if (byName < 0 && devices[i].name == name)
                byName = i;
        }
        box.setSelectedItemIndex (juce::jmax (0, byName), juce::dontSendNotification);
    }

    void fillPorts()
    {
        outs = juce::MidiOutput::getAvailableDevices();
        ins = juce::MidiInput::getAvailableDevices();
        outBox.clear (juce::dontSendNotification);
        inBox.clear (juce::dontSendNotification);
        for (int i = 0; i < outs.size(); ++i)
            outBox.addItem (outs[i].name, i + 1);
        for (int i = 0; i < ins.size(); ++i)
            inBox.addItem (ins[i].name, i + 1);
        outBox.setTextWhenNoChoicesAvailable ("No MIDI outputs");
        inBox.setTextWhenNoChoicesAvailable ("No MIDI inputs");
        select (outBox, outs, remembered (outSetting));
        select (inBox, ins, remembered (inSetting));
    }

    bool running() const { return job != nullptr && ! job->isDone(); }

    void startRead()
    {
        const auto o = outBox.getSelectedItemIndex(), i = inBox.getSelectedItemIndex();
        if (! juce::isPositiveAndBelow (o, outs.size()) || ! juce::isPositiveAndBelow (i, ins.size()))
        {
            status.setText ("Pick a MIDI Out and a MIDI In first (Refresh if you just plugged the unit in).", juce::dontSendNotification);
            return;
        }
        state::setSetting (outSetting, outs[o].identifier + "|" + outs[o].name);
        state::setSetting (inSetting, ins[i].identifier + "|" + ins[i].name);

        job.reset();
        job = std::make_unique<Job> (target, scenesToggle.getToggleState());
        const auto error = job->open (outs[o], ins[i]);
        if (error.isNotEmpty())
        {
            job.reset();
            status.setText (error, juce::dontSendNotification);
            return;
        }
        last = {};
        progress = 0.0;
        list.clear();
        status.setText ("Asking the " + target.deviceName + " for its preset names...", juce::dontSendNotification);
        readButton.setButtonText ("Cancel");
        applyButton.setEnabled (false);
        for (auto* c : std::initializer_list<juce::Component*> { &outBox, &inBox, &refreshPorts, &scenesToggle })
            c->setEnabled (false);
        job->startThread();
        startTimer (150);
    }

    void cancelRead()
    {
        cancelled = true;
        finishRead();
    }

    void timerCallback() override
    {
        if (job == nullptr)
            return;
        poll();
        if (job->isDone())
            finishRead();
    }

    void poll()
    {
        int doneCount = 0, total = 0, endedAt = -1, deviceId = 0;
        bool gaveUp = false, any = false;
        last = job->results (doneCount, total, gaveUp, endedAt, any, deviceId);
        lastGaveUp = gaveUp; lastEndedAt = endedAt; lastAny = any;
        progress = total > 0 ? (double) doneCount / total : 0.0;
        status.setText ("Reading " + juce::String (doneCount) + " of " + juce::String (total) + "... "
                        + juce::String ((int) last.presetNames.size()) + " names so far.", juce::dontSendNotification);
        showList();
    }

    void finishRead()
    {
        stopTimer();
        if (job != nullptr)
        {
            poll();
            job.reset();   // stops the thread and closes both ports
        }
        readButton.setButtonText ("Read again");
        for (auto* c : std::initializer_list<juce::Component*> { &outBox, &inBox, &refreshPorts, &scenesToggle })
            c->setEnabled (true);
        progress = 1.0;

        juce::String text;
        if (cancelled)
            text = "Stopped. " + juce::String ((int) last.presetNames.size()) + " names read; you can apply them.";
        else if (lastGaveUp || ! lastAny)
            text = "No answer from the " + target.deviceName + ". Check both cables (or USB), the ports picked here, and that the "
                   "unit is on. Nothing was changed.";
        else
            text = "Done: " + juce::String ((int) last.presetNames.size()) + " names read"
                   + (lastEndedAt >= 0 ? " (no answer from " + unitLabel (target, lastEndedAt) + " on, so that's where its presets end)" : juce::String())
                   + ". Check them, then Apply.";
        if (last.numberMismatches > 0 || last.checksumMismatches > 0)
            text << " For the report: " << last.numberMismatches << " replies numbered differently, " << last.checksumMismatches
                 << " with an unexpected checksum.";
        if (target.protocol == Protocol::bossGt1000 && ! last.presetNames.empty() && last.programMap.empty())
            text << " The PROGRAM MAP didn't come back, so Apply can't tell which PC loads which patch.";
        cancelled = false;
        status.setText (text, juce::dontSendNotification);
        applyButton.setEnabled (! last.presetNames.empty());
    }

    void showList()
    {
        juce::String s;
        if (last.hasSceneNames)
        {
            s << "Scenes of the loaded preset:\n";
            for (int i = 0; i < target.sceneCount; ++i)
                s << "  " << (i + 1) << "  " << last.sceneNames[(size_t) i] << "\n";
            s << "\n";
        }
        std::map<int, juce::String> pcFor;   // GT-1000: where each user patch sits in the PROGRAM MAP (its first PC)
        for (const auto& [key, value] : last.programMap)
            if (pcFor.count (value) == 0)
                pcFor[value] = "BANK" + juce::String (key / 128 + 1) + " PC#" + juce::String (key % 128 + 1);
        for (const auto& [index, name] : last.presetNames)
        {
            s << unitLabel (target, index).paddedRight (' ', 12) << (isPlaceholderName (name) ? juce::String ("(no name)") : name);
            if (target.protocol == Protocol::bossGt1000 && ! last.programMap.empty())
                s << "   " << (pcFor.count (index) > 0 ? pcFor[index] : juce::String ("(not in the PROGRAM MAP)"));
            s << "\n";
        }
        list.setText (s, false);
    }

    void apply()
    {
        Applied a;
        if (target.customDevice)
        {
            if (! customUnit.isValid())
            {
                status.setText ("Pick the " + target.deviceName + " device first.", juce::dontSendNotification);
                return;
            }
            a = applyToCustomUnit (customUnit, target, last, onlyNamedToggle.getToggleState());
        }
        else
        {
            a = applyToModeller (state, target, last, onlyNamedToggle.getToggleState(), replaceToggle.getToggleState());
        }
        status.setText ("Applied: " + juce::String (a.added) + (target.customDevice ? " tiles added, " : " presets added, ")
                            + juce::String (a.renamed) + " renamed. Use the menu's Save as default setup to keep them for new projects.",
                        juce::dontSendNotification);
    }

    void close()
    {
        stopTimer();
        job.reset();
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    }

    juce::ValueTree state;
    Target target;
    juce::ValueTree customUnit;
    std::unique_ptr<Job> job;
    Results last;
    bool lastGaveUp = false, lastAny = false, cancelled = false;
    int lastEndedAt = -1;
    double progress = 0.0;

    juce::Array<juce::MidiDeviceInfo> outs, ins;
    juce::Label title, beta, how, outLabel, inLabel, status;
    juce::ComboBox outBox, inBox;
    juce::TextButton refreshPorts { "Refresh" };
    juce::ToggleButton scenesToggle, onlyNamedToggle, replaceToggle;
    juce::ProgressBar progressBar { progress };
    juce::TextEditor list;
    juce::TextButton readButton { "Read" }, applyButton { "Apply" }, closeButton { "Close" };
};
} // namespace

void showMidiNameSync (juce::ValueTree state, const juce::String& profileOrTemplateId, juce::ValueTree customUnit)
{
    const auto target = targetFor (profileOrTemplateId);
    if (! target)
        return;
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new NameSyncDialog (std::move (state), *target, std::move (customUnit)));
    o.dialogTitle = "Read names from the unit";
    o.dialogBackgroundColour = background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}
} // namespace namesync
