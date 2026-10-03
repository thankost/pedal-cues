#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

namespace ui
{
// Implemented by the editor: switches pages and resolves tour targets (component IDs).
struct TourHost
{
    virtual ~TourHost() = default;
    virtual void showPage (int index) = 0;
    virtual void showQcExpression (bool) = 0;
    virtual void showWhammyDt (bool) = 0;
    virtual void showQuadCortex (bool) = 0;
    virtual juce::Rectangle<int> targetBounds (const juce::StringArray& componentIds) = 0;
    virtual void closeTour (bool finished) = 0;
};

struct TourStep
{
    int page;
    juce::StringArray targets;
    juce::String title, body;
    bool qcExpression = false;   // Quad Cortex page: show the Expression view (else Scenes & Stomps)
    bool whammyDt = false;       // Whammy page: show Whammy DT > Drop Tune for this step (the user's choice comes back after)
    bool anyUnit = false;        // first tab: leave the user's amp unit (other first-tab steps show the Quad Cortex page)
};

const std::vector<TourStep>& tourSteps();

// Dims the whole editor except the current target and shows a coach-mark card.
class TourOverlay final : public juce::Component
{
public:
    TourOverlay (TourHost&, int startStep);

    void setStep (int index);
    int getStep() const { return step; }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void layoutCard();

    TourHost& host;
    int step = 0;
    juce::Rectangle<int> hole, card;

    juce::TextButton backButton { "Back" }, nextButton { "Next" }, skipButton { "Skip tour" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TourOverlay)
};
} // namespace ui
