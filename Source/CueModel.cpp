#include "CueModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace cues
{

static constexpr int ticksPerQuarter = 960;

// The message that gives an all-at-tick-0 clip its length: the last message again, unless that's a Program Change.
// A repeated Program Change can reload the preset (Fractal's "Ignore Redundant PC" is off by default), so repeat the
// clip's last bank-select / setlist CC instead, which does nothing until a Program Change follows. A clip with only a
// Program Change gets a bank select of 0 (CC#0), which units without banks ignore.
juce::MidiMessage lengthPadding (const Cue& cue)
{
    const auto& last = cue.events.back().second;
    const auto spareBank = cue.padCc >= 0 ? cue.padCc : cue.cc0IsControl ? 32 : 0;   // a CC the unit ignores, bank select MSB, or LSB
    if (cue.toggles)
        return juce::MidiMessage::controllerEvent (last.getChannel(), spareBank, 0);
    if (! last.isProgramChange())
        return last;
    for (auto it = cue.events.rbegin(); it != cue.events.rend(); ++it)
        if (it->second.isController())
            return it->second;
    return juce::MidiMessage::controllerEvent (last.getChannel(), spareBank, 0);
}

juce::File writeMidiFile (const Cue& cue, double bpm)
{
    const auto safeBpm = juce::jlimit (20.0, 400.0, bpm);

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, cue.name), 0.0); // track name
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / safeBpm)), 0.0);
    seq.addEvent (juce::MidiMessage::textMetaEvent (6, cue.name), 0.0); // marker, visible in MIDI editors

    for (const auto& [beat, msg] : cue.events)
        seq.addEvent (msg, beat * ticksPerQuarter);

    // Ableton Live sizes a dropped clip by its last event and refuses a file whose events all sit on the first
    // tick (a single scene change, the tuner...). Add a message 1/16 later that changes nothing on the pedal
    // (lengthPadding), so the clip gets a length.
    const auto allAtStart = ! cue.events.empty()
                         && std::all_of (cue.events.begin(), cue.events.end(), [] (const auto& e) { return e.first <= 0.0; });
    if (allAtStart)
        seq.addEvent (lengthPadding (cue), 0.25 * ticksPerQuarter);

    seq.addEvent (juce::MidiMessage::endOfTrack(), juce::jmax (cue.lengthBeats, 0.5) * ticksPerQuarter);

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ticksPerQuarter);
    file.addTrack (seq);

    juce::MemoryOutputStream data;
    if (! file.writeTo (data, 1))
        return {};

    // One folder per unique content: DAWs that reference (rather than copy) the file keep working.
    const auto hash = juce::String::toHexString (data.getMemoryBlock().toBase64Encoding().hashCode64());
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                         .getChildFile ("PedalCues")
                         .getChildFile (hash);

    if (! dir.createDirectory())
        return {};

    auto name = juce::File::createLegalFileName (cue.name).trim();
    if (name.isEmpty())
        name = "Cue";

    const auto out = dir.getChildFile (name + ".mid");
    return out.replaceWithData (data.getData(), data.getDataSize()) ? out : juce::File();
}

juce::String formatBeats (double beats)
{
    if (std::abs (beats - 0.25) < 1.0e-6) return "1/16";
    if (std::abs (beats - 0.5)  < 1.0e-6) return "1/8";
    if (std::abs (beats - 1.0)  < 1.0e-6) return "1 beat";

    if (std::fmod (beats, 4.0) < 1.0e-6)
    {
        const auto bars = juce::roundToInt (beats / 4.0);
        return juce::String (bars) + (bars == 1 ? " bar" : " bars");
    }

    return juce::String (beats, beats == std::floor (beats) ? 0 : 2) + " beats";
}

//==============================================================================
// Continuous-controller moves shared by the Whammy treadle and the QC's expression pedals: the
// value (0 = heel, 1 = toe) is sampled 32 times per beat and only changes are written.
static constexpr int moveStepsPerBeat = 32;

static Cue ccMove (const juce::String& name, int channel, int controller, double lengthBeats, bool resetToHeel,
                   const std::function<double (double)>& valueAt, bool skipLastStep = false)
{
    const auto ch = juce::jlimit (1, 16, channel);
    const auto len = juce::jmax (0.125, lengthBeats);
    const auto steps = juce::jmax (1, juce::roundToInt (len * moveStepsPerBeat));

    Cue c;
    c.name = name;

    int last = -1;
    for (int i = 0; i <= steps; ++i)
    {
        if (skipLastStep && i == steps)
            break;

        const auto v = juce::jlimit (0, 127, juce::roundToInt (valueAt ((double) i / steps) * 127.0));
        if (v != last)
        {
            c.add (len * i / steps, juce::MidiMessage::controllerEvent (ch, controller, v));
            last = v;
        }
    }

    c.lengthBeats = len + 1.0 / 32.0;

    if (resetToHeel && last != 0)
    {
        c.add (len + 1.0 / 16.0, juce::MidiMessage::controllerEvent (ch, controller, 0));
        c.lengthBeats = len + 1.0 / 8.0;
    }

    return c;
}

// Linear interpolation through evenly spaced points (a drawn move).
static double samplePoints (const std::vector<float>& points, double t)
{
    const auto n = (int) points.size();
    if (n == 0)
        return 0.0;
    const auto pos = t * (n - 1);
    const auto i0 = juce::jlimit (0, n - 1, (int) std::floor (pos));
    const auto i1 = juce::jmin (n - 1, i0 + 1);
    const auto frac = pos - i0;
    return juce::jlimit (0.0, 1.0, (double) points[(size_t) i0] * (1.0 - frac) + (double) points[(size_t) i1] * frac);
}

static double eased (double x, double curve) { return std::pow (juce::jlimit (0.0, 1.0, x), curve); }

//==============================================================================
namespace qc
{
    static int clampChannel (int ch) { return juce::jlimit (1, 16, ch); }

    juce::String letter (int i)
    {
        return juce::String::charToString ((juce::juce_wchar) ('A' + juce::jlimit (0, 7, i)));
    }

    juce::String miniLabel (int i)
    {
        const auto index = juce::jlimit (0, 7, i);
        return letter (index % 4) + (index < 4 ? " (I)" : " (II)");
    }

    juce::String location (int setlist, int bank, int slot)
    {
        return (setlist <= 0 ? juce::String ("Factory") : "SL" + juce::String (setlist)) + " | " + juce::String (bank) + letter (slot);
    }

    void addScene (Cue& c, int channel, int scene, double beat)
    {
        c.add (beat, juce::MidiMessage::controllerEvent (clampChannel (channel), cc::scene, juce::jlimit (0, 7, scene)));
    }

    void addPresetLoad (Cue& c, int channel, int setlist, int bank, int slot, bool sendSetlist, double beat)
    {
        const auto ch = clampChannel (channel);
        const auto index = (juce::jlimit (1, 32, bank) - 1) * 8 + juce::jlimit (0, 7, slot); // 0..255

        c.add (beat, juce::MidiMessage::controllerEvent (ch, cc::bankMsb, index / 128));

        if (sendSetlist)
            c.add (beat, juce::MidiMessage::controllerEvent (ch, cc::setlist, juce::jlimit (0, 127, setlist)));   // 0 = Factory Presets

        c.add (beat, juce::MidiMessage::programChange (ch, index % 128));
    }

    Cue scene (int channel, int sceneIndex, const juce::String& label, bool mini)
    {
        Cue c;
        c.name = "QC Scene " + (mini ? miniLabel (sceneIndex) : letter (sceneIndex)) + (label.isNotEmpty() ? " - " + label : juce::String());
        addScene (c, channel, sceneIndex, 0.0);
        return c;
    }

    Cue preset (int channel, int setlist, int bank, int slot, bool sendSetlist, const juce::String& label)
    {
        Cue c;
        c.name = "QC Preset " + juce::String (bank) + letter (slot) + (label.isNotEmpty() ? " - " + label : juce::String());
        addPresetLoad (c, channel, setlist, bank, slot, sendSetlist, 0.0);
        return c;
    }

    Cue tuner (int channel, bool on)
    {
        Cue c;
        c.name = on ? "QC Tuner On" : "QC Tuner Off";
        c.add (0.0, juce::MidiMessage::controllerEvent (clampChannel (channel), cc::tuner, on ? 127 : 0));
        return c;
    }

    Cue stomp (int channel, int footswitch, bool on, const juce::String& label, bool mini)
    {
        Cue c;
        c.name = "QC " + (label.isNotEmpty() ? label : "Stomp " + (mini ? miniLabel (footswitch) : letter (footswitch))) + (on ? " On" : " Off");
        c.add (0.0, juce::MidiMessage::controllerEvent (clampChannel (channel),
                                                        cc::stompA + juce::jlimit (0, 7, footswitch),
                                                        on ? 127 : 0));
        return c;
    }

    Cue looper (int channel, const juce::String& name, int controller, int value, bool press)
    {
        Cue c;
        c.name = "QC Looper " + name;
        c.toggles = press;
        c.add (0.0, juce::MidiMessage::controllerEvent (clampChannel (channel), juce::jlimit (0, 127, controller), juce::jlimit (0, 127, value)));
        return c;
    }

    Cue footswitchPage (int channel, int page)
    {
        Cue c;
        c.name = page == 2 ? "QC Mini Page II" : "QC Mini Page I";
        c.add (0.0, juce::MidiMessage::controllerEvent (clampChannel (channel), cc::page, page == 2 ? 127 : 0));
        return c;
    }

    Cue tap (int channel)
    {
        Cue c;
        c.name = "QC Tap";
        c.toggles = true;
        c.add (0.0, juce::MidiMessage::controllerEvent (clampChannel (channel), cc::tap, 127));
        return c;
    }

    Cue gigView (int channel, bool open)
    {
        Cue c;
        c.name = open ? "QC Gig View On" : "QC Gig View Off";
        c.add (0.0, juce::MidiMessage::controllerEvent (clampChannel (channel), cc::gigView, open ? 127 : 0));
        return c;
    }

    Cue gigMode (int channel, int mode)
    {
        static const char* names[] = { "Preset", "Scene", "Stomp" };
        const auto m = juce::jlimit (0, 2, mode);

        Cue c;
        c.name = juce::String ("QC ") + names[m] + " Mode";
        static const int values[] = { 0, 2, 1 }; // CC#47 values for preset, scene, stomp
        c.add (0.0, juce::MidiMessage::controllerEvent (clampChannel (channel), cc::gigMode, values[m]));
        return c;
    }

    static int expController (int pedal) { return pedal == 2 ? cc::exp2 : cc::exp1; }
    static juce::String expPrefix (int pedal) { return "QC Exp " + juce::String (pedal == 2 ? 2 : 1) + " "; }

    juce::String expShapeName (ExpShape s)
    {
        switch (s)
        {
            case ExpShape::swellIn:   return "Swell In";
            case ExpShape::fadeOut:   return "Fade Out";
            case ExpShape::riseFall:  return "Rise & Fall";
            case ExpShape::slowRise:  return "Slow Rise";
            case ExpShape::wahRhythm: return "Wah Rhythm";
            case ExpShape::riseToBar: return "Rise to Bar";
            case ExpShape::toe:       return "Toe Down";
            case ExpShape::heel:      return "Heel Down";
        }
        return {};
    }

    juce::String expShapeDescription (ExpShape s)
    {
        switch (s)
        {
            case ExpShape::swellIn:   return "Heel to toe over the whole length (volume swell, opening a filter)";
            case ExpShape::fadeOut:   return "Toe to heel over the whole length";
            case ExpShape::riseFall:  return "Heel to toe and back";
            case ExpShape::slowRise:  return "Barely moves at first, then rises quickly to toe (a build-up)";
            case ExpShape::wahRhythm: return "Heel to toe and back on every beat (rhythmic wah)";
            case ExpShape::riseToBar: return "Hold heel, then rise during the last beat so it reaches toe on the next bar line";
            case ExpShape::toe:       return "Jump to toe and hold";
            case ExpShape::heel:      return "Jump to heel and hold";
        }
        return {};
    }

    // A ready-made pedal move on any controller (QC expression, Kemper pedals).
    Cue shapedMove (const juce::String& name, int channel, int controller, ExpShape s, double lengthBeats, double curve,
                    bool resetToHeel)
    {
        const auto len = juce::jmax (0.125, lengthBeats);
        auto value = [s, len, curve] (double t)
        {
            switch (s)
            {
                case ExpShape::swellIn:   return eased (t, curve);
                case ExpShape::fadeOut:   return 1.0 - eased (t, curve);
                case ExpShape::riseFall:  return t < 0.5 ? eased (t * 2.0, curve) : 1.0 - eased ((t - 0.5) * 2.0, curve);
                case ExpShape::slowRise:  return eased (t, 3.0 * curve);
                case ExpShape::wahRhythm: return 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * t * len);
                case ExpShape::riseToBar:
                {
                    const auto start = juce::jmax (0.0, 1.0 - 1.0 / len);
                    return t < start ? 0.0 : eased ((t - start) / (1.0 - start), curve);
                }
                case ExpShape::toe:  return 1.0;
                case ExpShape::heel: return 0.0;
            }
            return 0.0;
        };
        return ccMove (name + expShapeName (s) + " " + formatBeats (len), channel, controller, len, resetToHeel, value);
    }

    Cue expressionMove (int channel, int pedal, ExpShape s, double lengthBeats, double curve, bool resetToHeel)
    {
        return shapedMove (expPrefix (pedal), channel, expController (pedal), s, lengthBeats, curve, resetToHeel);
    }

    Cue drawnMove (const juce::String& name, int channel, int controller, const std::vector<float>& points,
                   double lengthBeats, bool resetToHeel)
    {
        const auto len = juce::jmax (0.125, lengthBeats);
        return ccMove (name + " " + formatBeats (len), channel, controller, len, resetToHeel,
                       [&points] (double t) { return samplePoints (points, t); });
    }

    Cue expressionDrawn (int channel, int pedal, const std::vector<float>& points, double lengthBeats, bool resetToHeel,
                         const juce::String& name)
    {
        const auto len = juce::jmax (0.125, lengthBeats);
        return ccMove (expPrefix (pedal) + (name.isNotEmpty() ? name : juce::String ("Drawn")) + " " + formatBeats (len), channel, expController (pedal), len, resetToHeel,
                       [&points] (double t) { return samplePoints (points, t); });
    }

    Cue withPresetFirst (const Cue& cue, int channel, int setlist, int bank, int slot, bool sendSetlist,
                         const juce::String& presetName)
    {
        constexpr double delay = 0.25;   // same gap as scene and stomp tiles

        Cue c;
        c.name = "QC " + presetName + " > " + cue.name.fromFirstOccurrenceOf ("QC ", false, false);
        addPresetLoad (c, channel, setlist, bank, slot, sendSetlist, 0.0);
        for (const auto& [beat, msg] : cue.events)
            c.add (beat + delay, msg);
        c.lengthBeats = cue.lengthBeats + delay;
        return c;
    }

    Cue expressionSet (int channel, int pedal, float position)
    {
        const auto v = juce::jlimit (0, 127, juce::roundToInt (juce::jlimit (0.0f, 1.0f, position) * 127.0f));
        Cue c;
        c.name = expPrefix (pedal) + (v == 0 ? juce::String ("Heel") : v == 127 ? juce::String ("Toe")
                                                                     : juce::String (juce::roundToInt (position * 100.0f)) + "%");
        c.add (0.0, juce::MidiMessage::controllerEvent (clampChannel (channel), expController (pedal), v));
        return c;
    }
}

//==============================================================================
namespace kemper
{
    static int ch (int channel) { return juce::jlimit (1, 16, channel); }

    int numPerformances (Unit u) { return u == Unit::player ? 10 : 125; }

    int slotIndex (int performance, int slot)
    {
        return (juce::jmax (1, performance) - 1) * slotsPerPerformance + juce::jlimit (0, slotsPerPerformance - 1, slot);
    }

    Cue slot (int channel, Unit u, int performance, int slotNumber, const juce::String& label)
    {
        const auto index = juce::jlimit (0, numPerformances (u) * slotsPerPerformance - 1, slotIndex (performance, slotNumber));
        Cue c;
        c.name = "Kemper P" + juce::String (performance) + "." + juce::String (slotNumber + 1)
               + (label.isNotEmpty() ? " - " + label : juce::String());
        if (u == Unit::profiler)
            c.add (0.0, juce::MidiMessage::controllerEvent (ch (channel), cc::bankLsb, index / 128));   // 625 slots over 5 banks
        c.add (0.0, juce::MidiMessage::programChange (ch (channel), index % 128));
        return c;
    }

    Cue slotOfCurrent (int channel, int slotNumber, const juce::String& label)
    {
        Cue c;
        c.name = "Kemper Slot " + juce::String (slotNumber + 1) + (label.isNotEmpty() ? " - " + label : juce::String());
        c.add (0.0, juce::MidiMessage::controllerEvent (ch (channel), cc::slot1 + juce::jlimit (0, 4, slotNumber), 1));
        return c;
    }

    juce::String effectName (int i)
    {
        static const char* names[numEffects] = { "Stomp A", "Stomp B", "Stomp C", "Stomp D", "Effect X", "Mod", "Delay", "Reverb" };
        return names[juce::jlimit (0, numEffects - 1, i)];
    }

    int effectController (int i, bool keepTails)
    {
        static const int controllers[numEffects] = { 17, 18, 19, 20, 22, 24, 26, 28 };
        i = juce::jlimit (0, numEffects - 1, i);
        return controllers[i] + (i >= 6 && keepTails ? 1 : 0);
    }

    Cue effect (int channel, int i, bool on, bool keepTails, const juce::String& label)
    {
        Cue c;
        c.name = "Kemper " + (label.isNotEmpty() ? label : effectName (i)) + (on ? " On" : " Off");
        c.add (0.0, juce::MidiMessage::controllerEvent (ch (channel), effectController (i, keepTails), on ? 1 : 0));
        return c;
    }

    static Cue switchCue (const juce::String& name, int channel, int controller, bool on)
    {
        Cue c;
        c.name = "Kemper " + name;
        c.add (0.0, juce::MidiMessage::controllerEvent (ch (channel), controller, on ? 1 : 0));
        return c;
    }

    Cue tuner    (int channel, bool on)   { return switchCue (on ? "Tuner On" : "Tuner Off", channel, cc::tuner, on); }
    Cue morph    (int channel, bool on)   { return switchCue (on ? "Morph On" : "Morph Off", channel, cc::morph, on); }
    Cue rotary   (int channel, bool fast) { return switchCue (fast ? "Rotary Fast" : "Rotary Slow", channel, cc::rotary, fast); }
    Cue infinity (int channel, bool on)   { return switchCue (on ? "Delay Infinity On" : "Delay Infinity Off", channel, cc::infinity, on); }
    Cue freeze   (int channel, bool on)   { return switchCue (on ? "Freeze On" : "Freeze Off", channel, cc::freeze, on); }

    Cue tapTempo (int channel, int taps)
    {
        // Value 0: a plain tap. (Holding value 1 for 3 seconds would start the Beat Scanner instead.)
        Cue c;
        taps = juce::jlimit (1, 16, taps);
        c.name = "Kemper Tap Tempo x" + juce::String (taps);
        for (int i = 0; i < taps; ++i)
            c.add ((double) i, juce::MidiMessage::controllerEvent (ch (channel), cc::tap, 0));
        c.lengthBeats = (double) taps;
        return c;
    }

    Cue withSlotFirst (const Cue& cue, int channel, Unit u, int performance, int slotNumber, const juce::String& performanceName)
    {
        constexpr double delay = 0.25;   // the same 1/16 as the Quad Cortex's Load 1A first
        auto c = slot (channel, u, performance, slotNumber, {});
        c.name = "Kemper " + performanceName + " " + juce::String (slotNumber + 1) + " > "
               + cue.name.fromFirstOccurrenceOf ("Kemper ", false, false);
        for (const auto& [beat, msg] : cue.events)
            c.add (beat + delay, msg);
        c.lengthBeats = cue.lengthBeats + delay;
        return c;
    }

    juce::String pedalName (int p)
    {
        static const char* names[numPedals] = { "Wah", "Pitch", "Volume", "Morph" };
        return names[juce::jlimit (0, numPedals - 1, p)];
    }

    int pedalController (int p)
    {
        static const int controllers[numPedals] = { cc::wah, cc::pitch, cc::volume, cc::morphPedal };
        return controllers[juce::jlimit (0, numPedals - 1, p)];
    }
}

//==============================================================================
namespace whammy
{
    // Order follows the effect knob / MIDI program chart (program 1 = first entry).
    static const char* const effectNames[numEffects] = {
        "2 Oct Up", "Oct Up", "5th Up", "4th Up", "2nd Down", "4th Down", "5th Down",
        "Oct Down", "2 Oct Down", "Dive Bomb",
        "Deep Detune", "Shallow Detune",
        "2nd Up / 3rd Up", "b3rd Up / 3rd Up", "3rd Up / 4th Up", "4th Up / 5th Up",
        "5th Up / 6th Up", "5th Up / 7th Up", "4th Down / 3rd Down", "5th Down / 4th Down",
        "Oct Up / Oct Down"
    };

    juce::String defaultName (int effect)
    {
        return effectNames[juce::jlimit (0, numEffects - 1, effect)];
    }

    Category category (int effect)
    {
        if (effect < 10) return Category::whammy;
        if (effect < 12) return Category::detune;
        return Category::harmony;
    }

    juce::Colour colour (int effect)
    {
        switch (category (effect))
        {
            case Category::whammy:  return juce::Colour (0xffd9342b);
            case Category::detune:  return juce::Colour (0xff2f7fd6);
            case Category::harmony: return juce::Colour (0xff3a9b52);
        }
        return juce::Colours::grey;
    }

    int programNumber (int effect, bool chords, bool bypass)
    {
        return 1 + juce::jlimit (0, numEffects - 1, effect)
                 + (bypass ? numEffects : 0)
                 + (chords ? 2 * numEffects : 0);
    }

    Cue effect (int channel, int effectIndex, const juce::String& name, bool chords, bool bypass,
                int pcNumberBase, bool heelFirst)
    {
        const auto ch = juce::jlimit (1, 16, channel);
        const auto program = juce::jlimit (0, 127, programNumber (effectIndex, chords, bypass) - pcNumberBase);

        Cue c;
        c.name = "Whammy " + name + (chords ? " [Chords]" : "") + (bypass ? " (Bypass)" : "");

        if (heelFirst)
            c.add (0.0, juce::MidiMessage::controllerEvent (ch, 11, 0));

        c.add (0.0, juce::MidiMessage::programChange (ch, program));
        return c;
    }

    juce::String shiftName (int step)
    {
        step = juce::jlimit (0, numShifts - 1, step);
        return step == 7 ? juce::String ("Oct") : step == 8 ? juce::String ("Oct + Dry") : juce::String (step + 1);
    }

    juce::String shiftDescription (bool up, int step)
    {
        step = juce::jlimit (0, numShifts - 1, step);
        const juce::String way = up ? "up" : "down";
        if (step == 7) return "Shifts everything " + way + " an octave";
        if (step == 8) return "Shifts " + way + " an octave and adds the dry signal (12-string sound)";
        const auto n = step + 1;
        return "Shifts everything " + way + " " + juce::String (n) + (n == 1 ? " semitone" : " semitones")
             + (n == 1 ? (up ? juce::String() : juce::String (" (Eb tuning)")) : n == 2 ? " (a whole step)" : juce::String());
    }

    int dropTuneProgram (bool up, int step, bool bypass)
    {
        step = juce::jlimit (0, numShifts - 1, step);
        return (up ? 43 + step : 60 - step) + (bypass ? 18 : 0);
    }

    Cue dropTune (int channel, bool up, int step, bool bypass, int pcNumberBase)
    {
        Cue c;
        c.name = "Whammy DT Shift " + juce::String (up ? "Up " : "Down ") + shiftName (step) + (bypass ? " (Bypass)" : "");
        c.add (0.0, juce::MidiMessage::programChange (juce::jlimit (1, 16, channel),
                                                      juce::jlimit (0, 127, dropTuneProgram (up, step, bypass) - pcNumberBase)));
        return c;
    }

    juce::String shapeName (Shape s)
    {
        switch (s)
        {
            case Shape::rampUp:    return "Ramp Up";
            case Shape::rampDown:  return "Ramp Down";
            case Shape::swell:     return "Rise & Fall";
            case Shape::dive:      return "Dive";
            case Shape::trill:     return "Trill";
            case Shape::bendToBar: return "Bend to Bar";
            case Shape::toe:       return "Toe Down";
            case Shape::heel:      return "Heel Down";
        }
        return {};
    }

    juce::String shapeDescription (Shape s)
    {
        switch (s)
        {
            case Shape::rampUp:    return "Heel to toe over the whole length";
            case Shape::rampDown:  return "Toe to heel over the whole length";
            case Shape::swell:     return "Heel to toe to heel";
            case Shape::dive:      return "Slow start, accelerating to full toe (great with Dive Bomb)";
            case Shape::trill:     return "Toggle heel/toe on 1/16 notes";
            case Shape::bendToBar: return "Hold heel, then bend during the last beat so it lands on the next bar line";
            case Shape::toe:       return "Jump to toe and hold";
            case Shape::heel:      return "Jump to heel and hold";
        }
        return {};
    }

    static double shapeValue (Shape s, double t, double lengthBeats, double curve)
    {
        switch (s)
        {
            case Shape::rampUp:   return eased (t, curve);
            case Shape::rampDown: return 1.0 - eased (t, curve);
            case Shape::swell:    return t < 0.5 ? eased (t * 2.0, curve) : 1.0 - eased ((t - 0.5) * 2.0, curve);
            case Shape::dive:     return eased (t, 3.0 * curve);
            case Shape::trill:
            {
                const auto sixteenth = (int) std::floor (t * lengthBeats * 4.0 + 1.0e-9);
                return (sixteenth % 2) == 0 ? 1.0 : 0.0;
            }
            case Shape::bendToBar:
            {
                const auto start = juce::jmax (0.0, 1.0 - 1.0 / lengthBeats);
                return t < start ? 0.0 : eased ((t - start) / (1.0 - start), curve);
            }
            case Shape::toe:  return 1.0;
            case Shape::heel: return 0.0;
        }
        return 0.0;
    }

    Cue sweep (int channel, Shape s, double lengthBeats, double curve, bool resetToHeel)
    {
        const auto len = juce::jmax (0.125, lengthBeats);
        // The trill holds its last state until the end rather than toggling on the final tick.
        return ccMove ("Whammy " + shapeName (s) + " " + formatBeats (len), channel, 11, len, resetToHeel,
                       [s, len, curve] (double t) { return shapeValue (s, t, len, curve); }, s == Shape::trill);
    }

    Cue drawn (int channel, const std::vector<float>& points, double lengthBeats, bool resetToHeel,
               const juce::String& name)
    {
        const auto len = juce::jmax (0.125, lengthBeats);
        return ccMove ("Whammy " + (name.isNotEmpty() ? name : juce::String ("Drawn")) + " " + formatBeats (len), channel, 11, len, resetToHeel,
                       [&points] (double t) { return samplePoints (points, t); });
    }

    void moveEarly (Cue& cue, double beats)
    {
        if (beats <= 0.0 || cue.events.empty())
            return;
        constexpr double tick = 1.0 / 960.0;   // a MIDI file's resolution: later points never land on the first one's tick
        const auto first = cue.events.front().first;
        for (size_t i = 1; i < cue.events.size(); ++i)
            if (cue.events[i].first > first)
                cue.events[i].first = juce::jmax (first + tick, cue.events[i].first - beats);
        std::stable_sort (cue.events.begin(), cue.events.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });
    }

    std::vector<float> defaultDrawing()
    {
        // Rise, hold at toe, shake, then fall back to heel.
        std::vector<float> v ((size_t) drawPoints);
        for (int i = 0; i < drawPoints; ++i)
        {
            const auto t = (double) i / (drawPoints - 1);
            double y;
            if (t < 0.2)       y = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::pi * t / 0.2);
            else if (t < 0.5)  y = 1.0;
            else if (t < 0.85) y = 0.75 + 0.25 * std::cos ((t - 0.5) / 0.35 * juce::MathConstants<double>::twoPi * 2.0);
            else               y = 1.0 - (t - 0.85) / 0.15;
            v[(size_t) i] = (float) juce::jlimit (0.0, 1.0, y);
        }
        return v;
    }

    juce::String waveName (Wave w)
    {
        static const char* names[] = { "Sine", "Triangle", "Square", "Saw up", "Saw down" };
        return names[juce::jlimit (0, numWaves - 1, (int) w)];
    }

    std::vector<float> waveDrawing (Wave w, double cycles, double phaseDegrees, double shape, double low, double high,
                                    double grow, double speed)
    {
        const auto peak = juce::jlimit (0.02, 0.98, 0.5 + 0.5 * juce::jlimit (-1.0, 1.0, shape));   // where the wave tops out
        const auto g = juce::jlimit (-1.0, 1.0, grow);
        const auto warp = std::pow (3.0, juce::jlimit (-1.0, 1.0, speed));   // > 1: slow start, waves bunch up at the end
        const auto lo = juce::jlimit (0.0, 1.0, low), hi = juce::jlimit (0.0, 1.0, high);
        std::vector<float> v ((size_t) drawPoints);
        for (int i = 0; i < drawPoints; ++i)
        {
            const auto t = (double) i / (drawPoints - 1);
            auto x = std::pow (t, warp) * juce::jmax (0.0, cycles) + phaseDegrees / 360.0;
            x -= std::floor (x);                                     // 0..1 within the current cycle
            double y = 0.0;
            switch (w)
            {
                case Wave::triangle: y = x < peak ? x / peak : (1.0 - x) / (1.0 - peak); break;
                case Wave::square:   y = x < peak ? 1.0 : 0.0; break;
                case Wave::sawUp:    y = x; break;
                case Wave::sawDown:  y = 1.0 - x; break;
                case Wave::sine:
                {
                    // Starts at the low point; skew moves the top earlier or later in the cycle.
                    const auto warped = x < peak ? 0.5 * x / peak : 0.5 + 0.5 * (x - peak) / (1.0 - peak);
                    y = 0.5 - 0.5 * std::cos (juce::MathConstants<double>::twoPi * warped);
                    break;
                }
            }
            const auto envelope = g >= 0.0 ? 1.0 - g + g * t : 1.0 + g * t;   // grow: from low up to full; < 0: dies away
            v[(size_t) i] = (float) (lo + (hi - lo) * juce::jlimit (0.0, 1.0, y) * envelope);
        }
        return v;
    }

    ImportedMove importMove (const juce::MidiFile& file, int preferredCc, const std::vector<double>& lengths)
    {
        ImportedMove result;
        const auto ppq = (int) file.getTimeFormat();
        if (ppq <= 0)
        {
            result.error = "This MIDI file uses SMPTE time, not beats: export it from your DAW as a normal MIDI clip.";
            return result;
        }

        // Every CC and pitch bend in the file, on any track and channel.
        std::array<int, 128> ccCount {};
        int bendCount = 0;
        double lastBeat = 0.0;
        for (int t = 0; t < file.getNumTracks(); ++t)
            if (const auto* track = file.getTrack (t))
                for (const auto* e : *track)
                {
                    lastBeat = juce::jmax (lastBeat, e->message.getTimeStamp() / ppq);
                    if (e->message.isController())
                        ++ccCount[(size_t) e->message.getControllerNumber()];
                    else if (e->message.isPitchWheel())
                        ++bendCount;
                }

        int cc = -1;
        if (juce::isPositiveAndBelow (preferredCc, 128) && ccCount[(size_t) preferredCc] > 0)
            cc = preferredCc;
        else
            for (int n = 1; n < 128; ++n)   // never bank select (CC#0 / CC#32)
                if (n != 32 && ccCount[(size_t) n] > 0 && (cc < 0 || ccCount[(size_t) n] > ccCount[(size_t) cc]))
                    cc = n;
        if (cc < 0 && bendCount == 0)
        {
            result.error = "No CC or pitch bend in this MIDI file: draw the move as CC automation (e.g. CC#11) in your DAW and export the clip.";
            return result;
        }

        // The curve as (beat, value) steps, in time order.
        std::vector<std::pair<double, float>> steps;
        for (int t = 0; t < file.getNumTracks(); ++t)
            if (const auto* track = file.getTrack (t))
                for (const auto* e : *track)
                {
                    const auto& m = e->message;
                    if (cc >= 0 && m.isController() && m.getControllerNumber() == cc)
                        steps.push_back ({ m.getTimeStamp() / ppq, (float) m.getControllerValue() / 127.0f });
                    else if (cc < 0 && m.isPitchWheel())
                        steps.push_back ({ m.getTimeStamp() / ppq, (float) m.getPitchWheelValue() / 16383.0f });
                }
        std::stable_sort (steps.begin(), steps.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

        // The shortest length that holds the whole clip (or the longest there is). A tail up to 1/8 beat past a length
        // doesn't count: PedalCues' own clips end just after the move, with the "back to heel" CC 1/16 later.
        constexpr double tail = 0.125 + 1.0e-6;
        const auto clip = juce::jmax (lastBeat, steps.back().first);
        result.beats = lengths.empty() ? juce::jmax (0.25, clip) : lengths.back();
        for (const auto l : lengths)
            if (l + tail >= clip)
            {
                result.beats = l;
                break;
            }
        result.truncated = clip > result.beats + tail;
        // Anything after the move's end (that back-to-heel CC) isn't part of the shape.
        steps.erase (std::remove_if (steps.begin(), steps.end(), [&] (const auto& st) { return st.first > result.beats + 1.0e-9; }), steps.end());
        if (steps.empty())
        {
            result.error = "The CC curve starts after the end of the move.";
            return result;
        }
        result.controller = cc;

        // Sample it like the controller hears it: a value holds until the next one, except that values close together
        // (a drawn ramp, like PedalCues' own 32 per beat) are joined by a straight line, so slopes don't turn into stairs.
        constexpr double joinGap = 1.0 / 16.0 + 1.0e-6;
        result.points.resize ((size_t) drawPoints);
        size_t next = 0;
        for (int i = 0; i < drawPoints; ++i)
        {
            const auto beat = result.beats * i / (drawPoints - 1);
            while (next < steps.size() && steps[next].first <= beat + 1.0e-9)
                ++next;
            float value;
            if (next == 0)
                value = steps.front().second;                          // before the first value: that one
            else if (next == steps.size())
                value = steps.back().second;                           // after the last: it holds
            else
            {
                const auto& a = steps[next - 1];
                const auto& b = steps[next];
                const auto gap = b.first - a.first;
                value = gap <= joinGap && gap > 0.0 ? a.second + (b.second - a.second) * (float) ((beat - a.first) / gap) : a.second;
            }
            result.points[(size_t) i] = juce::jlimit (0.0f, 1.0f, value);
        }
        return result;
    }

    juce::String encodeDrawing (const std::vector<float>& points)
    {
        juce::StringArray parts;
        for (auto p : points)
            parts.add (juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, p) * 1000.0f)));
        return parts.joinIntoString (",");
    }

    std::vector<float> decodeDrawing (const juce::String& text)
    {
        const auto parts = juce::StringArray::fromTokens (text, ",", "");
        if (parts.size() < 2)
            return defaultDrawing();

        std::vector<float> raw;
        for (const auto& p : parts)
            raw.push_back (juce::jlimit (0.0f, 1.0f, p.trim().getFloatValue() / 1000.0f));

        if ((int) raw.size() == drawPoints)
            return raw;

        std::vector<float> v ((size_t) drawPoints);
        for (int i = 0; i < drawPoints; ++i)
        {
            const auto pos = (double) i / (drawPoints - 1) * (double) (raw.size() - 1);
            const auto i0 = (size_t) std::floor (pos);
            const auto i1 = juce::jmin (raw.size() - 1, i0 + 1);
            const auto frac = (float) (pos - (double) i0);
            v[(size_t) i] = raw[i0] * (1.0f - frac) + raw[i1] * frac;
        }
        return v;
    }

    juce::String segmentName (Segment s)
    {
        switch (s)
        {
            case Segment::square:       return "Square";
            case Segment::linear:       return "Linear";
            case Segment::slowStartEnd: return "Slow start/end";
            case Segment::fastStart:    return "Fast start";
            case Segment::fastEnd:      return "Fast end";
            case Segment::bezier:       return "Bezier";
        }
        return {};
    }

    float segmentValue (Segment shape, float tension, float a, float b, float x)
    {
        x = juce::jlimit (0.0f, 1.0f, x);
        float g = x;
        switch (shape)
        {
            case Segment::square:       g = x < 1.0f ? 0.0f : 1.0f; break;
            case Segment::linear:       break;
            case Segment::slowStartEnd: g = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::pi * x); break;
            case Segment::fastStart:    g = 1.0f - std::pow (1.0f - x, 3.0f); break;
            case Segment::fastEnd:      g = std::pow (x, 3.0f); break;
            case Segment::bezier:
            {
                // Positive tension bulges the line up whichever way it runs: early on a rise, late on a fall.
                const auto u = juce::jlimit (-1.0f, 1.0f, tension) * (b >= a ? 1.0f : -1.0f);
                const auto power = 1.0f + 4.0f * std::abs (u);
                g = u > 0.0f ? 1.0f - std::pow (1.0f - x, power) : std::pow (x, power);
                break;
            }
        }
        return a + (b - a) * g;
    }

    std::vector<float> renderBreakpoints (const std::vector<Breakpoint>& bps)
    {
        std::vector<float> v ((size_t) drawPoints, 0.0f);
        if (bps.empty())
            return v;

        size_t seg = 0;
        for (int i = 0; i < drawPoints; ++i)
        {
            const auto t = (float) i / (float) (drawPoints - 1);
            while (seg + 1 < bps.size() && bps[seg + 1].time <= t)
                ++seg;
            float value;
            if (t <= bps.front().time)
                value = bps.front().value;
            else if (seg + 1 >= bps.size())
                value = bps.back().value;
            else
            {
                const auto& a = bps[seg];
                const auto& b = bps[seg + 1];
                const auto dt = b.time - a.time;
                value = dt <= 0.0f ? b.value : segmentValue (a.shape, a.tension, a.value, b.value, (t - a.time) / dt);
            }
            v[(size_t) i] = juce::jlimit (0.0f, 1.0f, value);
        }
        return v;
    }

    std::vector<Breakpoint> simplifyDrawing (const std::vector<float>& samples, float tolerance)
    {
        const auto n = samples.size();
        if (n == 0)
            return { { 0.0f, 0.0f }, { 1.0f, 0.0f } };
        if (n == 1)
            return { { 0.0f, samples[0] }, { 1.0f, samples[0] } };

        std::vector<bool> keep (n, false);
        keep.front() = keep.back() = true;
        std::vector<std::pair<size_t, size_t>> stack { { 0, n - 1 } };
        while (! stack.empty())
        {
            const auto [a, b] = stack.back();
            stack.pop_back();
            float worst = 0.0f;
            size_t at = a;
            for (auto i = a + 1; i < b; ++i)
            {
                const auto line = samples[a] + (samples[b] - samples[a]) * (float) (i - a) / (float) (b - a);
                const auto d = std::abs (samples[i] - line);
                if (d > worst) { worst = d; at = i; }
            }
            if (worst > tolerance)
            {
                keep[at] = true;
                stack.push_back ({ a, at });
                stack.push_back ({ at, b });
            }
        }

        std::vector<Breakpoint> out;
        for (size_t i = 0; i < n; ++i)
            if (keep[i])
                out.push_back ({ (float) i / (float) (n - 1), samples[i] });
        return out;
    }

    double gridStepBeats (double lengthBeats, double widthPx, double minGapPx)
    {
        for (auto step : { 0.25, 0.5 })
            if (lengthBeats > 0.0 && widthPx * step / lengthBeats >= minGapPx)
                return step;
        return 1.0;
    }

    float snapTime (float time, double lengthBeats, double stepBeats)
    {
        if (lengthBeats <= 0.0 || stepBeats <= 0.0)
            return juce::jlimit (0.0f, 1.0f, time);
        const auto steps = lengthBeats / stepBeats;
        return juce::jlimit (0.0f, 1.0f, (float) (std::round (time * steps) / steps));
    }

    float snapValue (float value)
    {
        return juce::jlimit (0.0f, 1.0f, std::round (value * 100.0f) / 100.0f);
    }

    juce::String positionLabel (double beat)
    {
        beat = juce::jmax (0.0, beat);
        const auto near = [] (double x) { return std::abs (x - std::round (x)) < 1.0e-3; };
        if (near (beat))
            beat = std::round (beat);
        const auto whole = std::floor (beat + 1.0e-6);
        const auto bar = (int) (whole / 4.0) + 1;
        const auto beatInBar = (int) whole % 4 + 1;
        const auto label = juce::String (bar) + "." + juce::String (beatInBar);
        const auto frac = beat - whole;
        if (frac < 1.0e-3)
            return label;
        if (near (frac * 4.0))
            return label + "." + juce::String ((int) std::round (frac * 4.0) + 1);
        // Off the grid: the nearest beat and how far from it ("1.3 -0.02" just before beat 3).
        const auto nearest = std::round (beat);
        const auto offset = beat - nearest;
        const auto nearLabel = juce::String ((int) (nearest / 4.0) + 1) + "." + juce::String ((int) nearest % 4 + 1);
        return nearLabel + (offset < 0.0 ? " -" : " +") + juce::String (std::abs (offset), 2);
    }

    double parsePosition (const juce::String& text)
    {
        const auto t = text.trim();
        if (t.isEmpty())
            return -1.0;
        if (t.startsWithChar ('+'))
        {
            const auto rest = t.substring (1).trim();
            return rest.containsOnly ("0123456789.") && rest.isNotEmpty() ? rest.getDoubleValue() : -1.0;
        }
        if (! t.containsOnly ("0123456789."))
            return -1.0;
        const auto parts = juce::StringArray::fromTokens (t, ".", "");
        if (parts.size() < 1 || parts.size() > 3)
            return -1.0;
        for (const auto& part : parts)
            if (part.isEmpty())
                return -1.0;
        const auto bar = parts[0].getIntValue();
        const auto beat = parts.size() > 1 ? parts[1].getIntValue() : 1;
        const auto sixteenth = parts.size() > 2 ? parts[2].getIntValue() : 1;
        if (bar < 1 || beat < 1 || beat > 4 || sixteenth < 1 || sixteenth > 4)
            return -1.0;
        return (bar - 1) * 4.0 + (beat - 1) + (sixteenth - 1) * 0.25;
    }

    int numSelected (const std::vector<Breakpoint>& bps)
    {
        return (int) std::count_if (bps.begin(), bps.end(), [] (const Breakpoint& b) { return b.selected; });
    }

    float limitSelectionShift (const std::vector<Breakpoint>& bps, float dt)
    {
        const auto n = (int) bps.size();
        auto lo = -2.0f, hi = 2.0f;
        auto any = false;
        const auto moves = [&] (int i) { return i > 0 && i < n - 1 && bps[(size_t) i].selected; };
        for (int i = 1; i < n - 1; ++i)
        {
            if (! moves (i))
                continue;
            any = true;
            if (! moves (i - 1))
                lo = juce::jmax (lo, bps[(size_t) i - 1].time + minPointGap - bps[(size_t) i].time);
            if (! moves (i + 1))
                hi = juce::jmin (hi, bps[(size_t) i + 1].time - minPointGap - bps[(size_t) i].time);
        }
        if (! any || lo > hi)
            return 0.0f;
        return juce::jlimit (lo, hi, dt);
    }

    std::vector<Breakpoint> moveSelection (const std::vector<Breakpoint>& from, float dt, float dv)
    {
        auto out = from;
        dt = limitSelectionShift (from, dt);
        const auto n = out.size();
        for (size_t i = 0; i < n; ++i)
        {
            if (! out[i].selected)
                continue;
            if (i > 0 && i + 1 < n)
                out[i].time = juce::jlimit (0.0f, 1.0f, out[i].time + dt);
            out[i].value = juce::jlimit (0.0f, 1.0f, out[i].value + dv);
        }
        return out;
    }

    namespace
    {
        bool selectionRange (const std::vector<Breakpoint>& bps, float& lo, float& hi, float& t0, float& t1)
        {
            auto any = false;
            for (const auto& b : bps)
            {
                if (! b.selected)
                    continue;
                if (! any) { lo = hi = b.value; t0 = t1 = b.time; any = true; }
                lo = juce::jmin (lo, b.value); hi = juce::jmax (hi, b.value);
                t0 = juce::jmin (t0, b.time);  t1 = juce::jmax (t1, b.time);
            }
            return any;
        }
    }

    std::vector<Breakpoint> scaleSelection (const std::vector<Breakpoint>& from, float factor)
    {
        auto out = from;
        float lo = 0, hi = 0, t0 = 0, t1 = 0;
        if (! selectionRange (from, lo, hi, t0, t1))
            return out;
        const auto centre = (lo + hi) * 0.5f;
        factor = juce::jmax (0.0f, factor);
        for (auto& b : out)
            if (b.selected)
                b.value = juce::jlimit (0.0f, 1.0f, centre + (b.value - centre) * factor);
        return out;
    }

    std::vector<Breakpoint> tiltSelection (const std::vector<Breakpoint>& from, float left, float right)
    {
        auto out = from;
        float lo = 0, hi = 0, t0 = 0, t1 = 0;
        if (! selectionRange (from, lo, hi, t0, t1))
            return out;
        for (auto& b : out)
            if (b.selected)
            {
                const auto f = t1 > t0 ? (b.time - t0) / (t1 - t0) : 0.5f;
                b.value = juce::jlimit (0.0f, 1.0f, b.value + left + (right - left) * f);
            }
        return out;
    }

    void invertSelection (std::vector<Breakpoint>& bps)
    {
        for (auto& b : bps)
            if (b.selected)
                b.value = 1.0f - b.value;
    }

    int deleteSelection (std::vector<Breakpoint>& bps)
    {
        if (bps.size() <= 2)
            return 0;
        const auto before = (int) bps.size();
        const auto first = bps.front(), last = bps.back();
        std::vector<Breakpoint> kept { first };
        for (size_t i = 1; i + 1 < bps.size(); ++i)
            if (! bps[i].selected)
                kept.push_back (bps[i]);
        kept.push_back (last);
        bps = kept;
        return before - (int) bps.size();
    }

    void setSelectionShape (std::vector<Breakpoint>& bps, Segment shape)
    {
        for (size_t i = 0; i + 1 < bps.size(); ++i)
            if (bps[i].selected)
            {
                bps[i].shape = shape;
                if (shape != Segment::bezier)
                    bps[i].tension = 0.0f;
                else if (std::abs (bps[i].tension) < 1.0e-4f)
                    bps[i].tension = 0.5f;   // a visible curve from the start (straight would look like Linear)
            }
    }

    std::vector<Breakpoint> mergeStroke (const std::vector<Breakpoint>& bps, const std::vector<float>& samples, int first, int last,
                                         float tolerance)
    {
        const auto n = (int) samples.size();
        if (n < 2)
            return bps;
        if (first > last)
            std::swap (first, last);
        first = juce::jlimit (0, n - 1, first);
        last = juce::jlimit (0, n - 1, last);
        const auto t0 = (float) first / (float) (n - 1);
        const auto t1 = (float) last / (float) (n - 1);

        std::vector<Breakpoint> stroke;
        if (first == last)
        {
            stroke.push_back ({ t0, juce::jlimit (0.0f, 1.0f, samples[(size_t) first]) });
        }
        else
        {
            const std::vector<float> part (samples.begin() + first, samples.begin() + last + 1);
            for (auto p : simplifyDrawing (part, tolerance))
                stroke.push_back ({ t0 + p.time * (t1 - t0), juce::jlimit (0.0f, 1.0f, p.value) });
        }

        std::vector<Breakpoint> out;
        const auto eps = minPointGap - 1.0e-5f;
        for (const auto& b : bps)
            if (b.time < t0 - eps || b.time > t1 + eps)
                out.push_back (b);
        for (const auto& b : stroke)
            out.push_back (b);
        std::sort (out.begin(), out.end(), [] (const Breakpoint& a, const Breakpoint& b) { return a.time < b.time; });
        if (out.empty() || out.front().time > 0.0f)
            out.insert (out.begin(), { 0.0f, out.empty() ? 0.0f : out.front().value });
        if (out.back().time < 1.0f)
            out.push_back ({ 1.0f, out.back().value });
        return out;
    }

    bool samePoints (const std::vector<Breakpoint>& a, const std::vector<Breakpoint>& b)
    {
        if (a.size() != b.size())
            return false;
        for (size_t i = 0; i < a.size(); ++i)
            if (! juce::exactlyEqual (a[i].time, b[i].time) || ! juce::exactlyEqual (a[i].value, b[i].value) || a[i].shape != b[i].shape
                || ! juce::exactlyEqual (a[i].tension, b[i].tension))
                return false;
        return true;
    }

    void DrawHistory::record (const std::vector<Breakpoint>& s, const juce::String& mergeKey)
    {
        if (index >= 0 && samePoints (states[(size_t) index], s))
        {
            states[(size_t) index] = s;   // the same line: keep the newer selection, no new step
            return;
        }
        states.resize ((size_t) (index + 1));   // a new edit drops what could be redone
        if (mergeKey.isNotEmpty() && mergeKey == lastKey && index > 0)
            states[(size_t) index] = s;
        else
        {
            states.push_back (s);
            ++index;
        }
        lastKey = mergeKey;
        while ((int) states.size() > maxSteps + 1)
        {
            states.erase (states.begin());
            --index;
        }
    }

    std::vector<Breakpoint> DrawHistory::undo()
    {
        lastKey = {};
        if (index > 0)
            --index;
        return index >= 0 ? states[(size_t) index] : std::vector<Breakpoint>();
    }

    std::vector<Breakpoint> DrawHistory::redo()
    {
        lastKey = {};
        if (index + 1 < (int) states.size())
            ++index;
        return index >= 0 ? states[(size_t) index] : std::vector<Breakpoint>();
    }
}

//==============================================================================
namespace custom
{
    namespace
    {
        // "34=2", "34 = 2", "34 2": two numbers; returns false if it isn't exactly that.
        bool twoNumbers (juce::String t, int& a, int& b)
        {
            const auto parts = juce::StringArray::fromTokens (t.replaceCharacter ('=', ' '), " ", {});
            juce::StringArray nums;
            for (const auto& x : parts)
                if (x.isNotEmpty())
                    nums.add (x);
            if (nums.size() != 2 || ! nums[0].containsOnly ("0123456789") || ! nums[1].containsOnly ("0123456789"))
                return false;
            a = nums[0].getIntValue();
            b = nums[1].getIntValue();
            return true;
        }

        bool oneNumber (const juce::String& t, int& n)
        {
            const auto x = t.trim();
            if (x.isEmpty() || ! x.containsOnly ("0123456789"))
                return false;
            n = x.getIntValue();
            return true;
        }

        juce::String usage()
        {
            return "Write messages like PC 5, CC 34=127 or bank 1, separated by commas.";
        }
    }

    Parsed parse (const juce::String& text, int programBase)
    {
        Parsed r;
        const auto base = programBase == 1 ? 1 : 0;
        const auto items = juce::StringArray::fromTokens (text.replace (";", ",").replace ("\n", ","), ",", {});

        for (const auto& item : items)
        {
            const auto original = item.trim();
            if (original.isEmpty())
                continue;
            const auto t = original.toLowerCase().removeCharacters ("#");
            Step st;
            int a = 0, b = 0;

            if (t.startsWith ("bank"))
            {
                if (! oneNumber (t.fromFirstOccurrenceOf ("bank", false, false), a) || a > 127)
                    return { {}, "\"" + original + "\": bank takes a number from 0 to 127, for example bank 1." };
                st.kind = Step::Kind::controller;
                st.number = 0;
                st.value = a;
                st.bank = true;
            }
            else if (t.startsWith ("cc"))
            {
                if (! twoNumbers (t.substring (2), a, b))
                    return { {}, "\"" + original + "\": write a CC as CC number=value, for example CC 34=127." };
                if (a > 127 || b > 127)
                    return { {}, "\"" + original + "\": CC numbers and values go from 0 to 127." };
                st.kind = Step::Kind::controller;
                st.number = a;
                st.value = b;
            }
            else if (t.startsWith ("pc") || t.startsWith ("program"))
            {
                const auto rest = t.startsWith ("pc") ? t.substring (2) : t.fromFirstOccurrenceOf ("program", false, false);
                if (! oneNumber (rest, a) || a < base || a > 127 + base)
                    return { {}, "\"" + original + "\": PC takes a number from " + juce::String (base) + " to "
                                 + juce::String (127 + base) + ", for example PC " + juce::String (base + 4) + "." };
                st.kind = Step::Kind::program;
                st.number = a - base;
            }
            else
            {
                return { {}, "Can't read \"" + original + "\". " + usage() };
            }
            r.steps.push_back (st);
        }

        if (r.steps.empty())
            r.error = "No MIDI messages yet. " + usage();
        return r;
    }

    juce::String describe (const Parsed& p, int programBase)
    {
        if (! p.error.isEmpty())
            return p.error;
        juce::StringArray parts;
        for (const auto& st : p.steps)
        {
            if (st.kind == Step::Kind::program)
                parts.add ("PC " + juce::String (st.number + (programBase == 1 ? 1 : 0)));
            else if (st.bank)
                parts.add ("bank " + juce::String (st.value));
            else
                parts.add ("CC#" + juce::String (st.number) + " = " + juce::String (st.value));
        }
        return parts.joinIntoString (", ");
    }

    Cue cue (int channel, const juce::String& name, const juce::String& text, int programBase)
    {
        Cue c;
        c.name = name;
        const auto ch = juce::jlimit (1, 16, channel);
        for (const auto& st : parse (text, programBase).steps)
        {
            if (st.kind == Step::Kind::program)
                c.add (0.0, juce::MidiMessage::programChange (ch, st.number));
            else
                c.add (0.0, juce::MidiMessage::controllerEvent (ch, st.number, st.value));
        }
        // A CC-only tile may be a press or a toggle (a Boss CC, a block toggle): never send it twice when the clip
        // is padded for Ableton. Tiles with a Program Change keep repeating their bank CC (lengthPadding).
        c.toggles = std::none_of (c.events.begin(), c.events.end(), [] (const auto& e) { return e.second.isProgramChange(); });
        return c;
    }
}

} // namespace cues
