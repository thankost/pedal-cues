#include "EditorCommon.h"

using namespace theme;

namespace ui
{
namespace
{
// Numbered "how to hook it up" steps, drawn as badges + text.
class StepsList final : public juce::Component
{
public:
    juce::StringArray steps;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds();
        constexpr int rowH = 58;

        for (int i = 0; i < steps.size(); ++i)
        {
            auto row = r.removeFromTop (rowH);
            const auto badge = row.removeFromLeft (26).removeFromTop (26).toFloat().translated (0.0f, 2.0f);
            g.setColour (accent.withAlpha (0.18f));
            g.fillEllipse (badge);
            g.setColour (accent);
            g.setFont (font (12.5f, true));
            g.drawText (juce::String (i + 1), badge, juce::Justification::centred);

            row.removeFromLeft (12);
            g.setColour (text.withAlpha (0.92f));
            g.setFont (font (13.0f));
            g.drawFittedText (steps[i], row.withTrimmedTop (3), juce::Justification::topLeft, 3, 1.0f);
        }

        r.removeFromTop (14);
        if (r.getHeight() >= 110)
            paintFlow (g, r.removeFromTop (juce::jmin (r.getHeight(), 130)));
    }

private:
    static void paintFlow (juce::Graphics& g, juce::Rectangle<int> area)
    {
        g.setColour (dim);
        g.setFont (font (11.0f, true));
        g.drawText ("SIGNAL FLOW", area.removeFromTop (20), juce::Justification::centredLeft);

        struct Node { const char* title; const char* sub; juce::Colour colour; };
        const Node nodes[] = { { "Reaper", "PedalCues track", accent },
                               { "Quad Cortex", "USB MIDI, MIDI Thru", qcBlue },
                               { "Whammy V", "5-pin MIDI In", whammyRed } };

        constexpr int arrowW = 56;
        const auto boxW = (area.getWidth() - 2 * arrowW) / 3;
        auto row = area.withSizeKeepingCentre (area.getWidth(), 66);

        for (int i = 0; i < 3; ++i)
        {
            const auto b = row.removeFromLeft (boxW).toFloat();
            g.setColour (surfaceHi);
            g.fillRoundedRectangle (b, 10.0f);
            g.setColour (nodes[i].colour);
            g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.5f);
            g.setColour (text);
            g.setFont (font (14.0f, true));
            g.drawText (nodes[i].title, b.reduced (12.0f, 10.0f).removeFromTop (22.0f), juce::Justification::centredLeft);
            g.setColour (dim);
            g.setFont (font (11.5f));
            g.drawText (nodes[i].sub, b.reduced (12.0f, 10.0f).withTrimmedTop (24.0f), juce::Justification::centredLeft, true);

            if (i < 2)
            {
                const auto a = row.removeFromLeft (arrowW).toFloat();
                juce::Path arrow;
                arrow.addArrow ({ a.getX() + 8.0f, a.getCentreY(), a.getRight() - 8.0f, a.getCentreY() }, 2.0f, 10.0f, 9.0f);
                g.setColour (nodes[i].colour);
                g.fillPath (arrow);
                g.setFont (font (10.0f, true));
                g.drawText (i == 0 ? "USB" : "MIDI", a.withTrimmedBottom (a.getHeight() / 2 + 6), juce::Justification::centredBottom);
            }
        }
    }
};

class SettingsPage final : public Page
{
public:
    SettingsPage (PedalCuesProcessor& p, std::function<void()> tour)
        : state (p.state), startTour (std::move (tour))
    {
        for (auto* c : std::initializer_list<juce::Component*> { &midiSection, &librarySection, &setupSection })
            addAndMakeVisible (c);

        styleCaption (qcChannelLabel, "Quad Cortex channel");
        styleCaption (whChannelLabel, "Whammy V channel");
        styleCaption (pcBaseLabel, "Whammy program numbering");

        for (int ch = 1; ch <= 16; ++ch)
        {
            qcChannelBox.addItem ("Channel " + juce::String (ch), ch);
            whChannelBox.addItem ("Channel " + juce::String (ch), ch);
        }

        pcBaseBox.addItem ("As printed in the manual (1 = first)", 1);
        pcBaseBox.addItem ("Zero-based (0 = first)", 2);

        qcChannelBox.onChange = [this] { state.setProperty (IDs::qcChannel, qcChannelBox.getSelectedId(), nullptr); };
        whChannelBox.onChange = [this] { state.setProperty (IDs::whChannel, whChannelBox.getSelectedId(), nullptr); };
        pcBaseBox.onChange    = [this] { state.setProperty (IDs::whPcBase, pcBaseBox.getSelectedId() == 1 ? 1 : 0, nullptr); };
        setlistToggle.onClick = [this] { state.setProperty (IDs::sendSetlist, setlistToggle.getToggleState(), nullptr); };
        setlistToggle.setTooltip ("Also send CC#32 (setlist) before each preset change. Leave off if all presets are in the active setlist.");

        saveDefaultButton.onClick = [this]
        {
            setStatus (state::saveLibrary (state, state::defaultLibraryFile())
                           ? "Saved. New PedalCues instances will start with this library."
                           : "Could not save the default library.");
        };

        loadDefaultButton.onClick = [this]
        {
            setStatus (state::loadLibrary (state, state::defaultLibraryFile())
                           ? "Default library loaded."
                           : "No default library saved yet.");
        };

        exportButton.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Export PedalCues library",
                                                           juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                               .getChildFile ("PedalCues Library.xml"),
                                                           "*.xml");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto file = fc.getResult();
                                      if (file != juce::File())
                                          setStatus (state::saveLibrary (state, file.withFileExtension ("xml"))
                                                         ? "Exported to " + file.getFileName()
                                                         : "Export failed.");
                                  });
        };

        importButton.onClick = [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Import PedalCues library",
                                                           juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                           "*.xml");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                  [this] (const juce::FileChooser& fc)
                                  {
                                      const auto file = fc.getResult();
                                      if (file.existsAsFile())
                                          setStatus (state::loadLibrary (state, file) ? "Imported " + file.getFileName()
                                                                                      : "Not a PedalCues library file.");
                                  });
        };

        saveDefaultButton.setColour (juce::TextButton::buttonColourId, accent);
        saveDefaultButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);

        statusLabel.setColour (juce::Label::textColourId, accent);
        statusLabel.setFont (font (12.5f));
        statusLabel.setText ("Tip: name your presets and scenes once, then Save as default.", juce::dontSendNotification);

        libraryInfo.setText ("The library is every preset, scene, footswitch and Whammy name. It is saved inside each "
                             "project automatically; the default library is what new instances start with.",
                             juce::dontSendNotification);
        libraryInfo.setFont (font (12.5f));
        libraryInfo.setColour (juce::Label::textColourId, dim);
        libraryInfo.setJustificationType (juce::Justification::topLeft);

        steps.steps = {
            "Insert PedalCues on a track in Reaper (e.g. 'Pedal Cues'). Arm and monitor are not needed.",
            "Route: track I/O button > MIDI Hardware Output > your Quad Cortex (enable it in Preferences > MIDI Devices first).",
            "Whammy V: QC MIDI Out > Whammy MIDI In, MIDI Thru on in the QC. Or use any USB MIDI interface.",
            "Match the channels on the left with the pedals (QC: Settings > MIDI. Whammy: hold footswitch at power-up).",
            "Drag tiles onto the arrangement. Snap to grid for exact bars. Click the round play button to test a tile live."
        };

        tourButton.setColour (juce::TextButton::buttonColourId, accent);
        tourButton.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        tourButton.onClick = [this] { if (startTour) startTour(); };
        guideButton.onClick = [] { juce::URL (guideUrl).launchInDefaultBrowser(); };

        for (auto* c : std::initializer_list<juce::Component*> { &qcChannelLabel, &whChannelLabel, &pcBaseLabel,
                                                                 &qcChannelBox, &whChannelBox, &pcBaseBox, &setlistToggle,
                                                                 &saveDefaultButton, &loadDefaultButton, &exportButton,
                                                                 &importButton, &statusLabel, &libraryInfo, &steps,
                                                                 &tourButton, &guideButton })
            addAndMakeVisible (c);

        refresh();
    }

    void refresh() override
    {
        qcChannelBox.setSelectedId ((int) state[IDs::qcChannel], juce::dontSendNotification);
        whChannelBox.setSelectedId ((int) state[IDs::whChannel], juce::dontSendNotification);
        pcBaseBox.setSelectedId ((int) state[IDs::whPcBase] == 1 ? 1 : 2, juce::dontSendNotification);
        setlistToggle.setToggleState ((bool) state[IDs::sendSetlist], juce::dontSendNotification);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (14);

        auto left = r.removeFromLeft (juce::jmax (380, r.getWidth() * 2 / 5));
        r.removeFromLeft (12);

        midiSection.setBounds (left.removeFromTop (Section::headerHeight + 4 * 62 + 8));
        left.removeFromTop (12);
        librarySection.setBounds (left);
        setupSection.setBounds (r);

        {
            auto m = midiSection.contentArea().reduced (6, 4);
            auto field = [&m] (juce::Label& l, juce::Component& c)
            {
                auto row = m.removeFromTop (62);
                l.setBounds (row.removeFromTop (22));
                c.setBounds (row.removeFromTop (32));
            };
            field (qcChannelLabel, qcChannelBox);
            field (whChannelLabel, whChannelBox);
            field (pcBaseLabel, pcBaseBox);
            setlistToggle.setBounds (m.removeFromTop (40));
        }

        {
            auto l = librarySection.contentArea().reduced (6, 4);
            libraryInfo.setBounds (l.removeFromTop (54));
            l.removeFromTop (6);

            auto row1 = l.removeFromTop (34);
            saveDefaultButton.setBounds (row1.removeFromLeft (row1.getWidth() / 2 - 4));
            row1.removeFromLeft (8);
            loadDefaultButton.setBounds (row1);
            l.removeFromTop (8);
            auto row2 = l.removeFromTop (34);
            exportButton.setBounds (row2.removeFromLeft (row2.getWidth() / 2 - 4));
            row2.removeFromLeft (8);
            importButton.setBounds (row2);
            l.removeFromTop (8);
            statusLabel.setBounds (l.removeFromTop (40));
        }

        {
            auto s = setupSection.contentArea().reduced (8, 6);
            auto buttons = s.removeFromBottom (36);
            tourButton.setBounds (buttons.removeFromLeft (180));
            buttons.removeFromLeft (10);
            guideButton.setBounds (buttons.removeFromLeft (200));
            s.removeFromBottom (12);
            steps.setBounds (s);
        }
    }

private:
    void setStatus (const juce::String& t) { statusLabel.setText (t, juce::dontSendNotification); }

    juce::ValueTree state;
    std::function<void()> startTour;

    Section midiSection    { "set.midi", "MIDI", "channels & numbering" };
    Section librarySection { "set.library", "Library", "your names, saved per project" };
    Section setupSection   { "set.setup", "Setup in Reaper", "five steps, once", qcBlue };

    juce::Label qcChannelLabel, whChannelLabel, pcBaseLabel, statusLabel, libraryInfo;
    juce::ComboBox qcChannelBox, whChannelBox, pcBaseBox;
    juce::ToggleButton setlistToggle { "Send setlist (CC#32) with preset changes" };
    juce::TextButton saveDefaultButton { "Save as default" };
    juce::TextButton loadDefaultButton { "Load default" };
    juce::TextButton exportButton { "Export..." };
    juce::TextButton importButton { "Import..." };
    StepsList steps;
    juce::TextButton tourButton { "Show quick tour" };
    juce::TextButton guideButton { "Open user guide" };
    std::unique_ptr<juce::FileChooser> chooser;
};
} // namespace

std::unique_ptr<Page> makeSettingsPage (PedalCuesProcessor& p, std::function<void()> startTour)
{
    return std::make_unique<SettingsPage> (p, std::move (startTour));
}
} // namespace ui
