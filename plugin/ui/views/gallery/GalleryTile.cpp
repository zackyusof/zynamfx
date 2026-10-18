#include "GalleryTile.h"

#include "SlotPan.h"

#include <utility>
#include <vector>

#include "GalleryGeometry.h"

namespace t3k::ui {

GalleryTile::GalleryTile(Services& services, std::string blockId, int size)
    : services_(services), blockId_(std::move(blockId)), size_(size) {
  setSize(size, size);
  setMouseCursor(juce::MouseCursor::PointingHandCursor);
  // A drag on a tile sorts it; the gaps around the tiles pan the chain view.
  // A quick touch swipe clears this to hand the gesture over (mouseDrag).
  setViewportIgnoreDragFlag(true);
  // Sortable tiles are focusable (keyboard sorting) but draw no focus ring,
  // as the web didn't.
  setWantsKeyboardFocus(true);
  setMouseClickGrabsKeyboardFocus(false);
}

GalleryTile::~GalleryTile() {
  // The chooser lives on the overlay layer; take it down with the tile.
  if (effectMenu_ != nullptr) effectMenu_->close();
}

TileDragHost* GalleryTile::host() { return findParentComponentOfClass<TileDragHost>(); }

void GalleryTile::setTravelling(bool travelling) {
  if (travelling_ == travelling) return;
  travelling_ = travelling;
  setAlpha(travelling ? gallery::kDragGhostOpacity : 1.0f);
  travellingChanged(travelling);
}

void GalleryTile::setDropArmed(bool armed) {
  if (dropArmed_ == armed) return;
  dropArmed_ = armed;
  dropArmedChanged(armed);
  repaint();
}

void GalleryTile::filesDropped(const juce::StringArray& files, int, int) {
  setDropArmed(false);
  services_.localFiles.drop(blockId_, files);
}

void GalleryTile::loadLocalFile() {
  services_.localFiles.pick(blockId_, LocalFiles::Kind::file);
}

void GalleryTile::loadLocalFolder() {
  services_.localFiles.pick(blockId_, LocalFiles::Kind::folder);
}

// The hosted AU/VST3 chooser: right-click / ctrl-click / touch hold, on any
// slot tile (the "+" tile and a filled tone tile alike).
void GalleryTile::openSecondaryMenu(juce::Point<int> at) {
  // The chooser is a two-column panel, so it centres itself over the tile
  // rather than opening at the pointer; the press point is the anchor only
  // for tiles that override this.
  juce::ignoreUnused(at);
  // The release that follows (a right-click's, or the lifted finger of a touch
  // hold) must not read as a click on the tile.
  suppressClickUntilMs_ = juce::Time::currentTimeMillis() + kSuppressClickMs;
  openHostedEffectMenu();
}

void GalleryTile::openHostedEffectMenu() {
  // The bundles come from a plain directory listing (no scan/instantiation), so
  // this is instant. EffectMenu is our own two-column popover (vendors -> that
  // vendor's plugins) rather than a native PopupMenu, which lets us blur the
  // vendor column while the plugin column is active.
  auto& svc = services_;
  const auto listed = svc.backend.listHostedEffectBundles();
  const auto* array = listed.getArray();
  if (array == nullptr || array->isEmpty()) {
#if JUCE_MAC
    svc.toast.show("No AU/VST3 effects found in /Library/Audio/Plug-Ins");
#else
    svc.toast.show("No VST3 effects found in Common Files/VST3");
#endif
    return;
  }

  std::vector<EffectMenu::Entry> entries;
  entries.reserve(static_cast<size_t>(array->size()));
  for (const auto& effect : *array)
    entries.push_back({effect["vendor"].toString(), effect["name"].toString(),
                       effect["format"].toString() == "VST3" ? juce::String("VST3")
                                                             : juce::String("AU"),
                       effect["path"].toString()});

  effectMenu_ = std::make_unique<EffectMenu>(std::move(entries));
  // Defer the load until after this click has fully unwound: adding the block
  // triggers a chain resync that can rebuild the gallery (destroying the tiles and
  // this menu) — doing it inline is a use-after-free while the button is still in
  // its mouseUp. Services outlives the tiles, so capture it (not `this`).
  Services* svcPtr = &svc;
  const std::string target = blockId();
  effectMenu_->onChoose = [svcPtr, target](const juce::String& path, const juce::String& name) {
    juce::MessageManager::callAsync([svcPtr, target, path, name] {
      const auto id = svcPtr->backend.addHostedEffectFromFile(juce::File(path), target);
      if (id.empty()) {
        svcPtr->toast.show("Could not load this effect");
        return;
      }
      svcPtr->chain.refresh(true);
      // The effect block's detail embeds the plugin's GUI (open the tile to see it).
      juce::ignoreUnused(name);
    });
  };
  // The press that dismisses the chooser lands on the tile underneath: swallow
  // the click it would otherwise turn into (opening the tile).
  effectMenu_->onDismiss = [safe = juce::Component::SafePointer<GalleryTile>(this)] {
    if (safe != nullptr)
      safe->suppressClickUntilMs_ = juce::Time::currentTimeMillis() + kSuppressClickMs;
  };
  effectMenu_->open(*this, Popover::Align::left, 8);
  effectMenu_->centreOn(*this);  // centre over the tile and keep inside the window
  effectMenu_->focusSearch();    // the search field takes the keyboard on open
}

void GalleryTile::showSlotPan(const std::string& blockId, ChainSide side, float value, bool set) {
  // A tile can change lanes (tap / re-point a branch) and is reused across
  // resyncs, so a strip bound to another slot or lane is rebuilt. Otherwise
  // the strip is only re-valued (no allocation).
  if (slotPan_ == nullptr || slotPanBlockId_ != blockId || !slotPanSide_.has_value() ||
      *slotPanSide_ != side) {
    slotPan_.reset();
    slotPan_ = std::make_unique<SlotPan>(services_, blockId, side);
    slotPanBlockId_ = blockId;
    slotPanSide_ = side;
    addAndMakeVisible(*slotPan_);
  }
  slotPan_->setValue(value, set);
  // Bottom strip of the slot, inside the artwork's padding.
  const int pad = 6;
  slotPan_->setBounds(pad, size_ - SlotPan::kHeight - pad, size_ - 2 * pad, SlotPan::kHeight);
  slotPan_->setVisible(true);
}

void GalleryTile::hideSlotPan() {
  if (slotPan_ != nullptr) slotPan_->setVisible(false);
}

void GalleryTile::closeEffectMenu() {
  // Kept alive: we may be inside one of its row's click.
  if (effectMenu_ != nullptr) effectMenu_->close();
}

// Pointer
void GalleryTile::mouseDown(const juce::MouseEvent& e) {
  dragging_ = false;
  setViewportIgnoreDragFlag(true);  // re-arm after a pan whose release never came
  pressAt_ = e.getPosition();

  // Right-click / ctrl-click: the hosted-effect chooser, directly (the click
  // that follows is swallowed).
  if (e.mods.isPopupMenu() && !e.source.isTouch()) {
    openSecondaryMenu(e.getPosition());
    return;
  }

  // A held touch opens the chooser itself, on the system's long-press delay,
  // while the finger is down. Any touch, not only coarse-pointer builds: a
  // Windows tablet is a desktop build with a finger on it, and once the
  // editor takes touch directly (NativeEditor::usesWindowsMultiTouch) the
  // OS no longer synthesises a contextmenu press for us. Where a platform
  // does still turn the hold into a right-click, that path opened the chooser
  // first and this one leaves it alone.
  if (e.source.isTouch()) {
    hold_.start(kLongPressMs, [this, at = e.getPosition()] {
      if (!effectMenuOpen()) openSecondaryMenu(at);
    });
  }
}

void GalleryTile::mouseDrag(const juce::MouseEvent& e) {
  if (dragging_) {
    if (auto* h = host()) h->tileDragMove(e);
    return;
  }
  if (!getViewportIgnoreDragFlag()) return;  // a swipe: the chain view's viewport has it
  const auto travel = e.getPosition() - pressAt_;
  if (hold_.pending() &&
      (std::abs(travel.x) > kLongPressSlop || std::abs(travel.y) > kLongPressSlop))
    hold_.cancel();
  if (!e.mods.isLeftButtonDown() && !e.source.isTouch()) return;
  if (e.getDistanceFromDragStart() < gallery::kDragDistance) return;
  // A quick, mostly sideways touch swipe pans the chain view, as the web
  // view's scroll did; a slower drag still sorts, and the hold still opens
  // the chooser. Clearing the flag hands over: the viewport's drag listener
  // checks it on every move, this one included, and pans from the press with
  // its own inertia. Any touch source: iOS, and a finger on a Windows or
  // Linux tablet, which JUCE reports the same way. A mouse never gets here.
  if (e.source.isTouch() && (e.eventTime - e.mouseDownTime).inMilliseconds() < kFlickMs &&
      std::abs(travel.x) > std::abs(travel.y)) {
    hold_.cancel();
    setViewportIgnoreDragFlag(false);
    return;
  }
  // Past the activation distance the press is a drag; a chooser the hold
  // already opened yields to it.
  closeEffectMenu();
  hold_.cancel();
  dragging_ = true;
  if (auto* h = host()) h->tileDragStart(*this, e);
}

void GalleryTile::mouseUp(const juce::MouseEvent& e) {
  hold_.cancel();
  if (!getViewportIgnoreDragFlag()) {  // the press became a pan: no click, no sort end
    setViewportIgnoreDragFlag(true);
    return;
  }
  if (dragging_) {
    dragging_ = false;
    if (auto* h = host()) h->tileDragEnd(e);
    return;
  }
  if (e.mods.isPopupMenu() && !e.source.isTouch()) return;
  if (!e.mouseWasClicked()) return;
  const auto now = juce::Time::currentTimeMillis();
  if (now < suppressClickUntilMs_) {
    suppressClickUntilMs_ = 0;
    return;
  }
  if (e.mods.isCtrlDown() || e.mods.isCommandDown()) return;
  if (effectMenuOpen()) {
    closeEffectMenu();
    return;
  }
  open();
}

// Where this tile's block sits right now. A tile can outlive its block for a
// resync (a remove from another view), so every shortcut below is a no-op
// unless the block is still in the chain — and inserts are never touched.
namespace {
const ChainItem* locateBlock(const ChainState& state, const std::string& blockId,
                             ChainSide& side, int& index) {
  for (size_t i = 0; i < state.chain.size(); ++i)
    if (state.chain[i].blockId == blockId) {
      side = ChainSide::left;
      index = static_cast<int>(i);
      return &state.chain[i];
    }
  if (state.chainRight)
    for (size_t i = 0; i < state.chainRight->size(); ++i)
      if ((*state.chainRight)[i].blockId == blockId) {
        side = ChainSide::right;
        index = static_cast<int>(i);
        return &(*state.chainRight)[i];
      }
  return nullptr;
}
}  // namespace

bool GalleryTile::keyPressed(const juce::KeyPress& key) {
  // The block clipboard on the keyboard: ⌘C copies this block, ⌘V pastes the
  // copy into this slot, Delete/Backspace removes it (the two context-menu
  // rows the tile lost). Every other key — space/enter to pick up, arrows to
  // sort, escape empty — stays the drag host's.
  const auto mods = key.getModifiers();
  const int code = key.getKeyCode();
  const bool editMod = mods.isCommandDown() || mods.isCtrlDown();
  ChainSide side = ChainSide::left;
  int index = 0;
  if (const ChainItem* item = locateBlock(services_.chain.state(), blockId_, side, index)) {
    if (!item->isInsert) {
      if (editMod && (code == 'C' || code == 'c')) {
        services_.chain.copyBlock(blockId_);
        return true;
      }
      if (editMod && (code == 'V' || code == 'v')) {
        services_.chain.pasteBlock(side, index);
        return true;
      }
      if (!editMod && !mods.isAltDown() &&
          (code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey)) {
        // The resync deletes this tile: remove on the message loop so the key
        // callback returns into a live component.
        auto* services = &services_;
        const auto id = blockId_;
        juce::MessageManager::callAsync([services, id] { services->chain.removeBlock(id); });
        return true;
      }
    }
  }
  if (auto* h = host()) return h->tileKey(*this, key);
  return false;
}

std::unique_ptr<juce::AccessibilityHandler> GalleryTile::createAccessibilityHandler() {
  return std::make_unique<juce::AccessibilityHandler>(
      *this, juce::AccessibilityRole::button,
      juce::AccessibilityActions()
          .addAction(juce::AccessibilityActionType::press, [this] { open(); })
          .addAction(juce::AccessibilityActionType::showMenu,
                     [this] { openSecondaryMenu(getLocalBounds().getCentre()); }));
}

}  // namespace t3k::ui
