#include "EditorCommon.h"
#include "HelixImport.h"
#include "Modellers.h"
#include "State.h"

using namespace theme;

namespace ui
{
namespace
{
// Line 6 page > Import from HX Edit: reads an exported .hls / .hlb / .hlx and fills the page's presets with its names.
class HelixImportDialog final : public juce::Component
{
public:
    HelixImportDialog (juce::ValueTree s, const modellers::Profile& p) : state (std::move (s)), profile (p)
    {
        title.setText ("Import from HX Edit", juce::dontSendNotification);
        title.setFont (font (16.0f, true));
        title.setColour (juce::Label::textColourId, theme::text);
        addAndMakeVisible (title);

        note.setText ("In HX Edit, export a setlist (.hls), a bundle (.hlb) or a preset (.hlx), then pick it here. PedalCues takes "
                      "the preset names, snapshot names and snapshot colours from it. Reads the file only; nothing is sent to your "
                      + profile.shortName + ".", juce::dontSendNotification);
        styleNote (note);
        addAndMakeVisible (note);

        choose.setButtonText ("Choose a .hls / .hlb / .hlx file...");
        choose.onClick = [this] { chooseFile(); };
        addAndMakeVisible (choose);

        status.setFont (font (13.0f, true));
        status.setColour (juce::Label::textColourId, theme::text);
        status.setJustificationType (juce::Justification::topLeft);
        status.setText ("No file chosen yet.", juce::dontSendNotification);
        addAndMakeVisible (status);

        for (auto* l : { &fromLabel, &toLabel })
        {
            l->setFont (font (13.0f));
            l->setColour (juce::Label::textColourId, dim);
            addChildComponent (*l);
        }
        fromLabel.setText ("From the file", juce::dontSendNotification);
        toLabel.setText (juce::String ("To ") + (modellers::hasSetlists (profile) ? "setlist" : "preset slot"), juce::dontSendNotification);
        fromBox.onChange = [this] { pickDefaultTarget(); updateSummary(); };
        toBox.onChange = [this] { updateSummary(); };
        addChildComponent (fromBox);
        addChildComponent (toBox);

        skipEmpty.setButtonText ("Skip empty presets (\"New Preset\" with nothing in it)");
        skipEmpty.setToggleState (true, juce::dontSendNotification);
        skipEmpty.onClick = [this] { updateSummary(); };
        addChildComponent (skipEmpty);

        replaceNote.setFont (font (12.5f, true));
        replaceNote.setColour (juce::Label::textColourId, accent);
        replaceNote.setJustificationType (juce::Justification::topLeft);
        addChildComponent (replaceNote);

        importButton.setColour (juce::TextButton::buttonColourId, accent);
        importButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        importButton.setEnabled (false);
        importButton.onClick = [this] { doImport(); };
        cancelButton.onClick = [this] { close(); };
        addAndMakeVisible (importButton);
        addAndMakeVisible (cancelButton);

        setSize (560, 380);
    }

    void paint (juce::Graphics& g) override { g.fillAll (background); }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20, 16);
        title.setBounds (r.removeFromTop (26));
        note.setBounds (r.removeFromTop (50));
        r.removeFromTop (6);
        choose.setBounds (r.removeFromTop (30).removeFromLeft (280));
        r.removeFromTop (10);
        status.setBounds (r.removeFromTop (40));

        auto buttons = r.removeFromBottom (34);
        importButton.setBounds (buttons.removeFromRight (130));
        buttons.removeFromRight (10);
        cancelButton.setBounds (buttons.removeFromRight (110));
        r.removeFromBottom (10);

        auto row = [&r] (juce::Label& label, juce::ComboBox& box)
        {
            auto line = r.removeFromTop (30);
            label.setBounds (line.removeFromLeft (120));
            box.setBounds (line.removeFromLeft (300));
            r.removeFromTop (6);
        };
        row (fromLabel, fromBox);
        row (toLabel, toBox);
        skipEmpty.setBounds (r.removeFromTop (28));
        replaceNote.setBounds (r.removeFromTop (36));
    }

private:
    static constexpr int allSetlistsId = 1000;   // the bundle's "All setlists" item

    static void styleNote (juce::Label& l)
    {
        l.setFont (font (12.0f));
        l.setColour (juce::Label::textColourId, dim);
        l.setJustificationType (juce::Justification::topLeft);
    }

    bool isSinglePreset() const { return result.kind == "preset"; }

    // A bundle with one setlist for each of the unit's can go in all at once, each to its own number.
    bool canImportAll() const
    {
        return result.kind == "bundle" && modellers::hasSetlists (profile) && result.setlists.size() > 1
               && (int) result.setlists.size() <= modellers::setlistNames (profile).size();
    }

    void chooseFile()
    {
        chooser = std::make_unique<juce::FileChooser> ("Choose an HX Edit export", lastFolder(), "*.hls;*.hlb;*.hlx");
        juce::Component::SafePointer<HelixImportDialog> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
                              {
                                  if (safe != nullptr && fc.getResult() != juce::File())
                                      safe->load (fc.getResult());
                              });
    }

    static juce::File lastFolder()
    {
        const juce::File f (state::getSetting ("helixImportFolder"));
        return f.isDirectory() ? f : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    }

    void load (const juce::File& file)
    {
        state::setSetting ("helixImportFolder", file.getParentDirectory().getFullPathName());
        result = helix::readFile (file);
        for (auto* c : std::initializer_list<juce::Component*> { &fromLabel, &fromBox, &toLabel, &toBox, &skipEmpty, &replaceNote })
            c->setVisible (false);
        importButton.setEnabled (false);

        if (! result.ok())
        {
            status.setText (file.getFileName() + ": " + result.error, juce::dontSendNotification);
            return;
        }

        int presets = 0;
        for (const auto& s : result.setlists)
            presets += (int) s.presets.size();
        const auto what = result.kind == "bundle"  ? juce::String ((int) result.setlists.size()) + " setlists, " + juce::String (presets) + " presets"
                        : result.kind == "setlist" ? "a setlist with " + juce::String (presets) + " presets"
                                                   : "one preset (" + result.setlists.front().presets.front().name + ")";
        status.setText (file.getFileName() + ": " + what + ".", juce::dontSendNotification);

        fromBox.clear (juce::dontSendNotification);
        for (int i = 0; i < (int) result.setlists.size(); ++i)
        {
            const auto& s = result.setlists[(size_t) i];
            const auto name = isSinglePreset() ? s.presets.front().name : s.name.isNotEmpty() ? s.name : "Setlist " + juce::String (i + 1);
            fromBox.addItem (name + (isSinglePreset() ? juce::String() : "  (" + juce::String (count (s, true)) + " presets)"), i + 1);
        }
        if (canImportAll())
            fromBox.addItem ("All setlists", allSetlistsId);

        toBox.clear (juce::dontSendNotification);
        if (isSinglePreset())
        {
            // The file doesn't say which slot it came from: pick one (in the unit's default setlist).
            const auto setlist = modellers::defaultSetlist (profile);
            for (int i = 0; i < modellers::presetsPerSetlist (profile, setlist); ++i)
                toBox.addItem ((modellers::hasSetlists (profile) ? modellers::setlistLabel (profile, setlist) + " " : juce::String())
                               + modellers::presetLabel (profile, setlist, i), i + 1);
        }
        else if (modellers::hasSetlists (profile))
        {
            const auto names = modellers::setlistNames (profile);
            for (int i = 0; i < names.size(); ++i)
                toBox.addItem (names[i], i + 1);
        }

        fromBox.setSelectedItemIndex (0, juce::dontSendNotification);
        pickDefaultTarget();
        fromLabel.setVisible (true);
        fromBox.setVisible (true);
        toLabel.setVisible (toBox.getNumItems() > 0);
        toBox.setVisible (toBox.getNumItems() > 0);
        skipEmpty.setVisible (! isSinglePreset());
        replaceNote.setVisible (true);
        importButton.setEnabled (true);
        updateSummary();
    }

    int count (const helix::Setlist& s, bool honourSkip) const
    {
        int n = 0;
        for (const auto& p : s.presets)
            n += (honourSkip && skipEmpty.getToggleState() && p.empty) ? 0 : 1;
        return n;
    }

    // The unit setlist with the same name as the file's (USER 1...), else the unit's default one.
    void pickDefaultTarget()
    {
        if (isSinglePreset() || ! modellers::hasSetlists (profile))
        {
            if (isSinglePreset())
                toBox.setSelectedItemIndex (0, juce::dontSendNotification);
            return;
        }
        const auto from = fromBox.getSelectedId() - 1;
        toBox.setEnabled (fromBox.getSelectedId() != allSetlistsId);
        int target = modellers::defaultSetlist (profile);
        if (juce::isPositiveAndBelow (from, (int) result.setlists.size()))
        {
            const auto names = modellers::setlistNames (profile);
            for (int i = 0; i < names.size(); ++i)
                if (names[i].equalsIgnoreCase (result.setlists[(size_t) from].name.trim()))
                    target = i;
        }
        toBox.setSelectedId (target + 1, juce::dontSendNotification);
    }

    void updateSummary()
    {
        if (! result.ok() || result.setlists.empty())
            return;
        juce::String text;
        if (isSinglePreset())
            text = "Replaces the preset in that slot on this page, if there is one.";
        else if (fromBox.getSelectedId() == allSetlistsId)
            text = "Replaces every setlist's presets on this page with the file's, setlist by setlist (the file's first setlist goes to "
                   + modellers::setlistNames (profile)[0] + ").";
        else if (modellers::hasSetlists (profile))
            text = "Replaces the presets in " + toBox.getText() + " on this page with the file's. Other setlists stay as they are.";
        else
            text = "Replaces the presets on this page with the file's.";
        replaceNote.setText (text + " Footswitch names stay.", juce::dontSendNotification);
    }

    void doImport()
    {
        auto root = state;
        auto node = state::modeller (root, profile.id);
        if (! node.isValid() || ! result.ok())
            return;

        const auto skip = skipEmpty.getToggleState() && ! isSinglePreset();
        if (isSinglePreset())
        {
            auto copy = result;
            copy.setlists.front().presets.front().slot = juce::jmax (0, toBox.getSelectedItemIndex());
            helix::apply (node, profile, copy, 0, modellers::defaultSetlist (profile), false);
        }
        else if (fromBox.getSelectedId() == allSetlistsId)
        {
            for (int i = 0; i < (int) result.setlists.size(); ++i)
                helix::apply (node, profile, result, i, i, skip);
        }
        else
        {
            const auto target = modellers::hasSetlists (profile) ? toBox.getSelectedId() - 1 : -1;
            helix::apply (node, profile, result, fromBox.getSelectedId() - 1, target, skip);
        }
        close();
    }

    void close()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    }

    juce::ValueTree state;
    const modellers::Profile& profile;
    helix::Result result;
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Label title, note, status, fromLabel, toLabel, replaceNote;
    juce::TextButton choose;
    juce::ComboBox fromBox, toBox;
    juce::ToggleButton skipEmpty;
    juce::TextButton importButton { "Import" }, cancelButton { "Cancel" };
};
} // namespace

void showHelixImport (juce::ValueTree state, const juce::String& profileId)
{
    const auto* profile = modellers::find (profileId);
    if (profile == nullptr || ! helix::supports (*profile))
        return;
    juce::DialogWindow::LaunchOptions o;
    o.content.setOwned (new HelixImportDialog (std::move (state), *profile));
    o.dialogTitle = "Import from HX Edit";
    o.dialogBackgroundColour = background;
    o.escapeKeyTriggersCloseButton = true;
    o.useNativeTitleBar = true;
    o.resizable = false;
    o.launchAsync();
}
} // namespace ui
