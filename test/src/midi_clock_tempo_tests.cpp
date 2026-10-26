#include "MidiClockTempo.h"
#include "Processor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

void send(MidiClockTempo& clock, const juce::MidiMessage& message, int samples = 0) {
  juce::MidiBuffer midi;
  midi.addEvent(message, 0);
  clock.processBlock(midi, samples);
}

MidiClockTempo::Snapshot follow(double bpm, int ticks = 120) {
  MidiClockTempo clock;
  clock.prepare(48000.0);
  const auto interval = static_cast<int>(std::lround(48000.0 * 60.0 / (bpm * 24.0)));
  send(clock, juce::MidiMessage::midiStart());
  for (int i = 0; i < ticks; ++i) send(clock, juce::MidiMessage::midiClock(), interval);
  return clock.snapshot();
}

TEST(MidiClockTempoTest, LocksAtCommonTemposUsingSampleSpans) {
  for (const double bpm : {60.0, 120.0, 200.0}) {
    const auto state = follow(bpm);
    EXPECT_EQ(state.state, MidiClockTempo::LockState::locked);
    EXPECT_NEAR(state.bpm, bpm, 0.1);
  }
}

TEST(MidiClockTempoTest, OneDroppedTickDoesNotHalveTheTempo) {
  MidiClockTempo clock;
  clock.prepare(48000.0);
  const int interval = 1000;  // 120 BPM at the test's deliberately compact scale.
  send(clock, juce::MidiMessage::midiClock());
  for (int i = 0; i < 80; ++i)
    send(clock, juce::MidiMessage::midiClock(), interval * (i == 30 ? 2 : 1));
  EXPECT_NEAR(clock.snapshot().bpm, 120.0, 0.1);
}

TEST(MidiClockTempoTest, SampleSpanEstimatorRejectsSmallClockJitter) {
  MidiClockTempo clock;
  clock.prepare(48000.0);
  send(clock, juce::MidiMessage::midiClock());
  for (int i = 0; i < 48; ++i)
    send(clock, juce::MidiMessage::midiClock(), 1000);
  for (int i = 0; i < 72; ++i)
    send(clock, juce::MidiMessage::midiClock(), 1000 + ((i % 5) - 2) * 3);
  EXPECT_EQ(clock.snapshot().state, MidiClockTempo::LockState::locked);
  EXPECT_NEAR(clock.snapshot().bpm, 120.0, 0.2);
}

TEST(MidiClockTempoTest, StaleClockFreezesTempoAndResumesThroughClockState) {
  MidiClockTempo clock;
  clock.prepare(48000.0);
  const int interval = 1000;
  send(clock, juce::MidiMessage::midiClock());
  for (int i = 0; i < 80; ++i) send(clock, juce::MidiMessage::midiClock(), interval);
  const auto before = clock.snapshot();
  EXPECT_EQ(before.state, MidiClockTempo::LockState::locked);
  for (int i = 0; i < 60; ++i) clock.processBlock(juce::MidiBuffer(), 480);
  const auto stale = clock.snapshot();
  EXPECT_TRUE(stale.stale);
  EXPECT_EQ(stale.state, MidiClockTempo::LockState::noSignal);
  EXPECT_NEAR(stale.bpm, before.bpm, 0.01);
  send(clock, juce::MidiMessage::midiClock(), 1000);
  EXPECT_EQ(clock.snapshot().state, MidiClockTempo::LockState::clock);
}

TEST(MidiClockTempoTest, TransportAndSongPositionSemanticsAreExplicit) {
  MidiClockTempo clock;
  clock.prepare(48000.0);
  send(clock, juce::MidiMessage::songPositionPointer(42));
  EXPECT_EQ(clock.snapshot().songPosition, 42);
  send(clock, juce::MidiMessage::midiStart());
  EXPECT_EQ(clock.snapshot().transport, MidiClockTempo::Transport::running);
  send(clock, juce::MidiMessage::songPositionPointer(7));
  EXPECT_EQ(clock.snapshot().songPosition, 0);  // running ignores SPP
  send(clock, juce::MidiMessage::midiStop());
  EXPECT_EQ(clock.snapshot().transport, MidiClockTempo::Transport::stopped);
  send(clock, juce::MidiMessage::songPositionPointer(7));
  EXPECT_EQ(clock.snapshot().songPosition, 7);
  send(clock, juce::MidiMessage(static_cast<juce::uint8>(0xff)));
  EXPECT_EQ(clock.snapshot().state, MidiClockTempo::LockState::noSignal);
}

TEST(MidiClockTempoTest, AnyMidiMessageHoldsActivityWithoutAClock) {
  MidiClockTempo clock;
  clock.prepare(48000.0);
  send(clock, juce::MidiMessage::controllerEvent(1, 7, 100));
  EXPECT_TRUE(clock.snapshot().activity);
  for (int i = 0; i < 15; ++i) clock.processBlock(juce::MidiBuffer(), 512);
  EXPECT_FALSE(clock.snapshot().activity);
}

// Deterministic edge jitter, uniform in [-ms, +ms]. Absolute tick positions
// move, so one interval gains and the next loses (the model in
// docs/midi-clock-tempo-howto.md). Own LCG so a failing run is reproducible.
struct JitterFeed {
  std::uint32_t seed = 1;
  std::int64_t edge = 0;
  std::int64_t elapsed = 0;
  void run(MidiClockTempo& clock, double bpm, int ticks, double jitterMs = 0.0) {
    const auto period = 48000.0 * 60.0 / (bpm * 24.0);
    for (int i = 0; i < ticks; ++i) {
      seed = seed * 1664525u + 1013904223u;
      const auto unit = (static_cast<double>(seed >> 8) / 16777215.0) * 2.0 - 1.0;
      elapsed += static_cast<std::int64_t>(std::lround(period));
      const auto next =
          elapsed + static_cast<std::int64_t>(std::lround(unit * jitterMs * 48.0));
      const auto block = static_cast<int>(std::max<std::int64_t>(1, next - edge));
      edge = next;
      send(clock, juce::MidiMessage::midiClock(), block);
    }
  }
};

TEST(MidiClockTempoTest, AcquisitionNeverHalvesTheTempoUnderLinkJitter) {
  // A first estimate taken with alpha=0.5 from zero published 60.0 at a 120.0
  // source; with >=0.1 ms of edge jitter the half-tempo expectation then
  // rejected every slightly-short tick and the follower latched there.
  for (std::uint32_t seed : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}) {
    MidiClockTempo clock;
    clock.prepare(48000.0);
    JitterFeed feed{seed};
    feed.run(clock, 120.0, 600, 1.0);
    const auto state = clock.snapshot();
    EXPECT_NEAR(state.bpm, 120.0, 0.3) << "seed " << seed;
    EXPECT_EQ(state.state, MidiClockTempo::LockState::locked) << "seed " << seed;
  }
}

TEST(MidiClockTempoTest, TracksTempoHalvingInsteadOfReadingDroppedTicks) {
  // 120 -> 60 doubled the interval. The old ratio gate read that as "every
  // other tick is missing", compensated, and stayed on 120 forever.
  MidiClockTempo clock;
  clock.prepare(48000.0);
  JitterFeed feed{42u};
  feed.run(clock, 120.0, 200, 0.5);
  ASSERT_NEAR(clock.snapshot().bpm, 120.0, 0.3);
  feed.run(clock, 60.0, 200, 0.5);
  const auto state = clock.snapshot();
  EXPECT_NEAR(state.bpm, 60.0, 0.5);
  EXPECT_GT(state.bpm, 1.0f) << "tempo must never flash to zero during a step";
}

TEST(MidiClockTempoTest, ReacquiresAfterLargeTempoIncrease) {
  // 120 -> 300 (ratio 0.4) used to re-enter the reset branch on every tick:
  // the window was cleared but the stale expectation was kept, so it never
  // measured again.
  for (const double target : {240.0, 300.0}) {
    MidiClockTempo clock;
    clock.prepare(48000.0);
    JitterFeed feed{7u};
    feed.run(clock, 120.0, 200, 0.5);
    ASSERT_NEAR(clock.snapshot().bpm, 120.0, 0.3);
    feed.run(clock, target, 300, 0.5);
    EXPECT_NEAR(clock.snapshot().bpm, target, 0.5) << "target " << target;
  }
}

TEST(MidiClockTempoTest, StaleTimeoutScalesWithTempoInsteadOfAFixed500ms) {
  // The timeout is four tick periods clamped to 120..500 ms: ~120 ms at
  // 120 BPM (the old expression was 30x a tick period, so the clamp always
  // won and every tempo waited the full 500 ms).
  MidiClockTempo fast;
  fast.prepare(48000.0);
  JitterFeed feed{11u};
  feed.run(fast, 120.0, 200, 0.0);
  ASSERT_EQ(fast.snapshot().state, MidiClockTempo::LockState::locked);
  fast.processBlock(juce::MidiBuffer(), 4000);  // ~83 ms since the last tick
  EXPECT_FALSE(fast.snapshot().stale);
  fast.processBlock(juce::MidiBuffer(), 3000);  // ~146 ms total > 120 ms
  EXPECT_TRUE(fast.snapshot().stale);

  MidiClockTempo slow;
  slow.prepare(48000.0);
  JitterFeed slowFeed{12u};
  slowFeed.run(slow, 20.0, 200, 0.0);
  slow.processBlock(juce::MidiBuffer(), 14400);  // 300 ms < 500 ms at 20 BPM
  EXPECT_FALSE(slow.snapshot().stale) << "a 20 BPM master must not be declared gone after 300 ms";
}

// A hosted EFFECT slot reads tempo from AudioPlayHead, not from MIDI clock
// bytes. These cover the two contracts the slot depends on: a standalone
// clock-only transport gets a complete position, and an outer DAW playhead is
// passed through with only the BPM replaced.
class StubPlayHead final : public juce::AudioPlayHead {
public:
  explicit StubPlayHead(double bpm, double ppq) {
    juce::AudioPlayHead::PositionInfo info;
    info.setBpm(bpm);
    info.setPpqPosition(ppq);
    info.setTimeInSamples(static_cast<juce::int64>(ppq * 24000.0));
    info.setTimeInSeconds(ppq * 0.5);
    info.setTimeSignature(juce::AudioPlayHead::TimeSignature{3, 4});
    info.setIsPlaying(true);
    position_ = info;
  }

  juce::Optional<PositionInfo> getPosition() const override { return position_; }

private:
  juce::AudioPlayHead::PositionInfo position_;
};

TEST(HostedClockPlayHeadTest, SynthesisesFullContractFromExternalClock) {
  HostedClockPlayHead head;
  head.update(nullptr, 95.0f, true, 48000.0, 512);

  const auto position = head.getPosition();
  ASSERT_TRUE(position.hasValue());
  ASSERT_TRUE(position->getBpm().hasValue());
  EXPECT_NEAR(*position->getBpm(), 95.0, 0.001);
  EXPECT_TRUE(position->getPpqPosition().hasValue());
  EXPECT_TRUE(position->getTimeInSamples().hasValue());
  EXPECT_TRUE(position->getTimeInSeconds().hasValue());
  ASSERT_TRUE(position->getTimeSignature().hasValue());
  EXPECT_EQ(position->getTimeSignature()->numerator, 4);
  EXPECT_TRUE(position->getIsPlaying());
}

TEST(HostedClockPlayHeadTest, PublishesNothingWithoutAClockOrHost) {
  HostedClockPlayHead head;
  head.update(nullptr, 0.0f, false, 48000.0, 512);
  EXPECT_FALSE(head.getPosition().hasValue());
}

TEST(HostedClockPlayHeadTest, PassesOuterHostPositionThroughAndOverridesBpm) {
  StubPlayHead host{120.0, 8.0};
  HostedClockPlayHead head;
  head.update(&host, 95.0f, true, 48000.0, 512);

  const auto position = head.getPosition();
  ASSERT_TRUE(position.hasValue());
  EXPECT_NEAR(*position->getBpm(), 95.0, 0.001);
  ASSERT_TRUE(position->getPpqPosition().hasValue());
  EXPECT_NEAR(*position->getPpqPosition(), 8.0, 0.001);
  ASSERT_TRUE(position->getTimeSignature().hasValue());
  EXPECT_EQ(position->getTimeSignature()->numerator, 3);
}

// The standalone's AudioProcessorPlayer publishes a position containing only
// hostTime / timeInSamples / timeInSeconds (see juce_AudioProcessorPlayer.cpp:
// its PlayHead sets nothing else). That is a source with no transport, and it
// used to leave isPlaying=false and PPQ frozen, so a hosted tempo-synced
// delay never followed the clock.
class TimeOnlyPlayHead final : public juce::AudioPlayHead {
public:
  juce::Optional<PositionInfo> getPosition() const override {
    PositionInfo info;
    info.setTimeInSamples(48000);
    info.setTimeInSeconds(1.0);
    return info;
  }
};

TEST(HostedClockPlayHeadTest, SynthesisesTransportFromATimeOnlyStandaloneSource) {
  TimeOnlyPlayHead host;
  HostedClockPlayHead head;
  head.update(&host, 95.0f, true, 48000.0, 512);

  const auto first = head.getPosition();
  ASSERT_TRUE(first.hasValue());
  EXPECT_NEAR(*first->getBpm(), 95.0, 0.001);
  EXPECT_TRUE(first->getIsPlaying()) << "hosted sync gates on isPlaying";
  ASSERT_TRUE(first->getPpqPosition().hasValue());
  ASSERT_TRUE(first->getTimeSignature().hasValue());
  const auto ppqAfterOneBlock = *first->getPpqPosition();
  EXPECT_GT(ppqAfterOneBlock, 0.0);

  head.update(&host, 95.0f, true, 48000.0, 512);
  const auto second = head.getPosition();
  ASSERT_TRUE(second.hasValue());
  ASSERT_TRUE(second->getPpqPosition().hasValue());
  EXPECT_GT(*second->getPpqPosition(), ppqAfterOneBlock)
      << "PPQ must advance while the external clock runs";
}

}  // namespace
