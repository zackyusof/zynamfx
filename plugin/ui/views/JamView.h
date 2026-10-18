#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "core/CustomIcons.h"
#include "services/Services.h"
#include "widgets/IconButton.h"
#include "widgets/PillButton.h"

namespace t3k::ui {

class JamView : public juce::Component,
                public juce::Timer,
                public juce::FileDragAndDropTarget {
public:
  explicit JamView(Services& services);
  ~JamView() override;

  std::function<void()> onClose;

  void paint(juce::Graphics& g) override;
  void resized() override;
  void timerCallback() override;

  // juce::FileDragAndDropTarget
  bool isInterestedInFileDrag(const juce::StringArray& files) override;
  void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
  Services& services_;
  IconButton close_{Icon::X, 28, 20};

  // Metronome Section
  juce::Slider bpmSlider_;
  PillButton tapButton_{"TAP", PillButton::Style::outline};
  PillButton minusOneBtn_{"-1", PillButton::Style::outline};
  PillButton plusOneBtn_{"+1", PillButton::Style::outline};
  PillButton minusFiveBtn_{"-5", PillButton::Style::outline};
  PillButton plusFiveBtn_{"+5", PillButton::Style::outline};
  PillButton metroToggleBtn_{"START CLICK", PillButton::Style::filled};
  juce::Slider metroVolSlider_;

  PillButton timeSig2_{"2/4", PillButton::Style::outline};
  PillButton timeSig3_{"3/4", PillButton::Style::outline};
  PillButton timeSig4_{"4/4", PillButton::Style::filled};
  PillButton timeSig6_{"6/8", PillButton::Style::outline};

  // Backing Track Section
  PillButton browseFileBtn_{"Browse Audio File...", PillButton::Style::filled};
  IconButton playPauseBtn_{custom_icons::kPlay, 44, 26};
  IconButton stopBtn_{custom_icons::kStop, 38, 20};
  IconButton loopBtn_{custom_icons::kRepeat, 38, 20};
  juce::Slider trackVolSlider_;
  juce::Slider trackProgressSlider_;

  std::unique_ptr<juce::FileChooser> fileChooser_;

  // Tap tempo state
  std::vector<double> tapTimesMs_;
  void handleTapTempo();

  // Scrubber dragging state
  bool isScrubbing_ = false;

  void updateMetroTimeSigButtons();
  void loadAudioFile(const juce::File& file);
  static juce::String formatTime(double seconds);
};

}  // namespace t3k::ui
