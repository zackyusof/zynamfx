#include "EffectSearch.h"

#include <algorithm>

namespace t3k::ui {

namespace {

// Split on whitespace runs, keeping token order; "  fab  au " -> {"fab","au"}.
juce::StringArray tokenise(const juce::String& query) {
  juce::StringArray tokens;
  const int n = query.length();
  int start = -1;
  for (int i = 0; i < n; ++i) {
    const bool space = juce::CharacterFunctions::isWhitespace(query[i]);
    if (!space && start < 0)
      start = i;
    if ((space || i == n - 1) && start >= 0) {
      tokens.add(query.substring(start, space ? i : i + 1));
      start = -1;
    }
  }
  return tokens;
}

}  // namespace

std::vector<size_t> searchEffectEntries(const std::vector<EffectEntry>& entries,
                                        const juce::String& query) {
  std::vector<size_t> matches;

  const auto tokens = tokenise(query.trim());
  if (tokens.isEmpty())
    return matches;

  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& e = entries[i];
    // "\n" separates the fields so a token cannot span two of them by accident.
    const juce::String haystack = e.name + "\n" + e.vendor + "\n" + e.format;
    bool all = true;
    for (int t = 0; t < tokens.size(); ++t)
      if (!haystack.containsIgnoreCase(tokens[t])) {
        all = false;
        break;
      }
    if (all)
      matches.push_back(i);
  }

  std::sort(matches.begin(), matches.end(), [&entries](size_t ia, size_t ib) {
    const auto& a = entries[ia];
    const auto& b = entries[ib];
    if (const int c = a.name.compareIgnoreCase(b.name); c != 0) return c < 0;
    if (const int c = a.vendor.compareIgnoreCase(b.vendor); c != 0) return c < 0;
    if (const int c = a.format.compareIgnoreCase(b.format); c != 0) return c < 0;
    if (const int c = a.path.compareIgnoreCase(b.path); c != 0) return c < 0;
    return ia < ib;  // total order, so identical metadata still sorts stably
  });
  return matches;
}

}  // namespace t3k::ui
