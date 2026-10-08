#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace theme
{
inline const juce::Colour background { 0xff0d0e11 };
inline const juce::Colour surface    { 0xff16181d };
inline const juce::Colour surfaceHi  { 0xff1e2128 };
inline const juce::Colour raised     { 0xff282c35 };
inline const juce::Colour outline    { 0xff2e333d };
inline const juce::Colour text       { 0xffeef0f4 };
inline const juce::Colour dim        { 0xff8a909c };
inline const juce::Colour accent     { 0xfff5a524 };
inline const juce::Colour qcBlue     { 0xff5ac8fa };
inline const juce::Colour whammyRed  { 0xffd7263d };
inline const juce::Colour ledGreen   { 0xff3ddc84 };
inline const juce::Colour kemperGreen { 0xff35c46a };   // the Kemper page's accent

juce::FontOptions font (float height, bool bold = false);
void drawCard (juce::Graphics&, juce::Rectangle<float>, juce::Colour fill, float radius = 12.0f);

/** The PedalCues logo (same artwork as the app icon). withMargin = full 1024 icon canvas incl. shadow margin. */
void drawAppIcon (juce::Graphics&, juce::Rectangle<float> bounds, bool withMargin);

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height, float sliderPos,
                           float minSliderPos, float maxSliderPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;
};
} // namespace theme
