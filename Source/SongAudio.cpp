#include "SongAudio.h"

#include <cmath>

SongAudio::SongAudio()
{
    formatManager.registerBasicFormats();   // WAV, AIFF, FLAC, Ogg, MP3 (JUCE_USE_MP3AUDIOFORMAT), and the OS's own
    readAhead.startThread();
}

SongAudio::~SongAudio()
{
    transport.setSource (nullptr);
    readAhead.stopThread (2000);
}

void SongAudio::prepare (double rate, int maxBlockSize)
{
    sampleRate = rate > 0.0 ? rate : 44100.0;
    blockSize = juce::jmax (64, maxBlockSize);
    scratch.setSize (2, juce::jmax (blockSize, 4096));
    transport.prepareToPlay (blockSize, sampleRate);
}

void SongAudio::release()
{
    transport.releaseResources();
}

bool SongAudio::loadBacking (const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return false;
    const auto rate = reader->sampleRate;
    auto source = std::make_unique<juce::AudioFormatReaderSource> (reader.release(), true);
    transport.stop();
    transport.setSource (source.get(), 32768, &readAhead, rate, 2);
    readerSource = std::move (source);
    loaded = file;
    return true;
}

void SongAudio::clearBacking()
{
    transport.stop();
    transport.setSource (nullptr);
    readerSource.reset();
    loaded = juce::File();
}

void SongAudio::start (juce::int64 now, const songs::ClickSound& clickSound, int clickRoute, float gain)
{
    stop();
    base = now;
    route = juce::jlimit (0, 2, clickRoute);
    backingGain = juce::jlimit (0.0f, 1.0f, gain);
    const juce::SpinLock::ScopedLockType sl (lock);
    sound = clickSound;
}

void SongAudio::addClicks (const std::vector<Click>& clicks, double offsetSeconds)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    for (const auto& c : clicks)
        scheduled.push_back ({ base + (juce::int64) ((c.seconds + offsetSeconds) * sampleRate), c.accent, c.sub, c.countIn });
}

void SongAudio::setSound (const songs::ClickSound& clickSound)
{
    const juce::SpinLock::ScopedLockType sl (lock);
    sound = clickSound;
}

void SongAudio::addBacking (double atSeconds, double fileSeconds)
{
    if (readerSource == nullptr)
        return;
    if (fileSeconds < 0.0)   // the audio starts after this point
    {
        atSeconds -= fileSeconds;
        fileSeconds = 0.0;
    }
    if (fileSeconds >= transport.getLengthInSeconds())
        return;
    const juce::SpinLock::ScopedLockType sl (lock);
    backingStarts.push_back ({ base + (juce::int64) (atSeconds * sampleRate), fileSeconds });
}

void SongAudio::stop()
{
    transport.stop();
    const juce::SpinLock::ScopedLockType sl (lock);
    scheduled.clear();
    backingStarts.clear();
}

void SongAudio::render (juce::AudioBuffer<float>& buffer, int numSamples, juce::int64 blockStart)
{
    const auto channels = buffer.getNumChannels();
    if (channels == 0 || numSamples <= 0)
        return;

    // What starts in this block: clicks (at an offset inside it) and backing-track starts (a loop's restart).
    auto& clickSound = renderSound;   // the last sound we could read: a busy lock keeps it for this block
    bool restart = false;
    double restartAt = 0.0;
    {
        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (sl.isLocked())
        {
            clickSound = sound;
            for (auto it = scheduled.begin(); it != scheduled.end();)
                if (it->at < blockStart + numSamples)
                {
                    if (it->countIn || metronomeOn.load())   // the metronome switched off: skip it, keep the timing
                        active.push_back ({ (int) juce::jmin<juce::int64> (0, blockStart - it->at), it->accent, it->sub });
                    it = scheduled.erase (it);
                }
                else
                    ++it;
            for (auto it = backingStarts.begin(); it != backingStarts.end();)
                if (it->at < blockStart + numSamples)
                {
                    restart = true;
                    restartAt = it->fileSeconds;
                    it = backingStarts.erase (it);
                }
                else
                    ++it;
        }
    }

    // Click: both sides, or one side with the backing track on the other.
    const auto r = route.load();
    const auto clickOn = [r, channels] (int ch) { return channels < 2 || r == 0 || (r == 1 ? ch == 0 : ch == 1); };
    const auto backingOn = [r, channels] (int ch) { return channels < 2 || r == 0 || (r == 1 ? ch == 1 : ch == 0); };

    const auto clickSamples = (int) (songs::clickLength (clickSound) * sampleRate);
    for (auto it = active.begin(); it != active.end();)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const auto age = it->age + i;
            if (age < 0)
                continue;   // starts later in this block
            if (age >= clickSamples)
                break;
            const auto v = songs::clickSample (clickSound, it->accent, age / sampleRate, &noise, it->sub);
            for (int ch = 0; ch < channels; ++ch)
                if (clickOn (ch))
                    buffer.addSample (ch, i, v);
        }
        it->age += numSamples;
        it = it->age >= clickSamples ? active.erase (it) : it + 1;
    }

    if (restart)
    {
        transport.setPosition (restartAt);
        if (! transport.isPlaying())
            transport.start();
    }
    if (transport.isPlaying())
    {
        const auto gain = backingMuted.load() ? 0.0f : backingGain.load();   // muted: keeps playing silently, in time
        for (int done = 0; done < numSamples;)
        {
            const auto n = juce::jmin (numSamples - done, scratch.getNumSamples());
            juce::AudioSourceChannelInfo info (&scratch, 0, n);
            transport.getNextAudioBlock (info);
            for (int ch = 0; ch < channels; ++ch)
                if (backingOn (ch))
                    buffer.addFrom (ch, done, scratch, juce::jmin (ch, scratch.getNumChannels() - 1), 0, n, gain);
            done += n;
        }
    }
}

juce::AudioBuffer<float> SongAudio::renderBacking (const juce::File& file, double fileSecondsAtBar1, double leadSeconds, double seconds,
                                                   double rate, float gain)
{
    juce::AudioBuffer<float> out (2, juce::jmax (1, (int) std::ceil (seconds * rate)));
    out.clear();
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr)
        return out;

    // The file's time at the export's first sample, and where the file starts in the export.
    const auto fileStart = fileSecondsAtBar1 - leadSeconds;   // negative: silence first
    juce::AudioFormatReaderSource source (reader.get(), false);
    juce::ResamplingAudioSource resampler (&source, false, 2);
    resampler.setResamplingRatio (reader->sampleRate / rate);
    resampler.prepareToPlay (8192, rate);
    const auto skipOut = fileStart < 0.0 ? (int) std::round (-fileStart * rate) : 0;
    source.setNextReadPosition (fileStart > 0.0 ? (juce::int64) std::round (fileStart * reader->sampleRate) : 0);
    for (int pos = skipOut; pos < out.getNumSamples();)
    {
        const auto n = juce::jmin (8192, out.getNumSamples() - pos);
        juce::AudioSourceChannelInfo info (&out, pos, n);
        resampler.getNextAudioBlock (info);
        pos += n;
    }
    resampler.releaseResources();
    out.applyGain (gain);
    if (reader->numChannels == 1)
        out.copyFrom (1, 0, out, 0, 0, out.getNumSamples());
    return out;
}
