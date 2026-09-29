#pragma once

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

    double getHostBpm() const { return hostBpm.load(); }

    juce::ValueTree state { state::createDefault() };

private:
    struct Scheduled
    {
        juce::int64 sampleTime;
        juce::MidiMessage message;
    };

    juce::CriticalSection pendingLock;
    std::vector<Scheduled> pending;

    std::atomic<double> hostBpm { 120.0 };
    std::atomic<double> currentSampleRate { 44100.0 };
    std::atomic<juce::int64> sampleCounter { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PedalCuesProcessor)
};
