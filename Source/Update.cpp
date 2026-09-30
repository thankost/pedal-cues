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
    return "PedalCues-macOS.zip";
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

    const auto options = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                             .withConnectionTimeoutMs (6000)
                             .withExtraHeaders ("User-Agent: PedalCues/" + thisVersion() + "\r\nAccept: application/vnd.github+json");

    if (auto stream = juce::URL ("https://api.github.com/repos/" + repo + "/releases/latest").createInputStream (options))
    {
        const auto json = juce::JSON::parse (stream->readEntireStreamAsString());
        const auto tag = json.getProperty ("tag_name", {}).toString();

        if (tag.isNotEmpty())
        {
            info.latest = tag.trimCharactersAtStart ("vV");
            info.notes = json.getProperty ("body", {}).toString().trim();
            info.pageUrl = json.getProperty ("html_url", {}).toString();
            info.downloadUrl = "https://github.com/" + repo + "/releases/download/" + tag + "/" + assetName();
            info.status = isNewer (info.latest, thisVersion()) ? Info::Status::available : Info::Status::upToDate;
        }
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
