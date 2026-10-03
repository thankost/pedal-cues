#pragma once

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

// Fuzzy search for the preset, performance and tile lists: every word of the query must be found in the text,
// either as letters in order ("drpc" finds "Drop C") or, for longer words, with one typo ("hevy" finds "Heavy").
// Matches at word starts and unbroken runs score higher, so the best match comes first.
namespace fuzzy
{
namespace detail
{
    // Letters of 'word' in order inside 'text'; -1 if they aren't all there.
    inline int subsequenceScore (const juce::String& word, const juce::String& text)
    {
        if (const auto at = text.indexOf (word); at >= 0)   // the word as it is: best
            return 100 + word.length() * 10 + (at == 0 || ! juce::CharacterFunctions::isLetterOrDigit (text[at - 1]) ? 40 : 0);

        int score = 0, ti = 0, previous = -2;
        for (int wi = 0; wi < word.length(); ++wi)
        {
            const auto c = word[wi];
            while (ti < text.length() && text[ti] != c)
                ++ti;
            if (ti >= text.length())
                return -1;
            const auto wordStart = ti == 0 || ! juce::CharacterFunctions::isLetterOrDigit (text[ti - 1]);
            score += 10 + (ti == previous + 1 ? 8 : 0) + (wordStart ? 12 : 0);
            previous = ti++;
        }
        return score;
    }

    // Optimal string alignment distance, capped: 0, 1 or "more" (2).
    inline int editDistanceUpToOne (const juce::String& a, const juce::String& b)
    {
        const auto n = a.length(), m = b.length();
        if (std::abs (n - m) > 1)
            return 2;
        std::vector<std::vector<int>> d ((size_t) n + 1, std::vector<int> ((size_t) m + 1));
        for (int i = 0; i <= n; ++i) d[(size_t) i][0] = i;
        for (int j = 0; j <= m; ++j) d[0][(size_t) j] = j;
        for (int i = 1; i <= n; ++i)
            for (int j = 1; j <= m; ++j)
            {
                const auto cost = a[i - 1] == b[j - 1] ? 0 : 1;
                auto v = std::min ({ d[(size_t) i - 1][(size_t) j] + 1, d[(size_t) i][(size_t) j - 1] + 1, d[(size_t) i - 1][(size_t) j - 1] + cost });
                if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1])
                    v = std::min (v, d[(size_t) i - 2][(size_t) j - 2] + 1);   // swapped letters
                d[(size_t) i][(size_t) j] = v;
            }
        return std::min (2, d[(size_t) n][(size_t) m]);
    }

    inline int wordScore (const juce::String& word, const juce::String& text)
    {
        if (const auto s = subsequenceScore (word, text); s >= 0)
            return s;
        if (word.length() < 4)
            return -1;
        // One typo against any word of the text, or against the start of one ("hevy" -> "heavy", "crnch" -> "crunch").
        for (const auto& w : juce::StringArray::fromTokens (text, " |-_/.,()", {}))
            if (w.isNotEmpty() && (editDistanceUpToOne (word, w) <= 1 || editDistanceUpToOne (word, w.substring (0, word.length())) <= 1))
                return 5;
        return -1;
    }
}

// How well 'query' matches 'text' (higher is better), or -1 for no match. An empty query matches everything with 0.
// 'exact' (a location like "SL1 | 2B", messages, notes) is only searched as plain text, so a short query doesn't
// pick up scattered letters from it.
inline int score (const juce::String& query, const juce::String& text, const juce::String& exact = {})
{
    const auto haystack = text.toLowerCase();
    const auto plain = exact.toLowerCase();
    int total = 0;
    for (const auto& word : juce::StringArray::fromTokens (query.toLowerCase(), " ", {}))
    {
        if (word.isEmpty())
            continue;
        auto s = detail::wordScore (word, haystack);
        if (s < 0 && plain.contains (word))
            s = 50;
        if (s < 0)
            return -1;
        total += s;
    }
    return total;
}

// The indices of 'texts' that match, best first (ties keep the list order). An empty query keeps every item, in order.
inline std::vector<int> order (const juce::String& query, const juce::StringArray& texts, const juce::StringArray& exact = {})
{
    std::vector<std::pair<int, int>> scored;   // (score, index)
    for (int i = 0; i < texts.size(); ++i)
        if (const auto s = score (query, texts[i], exact[i]); s >= 0)
            scored.emplace_back (s, i);
    if (query.trim().isNotEmpty())
        std::stable_sort (scored.begin(), scored.end(), [] (const auto& a, const auto& b) { return a.first > b.first; });
    std::vector<int> result;
    for (const auto& [s, i] : scored)
        result.push_back (i);
    return result;
}
}
