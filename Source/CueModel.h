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
        constexpr int setlist = 32; // setlist select: value = setlist number as the QC shows it, 0 = Factory Presets
        constexpr int stompA  = 35; // 35..42 = footswitch A..H, 0-63 off / 64-127 on
        constexpr int scene   = 43; // 0..7 = scene A..H
        constexpr int tuner   = 45; // 0-63 off / 64-127 on
        constexpr int gigMode = 47; // 0 preset, 1 stomp, 2 scene (tested on a QC)
        constexpr int exp1    = 1;  // expression pedal 1, 0-127 (heel to toe)
        constexpr int exp2    = 2;  // expression pedal 2
    }

    juce::String letter (int zeroBasedIndex);
    juce::String location (int setlist, int bank, int slot); // e.g. "SL1 | 3B"; setlist 0 = "Factory | 3B"

    void addScene      (Cue&, int channel, int scene, double beat);
    void addPresetLoad (Cue&, int channel, int setlist, int bank, int slot, bool sendSetlist, double beat);

    Cue scene   (int channel, int scene, const juce::String& label);
    Cue preset  (int channel, int setlist, int bank, int slot, bool sendSetlist, const juce::String& label);
    Cue tuner   (int channel, bool on);
    Cue stomp   (int channel, int footswitch, bool on, const juce::String& label);
    Cue gigMode (int channel, int mode);

    // Expression pedal automation (CC#1 / CC#2). Moves whatever is assigned to Expression 1 or 2 on the QC.
    // 'pedal' is 1 or 2. 'curve' is an exponent (1 = linear).
    enum class ExpShape { swellIn, fadeOut, riseFall, slowRise, wahRhythm, riseToBar, toe, heel };
    constexpr int numExpShapes = 8;

    juce::String expShapeName        (ExpShape);
    juce::String expShapeDescription (ExpShape);

    Cue expressionMove  (int channel, int pedal, ExpShape, double lengthBeats, double curve, bool resetToHeel);
    Cue expressionDrawn (int channel, int pedal, const std::vector<float>& points, double lengthBeats, bool resetToHeel,
                         const juce::String& name = {});
    Cue expressionSet   (int channel, int pedal, float position);   // 0 = heel, 1 = toe

    // The same cue, but it loads a preset first and starts 1/16 note later ("Load 1A first").
    Cue withPresetFirst (const Cue&, int channel, int setlist, int bank, int slot, bool sendSetlist,
                         const juce::String& presetName);
}

//==============================================================================
// DigiTech Whammy V (5th gen) and Whammy DT. The DT's Whammy side uses the V's Classic numbers (1-42);
// on the DT, 43-78 are its Drop Tune (Shift Up / Shift Down) effects, so the V's Chords range doesn't apply.
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

    // Whammy DT Drop Tune (Whammy DT manual, page 13). 'step' 0-6 = 1-7 semitones, 7 = Oct, 8 = Oct + Dry.
    // Shift Up: 43-51 active / 61-69 bypassed. Shift Down: 60-52 active / 78-70 bypassed.
    constexpr int numShifts = 9;
    juce::String shiftName      (int step);              // "1" .. "7", "Oct", "Oct + Dry"
    juce::String shiftDescription (bool up, int step);   // for tooltips
    int          dropTuneProgram (bool up, int step, bool bypass);   // 1-based, as printed in the manual
    Cue          dropTune (int channel, bool up, int step, bool bypass, int pcNumberBase);

    enum class Shape { rampUp, rampDown, swell, dive, trill, bendToBar, toe, heel };
    constexpr int numShapes = 8;

    juce::String shapeName        (Shape);
    juce::String shapeDescription (Shape);

    // CC11 treadle automation. 'curve' is an exponent (1 = linear).
    Cue sweep (int channel, Shape, double lengthBeats, double curve, bool resetToHeel);

    // Freehand treadle move. 'points' are evenly spaced treadle positions (0 = heel, 1 = toe)
    // spread across the length; values in between are interpolated.
    constexpr int drawPoints = 64;
    // 'name' is the saved drawing's name, if any ("Whammy Big Bend 1 bar"; otherwise "Whammy Drawn 1 bar").
    Cue drawn (int channel, const std::vector<float>& points, double lengthBeats, bool resetToHeel,
               const juce::String& name = {});

    std::vector<float> defaultDrawing();
    juce::String       encodeDrawing (const std::vector<float>&);
    std::vector<float> decodeDrawing (const juce::String&);   // always drawPoints values
}

} // namespace cues
