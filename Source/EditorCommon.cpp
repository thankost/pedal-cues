#include "EditorCommon.h"

using namespace theme;

namespace ui
{

Section::Section (const juce::String& id, const juce::String& t, const juce::String& h, juce::Colour a)
    : title (t), hint (h), accentColour (a)
{
    setComponentID (id);
    setInterceptsMouseClicks (false, false);
}

void Section::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (0.5f);
    drawCard (g, b, fill, 14.0f);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (padding + 4, 0);

    g.setColour (accentColour);
    g.fillRoundedRectangle ((float) header.getX(), (float) header.getCentreY() - 6.0f, 3.0f, 12.0f, 1.5f);
    header.removeFromLeft (10);

    const juce::Font titleFont (font (12.0f, true));
    const auto titleText = title.toUpperCase();
    const auto titleWidth = juce::roundToInt (juce::GlyphArrangement::getStringWidth (titleFont, titleText)) + 12;
    g.setColour (text);
    g.setFont (titleFont);
    g.drawText (titleText, header.removeFromLeft (titleWidth), juce::Justification::centredLeft);

    g.setColour (dim);
    g.setFont (font (12.0f));
    g.drawText (hint, header, juce::Justification::centredLeft, true);
}

juce::Rectangle<int> Section::headerArea() const
{
    return getBounds().removeFromTop (headerHeight).reduced (padding, 6).withTrimmedLeft (getWidth() / 2);
}

juce::Rectangle<int> Section::contentArea() const
{
    auto r = getBounds().reduced (padding);
    r.removeFromTop (headerHeight - padding);
    return r;
}

//==============================================================================
juce::ValueTree nthOfType (const juce::ValueTree& parent, const juce::Identifier& type, int n)
{
    int count = 0;
    for (auto child : parent)
        if (child.hasType (type) && count++ == n)
            return child;
    return {};
}

juce::Colour paletteColour (int index)
{
    const auto& p = state::palette();
    return juce::Colour (p.getReference (((index % p.size()) + p.size()) % p.size()).argb);
}

juce::PopupMenu colourMenu (juce::Colour current, int baseId)
{
    juce::PopupMenu m;
    const auto& p = state::palette();
    for (int i = 0; i < p.size(); ++i)
    {
        const juce::Colour c (p.getReference (i).argb);
        m.addColouredItem (baseId + i, p.getReference (i).name, c, true, c == current);
    }
    return m;
}

void layoutGrid (juce::OwnedArray<Tile>& tiles, juce::Rectangle<int> area, int columns, int gap, int first, int count)
{
    if (count < 0)
        count = tiles.size() - first;
    if (count <= 0 || columns <= 0)
        return;

    const auto rows = (count + columns - 1) / columns;
    const auto cellW = (area.getWidth() + gap) / columns;
    const auto cellH = (area.getHeight() + gap) / rows;

    for (int i = 0; i < count; ++i)
        if (auto* t = tiles[first + i])
            t->setBounds (area.getX() + (i % columns) * cellW, area.getY() + (i / columns) * cellH,
                          cellW - gap, cellH - gap);
}

void styleCaption (juce::Label& l, const juce::String& t)
{
    l.setText (t, juce::dontSendNotification);
    l.setFont (font (12.0f, true));
    l.setColour (juce::Label::textColourId, dim);
}

//==============================================================================
void askText (const juce::String& title, const juce::String& current, std::function<void (const juce::String&)> done)
{
    auto* w = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("text", current);
    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, done] (int result)
    {
        if (result == 1)
            done (w->getTextEditorContents ("text").trim());
    }), true);
}

void renameNode (juce::ValueTree node, const juce::String& title)
{
    askText (title, node[IDs::name].toString(), [node] (const juce::String& t) mutable
    {
        if (t.isNotEmpty())
            node.setProperty (IDs::name, t, nullptr);
    });
}

void editPresetDialog (juce::ValueTree preset)
{
    auto* w = new juce::AlertWindow ("Edit preset",
                                     "Name it, and tell PedalCues where it lives on the Quad Cortex\n"
                                     "(setlist, bank and slot exactly as shown on the pedal).",
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", preset[IDs::name].toString(), "Name");
    w->addTextEditor ("setlist", preset[IDs::setlist].toString(), "Setlist number");
    w->addTextEditor ("bank", preset[IDs::bank].toString(), "Bank (1-32)");

    juce::StringArray slots;
    for (int i = 0; i < 8; ++i)
        slots.add (cues::qc::letter (i));

    w->addComboBox ("slot", slots, "Slot");
    w->getComboBoxComponent ("slot")->setSelectedItemIndex ((int) preset[IDs::slot], juce::dontSendNotification);

    w->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    w->enterModalState (true, juce::ModalCallbackFunction::create ([w, preset] (int result) mutable
    {
        if (result != 1)
            return;

        const auto name = w->getTextEditorContents ("name").trim();
        if (name.isNotEmpty())
            preset.setProperty (IDs::name, name, nullptr);

        preset.setProperty (IDs::setlist, juce::jlimit (1, 128, w->getTextEditorContents ("setlist").getIntValue()), nullptr);
        preset.setProperty (IDs::bank,    juce::jlimit (1, 32,  w->getTextEditorContents ("bank").getIntValue()), nullptr);
        preset.setProperty (IDs::slot,    juce::jmax (0, w->getComboBoxComponent ("slot")->getSelectedItemIndex()), nullptr);
    }), true);
}

} // namespace ui
