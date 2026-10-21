#include "GalleryLane.h"

#include "GalleryGeometry.h"
#include "core/Design.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Theme.h"
#include "widgets/Clickable.h"

namespace t3k::ui {

namespace {
// Diameter of the branch dots: half the power-button chrome footprint.
constexpr int kBranchDot = theme::kIconBoxSize / 2;
// Stereo mode keeps the dots faintly visible so the parallel path can be
// found without hunting a 24px gap; hover/press brings them up.
constexpr float kBranchIdleAlpha = 0.5f;
}  // namespace

// Full-gap hover zone wrapping a branch/merge dot: the whole 24px connector
// run is the hit/hover area; the disc sits faint until hovered so the gap
// reads as a control instead of a plain connector (and is fully shown while
// the pointer is a finger, which can't hover a 24px gap). Rejoin dots are
// drawn as a ring so a mergable gap never reads as a branch point.
class GalleryLane::BranchGap : public Clickable, private Pointer::Listener {
public:
  BranchGap(Pointer& pointer, help::Key helpKey, bool ring, std::function<void()> action)
      : Clickable({}), pointer_(pointer), ring_(ring) {
    setHelpText(help::text(helpKey));
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    onClick = std::move(action);
    pointer_.addListener(this);
    setAlpha(pointer_.coarse() ? 1.0f : kBranchIdleAlpha);
    pointerChanged();
  }
  ~BranchGap() override { pointer_.removeListener(this); }

  void mouseEnter(const juce::MouseEvent&) override { setAlpha(1.0f); }
  void mouseExit(const juce::MouseEvent&) override {
    if (!pointer_.coarse()) setAlpha(kBranchIdleAlpha);
  }
  void pointerChanged() override {
    if (!isMouseOver()) setAlpha(pointer_.coarse() ? 1.0f : kBranchIdleAlpha);
  }

  void paintButton(juce::Graphics& g, bool, bool) override {
    const auto dot = getLocalBounds().toFloat().withSizeKeepingCentre(kBranchDot, kBranchDot);
    g.setColour(theme::kWhite);
    if (ring_)
      g.drawEllipse(dot.reduced(0.5f), 1.0f);
    else
      g.fillEllipse(dot);
  }

private:
  Pointer& pointer_;
  bool ring_ = false;
};

GalleryLane::GalleryLane(Services& services, ChainSide side) : services_(services), side_(side) {
  setInterceptsMouseClicks(false, true);
}

GalleryLane::~GalleryLane() = default;

GalleryTile* GalleryLane::tileFor(const std::string& blockId) const {
  const auto it = tiles_.find(blockId);
  return it == tiles_.end() ? nullptr : it->second.get();
}

int GalleryLane::indexOf(const std::string& blockId) const {
  for (size_t i = 0; i < items_.size(); ++i)
    if (items_[i].blockId == blockId) return static_cast<int>(i);
  return -1;
}

void GalleryLane::setItems(const std::vector<ChainItem>& items, int tileSize) {
  items_ = items;
  tile_ = tileSize;

  // Reuse tiles of the same kind; anything else is rebuilt.
  std::map<std::string, std::unique_ptr<GalleryTile>> next;
  for (size_t i = 0; i < items_.size(); ++i) {
    const auto& item = items_[i];
    auto existing = tiles_.find(item.blockId);
    std::unique_ptr<GalleryTile> tile;
    if (existing != tiles_.end()) {
      const bool sameKind = item.isInsert ? dynamic_cast<AddTile*>(existing->second.get()) != nullptr
                                          : dynamic_cast<ToneTile*>(existing->second.get()) != nullptr;
      if (sameKind && existing->second->tileSize() == tile_) tile = std::move(existing->second);
      tiles_.erase(existing);
    }
    if (tile == nullptr) {
      if (item.isInsert)
        tile = std::make_unique<AddTile>(services_, item.blockId, tile_);
      else
        tile = std::make_unique<ToneTile>(services_, item, tile_);
      wire(*tile);
      addAndMakeVisible(*tile);
    }
    if (auto* add = dynamic_cast<AddTile*>(tile.get())) {
      add->setRouting(AddTile::routingFor(static_cast<int>(i), static_cast<int>(items_.size())));
      add->setCanPaste(canPaste_);
    } else if (auto* tone = dynamic_cast<ToneTile*>(tile.get())) {
      tone->setBlock(item);
    }
    next[item.blockId] = std::move(tile);
  }
  tiles_ = std::move(next);  // dropped tiles (and their menus) go here
  applyVisibility();

  setSize(gallery::laneWidth(static_cast<int>(items_.size()), tile_), tile_);
  rebuildBranchGaps();
  resized();
  repaint();
}

void GalleryLane::setBranch(bool stereo, const std::optional<ChainBranch>& branch,
                            bool interactive) {
  stereo_ = stereo;
  branch_ = branch;
  branchInteractive_ = interactive;
  rebuildBranchGaps();
  resized();
}

void GalleryLane::setCanPaste(bool canPaste) {
  canPaste_ = canPaste;
  for (auto& [id, tile] : tiles_)
    if (auto* add = dynamic_cast<AddTile*>(tile.get())) add->setCanPaste(canPaste);
}

void GalleryLane::setPlaceholder(const std::string& blockId) {
  placeholder_ = blockId;
  applyVisibility();
  repaint();
}

// A rejoin ends the branch lane at the merge column: everything at or past it
// is in the parallel section before the sum, so the row stops inviting blocks
// there. Only insert anchors (the add tile and the ghost plus slots) go; real
// tiles stay put and keep processing.
bool GalleryLane::slotHidden(int index) const {
  if (!mergeCutoff_ || index < 0 || index >= static_cast<int>(items_.size()) ||
      !items_[static_cast<size_t>(index)].isInsert)
    return false;
  return gallery::slotAtOrPastJoin(index, tile_, *mergeCutoff_);
}

void GalleryLane::applyVisibility() {
  for (size_t i = 0; i < items_.size(); ++i)
    if (auto* tile = tileFor(items_[i].blockId))
      tile->setVisible(items_[i].blockId != placeholder_ && !slotHidden(static_cast<int>(i)));
}

void GalleryLane::setMergeCutoff(std::optional<float> localX) {
  if (mergeCutoff_ == localX) return;
  mergeCutoff_ = localX;
  applyVisibility();
  repaint();
}

std::unique_ptr<GalleryTile> GalleryLane::releaseTile(const std::string& blockId) {
  const auto it = tiles_.find(blockId);
  if (it == tiles_.end()) return nullptr;
  auto tile = std::move(it->second);
  tiles_.erase(it);
  return tile;
}

void GalleryLane::adoptTile(std::unique_ptr<GalleryTile> tile) {
  if (tile == nullptr) return;
  addChildComponent(*tile);  // reparents; the next setItems places and shows it
  wire(*tile);
  tiles_[tile->blockId()] = std::move(tile);
}

void GalleryLane::wire(GalleryTile& tile) {
  if (auto* add = dynamic_cast<AddTile*>(&tile)) {
    add->onAdd = [this](const std::string& id) { if (onAdd) onAdd(id); };
    add->onPaste = [this](const std::string& id) {
      if (onPaste) onPaste(indexOf(id));
    };
  } else if (auto* tone = dynamic_cast<ToneTile*>(&tile)) {
    tone->onOpen = [this](const std::string& id) { if (onOpen) onOpen(id); };
    tone->onSwap = [this](const std::string& id) { if (onSwap) onSwap(id); };
  }
}

// Every gap following a tone block carries a set-branch dot, except the
// active tap gap on the trunk lane, whose dot clears the branch instead. On
// the trunk lane, gaps after the tap also carry a rejoin ring: setting it
// merges the branch lane back in after the block before that gap, and the
// active merge gap's ring clears the merge.
void GalleryLane::rebuildBranchGaps() {
  gaps_.clear();
  if (!stereo_) return;
  const bool trunk = branch_.has_value() && branch_->side == side_;
  const int tapIndex = trunk ? indexOf(branch_->afterBlockId) : -1;
  const int mergeIndex =
      trunk && !branch_->mergeAfterBlockId.empty() ? indexOf(branch_->mergeAfterBlockId) : -1;
  const int count = static_cast<int>(items_.size());
  for (int i = 0; i < count - 1; ++i) {
    const auto& item = items_[static_cast<size_t>(i)];
    if (item.isInsert) continue;
    const bool tap = trunk && i == tapIndex;
    // A rejoin only exists downstream of the split: the merged-into block is
    // a trunk block after the tap. `i` is the block before this gap.
    const bool mergeGap = trunk && tapIndex != -1 && i > tapIndex;
    const bool mergeHere = mergeGap && i == mergeIndex;
    if (!tap && !mergeHere && !branchInteractive_) continue;
    std::unique_ptr<BranchGap> gap;
    if (tap)
      gap = std::make_unique<BranchGap>(services_.pointer, help::Key::branchJunction,
                                        /*ring=*/false,
                                        [this] { if (onClearBranch) onClearBranch(); });
    else if (mergeHere)
      gap = std::make_unique<BranchGap>(services_.pointer, help::Key::mergeJunction, /*ring=*/true,
                                        [this] { if (onClearMerge) onClearMerge(); });
    else if (mergeGap)
      gap = std::make_unique<BranchGap>(services_.pointer, help::Key::mergeGap, /*ring=*/true,
                                        [this, id = item.blockId] {
                                          if (onSetMerge) onSetMerge(id);
                                        });
    else
      gap = std::make_unique<BranchGap>(services_.pointer, help::Key::branchGap,
                                        /*ring=*/false,
                                        [this, id = item.blockId] {
                                          if (onSetBranch) onSetBranch(id);
                                        });
    gap->setBounds(design::snap(gallery::gapCentreX(i + 1, tile_) - gallery::kTileGap / 2.0f), 0,
                   gallery::kTileGap, tile_);
    addAndMakeVisible(*gap);
    gap->toFront(false);
    gaps_.push_back(std::move(gap));
  }
}

void GalleryLane::setSlotPan(bool show) {
  for (const auto& entry : tiles_)
    if (entry.second != nullptr) entry.second->hideSlotPan();
  if (!show) return;
  // Every populated slot carries its own placement, so either row can be
  // panned from wherever the pointer already is instead of only from the
  // lane's last block. A slot that has its own pan shows it bright; an unset
  // slot shows the lane's resolved placement dimmed and inherits. Inserts are
  // the empty add slots, not audio.
  const double lanePan = side_ == ChainSide::left ? services_.chain.state().panLeft
                                                  : services_.chain.state().panRight;
  for (const auto& item : items_) {
    if (item.isInsert) continue;
    const bool set = item.params.pan >= 0.0;
    const float v = static_cast<float>(set ? item.params.pan : lanePan);
    if (auto* tile = tileFor(item.blockId))
      tile->showSlotPan(item.blockId, side_, v, set, item.placesBothLanes);
  }
}

void GalleryLane::resized() {
  for (size_t i = 0; i < items_.size(); ++i)
    if (auto* tile = tileFor(items_[i].blockId))
      tile->setBounds(gallery::tileX(static_cast<int>(i), tile_), 0, tile_, tile_);
}

// The ghost rail: one plus circle per slot, connector lines between them,
// each line extended past the icon boxes by plusCircleInset so it meets the
// drawn ring (stopping at the box edge reads as a hairline gap). A rejoin ends
// this (branch) lane at the merge column: no plus slot — and no connector run
// trailing into one — is drawn past it.
void GalleryLane::paint(juce::Graphics& g) {
  const int count = static_cast<int>(items_.size());
  if (count == 0) return;
  const int icon = gallery::plusIconSize(tile_);
  const float inset = gallery::plusCircleInset(icon);
  const float cy = tile_ / 2.0f;
  // Tiles repaint on every meter tick; only draw the slots that actually
  // fall in the dirty area rather than re-rendering the SVG for every slot.
  const auto clip = g.getClipBounds().toFloat();
  g.setColour(theme::kWhite);
  for (int i = 0; i < count; ++i) {
    if (slotHidden(i)) continue;
    const float cx = gallery::tileX(i, tile_) + tile_ / 2.0f;
    if (i > 0 && !slotHidden(i - 1)) {
      const float x0 = cx - (tile_ + gallery::kTileGap) + icon / 2.0f - inset;
      const float x1 = cx - icon / 2.0f + inset;
      const juce::Rectangle<float> line(x0, cy - gallery::kLineWidth / 2, x1 - x0, gallery::kLineWidth);
      if (line.intersects(clip)) g.fillRect(line);
    }
    const auto circle = juce::Rectangle<float>(icon, icon).withCentre({cx, cy});
    if (circle.intersects(clip))
      Icons::draw(g, Icon::PlusCircle, circle, theme::kWhite, /*strokeWidth=*/1.0f);
  }
}

}  // namespace t3k::ui
