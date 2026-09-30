#include "PluginEditor.h"

using namespace theme;

namespace
{
const juce::Colour tabColours[] = { qcBlue, whammyRed, accent };

int liveEditors = 0; // message thread only

juce::Component* findById (juce::Component& root, const juce::String& id)
{
    if (root.getComponentID() == id)
        return &root;

    for (auto* child : root.getChildren())
        if (child->isVisible())
            if (auto* found = findById (*child, id))
                return found;

    return nullptr;
}
} // namespace

void PedalCuesEditor::TabBar::paint (juce::Graphics& g)
{
    g.setColour (background);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 11.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 11.0f, 1.0f);
}

//==============================================================================
PedalCuesEditor::PedalCuesEditor (PedalCuesProcessor& p, bool allowFirstRunTour)
    : AudioProcessorEditor (p), pedalProcessor (p), state (p.state)
{
    ++liveEditors;
    setLookAndFeel (&lookAndFeel.get());
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel.get()); // popup menus and dialogs

    tabBar.setComponentID ("hdr.tabs");
    addAndMakeVisible (tabBar);

    const char* names[] = { "Quad Cortex", "Whammy V", "Settings" };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = tabButtons.add (new juce::TextButton (names[i]));
        b->setClickingTogglesState (true);
        b->setRadioGroupId (1001);
        b->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        b->setColour (juce::TextButton::buttonOnColourId, tabColours[i]);
        b->setColour (juce::TextButton::textColourOffId, dim);
        b->setColour (juce::TextButton::textColourOnId, i == 1 ? juce::Colours::white : juce::Colours::black);
        b->onClick = [this, i] { showPage (i); };
        tabBar.addAndMakeVisible (b);
    }

    helpButton.setComponentID ("hdr.help");
    helpButton.setTooltip ("Quick tour and user guide");
    helpButton.onClick = [this] { showHelpMenu(); };
    addAndMakeVisible (helpButton);

    pages.push_back (ui::makeQcPage (p));
    pages.push_back (ui::makeWhammyPage (p));
    pages.push_back (ui::makeSettingsPage (p, [this] { startTour (0); }));
    for (auto& page : pages)
        addChildComponent (*page);

    state.addListener (this);

    setResizable (true, true);
    setResizeLimits (980, 680, 2400, 1600);
    setSize (1120, 760);

    showPage (0);
    timerCallback();
    startTimerHz (2);

    if (allowFirstRunTour && ! state::getFlag (tourDoneFlag))
    {
        juce::Component::SafePointer<PedalCuesEditor> safe (this);
        juce::Timer::callAfterDelay (400, [safe]
        {
            if (safe != nullptr && safe->isShowing() && safe->tour == nullptr)
                safe->startTour (0);
        });
    }
}

PedalCuesEditor::~PedalCuesEditor()
{
    state.removeListener (this);
    tour.reset();
    setLookAndFeel (nullptr);

    if (--liveEditors == 0)
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
}

void PedalCuesEditor::showPage (int index)
{
    currentPage = juce::jlimit (0, (int) pages.size() - 1, index);
    for (int i = 0; i < (int) pages.size(); ++i)
    {
        pages[(size_t) i]->setVisible (i == currentPage);
        tabButtons[i]->setToggleState (i == currentPage, juce::dontSendNotification);
    }
    repaint();
}

juce::Rectangle<int> PedalCuesEditor::targetBounds (const juce::StringArray& ids)
{
    juce::Rectangle<int> result;
    for (const auto& id : ids)
        if (auto* c = findById (*this, id))
        {
            const auto r = getLocalArea (c->getParentComponent(), c->getBounds());
            result = result.isEmpty() ? r : result.getUnion (r);
        }
    return result;
}

void PedalCuesEditor::startTour (int step)
{
    tour = std::make_unique<ui::TourOverlay> (*this, step);
    addAndMakeVisible (*tour);
    tour->setBounds (getLocalBounds());
    tour->setStep (step);
    tour->grabKeyboardFocus();
}

void PedalCuesEditor::closeTour (bool)
{
    state::setFlag (tourDoneFlag, true);

    // Called from the overlay's own buttons: delete it after the click has finished.
    juce::Component::SafePointer<PedalCuesEditor> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe != nullptr)
        {
            safe->tour.reset();
            safe->showPage (0);
        }
    });
}

void PedalCuesEditor::showHelpMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("PedalCues " JucePlugin_VersionString);
    m.addItem (1, "Show quick tour");
    m.addItem (2, "Open user guide (web)");
    m.addSeparator();
    m.addItem (3, "About PedalCues");
    m.addItem (4, "Project on GitHub");
    m.addSeparator();
    m.addItem (5, "Support PedalCues (optional)...");

    juce::Component::SafePointer<PedalCuesEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&helpButton), [safe] (int result)
    {
        if (safe == nullptr)
            return;
        if (result == 1)
            safe->startTour (0);
        else if (result == 2)
            juce::URL (ui::guideUrl).launchInDefaultBrowser();
        else if (result == 3)
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "PedalCues " JucePlugin_VersionString,
                                                    "Drag-and-drop MIDI cues for the Quad Cortex and Whammy V.\n\n"
                                                    "Created by " + ui::author + "\n" + ui::repoUrl + "\n\n"
                                                    "Free software under the MIT License. Built with JUCE.\n"
                                                    "Not affiliated with Neural DSP or DigiTech.");
        else if (result == 4)
            juce::URL (ui::repoUrl).launchInDefaultBrowser();
        else if (result == 5)
            safe->showSupportDialog();
    });
}

void PedalCuesEditor::showSupportDialog()
{
    auto* w = new juce::AlertWindow ("Support PedalCues",
                                     "Hi, I'm Thanasis. My bandmate Leo and I play in ORIA, a progressive groove metal band from "
                                     "Thessaloniki, Greece. We were tired of programming MIDI by hand for every song, so I built PedalCues.\n\n"
                                     "PedalCues is free and open source, and it will stay that way. If it saves you time at rehearsal "
                                     "or on stage, a coffee helps me keep improving it: new features, fixes, and support for more pedals.\n\n"
                                     "Completely optional. Thanks for playing loud!",
                                     juce::MessageBoxIconType::NoIcon);
    w->addButton ("Buy Me a Coffee", 3);
    w->addButton ("PayPal", 1);
    w->addButton ("Revolut", 2);
    w->addButton ("Close", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([] (int result)
    {
        if (result == 1)
            juce::URL (ui::paypalUrl).launchInDefaultBrowser();
        else if (result == 2)
            juce::URL (ui::revolutUrl).launchInDefaultBrowser();
        else if (result == 3)
            juce::URL (ui::coffeeUrl).launchInDefaultBrowser();
    }), true);
}

void PedalCuesEditor::handleAsyncUpdate()
{
    for (auto& page : pages)
        page->refresh();
}

void PedalCuesEditor::timerCallback()
{
    const auto bpm = pedalProcessor.getHostBpm();
    if (std::abs (bpm - shownBpm) > 0.01)
    {
        shownBpm = bpm;
        repaint (getLocalBounds().removeFromTop (64));
    }
}

void PedalCuesEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto header = getLocalBounds().removeFromTop (64);
    g.setGradientFill (juce::ColourGradient (surfaceHi, 0.0f, 0.0f, surface, 0.0f, (float) header.getBottom(), false));
    g.fillRect (header);
    g.setColour (outline);
    g.drawHorizontalLine (header.getBottom() - 1, 0.0f, (float) getWidth());

    // Accent line in the colour of the current pedal.
    g.setColour (tabColours[currentPage]);
    g.fillRect (0, header.getBottom() - 2, getWidth(), 2);

    // Logo: same artwork as the app icon.
    auto h = header.reduced (18, 0);
    drawAppIcon (g, h.removeFromLeft (40).withSizeKeepingCentre (40, 40).toFloat(), false);

    h.removeFromLeft (12);
    auto titleArea = h.removeFromLeft (170);
    g.setColour (text);
    g.setFont (font (20.0f, true));
    g.drawText ("PedalCues", titleArea.removeFromTop (titleArea.getHeight() / 2 + 6), juce::Justification::bottomLeft);
    g.setColour (dim);
    g.setFont (font (11.0f));
    g.drawText ("Quad Cortex + Whammy V", titleArea, juce::Justification::topLeft);

    // Tempo pill on the right, left of the help button.
    auto right = header.reduced (18, 0);
    right.removeFromRight (44);
    const auto pill = right.removeFromRight (190).withSizeKeepingCentre (190, 30).toFloat();
    g.setColour (background);
    g.fillRoundedRectangle (pill, 15.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (pill.reduced (0.5f), 15.0f, 1.0f);
    g.setColour (ledGreen);
    g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ pill.getX() + 16.0f, pill.getCentreY() }));
    g.setColour (text);
    g.setFont (font (13.0f, true));
    g.drawText (juce::String (shownBpm, 1) + " BPM", pill.withTrimmedLeft (28.0f).withWidth (80.0f), juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (font (11.5f));
    g.drawText ("host tempo", pill.withTrimmedLeft (104.0f), juce::Justification::centredLeft);
}

void PedalCuesEditor::resized()
{
    auto header = getLocalBounds().removeFromTop (64).reduced (18, 13);

    helpButton.setBounds (header.removeFromRight (38).withSizeKeepingCentre (34, 34));

    const auto tabsWidth = 3 * 130 + 8;
    tabBar.setBounds (juce::Rectangle<int> (tabsWidth, 38).withCentre ({ getWidth() / 2, header.getCentreY() }));
    auto t = tabBar.getLocalBounds().reduced (4);
    for (auto* b : tabButtons)
        b->setBounds (t.removeFromLeft (130));

    auto content = getLocalBounds().withTrimmedTop (64);
    for (auto& page : pages)
        page->setBounds (content);

    if (tour != nullptr)
    {
        tour->setBounds (getLocalBounds());
        tour->setStep (tour->getStep());
    }
}
