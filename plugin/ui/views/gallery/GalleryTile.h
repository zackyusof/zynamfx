// Gesture base for the gallery's tiles (the useTileMenu / useSortable /
// file-drop wiring of GalleryBlock.tsx): a primary click opens the tile (the
// tone loader / the block detail), a right-click or ctrl-click opens the
// hosted AU/VST3 effect chooser directly — a held touch stands in for the
// right-click on a touch screen. Travel past the drag distance hands the tile
// to the lane's drag host, and an OS file drag arms the tile as a drop target.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <memory>
#include <optional>
#include <string>

#include "core/DelayedCall.h"
#include "model/ChainState.h"
#include "services/Services.h"
#include "widgets/EffectMenu.h"

namespace t3k::ui { class SlotPan; }

namespace t3k::ui {

class GalleryTile;

// The lane's owner (ChainView) runs the drag: the tile only reports
// pointer travel. A keyboard grab sorts one slot per arrow press.
class TileDragHost {
public:
  virtual ~TileDragHost() = default;
  virtual void tileDragStart(GalleryTile& tile, const juce::MouseEvent& e) = 0;
  virtual void tileDragMove(const juce::MouseEvent& e) = 0;
  virtual void tileDragEnd(const juce::MouseEvent& e) = 0;
  // Space/Enter picks up or drops; arrows move; Escape cancels. Returns
  // whether the key was consumed.
  virtual bool tileKey(GalleryTile& tile, const juce::KeyPress& key) = 0;
};

class GalleryTile : public juce::Component, public juce::FileDragAndDropTarget {
public:
  GalleryTile(Services& services, std::string blockId, int size);
  ~GalleryTile() override;

  const std::string& blockId() const { return blockId_; }
  int tileSize() const { return size_; }
  // The web dims a travelling tile (dnd-kit Feedback) to 0.75.
  void setTravelling(bool travelling);
  bool travelling() const { return travelling_; }

  void mouseDown(const juce::MouseEvent& e) override;
  void mouseDrag(const juce::MouseEvent& e) override;
  void mouseUp(const juce::MouseEvent& e) override;
  bool keyPressed(const juce::KeyPress& key) override;
  // A button to screen readers: press opens, show-menu opens the hosted
  // effect chooser. The subclass names it with setTitle().
  std::unique_ptr<juce::AccessibilityHandler> createAccessibilityHandler() override;

  // The slot's own placement, shown on every populated slot of the lane
  // (stereo mode): 0 = hard left, 0.5 = centre, 1 = hard right. Bound to the
  // slot's own block, so each slot places independently; `set` false means
  // the slot inherits (and displays dimmed) the lane's resolved placement.
  // `placesBothLanes` marks a shared-rejoin-tail placement: it applies to
  // both lanes after the merge and gets its own affordance in the strip.
  void showSlotPan(const std::string& blockId, ChainSide side, float value, bool set,
                   bool placesBothLanes = false);
  void hideSlotPan();
  bool hasSlotPan() const { return slotPan_ != nullptr; }

  bool isInterestedInFileDrag(const juce::StringArray&) override { return true; }
  void fileDragEnter(const juce::StringArray&, int, int) override { setDropArmed(true); }
  void fileDragExit(const juce::StringArray&) override { setDropArmed(false); }
  void filesDropped(const juce::StringArray& files, int, int) override;

protected:
  // Primary click (not a drag, not a swallowed post-menu click).
  virtual void open() = 0;
  // The secondary click (right-click, ctrl-click, a touch hold, a screen
  // reader's "show menu"): the hosted AU/VST3 chooser, opened directly over
  // this tile and loading into this slot (blockId()).
  virtual void openSecondaryMenu(juce::Point<int> at);
  // An OS file drag is hovering (upload glyph + green dashed border).
  virtual void dropArmedChanged(bool /*armed*/) {}
  virtual void travellingChanged(bool /*travelling*/) {}

  bool dropArmed() const { return dropArmed_; }
  bool effectMenuOpen() const { return effectMenu_ != nullptr && effectMenu_->isOpen(); }
  Services& services() { return services_; }
  void closeEffectMenu();
  // The tile's own local-file affordances: the OS picker, loading into this
  // slot (an insert slot adds, a tone tile swaps in place).
  void loadLocalFile();
  void loadLocalFolder();

private:
  static constexpr int kLongPressMs = 500;
  static constexpr int kLongPressSlop = 5;
  // A touch that covers the drag distance within this is a swipe that pans
  // the lane, not a sort. In practice a dwell threshold: 6 px in 150 ms is
  // only 40 px/s, so what separates the two is whether the finger paused
  // before it moved.
  static constexpr int kFlickMs = 150;
  // How long a set click suppression stays valid.
  static constexpr int kSuppressClickMs = 700;

  // The vendor -> effect two-column chooser (the same one the add tile had).
  void openHostedEffectMenu();
  void setDropArmed(bool armed);
  TileDragHost* host();

  Services& services_;
  std::string blockId_;
  int size_;
  bool travelling_ = false;
  bool dropArmed_ = false;
  bool dragging_ = false;
  juce::Point<int> pressAt_;
  juce::int64 suppressClickUntilMs_ = 0;
  DelayedCall hold_;
  std::unique_ptr<EffectMenu> effectMenu_;
  std::unique_ptr<SlotPan> slotPan_;
  // Which slot the pan strip belongs to: a tile can change lanes (tap /
  // re-point a branch) and is reused across resyncs, so a reused tile must
  // not keep the old block's or lane's control.
  std::string slotPanBlockId_;
  std::optional<ChainSide> slotPanSide_;
};

}  // namespace t3k::ui
