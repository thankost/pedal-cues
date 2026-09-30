#pragma once

#include <juce_events/juce_events.h>

#include <functional>

// Asks GitHub for the latest PedalCues release, at most once per app run.
namespace update
{
inline const juce::String repo { "thankost/pedal-cues" };
inline constexpr const char* disabledFlag = "updateCheckDisabled";

struct Info
{
    enum class Status { checking, upToDate, available, failed, disabled };

    Status status = Status::checking;
    juce::String latest;       // e.g. "0.4.12"
    juce::String notes;        // release notes, plain text
    juce::String pageUrl;      // release page
    juce::String downloadUrl;  // zip for this platform
};

// True if version a (e.g. "0.4.12") is newer than version b. A leading "v" is ignored.
bool isNewer (const juce::String& a, const juce::String& b);

// Calls done on the message thread. Uses the cached answer unless force is true.
void check (std::function<void (const Info&)> done, bool force = false);

// Blocking request to GitHub (use check() from the UI). Exposed for the online test.
Info fetchLatest();

// For the screenshot tool: answer every check with this, without going online.
void setOfflineResult (const Info&);
} // namespace update
