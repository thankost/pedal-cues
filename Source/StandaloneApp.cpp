// The standalone app: JUCE's standard standalone window, but with real Options and Help menus
// (the macOS menu bar, or a menu bar in the window on Windows) instead of JUCE's "Options" button.
// Built because the project sets JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1.

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

#include "EditorCommon.h"
#include "PluginEditor.h"
#include "Update.h"

namespace
{
class PedalCuesWindow final : public juce::StandaloneFilterWindow,
                              private juce::MenuBarModel
{
public:
    PedalCuesWindow (const juce::String& title, juce::Colour background, std::unique_ptr<juce::StandalonePluginHolder> holder)
        : StandaloneFilterWindow (title, background, std::move (holder))
    {
        // JUCE's own "Options" button is private: find it and hide it (its items are in the menus now).
        for (auto* child : getChildren())
            if (auto* b = dynamic_cast<juce::TextButton*> (child))
                if (b->getButtonText() == "Options")
                    b->setVisible (false);

       #if JUCE_MAC
        juce::PopupMenu appMenu;
        appMenu.addItem (about, "About PedalCues");
        appMenu.addItem (checkUpdates, "Check for Updates");
        appMenu.addItem (autoCheck, "Check for Updates Automatically", true, ! state::getFlag (update::disabledFlag));
        juce::MenuBarModel::setMacMainMenu (this, &appMenu);
       #else
        setMenuBar (this);
       #endif

        if (auto* e = editor())
            e->setHelpInMenuBar (true);
    }

    void refreshAppMenu()
    {
       #if JUCE_MAC
        juce::PopupMenu appMenu;
        appMenu.addItem (about, "About PedalCues");
        appMenu.addItem (checkUpdates, "Check for Updates");
        appMenu.addItem (autoCheck, "Check for Updates Automatically", true, ! state::getFlag (update::disabledFlag));
        juce::MenuBarModel::setMacMainMenu (this, &appMenu);
       #endif
    }

    ~PedalCuesWindow() override
    {
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (nullptr);
       #else
        setMenuBar (nullptr);
       #endif
    }

private:
    enum ItemIds { checkUpdates = 1, autoCheck, tour, guide, github, support, about,
                   saveDefault, loadDefault, exportSetup, importSetup, wiringGuide, connectRig, whatsNew, reportProblem, midiNone = 100, midiFirst = 101 };

    PedalCuesProcessor* processor() const
    {
        return pluginHolder != nullptr ? dynamic_cast<PedalCuesProcessor*> (pluginHolder->processor.get()) : nullptr;
    }

    juce::StringArray getMenuBarNames() override { return { "File", "Options", "Help" }; }

    juce::PopupMenu getMenuForIndex (int index, const juce::String&) override
    {
        juce::PopupMenu m;
        if (index == 0)
        {
            // Your setup = names, colours, MIDI settings and playing preferences.
            m.addItem (saveDefault, "Save as Default Setup");
            m.addItem (loadDefault, "Load Default Setup");
            m.addSeparator();
            m.addItem (exportSetup, "Export Setup");
            m.addItem (importSetup, "Import Setup");
        }
        else if (index == 1)
        {
            // Tiles go straight to this port; the app needs no audio device.
            juce::PopupMenu ports;
            midiDevices = juce::MidiOutput::getAvailableDevices();
            const auto current = processor() != nullptr ? processor()->getDirectMidiOutput() : juce::String();
            ports.addItem (midiNone, "None", true, current.isEmpty());
            for (int i = 0; i < midiDevices.size(); ++i)
                ports.addItem (midiFirst + i, midiDevices[i].name, true, midiDevices[i].identifier == current);
            if (midiDevices.isEmpty())
                ports.addItem (midiFirst - 2, "No MIDI devices found", false);
            m.addSubMenu ("MIDI Output", ports);
        }
        else
        {
            m.addItem (tour, "Quick Tour");
            m.addItem (guide, "User Guide");
            m.addItem (connectRig, "How to Connect (Wiring and DAW Tracks)...");
            m.addItem (wiringGuide, "Wiring Guide");
            m.addItem (whatsNew, "What's New");
            m.addSeparator();
            m.addItem (github, "PedalCues on GitHub");
            m.addItem (reportProblem, "Report a Problem");
            m.addSeparator();
            m.addItem (support, "Support PedalCues");
           #if ! JUCE_MAC
            m.addSeparator();
            m.addItem (checkUpdates, "Check for Updates");   // on macOS these are in the PedalCues menu
            m.addItem (autoCheck, "Check for Updates Automatically", true, ! state::getFlag (update::disabledFlag));
            m.addItem (about, "About PedalCues");
           #endif
        }
        return m;
    }

    void menuItemSelected (int id, int) override
    {
        auto* e = editor();
        if (id >= midiNone)
        {
            if (auto* p = processor())
                p->setDirectMidiOutput (juce::isPositiveAndBelow (id - midiFirst, midiDevices.size())
                                            ? midiDevices[id - midiFirst].identifier : juce::String());
            if (e != nullptr)
                e->refreshNow();   // How to connect's Test port shows the same choice
            return;
        }

        switch (id)
        {
            case checkUpdates:  if (e != nullptr) e->checkForUpdates (true); break;
            case autoCheck:     state::setFlag (update::disabledFlag, ! state::getFlag (update::disabledFlag)); refreshAppMenu(); break;
            case tour:          if (e != nullptr) e->startTour (0); break;
            case guide:         juce::URL (ui::guideUrl).launchInDefaultBrowser(); break;
            case github:        juce::URL (ui::repoUrl).launchInDefaultBrowser(); break;
            case support:       if (e != nullptr) e->showSupportDialog(); break;
            case about:         if (e != nullptr) e->showAboutDialog(); break;
            case saveDefault:   if (e != nullptr) e->saveDefaultSetup(); break;
            case loadDefault:   if (e != nullptr) e->loadDefaultSetup(); break;
            case exportSetup:   if (e != nullptr) e->exportSetup(); break;
            case importSetup:   if (e != nullptr) e->importSetup(); break;
            case wiringGuide:   if (processor() != nullptr) ui::showWiringGuide (processor()->state); break;
            case connectRig:    if (processor() != nullptr) ui::showConnectDialog (*processor()); break;
            case whatsNew:      juce::URL (ui::changelogUrl).launchInDefaultBrowser(); break;
            case reportProblem: ui::problemReportUrl().launchInDefaultBrowser(); break;
            default: break;
        }
    }

    juce::Array<juce::MidiDeviceInfo> midiDevices;

    PedalCuesEditor* editor() const
    {
        return pluginHolder != nullptr && pluginHolder->processor != nullptr
                 ? dynamic_cast<PedalCuesEditor*> (pluginHolder->processor->getActiveEditor())
                 : nullptr;
    }
};

class PedalCuesStandaloneApp final : public juce::JUCEApplication
{
public:
    PedalCuesStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = JucePlugin_Name;
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #endif
        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override           { return JucePlugin_Name; }
    const juce::String getApplicationVersion() override        { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override                 { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String&) override
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
            return;

        // No audio inputs (0 in / 2 out), so the app never asks for microphone access.
        juce::Array<juce::StandalonePluginHolder::PluginInOuts> channels;
        channels.add ({ 0, 2 });
        auto holder = std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(), false, juce::String{},
                                                                      nullptr, channels, false);
        mainWindow = std::make_unique<PedalCuesWindow> (getApplicationName(),
                                                        juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
                                                        std::move (holder));
        mainWindow->setVisible (true);
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<PedalCuesWindow> mainWindow;
};
} // namespace

juce::JUCEApplicationBase* juce_CreateApplication();
juce::JUCEApplicationBase* juce_CreateApplication() { return new PedalCuesStandaloneApp(); }
