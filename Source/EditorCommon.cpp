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

    g.setColour (hintColour);
    g.setFont (font (12.0f));
    g.drawText (hint, header, juce::Justification::centredLeft, true);
}

SaveIndicator::SaveIndicator (PedalCuesProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (true, false);
    startTimerHz (20);
}

// Worked out on hover: the standalone app switches on its own saving after the window is built.
juce::String SaveIndicator::getTooltip()
{
    return proc.autoSave.writesToDisk()
               ? "Your devices, presets, songs and settings are saved automatically by PedalCues. Nothing to do."
               : "Your devices, presets, songs and settings are saved automatically by PedalCues, with your DAW "
                 "project: just save the project as usual.";
}

float SaveIndicator::wordsAlpha() const
{
    if (settledForScreenshots)
        return 0.0f;
    if (proc.autoSave.isPending())
        return 1.0f;
    const auto since = juce::Time::getMillisecondCounterHiRes() - savedAt;
    if (savedAt <= 0.0 || since > 2600.0)
        return 0.0f;
    return since < 2000.0 ? 1.0f : (float) (1.0 - (since - 2000.0) / 600.0);
}

void SaveIndicator::timerCallback()
{
    if (proc.autoSave.isPending() != shownPending)
    {
        shownPending = proc.autoSave.isPending();
        if (! shownPending)
            savedAt = juce::Time::getMillisecondCounterHiRes();
        repaint();
    }
    if (savedAt > 0.0 && juce::Time::getMillisecondCounterHiRes() - savedAt < 2800.0)
        repaint();   // the fade
}

void SaveIndicator::paint (juce::Graphics& g)
{
    const auto pending = proc.autoSave.isPending() && ! settledForScreenshots;
    const auto words = pending ? juce::String ("Saving...")
                               : proc.autoSave.writesToDisk() ? juce::String ("Saved") : juce::String ("In your project");
    const auto textFont = juce::Font (theme::font (12.5f));
    auto b = getLocalBounds().toFloat();
    if (alignRight && ! compact)   // icon and words together at the right edge
        b = b.removeFromRight (juce::jmin (b.getWidth(), 24.0f + juce::GlyphArrangement::getStringWidth (textFont, words)));
    const auto icon = b.removeFromLeft (juce::jmin (b.getHeight(), 18.0f)).withSizeKeepingCentre (14.0f, 14.0f);
    // Always there, quiet: bright while saving and just after, then dim.
    const auto emphasis = wordsAlpha();
    g.setColour (pending ? theme::accent : theme::ledGreen.withAlpha (0.55f + 0.45f * emphasis));
    if (pending)
    {
        // Three dots: saving.
        for (int i = 0; i < 3; ++i)
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ icon.getX() + 2.5f + i * 4.5f, icon.getCentreY() }));
    }
    else
    {
        g.drawEllipse (icon.reduced (0.75f), 1.5f);
        juce::Path tick;
        tick.startNewSubPath (icon.getX() + 3.8f, icon.getCentreY() + 0.2f);
        tick.lineTo (icon.getX() + 6.2f, icon.getBottom() - 4.0f);
        tick.lineTo (icon.getRight() - 3.5f, icon.getY() + 4.2f);
        g.strokePath (tick, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (compact)
        return;
    g.setColour (pending ? theme::accent : theme::dim.withAlpha (0.75f + 0.25f * emphasis));
    g.setFont (textFont);
    g.drawText (words, b.withTrimmedLeft (6.0f), juce::Justification::centredLeft, true);
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
SearchBox::SearchBox (const juce::String& placeholder)
{
    setTextToShowWhenEmpty (placeholder, dim);
    setFont (font (13.5f));
    setIndents (30, 0);
    setJustification (juce::Justification::centredLeft);
    setSelectAllWhenFocused (true);
    setTooltip ("Type part of a name or location; letters in order and small typos also match. Esc clears, Return opens the first.");
    onTextChange = [this] { if (onSearch) onSearch(); };
    // Return and Esc hand the keyboard back, so the DAW's own keys (the space bar) work again.
    onEscapeKey  = [this] { clear(); if (onSearch) onSearch(); giveAwayKeyboardFocus(); };
    onReturnKey  = [this] { if (onSubmit) onSubmit(); giveAwayKeyboardFocus(); };
}

void SearchBox::paintOverChildren (juce::Graphics& g)
{
    juce::TextEditor::paintOverChildren (g);   // the placeholder text
    // A magnifier on the left.
    const auto c = juce::Point<float> (15.0f, (float) getHeight() * 0.5f - 1.0f);
    g.setColour (dim);
    g.drawEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f, 1.6f);
    g.drawLine (c.x + 3.6f, c.y + 3.6f, c.x + 7.0f, c.y + 7.0f, 1.8f);
}

std::vector<int> layoutFilteredRows (juce::OwnedArray<Tile>& rows, juce::Component& list, int width, int rowHeight, const juce::String& query)
{
    juce::StringArray names, locations;
    for (auto* t : rows)
    {
        names.add (t->title);
        locations.add (t->subtitle);
    }
    const auto shown = fuzzy::order (query, names, locations);

    for (auto* t : rows)
        t->setVisible (false);
    for (size_t k = 0; k < shown.size(); ++k)
    {
        auto* t = rows[shown[k]];
        t->setBounds (0, (int) k * rowHeight, width, rowHeight);
        t->setVisible (true);
    }
    list.setSize (width, juce::jmax (rowHeight, rowHeight * (int) shown.size()));
    return shown;
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
juce::String commandKeyName()
{
   #if JUCE_MAC
    return "Cmd";
   #else
    return "Ctrl";
   #endif
}

juce::String optionKeyName()
{
   #if JUCE_MAC
    return "Option";
   #else
    return "Alt";
   #endif
}

namespace
{
constexpr float sheetTitleH = 22.0f, sheetRowH = 22.0f, sheetGroupGap = 14.0f, sheetColumnGap = 22.0f;
juce::Font sheetKeyFont() { return juce::Font (font (11.5f, true)); }
juce::Font sheetTextFont() { return juce::Font (font (12.5f)); }
}

ShortcutSheet::ShortcutSheet (std::vector<ShortcutGroup> g, int c) : groups (std::move (g)), columns (juce::jmax (1, c))
{
    setInterceptsMouseClicks (false, false);
}

// Groups go down the columns in order, a new column when this one is past its share of the rows.
std::vector<ShortcutSheet::Placed> ShortcutSheet::place (int width, float& height) const
{
    (void) width;
    int total = 0;
    for (const auto& g : groups)
        total += (int) g.rows.size() + 2;
    const auto share = (float) total / (float) columns;
    std::vector<Placed> out;
    int column = 0;
    float y = 0.0f, used = 0.0f;
    height = 0.0f;
    for (const auto& g : groups)
    {
        const auto rows = (float) g.rows.size() + 2.0f;
        if (used > 0.0f && used + rows * 0.5f > share && column < columns - 1)
        {
            ++column;
            y = 0.0f;
            used = 0.0f;
        }
        out.push_back ({ &g, column, y });
        y += sheetTitleH + (float) g.rows.size() * sheetRowH + sheetGroupGap;
        used += rows;
        height = juce::jmax (height, y - sheetGroupGap);
    }
    return out;
}

int ShortcutSheet::heightForWidth (int width) const
{
    float h = 0.0f;
    place (width, h);
    return (int) std::ceil (h);
}

void ShortcutSheet::paint (juce::Graphics& g)
{
    float h = 0.0f;
    const auto placed = place (getWidth(), h);
    const auto columnW = ((float) getWidth() - sheetColumnGap * (float) (columns - 1)) / (float) columns;
    for (const auto& p : placed)
    {
        const auto x = (float) p.column * (columnW + sheetColumnGap);
        auto y = p.y;
        g.setColour (accent);
        g.setFont (font (11.0f, true));
        g.drawText (p.group->title.toUpperCase(), juce::Rectangle<float> (x, y, columnW, sheetTitleH), juce::Justification::centredLeft, true);
        y += sheetTitleH;
        // The key chips share a width per group, so the actions line up.
        float keyW = 0.0f;
        for (const auto& r : p.group->rows)
            keyW = juce::jmax (keyW, juce::GlyphArrangement::getStringWidth (sheetKeyFont(), r.keys) + 14.0f);
        keyW = juce::jmin (keyW, columnW * 0.55f);
        for (const auto& r : p.group->rows)
        {
            const auto chip = juce::Rectangle<float> (x, y + 2.0f, keyW, sheetRowH - 4.0f);
            g.setColour (raised);
            g.fillRoundedRectangle (chip, 4.0f);
            g.setColour (text);
            g.setFont (sheetKeyFont());
            g.drawFittedText (r.keys, chip.toNearestInt().reduced (5, 0), juce::Justification::centred, 1, 0.8f);
            g.setColour (dim);
            g.setFont (sheetTextFont());
            g.drawFittedText (r.action, juce::Rectangle<float> (x + keyW + 8.0f, y, columnW - keyW - 8.0f, sheetRowH).toNearestInt(),
                              juce::Justification::centredLeft, 1, 0.85f);
            y += sheetRowH;
        }
    }
}

namespace
{
class ShortcutPopup final : public juce::Component
{
public:
    ShortcutPopup (const juce::String& t, std::vector<ShortcutGroup> groups, int width) : sheet (std::move (groups))
    {
        title.setText (t, juce::dontSendNotification);
        title.setFont (font (15.0f, true));
        title.setColour (juce::Label::textColourId, text);
        addAndMakeVisible (title);
        addAndMakeVisible (sheet);
        setSize (width, 16 + 26 + 10 + sheet.heightForWidth (width - 32) + 16);
    }
    void paint (juce::Graphics& g) override { g.fillAll (surface); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (16);
        title.setBounds (r.removeFromTop (26));
        r.removeFromTop (10);
        sheet.setBounds (r);
    }

private:
    juce::Label title;
    ShortcutSheet sheet;
};
}

void showShortcuts (juce::Component& anchor, const juce::String& title, std::vector<ShortcutGroup> groups, int width)
{
    juce::CallOutBox::launchAsynchronously (std::make_unique<ShortcutPopup> (title, std::move (groups), width), anchor.getScreenBounds(), nullptr);
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
    w->addTextEditor ("setlist", preset[IDs::setlist].toString(), "Setlist number (as on the QC; 0 = Factory Presets)");
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

        preset.setProperty (IDs::setlist, juce::jlimit (0, 127, w->getTextEditorContents ("setlist").getIntValue()), nullptr);
        preset.setProperty (IDs::bank,    juce::jlimit (1, 32,  w->getTextEditorContents ("bank").getIntValue()), nullptr);
        preset.setProperty (IDs::slot,    juce::jmax (0, w->getComboBoxComponent ("slot")->getSelectedItemIndex()), nullptr);
    }), true);
}

// The pedal faceplate: the Whammy's red banner, and every effect pedal page in its brand colour.
void Faceplate::paint (juce::Graphics& g)
    {
        const auto b = getLocalBounds().toFloat();
        juce::ColourGradient grad (top, b.getTopLeft(), bottom, b.getBottomRight(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b, 14.0f);

        // Brushed highlights.
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.fillRoundedRectangle (b.withHeight (b.getHeight() * 0.45f).reduced (2.0f, 2.0f), 12.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (b.reduced (0.5f), 14.0f, 1.0f);

        // Screws.
        for (auto p : { juce::Point<float> (12.0f, 12.0f), { 12.0f, b.getBottom() - 12.0f } })
        {
            g.setColour (bottom.darker (0.6f));
            g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre (p));
            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.drawLine (p.x - 2.5f, p.y, p.x + 2.5f, p.y, 1.0f);
        }

        auto area = getLocalBounds().reduced (28, 10);
        auto logo = area.removeFromLeft (logoWidth).toFloat();

        // Italic, heavy logotype drawn as a skewed glyph run.
        // Long names ("GENERATION LOSS MKII", "VENTRIS DUAL REVERB") shrink to fit the space left of the display.
        auto size = logo.getHeight() * 0.58f;
        const auto width = juce::GlyphArrangement::getStringWidth (juce::Font (font (size, true)).withHorizontalScale (1.1f), model);
        const auto room = logo.getWidth() - 16.0f;   // the shear leans the top of the letters right
        if (width > room)
            size *= room / width;
        juce::GlyphArrangement ga;
        ga.addLineOfText (juce::Font (font (size, true)).withHorizontalScale (1.1f),
                          model, logo.getX(), logo.getCentreY() + logo.getHeight() * 0.17f);
        juce::Path text;
        ga.createPath (text);
        text.applyTransform (juce::AffineTransform::shear (-0.22f, 0.0f)
                                 .translated (0.22f * (logo.getCentreY()), 0.0f));

        const auto darkInk = ink.getPerceivedBrightness() < 0.5f;
        g.setColour (darkInk ? juce::Colours::white.withAlpha (0.35f) : juce::Colours::black.withAlpha (0.35f));
        g.fillPath (text, juce::AffineTransform::translation (darkInk ? 1.0f : 2.0f, darkInk ? 1.5f : 3.0f));
        g.setColour (ink);
        g.fillPath (text);

        const auto tb = text.getBounds();

        g.setColour (ink.withAlpha (0.8f));
        g.setFont (font (11.0f, true));
        g.drawText (tagline, juce::Rectangle<float> (tb.getX(), tb.getBottom() + 4.0f, 460.0f, 14.0f),
                    juce::Justification::centredLeft);
    }

} // namespace ui
