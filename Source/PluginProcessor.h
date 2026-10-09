#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "CueModel.h"
#include "SongAudio.h"
#include "State.h"

#include <atomic>
#include <vector>

class PedalCuesProcessor final : public juce::AudioProcessor
{
public:
    PedalCuesProcessor();
    ~PedalCuesProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Sends a cue out of the plugin's MIDI output right now (tempo-scaled), for auditioning.
    void preview (const cues::Cue&);
    // Song Builder playback: messages at seconds from now (the song's own tempo map), and Stop clearing what's still queued.
    void previewTimed (const std::vector<std::pair<double, juce::MidiMessage>>& secondsAndMessages);
    void stopPreview();

    // Song Builder: the metronome and the backing track, mixed into the output (SongAudio.h).
    SongAudio songAudio;
    juce::int64 nowSample() const { return sampleCounter.load(); }

    double getHostBpm() const { return hostBpm.load(); }

    // Standalone app only: tiles are sent straight to this MIDI output on their own clock,
    // so no audio device is needed. Empty = none (falls back to the audio/MIDI settings).
    static bool isStandalone() { return standaloneLayoutForScreenshots || juce::JUCEApplicationBase::isStandaloneApp(); }

    // Screenshot tool only: render the standalone app's layout, with a demo MIDI port instead of the real ones.
    static inline bool standaloneLayoutForScreenshots = false;
    void setDirectMidiOutput (const juce::String& deviceIdentifier);
    juce::String getDirectMidiOutput() const { return directOutId; }

    // Standalone app only: there is no host tempo, so the user sets it (remembered between sessions).
    void setManualBpm (double bpm);

    juce::ValueTree state { state::createDefault() };

    // Automatic save: a moment after every change, the standalone app writes its state (saveNow, set by the app) and the
    // plugin tells the DAW its project changed (the DAW keeps it when the project is saved). The save icons read it.
    struct AutoSave final : private juce::ValueTree::Listener, private juce::Timer
    {
        explicit AutoSave (PedalCuesProcessor& p) : owner (p) { owner.state.addListener (this); }
        ~AutoSave() override { owner.state.removeListener (this); }

        std::function<void()> saveNow;   // standalone only
        bool isPending() const { return pending; }
        bool writesToDisk() const { return saveNow != nullptr; }
        bool loading = false;            // a project being opened isn't a change

    private:
        void changed() { if (loading) return; pending = true; startTimer (800); }
        // Browsing (the open song, preset or performance, a page's view, Shapes / Draw, Exp 1 / 2, the Song Builder's track
        // height) isn't an edit: it's remembered, but written with the next real change, so the DAW project isn't marked
        // as changed by looking around.
        static bool isViewOnly (const juce::Identifier& id)
        {
            for (const auto& v : { IDs::selectedSong, IDs::selectedPreset, IDs::selectedPerformance,
                                   IDs::mdView, IDs::fxView, IDs::cuExpressionView, IDs::kemperPedalsView, IDs::whDropTuneView,
                                   IDs::qcExpressionView, IDs::qcLooperView,
                                   IDs::mdDraw, IDs::fxDraw, IDs::cuDraw, IDs::kpDraw, IDs::sweepDraw, IDs::expDraw,
                                   IDs::expPedal, IDs::mdPedal, IDs::fxPedal, IDs::trackHeight })
                if (id == v)
                    return true;
            return false;
        }
        void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& id) override { if (! isViewOnly (id)) changed(); }
        void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override { changed(); }
        void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { changed(); }
        void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override { changed(); }
        void timerCallback() override
        {
            stopTimer();
            if (saveNow)
                saveNow();
            else
                owner.updateHostDisplay (juce::AudioProcessorListener::ChangeDetails().withNonParameterStateChanged (true));
            pending = false;
        }

        PedalCuesProcessor& owner;
        bool pending = false;
    };
    AutoSave autoSave { *this };

    // A move tile's loop (the loop button next to its play button): the move again and again, in time at the current
    // tempo, until stopped, so it can be tried with both hands on the guitar. One loop at a time. Each pass asks the
    // tile for its move (edits are heard on the next pass); when the tile is gone, the last move keeps looping.
    struct MoveLoop final : public juce::ChangeBroadcaster, private juce::Timer
    {
        explicit MoveLoop (PedalCuesProcessor& p) : owner (p) {}
        ~MoveLoop() override { stopTimer(); }

        void start (const juce::String& key, juce::Component* tile, std::function<cues::Cue()> make)
        {
            owner.stopPreview();
            current = key;
            maker = std::move (make);
            provider = tile;
            last = maker ? maker() : cues::Cue();
            running = true;
            nextAt = juce::Time::getMillisecondCounterHiRes() + 30.0;
            startTimerHz (40);
            sendChangeMessage();
        }
        void stop()
        {
            if (! running)
                return;
            running = false;
            stopTimer();
            owner.stopPreview();
            sendChangeMessage();
        }
        bool isLooping (const juce::String& key) const { return running && key.isNotEmpty() && key == current; }
        // A tile rebuilt with the same key takes over (its move is the current one).
        void adopt (const juce::String& key, juce::Component* tile, std::function<cues::Cue()> make)
        {
            if (isLooping (key)) { provider = tile; maker = std::move (make); }
        }

    private:
        void timerCallback() override
        {
            const auto now = juce::Time::getMillisecondCounterHiRes();
            if (now < nextAt - 250.0)   // the next pass is queued a moment before it's due
                return;
            if (provider != nullptr && maker)
                last = maker();
            const auto bpm = juce::jlimit (10.0, 960.0, owner.getHostBpm());
            const auto secondsPerBeat = 60.0 / bpm;
            const auto lead = juce::jmax (0.0, (nextAt - now) / 1000.0);
            std::vector<std::pair<double, juce::MidiMessage>> events;
            for (const auto& [beat, message] : last.events)
                events.emplace_back (lead + beat * secondsPerBeat, message);
            owner.previewTimed (events);
            nextAt += juce::jmax (0.25, last.lengthBeats) * secondsPerBeat * 1000.0;
        }

        PedalCuesProcessor& owner;
        juce::String current;
        std::function<cues::Cue()> maker;
        juce::Component::SafePointer<juce::Component> provider;
        cues::Cue last;
        double nextAt = 0.0;
        bool running = false;
    };
    MoveLoop moveLoop { *this };

private:
    struct Scheduled
    {
        juce::int64 sampleTime;
        juce::MidiMessage message;
    };

    std::unique_ptr<juce::MidiOutput> directOut;
    juce::String directOutId;

    juce::CriticalSection pendingLock;
    std::vector<Scheduled> pending;

    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<juce::int64> sampleCounter { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalCuesProcessor)
};
