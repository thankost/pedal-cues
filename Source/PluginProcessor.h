#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "CueModel.h"
#include "State.h"

#include <atomic>
#include <vector>

class PedalCuesProcessor final : public juce::AudioProcessor
{
public:
    PedalCuesProcessor();
    ~PedalCuesProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
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
        void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { changed(); }
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
