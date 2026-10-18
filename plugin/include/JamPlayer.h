#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

#include <atomic>
#include <cmath>
#include <memory>
#include <string>

namespace t3k {

class JamPlayer {
public:
  JamPlayer();
  ~JamPlayer();

  void prepareToPlay(double sampleRate, int samplesPerBlock);
  void releaseResources();
  void process(juce::AudioBuffer<float>& buffer);

  // --- Metronome Controls ---
  void setMetronomeEnabled(bool enabled);
  bool isMetronomeEnabled() const { return metronomeEnabled_.load(); }
  void setMetronomeBpm(float bpm);
  float getMetronomeBpm() const { return metronomeBpm_.load(); }
  void setMetronomeGain(float gain) { metronomeGain_.store(juce::jlimit(0.0f, 1.0f, gain)); }
  float getMetronomeGain() const { return metronomeGain_.load(); }
  void setBeatsPerBar(int beats) { beatsPerBar_.store(juce::jlimit(1, 16, beats)); }
  int getBeatsPerBar() const { return beatsPerBar_.load(); }
  int getCurrentBeat() const { return currentBeatVisual_.load(); }

  // --- Track Player Controls ---
  bool loadTrackFile(const juce::File& file);
  void play();
  void pause() { isPlaying_.store(false); }
  void stop();
  bool isPlaying() const { return isPlaying_.load(); }
  bool isTrackLoaded() const { return isLoaded_.load(); }
  void setLooping(bool loop) { isLooping_.store(loop); }
  bool isLooping() const { return isLooping_.load(); }
  void setTrackGain(float gain) { trackGain_.store(juce::jlimit(0.0f, 1.5f, gain)); }
  float getTrackGain() const { return trackGain_.load(); }

  juce::String getTrackName() const;
  double getTrackLengthSeconds() const;
  double getPlaybackPositionSeconds() const;
  void setPlaybackPositionSeconds(double seconds);

private:
  juce::AudioFormatManager formatManager_;
  double sampleRate_ = 44100.0;

  // Metronome DSP state
  std::atomic<bool> metronomeEnabled_{false};
  std::atomic<float> metronomeBpm_{120.0f};
  std::atomic<float> metronomeGain_{0.75f};
  std::atomic<int> beatsPerBar_{4};
  std::atomic<int> currentBeatVisual_{0};

  int samplesPerBeat_ = 22050;
  int sampleCounter_ = 0;
  int currentBeatInternal_ = 0;
  int clickSamplesRemaining_ = 0;
  int clickTotalSamples_ = 0;
  float clickFreq_ = 1000.0f;
  float clickPhase_ = 0.0f;
  float clickPhaseDelta_ = 0.0f;

  void updateMetronomeRate();
  void renderMetronome(juce::AudioBuffer<float>& buffer, int numSamples);

  // Track Player state
  juce::CriticalSection trackLock_;
  juce::AudioBuffer<float> trackBuffer_;
  double trackSampleRate_ = 44100.0;
  juce::String trackName_;
  std::atomic<bool> isLoaded_{false};
  std::atomic<bool> isPlaying_{false};
  std::atomic<bool> isLooping_{true};
  std::atomic<float> trackGain_{0.8f};
  std::atomic<double> playbackPositionSamples_{0.0};

  void renderTrack(juce::AudioBuffer<float>& buffer, int numSamples);
};

}  // namespace t3k
