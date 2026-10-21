#include "SlotPan.h"

#include <cmath>

#include "core/Fonts.h"
#include "core/Theme.h"

namespace t3k::ui {

namespace {
constexpr int kInset = 6;      // room for the L / R letters
constexpr int kThumbW = 8;
constexpr int kTrackH = 4;
// An inherited placement reads as "not placed here": the whole strip dims.
constexpr float kInheritedAlpha = 0.45f;
}  // namespace

SlotPan::SlotPan(Services& services, std::string blockId, ChainSide side)
    : services_(services), blockId_(std::move(blockId)), side_(side) {
  value_ = laneDefault();
  inherited_ = laneDefault();
  updateAccessibility();  // screen-reader name + description
  setWantsKeyboardFocus(true);
  setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
}

SlotPan::~SlotPan() = default;

void SlotPan::setValue(float value, bool set, bool placesBothLanes) {
  value_ = juce::jlimit(0.0f, 1.0f, value);
  set_ = set;
  if (!set) inherited_ = value_;
  if (placesBothLanes != placesBothLanes_) {
    placesBothLanes_ = placesBothLanes;
    updateAccessibility();
  }
  repaint();
}

void SlotPan::updateAccessibility() {
  if (placesBothLanes_) {
    // A shared-tail placement is a balance of the already-merged image: name
    // it distinctly so it is never read as a single-lane placement.
    setTitle("Slot pan (both lanes)");
    setDescription("Applies to both lanes after the merge: 0 = hard left, 0.5 = centre, "
                   "1 = hard right of the merged signal; double-click to inherit");
  } else {
    setTitle("Slot pan");
    setDescription("0 = hard left, 0.5 = centre, 1 = hard right; double-click to inherit the lane");
  }
}

void SlotPan::paint(juce::Graphics& g) {
  const float a = set_ ? 1.0f : kInheritedAlpha;
  auto r = getLocalBounds().toFloat();
  const float cy = r.getCentreY();
  const float x0 = r.getX() + kInset, x1 = r.getRight() - kInset;
  const float inner = juce::jmax(1.0f, x1 - x0);

  // End letters, then the track between them. A shared-tail placement reaches
  // both lanes, so the L/R ends stay bright (never dimmed) and take the app
  // accent: both ends are live, not just the played lane.
  const float lettersAlpha = placesBothLanes_ ? 1.0f : a;
  g.setFont(Fonts::mono(8.0f));
  g.setColour((placesBothLanes_ ? theme::kAccent : theme::kMuted).withMultipliedAlpha(lettersAlpha));
  g.drawText("L", r.withWidth(static_cast<float>(kInset)).withTrimmedLeft(1.0f).toNearestInt(),
             juce::Justification::centredLeft, false);
  g.drawText("R", r.withTrimmedLeft(r.getWidth() - static_cast<float>(kInset)).toNearestInt(),
             juce::Justification::centredRight, false);

  const juce::Rectangle<float> track(x0, cy - kTrackH / 2.0f, inner, static_cast<float>(kTrackH));
  g.setColour(theme::kSegmentedTrack.withMultipliedAlpha(a));
  g.fillRoundedRectangle(track, kTrackH / 2.0f);
  // Centre notch: the "no pan" reference.
  g.setColour(theme::kSubtle.withMultipliedAlpha(a));
  g.fillRect(juce::Rectangle<float>(x0 + inner / 2.0f - 0.5f, cy - 3.0f, 1.0f, 6.0f));

  // Bipolar progress: the bar grows out of the centre notch toward the
  // thumb, so "no pan" reads as an empty bar on both lanes.
  const float centreX = x0 + inner / 2.0f;
  const float thumbX = x0 + value_ * inner;
  if (std::abs(thumbX - centreX) > 1.0f) {
    const juce::Rectangle<float> fill(std::min(centreX, thumbX), cy - kTrackH / 2.0f,
                                      std::abs(thumbX - centreX), static_cast<float>(kTrackH));
    g.setColour(theme::kWhite.withAlpha(0.85f * a));
    g.fillRoundedRectangle(fill, kTrackH / 2.0f);
  }

  g.setColour(theme::kWhite.withMultipliedAlpha(a));
  g.fillRoundedRectangle(juce::Rectangle<float>(thumbX - kThumbW / 2.0f, cy - 6.0f,
                                                static_cast<float>(kThumbW), 12.0f),
                         3.0f);

  // Shared-tail marker: a compact two-headed arrow along the track ("this
  // placement reaches both ends/lanes"). Drawn last, so the heads stay visible
  // wherever the grip sits (a hard-panned grip would otherwise hide one).
  // No asset: a juce::Path in the accent.
  if (placesBothLanes_) {
    const float y = cy;
    const float lx = x0 + 1.5f, rx = x1 - 1.5f;
    const float head = 3.5f, flare = 3.0f;
    juce::Path arrow;
    arrow.startNewSubPath(lx, y);
    arrow.lineTo(rx, y);
    arrow.startNewSubPath(lx, y);
    arrow.lineTo(lx + head, y - flare);
    arrow.startNewSubPath(lx, y);
    arrow.lineTo(lx + head, y + flare);
    arrow.startNewSubPath(rx, y);
    arrow.lineTo(rx - head, y - flare);
    arrow.startNewSubPath(rx, y);
    arrow.lineTo(rx - head, y + flare);
    g.setColour(theme::kAccent.withAlpha(a));
    g.strokePath(arrow, juce::PathStrokeType(1.2f));
  }
}

void SlotPan::mouseDown(const juce::MouseEvent& e) {
  grabKeyboardFocus();
  if (e.getNumberOfClicks() >= 2) {
    write(-1.0f, false);  // clear: inherit the lane's placement
    return;
  }
  writeFromX(e.x);
}

void SlotPan::mouseDrag(const juce::MouseEvent& e) { writeFromX(e.x); }

void SlotPan::writeFromX(int x) {
  const float x0 = static_cast<float>(kInset);
  const float inner = juce::jmax(1.0f, static_cast<float>(getWidth()) - 2.0f * x0);
  const float v = juce::jlimit(0.0f, 1.0f, (static_cast<float>(x) - x0) / inner);
  write(v, true);
}

void SlotPan::nudge(float delta) {
  write(juce::jlimit(0.0f, 1.0f, value_ + delta), true);
}

void SlotPan::write(float v, bool set) {
  services_.chain.setBlockParam(blockId_, "pan", set ? static_cast<double>(v) : -1.0);
  set_ = set;
  if (set) {
    value_ = v;
  } else {
    value_ = inherited_;
  }
  repaint();
}

bool SlotPan::keyPressed(const juce::KeyPress& key) {
  const float step = key.getModifiers().isShiftDown() ? 0.005f : 0.02f;
  if (key == juce::KeyPress::leftKey) { nudge(-step); return true; }
  if (key == juce::KeyPress::rightKey) { nudge(step); return true; }
  if (key == juce::KeyPress::homeKey) { nudge(-1.0f); return true; }
  if (key == juce::KeyPress::endKey) { nudge(1.0f); return true; }
  return false;
}

}  // namespace t3k::ui
