#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <juce_graphics/juce_graphics.h>

#include <vector>

// Ready-made custom MIDI devices (beta) for units whose MIDI numbers you assign yourself (Fractal Axe-Fx III / FM9 / FM3 /
// VP4, Boss GT-1000), built from each manufacturer's manual.
// Picking one creates an ordinary custom device the player can edit and share. Nothing here is tested on hardware:
// every template says so (disclaimer), and its notes give the manual and what to check on the unit.
namespace templates
{
struct TileDef
{
    juce::String name, messages, note;
    int colour = 5;   // palette index
};

struct GroupDef
{
    juce::String name;
    std::vector<TileDef> tiles;
};

struct Template
{
    juce::String id;          // stored on the device (IDs::templateId), survives renames: "line6.helix-floor"
    juce::String brand;       // "Line 6", "Fractal Audio"
    juce::String model;       // "Helix Floor"
    juce::String aliases;     // extra search words: "axe fx 3 iii"
    juce::Colour colour;
    juce::String notes;       // About this unit (read-only, always from the app): manual, what to set on the unit, connection facts
    int programBase = 0;      // how the unit numbers Program Changes (Boss: PC#1-128); the tiles' "PC n" use it
    int expCc = 11;           // the CC its Expression view starts on
    std::vector<GroupDef> groups;
};

const std::vector<Template>& all();
const Template* find (const juce::String& id);

// The device card's amber line, e.g. "From the Helix Floor manual (firmware 3.80). Not tested on hardware: check the numbers on your unit."
juce::String disclaimer (const Template&);

// A new custom device (IDs::Unit) filled from the template.
juce::ValueTree createUnit (const Template&);
}
