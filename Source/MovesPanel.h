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
    juce::Identifier drawingNameId;        // name of the saved drawing loaded in the pad ("" = not saved)

    juce::String resetText, resetTooltip;
    juce::String padHint, padTooltip;      // unused since the pad has one editing mode (MovesPanel writes its own help)
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
    std::function<cues::Cue (const std::vector<float>&, const juce::String& name)> makeDrawn;
    std::function<cues::Cue (cues::Cue)> finish;   // optional, applied to what's dragged or played (not the preview curve)
    std::function<int()> controller;      // the CC this card moves (Import MIDI... reads it first); optional
};

class MovesPanel final : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::ValueTree::Listener,
                         private juce::AsyncUpdater
{
public:
    // Draw > Wave: the generated line's settings (kept while the panel lives).
    struct WaveSettings
    {
        int type = 0;                       // cues::whammy::Wave
        double cycles = 2.0, phase = 0.0, shape = 0.0, low = 0.0, high = 1.0, grow = 0.0, speed = 0.0;
    };

    MovesPanel (PedalCuesProcessor&, MovesConfig);
    ~MovesPanel() override;

    void refresh();
    void resized() override;
    void setHint (const juce::String& hint)   { section.hint = hint; section.repaint(); }
    void setTitle (const juce::String& title) { section.title = title; section.repaint(); }

    // The Draw > Wave... panel on its own (DocShots).
    static std::unique_ptr<juce::Component> makeWaveEditor (WaveSettings, juce::Colour);

    juce::Component& extraHeader()  { return header; }
    juce::Component& extraRow()     { return row; }

    // Height of the controls row plus the gap below it, inside the card.
    static constexpr int controlsHeight = 42;
    int extraControlsRow = 0;   // a second tools row when the panel is narrow
    static constexpr int newDrawingId = 10000;   // "New drawing" in My drawings

private:
    struct DrawDoc;
    class DrawPad;
    class WaveEditor;
    class LargeEditor;
    class LargeWindow;
    void showWaveEditor();   // above the Wave... button; a second click closes it
    void applyWave();
    void openLargeEditor();  // the pad in a resizable window, same drawing
    void setGrid (double beats);
    void clearDrawing();
    void smoothDrawing();
    void padChanged();       // a pad is being edited: the other pad and the tile follow
    // Draw > Import MIDI... (or a .mid dropped on the card): a move from a MIDI clip, to reuse in any song.
    void chooseMidiFile();
    void importMidi (const juce::File&);
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override  { fileHover = true; repaint(); }
    void fileDragExit (const juce::StringArray&) override             { fileHover = false; repaint(); }
    void filesDropped (const juce::StringArray&, int, int) override;
    void paintOverChildren (juce::Graphics&) override;
    bool fileHover = false;   // a .mid file is held over the card: show where it goes
    std::unique_ptr<juce::FileChooser> chooser;

    void updateDrawTile();
    void handleAsyncUpdate() override  { refresh(); }
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override        { triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { triggerAsyncUpdate(); }
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { triggerAsyncUpdate(); }
    void loadDrawing (int itemId);
    void saveDrawing();
    void saveDrawingAs (const juce::String& suggestion);
    void showDrawingMenu();
    juce::String drawingName() const;
    bool drawingEdited() const;
    cues::Cue finished (cues::Cue c) const   { return config.finish ? config.finish (std::move (c)) : c; }
    void commitDrawing (const juce::String& mergeKey = {});

    PedalCuesProcessor& proc;
    juce::ValueTree state;
    juce::ValueTree drawings { state::myDrawings() };   // My drawings, shared by every panel and project
    const MovesConfig config;

    Section section;
    juce::Component controls, header, row;
    juce::Label lengthLabel, curveLabel;
    juce::ComboBox lengthBox;
    juce::Slider curveSlider;
    juce::ToggleButton resetToggle;
    juce::TextButton shapesButton { "Shapes" }, drawButton { "Draw" };
    juce::TextButton clearButton { "Clear" }, smoothButton { "Smooth" };
    juce::Label gridLabel;
    juce::ComboBox gridBox;                // the pad's snap grid (Draw only)
    juce::TextButton waveButton { "Wave..." }, importButton { "Import MIDI..." };
    WaveSettings wave;
    juce::ComboBox libraryBox;             // My drawings
    juce::TextButton saveButton { "Save" }, moreButton { "..." };
    std::unique_ptr<DrawDoc> doc;          // the drawing's points, grid and undo history, shared by both pads
    std::unique_ptr<DrawPad> pad;
    std::unique_ptr<juce::Button> expandButton;
    std::unique_ptr<juce::DocumentWindow> largeWindow;
    LargeEditor* large = nullptr;          // largeWindow's content
    juce::Component::SafePointer<juce::CallOutBox> waveBox;
    int waveSession = 0;                   // each Wave panel opening is one undo step
    std::unique_ptr<Tile> drawTile;
    juce::OwnedArray<Tile> shapeTiles;
};
} // namespace ui
