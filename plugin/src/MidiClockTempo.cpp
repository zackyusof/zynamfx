#include "MidiClockTempo.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double kMinBpm = 20.0;
constexpr double kMaxBpm = 999.0;
}
void MidiClockTempo::prepare(double sampleRate) noexcept {
  sampleRate_ = sampleRate > 0.0 ? sampleRate : 48000.0;
  reset();
}

void MidiClockTempo::reset() noexcept {
  sampleCounter_ = 0;
  lastClockSample_ = -1;
  lastMessageSample_ = -1;
  activityUntil_ = -1;
  lastAcceptedSample_ = -1;
  expectedTickSamples_ = tickSamplesForBpm(120.0f);
  bpm_ = 0.0f;
  stableUpdateCount_ = 0;
  suspectShape_ = 0;
  suspectRun_ = 0;
  stepDir_ = 0;
  stepRun_ = 0;
  pendingGap_ = false;
  prevInterval_ = 0;
  acquiring_ = false;
  state_ = LockState::noSignal;
  transport_ = Transport::stopped;
  stale_ = false;
  songPosition_ = 0;
  acceptedTicks_ = 0;
  historyCount_ = 0;
  historyHead_ = 0;
  publish(false);
}

std::int64_t MidiClockTempo::tickSamplesForBpm(float bpm) const noexcept {
  return std::max<std::int64_t>(
      1, static_cast<std::int64_t>(sampleRate_ * 60.0 /
                                   (std::max(1.0f, bpm) * kTicksPerQuarter)));
}

std::int64_t MidiClockTempo::msToSamples(std::int64_t ms) const noexcept {
  return std::max<std::int64_t>(1, static_cast<std::int64_t>(sampleRate_ * ms / 1000.0));
}

void MidiClockTempo::processBlock(const juce::MidiBuffer& midi, int numSamples) noexcept {
  const int blockSamples = std::max(0, numSamples);
  const auto blockStart = sampleCounter_;
  for (const auto metadata : midi) {
    const auto eventSample = blockStart + std::max(0, metadata.samplePosition);
    const auto message = metadata.getMessage();
    lastMessageSample_ = eventSample;
    activityUntil_ = eventSample + msToSamples(kActivityHoldMs);
    consume(message, eventSample);
  }

  sampleCounter_ += blockSamples;
  if (lastAcceptedSample_ >= 0) {
    // 4 tick periods, clamped: a stopped master is detected in ~120 ms at
    // 120 BPM instead of a flat 500 ms (the old expression was 30x a tick
    // period, so the clamp always won).
    const auto timeoutMs = std::clamp<std::int64_t>(
        static_cast<std::int64_t>(4.0 * 2500.0 / std::max(1.0f, bpm_)), kTimeoutMinMs,
        kTimeoutMaxMs);
    stale_ = sampleCounter_ - lastAcceptedSample_ > msToSamples(timeoutMs);
    if (stale_) state_ = LockState::noSignal;
  }
  publish(activityUntil_ >= sampleCounter_);
}

void MidiClockTempo::consume(const juce::MidiMessage& message, std::int64_t sample) noexcept {
  const auto* raw = message.getRawData();
  const auto size = message.getRawDataSize();
  if (raw == nullptr || size <= 0) return;
  const auto status = raw[0];
  switch (status) {
    case 0xf8: consumeClock(sample); break;
    case 0xfa:
      transport_ = Transport::running;
      songPosition_ = 0;
      break;
    case 0xfb: transport_ = Transport::running; break;
    case 0xfc: transport_ = Transport::stopped; break;
    case 0xf2:
      if (size >= 3 && transport_ == Transport::stopped)
        songPosition_ = static_cast<std::uint16_t>(raw[1] | (raw[2] << 7));
      break;
    case 0xff: clearEstimator(); transport_ = Transport::stopped; break;
    default: break;  // FE active sensing and all channel/system messages are harmless.
  }
}

void MidiClockTempo::clearEstimator() noexcept {
  lastClockSample_ = -1;
  lastAcceptedSample_ = -1;
  acceptedTicks_ = 0;
  historyCount_ = 0;
  historyHead_ = 0;
  bpm_ = 0.0f;
  stableUpdateCount_ = 0;
  suspectShape_ = 0;
  suspectRun_ = 0;
  stepDir_ = 0;
  stepRun_ = 0;
  pendingGap_ = false;
  prevInterval_ = 0;
  acquiring_ = false;
  expectedTickSamples_ = tickSamplesForBpm(120.0f);
  state_ = LockState::noSignal;
  stale_ = false;
}

void MidiClockTempo::reacquire(std::int64_t sample) noexcept {
  // Keep bpm_ (and therefore the published tempo and hosted-slot playhead)
  // until the new window has a measurement; only the measurement is dropped.
  lastClockSample_ = sample;
  lastAcceptedSample_ = sample;
  acceptedTicks_ = 0;
  historyCount_ = 0;
  historyHead_ = 0;
  stableUpdateCount_ = 0;
  suspectShape_ = 0;
  suspectRun_ = 0;
  stepDir_ = 0;
  stepRun_ = 0;
  pendingGap_ = false;
  prevInterval_ = 0;
  acquiring_ = true;
  state_ = LockState::clock;
  stale_ = false;
}

void MidiClockTempo::consumeClock(std::int64_t sample) noexcept {
  if (lastClockSample_ < 0) {
    lastClockSample_ = sample;
    state_ = LockState::clock;
    stale_ = false;
    return;
  }

  const auto interval = sample - lastClockSample_;
  lastClockSample_ = sample;
  if (interval < kMinTickSamples || interval > kMaxTickSamples) return;

  int repair = 0;
  if (bpm_ > 0.0f && !acquiring_) {
    const auto ratio =
        static_cast<double>(interval) / static_cast<double>(expectedTickSamples_);

    // Classify the interval against the current expectation. A shape that
    // repeats is a tempo step; an isolated one is a lost/duplicated tick.
    const auto shape = ratio > kLongRatio ? 1 : (ratio < kShortRatio ? -1 : 0);
    if (shape == 0) {
      suspectShape_ = 0;
      suspectRun_ = 0;
    } else if (shape == suspectShape_) {
      ++suspectRun_;
    } else {
      suspectShape_ = shape;
      suspectRun_ = 1;
    }

    if (suspectRun_ >= kSustainedIntervals) {
      // Real tempo change (e.g. 120 -> 60 or 120 -> 300). Re-anchor and
      // measure the new tempo from scratch; treating this as lost ticks would
      // latch the follower on the old BPM forever.
      reacquire(sample);
      return;
    }

    // A lost clock message looks exactly like "one interval of 2T"; edge
    // jitter looks the same for a single interval, so the decision is
    // deferred one tick. A real gap is followed by normal spacing (pair sum
    // ~= 3T, current interval ~= T); a jitter spike is followed by the
    // compensating short interval. Without this test, heavy DAW jitter
    // fabricates ticks and reads the tempo high.
    if (pendingGap_) {
      const auto pair = prevInterval_ + interval;
      const auto expected = static_cast<double>(expectedTickSamples_);
      if (pair >= 2.5 * expected && interval >= 0.75 * expected &&
          interval <= 1.3 * expected)
        ++repair;
      pendingGap_ = false;
    }
    pendingGap_ = ratio >= 1.6 && ratio <= 2.4;
    prevInterval_ = interval;
  }

  const auto index = static_cast<std::size_t>(historyHead_);
  const auto previous = static_cast<std::size_t>(
      (historyHead_ + kHistorySize - 1) % kHistorySize);
  // Attribute a repaired lost tick to the interval it belongs to (the gap
  // tick itself), so a window that starts at that tick still counts it.
  if (repair != 0 && historyCount_ > 0) tickTotals_[previous] += 1u;
  clockSamples_[index] = sample;
  tickTotals_[index] = (historyCount_ == 0 ? 0u : tickTotals_[previous]) + 1u;
  historyHead_ = (historyHead_ + 1) % kHistorySize;
  historyCount_ = std::min(historyCount_ + 1, kHistorySize);
  acceptedTicks_ += 1u;
  lastAcceptedSample_ = sample;
  stale_ = false;
  state_ = LockState::clock;

  if (historyCount_ >= 2) {
    const auto oldest = static_cast<std::size_t>(
        (historyHead_ + kHistorySize - historyCount_) % kHistorySize);
    const auto newest = static_cast<std::size_t>(
        (historyHead_ + kHistorySize - 1) % kHistorySize);
    const auto span = clockSamples_[newest] - clockSamples_[oldest];
    const auto ticks = tickTotals_[newest] - tickTotals_[oldest];
    if (span > 0 && ticks >= 24) {
      const auto measured = (60.0 / kTicksPerQuarter) * ticks * sampleRate_ / span;
      if (measured >= kMinBpm && measured <= kMaxBpm) {
        const auto previousBpm = bpm_;
        // The first measurement of an acquisition is taken whole. Starting
        // from zero with alpha 0.5 published half the real tempo and left the
        // expectation at 2x, which ordinary link jitter then locked in.
        // A large residual gets the fast coefficient only when it persists in
        // the same direction: a single jittery window must not yank the
        // published tempo around (it used to, at 300 BPM especially).
        const auto delta = measured - bpm_;
        const auto dir = delta > 1.0 ? 1 : (delta < -1.0 ? -1 : 0);
        if (dir == 0) {
          stepDir_ = 0;
          stepRun_ = 0;
        } else if (dir == stepDir_) {
          ++stepRun_;
        } else {
          stepDir_ = dir;
          stepRun_ = 1;
        }
        const auto alpha =
            (bpm_ <= 0.0f || acquiring_)
                ? 1.0f
                : (stepRun_ >= 2 ? 0.5f : 0.12f);
        bpm_ = static_cast<float>(bpm_ + alpha * (measured - bpm_));
        acquiring_ = false;
        expectedTickSamples_ = tickSamplesForBpm(bpm_);
        // Lock when the estimate has stopped moving: each smoothed update under
        // 0.2 BPM means the follower has converged. A real tempo change makes a
        // bigger step and drops the state back to "clock".
        if (acceptedTicks_ >= 24 && std::abs(bpm_ - previousBpm) < 0.2f)
          ++stableUpdateCount_;
        else
          stableUpdateCount_ = 0;
        state_ = stableUpdateCount_ >= 3 ? LockState::locked : LockState::clock;
      }
    }
  }
}

void MidiClockTempo::publish(bool activity) noexcept {
  const auto bpmCents = static_cast<std::uint64_t>(std::clamp(
      static_cast<int>(std::lround(std::max(0.0f, bpm_) * 100.0f)), 0, 99900));
  std::uint64_t value = bpmCents;
  value |= static_cast<std::uint64_t>(state_) << 32;
  value |= static_cast<std::uint64_t>(transport_) << 34;
  value |= static_cast<std::uint64_t>(stale_) << 35;
  value |= static_cast<std::uint64_t>(activity) << 36;
  value |= static_cast<std::uint64_t>(songPosition_ & 0x3fff) << 37;
  packed_.store(value, std::memory_order_release);
}

MidiClockTempo::Snapshot MidiClockTempo::snapshot() const noexcept {
  const auto value = packed_.load(std::memory_order_acquire);
  Snapshot result;
  result.bpm = static_cast<float>(value & 0xffffffffu) / 100.0f;
  result.state = static_cast<LockState>((value >> 32) & 0x3u);
  result.transport = static_cast<Transport>((value >> 34) & 0x1u);
  result.stale = ((value >> 35) & 0x1u) != 0;
  result.activity = ((value >> 36) & 0x1u) != 0;
  result.songPosition = static_cast<std::uint16_t>((value >> 37) & 0x3fffu);
  return result;
}
