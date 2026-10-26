#include "HintBar.h"

#include "core/Design.h"
#include "core/Fonts.h"
#include "core/Help.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
constexpr int kPadX = 24;
constexpr int kGap = 16;
// Fixed footprint sized for the widest value ("100.0%") so digit count
// changes never shift the row.
constexpr int kCpuMinWidth = 72;
constexpr int kCpuGap = 6;
constexpr int kTempoWidth = 142;
}  // namespace

// CPU readout
HintBar::CpuReadout::CpuReadout(MeterStore& meters) : meters_(meters) {
  setHelpText(help::text(help::Key::cpuLoad));
  meters_.addListener(this);
}

HintBar::TempoReadout::TempoReadout(TempoStore& tempo) : tempo_(tempo) {
  tempo_.addListener(this);
}

HintBar::TempoReadout::~TempoReadout() { tempo_.removeListener(this); }

void HintBar::TempoReadout::paint(juce::Graphics& g) {
  const auto& state = tempo_.state();
  const auto text = state.lock == 2 ? "EXT " + juce::String(state.bpm, 1) + " LOCKED"
                   : state.lock == 1
                       ? (state.bpm > 0.0f ? "EXT " + juce::String(state.bpm, 1) + " CLOCK"
                                           : "EXT - CLOCK")
                       : "EXT - NO SIGNAL";
  const auto font = Fonts::mono(10);
  const auto row = getLocalBounds();
  const auto textWidth = Fonts::width(font, text);
  const auto x = juce::jmax(0, juce::roundToInt((getWidth() - textWidth - 10.0f) * 0.5f));
  paint::text(g, text, row.withX(x).withWidth(getWidth() - x), font, theme::kMuted);
  g.setColour(state.activity ? theme::kAccent : theme::kBorder);
  g.fillEllipse(static_cast<float>(getWidth() - 7), getHeight() / 2.0f - 3.0f, 6.0f, 6.0f);
}

HintBar::CpuReadout::~CpuReadout() { meters_.removeListener(this); }

void HintBar::CpuReadout::paint(juce::Graphics& g) {
  const auto font = Fonts::sans(12);
  const auto value = juce::String(meters_.cpuPercent(), 1) + "%";
  const float wLabel = Fonts::width(font, "CPU");
  const float wValue = Fonts::width(font, value);
  const float x0 = getWidth() / 2.0f - (wLabel + kCpuGap + wValue) / 2;
  const auto row = getLocalBounds();
  paint::text(g, "CPU", row.withX(juce::roundToInt(x0)).withWidth(juce::roundToInt(wLabel) + 2), font,
              theme::kMuted);
  paint::text(g, value,
              row.withX(juce::roundToInt(x0 + wLabel + kCpuGap)).withWidth(juce::roundToInt(wValue) + 2),
              font, theme::kMuted);
}

// Bar
HintBar::HintBar(Services& services) : services_(services), cpu_(services.meters), tempo_(services.tempo) {
  addAndMakeVisible(cpu_);
  addAndMakeVisible(tempo_);
  hide_.setHelpText(help::text(help::Key::hideHints));
  hide_.onClick = [this] { services_.hints.setEnabled(false); };
  addAndMakeVisible(hide_);
  services_.hints.addListener(this);
  setSize(design::kWidth, design::kHintHeight);
}

HintBar::~HintBar() { services_.hints.removeListener(this); }

void HintBar::paint(juce::Graphics& g) {
  g.fillAll(theme::kBlack);
  paint::hairlineH(g, 0, static_cast<float>(getWidth()), 0, theme::kBorder);
  auto area = getLocalBounds().reduced(kPadX, 0);
  area.setRight(tempo_.getX() - kGap);
  paint::text(g, services_.hints.current(), area, Fonts::sans(13), theme::kMuted);
}

void HintBar::resized() {
  auto area = getLocalBounds().reduced(kPadX, 0);
  hide_.setBounds(area.removeFromRight(hide_.getWidth()).withSizeKeepingCentre(hide_.getWidth(), hide_.getHeight()));
  area.removeFromRight(kGap);
  tempo_.setBounds(area.removeFromRight(kTempoWidth));
  area.removeFromRight(kGap);
  cpu_.setBounds(area.removeFromRight(kCpuMinWidth));
}

}  // namespace t3k::ui
