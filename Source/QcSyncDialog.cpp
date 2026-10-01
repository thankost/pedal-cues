#include "EditorCommon.h"
#include "QcUsb.h"

#include <thread>

using namespace theme;

namespace ui
{
namespace
{
// "Sync from QC": reads setlists and preset names over USB, then lets the player pick what to import.
class QcSyncDialog final : public juce::Component
{
public:
    explicit QcSyncDialog (juce::ValueTree s) : state (std::move (s))
    {
        title.setText ("Sync presets from the Quad Cortex", juce::dontSendNotification);
        title.setFont (font (16.0f, true));
        title.setColour (juce::Label::textColourId, theme::text);
        addAndMakeVisible (title);

        note.setText ("Reads your setlists and preset names over the QC's USB port. It only reads: nothing on the pedal "
                      "changes. Quit Cortex Control first. Your MIDI cues can still use any cable.",
                      juce::dontSendNotification);
        styleNote (note);
        addAndMakeVisible (note);

        status.setFont (font (13.0f, true));
        status.setColour (juce::Label::textColourId, theme::text);
        status.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (status);

        replaceToggle.setButtonText ("Replace my current preset list (otherwise add new presets and rename matching ones)");
        addChildComponent (replaceToggle);

        rowsView.setViewedComponent (&rows, false);
        rowsView.setScrollBarsShown (true, false);
        addChildComponent (rowsView);

        primary.setColour (juce::TextButton::buttonColourId, accent);
        primary.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        primary.onClick = [this] { onPrimary(); };
        secondary.onClick = [this] { close(); };
        addAndMakeVisible (primary);
        addAndMakeVisible (secondary);

        setSize (640, 560);
        startReading();
    }

    ~QcSyncDialog() override { cancel->store (true); }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20, 16);
        title.setBounds (r.removeFromTop (26));
        note.setBounds (r.removeFromTop (40));
        r.removeFromTop (8);

        auto buttons = r.removeFromBottom (34);
        primary.setBounds (buttons.removeFromRight (150));
        buttons.removeFromRight (10);
        secondary.setBounds (buttons.removeFromRight (110));
        r.removeFromBottom (10);

        if (! picking)
        {
            status.setBounds (r);
            return;
        }

        status.setBounds (r.removeFromTop (40));
        replaceToggle.setBounds (r.removeFromBottom (30));
        r.removeFromBottom (6);
        rowsView.setBounds (r);

        constexpr int rowH = 36;
        rows.setSize (rowsView.getWidth() - rowsView.getScrollBarThickness(), rowH * (int) setlistRows.size());
        for (int i = 0; i < (int) setlistRows.size(); ++i)
        {
            auto row = juce::Rectangle<int> (0, i * rowH, rows.getWidth(), rowH).reduced (0, 3);
            setlistRows[(size_t) i]->number.setBounds (row.removeFromRight (120));
            row.removeFromRight (8);
            setlistRows[(size_t) i]->tick.setBounds (row);
        }
    }

private:
    struct Row
    {
        qcusb::Folder folder;
        juce::ToggleButton tick;
        juce::ComboBox number;
    };

    static void styleNote (juce::Label& l)
    {
        l.setFont (font (12.0f));
        l.setColour (juce::Label::textColourId, dim);
        l.setJustificationType (juce::Justification::topLeft);
    }

    void startReading()
    {
        picking = false;
        rowsView.setVisible (false);
        replaceToggle.setVisible (false);
        status.setText ("Looking for the Quad Cortex on USB...", juce::dontSendNotification);
        primary.setButtonText ("Retry");
        primary.setEnabled (false);
        secondary.setButtonText ("Cancel");
        resized();

        cancel->store (true);                                   // stop any previous attempt
        cancel = std::make_shared<std::atomic<bool>> (false);
        juce::Component::SafePointer<QcSyncDialog> safe (this);
        auto flag = cancel;
        std::thread ([safe, flag]
        {
            auto result = qcusb::readSetlists ([safe] (const juce::String& text)
            {
                juce::MessageManager::callAsync ([safe, text] { if (safe != nullptr) safe->status.setText (text, juce::dontSendNotification); });
            }, *flag);

            juce::MessageManager::callAsync ([safe, flag, result]
            {
                if (safe != nullptr && ! flag->load())
                    safe->showResult (result);
            });
        }).detach();
    }

    void showResult (const qcusb::Result& result)
    {
        secondary.setButtonText ("Close");
        primary.setEnabled (true);

        if (! result.ok)
        {
            status.setText (result.error, juce::dontSendNotification);
            primary.setButtonText ("Retry");
            return;
        }

        picking = true;
        primary.setButtonText ("Import");
        status.setText ("Found " + juce::String ((int) result.setlists.size()) + " setlists on your "
                        + juce::String (result.isMini ? "QC Mini" : "Quad Cortex") + " (CorOS " + result.corosVersion + "). "
                        "Tick the ones to import, and check each one's setlist number (used when \"Send setlist\" is on).",
                        juce::dontSendNotification);

        setlistRows.clear();
        rows.removeAllChildren();
        int number = 1;
        for (const auto& f : result.setlists)
        {
            auto row = std::make_unique<Row>();
            row->folder = f;
            row->tick.setButtonText (f.name + "  (" + juce::String ((int) f.presets.size()) + " presets)");
            row->tick.setToggleState (! f.isFactory && ! f.presets.empty(), juce::dontSendNotification);
            for (int n = 1; n <= 13; ++n)
                row->number.addItem ("Setlist " + juce::String (n), n);
            row->number.setSelectedId (juce::jlimit (1, 13, number++), juce::dontSendNotification);
            rows.addAndMakeVisible (row->tick);
            rows.addAndMakeVisible (row->number);
            setlistRows.push_back (std::move (row));
        }

        rowsView.setVisible (true);
        replaceToggle.setVisible (true);
        resized();
    }

    void onPrimary()
    {
        if (! picking)
        {
            startReading();
            return;
        }

        auto qc = state.getChildWithName (IDs::QC);
        if (replaceToggle.getToggleState())
            qc.removeAllChildren (nullptr);

        int added = 0, renamed = 0;
        for (const auto& row : setlistRows)
        {
            if (! row->tick.getToggleState())
                continue;
            const auto setlist = row->number.getSelectedId();
            for (const auto& p : row->folder.presets)
            {
                const auto bank = qcusb::bankOf (p.position), slot = qcusb::slotOf (p.position);
                juce::ValueTree match;
                for (auto child : qc)
                    if ((int) child[IDs::setlist] == setlist && (int) child[IDs::bank] == bank && (int) child[IDs::slot] == slot)
                        match = child;

                if (match.isValid())
                {
                    match.setProperty (IDs::name, p.name, nullptr);   // keeps its scene and stomp names
                    ++renamed;
                }
                else
                {
                    qc.appendChild (state::createPreset (p.name, setlist, bank, slot, paletteColour (qc.getNumChildren())), nullptr);
                    ++added;
                }
            }
        }

        state.setProperty (IDs::selectedPreset, 0, nullptr);
        picking = false;
        rowsView.setVisible (false);
        replaceToggle.setVisible (false);
        status.setText ("Done: " + juce::String (added) + " presets added, " + juce::String (renamed) + " renamed. "
                        "Scene and stomp names stay as they are; rename them on each preset (double-click). "
                        "Use the menu's Save as default setup to keep them for new projects.",
                        juce::dontSendNotification);
        primary.setButtonText ("Sync again");
        resized();
    }

    void close()
    {
        cancel->store (true);
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    }

    juce::ValueTree state;
    std::shared_ptr<std::atomic<bool>> cancel = std::make_shared<std::atomic<bool>> (false);
    bool picking = false;

    juce::Label title, note, status;
    juce::ToggleButton replaceToggle;
    juce::Viewport rowsView;
    juce::Component rows;
    std::vector<std::unique_ptr<Row>> setlistRows;
    juce::TextButton primary { "Retry" }, secondary { "Cancel" };
};
} // namespace

void showQcSyncDialog (juce::ValueTree state)
{
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new QcSyncDialog (std::move (state)));
    o.dialogTitle = "Sync from Quad Cortex";
    o.dialogBackgroundColour = background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}
} // namespace ui
