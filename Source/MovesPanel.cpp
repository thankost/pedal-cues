#include "MovesPanel.h"

#include <algorithm>
#include <array>
#include <cmath>

using namespace theme;

namespace ui
{
namespace
{
constexpr std::array<double, 12> lengths { 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 24.0, 32.0 };

void curveOf (Tile& t, const cues::Cue& cue)
{
    t.curve.clear();
    for (const auto& [beat, msg] : cue.events)
        if (msg.isController())
            t.curve.push_back ({ (float) (beat / cue.lengthBeats), (float) msg.getControllerValue() / 127.0f });
}
} // namespace

//==============================================================================
// Freehand canvas: drag to draw heel (bottom) to toe (top) across the move's length.
class MovesPanel::DrawPad final : public juce::Component,
                                  public juce::SettableTooltipClient
{
public:
    DrawPad (const MovesConfig& c) : config (c)
    {
        setComponentID (config.idPrefix + ".draw");
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        setTooltip (config.padTooltip);
    }

    std::vector<float> points;
    double beats = 4.0;
    std::function<void()> onChange, onCommit;

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (background);
        g.fillRoundedRectangle (b, 8.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);

        const auto plot = plotArea();

        for (auto frac : { 0.25f, 0.5f, 0.75f })
        {
            g.setColour (outline.withAlpha (frac == 0.5f ? 1.0f : 0.6f));
            g.drawHorizontalLine ((int) (plot.getY() + plot.getHeight() * frac), plot.getX(), plot.getRight());
        }

        // Beat and bar lines (4/4).
        const auto wholeBeats = (int) std::floor (beats);
        if (wholeBeats >= 1 && wholeBeats <= 64)
        {
            for (int k = 1; k < (int) std::ceil (beats); ++k)
            {
                const auto x = plot.getX() + plot.getWidth() * (float) (k / beats);
                const auto bar = k % 4 == 0;
                if (! bar && beats > 16.0)
                    continue;
                g.setColour (bar ? dim.withAlpha (0.45f) : outline.withAlpha (0.7f));
                g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            }
        }

        g.setColour (dim.withAlpha (0.8f));
        g.setFont (font (10.0f, true));
        g.drawText ("TOE", juce::Rectangle<float> (b.getX() + 6.0f, plot.getY() - 6.0f, 36.0f, 12.0f), juce::Justification::centredLeft);
        g.drawText ("HALF", juce::Rectangle<float> (b.getX() + 6.0f, plot.getCentreY() - 6.0f, 36.0f, 12.0f), juce::Justification::centredLeft);
        g.drawText ("HEEL", juce::Rectangle<float> (b.getX() + 6.0f, plot.getBottom() - 6.0f, 36.0f, 12.0f), juce::Justification::centredLeft);

        if (points.size() < 2)
            return;

        juce::Path line, fill;
        for (size_t i = 0; i < points.size(); ++i)
        {
            const auto x = plot.getX() + plot.getWidth() * (float) i / (float) (points.size() - 1);
            const auto y = plot.getBottom() - plot.getHeight() * points[i];
            if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, plot.getBottom()); }
            else          line.lineTo (x, y);
            fill.lineTo (x, y);
        }
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.closeSubPath();

        g.setGradientFill (juce::ColourGradient (config.colour.withAlpha (0.35f), 0.0f, plot.getY(),
                                                 config.colour.withAlpha (0.03f), 0.0f, plot.getBottom(), false));
        g.fillPath (fill);
        g.setColour (config.line);
        g.strokePath (line, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (std::all_of (points.begin(), points.end(), [] (float v) { return v <= 0.0f; }))
        {
            g.setColour (dim);
            g.setFont (font (13.0f, true));
            g.drawText (config.padHint, plot, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override  { lastIndex = -1; apply (e); }
    void mouseDrag (const juce::MouseEvent& e) override  { apply (e); }
    void mouseUp (const juce::MouseEvent&) override      { if (onCommit) onCommit(); }

private:
    juce::Rectangle<float> plotArea() const { return getLocalBounds().toFloat().reduced (10.0f, 12.0f).withTrimmedLeft (34.0f); }

    void apply (const juce::MouseEvent& e)
    {
        if (points.size() < 2)
            return;

        const auto plot = plotArea();
        const auto n = (int) points.size();
        const auto index = juce::jlimit (0, n - 1, juce::roundToInt ((e.position.x - plot.getX()) / plot.getWidth() * (float) (n - 1)));
        auto value = juce::jlimit (0.0f, 1.0f, (plot.getBottom() - e.position.y) / plot.getHeight());
        if (e.mods.isShiftDown())
            value = std::round (value * 4.0f) / 4.0f;

        if (lastIndex < 0 || lastIndex == index)
        {
            points[(size_t) index] = value;
        }
        else
        {
            const auto step = index > lastIndex ? 1 : -1;
            for (int i = lastIndex; i != index + step; i += step)
            {
                const auto t = (float) (i - lastIndex) / (float) (index - lastIndex);
                points[(size_t) i] = lastValue + (value - lastValue) * t;
            }
        }

        lastIndex = index;
        lastValue = value;
        repaint();
        if (onChange)
            onChange();
    }

    const MovesConfig& config;
    int lastIndex = -1;
    float lastValue = 0.0f;
};

//==============================================================================
// Draw > Wave...: a sine, triangle, square or saw (like Reaper's CC LFO) written into the drawing as you set it.
class MovesPanel::WaveEditor final : public juce::Component
{
public:
    WaveEditor (WaveSettings start, juce::Colour colour, std::function<void (const WaveSettings&)> changed)
        : settings (start), onChange (std::move (changed))
    {
        for (int w = 0; w < cues::whammy::numWaves; ++w)
            typeBox.addItem (cues::whammy::waveName ((cues::whammy::Wave) w), w + 1);
        typeBox.setSelectedId (settings.type + 1, juce::dontSendNotification);
        typeBox.onChange = [this] { settings.type = typeBox.getSelectedId() - 1; update(); };
        addAndMakeVisible (typeBox);

        auto setup = [this, colour] (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& tip,
                                     double lo, double hi, double step, double value, const juce::String& suffix)
        {
            styleCaption (l, name);
            l.setTooltip (tip);
            addAndMakeVisible (l);
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
            s.setRange (lo, hi, step);
            s.setTextValueSuffix (suffix);
            s.setValue (value, juce::dontSendNotification);
            s.setColour (juce::Slider::trackColourId, colour);
            s.setTooltip (tip);
            s.onValueChange = [this] { read(); };
            addAndMakeVisible (s);
        };
        setup (cyclesSlider, cyclesLabel, "WAVES", "How many waves across the move's length (it follows Length, so it's in time)",
               0.5, 8.0, 0.5, settings.cycles, "");
        setup (phaseSlider, phaseLabel, "PHASE", "Where the wave starts: 0 = its low point, 180 = its high point", 0.0, 360.0, 15.0, settings.phase, " deg");
        setup (skewSlider, skewLabel, "SHAPE", "Tilts each wave: a triangle leans towards a saw, a sine rises fast and falls slowly "
               "(or the other way round), a square gets a shorter or longer high part. Saws don't use it.", -100.0, 100.0, 5.0, settings.shape * 100.0, " %");
        setup (lowSlider, lowLabel, "LOW", "The wave's lowest point (0 = heel)", 0.0, 100.0, 5.0, settings.low * 100.0, " %");
        setup (highSlider, highLabel, "HIGH", "The wave's highest point (100 = toe)", 0.0, 100.0, 5.0, settings.high * 100.0, " %");
        setup (growSlider, growLabel, "GROW", "Above 0: the waves build up from Low to full across the move. Below 0: they die away "
               "(Reaper calls it amp skew)", -100.0, 100.0, 5.0, settings.grow * 100.0, " %");
        setup (speedSlider, speedLabel, "SPEED", "Above 0: the waves speed up across the move. Below 0: they slow down "
               "(Reaper calls it frequency skew)", -100.0, 100.0, 5.0, settings.speed * 100.0, " %");

        styleCaption (typeLabel, "TYPE");
        addAndMakeVisible (typeLabel);
        hint.setText ("Changes replace the drawing. Fix it by hand after, or save it in My drawings.", juce::dontSendNotification);
        hint.setFont (font (11.5f));
        hint.setColour (juce::Label::textColourId, dim);
        hint.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (hint);
        updateSkew();
        setSize (340, 330);
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12);
        auto rowOf = [&r] (juce::Label& l, juce::Component& c)
        {
            auto row = r.removeFromTop (30);
            l.setBounds (row.removeFromLeft (64));
            c.setBounds (row.reduced (0, 2));
            r.removeFromTop (4);
        };
        rowOf (typeLabel, typeBox);
        rowOf (cyclesLabel, cyclesSlider);
        rowOf (phaseLabel, phaseSlider);
        rowOf (skewLabel, skewSlider);
        rowOf (lowLabel, lowSlider);
        rowOf (highLabel, highSlider);
        rowOf (growLabel, growSlider);
        rowOf (speedLabel, speedSlider);
        hint.setBounds (r);
    }

private:
    void read()
    {
        settings.cycles = cyclesSlider.getValue();
        settings.phase = phaseSlider.getValue();
        settings.shape = skewSlider.getValue() / 100.0;
        settings.grow = growSlider.getValue() / 100.0;
        settings.speed = speedSlider.getValue() / 100.0;
        settings.low = lowSlider.getValue() / 100.0;
        settings.high = highSlider.getValue() / 100.0;
        update();
    }

    void update()
    {
        updateSkew();
        if (onChange)
            onChange (settings);
    }

    void updateSkew()
    {
        const auto saw = settings.type == (int) cues::whammy::Wave::sawUp || settings.type == (int) cues::whammy::Wave::sawDown;
        skewSlider.setEnabled (! saw);
        skewLabel.setAlpha (saw ? 0.4f : 1.0f);
    }

    WaveSettings settings;
    std::function<void (const WaveSettings&)> onChange;
    juce::ComboBox typeBox;
    juce::Slider cyclesSlider, phaseSlider, skewSlider, lowSlider, highSlider, growSlider, speedSlider;
    juce::Label typeLabel, cyclesLabel, phaseLabel, skewLabel, lowLabel, highLabel, growLabel, speedLabel, hint;
};

//==============================================================================
MovesPanel::MovesPanel (PedalCuesProcessor& p, MovesConfig c)
    : proc (p), state (p.state), config (std::move (c)),
      section (config.idPrefix + ".sweeps", config.title, config.hint, config.colour)
{
    pad = std::make_unique<DrawPad> (config);
    drawings.addListener (this);   // another panel or PedalCues instance saved, renamed or deleted one

    addAndMakeVisible (section);
    addAndMakeVisible (controls);
    for (auto* slot : { &header, &row })
    {
        slot->setInterceptsMouseClicks (false, true);
        addAndMakeVisible (slot);
    }
    controls.setComponentID (config.idPrefix + ".sweepControls");
    controls.setInterceptsMouseClicks (false, true);

    for (size_t i = 0; i < lengths.size(); ++i)
        lengthBox.addItem (cues::formatBeats (lengths[i]), (int) i + 1);
    lengthBox.onChange = [this]
    {
        const auto i = lengthBox.getSelectedItemIndex();
        if (i >= 0)
            state.setProperty (config.beatsId, lengths[(size_t) i], nullptr);
    };

    curveSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    curveSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 20);
    curveSlider.setRange (0.25, 4.0, 0.01);
    curveSlider.setSkewFactorFromMidPoint (1.0);
    curveSlider.setColour (juce::Slider::trackColourId, config.colour);
    curveSlider.setTooltip ("1 = linear, below 1 = fast start, above 1 = slow start");
    curveSlider.onValueChange = [this] { state.setProperty (config.curveId, curveSlider.getValue(), nullptr); };

    styleCaption (lengthLabel, "LENGTH");
    styleCaption (curveLabel, "CURVE");
    resetToggle.setButtonText (config.resetText);
    resetToggle.setTooltip (config.resetTooltip);
    resetToggle.setColour (juce::ToggleButton::tickColourId, config.line);
    resetToggle.onClick = [this] { state.setProperty (config.resetId, resetToggle.getToggleState(), nullptr); };

    for (auto* comp : std::initializer_list<juce::Component*> { &lengthLabel, &lengthBox, &curveLabel, &curveSlider, &resetToggle })
        addAndMakeVisible (comp);

    // Draw mode: Wave... takes Curve's place (Curve only shapes the ready-made moves).
    waveButton.setComponentID (config.idPrefix + ".wave");
    waveButton.setColour (juce::TextButton::buttonColourId, surface);
    waveButton.setColour (juce::TextButton::textColourOffId, text);
    waveButton.setTooltip ("Make a wave: sine, triangle, square or saw, with how many waves, phase, shape, range, grow and speed "
                           "(like Reaper's CC LFO)");
    waveButton.onClick = [this] { showWaveEditor(); };
    addChildComponent (waveButton);
    importButton.setComponentID (config.idPrefix + ".importMidi");
    importButton.setColour (juce::TextButton::buttonColourId, surface);
    importButton.setColour (juce::TextButton::textColourOffId, text);
    importButton.setTooltip ("Load a move from a MIDI file, e.g. a clip with CC automation you drew in your DAW (or drop the .mid "
                             "file on this card). Then Save it in My drawings to reuse it in any song.");
    importButton.onClick = [this] { chooseMidiFile(); };
    addChildComponent (importButton);

    // Shapes / Draw switch in the card header.
    const auto onText = config.colour.getPerceivedBrightness() > 0.6f ? juce::Colours::black : juce::Colours::white;
    for (auto* b : { &shapesButton, &drawButton })
    {
        b->setClickingTogglesState (true);
        b->setRadioGroupId (4301);
        b->setColour (juce::TextButton::buttonColourId, surface);
        b->setColour (juce::TextButton::buttonOnColourId, config.colour);
        b->setColour (juce::TextButton::textColourOffId, dim);
        b->setColour (juce::TextButton::textColourOnId, onText);
        addAndMakeVisible (b);
    }
    shapesButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    drawButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    drawButton.setComponentID (config.idPrefix + ".drawMode");
    shapesButton.setTooltip (config.shapesTooltip);
    drawButton.setTooltip (config.drawTooltip);
    shapesButton.onClick = [this] { if (shapesButton.getToggleState()) state.setProperty (config.drawId, false, nullptr); };
    drawButton.onClick   = [this] { if (drawButton.getToggleState())   state.setProperty (config.drawId, true, nullptr); };

    for (auto* b : { &clearButton, &smoothButton })
    {
        b->setColour (juce::TextButton::buttonColourId, surface);
        b->setColour (juce::TextButton::textColourOffId, text);
        addChildComponent (b);
    }
    for (auto* b : { &saveButton, &moreButton })
    {
        b->setColour (juce::TextButton::buttonColourId, surface);
        b->setColour (juce::TextButton::textColourOffId, text);
        addChildComponent (b);
    }
    saveButton.setTooltip ("Save this drawing in My drawings (it's kept with your setup)");
    moreButton.setTooltip ("Save as new, rename or delete a saved drawing");
    saveButton.onClick = [this] { saveDrawing(); };
    moreButton.onClick = [this] { showDrawingMenu(); };

    libraryBox.setTextWhenNothingSelected ("My drawings");
    libraryBox.setTextWhenNoChoicesAvailable ("No saved drawings");
    libraryBox.setTooltip ("Load one of your saved drawings to use or edit it. Shared by the Whammy and the QC expression.");
    libraryBox.onChange = [this] { loadDrawing (libraryBox.getSelectedId()); };
    addChildComponent (libraryBox);

    clearButton.setTooltip ("Reset the drawing to heel");
    smoothButton.setTooltip ("Round off sharp edges in the drawing");
    clearButton.onClick = [this]
    {
        std::fill (pad->points.begin(), pad->points.end(), 0.0f);
        commitDrawing();
    };
    smoothButton.onClick = [this]
    {
        auto& v = pad->points;
        for (int pass = 0; pass < 2 && v.size() > 2; ++pass)
        {
            const auto copy = v;
            for (size_t i = 1; i + 1 < v.size(); ++i)
                v[i] = (copy[i - 1] + 2.0f * copy[i] + copy[i + 1]) * 0.25f;
        }
        commitDrawing();
    };

    pad->onChange = [this] { updateDrawTile(); };
    pad->onCommit = [this] { commitDrawing(); };
    addChildComponent (*pad);

    refresh();
}

MovesPanel::~MovesPanel()
{
    drawings.removeListener (this);
}

void MovesPanel::refresh()
{
    const auto beats = (double) state[config.beatsId];
    const auto drawMode = (bool) state[config.drawId];

    resetToggle.setToggleState ((bool) state[config.resetId], juce::dontSendNotification);
    curveSlider.setValue ((double) state[config.curveId], juce::dontSendNotification);

    int bestLength = 0;
    for (size_t i = 0; i < lengths.size(); ++i)
        if (std::abs (lengths[i] - beats) < std::abs (lengths[(size_t) bestLength] - beats))
            bestLength = (int) i;
    lengthBox.setSelectedItemIndex (bestLength, juce::dontSendNotification);

    (drawMode ? drawButton : shapesButton).setToggleState (true, juce::dontSendNotification);
    curveLabel.setVisible (! drawMode);
    curveSlider.setVisible (! drawMode);
    waveButton.setVisible (drawMode);
    importButton.setVisible (drawMode);

    pad->points = cues::whammy::decodeDrawing (state[config.drawingId].toString());
    pad->beats = beats;
    pad->setVisible (drawMode);
    for (auto* c : std::initializer_list<juce::Component*> { &clearButton, &smoothButton, &saveButton, &moreButton, &libraryBox })
        c->setVisible (drawMode);

    // My drawings, with the loaded one selected.
    libraryBox.clear (juce::dontSendNotification);
    const auto saved = drawings;
    for (int i = 0; i < saved.getNumChildren(); ++i)
        libraryBox.addItem (saved.getChild (i)[IDs::name].toString(), i + 1);
    if (saved.getNumChildren() > 0)
        libraryBox.addSeparator();
    libraryBox.addItem ("New drawing", newDrawingId);
    for (int i = 0; i < saved.getNumChildren(); ++i)
        if (saved.getChild (i)[IDs::name].toString() == drawingName())
            libraryBox.setSelectedId (i + 1, juce::dontSendNotification);
    pad->repaint();

    drawTile.reset (new Tile (proc, Tile::Look::sweep));
    drawTile->title = drawingName().isNotEmpty() ? drawingName() : juce::String ("Drawn move");
    drawTile->colour = config.line;
    drawTile->setTooltip (config.drawnTooltip + config.tileNote);
    drawTile->makeCue = [this] { return finished (config.makeDrawn (pad->points, drawingName())); };
    addChildComponent (drawTile.get());
    drawTile->setVisible (drawMode);
    updateDrawTile();

    shapeTiles.clear();
    for (int s = 0; s < config.numShapes; ++s)
    {
        auto* t = shapeTiles.add (new Tile (proc, Tile::Look::sweep));
        t->makeCue = [this, s] { return finished (config.makeShape (s)); };
        curveOf (*t, config.makeShape (s));
        t->title = config.shapeName (s);
        t->subtitle = (config.shapeHolds && config.shapeHolds (s) ? "hold " : "") + cues::formatBeats (beats);
        t->colour = config.line;
        t->setTooltip (config.shapeDescription (s) + ". Drag onto the timeline where the move should start." + config.tileNote);
        addChildComponent (t);
        t->setVisible (! drawMode);
    }

    resized();
}

void MovesPanel::resized()
{
    section.setBounds (getLocalBounds());

    auto content = section.contentArea();
    if (config.extraRowHeight > 0)
    {
        row.setBounds (content.removeFromTop (config.extraRowHeight));
        content.removeFromTop (8);
    }

    auto controlsRow = content.withHeight (34);
    controls.setBounds (controlsRow);
    lengthLabel.setBounds (controlsRow.removeFromLeft (58));
    lengthBox.setBounds (controlsRow.removeFromLeft (110).reduced (0, 3));
    controlsRow.removeFromLeft (24);
    {
        auto curveArea = controlsRow.removeFromLeft (272);
        waveButton.setBounds (curveArea.withWidth (110).reduced (0, 3));
        importButton.setBounds (curveArea.withTrimmedLeft (118).withWidth (140).reduced (0, 3));
        curveLabel.setBounds (curveArea.removeFromLeft (52));
        curveSlider.setBounds (curveArea);
    }
    controlsRow.removeFromLeft (24);
    resetToggle.setBounds (controlsRow.removeFromLeft (260));

    {
        auto hdr = section.headerArea().withSizeKeepingCentre (section.headerArea().getWidth(), 28);
        drawButton.setBounds (hdr.removeFromRight (80));
        shapesButton.setBounds (hdr.removeFromRight (80));
        hdr.removeFromRight (16);
        header.setBounds (hdr.removeFromRight (config.extraHeaderWidth));
    }

    auto tiles = content.withTrimmedTop (controlsHeight).expanded (3);
    const auto rows = juce::jmax (1, config.shapeRows);
    const auto columns = (config.numShapes + rows - 1) / rows;
    layoutGrid (shapeTiles, tiles, columns, rows > 1 ? 6 : 0);

    auto area = tiles;
    const auto tileW = area.getWidth() / juce::jmax (1, columns);
    if (drawTile != nullptr)
        drawTile->setBounds (area.removeFromRight (tileW).withSizeKeepingCentre (tileW, juce::jmin (area.getHeight(), 160)));
    area.removeFromRight (6);
    {
        // My drawings, Save / ..., Clear / Smooth.
        auto buttons = area.removeFromRight (150).reduced (3, 0).withSizeKeepingCentre (144, 3 * 30 + 2 * 8);
        libraryBox.setBounds (buttons.removeFromTop (30));
        buttons.removeFromTop (8);
        auto saveRow = buttons.removeFromTop (30);
        moreButton.setBounds (saveRow.removeFromRight (36));
        saveRow.removeFromRight (6);
        saveButton.setBounds (saveRow);
        buttons.removeFromTop (8);
        auto editRow = buttons.removeFromTop (30);
        clearButton.setBounds (editRow.removeFromLeft ((editRow.getWidth() - 6) / 2));
        editRow.removeFromLeft (6);
        smoothButton.setBounds (editRow);
    }
    area.removeFromRight (6);
    pad->setBounds (area.reduced (3));
}

void MovesPanel::updateDrawTile()
{
    if (drawTile == nullptr)
        return;

    curveOf (*drawTile, config.makeDrawn (pad->points, drawingName()));
    drawTile->subtitle = cues::formatBeats ((double) state[config.beatsId]) + (drawingEdited() ? "  -  edited" : "");
    drawTile->repaint();
}

std::unique_ptr<juce::Component> MovesPanel::makeWaveEditor (WaveSettings s, juce::Colour colour)
{
    return std::make_unique<WaveEditor> (s, colour, nullptr);
}

void MovesPanel::showWaveEditor()
{
    juce::Component::SafePointer<MovesPanel> safe (this);
    auto editor = std::make_unique<WaveEditor> (wave, config.colour, [safe] (const WaveSettings& s)
    {
        if (safe != nullptr)
        {
            safe->wave = s;
            safe->applyWave();
        }
    });
    juce::CallOutBox::launchAsynchronously (std::move (editor), waveButton.getScreenBounds(), nullptr);
}

void MovesPanel::applyWave()
{
    pad->points = cues::whammy::waveDrawing ((cues::whammy::Wave) wave.type, wave.cycles, wave.phase, wave.shape, wave.low, wave.high,
                                             wave.grow, wave.speed);
    commitDrawing();
}

void MovesPanel::chooseMidiFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Import a move from a MIDI file", juce::File(), "*.mid;*.midi");
    juce::Component::SafePointer<MovesPanel> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [safe] (const juce::FileChooser& fc)
    {
        if (safe != nullptr && fc.getResult().existsAsFile())
            safe->importMidi (fc.getResult());
    });
}

bool MovesPanel::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).hasFileExtension ("mid;midi");
}

void MovesPanel::paintOverChildren (juce::Graphics& g)
{
    if (! fileHover)
        return;
    // A MIDI file held over the card: a light veil with a +, like dropping a file anywhere else.
    const auto b = getLocalBounds().toFloat().reduced (2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.drawRoundedRectangle (b.reduced (1.0f), 10.0f, 2.0f);
    auto centre = b.withSizeKeepingCentre (b.getWidth(), 110.0f);
    const auto plus = centre.removeFromTop (64.0f).withSizeKeepingCentre (64.0f, 64.0f);
    g.fillEllipse (plus);
    g.setColour (background);
    g.fillRect (plus.withSizeKeepingCentre (30.0f, 5.0f));
    g.fillRect (plus.withSizeKeepingCentre (5.0f, 30.0f));
    g.setColour (juce::Colours::white);
    g.setFont (font (16.0f, true));
    g.drawText ("Drop to import this move", centre.withTrimmedTop (10.0f), juce::Justification::centredTop);
}

void MovesPanel::filesDropped (const juce::StringArray& files, int, int)
{
    fileHover = false;
    repaint();
    if (isInterestedInFileDrag (files))
        importMidi (juce::File (files[0]));
}

void MovesPanel::importMidi (const juce::File& file)
{
    juce::MidiFile midi;
    juce::FileInputStream in (file);
    if (! in.openedOk() || ! midi.readFrom (in))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import MIDI", "That isn't a MIDI file PedalCues can read.");
        return;
    }
    const auto move = cues::whammy::importMove (midi, config.controller ? config.controller() : -1,
                                                std::vector<double> (lengths.begin(), lengths.end()));
    if (move.error.isNotEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Import MIDI", move.error);
        return;
    }

    // Into the pad, at the clip's length, then offered for My drawings under the file's name.
    state.setProperty (config.drawId, true, nullptr);
    state.setProperty (config.beatsId, move.beats, nullptr);
    pad->points = move.points;
    state.setProperty (config.drawingNameId, juce::String(), nullptr);
    commitDrawing();
    if (move.truncated)
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Import MIDI",
                                                "The clip is longer than 8 bars: PedalCues kept its first 8 bars.");
    saveDrawingAs (file.getFileNameWithoutExtension());
}

void MovesPanel::commitDrawing()
{
    pad->repaint();
    updateDrawTile();
    state.setProperty (config.drawingId, cues::whammy::encodeDrawing (pad->points), nullptr);
}

//==============================================================================
juce::String MovesPanel::drawingName() const
{
    const auto name = state[config.drawingNameId].toString();
    return state::findDrawing (drawings, name).isValid() ? name : juce::String();
}

bool MovesPanel::drawingEdited() const
{
    const auto saved = state::findDrawing (drawings, drawingName());
    // Compare at today's resolution: drawings saved before v0.8.5 have 64 points.
    return saved.isValid() && cues::whammy::encodeDrawing (cues::whammy::decodeDrawing (saved[IDs::points].toString()))
                                  != cues::whammy::encodeDrawing (pad->points);
}

void MovesPanel::loadDrawing (int itemId)
{
    if (itemId == newDrawingId)
    {
        std::fill (pad->points.begin(), pad->points.end(), 0.0f);
        state.setProperty (config.drawingNameId, juce::String(), nullptr);
        commitDrawing();
        return;
    }

    const auto d = drawings.getChild (itemId - 1);
    if (! d.isValid())
        return;

    pad->points = cues::whammy::decodeDrawing (d[IDs::points].toString());
    state.setProperty (config.drawingNameId, d[IDs::name], nullptr);
    commitDrawing();
}

void MovesPanel::saveDrawing()
{
    if (drawingName().isEmpty())
    {
        saveDrawingAs ("My move");
        return;
    }

    state::saveDrawing (drawings, drawingName(), cues::whammy::encodeDrawing (pad->points));
    state::storeMyDrawings();
}

void MovesPanel::saveDrawingAs (const juce::String& suggestion)
{
    juce::Component::SafePointer<MovesPanel> safe (this);
    askText ("Save drawing as", suggestion, [safe] (const juce::String& name)
    {
        if (safe == nullptr || name.isEmpty())
            return;

        state::saveDrawing (safe->drawings, name, cues::whammy::encodeDrawing (safe->pad->points));
        state::storeMyDrawings();
        safe->state.setProperty (safe->config.drawingNameId, name, nullptr);
    });
}

void MovesPanel::showDrawingMenu()
{
    const auto name = drawingName();

    juce::PopupMenu m;
    m.addItem (1, "Save as new drawing");
    m.addItem (2, name.isNotEmpty() ? "Rename \"" + name + "\"" : juce::String ("Rename"), name.isNotEmpty());
    m.addItem (3, name.isNotEmpty() ? "Delete \"" + name + "\"" : juce::String ("Delete"), name.isNotEmpty());

    juce::Component::SafePointer<MovesPanel> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&moreButton), [safe, name] (int result)
    {
        if (safe == nullptr)
            return;

        if (result == 1)
        {
            safe->saveDrawingAs (name.isNotEmpty() ? name + " 2" : juce::String ("My move"));
        }
        else if (result == 2)
        {
            askText ("Rename drawing", name, [safe, name] (const juce::String& to)
            {
                if (safe != nullptr && state::renameDrawing (safe->drawings, name, to))
                {
                    state::storeMyDrawings();
                    safe->state.setProperty (safe->config.drawingNameId, to, nullptr);
                }
            });
        }
        else if (result == 3)
        {
            juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                              .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                              .withTitle ("Delete drawing")
                                              .withMessage ("Delete \"" + name + "\" from My drawings? Clips you already dragged "
                                                            "onto the timeline keep working.")
                                              .withButton ("Delete")
                                              .withButton ("Cancel"),
                                          [safe, name] (int button)
            {
                if (safe == nullptr || button != 1)
                    return;
                state::deleteDrawing (safe->drawings, name);
                state::storeMyDrawings();
                safe->state.setProperty (safe->config.drawingNameId, juce::String(), nullptr);
            });
        }
    });
}
} // namespace ui
