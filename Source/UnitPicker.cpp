#include "EditorCommon.h"
#include "DeviceTemplates.h"
#include "Modellers.h"

using namespace theme;

namespace ui
{
namespace
{
// The unit picker (the ▾ on the first tab, and the devices in How to connect): a searchable, scrolling list of the
// built-in units, the Fractal and Line 6 templates, and the player's own MIDI devices.
class UnitPicker final : public juce::Component, private juce::ListBoxModel
{
public:
    UnitPicker (juce::ValueTree s, std::function<void()> done, bool pedalsTab)
        : state (std::move (s)), onDone (std::move (done)), pedals (pedalsTab)
    {
        search.onSearch = [this] { filter(); };
        search.onSubmit = [this]
        {
            for (int i = 0; i < (int) shown.size(); ++i)
                if (entries[(size_t) shown[(size_t) i]].kind != Kind::header)
                {
                    choose (shown[(size_t) i]);
                    return;
                }
        };
        search.setTextToShowWhenEmpty (pedals ? "Search: Whammy DT, DL4, your devices..." : "Search: Helix, Axe-Fx, Kemper...", dim);
        addAndMakeVisible (search);

        list.setModel (this);
        list.setRowHeight (34);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setOutlineThickness (0);
        addAndMakeVisible (list);

        // Actions, not devices: buttons under the list, whatever the search shows.
        newButton.setColour (juce::TextButton::buttonColourId, accent);
        newButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        newButton.setTooltip ("Make your own device for any gear that takes MIDI (Boss, a synth, a looper...)");
        newButton.onClick = [this] { act (Kind::newDevice); };
        importButton.setColour (juce::TextButton::buttonColourId, raised);
        importButton.setTooltip ("Add a device from a .pedalcues-device file");
        importButton.onClick = [this] { act (Kind::importDevice); };
        addAndMakeVisible (newButton);
        addAndMakeVisible (importButton);

        key.setText ("beta: built from the manual, not tested on hardware yet. template: ready-made tiles to adjust, for units whose "
                     "MIDI you assign yourself or whose numbers aren't in the manual. Any other device: + New MIDI device.", juce::dontSendNotification);
        key.setFont (font (11.5f));
        key.setColour (juce::Label::textColourId, dim);
        key.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (key);

        build();
        filter();
        setSize (420, 540);

        // The player opened this to search: give the box the keyboard right away.
        juce::Component::SafePointer<UnitPicker> safe (this);
        juce::Timer::callAfterDelay (60, [safe] { if (safe != nullptr && safe->isShowing()) safe->search.grabKeyboardFocus(); });
    }

    void setSearch (const juce::String& query)
    {
        search.setText (query, false);
        filter();
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (10);
        search.setBounds (r.removeFromTop (32));
        r.removeFromTop (8);
        auto buttons = r.removeFromBottom (32);
        newButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2 - 4));
        buttons.removeFromLeft (8);
        importButton.setBounds (buttons);
        r.removeFromBottom (6);
        key.setBounds (r.removeFromBottom (46));
        r.removeFromBottom (4);
        list.setBounds (r);
    }

private:
    enum class Kind { header, builtin, modeller, templ, device, newDevice, importDevice };

    struct Entry
    {
        Kind kind;
        juce::String label, sub, searchExtra;
        int value = 0;               // builtin: ampUnit; device: its index
        juce::String templateId;
        bool current = false;
    };

    // The Effects & Pedals tab's pages (DL4 MkII, HX One) and templates (VP4, Microtubes Infinity), grouped by brand.
    void addTemplates (const juce::ValueTree& units, bool fxIsCustom, int fxSelected)
    {
        const auto fxPage = (int) state[IDs::fxUnit] == state::fxModeller ? state[IDs::fxProfile].toString() : juce::String();
        juce::StringArray brands;
        for (const auto& m : modellers::all())
            if (m.pedal)
                brands.addIfNotAlreadyThere (m.brand);
        for (const auto& t : templates::all())
            if (t.pedal)
                brands.addIfNotAlreadyThere (t.brand);
        for (const auto& brand : brands)
        {
            entries.push_back ({ Kind::header, brand, {}, {} });
            for (const auto& m : modellers::all())
                if (m.pedal && m.brand == brand)
                    entries.push_back ({ Kind::modeller, m.model, m.beta ? juce::String ("beta") : juce::String(), m.brand + " " + m.aliases + " pedal effect",
                                         0, m.id, m.id == fxPage });
            for (const auto& t : templates::all())
            {
                if (t.brand != brand || ! t.pedal)
                    continue;
                int existing = -1;
                for (int i = 0; i < units.getNumChildren(); ++i)
                    if (units.getChild (i)[IDs::templateId].toString() == t.id)
                        existing = i;
                entries.push_back ({ Kind::templ, t.model, existing >= 0 ? juce::String ("added, template") : juce::String ("template"),
                                     t.brand + " " + t.aliases, existing, t.id, fxIsCustom && existing >= 0 && existing == fxSelected });
            }
        }
    }

    void build()
    {
        const auto ampUnit = (int) state[IDs::ampUnit];
        const auto units = state.getChildWithName (IDs::CustomUnits);
        const auto selected = (int) state[IDs::selectedCustomUnit];
        const auto fxIsCustom = (int) state[IDs::fxUnit] == state::fxCustom;
        const auto fxSelected = (int) state[IDs::fxCustomUnit];

        if (pedals)
        {
            // The pedals tab (second tab): the Whammy first, then your own MIDI devices (not the one on the first tab).
            const auto fx = (int) state[IDs::fxUnit];
            entries.push_back ({ Kind::builtin, "No pedal", {}, "none empty nothing", state::fxNone, {}, fx == state::fxNone });
            entries.push_back ({ Kind::header, "DigiTech", {}, {} });
            // Two devices with one page: the DT adds Drop Tune and has no Chords (whModel).
            const auto dt = (int) state[IDs::whModel] == 1;
            entries.push_back ({ Kind::builtin, "Whammy V", {}, "digitech whammy v 5 pitch", state::fxWhammy, "v", fx == state::fxWhammy && ! dt });
            entries.push_back ({ Kind::builtin, "Whammy DT", {}, "digitech whammy dt drop tune pitch", state::fxWhammy, "dt", fx == state::fxWhammy && dt });
            addTemplates (units, fxIsCustom, fxSelected);
            entries.push_back ({ Kind::header, "Your MIDI devices (beta)", {}, {} });
            for (int i = 0; i < units.getNumChildren(); ++i)
            {
                const auto u = units.getChild (i);
                const auto* t = templates::find (u[IDs::templateId].toString());
                if (! state::isPedal (u) || (ampUnit == state::customAmpUnit && i == selected))
                    continue;   // amps & modellers, or already on the first tab
                entries.push_back ({ Kind::device, u[IDs::name].toString(), t != nullptr ? juce::String ("template") : juce::String ("your own"),
                                     t != nullptr ? t->brand + " " + t->aliases : juce::String(), i, {}, fxIsCustom && i == fxSelected });
            }
            return;
        }

        // Grouped by brand: Neural DSP, Kemper, Fractal Audio, Line 6, HeadRush, Boss, Darkglass, then your own devices. "beta" marks
        // units built from the manuals and not tested on hardware; "template" the units whose MIDI you assign yourself.
        entries.push_back ({ Kind::header, "Neural DSP", {}, {} });
        entries.push_back ({ Kind::builtin, ampUnitName (0), {}, "neural dsp qc amp modeller", 0, {}, ampUnit == 0 });
        entries.push_back ({ Kind::builtin, ampUnitName (state::qcMiniAmpUnit), {}, "neural dsp qc mini amp modeller",
                             state::qcMiniAmpUnit, {}, ampUnit == state::qcMiniAmpUnit });
        const auto currentModel = ampUnit == state::modellerAmpUnit ? state[IDs::modellerProfile].toString() : juce::String();
        for (const auto& m : modellers::all())   // the Nano Cortex: a page of its own
            if (m.brand == "Neural DSP" && ! m.pedal)
                entries.push_back ({ Kind::modeller, m.model, m.beta ? juce::String ("beta") : juce::String(), m.brand + " " + m.aliases, 0, m.id,
                                     m.id == currentModel });
        entries.push_back ({ Kind::header, "Kemper", {}, {} });
        for (int u = 1; u < 3; ++u)
            entries.push_back ({ Kind::builtin, ampUnitName (u), {}, "kemper amp modeller", u, {}, ampUnit == u });

        for (const auto* brand : { "Fractal Audio", "Line 6", "HeadRush", "Boss", "Darkglass" })
        {
            entries.push_back ({ Kind::header, brand, {}, {} });
            for (const auto& m : modellers::all())
                if (m.brand == brand && ! m.pedal)   // DL4 MkII, HX One: on the Effects & Pedals tab
                    entries.push_back ({ Kind::modeller, m.model, m.beta ? juce::String ("beta") : juce::String(), m.brand + " " + m.aliases + " amp modeller",
                                         0, m.id, m.id == currentModel });
            for (const auto& t : templates::all())
            {
                if (t.brand != brand || t.pedal)
                    continue;   // pedal templates are on the Effects & Pedals tab
                int existing = -1;
                for (int i = 0; i < units.getNumChildren(); ++i)
                    if (units.getChild (i)[IDs::templateId].toString() == t.id)
                        existing = i;
                entries.push_back ({ Kind::templ, t.model, existing >= 0 ? juce::String ("added, template") : juce::String ("template"),
                                     t.brand + " " + t.aliases, existing, t.id,
                                     ampUnit == state::customAmpUnit && existing >= 0 && existing == selected });
            }
        }

        entries.push_back ({ Kind::header, "Your MIDI devices (beta)", {}, {} });
        for (int i = 0; i < units.getNumChildren(); ++i)
        {
            const auto u = units.getChild (i);
            const auto* t = templates::find (u[IDs::templateId].toString());
            if (state::isPedal (u) || (fxIsCustom && i == fxSelected))
                continue;   // an effect / pedal, or on the Effects & Pedals tab
            entries.push_back ({ Kind::device, u[IDs::name].toString(), t != nullptr ? juce::String ("template") : juce::String ("your own"),
                                 t != nullptr ? t->brand + " " + t->aliases : juce::String(), i, {}, ampUnit == state::customAmpUnit && i == selected });
        }
    }

    // Matching entries in list order; a group's heading shows when any of its entries does.
    void filter()
    {
        const auto query = search.getText().trim();
        shown.clear();
        int header = -1;
        bool headerShown = false;
        for (int i = 0; i < (int) entries.size(); ++i)
        {
            const auto& e = entries[(size_t) i];
            if (e.kind == Kind::header)
            {
                header = i;
                headerShown = false;
                continue;
            }
            if (fuzzy::score (query, e.label, e.sub + " " + e.searchExtra) < 0)
                continue;
            if (! headerShown && header >= 0)
            {
                shown.push_back (header);
                headerShown = true;
            }
            shown.push_back (i);
        }
        list.updateContent();
        list.repaint();
    }

    int getNumRows() override { return (int) shown.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, (int) shown.size()))
            return;
        const auto& e = entries[(size_t) shown[(size_t) row]];
        auto r = juce::Rectangle<int> (0, 0, width, height);
        if (e.kind == Kind::header)
        {
            g.setColour (dim);
            g.setFont (font (11.5f, true));
            g.drawText (e.label.toUpperCase(), r.reduced (8, 0).withTrimmedTop (10), juce::Justification::centredLeft);
            return;
        }
        if (selected || e.current)
        {
            g.setColour (selected ? surfaceHi : surface);
            g.fillRoundedRectangle (r.toFloat().reduced (2.0f, 1.0f), 6.0f);
        }
        r = r.reduced (12, 0);
        g.setColour (e.current ? accent : text);
        g.setFont (font (14.0f, e.current));
        const auto check = e.current ? juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93  ")) : juce::String();
        auto subArea = r.removeFromRight (e.sub.isEmpty() ? 0 : 110);
        g.drawFittedText (check + e.label, r, juce::Justification::centredLeft, 1, 0.85f);
        g.setColour (dim);
        g.setFont (font (11.5f));
        g.drawText (e.sub, subArea, juce::Justification::centredRight, true);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override { pick (row); }
    void returnKeyPressed (int row) override                           { pick (row); }

    void pick (int row)
    {
        if (juce::isPositiveAndBelow (row, (int) shown.size()) && entries[(size_t) shown[(size_t) row]].kind != Kind::header)
            choose (shown[(size_t) row]);
    }

    void choose (int index)
    {
        act (entries[(size_t) index].kind, index);
    }

    void act (Kind kind, int index = -1)
    {
        const auto e = index >= 0 ? entries[(size_t) index] : Entry { kind, {}, {}, {} };
        auto s = state;
        switch (e.kind)
        {
            case Kind::builtin:
                s.setProperty (pedals ? IDs::fxUnit : IDs::ampUnit, e.value, nullptr);   // pedals tab: the Whammy
                if (pedals && e.value == state::fxWhammy)
                    s.setProperty (IDs::whModel, e.templateId == "dt" ? 1 : 0, nullptr);
                break;
            case Kind::modeller:
                if (pedals)
                {
                    s.setProperty (IDs::fxProfile, e.templateId, nullptr);
                    s.setProperty (IDs::fxUnit, state::fxModeller, nullptr);
                    state::fxModellerData (s);
                    break;
                }
                s.setProperty (IDs::modellerProfile, e.templateId, nullptr);
                state::modeller (s, e.templateId);
                s.setProperty (IDs::ampUnit, state::modellerAmpUnit, nullptr);
                break;
            case Kind::device:
                state::showCustomUnitOn (s, e.value, pedals);
                break;
            case Kind::templ:
                // Already made from this template: show it. Otherwise add a new device from it.
                if (e.value >= 0)
                    state::showCustomUnitOn (s, e.value, pedals);
                else if (const auto* t = templates::find (e.templateId))
                {
                    state::addCustomUnit (s, templates::createUnit (*t), pedals);
                    if (! pedals)
                        s.setProperty (IDs::ampUnit, state::customAmpUnit, nullptr);
                }
                break;
            case Kind::newDevice:
            {
                const auto fx = pedals;
                juce::MessageManager::callAsync ([s, fx] { newCustomUnit (s, fx); });
                break;
            }
            case Kind::importDevice:
            {
                const auto fx = pedals;
                juce::MessageManager::callAsync ([s, fx] { importCustomUnit (s, fx); });
                break;
            }
            case Kind::header:
                return;
        }

        auto done = onDone;
        if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
            box->dismiss();
        if (done)
            juce::MessageManager::callAsync (done);
    }

    juce::ValueTree state;
    std::function<void()> onDone;
    const bool pedals;   // the pedals tab's list (the Whammy and your devices) rather than the first tab's
    SearchBox search { "Search devices" };
    juce::TextButton newButton { "+ New MIDI device" }, importButton { "Import device..." };
    juce::Label key;
    juce::ListBox list;
    std::vector<Entry> entries;
    std::vector<int> shown;
};
} // namespace

std::unique_ptr<juce::Component> makeUnitPicker (juce::ValueTree state, const juce::String& query, bool pedalsTab)
{
    auto picker = std::make_unique<UnitPicker> (std::move (state), std::function<void()> {}, pedalsTab);
    picker->setSearch (query);
    return picker;
}

void showUnitPicker (juce::ValueTree state, juce::Component& target, std::function<void()> onDone, bool pedalsTab)
{
    auto picker = std::make_unique<UnitPicker> (std::move (state), std::move (onDone), pedalsTab);
    juce::CallOutBox::launchAsynchronously (std::move (picker), target.getScreenBounds(), nullptr);
}
} // namespace ui
