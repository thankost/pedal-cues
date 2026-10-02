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

    const char* names[] = { "Quad Cortex", "Whammy V", "MIDI Setup" };
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

    updateBadge.setComponentID ("hdr.update");
    updateBadge.onOpen = [this] { showUpdateDialog (updateBadge.info); };
    updateBadge.onRefresh = [this] { checkForUpdates (true); };
    addAndMakeVisible (updateBadge);
    checkForUpdates (false);

    helpButton.setComponentID ("hdr.help");
    helpButton.setTooltip ("Menu: quick tour, guide, save or share your setup, updates");
    helpButton.onClick = [this] { showHelpMenu (&helpButton); };
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

void PedalCuesEditor::showQcExpression (bool show)
{
    state.setProperty (IDs::qcExpressionView, show, nullptr);
    refreshNow();   // lay out the view now, so the tour can find its targets
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

void PedalCuesEditor::setHelpInMenuBar (bool inMenuBar)
{
    helpButton.setVisible (! inMenuBar);
}

void PedalCuesEditor::showAboutDialog()
{
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "PedalCues " JucePlugin_VersionString,
                                            "Drag-and-drop MIDI cues for the Quad Cortex and Whammy V.\n\n"
                                            "Created by " + ui::author + "\n" + ui::repoUrl + "\n\n"
                                            "Free software under the MIT License. Built with JUCE.\n"
                                            "Not affiliated with Neural DSP or DigiTech.");
}

void PedalCuesEditor::MenuButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? surfaceHi.brighter (0.1f) : (over ? surfaceHi : raised));
    g.fillRoundedRectangle (b, 9.0f);
    g.setColour (outline);
    g.drawRoundedRectangle (b.reduced (0.5f), 9.0f, 1.0f);

    // ☰ drawn as three bars, so it looks the same with every font.
    g.setColour (over ? theme::text : theme::text.withAlpha (0.85f));
    const auto w = b.getWidth() * 0.42f;
    for (int i = -1; i <= 1; ++i)
        g.fillRoundedRectangle (juce::Rectangle<float> (w, 2.2f).withCentre (b.getCentre().translated (0.0f, (float) i * 5.5f)), 1.1f);
}

void PedalCuesEditor::saveDefaultSetup()
{
    const auto ok = state::saveLibrary (state, state::defaultLibraryFile());
    juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, ok ? "Default setup saved" : "Couldn't save",
                                            ok ? "New PedalCues instances will start with your names, colours, MIDI settings and playing preferences."
                                               : "PedalCues couldn't write the default setup file.");
}

void PedalCuesEditor::loadDefaultSetup()
{
    if (! state::loadLibrary (state, state::defaultLibraryFile()))
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "No default setup yet",
                                                "Use Save as Default Setup first.");
}

void PedalCuesEditor::exportSetup()
{
    chooser = std::make_unique<juce::FileChooser> ("Export PedalCues setup",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                       .getChildFile ("PedalCues Setup.xml"),
                                                   "*.xml");
    juce::Component::SafePointer<PedalCuesEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (safe == nullptr || file == juce::File())
                                  return;
                              // An exported setup also carries My drawings and the wiring choice (a per-computer setting).
                              const auto drawings = state::myDrawings();
                              auto setup = safe->state.createCopy();
                              setup.setProperty (IDs::setupViaQcChain, state::getFlag ("setupViaQcChain"), nullptr);
                              if (! state::saveLibrary (setup, file.withFileExtension ("xml"), &drawings))
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Export failed",
                                                                          "PedalCues couldn't write " + file.getFileName() + ".");
                          });
}

void PedalCuesEditor::importSetup()
{
    chooser = std::make_unique<juce::FileChooser> ("Import PedalCues setup",
                                                   juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.xml");
    juce::Component::SafePointer<PedalCuesEditor> safe (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [safe] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              auto drawings = state::myDrawings();   // its drawings are added to My drawings
                              if (safe == nullptr || ! file.existsAsFile())
                                  return;
                              if (state::loadLibrary (safe->state, file, &drawings))
                              {
                                  state::storeMyDrawings();
                                  if (safe->state.hasProperty (IDs::setupViaQcChain))
                                  {
                                      state::setFlag ("setupViaQcChain", (bool) safe->state[IDs::setupViaQcChain]);
                                      safe->state.removeProperty (IDs::setupViaQcChain, nullptr);
                                      safe->refreshNow();   // MIDI Setup shows the imported wiring
                                  }
                              }
                              else
                                  juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::NoIcon, "Not a PedalCues setup",
                                                                          file.getFileName() + " isn't a PedalCues setup file.");
                          });
}

void PedalCuesEditor::showHelpMenu (juce::Component* target, const juce::String& extraItemName, std::function<void()> extra)
{
    juce::PopupMenu m;
    m.addSectionHeader ("PedalCues " JucePlugin_VersionString);
    m.addItem (1, "Quick tour");
    m.addItem (2, "User guide");
    m.addItem (14, "Wiring guide");
    m.addItem (15, "What's new");
    m.addSeparator();
    m.addSectionHeader ("Your setup: names, MIDI settings, preferences");
    m.addItem (10, "Save as default setup");
    m.addItem (11, "Load default setup");
    m.addItem (12, "Export setup");
    m.addItem (13, "Import setup");
    m.addSeparator();
    m.addItem (7, "Check for updates automatically", true, ! state::getFlag (update::disabledFlag));
    m.addSeparator();
    m.addItem (3, "About PedalCues");
    m.addItem (4, "PedalCues on GitHub");
    m.addItem (16, "Report a problem");
    m.addItem (5, "Support PedalCues");
    if (extra)
    {
        m.addSeparator();
        m.addItem (6, extraItemName);
    }

    juce::Component::SafePointer<PedalCuesEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target), [safe, extra] (int result)
    {
        if (safe == nullptr)
            return;
        switch (result)
        {
            case 1:  safe->startTour (0); break;
            case 2:  juce::URL (ui::guideUrl).launchInDefaultBrowser(); break;
            case 3:  safe->showAboutDialog(); break;
            case 4:  juce::URL (ui::repoUrl).launchInDefaultBrowser(); break;
            case 5:  safe->showSupportDialog(); break;
            case 6:  if (extra) extra(); break;
            case 7:  state::setFlag (update::disabledFlag, ! state::getFlag (update::disabledFlag)); break;
            case 10: safe->saveDefaultSetup(); break;
            case 11: safe->loadDefaultSetup(); break;
            case 12: safe->exportSetup(); break;
            case 13: safe->importSetup(); break;
            case 14: ui::showWiringGuide(); break;
            case 15: juce::URL (ui::changelogUrl).launchInDefaultBrowser(); break;
            case 16: ui::problemReportUrl().launchInDefaultBrowser(); break;
            default: break;
        }
    });
}

namespace
{
// The support message, closed with an X in the top-right corner (or Esc) instead of a Close button.
class SupportWindow final : public juce::AlertWindow
{
public:
    using juce::AlertWindow::AlertWindow;

    void initialise()
    {
        closeButton.setTooltip ("Close");
        closeButton.onClick = [this] { exitModalState (0); };
        addAndMakeVisible (closeButton);
        resized();
    }

    void resized() override
    {
        juce::AlertWindow::resized();
        closeButton.setBounds (getWidth() - 40, 10, 28, 28);
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey)
        {
            exitModalState (0);
            return true;
        }
        return juce::AlertWindow::keyPressed (key);
    }

private:
    struct CloseButton final : public juce::Button
    {
        CloseButton() : juce::Button ("Close") {}

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            const auto b = getLocalBounds().toFloat();
            if (over || down)
            {
                g.setColour (juce::Colours::white.withAlpha (down ? 0.18f : 0.1f));
                g.fillEllipse (b);
            }
            const auto c = b.reduced (b.getWidth() * 0.32f);
            g.setColour (juce::Colours::white.withAlpha (over ? 1.0f : 0.7f));
            g.drawLine ({ c.getTopLeft(), c.getBottomRight() }, 2.0f);
            g.drawLine ({ c.getTopRight(), c.getBottomLeft() }, 2.0f);
        }
    };

    CloseButton closeButton;
};
} // namespace

void PedalCuesEditor::showSupportDialog()
{
    auto* w = new SupportWindow ("Support PedalCues",
                                     "Hi, I'm Thanasis. My bandmate Leo and I play in ORIA, a progressive groove metal band from "
                                     "Thessaloniki, Greece. We were tired of programming MIDI by hand for every song, so I built PedalCues.\n\n"
                                     "PedalCues is free and open source, and it will stay that way. If it saves you time at rehearsal "
                                     "or on stage, a coffee helps me keep improving it: new features, fixes, and support for more pedals.\n\n"
                                     "Completely optional. Thanks for playing loud!",
                                     juce::MessageBoxIconType::NoIcon);
    w->addButton ("Buy Me a Coffee", 3);
    w->addButton ("PayPal", 1);
    w->addButton ("Revolut", 2);
    w->initialise();

    // Each service in its own brand colour.
    const std::pair<const char*, std::pair<juce::uint32, juce::uint32>> brandColours[] = {
        { "Buy Me a Coffee", { 0xffffdd00, 0xff000000 } },
        { "PayPal",          { 0xff0070ba, 0xffffffff } },
        { "Revolut",         { 0xffffffff, 0xff191c1f } },
    };
    for (const auto& [name, colours] : brandColours)
        if (auto* b = w->getButton (name))
        {
            b->setColour (juce::TextButton::buttonColourId, juce::Colour (colours.first));
            b->setColour (juce::TextButton::textColourOffId, juce::Colour (colours.second));
        }

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

void PedalCuesEditor::checkForUpdates (bool force)
{
    updateBadge.setInfo ({});   // "checking"

    juce::Component::SafePointer<PedalCuesEditor> safe (this);
    update::check ([safe] (const update::Info& info)
    {
        if (safe == nullptr)
            return;

        safe->updateBadge.setInfo (info);
    }, force);
}

PedalCuesEditor::UpdateBadge::UpdateBadge()
{
    refresh.setTooltip ("Check for updates now");
    refresh.onClick = [this] { if (onRefresh) onRefresh(); };
    addChildComponent (refresh);
}

void PedalCuesEditor::UpdateBadge::setInfo (const update::Info& newInfo)
{
    using S = update::Info::Status;
    info = newInfo;
    setMouseCursor (info.status == S::available ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    setTooltip (info.status == S::available ? "A newer PedalCues is out. Click to see what's new and download it."
              : info.status == S::upToDate  ? "You have the latest PedalCues."
              : info.status == S::failed    ? "Couldn't reach GitHub to check for updates."
              : info.status == S::disabled  ? "Automatic update check is off (turn it on in the menu)."
                                            : juce::String());
    layoutRefresh();
    repaint();
}

juce::String PedalCuesEditor::UpdateBadge::label() const
{
    using S = update::Info::Status;
    switch (info.status)
    {
        case S::available: return "Update to v" + info.latest;
        case S::upToDate:  return "Up to date";
        case S::checking:  return "Checking...";
        case S::failed:    return "Couldn't check";
        case S::disabled:  break;
    }
    return {};
}

namespace
{
constexpr float iconSize = 14.0f, versionGap = 10.0f;

juce::String versionText() { return juce::String ("v") + JucePlugin_VersionString; }
juce::Font versionFont()   { return font (11.5f); }
juce::Font statusFont()    { return font (12.0f, true); }

// Two curved arrows chasing each other (sync / refresh).
void drawSyncIcon (juce::Graphics& g, juce::Rectangle<float> b, float thickness)
{
    const auto c = b.getCentre();
    const auto r = b.getWidth() * 0.36f;
    const auto pi = juce::MathConstants<float>::pi;

    for (int half = 0; half < 2; ++half)
    {
        const auto start = (float) half * pi + 0.35f, end = start + pi - 0.7f;
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, r, r, 0.0f, start, end, true);
        g.strokePath (arc, juce::PathStrokeType (thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Arrowhead at the end of the arc, pointing along the direction of travel.
        const juce::Point<float> tip (c.x + r * std::sin (end), c.y - r * std::cos (end));
        const juce::Point<float> along (std::cos (end), std::sin (end));
        const juce::Point<float> across (std::sin (end), -std::cos (end));
        const auto s = thickness * 2.2f;
        juce::Path head;
        head.addTriangle (tip + along * s, tip - along * (s * 0.3f) + across * s, tip - along * (s * 0.3f) - across * s);
        g.fillPath (head);
    }
}

void drawStatusIcon (juce::Graphics& g, update::Info::Status status, juce::Rectangle<float> b)
{
    using S = update::Info::Status;
    const auto circle = b.reduced (0.75f);
    g.drawEllipse (circle, 1.4f);

    juce::Path p;
    if (status == S::upToDate)
    {
        p.startNewSubPath (b.getX() + b.getWidth() * 0.30f, b.getY() + b.getHeight() * 0.52f);
        p.lineTo (b.getX() + b.getWidth() * 0.45f, b.getY() + b.getHeight() * 0.67f);
        p.lineTo (b.getX() + b.getWidth() * 0.72f, b.getY() + b.getHeight() * 0.36f);
    }
    else if (status == S::available)   // down arrow
    {
        p.startNewSubPath (b.getCentreX(), b.getY() + b.getHeight() * 0.28f);
        p.lineTo (b.getCentreX(), b.getY() + b.getHeight() * 0.70f);
        p.startNewSubPath (b.getX() + b.getWidth() * 0.33f, b.getY() + b.getHeight() * 0.53f);
        p.lineTo (b.getCentreX(), b.getY() + b.getHeight() * 0.72f);
        p.lineTo (b.getX() + b.getWidth() * 0.67f, b.getY() + b.getHeight() * 0.53f);
    }
    else                                // exclamation mark
    {
        p.startNewSubPath (b.getCentreX(), b.getY() + b.getHeight() * 0.27f);
        p.lineTo (b.getCentreX(), b.getY() + b.getHeight() * 0.57f);
        g.fillEllipse (juce::Rectangle<float> (1.8f, 1.8f).withCentre ({ b.getCentreX(), b.getY() + b.getHeight() * 0.73f }));
    }
    g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

// The clickable/status part, right of the version number.
juce::Rectangle<float> PedalCuesEditor::UpdateBadge::textArea() const
{
    using S = update::Info::Status;
    const auto x = juce::GlyphArrangement::getStringWidth (versionFont(), versionText()) + versionGap;
    if (info.status == S::disabled)
        return { x, 0.0f, 0.0f, (float) getHeight() };

    auto w = iconSize + 5.0f + juce::GlyphArrangement::getStringWidth (statusFont(), label());
    if (info.status == S::available)
        w += 16.0f; // pill padding
    return { x, 0.0f, juce::jmin ((float) getWidth() - x - 24.0f, w), (float) getHeight() };
}

void PedalCuesEditor::UpdateBadge::layoutRefresh()
{
    using S = update::Info::Status;
    const auto area = textArea();
    refresh.setBounds (juce::roundToInt (area.getRight()) + 5, (getHeight() - 20) / 2, 20, 20);
    refresh.setVisible (info.status != S::disabled);
    refresh.setEnabled (info.status != S::checking);
}

void PedalCuesEditor::UpdateBadge::mouseUp (const juce::MouseEvent& e)
{
    if (info.status == update::Info::Status::available && textArea().contains (e.position) && onOpen)
        onOpen();
}

void PedalCuesEditor::UpdateBadge::RefreshButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    const auto b = getLocalBounds().toFloat();
    if ((over || down) && isEnabled())
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.18f : 0.1f));
        g.fillEllipse (b);
    }
    g.setColour (isEnabled() ? (over ? theme::text : dim) : dim.withAlpha (0.45f));
    drawSyncIcon (g, b.reduced (3.0f), 1.5f);
}

void PedalCuesEditor::UpdateBadge::paint (juce::Graphics& g)
{
    using S = update::Info::Status;
    const auto h = (float) getHeight();

    g.setColour (dim);
    g.setFont (versionFont());
    g.drawText (versionText(), juce::Rectangle<float> (0.0f, 0.0f, textArea().getX(), h), juce::Justification::centredLeft);

    if (info.status == S::disabled)
        return;

    auto area = textArea();
    auto colour = info.status == S::upToDate ? ledGreen : dim;

    if (info.status == S::available)
    {
        const auto pill = area.reduced (0.0f, 1.0f);
        g.setColour (isMouseOver() ? accent.brighter (0.15f) : accent);
        g.fillRoundedRectangle (pill, pill.getHeight() * 0.5f);
        area = pill.reduced (8.0f, 0.0f);
        colour = juce::Colours::black;
    }

    g.setColour (colour);
    const auto icon = area.removeFromLeft (iconSize).withSizeKeepingCentre (iconSize, iconSize);
    if (info.status == S::checking)
        drawSyncIcon (g, icon, 1.4f);
    else
        drawStatusIcon (g, info.status, icon);

    area.removeFromLeft (5.0f);
    g.setFont (statusFont());
    g.drawText (label(), area, juce::Justification::centredLeft);
}

void PedalCuesEditor::showUpdateDialog (const update::Info& info)
{
    juce::StringArray lines;
    lines.addLines (info.notes.replace ("\r", "").replace ("**", "").replace ("## ", ""));
    for (int i = lines.size(); --i >= 0;)
        if (lines[i].startsWith ("Full Changelog"))
            lines.remove (i);
    auto notes = lines.joinIntoString ("\n").trim();
    if (notes.length() > 700)
        notes = notes.substring (0, 700).upToLastOccurrenceOf ("\n", false, false) + "\n...";

    auto* w = new juce::AlertWindow ("PedalCues v" + info.latest + " is available",
                                     "You have v" JucePlugin_VersionString ".\n\n"
                                     + (notes.isNotEmpty() ? "What's new:\n" + notes + "\n\n" : juce::String())
                                    #if JUCE_MAC
                                     + "Download the installer, close your DAW, then open it and click Install.",
                                    #else
                                     + "Download the zip, close your DAW, then replace the plugin files the same way you installed them.",
                                    #endif
                                     juce::MessageBoxIconType::NoIcon);
    w->addButton ("Download", 1);
    w->addButton ("Release page", 2);
    w->addButton ("Later", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    if (auto* b = w->getButton ("Download"))
    {
        b->setColour (juce::TextButton::buttonColourId, accent);
        b->setColour (juce::TextButton::textColourOffId, juce::Colours::black);
    }

    w->enterModalState (true, juce::ModalCallbackFunction::create ([info] (int result)
    {
        if (result == 1)
            juce::URL (info.downloadUrl).launchInDefaultBrowser();
        else if (result == 2)
            juce::URL (info.pageUrl).launchInDefaultBrowser();
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

    // Tempo pill on the right, left of the help button. In the standalone app it's clickable (set tempo).
    const auto pill = tempoPill();
    const auto manual = PedalCuesProcessor::isStandalone();
    g.setColour (manual && isMouseOver() && pill.contains (getMouseXYRelative().toFloat()) ? surfaceHi : background);
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
    g.drawText (manual ? "set tempo" : "host tempo", pill.withTrimmedLeft (104.0f), juce::Justification::centredLeft);
}

juce::Rectangle<float> PedalCuesEditor::tempoPill() const
{
    auto right = getLocalBounds().removeFromTop (64).reduced (18, 0);
    right.removeFromRight (44);
    return right.removeFromRight (190).withSizeKeepingCentre (190, 30).toFloat();
}

void PedalCuesEditor::mouseMove (const juce::MouseEvent& e)
{
    if (! PedalCuesProcessor::isStandalone())
        return;
    const auto over = tempoPill().contains (e.position);
    setMouseCursor (over ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint (tempoPill().toNearestInt().expanded (2));
}

void PedalCuesEditor::mouseUp (const juce::MouseEvent& e)
{
    if (PedalCuesProcessor::isStandalone() && tempoPill().contains (e.position))
        showTempoEditor();
}

namespace
{
// Tempo for the standalone app: type it, drag it, or tap it.
class TempoPanel final : public juce::Component
{
public:
    explicit TempoPanel (PedalCuesProcessor& p) : proc (p)
    {
        title.setText ("Song tempo (BPM)", juce::dontSendNotification);
        title.setFont (font (12.0f, true));
        title.setColour (juce::Label::textColourId, dim);
        addAndMakeVisible (title);

        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 26);
        slider.setRange (20.0, 300.0, 0.5);
        slider.setValue (proc.getHostBpm(), juce::dontSendNotification);
        slider.onValueChange = [this] { proc.setManualBpm (slider.getValue()); };
        addAndMakeVisible (slider);

        tap.setTooltip ("Click in time with the song a few times");
        tap.onClick = [this] { tapped(); };
        addAndMakeVisible (tap);

        hint.setText ("Sets how long treadle and expression moves last when you click their "
                      "play button. In your DAW the plugin follows the project tempo.", juce::dontSendNotification);
        hint.setFont (font (11.0f));
        hint.setColour (juce::Label::textColourId, dim);
        hint.setJustificationType (juce::Justification::topLeft);
        addAndMakeVisible (hint);

        setSize (320, 132);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (12, 10);
        title.setBounds (r.removeFromTop (20));
        r.removeFromTop (4);
        auto row = r.removeFromTop (30);
        tap.setBounds (row.removeFromRight (56));
        row.removeFromRight (8);
        slider.setBounds (row);
        r.removeFromTop (8);
        hint.setBounds (r);
    }

private:
    void tapped()
    {
        const auto now = juce::Time::getMillisecondCounterHiRes();
        if (! taps.empty() && now - taps.back() > 2000.0)
            taps.clear();
        taps.push_back (now);
        if (taps.size() > 8)
            taps.erase (taps.begin());
        if (taps.size() >= 2)
            slider.setValue (60000.0 * (double) (taps.size() - 1) / (taps.back() - taps.front()), juce::sendNotification);
    }

    PedalCuesProcessor& proc;
    juce::Label title, hint;
    juce::Slider slider;
    juce::TextButton tap { "Tap" };
    std::vector<double> taps;
};
} // namespace

void PedalCuesEditor::showTempoEditor()
{
    juce::CallOutBox::launchAsynchronously (std::make_unique<TempoPanel> (pedalProcessor),
                                            getScreenBounds().getPosition().toFloat().isOrigin() ? tempoPill().toNearestInt()
                                                                                                  : tempoPill().toNearestInt(),
                                            this);
}

void PedalCuesEditor::resized()
{
    auto header = getLocalBounds().removeFromTop (64).reduced (18, 13);

    helpButton.setBounds (header.removeFromRight (38).withSizeKeepingCentre (34, 34));
    updateBadge.setBounds (18 + 40 + 10, 36, 270, 20);

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
