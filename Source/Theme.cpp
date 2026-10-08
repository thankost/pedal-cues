#include "Theme.h"

namespace theme
{

juce::FontOptions font (float height, bool bold)
{
    return juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain);
}

void drawCard (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour fill, float radius)
{
    g.setColour (fill);
    g.fillRoundedRectangle (r, radius);
    g.setColour (outline);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}

LookAndFeel::LookAndFeel()
    : LookAndFeel_V4 (LookAndFeel_V4::ColourScheme (background, surface, surfaceHi, outline, text,
                                                    accent, juce::Colours::black, accent, text))
{
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::TextButton::buttonColourId, raised);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    setColour (juce::ComboBox::backgroundColourId, raised);
    setColour (juce::ComboBox::outlineColourId, outline);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::arrowColourId, dim);
    setColour (juce::PopupMenu::backgroundColourId, surfaceHi);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, raised);
    setColour (juce::TextEditor::outlineColourId, outline);
    setColour (juce::TextEditor::focusedOutlineColourId, accent);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::AlertWindow::backgroundColourId, surfaceHi);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, outline);
    setColour (juce::ScrollBar::thumbColourId, raised.brighter (0.25f));
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, raised);
    setColour (juce::Slider::thumbColourId, text);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, raised);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId, surfaceHi);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, outline);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, accent);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                        bool highlighted, bool down)
{
    auto c = bg;
    if (c.isTransparent())
    {
        if (! highlighted && ! down)
            return;
        c = raised;
    }

    if (down)
        c = c.darker (0.15f);
    else if (highlighted)
        c = c.brighter (0.12f);

    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (c);
    g.fillRoundedRectangle (r, juce::jmin (9.0f, r.getHeight() * 0.5f));
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return juce::Font (font (juce::jmin (14.0f, (float) buttonHeight * 0.5f), true));
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    const auto r = b.getLocalBounds().toFloat();
    constexpr float h = 18.0f, w = 32.0f;
    const juce::Rectangle<float> sw (r.getX() + 2.0f, r.getCentreY() - h * 0.5f, w, h);
    const auto on = b.getToggleState();

    auto track = on ? b.findColour (juce::ToggleButton::tickColourId) : raised.brighter (0.08f);
    if (highlighted)
        track = track.brighter (0.1f);

    g.setColour (track);
    g.fillRoundedRectangle (sw, h * 0.5f);

    const auto knob = juce::Rectangle<float> (h - 4.0f, h - 4.0f)
                          .withPosition (on ? sw.getRight() - h + 2.0f : sw.getX() + 2.0f, sw.getY() + 2.0f);
    g.setColour (on ? juce::Colours::black.withAlpha (0.85f) : text.withAlpha (0.9f));
    g.fillEllipse (knob);

    g.setColour (b.findColour (juce::ToggleButton::textColourId).withMultipliedAlpha (b.isEnabled() ? 1.0f : 0.5f));
    g.setFont (font (13.5f));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (w + 10.0f).toNearestInt(), juce::Justification::centredLeft, 2);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (box.findColour (juce::ComboBox::outlineColourId));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    const auto cx = (float) width - 16.0f, cy = (float) height * 0.5f;
    juce::Path chevron;
    chevron.startNewSubPath (cx - 4.0f, cy - 2.0f);
    chevron.lineTo (cx, cy + 2.0f);
    chevron.lineTo (cx + 4.0f, cy - 2.0f);
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (font (13.5f));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (10, 1, box.getWidth() - 34, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                                    float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle style,
                                    juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    const juce::Rectangle<float> track ((float) x, (float) y + (float) height * 0.5f - 2.0f, (float) width, 4.0f);
    g.setColour (slider.findColour (juce::Slider::backgroundColourId));
    g.fillRoundedRectangle (track, 2.0f);
    g.setColour (slider.findColour (juce::Slider::trackColourId));
    g.fillRoundedRectangle (track.withRight (sliderPos), 2.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (14.0f, 14.0f).withCentre ({ sliderPos, track.getCentreY() }));
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
    g.setColour (outline);
    g.drawRect (0, 0, width, height);
}

// Tooltips: sized and drawn from the same layout, so long ones (several lines, file lists) show in full.
namespace
{
constexpr float tooltipMaxWidth = 440.0f;
constexpr int tooltipPadX = 9, tooltipPadY = 6;

juce::TextLayout tooltipLayout (const juce::String& tip, juce::Colour colour)
{
    juce::AttributedString s;
    s.setJustification (juce::Justification::topLeft);
    s.append (tip, font (13.0f), colour);
    juce::TextLayout layout;
    layout.createLayoutWithBalancedLineLengths (s, tooltipMaxWidth);
    return layout;
}
} // namespace

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> screenPos, juce::Rectangle<int> parentArea)
{
    const auto layout = tooltipLayout (tip, text);
    const auto w = (int) std::ceil (layout.getWidth()) + tooltipPadX * 2;
    const auto h = (int) std::ceil (layout.getHeight()) + tooltipPadY * 2;
    return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                 screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
        .constrainedWithin (parentArea);
}

// Pop-out panels (Click..., Wave...): opaque, so what's behind doesn't show through their controls.
void LookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillPath (path, juce::AffineTransform::translation (0.0f, 3.0f));
    g.setColour (surface);
    g.fillPath (path);
    g.setColour (outline);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tip, int width, int height)
{
    g.fillAll (findColour (juce::TooltipWindow::backgroundColourId));
    g.setColour (outline);
    g.drawRect (0, 0, width, height);
    tooltipLayout (tip, text).draw (g, juce::Rectangle<int> (width, height).reduced (tooltipPadX, tooltipPadY).toFloat());
}


void drawAppIcon (juce::Graphics& g, juce::Rectangle<float> bounds, bool withMargin)
{
    // Artwork is authored on a 1024 canvas with an 824 px body starting at 100.
    const auto canvas = withMargin ? 1024.0f : 824.0f;
    const auto origin = withMargin ? 0.0f : 100.0f;
    const auto k = juce::jmin (bounds.getWidth(), bounds.getHeight()) / canvas;
    juce::Graphics::ScopedSaveState saved (g);
    g.addTransform (juce::AffineTransform::translation (-origin, -origin).scaled (k).translated (bounds.getX(), bounds.getY()));

    // macOS-style squircle: 824 px body on a 1024 canvas.
    const juce::Rectangle<float> body (100.0f, 100.0f, 824.0f, 824.0f);
    juce::Path shape;
    shape.addRoundedRectangle (body, 185.0f);

    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillPath (shape, juce::AffineTransform::translation (0.0f, 8.0f));

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2b303b), body.getX(), body.getY(),
                                             juce::Colour (0xff0c0d10), body.getX(), body.getBottom(), false));
    g.fillPath (shape);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.strokePath (shape, juce::PathStrokeType (4.0f));

    g.saveState();
    g.reduceClipRegion (shape);

    // Timeline lane
    const juce::Rectangle<float> lane (100.0f, 640.0f, 824.0f, 170.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (lane);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    for (float x = 170.0f; x < 924.0f; x += 131.0f)
        g.fillRect (x, lane.getY(), 3.0f, lane.getHeight());

    auto clip = [&g] (juce::Rectangle<float> r, juce::Colour c)
    {
        g.setColour (c.withAlpha (0.35f));
        g.fillRoundedRectangle (r, 18.0f);
        g.setColour (c);
        g.fillRoundedRectangle (r.withHeight (46.0f), 18.0f);
        g.fillRect (r.withHeight (46.0f).withTrimmedTop (23.0f));
        g.drawRoundedRectangle (r, 18.0f, 6.0f);
    };
    clip ({ 150.0f, 665.0f, 220.0f, 120.0f }, qcBlue);
    clip ({ 654.0f, 665.0f, 220.0f, 120.0f }, whammyRed);

    // Drop target
    const juce::Rectangle<float> target (402.0f, 665.0f, 220.0f, 120.0f);
    g.setColour (accent.withAlpha (0.18f));
    g.fillRoundedRectangle (target, 18.0f);
    juce::Path outline, dashed;
    outline.addRoundedRectangle (target, 18.0f);
    const float dashes[] = { 22.0f, 14.0f };
    juce::PathStrokeType (6.0f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (accent);
    g.fillPath (dashed);

    // Playhead
    g.setColour (accent);
    g.fillRect (512.0f - 3.0f, 600.0f, 6.0f, 324.0f);
    g.restoreState();

    // The tile being dropped: an amber footswitch cue, slightly tilted.
    const juce::Rectangle<float> tile (322.0f, 170.0f, 380.0f, 300.0f);
    const auto tilt = juce::AffineTransform::rotation (-0.10f, tile.getCentreX(), tile.getCentreY());
    juce::Path tilePath;
    tilePath.addRoundedRectangle (tile, 44.0f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillPath (tilePath, tilt.translated (10.0f, 22.0f));
    g.setGradientFill (juce::ColourGradient (accent.brighter (0.25f), tile.getX(), tile.getY(),
                                             accent.darker (0.35f), tile.getX(), tile.getBottom(), false));
    g.fillPath (tilePath, tilt);

    g.saveState();
    g.addTransform (tilt);
    const auto knob = juce::Rectangle<float> (0.0f, 0.0f, 170.0f, 170.0f).withCentre (tile.getCentre().translated (0.0f, 12.0f));
    g.setColour (juce::Colour (0xff15171c));
    g.fillEllipse (knob.expanded (18.0f));
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xffe8ebf0), knob.getX(), knob.getY(),
                                             juce::Colour (0xff7d838f), knob.getRight(), knob.getBottom(), false));
    g.fillEllipse (knob);
    g.setColour (juce::Colour (0xff15171c).withAlpha (0.35f));
    g.drawEllipse (knob.reduced (26.0f), 6.0f);
    // LED
    const auto led = juce::Rectangle<float> (0.0f, 0.0f, 34.0f, 34.0f).withCentre ({ tile.getCentreX(), tile.getY() + 42.0f });
    g.setColour (ledGreen.withAlpha (0.35f));
    g.fillEllipse (led.expanded (14.0f));
    g.setColour (ledGreen.brighter (0.3f));
    g.fillEllipse (led);
    g.restoreState();

    // Arrow into the gap
    juce::Path arrow;
    arrow.startNewSubPath (512.0f, 500.0f);
    arrow.lineTo (512.0f, 585.0f);
    g.setColour (juce::Colours::white);
    g.strokePath (arrow, juce::PathStrokeType (22.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path head;
    head.addTriangle (462.0f, 575.0f, 562.0f, 575.0f, 512.0f, 635.0f);
    g.fillPath (head);

}

} // namespace theme
