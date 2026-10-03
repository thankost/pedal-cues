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
        title.setText ("Sync from the Quad Cortex", juce::dontSendNotification);
        title.setFont (font (16.0f, true));
        title.setColour (juce::Label::textColourId, theme::text);
        addAndMakeVisible (title);

        note.setText ("Reads your setlists, preset names, and scene names, colours and stomps over the QC's USB port. "
                      "Nothing on the pedal changes. Only USB is needed for this; your MIDI cues can still use any cable.",
                      juce::dontSendNotification);
        styleNote (note);
        addAndMakeVisible (note);

        // The most common reason a sync fails, so it gets its own, prominent line.
        warning.setText ("Quit Cortex Control first: it keeps the USB connection to itself.", juce::dontSendNotification);
        warning.setFont (font (14.0f, true));
        warning.setColour (juce::Label::textColourId, accent);
        addAndMakeVisible (warning);

        status.setFont (font (13.0f, true));
        status.setColour (juce::Label::textColourId, theme::text);
        status.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (status);

        replaceToggle.setButtonText ("Replace my current preset list (otherwise add new presets and rename matching ones)");
        addChildComponent (replaceToggle);

        scanToggle.setButtonText ("Also read scenes, colours and stomps for every ticked preset");
        addChildComponent (scanToggle);
        scanWarning.setText ("The QC loads each preset in turn to read it (the audio cuts each time), then goes back to the one "
                             "you were on. Do it at home, not on stage. It won't start if the loaded preset has unsaved changes. "
                             "The QC's Recents list may change.", juce::dontSendNotification);
        styleNote (scanWarning);
        addChildComponent (scanWarning);

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
        note.setBounds (r.removeFromTop (34));
        warning.setBounds (r.removeFromTop (24));
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

        status.setBounds (r.removeFromTop (52));
        replaceToggle.setBounds (r.removeFromBottom (28));
        scanWarning.setBounds (r.removeFromBottom (44).withTrimmedLeft (26));
        scanToggle.setBounds (r.removeFromBottom (28));
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
        showPickers (false);
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
        lastResult = result;
        primary.setButtonText ("Import");
        status.setText ("Found " + juce::String ((int) result.setlists.size()) + " setlists on your "
                        + juce::String (result.isMini ? "QC Mini" : "Quad Cortex") + " (CorOS " + result.corosVersion + "). "
                        "Tick the ones to import, and check each one's setlist number (used when \"Send setlist\" is on)."
                        + (result.current ? " The loaded preset (" + result.current->name + ") comes with its scenes, colours and stomps."
                                          : juce::String()),
                        juce::dontSendNotification);

        setlistRows.clear();
        rows.removeAllChildren();
        // Numbers as the QC counts them (and as CC#32 selects them): 0 = Factory Presets, then your setlists from 1.
        int number = 1;
        for (const auto& f : result.setlists)
        {
            auto row = std::make_unique<Row>();
            row->folder = f;
            row->tick.setButtonText (f.name + "  (" + juce::String ((int) f.presets.size()) + " presets)");
            row->tick.setToggleState (! f.isFactory && ! f.presets.empty(), juce::dontSendNotification);
            row->number.addItem ("Factory (0)", factoryId);
            for (int n = 1; n <= 16; ++n)
                row->number.addItem ("Setlist " + juce::String (n), n);
            row->number.setSelectedId (f.isFactory ? factoryId : juce::jlimit (1, 16, number++), juce::dontSendNotification);
            rows.addAndMakeVisible (row->tick);
            rows.addAndMakeVisible (row->number);
            setlistRows.push_back (std::move (row));
        }

        showPickers (true);
        resized();
    }

    void showPickers (bool show)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &rowsView, &replaceToggle, &scanToggle, &scanWarning })
            c->setVisible (show);
    }

    void onPrimary()
    {
        if (! picking)
        {
            startReading();
            return;
        }
        if (! scanToggle.getToggleState())
        {
            importPresets ({});
            return;
        }

        // Read every ticked preset's scenes and stomps (loads each one on the QC), then import.
        std::vector<qcusb::ScanTarget> targets;
        for (const auto& row : setlistRows)
            if (row->tick.getToggleState())
                for (const auto& p : row->folder.presets)
                    targets.push_back ({ row->folder.key, row->folder.isFactory, p.position, p.name });

        picking = false;
        showPickers (false);
        primary.setEnabled (false);
        secondary.setButtonText ("Stop");
        status.setText ("Connecting to the Quad Cortex...", juce::dontSendNotification);
        resized();

        cancel = std::make_shared<std::atomic<bool>> (false);
        juce::Component::SafePointer<QcSyncDialog> safe (this);
        auto flag = cancel;
        std::thread ([safe, flag, targets]
        {
            auto scan = qcusb::scanPresets (targets, [safe] (const juce::String& text, double)
            {
                juce::MessageManager::callAsync ([safe, text] { if (safe != nullptr) safe->status.setText (text, juce::dontSendNotification); });
            }, *flag);

            juce::MessageManager::callAsync ([safe, scan]
            {
                if (safe == nullptr)
                    return;
                if (! scan.ok)
                {
                    safe->status.setText (scan.error + " Nothing was imported.", juce::dontSendNotification);
                    safe->secondary.setButtonText ("Close");
                    safe->primary.setButtonText ("Back");
                    safe->primary.setEnabled (true);
                    safe->primary.onClick = [safe] { if (safe != nullptr) { safe->primary.onClick = [safe] { safe->onPrimary(); }; safe->showResult (safe->lastResult); } };
                    return;
                }
                safe->importPresets (scan.presets);
            });
        }).detach();
    }

    // Adds/renames presets for the ticked setlists, with scenes/stomps from `details` (and the loaded preset).
    void importPresets (std::map<std::pair<juce::String, int>, qcusb::PresetDetails> details)
    {
        if (lastResult.current && lastResult.currentPosition >= 0)
            details.emplace (std::make_pair (lastResult.currentFolderKey, lastResult.currentPosition), *lastResult.current);

        auto qc = state.getChildWithName (IDs::QC);
        if (replaceToggle.getToggleState())
            qc.removeAllChildren (nullptr);

        int added = 0, renamed = 0, withScenes = 0;
        for (const auto& row : setlistRows)
        {
            if (! row->tick.getToggleState())
                continue;
            const auto setlist = row->number.getSelectedId() == factoryId ? 0 : row->number.getSelectedId();
            for (const auto& p : row->folder.presets)
            {
                const auto bank = qcusb::bankOf (p.position), slot = qcusb::slotOf (p.position);
                juce::ValueTree match;
                for (auto child : qc)
                    if ((int) child[IDs::setlist] == setlist && (int) child[IDs::bank] == bank && (int) child[IDs::slot] == slot)
                        match = child;

                if (match.isValid())
                {
                    match.setProperty (IDs::name, p.name, nullptr);
                    ++renamed;
                }
                else
                {
                    match = state::createPreset (p.name, setlist, bank, slot, paletteColour (qc.getNumChildren()));
                    qc.appendChild (match, nullptr);
                    ++added;
                }

                const auto it = details.find ({ row->folder.key.trimCharactersAtEnd ("/"), p.position });
                if (it != details.end())
                {
                    applyDetails (match, it->second);
                    ++withScenes;
                }
            }
        }

        state.setProperty (IDs::selectedPreset, 0, nullptr);
        picking = false;
        showPickers (false);
        primary.setEnabled (true);
        secondary.setButtonText ("Close");
        status.setText ("Done: " + juce::String (added) + " presets added, " + juce::String (renamed) + " renamed, "
                        + juce::String (withScenes) + " with their scene names, colours and stomps from the QC. "
                        "Use the menu's Save as default setup to keep them for new projects.",
                        juce::dontSendNotification);
        primary.setButtonText ("Sync again");
        resized();
    }

    // Scene names and colours, footswitch names. Unlabelled ones on the QC keep the name they had here.
    static void applyDetails (juce::ValueTree preset, const qcusb::PresetDetails& d)
    {
        for (int i = 0; i < juce::jmin (d.sceneCount, 8); ++i)
        {
            auto scene = nthOfType (preset, IDs::Scene, i);
            if (! scene.isValid())
                continue;
            if (d.sceneNames[(size_t) i].isNotEmpty())
                scene.setProperty (IDs::name, d.sceneNames[(size_t) i], nullptr);
            if (d.sceneColours[(size_t) i] != 0)
                scene.setProperty (IDs::colour, juce::Colour (d.sceneColours[(size_t) i] | 0xff000000u).toString(), nullptr);
        }
        for (int i = 0; i < 8; ++i)
        {
            auto stomp = nthOfType (preset, IDs::Stomp, i);
            if (stomp.isValid() && d.stompNames[(size_t) i].isNotEmpty())
                stomp.setProperty (IDs::name, d.stompNames[(size_t) i], nullptr);
        }
    }

    void close()
    {
        cancel->store (true);
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    }

    static constexpr int factoryId = 100;   // the setlist combo's "Factory (0)" item (ids must be non-zero)

    juce::ValueTree state;
    std::shared_ptr<std::atomic<bool>> cancel = std::make_shared<std::atomic<bool>> (false);
    bool picking = false;

    juce::Label title, note, warning, status;
    juce::ToggleButton replaceToggle, scanToggle;
    juce::Label scanWarning;
    qcusb::Result lastResult;
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
