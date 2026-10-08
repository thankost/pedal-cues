#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include "Songs.h"

#include <atomic>
#include <vector>

// Song Builder (beta) audio, mixed into the plugin's / app's output while a song plays: the metronome (synthesised
// clicks, see songs::clickSample) and one backing track (MP3, WAV, AIFF, FLAC, Ogg) read from disk in the background.
// Scheduling happens on the message thread, rendering on the audio thread. Times are seconds from start()'s `now`.
class SongAudio
{
public:
    SongAudio();
    ~SongAudio();

    void prepare (double sampleRate, int maxBlockSize);
    void release();

    // Message thread.
    bool loadBacking (const juce::File&);   // false when the file can't be read (it stays unloaded)
    void clearBacking();
    juce::File backingFile() const { return loaded; }
    juce::AudioFormatManager& formats() { return formatManager; }

    // Starts at `now` (the processor's sample counter). route: 0 both on both sides, 1 click left / backing right,
    // 2 click right / backing left. Then add clicks and backing starts (a loop adds more as it goes).
    // countIn: always sounds; the others follow setMetronome.
    struct Click { double seconds; bool accent; bool sub = false; bool countIn = false; };
    void start (juce::int64 now, const songs::ClickSound&, int route, float backingGain);
    // Live while playing: the click's sound / accent / volume, the routing, the metronome, the backing track's volume
    // and mute. Message thread.
    void setSound (const songs::ClickSound&);
    void setRoute (int newRoute) { route = juce::jlimit (0, 2, newRoute); }
    void setBackingGain (float gain) { backingGain = juce::jlimit (0.0f, 1.0f, gain); }
    void setMetronome (bool on) { metronomeOn = on; }
    void setBackingMuted (bool muted) { backingMuted = muted; }
    void addClicks (const std::vector<Click>& clicks, double offsetSeconds = 0.0);
    void addBacking (double atSeconds, double fileSeconds);   // play the backing track from fileSeconds at that time
    void stop();

    // Audio thread: adds into the buffer's channels.
    void render (juce::AudioBuffer<float>&, int numSamples, juce::int64 blockStart);

    // The backing track as an exported file needs it: `seconds` of audio where bar 1 of the song is `leadSeconds` in, the
    // file's `fileSecondsAtBar1` there (negative: it starts after bar 1), resampled to `sampleRate`. Message thread.
    juce::AudioBuffer<float> renderBacking (const juce::File&, double fileSecondsAtBar1, double leadSeconds, double seconds,
                                            double sampleRate, float gain);

private:
    struct Scheduled { juce::int64 at; bool accent, sub, countIn; };
    struct Active { int age; bool accent, sub; };
    struct BackingStart { juce::int64 at; double fileSeconds; };

    juce::AudioFormatManager formatManager;
    juce::TimeSliceThread readAhead { "PedalCues song audio" };
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transport;
    juce::File loaded;

    juce::SpinLock lock;                    // guards scheduled, backingStarts, sound
    std::vector<Scheduled> scheduled;
    std::vector<BackingStart> backingStarts;
    std::vector<Active> active;             // audio thread only
    songs::ClickSound renderSound;          // audio thread only: a copy of `sound`
    songs::ClickSound sound;
    juce::Random noise;
    std::atomic<juce::int64> base { 0 };
    std::atomic<int> route { 0 };
    std::atomic<float> backingGain { 0.8f };
    std::atomic<bool> metronomeOn { true }, backingMuted { false };
    juce::AudioBuffer<float> scratch;
    double sampleRate = 44100.0;
    int blockSize = 512;
};
