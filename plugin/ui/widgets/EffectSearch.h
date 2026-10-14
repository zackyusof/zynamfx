// Pure, GUI-free matching for the hosted-FX search field. Kept out of
// EffectMenu so it can be exercised without a window (and without JUCE GUI):
// it touches only juce_core. Filtering never scans or instantiates plug-ins;
// it works entirely on the already-listed in-memory bundles.
#pragma once

#include <juce_core/juce_core.h>

#include <cstddef>
#include <vector>

namespace t3k::ui {

// One hosted effect bundle, as listed by listHostedEffectBundles(): display
// metadata plus the exact bundle path, which is the stable identity (an AU and
// a VST3 build of the same plug-in are separate entries).
struct EffectEntry {
  juce::String vendor;
  juce::String name;
  juce::String format;  // "AU" or "VST3"
  juce::String path;
};

// Indices into `entries` matching `query`, sorted deterministically by name,
// then vendor, then format, then path (all case-insensitive where text).
// The query is trimmed and split on whitespace; EVERY token must occur
// (case-insensitive substring) in the entry's combined name + vendor + format.
// Returns {} for an empty or whitespace-only query. No fuzzy matching.
std::vector<size_t> searchEffectEntries(const std::vector<EffectEntry>& entries,
                                        const juce::String& query);

}  // namespace t3k::ui
