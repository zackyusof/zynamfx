// Build identity: which repository this build came from and its channel.
// Substituted at configure time (plugin/CMakeLists.txt -> cmake/ForkIdentity.cmake):
//   development (Gitea)  T3K_FORK_REPO_URL=https://git.zyusof.net/zack/zynamfx
//   public      (GitHub) T3K_FORK_REPO_URL=https://github.com/zackyusof/zynamfx
// The in-app logo link and the Settings footer both read these, so a Gitea build
// points at Gitea and a GitHub build points at GitHub without editing source.
// The fallbacks keep source-only compiles (the UI testbed, unit tests) working.
#pragma once

#include <juce_core/juce_core.h>

#ifndef T3K_REPO_URL
#define T3K_REPO_URL "https://git.zyusof.net/zack/zynamfx"
#endif
#ifndef T3K_BUILD_CHANNEL
#define T3K_BUILD_CHANNEL "development"
#endif
#ifndef T3K_VERSION
#define T3K_VERSION ""
#endif
#ifndef T3K_PRERELEASE
#define T3K_PRERELEASE ""
#endif

namespace t3k::ui::build {

// "development" (Gitea) or "public" (GitHub).
inline juce::String channel() { return T3K_BUILD_CHANNEL; }
// The repository this build was produced from.
inline juce::String repoUrl() { return T3K_REPO_URL; }
// The product version (from the repo-root VERSION file); empty in a
// source-only compile that does not define it.
inline juce::String version() { return T3K_VERSION; }
// The full label shown to users and reported to support, e.g. "2.0.2-beta.1". The prerelease part is
// empty in a stable build, so this equals version() there.
inline juce::String displayVersion() {
  const juce::String pre(T3K_PRERELEASE);
  return pre.isEmpty() ? version() : version() + "-" + pre;
}
// One-line identity for the Settings footer, e.g.
// "development build (git.zyusof.net/zack/zynamfx)".
inline juce::String label() {
  return juce::String(T3K_BUILD_CHANNEL) + " build (" + T3K_REPO_URL + ")";
}

}  // namespace t3k::ui::build
