#include "Update.h"
#include "State.h"

namespace update
{
namespace
{
juce::String thisVersion() { return JucePlugin_VersionString; }

juce::String assetName()
{
   #if JUCE_MAC
    return "PedalCues-macOS.pkg";   // the installer (a zip is published too, for manual installs)
   #elif JUCE_LINUX
    return "PedalCues-Linux.zip";
   #else
    return "PedalCues-Windows.zip";
   #endif
}

// Shared by every editor in the process; message thread only.
struct Cache
{
    bool done = false, running = false, offline = false;
    Info info;
    std::vector<std::function<void (const Info&)>> waiting;
};

Cache& cache()
{
    static Cache c;
    return c;
}

} // namespace

Info fetchLatest()
{
    Info info;
    info.status = Info::Status::failed;
    const auto headers = "User-Agent: PedalCues/" + thisVersion() + "\r\n";

    // 1. The release page redirects to .../releases/tag/vX.Y.Z. Unlike the API it isn't limited to
    //    60 requests an hour per network, which shared (office, venue) networks run out of quickly.
    juce::StringPairArray responseHeaders;
    int status = 0;
    const auto pageOptions = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                 .withConnectionTimeoutMs (6000)
                                 .withNumRedirectsToFollow (0)
                                 .withExtraHeaders (headers)
                                 .withResponseHeaders (&responseHeaders)
                                 .withStatusCode (&status);

    juce::String tag;
    if (auto stream = juce::URL ("https://github.com/" + repo + "/releases/latest").createInputStream (pageOptions))
    {
        const auto location = responseHeaders.getValue ("Location", {}).isNotEmpty() ? responseHeaders.getValue ("Location", {})
                                                                                     : responseHeaders.getValue ("location", {});
        if (location.contains ("/releases/tag/"))
            tag = juce::URL::removeEscapeChars (location.fromLastOccurrenceOf ("/releases/tag/", false, false)).trim();
    }

    if (tag.isEmpty())
        return info;

    info.latest = tag.trimCharactersAtStart ("vV");
    info.pageUrl = "https://github.com/" + repo + "/releases/tag/" + tag;
    info.downloadUrl = "https://github.com/" + repo + "/releases/download/" + tag + "/" + assetName();
    info.status = isNewer (info.latest, thisVersion()) ? Info::Status::available : Info::Status::upToDate;

    // 2. Only when there's something new: best-effort release notes from the API (may be rate-limited).
    if (info.status == Info::Status::available)
    {
        const auto apiOptions = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                    .withConnectionTimeoutMs (6000)
                                    .withExtraHeaders (headers + "Accept: application/vnd.github+json");
        if (auto stream = juce::URL ("https://api.github.com/repos/" + repo + "/releases/tags/" + tag).createInputStream (apiOptions))
            info.notes = juce::JSON::parse (stream->readEntireStreamAsString()).getProperty ("body", {}).toString().trim();
    }
    return info;
}

namespace
{
void finish (const Info& info)
{
    auto& c = cache();
    c.info = info;
    c.done = true;
    c.running = false;

    auto waiting = std::move (c.waiting);
    c.waiting.clear();
    for (auto& callback : waiting)
        callback (info);
}
} // namespace

bool isNewer (const juce::String& a, const juce::String& b)
{
    juce::StringArray pa, pb;
    pa.addTokens (a.trim().trimCharactersAtStart ("vV"), ".", {});
    pb.addTokens (b.trim().trimCharactersAtStart ("vV"), ".", {});

    for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
    {
        const auto x = pa[i].getIntValue(), y = pb[i].getIntValue();
        if (x != y)
            return x > y;
    }
    return false;
}

void check (std::function<void (const Info&)> done, bool force)
{
    auto& c = cache();

    if (state::getFlag (disabledFlag))
    {
        Info info;
        info.status = Info::Status::disabled;
        done (info);
        return;
    }

    if (c.offline || (c.done && ! force))
    {
        done (c.info);
        return;
    }

    c.waiting.push_back (std::move (done));
    if (c.running)
        return;

    c.running = true;
    juce::Thread::launch ([]
    {
        const auto info = fetchLatest();
        juce::MessageManager::callAsync ([info] { finish (info); });
    });
}

void setOfflineResult (const Info& info)
{
    auto& c = cache();
    c.offline = true;
    c.done = true;
    c.info = info;
}
} // namespace update
