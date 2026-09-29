#include "CueModel.h"

#include <cmath>

namespace cues
{

static constexpr int ticksPerQuarter = 960;

juce::File writeMidiFile (const Cue& cue, double bpm)
{
    const auto safeBpm = juce::jlimit (20.0, 400.0, bpm);

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, cue.name), 0.0); // track name
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / safeBpm)), 0.0);
    seq.addEvent (juce::MidiMessage::textMetaEvent (6, cue.name), 0.0); // marker, visible in MIDI editors

    for (const auto& [beat, msg] : cue.events)
        seq.addEvent (msg, beat * ticksPerQuarter);

    seq.addEvent (juce::MidiMessage::endOfTrack(), cue.lengthBeats * ticksPerQuarter);

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
namespace qc
{
    static int clampChannel (int ch) { return juce::jlimit (1, 16, ch); }

    juce::String letter (int i)
    {
        return juce::String::charToString ((juce::juce_wchar) ('A' + juce::jlimit (0, 7, i)));
    }

    juce::String location (int setlist, int bank, int slot)
    {
        return "SL" + juce::String (setlist) + " | " + juce::String (bank) + letter (slot);
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
            c.add (beat, juce::MidiMessage::controllerEvent (ch, cc::setlist, juce::jlimit (0, 127, setlist - 1)));

        c.add (beat, juce::MidiMessage::programChange (ch, index % 128));
    }

    Cue scene (int channel, int sceneIndex, const juce::String& label)
    {
        Cue c;
        c.name = "QC Scene " + letter (sceneIndex) + (label.isNotEmpty() ? " - " + label : juce::String());
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

    Cue stomp (int channel, int footswitch, bool on, const juce::String& label)
    {
        Cue c;
        c.name = "QC " + (label.isNotEmpty() ? label : "Stomp " + letter (footswitch)) + (on ? " On" : " Off");
        c.add (0.0, juce::MidiMessage::controllerEvent (clampChannel (channel),
                                                        cc::stompA + juce::jlimit (0, 7, footswitch),
                                                        on ? 127 : 0));
        return c;
    }

    Cue gigMode (int channel, int mode)
    {
        static const char* names[] = { "Preset", "Scene", "Stomp" };
        const auto m = juce::jlimit (0, 2, mode);

        Cue c;
        c.name = juce::String ("QC ") + names[m] + " Mode";
        c.add (0.0, juce::MidiMessage::controllerEvent (clampChannel (channel), cc::gigMode, m));
        return c;
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
        auto eased = [curve] (double x) { return std::pow (juce::jlimit (0.0, 1.0, x), curve); };

        switch (s)
        {
            case Shape::rampUp:   return eased (t);
            case Shape::rampDown: return 1.0 - eased (t);
            case Shape::swell:    return t < 0.5 ? eased (t * 2.0) : 1.0 - eased ((t - 0.5) * 2.0);
            case Shape::dive:     return std::pow (juce::jlimit (0.0, 1.0, t), 3.0 * curve);
            case Shape::trill:
            {
                const auto sixteenth = (int) std::floor (t * lengthBeats * 4.0 + 1.0e-9);
                return (sixteenth % 2) == 0 ? 1.0 : 0.0;
            }
            case Shape::bendToBar:
            {
                const auto start = juce::jmax (0.0, 1.0 - 1.0 / lengthBeats);
                return t < start ? 0.0 : eased ((t - start) / (1.0 - start));
            }
            case Shape::toe:  return 1.0;
            case Shape::heel: return 0.0;
        }
        return 0.0;
    }

    Cue sweep (int channel, Shape s, double lengthBeats, double curve, bool resetToHeel)
    {
        const auto ch = juce::jlimit (1, 16, channel);
        const auto len = juce::jmax (0.125, lengthBeats);
        constexpr int stepsPerBeat = 32;
        const auto steps = juce::jmax (1, juce::roundToInt (len * stepsPerBeat));

        Cue c;
        c.name = "Whammy " + shapeName (s) + " " + formatBeats (len);

        int last = -1;
        for (int i = 0; i <= steps; ++i)
        {
            const auto beat = len * i / steps;
            const auto t = (double) i / steps;
            const auto v = juce::jlimit (0, 127, juce::roundToInt (shapeValue (s, t, len, curve) * 127.0));

            // Hold the trill's last state until the end rather than toggling on the final tick.
            if (s == Shape::trill && i == steps)
                break;

            if (v != last)
            {
                c.add (beat, juce::MidiMessage::controllerEvent (ch, 11, v));
                last = v;
            }
        }

        c.lengthBeats = len + 1.0 / 32.0;

        if (resetToHeel && last != 0)
        {
            c.add (len + 1.0 / 16.0, juce::MidiMessage::controllerEvent (ch, 11, 0));
            c.lengthBeats = len + 1.0 / 8.0;
        }

        return c;
    }
}

} // namespace cues
