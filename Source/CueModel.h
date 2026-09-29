#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>

#include <utility>
#include <vector>

namespace cues
{

// A named bundle of MIDI events laid out in beats (quarter notes), tempo independent.
struct Cue
{
    juce::String name;
    double lengthBeats = 1.0;
    std::vector<std::pair<double, juce::MidiMessage>> events;

    void add (double beat, const juce::MidiMessage& m) { events.emplace_back (beat, m); }
};

// Writes the cue as a Standard MIDI File (named after the cue) into a temp folder,
// ready to be dragged onto a DAW timeline.
juce::File writeMidiFile (const Cue& cue, double bpm);

juce::String formatBeats (double beats);

//==============================================================================
// Neural DSP Quad Cortex / QC Mini (documented MIDI implementation)
namespace qc
{
    namespace cc
    {
        constexpr int bankMsb = 0;  // preset page: 0 = presets 1..128, 1 = 129..256
        constexpr int setlist = 32; // setlist select (value = setlist number - 1)
        constexpr int stompA  = 35; // 35..42 = footswitch A..H, 0-63 off / 64-127 on
        constexpr int scene   = 43; // 0..7 = scene A..H
        constexpr int tuner   = 45; // 0-63 off / 64-127 on
        constexpr int gigMode = 47; // 0 preset, 1 scene, 2 stomp
    }

    juce::String letter (int zeroBasedIndex);
    juce::String location (int setlist, int bank, int slot); // e.g. "SL1 | 3B"

    void addScene      (Cue&, int channel, int scene, double beat);
    void addPresetLoad (Cue&, int channel, int setlist, int bank, int slot, bool sendSetlist, double beat);

    Cue scene   (int channel, int scene, const juce::String& label);
    Cue preset  (int channel, int setlist, int bank, int slot, bool sendSetlist, const juce::String& label);
    Cue tuner   (int channel, bool on);
    Cue stomp   (int channel, int footswitch, bool on, const juce::String& label);
    Cue gigMode (int channel, int mode);
}

//==============================================================================
// DigiTech Whammy V (5th gen)
namespace whammy
{
    constexpr int numEffects = 21;

    enum class Category { whammy, detune, harmony };

    juce::String defaultName (int index);
    Category     category    (int index);
    juce::Colour colour      (int index);

    // Program number as printed in the Whammy manual (1-based):
    // Classic 1-21 on / 22-42 bypass, Chords 43-63 on / 64-84 bypass.
    int programNumber (int index, bool chords, bool bypass);

    Cue effect (int channel, int effectIndex, const juce::String& name, bool chords, bool bypass,
                int pcNumberBase, bool heelFirst);

    enum class Shape { rampUp, rampDown, swell, dive, trill, bendToBar, toe, heel };
    constexpr int numShapes = 8;

    juce::String shapeName        (Shape);
    juce::String shapeDescription (Shape);

    // CC11 treadle automation. 'curve' is an exponent (1 = linear).
    Cue sweep (int channel, Shape, double lengthBeats, double curve, bool resetToHeel);
}

} // namespace cues
