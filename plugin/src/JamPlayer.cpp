#include "JamPlayer.h"

namespace t3k {

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kClickDurationSeconds = 0.015f;  // 15ms click burst
constexpr float kDownbeatFreq = 1600.0f;        // Beat 1 pitch
constexpr float kNormalBeatFreq = 1000.0f;       // Other beats pitch
}  // namespace

JamPlayer::JamPlayer() {
  formatManager_.registerBasicFormats();
}

JamPlayer::~JamPlayer() = default;

void JamPlayer::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {
  sampleRate_ = sampleRate > 8000.0 ? sampleRate : 44100.0;
  updateMetronomeRate();
}

void JamPlayer::releaseResources() {}

void JamPlayer::setMetronomeEnabled(bool enabled) {
  metronomeEnabled_.store(enabled);
  if (enabled) {
    // Reset beat alignment on enable
    sampleCounter_ = samplesPerBeat_;  // trigger beat 1 on first callback
    currentBeatInternal_ = 0;
    clickSamplesRemaining_ = 0;
  }
}

void JamPlayer::setMetronomeBpm(float bpm) {
  metronomeBpm_.store(juce::jlimit(40.0f, 300.0f, bpm));
  updateMetronomeRate();
}

void JamPlayer::updateMetronomeRate() {
  const float bpm = metronomeBpm_.load();
  const double secondsPerBeat = 60.0 / static_cast<double>(bpm);
  samplesPerBeat_ = std::max(1, static_cast<int>(secondsPerBeat * sampleRate_));
  clickTotalSamples_ = std::max(1, static_cast<int>(kClickDurationSeconds * sampleRate_));
}

bool JamPlayer::loadTrackFile(const juce::File& file) {
  if (!file.existsAsFile())
    return false;

  std::unique_ptr<juce::AudioFormatReader> reader(formatManager_.createReaderFor(file));
  if (reader == nullptr)
    return false;

  const auto lengthInSamples = reader->lengthInSamples;
  if (lengthInSamples <= 0 || lengthInSamples > static_cast<juce::int64>(sampleRate_ * 3600 * 2))
    return false;  // Disallow files longer than 2 hours

  const int numChannels = std::min(2, static_cast<int>(reader->numChannels));

  juce::AudioBuffer<float> tempBuffer(numChannels, static_cast<int>(lengthInSamples));
  reader->read(&tempBuffer, 0, static_cast<int>(lengthInSamples), 0, true, true);

  {
    juce::ScopedLock lock(trackLock_);
    trackBuffer_ = std::move(tempBuffer);
    trackSampleRate_ = reader->sampleRate;
    trackName_ = file.getFileName();
    playbackPositionSamples_.store(0.0);
    isLoaded_.store(true);
    isPlaying_.store(false);
  }

  return true;
}

void JamPlayer::play() {
  if (isLoaded_.load()) {
    isPlaying_.store(true);
  }
}

void JamPlayer::stop() {
  isPlaying_.store(false);
  playbackPositionSamples_.store(0.0);
}

juce::String JamPlayer::getTrackName() const {
  juce::ScopedLock lock(const_cast<juce::CriticalSection&>(trackLock_));
  return trackName_;
}

double JamPlayer::getTrackLengthSeconds() const {
  juce::ScopedLock lock(const_cast<juce::CriticalSection&>(trackLock_));
  if (trackBuffer_.getNumSamples() <= 0 || trackSampleRate_ <= 0.0)
    return 0.0;
  return static_cast<double>(trackBuffer_.getNumSamples()) / trackSampleRate_;
}

double JamPlayer::getPlaybackPositionSeconds() const {
  if (!isLoaded_.load() || trackSampleRate_ <= 0.0)
    return 0.0;
  return playbackPositionSamples_.load() / trackSampleRate_;
}

void JamPlayer::setPlaybackPositionSeconds(double seconds) {
  if (!isLoaded_.load() || trackSampleRate_ <= 0.0)
    return;
  const double maxSec = getTrackLengthSeconds();
  const double targetSec = juce::jlimit(0.0, maxSec, seconds);
  playbackPositionSamples_.store(targetSec * trackSampleRate_);
}

void JamPlayer::process(juce::AudioBuffer<float>& buffer) {
  const int numSamples = buffer.getNumSamples();
  if (numSamples <= 0)
    return;

  // 1. Render Backing Track if playing
  if (isPlaying_.load() && isLoaded_.load()) {
    renderTrack(buffer, numSamples);
  }

  // 2. Render Metronome Click if enabled
  if (metronomeEnabled_.load()) {
    renderMetronome(buffer, numSamples);
  }
}

void JamPlayer::renderMetronome(juce::AudioBuffer<float>& buffer, int numSamples) {
  const float gain = metronomeGain_.load();
  if (gain <= 0.0001f)
    return;

  const int numChannels = buffer.getNumChannels();
  auto* left = buffer.getWritePointer(0);
  auto* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;
  const int beats = beatsPerBar_.load();

  for (int s = 0; s < numSamples; ++s) {
    if (sampleCounter_ >= samplesPerBeat_) {
      sampleCounter_ = 0;
      currentBeatInternal_ = (currentBeatInternal_ % beats) + 1;
      currentBeatVisual_.store(currentBeatInternal_);

      clickFreq_ = (currentBeatInternal_ == 1) ? kDownbeatFreq : kNormalBeatFreq;
      clickPhaseDelta_ = kTwoPi * clickFreq_ / static_cast<float>(sampleRate_);
      clickPhase_ = 0.0f;
      clickSamplesRemaining_ = clickTotalSamples_;
    }
    sampleCounter_++;

    if (clickSamplesRemaining_ > 0) {
      // Linear decaying envelope
      const float env = static_cast<float>(clickSamplesRemaining_) / static_cast<float>(clickTotalSamples_);
      const float clickSample = std::sin(clickPhase_) * env * gain * 0.7f;
      clickPhase_ += clickPhaseDelta_;
      if (clickPhase_ >= kTwoPi)
        clickPhase_ -= kTwoPi;
      clickSamplesRemaining_--;

      left[s] += clickSample;
      if (right != nullptr)
        right[s] += clickSample;
    }
  }
}

void JamPlayer::renderTrack(juce::AudioBuffer<float>& buffer, int numSamples) {
  juce::ScopedLock lock(trackLock_);
  const int totalTrackSamples = trackBuffer_.getNumSamples();
  if (totalTrackSamples <= 0)
    return;

  const float gain = trackGain_.load();
  const int numOutChannels = buffer.getNumChannels();
  const int numTrackChannels = trackBuffer_.getNumChannels();
  const double speedRatio = trackSampleRate_ / sampleRate_;
  const bool looping = isLooping_.load();

  auto* outL = buffer.getWritePointer(0);
  auto* outR = numOutChannels > 1 ? buffer.getWritePointer(1) : nullptr;
  const auto* trackL = trackBuffer_.getReadPointer(0);
  const auto* trackR = numTrackChannels > 1 ? trackBuffer_.getReadPointer(1) : trackL;

  double pos = playbackPositionSamples_.load();

  for (int s = 0; s < numSamples; ++s) {
    if (pos >= static_cast<double>(totalTrackSamples - 1)) {
      if (looping) {
        pos = 0.0;
      } else {
        isPlaying_.store(false);
        playbackPositionSamples_.store(0.0);
        break;
      }
    }

    const int idx0 = static_cast<int>(pos);
    const int idx1 = std::min(idx0 + 1, totalTrackSamples - 1);
    const float frac = static_cast<float>(pos - idx0);

    // Linear interpolation
    const float sampleL = (trackL[idx0] + frac * (trackL[idx1] - trackL[idx0])) * gain;
    const float sampleR = (trackR[idx0] + frac * (trackR[idx1] - trackR[idx0])) * gain;

    outL[s] += sampleL;
    if (outR != nullptr)
      outR[s] += sampleR;

    pos += speedRatio;
  }

  playbackPositionSamples_.store(pos);
}

}  // namespace t3k
