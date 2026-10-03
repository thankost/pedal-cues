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

// What writeMidiFile repeats 1/16 later when every event sits on the first tick (Ableton needs a clip length):
// never a second Program Change, which could reload the preset.
juce::MidiMessage lengthPadding (const Cue& cue);

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
        constexpr int gigView = 46; // Gig View screen: 0-63 close / 64-127 open
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
    Cue gigView (int channel, bool open);   // open or close the Gig View screen

    // Expression pedal automation (CC#1 / CC#2). Moves whatever is assigned to Expression 1 or 2 on the QC.
    // 'pedal' is 1 or 2. 'curve' is an exponent (1 = linear).
    enum class ExpShape { swellIn, fadeOut, riseFall, slowRise, wahRhythm, riseToBar, toe, heel };
    constexpr int numExpShapes = 8;

    juce::String expShapeName        (ExpShape);
    juce::String expShapeDescription (ExpShape);

    Cue expressionMove  (int channel, int pedal, ExpShape, double lengthBeats, double curve, bool resetToHeel);

    // The same moves on any controller; 'name' prefixes the clip name ("QC Exp 1 ", "Kemper Wah ").
    Cue shapedMove (const juce::String& name, int channel, int controller, ExpShape, double lengthBeats, double curve,
                    bool resetToHeel);
    Cue drawnMove  (const juce::String& name, int channel, int controller, const std::vector<float>& points,
                    double lengthBeats, bool resetToHeel);
    Cue expressionDrawn (int channel, int pedal, const std::vector<float>& points, double lengthBeats, bool resetToHeel,
                         const juce::String& name = {});
    Cue expressionSet   (int channel, int pedal, float position);   // 0 = heel, 1 = toe

    // The same cue, but it loads a preset first and starts 1/16 note later ("Load 1A first").
    Cue withPresetFirst (const Cue&, int channel, int setlist, int bank, int slot, bool sendSetlist,
                         const juce::String& presetName);
}

//==============================================================================
// Kemper Profiler (Head, PowerHead, Toaster, Rack, PowerRack, Stage) and Profiler Player, in Performance Mode.
// Source: Kemper "PROFILER MIDI Parameter Documentation" (OS 11) and the main manual. Switches are 1 = on / 0 = off.
namespace kemper
{
    enum class Unit { profiler, player };

    namespace cc
    {
        constexpr int wah = 1, pitch = 4, volume = 7, morphPedal = 11;   // continuous pedals
        constexpr int bankLsb = 32;     // Performance Mode: bank select LSB (0-4) before the Program Change
        constexpr int tap = 30, tuner = 31, rotary = 33, infinity = 34, freeze = 35;
        constexpr int slot1 = 50;       // 50-54: load slot 1-5 of the current Performance
        constexpr int morph = 80;       // 1: ramp to the morph sound, 0: back to the base sound
    }

    constexpr int slotsPerPerformance = 5;
    int numPerformances (Unit);         // Profiler 125, Player 10 (its 50 slots in banks of five)

    // Slot index across the unit (0-based): Performance 1 Slot 1 = 0 ... Performance 26 Slot 3 = 127.
    int slotIndex (int performance, int slot);   // performance 1-based, slot 0-4
    Cue slot (int channel, Unit, int performance, int slot, const juce::String& label);   // loads it from anywhere
    Cue slotOfCurrent (int channel, int slot, const juce::String& label);                 // CC#50-54

    // The eight effect modules: A, B, C, D, X, MOD, Delay, Reverb.
    constexpr int numEffects = 8;
    juce::String effectName (int index);
    int effectController (int index, bool keepTails);   // Delay/Reverb: keep tails = CC#27/29, cut = CC#26/28
    Cue effect (int channel, int index, bool on, bool keepTails, const juce::String& label);

    Cue tuner    (int channel, bool on);
    Cue morph    (int channel, bool on);
    Cue rotary   (int channel, bool fast);
    Cue infinity (int channel, bool on);
    Cue freeze   (int channel, bool on);
    Cue tapTempo (int channel, int taps);   // one tap per beat, so the delay follows the song

    // The same cue, but it first loads a slot and starts 1/16 note later ("Load P1 first").
    Cue withSlotFirst (const Cue&, int channel, Unit, int performance, int slot, const juce::String& performanceName);

    // Pedals (continuous): wah, pitch, volume, morph.
    constexpr int numPedals = 4;
    juce::String pedalName (int pedal);
    int pedalController (int pedal);
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

//==============================================================================
// Custom units (beta): any MIDI gear. A tile is a short list of messages the user types, for example
// "bank 1, PC 5" or "CC 50=127": Program Changes, Control Changes and bank select, sent together in this order.
namespace custom
{
    struct Step
    {
        enum class Kind { program, controller };
        Kind kind = Kind::program;
        int number = 0;      // program (0-127 as sent) or controller number
        int value = 0;       // controller value
        bool bank = false;   // written as "bank n" (CC#0)
    };

    struct Parsed
    {
        std::vector<Step> steps;
        juce::String error;   // empty when the whole text was understood
        bool ok() const { return error.isEmpty() && ! steps.empty(); }
    };

    // 'programBase' is how the unit's manual counts programs: 0 (PC 0 = the first) or 1 (PC 1 = the first).
    Parsed       parse    (const juce::String& text, int programBase);
    juce::String describe (const Parsed&, int programBase);   // "bank 1, PC 5, CC#34 = 2"
    Cue          cue      (int channel, const juce::String& name, const juce::String& text, int programBase);
}

} // namespace cues
