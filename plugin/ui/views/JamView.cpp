#include "JamView.h"

#include "core/CustomIcons.h"
#include "core/Design.h"
#include "core/Fonts.h"
#include "core/Help.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

JamView::JamView(Services& services) : services_(services) {
  // Close Button
  close_.onClick = [this] {
    if (onClose) onClose();
  };
  addAndMakeVisible(close_);

  // --- Metronome Controls ---
  bpmSlider_.setRange(40.0, 280.0, 1.0);
  bpmSlider_.setValue(services_.backend.getMetronomeBpm(), juce::dontSendNotification);
  bpmSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
  bpmSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 55, 24);
  bpmSlider_.setColour(juce::Slider::thumbColourId, theme::kAccent);
  bpmSlider_.setColour(juce::Slider::trackColourId, theme::kSurfaceRaised);
  bpmSlider_.setColour(juce::Slider::backgroundColourId, theme::kBlack);
  bpmSlider_.setColour(juce::Slider::textBoxTextColourId, theme::kWhite);
  bpmSlider_.setColour(juce::Slider::textBoxBackgroundColourId, theme::kSurface);
  bpmSlider_.setColour(juce::Slider::textBoxOutlineColourId, theme::kBorder);
  bpmSlider_.onValueChange = [this] {
    services_.backend.setMetronomeBpm(static_cast<float>(bpmSlider_.getValue()));
  };
  addAndMakeVisible(bpmSlider_);

  tapButton_.onClick = [this] { handleTapTempo(); };
  addAndMakeVisible(tapButton_);

  minusOneBtn_.onClick = [this] {
    bpmSlider_.setValue(bpmSlider_.getValue() - 1.0);
  };
  plusOneBtn_.onClick = [this] {
    bpmSlider_.setValue(bpmSlider_.getValue() + 1.0);
  };
  minusFiveBtn_.onClick = [this] {
    bpmSlider_.setValue(bpmSlider_.getValue() - 5.0);
  };
  plusFiveBtn_.onClick = [this] {
    bpmSlider_.setValue(bpmSlider_.getValue() + 5.0);
  };
  addAndMakeVisible(minusOneBtn_);
  addAndMakeVisible(plusOneBtn_);
  addAndMakeVisible(minusFiveBtn_);
  addAndMakeVisible(plusFiveBtn_);

  // Time Signature buttons
  timeSig2_.onClick = [this] {
    services_.backend.setMetronomeBeatsPerBar(2);
    updateMetroTimeSigButtons();
  };
  timeSig3_.onClick = [this] {
    services_.backend.setMetronomeBeatsPerBar(3);
    updateMetroTimeSigButtons();
  };
  timeSig4_.onClick = [this] {
    services_.backend.setMetronomeBeatsPerBar(4);
    updateMetroTimeSigButtons();
  };
  timeSig6_.onClick = [this] {
    services_.backend.setMetronomeBeatsPerBar(6);
    updateMetroTimeSigButtons();
  };
  addAndMakeVisible(timeSig2_);
  addAndMakeVisible(timeSig3_);
  addAndMakeVisible(timeSig4_);
  addAndMakeVisible(timeSig6_);
  updateMetroTimeSigButtons();

  // Metronome Click Toggle
  metroToggleBtn_.onClick = [this] {
    const bool running = services_.backend.isMetronomeEnabled();
    services_.backend.setMetronomeEnabled(!running);
    metroToggleBtn_.setLabel(!running ? "STOP CLICK" : "START CLICK");
  };
  addAndMakeVisible(metroToggleBtn_);

  // Metronome Volume
  metroVolSlider_.setRange(0.0, 100.0, 1.0);
  metroVolSlider_.setValue(services_.backend.getMetronomeGain() * 100.0f, juce::dontSendNotification);
  metroVolSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
  metroVolSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 24);
  metroVolSlider_.setTextValueSuffix("%");
  metroVolSlider_.setColour(juce::Slider::thumbColourId, theme::kAccent);
  metroVolSlider_.setColour(juce::Slider::trackColourId, theme::kSurfaceRaised);
  metroVolSlider_.setColour(juce::Slider::backgroundColourId, theme::kBlack);
  metroVolSlider_.setColour(juce::Slider::textBoxTextColourId, theme::kWhite);
  metroVolSlider_.setColour(juce::Slider::textBoxBackgroundColourId, theme::kSurface);
  metroVolSlider_.setColour(juce::Slider::textBoxOutlineColourId, theme::kBorder);
  metroVolSlider_.onValueChange = [this] {
    services_.backend.setMetronomeGain(static_cast<float>(metroVolSlider_.getValue()) / 100.0f);
  };
  addAndMakeVisible(metroVolSlider_);

  // --- Backing Track Controls ---
  browseFileBtn_.onClick = [this] {
    if (fileChooser_ != nullptr) return;
    fileChooser_ = std::make_unique<juce::FileChooser>(
        "Select Backing Track (WAV, MP3, FLAC, OGG)",
        juce::File::getSpecialLocation(juce::File::userMusicDirectory),
        "*.wav;*.mp3;*.flac;*.ogg;*.aif;*.aiff");

    const int flags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    juce::Component::SafePointer<JamView> self(this);
    fileChooser_->launchAsync(flags, [self](const juce::FileChooser& chooser) {
      if (self == nullptr) return;
      const auto result = chooser.getResult();
      if (result.existsAsFile()) {
        self->loadAudioFile(result);
      }
      juce::MessageManager::callAsync([self] {
        if (self != nullptr) self->fileChooser_.reset();
      });
    });
  };
  addAndMakeVisible(browseFileBtn_);

  // Play/Pause button
  playPauseBtn_.onClick = [this] {
    if (services_.backend.isJamTrackPlaying()) {
      services_.backend.pauseJamTrack();
    } else {
      services_.backend.playJamTrack();
    }
  };
  playPauseBtn_.setFillWhenActive(true);
  addAndMakeVisible(playPauseBtn_);

  // Stop button
  stopBtn_.onClick = [this] {
    services_.backend.stopJamTrack();
  };
  addAndMakeVisible(stopBtn_);

  // Loop button
  loopBtn_.onClick = [this] {
    const bool current = services_.backend.isJamTrackLooping();
    services_.backend.setJamTrackLooping(!current);
    loopBtn_.setFillWhenActive(!current);
  };
  loopBtn_.setFillWhenActive(services_.backend.isJamTrackLooping());
  addAndMakeVisible(loopBtn_);

  // Track Progress / Scrubber Slider
  trackProgressSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
  trackProgressSlider_.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  trackProgressSlider_.setRange(0.0, 1.0, 0.001);
  trackProgressSlider_.setValue(0.0, juce::dontSendNotification);
  trackProgressSlider_.setColour(juce::Slider::thumbColourId, theme::kAccent);
  trackProgressSlider_.setColour(juce::Slider::trackColourId, theme::kAccent);
  trackProgressSlider_.setColour(juce::Slider::backgroundColourId, theme::kBorder);
  trackProgressSlider_.onDragStart = [this] { isScrubbing_ = true; };
  trackProgressSlider_.onDragEnd = [this] {
    isScrubbing_ = false;
    const double len = services_.backend.getJamTrackLengthSeconds();
    if (len > 0.0) {
      const double targetSec = trackProgressSlider_.getValue() * len;
      services_.backend.setJamTrackPositionSeconds(targetSec);
    }
  };
  trackProgressSlider_.onValueChange = [this] {
    if (isScrubbing_) {
      const double len = services_.backend.getJamTrackLengthSeconds();
      if (len > 0.0) {
        const double targetSec = trackProgressSlider_.getValue() * len;
        services_.backend.setJamTrackPositionSeconds(targetSec);
      }
    }
  };
  addAndMakeVisible(trackProgressSlider_);

  // Track Volume
  trackVolSlider_.setRange(0.0, 150.0, 1.0);
  trackVolSlider_.setValue(services_.backend.getJamTrackGain() * 100.0f, juce::dontSendNotification);
  trackVolSlider_.setSliderStyle(juce::Slider::LinearHorizontal);
  trackVolSlider_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 45, 24);
  trackVolSlider_.setTextValueSuffix("%");
  trackVolSlider_.setColour(juce::Slider::thumbColourId, theme::kAccent);
  trackVolSlider_.setColour(juce::Slider::trackColourId, theme::kSurfaceRaised);
  trackVolSlider_.setColour(juce::Slider::backgroundColourId, theme::kBlack);
  trackVolSlider_.setColour(juce::Slider::textBoxTextColourId, theme::kWhite);
  trackVolSlider_.setColour(juce::Slider::textBoxBackgroundColourId, theme::kSurface);
  trackVolSlider_.setColour(juce::Slider::textBoxOutlineColourId, theme::kBorder);
  trackVolSlider_.onValueChange = [this] {
    services_.backend.setJamTrackGain(static_cast<float>(trackVolSlider_.getValue()) / 100.0f);
  };
  addAndMakeVisible(trackVolSlider_);

  startTimerHz(30);
}

JamView::~JamView() {
  stopTimer();
}

bool JamView::isInterestedInFileDrag(const juce::StringArray& files) {
  if (files.isEmpty()) return false;
  const juce::String ext = juce::File(files[0]).getFileExtension().toLowerCase();
  return (ext == ".wav" || ext == ".mp3" || ext == ".flac" || ext == ".ogg" || ext == ".aif" || ext == ".aiff");
}

void JamView::filesDropped(const juce::StringArray& files, int /*x*/, int /*y*/) {
  if (files.isEmpty()) return;
  loadAudioFile(juce::File(files[0]));
}

void JamView::loadAudioFile(const juce::File& file) {
  if (services_.backend.loadJamTrackFile(file)) {
    trackProgressSlider_.setValue(0.0, juce::dontSendNotification);
    repaint();
  }
}

void JamView::handleTapTempo() {
  const double now = juce::Time::getMillisecondCounterHiRes();
  if (tapTimesMs_.empty() || (now - tapTimesMs_.back()) > 2500.0) {
    tapTimesMs_.clear();
  }
  tapTimesMs_.push_back(now);

  if (tapTimesMs_.size() > 5) {
    tapTimesMs_.erase(tapTimesMs_.begin());
  }

  if (tapTimesMs_.size() >= 2) {
    double totalDelta = 0.0;
    for (size_t i = 1; i < tapTimesMs_.size(); ++i) {
      totalDelta += (tapTimesMs_[i] - tapTimesMs_[i - 1]);
    }
    const double avgDeltaMs = totalDelta / static_cast<double>(tapTimesMs_.size() - 1);
    if (avgDeltaMs > 100.0) {
      const double calculatedBpm = std::round(60000.0 / avgDeltaMs);
      bpmSlider_.setValue(juce::jlimit(40.0, 280.0, calculatedBpm));
    }
  }
}

void JamView::updateMetroTimeSigButtons() {
  const int beats = services_.backend.getMetronomeBeatsPerBar();
  timeSig2_.setMetrics(beats == 2 ? PillButton::kFilled : PillButton::kOutline);
  timeSig3_.setMetrics(beats == 3 ? PillButton::kFilled : PillButton::kOutline);
  timeSig4_.setMetrics(beats == 4 ? PillButton::kFilled : PillButton::kOutline);
  timeSig6_.setMetrics(beats == 6 ? PillButton::kFilled : PillButton::kOutline);
  repaint();
}

juce::String JamView::formatTime(double seconds) {
  const int totalSec = std::max(0, static_cast<int>(seconds));
  const int mins = totalSec / 60;
  const int secs = totalSec % 60;
  return juce::String::formatted("%02d:%02d", mins, secs);
}

void JamView::timerCallback() {
  // Sync Metronome toggle button label
  const bool metroOn = services_.backend.isMetronomeEnabled();
  metroToggleBtn_.setLabel(metroOn ? "STOP CLICK" : "START CLICK");

  // Sync Backing Track transport state
  const bool isPlaying = services_.backend.isJamTrackPlaying();
  playPauseBtn_.setSvg(isPlaying ? custom_icons::kPause : custom_icons::kPlay);
  loopBtn_.setFillWhenActive(services_.backend.isJamTrackLooping());

  if (!isScrubbing_) {
    const double len = services_.backend.getJamTrackLengthSeconds();
    if (len > 0.0) {
      const double pos = services_.backend.getJamTrackPositionSeconds();
      trackProgressSlider_.setValue(juce::jlimit(0.0, 1.0, pos / len), juce::dontSendNotification);
    } else {
      trackProgressSlider_.setValue(0.0, juce::dontSendNotification);
    }
  }

  // Repaint for metronome beat indicator pulse & track position text
  repaint();
}

void JamView::paint(juce::Graphics& g) {
  g.fillAll(theme::kBlack);

  const auto bounds = getLocalBounds().reduced(20, 16);
  const int gap = 24;
  const int cardW = (bounds.getWidth() - gap) / 2;
  const int cardH = bounds.getHeight();

  auto metroCard = juce::Rectangle<int>(bounds.getX(), bounds.getY(), cardW, cardH).toFloat();
  auto trackCard = juce::Rectangle<int>(bounds.getX() + cardW + gap, bounds.getY(), cardW, cardH).toFloat();

  // 1. Metronome Card Background
  paint::fill(g, metroCard, 10.0f, theme::kSurface);
  paint::border(g, metroCard, 10.0f, theme::kBorder);

  // Metronome Header
  g.setFont(Fonts::sans(15.0f, true));
  g.setColour(theme::kWhite);
  g.drawText("METRONOME", metroCard.getX() + 20, metroCard.getY() + 16, 200, 20, juce::Justification::centredLeft);

  // Tempo markings (Largo, Moderato, Allegro, etc.)
  const float bpm = services_.backend.getMetronomeBpm();
  juce::String tempoName = "Moderato";
  if (bpm < 60) tempoName = "Largo";
  else if (bpm < 76) tempoName = "Adagio";
  else if (bpm < 108) tempoName = "Andante";
  else if (bpm < 120) tempoName = "Moderato";
  else if (bpm < 168) tempoName = "Allegro";
  else if (bpm < 200) tempoName = "Presto";
  else tempoName = "Prestissimo";

  g.setFont(Fonts::mono(11.0f));
  g.setColour(theme::kAccent);
  g.drawText(tempoName.toUpperCase(), metroCard.getRight() - 180, metroCard.getY() + 16, 160, 20, juce::Justification::centredRight);

  // Large BPM readout
  g.setFont(Fonts::mono(42.0f, true));
  g.setColour(theme::kWhite);
  g.drawText(juce::String(static_cast<int>(bpm)), metroCard.getX() + 20, metroCard.getY() + 44, 120, 48, juce::Justification::centredLeft);

  g.setFont(Fonts::mono(12.0f));
  g.setColour(theme::kMuted);
  g.drawText("BPM", metroCard.getX() + 130, metroCard.getY() + 66, 60, 20, juce::Justification::centredLeft);

  // Beat indicator lamps
  const int totalBeats = services_.backend.getMetronomeBeatsPerBar();
  const int activeBeat = services_.backend.getMetronomeCurrentBeat();
  const bool metroRunning = services_.backend.isMetronomeEnabled();

  const float lampStartX = metroCard.getX() + 20.0f;
  const float lampY = metroCard.getY() + 104.0f;
  const float lampSize = 14.0f;
  const float lampGap = 12.0f;

  for (int i = 0; i < totalBeats; ++i) {
    auto lampBox = juce::Rectangle<float>(lampStartX + i * (lampSize + lampGap), lampY, lampSize, lampSize);
    const bool isThisBeat = metroRunning && (activeBeat == (i + 1));
    juce::Colour lampCol = theme::kSurfaceRaised;
    if (isThisBeat) {
      lampCol = (i == 0) ? theme::kAccent : theme::kWhite;
    }
    paint::fill(g, lampBox, lampSize * 0.5f, lampCol);
    paint::border(g, lampBox, lampSize * 0.5f, isThisBeat ? lampCol.brighter(0.3f) : theme::kBorder);
  }

  // Metronome Volume Label
  g.setFont(Fonts::sans(12.0f));
  g.setColour(theme::kMuted);
  g.drawText("Click Volume", metroCard.getX() + 20, metroCard.getY() + 218, 120, 20, juce::Justification::centredLeft);

  // 2. Backing Track Card Background
  paint::fill(g, trackCard, 10.0f, theme::kSurface);
  paint::border(g, trackCard, 10.0f, theme::kBorder);

  // Backing Track Header
  g.setFont(Fonts::sans(15.0f, true));
  g.setColour(theme::kWhite);
  g.drawText("BACKING TRACK PLAYER", trackCard.getX() + 20, trackCard.getY() + 16, 260, 20, juce::Justification::centredLeft);

  // Track name / status box
  const juce::String trackName = services_.backend.getJamTrackName();
  auto dropZone = juce::Rectangle<float>(trackCard.getX() + 20, trackCard.getY() + 48, trackCard.getWidth() - 40, 48);
  paint::fill(g, dropZone, 6.0f, theme::kBlack);
  paint::dashedBorder(g, dropZone, 6.0f, theme::kBorder, 1.0f);

  if (trackName.isNotEmpty()) {
    g.setFont(Fonts::mono(13.0f, true));
    g.setColour(theme::kWhite);
    g.drawText(trackName, dropZone.toNearestInt().reduced(12, 0), juce::Justification::centredLeft, true);
  } else {
    g.setFont(Fonts::sans(12.0f, false, true));
    g.setColour(theme::kMuted);
    g.drawText("Drop audio file here (WAV, MP3, FLAC, OGG) or click Browse", dropZone.toNearestInt(), juce::Justification::centred, false);
  }

  // Track Time Readout
  const double posSec = services_.backend.getJamTrackPositionSeconds();
  const double lenSec = services_.backend.getJamTrackLengthSeconds();
  const juce::String timeText = formatTime(posSec) + " / " + formatTime(lenSec);
  g.setFont(Fonts::mono(12.0f));
  g.setColour(theme::kWhite);
  g.drawText(timeText, trackCard.getX() + 20, trackCard.getY() + 104, 160, 20, juce::Justification::centredLeft);

  // Track Volume Label
  g.setFont(Fonts::sans(12.0f));
  g.setColour(theme::kMuted);
  g.drawText("Track Volume", trackCard.getX() + 20, trackCard.getY() + 218, 120, 20, juce::Justification::centredLeft);
}

void JamView::resized() {
  close_.setBounds(getWidth() - 44, 16, 28, 28);

  const auto bounds = getLocalBounds().reduced(20, 16);
  const int gap = 24;
  const int cardW = (bounds.getWidth() - gap) / 2;
  const int cardH = bounds.getHeight();

  const int metroX = bounds.getX() + 20;
  const int metroW = cardW - 40;
  const int metroY = bounds.getY();

  // BPM Slider
  bpmSlider_.setBounds(metroX, metroY + 130, metroW, 28);

  // Minus/Plus and Tap buttons row
  const int btnY = metroY + 168;
  minusFiveBtn_.setBounds(metroX, btnY, 40, 28);
  minusOneBtn_.setBounds(metroX + 46, btnY, 40, 28);
  plusOneBtn_.setBounds(metroX + 92, btnY, 40, 28);
  plusFiveBtn_.setBounds(metroX + 138, btnY, 40, 28);
  tapButton_.setBounds(metroX + 196, btnY, 80, 28);

  // Time Signature buttons
  timeSig2_.setBounds(metroX, btnY + 36, 44, 26);
  timeSig3_.setBounds(metroX + 50, btnY + 36, 44, 26);
  timeSig4_.setBounds(metroX + 100, btnY + 36, 44, 26);
  timeSig6_.setBounds(metroX + 150, btnY + 36, 44, 26);

  // Metronome Volume Slider
  metroVolSlider_.setBounds(metroX + 100, metroY + 242, metroW - 100, 28);

  // Start Click button
  metroToggleBtn_.setBounds(metroX, cardH - 36, metroW, 36);

  // --- Backing Track Controls ---
  const int trackX = bounds.getX() + cardW + gap + 20;
  const int trackW = cardW - 40;
  const int trackY = bounds.getY();

  // Scrubber slider
  trackProgressSlider_.setBounds(trackX, trackY + 128, trackW, 20);

  // Transport controls row (Play, Stop, Loop)
  const int trY = trackY + 156;
  playPauseBtn_.setBounds(trackX, trY, 44, 44);
  stopBtn_.setBounds(trackX + 54, trY + 3, 38, 38);
  loopBtn_.setBounds(trackX + 102, trY + 3, 38, 38);

  // Browse file button
  browseFileBtn_.setBounds(trackX + 160, trY + 5, trackW - 160, 34);

  // Track Volume Slider
  trackVolSlider_.setBounds(trackX + 100, trackY + 242, trackW - 100, 28);
}

}  // namespace t3k::ui
