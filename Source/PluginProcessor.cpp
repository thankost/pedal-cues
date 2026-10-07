#include "PluginProcessor.h"
#include "PluginEditor.h"

PedalCuesProcessor::PedalCuesProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    const auto lib = state::defaultLibraryFile();
    if (lib.existsAsFile())
        state::loadLibrary (state, lib);

    if (isStandalone())
    {
        setDirectMidiOutput (state::getSetting ("standaloneMidiOutput"));
        const auto bpm = state::getSetting ("standaloneBpm").getDoubleValue();
        hostBpm = bpm >= 20.0 ? bpm : 120.0;
    }
}

void PedalCuesProcessor::setManualBpm (double bpm)
{
    hostBpm = juce::jlimit (20.0, 300.0, bpm);
    state::setSetting ("standaloneBpm", juce::String (hostBpm.load(), 1));
}

void PedalCuesProcessor::setDirectMidiOutput (const juce::String& id)
{
    directOut.reset();
    directOutId = {};

    if (id.isNotEmpty())
        if ((directOut = juce::MidiOutput::openDevice (id)) != nullptr)
        {
            directOut->startBackgroundThread();
            directOutId = id;
        }

    state::setSetting ("standaloneMidiOutput", id);
}

void PedalCuesProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;
}

bool PedalCuesProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet().isDisabled() || layouts.getMainInputChannelSet() == out;
}

void PedalCuesProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Audio passes straight through; clear any outputs without a matching input.
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (auto* playHead = isStandalone() ? nullptr : getPlayHead())
        if (const auto position = playHead->getPosition())
            if (const auto bpm = position->getBpm())
                if (*bpm > 0.0)
                    hostBpm = *bpm;

    const auto numSamples = buffer.getNumSamples();
    const auto blockStart = sampleCounter.load();

    // Incoming MIDI (e.g. from dropped cue items) is left in the buffer, so it passes through.
    {
        const juce::ScopedTryLock lock (pendingLock);

        if (lock.isLocked())
        {
            for (auto it = pending.begin(); it != pending.end();)
            {
                if (it->sampleTime < blockStart + numSamples)
                {
                    const auto offset = (int) juce::jlimit<juce::int64> (0, numSamples - 1, it->sampleTime - blockStart);
                    midi.addEvent (it->message, offset);
                    it = pending.erase (it);
                }
                else
                {
                    ++it;
                }
            }
        }
    }

    sampleCounter = blockStart + numSamples;
}

void PedalCuesProcessor::preview (const cues::Cue& cue)
{
    if (directOut != nullptr)
    {
        // Timestamps in milliseconds: sendBlockOfMessages is told the "sample rate" is 1000.
        const auto msPerBeat = 60000.0 / juce::jmax (20.0, hostBpm.load());
        juce::MidiBuffer block;
        for (const auto& [beat, message] : cue.events)
            block.addEvent (message, juce::roundToInt (beat * msPerBeat));
        directOut->sendBlockOfMessages (block, juce::Time::getMillisecondCounterHiRes() + 2.0, 1000.0);
        return;
    }

    const auto samplesPerBeat = 60.0 / juce::jmax (20.0, hostBpm.load()) * currentSampleRate.load();
    const auto now = sampleCounter.load();

    const juce::ScopedLock lock (pendingLock);
    for (const auto& [beat, message] : cue.events)
        pending.push_back ({ now + (juce::int64) (beat * samplesPerBeat), message });
}

void PedalCuesProcessor::previewTimed (const std::vector<std::pair<double, juce::MidiMessage>>& events)
{
    if (directOut != nullptr)
    {
        juce::MidiBuffer block;
        for (const auto& [seconds, message] : events)
            block.addEvent (message, juce::roundToInt (seconds * 1000.0));
        directOut->sendBlockOfMessages (block, juce::Time::getMillisecondCounterHiRes() + 2.0, 1000.0);
        return;
    }

    const auto rate = currentSampleRate.load();
    const auto now = sampleCounter.load();
    const juce::ScopedLock lock (pendingLock);
    for (const auto& [seconds, message] : events)
        pending.push_back ({ now + (juce::int64) (seconds * rate), message });
}

void PedalCuesProcessor::stopPreview()
{
    if (directOut != nullptr)
        directOut->clearAllPendingMessages();
    const juce::ScopedLock lock (pendingLock);
    pending.clear();
}

void PedalCuesProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void PedalCuesProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto loaded = juce::ValueTree::fromXml (*xml);
        if (loaded.hasType (IDs::PedalCues))
        {
            state::sanitise (loaded);
            const juce::ScopedValueSetter<bool> opening (autoSave.loading, true);
            state.copyPropertiesAndChildrenFrom (loaded, nullptr);
        }
    }
}

juce::AudioProcessorEditor* PedalCuesProcessor::createEditor()
{
    return new PedalCuesEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PedalCuesProcessor();
}
