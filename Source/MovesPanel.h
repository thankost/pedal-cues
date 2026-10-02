#pragma once

#include "EditorCommon.h"

#include <functional>
#include <memory>
#include <vector>

namespace ui
{
// What a moves card automates. The Whammy treadle (CC#11) and the QC's expression pedals (CC#1 / CC#2)
// share the same card: ready-made shapes or a freehand drawing, with a length and a curve, as tiles.
struct MovesConfig
{
    juce::String idPrefix;                 // component IDs <prefix>.sweeps, .sweepControls, .draw, .drawMode
    juce::String title, hint;
    juce::Colour colour;                   // card accent, buttons
    juce::Colour line;                     // curves on the tiles and the pad

    juce::Identifier beatsId, curveId, resetId, drawId, drawingId;

    juce::String resetText, resetTooltip;
    juce::String padHint, padTooltip;      // shown on an empty pad / its tooltip
    juce::String drawnTooltip;
    juce::String tileNote;                 // appended to every move tile's tooltip
    juce::String shapesTooltip, drawTooltip;

    int numShapes = 0;
    int shapeRows = 1;
    int extraHeaderWidth = 0;              // room left of Shapes / Draw for the owner's controls (extraHeader())
    int extraRowHeight = 0;                // a row above Length / Curve for the owner's controls (extraRow())
    std::function<juce::String (int)> shapeName, shapeDescription;
    std::function<bool (int)> shapeHolds;  // "hold 1 bar" rather than "1 bar"
    std::function<cues::Cue (int)> makeShape;
    std::function<cues::Cue (const std::vector<float>&)> makeDrawn;
    std::function<cues::Cue (cues::Cue)> finish;   // optional, applied to what's dragged or played (not the preview curve)
};

class MovesPanel final : public juce::Component
{
public:
    MovesPanel (PedalCuesProcessor&, MovesConfig);
    ~MovesPanel() override;

    void refresh();
    void resized() override;
    void setHint (const juce::String& hint)   { section.hint = hint; section.repaint(); }

    juce::Component& extraHeader()  { return header; }
    juce::Component& extraRow()     { return row; }

    // Height of the controls row plus the gap below it, inside the card.
    static constexpr int controlsHeight = 42;

private:
    class DrawPad;

    void updateDrawTile();
    cues::Cue finished (cues::Cue c) const   { return config.finish ? config.finish (std::move (c)) : c; }
    void commitDrawing();

    PedalCuesProcessor& proc;
    juce::ValueTree state;
    const MovesConfig config;

    Section section;
    juce::Component controls, header, row;
    juce::Label lengthLabel, curveLabel;
    juce::ComboBox lengthBox;
    juce::Slider curveSlider;
    juce::ToggleButton resetToggle;
    juce::TextButton shapesButton { "Shapes" }, drawButton { "Draw" };
    juce::TextButton clearButton { "Clear" }, smoothButton { "Smooth" };
    std::unique_ptr<DrawPad> pad;
    std::unique_ptr<Tile> drawTile;
    juce::OwnedArray<Tile> shapeTiles;
};
} // namespace ui
