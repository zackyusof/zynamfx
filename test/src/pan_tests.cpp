// Per-slot pan resolution tests (see docs/pan-in-a-chain.md).
//
// The shipped control bound every populated slot of a lane to the *same*
// `chainPanLeft`/`chainPanRight` parameter, so all slots moved together (and
// the rail's link pill mirrored one lane onto the other). The design gives
// every populated slot its own stored placement (`panNormalized` + `panSet`)
// and resolves each lane's effective placement as the last placed enabled
// block along that lane's OWN signal path, else the rail fallback.
//
// A placement on a block in the SHARED REJOIN TAIL is not a lane placement:
// the tail runs after the merge, on the already-merged stereo image, so its
// placement COMPOSES with (does not override) the two lanes' own placements —
// a constant-power balance of the result. Shared-tail blocks are excluded from
// resolveLanePan and resolved separately by resolveTailPan.
//
// These tests drive that resolution through the PUBLIC surface only:
//   set   -> ProcessorChain::setBlockParam(id, "pan", v)   (0..1; v < 0 clears)
//   read  -> ProcessorChain::getChainState()["panLeft" / "panRight"] and the
//            per-block "placesBothLanes" flag a shared-tail placement carries
//   audio -> ProcessorChain::processBlock (the DSP-level composition and the
//            identity cases below)
// Nothing here reaches into resolveLanePan, resolveTailPan, ChainBlock or the
// internal lanes.
//
// The rig mirrors branch_tests.cpp: chains are seeded through
// setStateInformation with the IR bytes embedded (cache-first, no network),
// and no audio device is opened. A branched/rejoined chain only exercises the
// real resolution path once the RT routing state is live, so each case pumps
// a few callbacks (processStereo) before reading the resolved placement.

#include "Processor.h"
#include "chain_test_helpers.h"

#include <gtest/gtest.h>

#include <utility>
#include <vector>

namespace {

constexpr int kBlock = 512;

// The resolved lane placement out of getChainState's top-level payload.
double lanePan(const juce::var& state, const char* key) {
  return static_cast<double>(state[key]);
}

// True when the state marks `blockId` as a placement that runs on the merged
// image and so applies to both lanes (a shared-tail block with a pan set).
// The key may sit on the item or inside its `params` object; a missing key
// reads as false, exactly like an explicit false.
bool placesBothLanes(const juce::var& state, const juce::String& blockId) {
  for (const char* laneKey : {"chain", "chainRight"}) {
    const auto* arr = state[laneKey].getArray();
    if (arr == nullptr)
      continue;
    for (const auto& item : *arr) {
      if (item["blockId"].toString() != blockId)
        continue;
      if (static_cast<bool>(item["placesBothLanes"]))
        return true;
      if (static_cast<bool>(item["params"]["placesBothLanes"]))
        return true;
      return false;
    }
  }
  return false;
}

// Largest |a[i] - gain*b[i]| over the settled region (skips the first second:
// block wet-fades, smoothers and per-channel filter states converging). This
// is the composition check: the tail placement should scale each output
// channel by its constant-power gain, nothing else.
float maxAbsScaledDiff(const std::vector<float>& a, const std::vector<float>& b, float gain,
                       size_t skip = 48000) {
  float maxDiff = 0.0f;
  const size_t n = std::min(a.size(), b.size());
  for (size_t i = skip; i < n; ++i)
    maxDiff = std::max(maxDiff, std::abs(a[i] - gain * b[i]));
  return maxDiff;
}

// Pump a few callbacks so the processor's RT routing state (rtStereoChains,
// rtBranchTapIndex/rtBranchMergeIndex) is set before the resolved placement
// is read. No assertion on the audio: this only advances the live state.
void pumpAudio(TONE3000Processor& proc, int blocks = 16) {
  (void)processStereo(proc, makeNoise(blocks * kBlock, 4242, 0.05f), kBlock);
}

// Seed a rig, prepare it and wait until every tone block has loaded (the same
// hermetic shape the branch suite uses; no audio device is involved).
void seedPrepared(ChainTestProcessor& proc, const std::vector<juce::String>& leftIds,
                  const std::vector<juce::String>& rightIds) {
  // Play config first, like the branch suite: the restored blocks' engine
  // builds need the live host rate/block size.
  proc.setPlayConfigDetails(2, 2, kFs, kBlock);
  proc.prepareToPlay(kFs, kBlock);
  seedStereoChains(proc, leftIds, rightIds);
  ASSERT_TRUE(waitForChainLoaded(proc)) << "tone blocks never finished loading from cache";
}

// Build and prepare the branched rig every shared-tail case shares: trunk
// [blk-a, blk-b, blk-c] tapped after blk-a and rejoined at blk-c, so the
// shared tail is the trunk's own [blk-c]; the branch lane is `rightIds`.
// `trunkPan` places the trunk's parallel block blk-b, `branchPan` places the
// branch lane's first block, and `tailPan` (<0 = unset) places the shared
// tail blk-c. Lane placements (blk-b, the branch block) are lane-local; blk-c
// is a tail placement. The rig is pumped so the RT routing state is live.
void buildBranchedRig(ChainTestProcessor& proc, const std::vector<juce::String>& rightIds,
                      float trunkPan, float branchPan, float tailPan) {
  seedPrepared(proc, {"blk-a", "blk-b", "blk-c"}, rightIds);
  letAudioGoIdle();
  ASSERT_TRUE(proc.setChainBranch("left", "blk-a"));
  ASSERT_TRUE(proc.setChainMerge("blk-c"));  // shared tail = trunk [blk-c]
  if (trunkPan >= 0.0f)
    ASSERT_TRUE(proc.setBlockParam("blk-b", "pan", trunkPan));  // trunk lane
  if (branchPan >= 0.0f && !rightIds.empty())
    ASSERT_TRUE(proc.setBlockParam(rightIds.front().toStdString(), "pan", branchPan));  // branch lane
  if (tailPan >= 0.0f)
    ASSERT_TRUE(proc.setBlockParam("blk-c", "pan", tailPan));  // shared tail
  pumpAudio(proc);
}

// 1. Two pan-set blocks in the SAME lane: the later (downstream) one wins —
//    a serial lane holds one placement.
// 2. Clearing the later one (pan = -1) makes the earlier one win again.
TEST(PanResolve, LastSetSlotInALaneWins) {
  ChainTestProcessor proc;
  seedPrepared(proc, {"blk-a", "blk-b"}, {"blk-r"});

  ASSERT_TRUE(proc.setBlockParam("blk-a", "pan", 0.20));
  ASSERT_TRUE(proc.setBlockParam("blk-b", "pan", 0.80));
  pumpAudio(proc);
  {
    const juce::var state = proc.getChainState(-1);
    ASSERT_TRUE(state.hasProperty("panLeft"));
    EXPECT_NEAR(lanePan(state, "panLeft"), 0.80, 1e-5)
        << "the downstream slot must place the lane";
  }

  // Clearing the later slot returns the lane to the earlier placement.
  ASSERT_TRUE(proc.setBlockParam("blk-b", "pan", -1.0));
  pumpAudio(proc);
  EXPECT_NEAR(lanePan(proc.getChainState(-1), "panLeft"), 0.20, 1e-5)
      << "clearing the later slot must fall back to the earlier one";
}

// 3. A lane with no pan set falls back to the rail value
//    (chainPanLeft / chainPanRight). The rail is moved off its defaults so
//    the fallback provably reads the rail, not a hardcoded centre.
TEST(PanResolve, UnsetLaneFallsBackToTheRail) {
  ChainTestProcessor proc;
  seedPrepared(proc, {"blk-a"}, {"blk-r"});

  proc.parameters.getParameter("chainPanLeft")->setValueNotifyingHost(0.30f);
  proc.parameters.getParameter("chainPanRight")->setValueNotifyingHost(0.70f);
  pumpAudio(proc);  // primes the cached rail values from the atomics

  {
    const juce::var state = proc.getChainState(-1);
    ASSERT_TRUE(state.hasProperty("panLeft"));
    ASSERT_TRUE(state.hasProperty("panRight"));
    EXPECT_NEAR(lanePan(state, "panLeft"), 0.30, 1e-5);
    EXPECT_NEAR(lanePan(state, "panRight"), 0.70, 1e-5);
  }

  // A slot that sets a pan overrides the rail…
  ASSERT_TRUE(proc.setBlockParam("blk-a", "pan", 0.90));
  pumpAudio(proc);
  EXPECT_NEAR(lanePan(proc.getChainState(-1), "panLeft"), 0.90, 1e-5);

  // …and clearing it (pan = -1) inherits the rail again.
  ASSERT_TRUE(proc.setBlockParam("blk-a", "pan", -1.0));
  pumpAudio(proc);
  EXPECT_NEAR(lanePan(proc.getChainState(-1), "panLeft"), 0.30, 1e-5)
      << "a cleared slot must inherit the rail placement";
}

// 4. Branched chain: a pan on the branch lane resolves INDEPENDENTLY of the
//    trunk lane — each lane keeps its own last-set block.
TEST(PanResolve, BranchLanePanIsIndependentOfTrunk) {
  ChainTestProcessor proc;
  seedPrepared(proc, {"blk-a", "blk-b"}, {"blk-r"});

  letAudioGoIdle();
  ASSERT_TRUE(proc.setChainBranch("left", "blk-a"));

  ASSERT_TRUE(proc.setBlockParam("blk-a", "pan", 0.40));  // trunk
  ASSERT_TRUE(proc.setBlockParam("blk-r", "pan", 0.80));  // branch lane
  pumpAudio(proc);

  const juce::var state = proc.getChainState(-1);
  EXPECT_NEAR(lanePan(state, "panLeft"), 0.40, 1e-5)
      << "the branch lane's pan must not move the trunk";
  EXPECT_NEAR(lanePan(state, "panRight"), 0.80, 1e-5)
      << "the branch lane resolves its own placement";
}

// 5. A pan-set block in the shared rejoin tail does NOT override the lanes'
//    own placements: the tail is excluded from resolveLanePan, so each lane
//    keeps its own last-placed block and the tail placement only composes
//    (see docs/pan-in-a-chain.md § shared tail).
TEST(PanResolve, SharedRejoinTailDoesNotOverrideLanePlacements) {
  ChainTestProcessor proc;
  seedPrepared(proc, {"blk-a", "blk-b", "blk-c"}, {"blk-r"});

  letAudioGoIdle();
  ASSERT_TRUE(proc.setChainBranch("left", "blk-a"));
  ASSERT_TRUE(proc.setChainMerge("blk-c"));  // shared tail = trunk [blk-c]

  ASSERT_TRUE(proc.setBlockParam("blk-a", "pan", 0.20));  // trunk lane-local
  ASSERT_TRUE(proc.setBlockParam("blk-r", "pan", 0.90));  // branch lane-local
  ASSERT_TRUE(proc.setBlockParam("blk-c", "pan", 0.40));  // shared tail
  pumpAudio(proc);

  const juce::var state = proc.getChainState(-1);
  EXPECT_NEAR(lanePan(state, "panLeft"), 0.20, 1e-5)
      << "the shared tail must not override the trunk lane's own placement";
  EXPECT_NEAR(lanePan(state, "panRight"), 0.90, 1e-5)
      << "the shared tail must not override the branch lane's own placement";

  // The tail block is the only one that advertises a both-lanes placement;
  // the lanes' own blocks do not.
  EXPECT_TRUE(placesBothLanes(state, "blk-c")) << "the shared tail places both lanes";
  EXPECT_FALSE(placesBothLanes(state, "blk-a"));
  EXPECT_FALSE(placesBothLanes(state, "blk-r"));
}

// 6. Composition at the DSP level: with trunk hard left (0.0) and branch hard
//    right (1.0) the two lane placements are the identity, so each output
//    channel is its own lane's merged chain. A centred tail placement (0.5)
//    then scales each still-independent channel by its constant-power gain
//    cos/sin(0.5) = 0.7071 (-3.01 dB at centre) — it does not collapse the two
//    chains onto one shared pan, which is the old override.
//
//    The branch lane carries TWO cab blocks (the trunk parallel section one),
//    so the two lanes process genuinely different chains; otherwise — with the
//    same IR kernel on both — the channels would be bit-identical and the
//    "keeps its own chain" check below would be vacuous.
TEST(PanResolve, SharedRejoinTailComposesWithLanePlacements) {
  const auto in = makeNoise(240 * kBlock, 9090, 0.25f);

  ChainTestProcessor ref;
  buildBranchedRig(ref, {"blk-r", "blk-r2"}, 0.0f, 1.0f, -1.0f);  // no tail placement
  const auto [refL, refR] = processStereo(ref, in, kBlock);

  ChainTestProcessor tail;
  buildBranchedRig(tail, {"blk-r", "blk-r2"}, 0.0f, 1.0f, 0.5f);  // tail centred
  const auto [tailL, tailR] = processStereo(tail, in, kBlock);

  constexpr float kCentre = 0.70710678f;  // cos(pi/4) = sin(pi/4)
  EXPECT_LT(maxAbsScaledDiff(tailL, refL, kCentre), 1e-4f)
      << "the trunk channel keeps its own chain, scaled by the tail gain";
  EXPECT_LT(maxAbsScaledDiff(tailR, refR, kCentre), 1e-4f)
      << "the branch channel keeps its own chain, scaled by the tail gain";
  // The two channels still carry different chains: the old "tail overrides both
  // lanes" rule set every matrix gain equal, collapsing them into one image.
  EXPECT_GT(settledMaxChannelDiff(tailL, tailR), 1e-3f)
      << "a centred tail placement must not collapse the two chains";
}

// 7. Regression: with no placements anywhere and the rails at 0.0 / 1.0 the
//    image matrix is the identity — each output channel is its own lane's
//    chain, with no cross-mix. Proven at the DSP level: swapping the branch
//    lane's content leaves the trunk channel bit-identical while the branch
//    channel changes. (The diagonal's unity gain is exercised by the existing
//    ChainBranchTest.BranchedDualCabMatchesMonoChainMode, which compares this
//    exact default to mono-chain mode.)
TEST(PanResolve, NoPlacementsAnywhereIsIdentity) {
  const auto in = makeNoise(240 * kBlock, 4711, 0.25f);

  ChainTestProcessor proc;
  buildBranchedRig(proc, {"blk-r"}, -1.0f, -1.0f, -1.0f);

  const juce::var state = proc.getChainState(-1);
  EXPECT_NEAR(lanePan(state, "panLeft"), 0.0, 1e-5) << "the left rail is the fallback";
  EXPECT_NEAR(lanePan(state, "panRight"), 1.0, 1e-5) << "the right rail is the fallback";
  for (const char* id : {"blk-a", "blk-b", "blk-c", "blk-r"})
    EXPECT_FALSE(placesBothLanes(state, id))
        << id << " must not advertise a both-lanes placement";

  const auto [lA, rA] = processStereo(proc, in, kBlock);

  // Vary the branch lane's chain (an empty pass-through instead of blk-r).
  ChainTestProcessor other;
  buildBranchedRig(other, {}, -1.0f, -1.0f, -1.0f);
  const auto [lB, rB] = processStereo(other, in, kBlock);

  EXPECT_LT(settledMaxChannelDiff(lA, lB), 1e-5f)
      << "the trunk channel must not depend on the branch lane (no cross-mix)";
  EXPECT_GT(settledMaxChannelDiff(rA, rB), 1e-3f)
      << "the branch channel must follow the branch lane's own chain";
}

}  // namespace
