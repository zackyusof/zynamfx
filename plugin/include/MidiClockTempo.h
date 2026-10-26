#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>
#include <cstdint>

// Allocation-free MIDI clock follower. All times are absolute host samples;
// MidiMessage timestamps are deliberately ignored because their units vary by
// source and MidiBuffer metadata is the sample-accurate contract.
class MidiClockTempo {
public:
  enum class LockState : std::uint8_t { noSignal = 0, clock = 1, locked = 2 };
  enum class Transport : std::uint8_t { stopped = 0, running = 1 };

  struct Snapshot {
    float bpm = 0.0f;
    LockState state = LockState::noSignal;
    Transport transport = Transport::stopped;
    bool stale = false;
    bool activity = false;
    std::uint16_t songPosition = 0;
  };

  MidiClockTempo() = default;

  void prepare(double sampleRate) noexcept;
  void reset() noexcept;

  // Called once per processBlock, before any consumer that may discard MIDI.
  void processBlock(const juce::MidiBuffer& midi, int numSamples) noexcept;

  Snapshot snapshot() const noexcept;

private:
  static constexpr int kHistorySize = 49;  // 48 intervals
  static constexpr std::int64_t kMinTickSamples = 12;       // 0.25 ms @ 48 kHz
  static constexpr std::int64_t kMaxTickSamples = 96000;    // 2 s @ 48 kHz
  static constexpr std::int64_t kActivityHoldMs = 125;
  static constexpr std::int64_t kTimeoutMinMs = 120;
  static constexpr std::int64_t kTimeoutMaxMs = 500;
  // Interval shape gates. A single interval outside [kShortRatio, kLongRatio]
  // is a dropped/duplicated tick or link jitter and is compensated; the same
  // shape repeated kSustainedIntervals times is a real tempo step and the
  // estimator re-anchors instead (otherwise "half tempo" is read as "every
  // other tick is missing" and the follower latches on the old tempo).
  static constexpr double kLongRatio = 1.5;
  static constexpr double kShortRatio = 0.6;
  static constexpr int kSustainedIntervals = 3;
  static constexpr int kTicksPerQuarter = 24;

  void consume(const juce::MidiMessage& message, std::int64_t sample) noexcept;
  void consumeClock(std::int64_t sample) noexcept;
  void publish(bool activity) noexcept;
  void clearEstimator() noexcept;
  // Drop the measured window but keep the last published BPM: a tempo step
  // must not flash NO SIGNAL through the hosted-slot playhead while the new
  // window fills.
  void reacquire(std::int64_t sample) noexcept;
  std::int64_t tickSamplesForBpm(float bpm) const noexcept;
  std::int64_t msToSamples(std::int64_t ms) const noexcept;

  double sampleRate_ = 48000.0;
  std::int64_t sampleCounter_ = 0;
  std::int64_t lastClockSample_ = -1;
  std::int64_t lastMessageSample_ = -1;
  std::int64_t activityUntil_ = -1;
  std::int64_t lastAcceptedSample_ = -1;
  std::int64_t expectedTickSamples_ = 1000;
  float bpm_ = 0.0f;
  // Lock = the smoothed estimate has stopped moving. Comparing the lagging
  // smoothed bpm against the instantaneous window measurement never stabilised
  // under real link jitter (IAC measured +/-0.6 BPM between windows), so the UI
  // stayed on "CLOCK" forever; the size of each update is the honest signal.
  int stableUpdateCount_ = 0;
  // Interval-shape memory: +1 long, -1 short, 0 in range.
  int suspectShape_ = 0;
  int suspectRun_ = 0;
  // Sustained-residual memory for the fast smoothing coefficient.
  int stepDir_ = 0;
  int stepRun_ = 0;
  // Deferred lost-tick decision (see consumeClock).
  bool pendingGap_ = false;
  std::int64_t prevInterval_ = 0;
  // True while a tempo step is being re-measured: the ratio gates are bypassed
  // so a stale expectation cannot reject every tick of the new tempo.
  bool acquiring_ = false;
  LockState state_ = LockState::noSignal;
  Transport transport_ = Transport::stopped;
  bool stale_ = false;
  std::uint16_t songPosition_ = 0;
  std::uint32_t acceptedTicks_ = 0;

  std::array<std::int64_t, kHistorySize> clockSamples_{};
  std::array<std::uint32_t, kHistorySize> tickTotals_{};
  int historyCount_ = 0;
  int historyHead_ = 0;

  // [bpm*100: 32][state:2][transport:1][stale:1][activity:1][SPP:14]
  std::atomic<std::uint64_t> packed_{0};
};
