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

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& tip, int width, int height)
{
    g.fillAll (findColour (juce::TooltipWindow::backgroundColourId));
    g.setColour (outline);
    g.drawRect (0, 0, width, height);
    g.setColour (text);
    g.setFont (font (13.0f));
    g.drawFittedText (tip, juce::Rectangle<int> (width, height).reduced (8, 4), juce::Justification::centredLeft, 4);
}

} // namespace theme
