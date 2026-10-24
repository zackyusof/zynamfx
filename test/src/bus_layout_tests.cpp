// Main-bus layout tests
//
// Host contract: zynamfx accepts mono/mono, stereo/stereo and — since the
// mono-in/stereo-out insert was added — mono/stereo. A guitar on a mono
// track can therefore reach the stereo image stage without a utility plugin
// in front. The contracts:
//
//   - setBusesLayout accepts {{mono},{stereo}} and rejects {{stereo},{mono}}
//     (a discarded input channel is worse than no option at all),
//   - on that bus the host fills channel 0 only (the JUCE AU wrapper hands
//     the processor a two-channel buffer with ch1 untouched), so the plugin
//     must mirror it: a mono take on a 1-in/2-out insert renders exactly
//     what the same take renders on a 2-in/2-out bus with L == R,
//   - that holds in both chain modes: mono chain (one chain, image matrix
//     identity) and stereo chains (two lanes, both fed by the mirrored take),
//   - neither output channel is silent — the pre-fix failure mode was a dead
//     right lane, not a level error.
//
// The rigs are configured through setPlayConfigDetails, like the other DSP
// suites: it sets the host sample rate/block size and the bus channel sets in
// one call, exactly what the AU/VST3 wrappers do before prepareToPlay.
#include "Processor.h"
#include "chain_test_helpers.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace {

constexpr int kBlock = 512;

const std::vector<float>& take() {
  static const auto v = makeNoise(240 * kBlock, 5005, 0.1f);
  return v;
}
const std::vector<float>& silence() {
  static const std::vector<float> v(static_cast<size_t>(240 * kBlock), 0.0f);
  return v;
}

juce::ValueTree makeMonoAmpCabState() {
  juce::ValueTree state("ChainSnapshot");
  juce::ValueTree lane("ChainBlocks");
  lane.appendChild(makeNamBlockTree("blk-amp", 1, 100), nullptr);
  auto cab = makeIrBlockTree("blk-cab", 2, 200, "cab-ir-test.wav");
  cab.setProperty("mix", 0.7f, nullptr);
  lane.appendChild(cab, nullptr);
  state.appendChild(lane, nullptr);
  return state;
}

// The same chain in both stereo lanes.
juce::ValueTree makeStereoAmpCabState() {
  juce::ValueTree state("ChainSnapshot");
  state.setProperty("stereoEnabled", true, nullptr);
  juce::ValueTree left("ChainBlocks");
  left.appendChild(makeNamBlockTree("blk-amp-l", 1, 100), nullptr);
  left.appendChild(makeIrBlockTree("blk-cab-l", 2, 200, "cab-ir-test.wav"), nullptr);
  juce::ValueTree right("ChainBlocks");
  right.appendChild(makeNamBlockTree("blk-amp-r", 3, 300), nullptr);
  right.appendChild(makeIrBlockTree("blk-cab-r", 4, 400, "cab-ir-test.wav"), nullptr);
  state.appendChild(left, nullptr);
  state.appendChild(right, nullptr);
  return state;
}

juce::AudioProcessor::BusesLayout layoutOf(int in, int out) {
  juce::AudioProcessor::BusesLayout layout;
  layout.inputBuses.add(in == 1 ? juce::AudioChannelSet::mono()
                                : juce::AudioChannelSet::stereo());
  layout.outputBuses.add(out == 1 ? juce::AudioChannelSet::mono()
                                  : juce::AudioChannelSet::stereo());
  return layout;
}

// A processor on the given main-bus layout, chain loaded and settled.
std::unique_ptr<ChainTestProcessor> makeRig(const juce::ValueTree& state, int in, int out) {
  auto proc = std::make_unique<ChainTestProcessor>();
  proc->setPlayConfigDetails(in, out, kFs, kBlock);
  EXPECT_EQ(proc->getTotalNumInputChannels(), in) << "input bus not set to " << in;
  EXPECT_EQ(proc->getTotalNumOutputChannels(), out) << "output bus not set to " << out;
  proc->prepareToPlay(kFs, kBlock);
  proc->restoreFromTree(state);
  EXPECT_TRUE(waitForChainLoaded(*proc)) << "blocks never finished loading from cache";
  proc->prepareToPlay(kFs, kBlock);
  return proc;
}

float settledPeak(const std::vector<float>& x, size_t skip = 48000) {
  float peak = 0.0f;
  for (size_t i = skip; i < x.size(); ++i)
    peak = std::max(peak, std::abs(x[i]));
  return peak;
}

float settledDiff(const std::vector<float>& a, const std::vector<float>& b) {
  return settledMaxChannelDiff(a, b, 48000);
}

}  // namespace

// ---------------------------------------------------------------------------
// Layout gate
// ---------------------------------------------------------------------------

TEST(BusLayoutTest, MonoToStereoLayoutAccepted) {
  TONE3000Processor proc;
  ASSERT_TRUE(proc.setBusesLayout(layoutOf(1, 2)));
  EXPECT_EQ(proc.getMainBusNumInputChannels(), 1);
  EXPECT_EQ(proc.getMainBusNumOutputChannels(), 2);
}

TEST(BusLayoutTest, EqualLayoutsAccepted) {
  TONE3000Processor mono;
  EXPECT_TRUE(mono.setBusesLayout(layoutOf(1, 1)));
  TONE3000Processor stereo;
  EXPECT_TRUE(stereo.setBusesLayout(layoutOf(2, 2)));
}

TEST(BusLayoutTest, StereoToMonoRejected) {
  TONE3000Processor proc;
  EXPECT_FALSE(proc.setBusesLayout(layoutOf(2, 1)))
      << "a stereo-in/mono-out bus must stay unavailable";
}

// ---------------------------------------------------------------------------
// Signal path
// ---------------------------------------------------------------------------

// Mono chain: the mirrored take must render exactly like the same take on a
// stereo bus (where L == R already folds to unity).
TEST(BusLayoutTest, MonoSourceReachesBothChannels) {
  const auto state = makeMonoAmpCabState();

  auto wide = makeRig(state, 1, 2);
  const auto [wl, wr] = processStereoLR(*wide, take(), silence());

  auto stereoBus = makeRig(state, 2, 2);
  const auto [sl, sr] = processStereoLR(*stereoBus, take(), take());

  EXPECT_GT(settledPeak(wl), 1e-4f) << "left channel silent on the 1-in/2-out bus";
  EXPECT_GT(settledPeak(wr), 1e-4f) << "right channel silent on the 1-in/2-out bus";
  EXPECT_EQ(settledDiff(wl, sl), 0.0f) << "left diverged from the stereo-bus run";
  EXPECT_EQ(settledDiff(wr, sr), 0.0f) << "right diverged from the stereo-bus run";
  EXPECT_EQ(settledDiff(wl, wr), 0.0f) << "mono chain should be identical on both sides";
}

// Stereo chains on a mono input: both lanes must see the take, otherwise the
// right lane runs on the host's untouched (silent) channel.
TEST(BusLayoutTest, MonoSourceReachesBothStereoLanes) {
  const auto state = makeStereoAmpCabState();

  auto wide = makeRig(state, 1, 2);
  const auto [wl, wr] = processStereoLR(*wide, take(), silence());

  auto stereoBus = makeRig(state, 2, 2);
  const auto [sl, sr] = processStereoLR(*stereoBus, take(), take());

  EXPECT_GT(settledPeak(wl), 1e-4f) << "left lane silent on the 1-in/2-out bus";
  EXPECT_GT(settledPeak(wr), 1e-4f) << "right lane silent on the 1-in/2-out bus";
  EXPECT_EQ(settledDiff(wl, sl), 0.0f) << "left lane diverged from the stereo-bus run";
  EXPECT_EQ(settledDiff(wr, sr), 0.0f) << "right lane diverged from the stereo-bus run";
}

// A mono/mono insert still sees the take once, unaffected by the new bus.
TEST(BusLayoutTest, MonoToMonoUnchanged) {
  auto proc = makeRig(makeMonoAmpCabState(), 1, 1);
  juce::AudioBuffer<float> buffer(1, kBlock);
  juce::MidiBuffer midi;
  float peak = 0.0f;
  const auto& in = take();
  for (int off = 0; off + kBlock <= static_cast<int>(in.size()); off += kBlock) {
    buffer.copyFrom(0, 0, in.data() + off, kBlock);
    proc->processBlock(buffer, midi);
    if (off > 48000)
      peak = std::max(peak, buffer.getMagnitude(0, 0, kBlock));
  }
  EXPECT_GT(peak, 1e-4f) << "mono insert should still pass audio";
}
