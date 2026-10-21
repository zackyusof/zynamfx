#include "ToneTile.h"
#include "core/Brand.h"

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "core/BlockArt.h"
#include "core/Fonts.h"
#include "widgets/HostedEffectWindow.h"

namespace t3k::ui {

namespace {
constexpr int kImageFadeMs = 200;
constexpr float kDimmedImage = 0.35f;
const juce::Colour kStrip = juce::Colours::black.withAlpha(0.35f);

}  // namespace

void ToneTile::Strip::paint(juce::Graphics& g) {
  g.setColour(kStrip);
  g.fillRect(getLocalBounds().withHeight(kChromeHeight));
}

ToneTile::ToneTile(Services& services, const ChainItem& block, int size)
    : GalleryTile(services, block.blockId, size),
      block_(block),
      image_(services.images),
      power_(Icon::Power, ChromeIconButton::Tone::power, help::Key::blockPower),
      swap_(Icon::ArrowLeftRight, ChromeIconButton::Tone::plain, help::Key::swapTone),
      remove_(Icon::Trash2, ChromeIconButton::Tone::plain, help::Key::removeBlock),
      glow_(services.meters, MeterStore::blockOutId(block.blockId)),
      led_(services.meters, MeterStore::blockOutId(block.blockId)) {
  image_.setCornerRadius(gallery::kTileCorner);
  image_.setGlow(glow_.glow());
  glow_.onChange = [this] { image_.setGlow(glow_.glow()); };
  addAndMakeVisible(image_);
  addChildComponent(dots_);
  addChildComponent(retry_);
  retry_.onRetry = [this] { this->services().modelLoads.retry(blockId()); };

  addAndMakeVisible(chrome_);
  for (auto* b : {&power_, &swap_, &remove_}) chrome_.addAndMakeVisible(*b);
  power_.onClick = [this] { togglePower(); };
  swap_.onClick = [this] { if (onSwap) onSwap(blockId()); };
  remove_.onClick = [this] { this->services().chain.removeBlock(blockId()); };

  addAndMakeVisible(ledSlot_);
  ledSlot_.addChildComponent(led_);
  ledSlot_.setSize(BlockLed::kSize, BlockLed::kSize);
  addMouseListener(&hover_, true);
  services.pointer.addListener(this);
  hostedEffectGraphics().addChangeListener(this);  // repaint when a slot graphic arrives

  setBlock(block);
  setHovered(false);
  resized();  // the base set the size before these children existed
}

ToneTile::~ToneTile() {
  hostedEffectGraphics().removeChangeListener(this);
  services().pointer.removeListener(this);
  removeMouseListener(&hover_);
}

void ToneTile::changeListenerCallback(juce::ChangeBroadcaster*) {
  // A hosted effect's screenshot just arrived (or was replaced): put it on the
  // glow path, then repaint the tile chrome around it.
  syncEffectGraphic();
  repaint();
}

void ToneTile::syncEffectGraphic() {
  if (!block_.isEffect) {
    image_.clearInjectedImage();
    return;
  }
  const auto graphic = hostedEffectGraphic(blockId());
  if (graphic.isValid())
    image_.setInjectedImage(graphic);
  else
    image_.clearInjectedImage();
}

void ToneTile::setBlock(const ChainItem& block) {
  block_ = block;
  enabled_ = block.params.enabled;
  setHelpText(help::toneTile(block.tone.title));
  setTitle(block.tone.title);
  image_.setTone(block.tone.image, block.tone.gear, block.tone.local, kGlyphSize);
  syncEffectGraphic();
  swap_.setVisible(!block.isEffect);
  syncState();
}

// A model download/prepare is in flight: `modelLoading` covers switches
// (the previous model keeps playing, so `loaded` stays true) and `!loaded`
// covers fresh blocks that have nothing to play yet.
void ToneTile::syncState() {
  const bool busy = block_.modelLoading || (!block_.loaded && !block_.loadFailed);
  const bool armed = dropArmed();
  // Hosted-effect art rides the same ToneImage as tone art so it composites
  // the energy glow; until a screenshot has been captured the tile paints the
  // gear/effect glyph itself.
  image_.setVisible(!armed && !block_.tone.local &&
                    (!block_.isEffect || image_.hasInjectedImage()));
  imageFade_.animateTo(enabled_ && !busy && !block_.loadFailed ? 1.0f : kDimmedImage, kImageFadeMs);
  dots_.setVisible(!armed && busy && !block_.loadFailed);
  retry_.setVisible(!armed && block_.loadFailed);
  chrome_.setVisible(!armed);
  ledSlot_.setVisible(!armed);
  power_.setOn(enabled_);
  repaint();
}

void ToneTile::togglePower() {
  enabled_ = !enabled_;
  power_.setOn(enabled_);
  imageFade_.animateTo(enabled_ ? 1.0f : kDimmedImage, kImageFadeMs);
  this->services().chain.setBlockParam(blockId(), "enabled", enabled_);
}

void ToneTile::open() {
  if (block_.isEffect) {
    if (!openHostedEffectEditor(services(), blockId(), block_.tone.title))
      services().toast.show("Effect is missing; reinstall it to restore this block");
    return;
  }
  if (onOpen) onOpen(blockId());
}

void ToneTile::dropArmedChanged(bool) { syncState(); }

void ToneTile::travellingChanged(bool) { setHovered(hovered_); }

void ToneTile::pointerMoved(const juce::MouseEvent& e, bool leaving) {
  // Leaving onto one of our own children is still hovering the tile.
  setHovered(!leaving || getLocalBounds().contains(e.getEventRelativeTo(this).getPosition()));
}

void ToneTile::setHovered(bool hovered) {
  hovered_ = hovered;
  const bool shown = services().pointer.coarse() || hovered_ || travelling();
  chrome_.setAlpha(shown ? 1.0f : 0.0f);
  chrome_.setInterceptsMouseClicks(false, shown);
}

void ToneTile::resized() {
  const auto box = getLocalBounds();
  image_.setBounds(box.withTrimmedTop(22).withTrimmedBottom(38).reduced(8, 0));
  dots_.setBounds(box.withSizeKeepingCentre(LoadingDots::kWidth, LoadingDots::kDot + 2 * LoadingDots::kMargin));
  retry_.setBounds(box.withSizeKeepingCentre(retry_.getWidth(), retry_.getHeight()));

  chrome_.setBounds(box.withHeight(kChromeHeight));
  const int y = kChromePad;
  power_.setBounds(kChromePad, y, theme::kIconBoxSize, theme::kIconBoxSize);
  remove_.setBounds(getWidth() - kChromePad - theme::kIconBoxSize, y, theme::kIconBoxSize,
                    theme::kIconBoxSize);
  swap_.setBounds(remove_.getX() - kChromeGap - theme::kIconBoxSize, y, theme::kIconBoxSize,
                  theme::kIconBoxSize);

  ledSlot_.setTopLeftPosition(getWidth() - kLedInset - BlockLed::kSize,
                              getHeight() - kLedInset - BlockLed::kSize);
}

void ToneTile::paint(juce::Graphics& g) {
 const auto box=getLocalBounds().toFloat();
 const auto accent=blockart::colour(block_.tone.gear,block_.isEffect);
 paint::fill(g,box,gallery::kTileCorner,theme::kSurface);
 paint::border(g,box,gallery::kTileCorner,enabled_ ? accent.withAlpha(0.7f) : theme::kBorder);
 if(dropArmed()) {
  Icons::draw(g,Icon::Upload,box.withSizeKeepingCentre(28,28),theme::kEffect);return;
 }
 g.setColour(accent.withAlpha(enabled_ ? 0.13f : 0.04f));
 g.fillRoundedRectangle(box.reduced(4),5.0f);
 g.setColour(enabled_ ? accent : theme::kGray);
 g.fillRoundedRectangle(10.0f,9.0f,4.0f,4.0f,2.0f);
 g.setFont(Fonts::mono(8));
 g.drawText(blockart::label(block_.tone.gear,block_.isEffect),18,5,getWidth()-24,12,
            juce::Justification::centredLeft,false);
 {
  juce::Graphics::ScopedSaveState save(g);
  g.setOpacity(enabled_ ? 1.0f : 0.35f);
  const auto effectGraphic = (block_.isEffect && !image_.hasInjectedImage())
                                 ? hostedEffectGraphic(blockId())
                                 : juce::Image();
  if (effectGraphic.isValid())
    g.drawImageWithin(effectGraphic, 8, 22, getWidth()-16, getHeight()-60, juce::RectanglePlacement::centred);
  else if (block_.isEffect || block_.tone.local)
    blockart::draw(g,box.withSizeKeepingCentre(40,40).translated(0,-4),block_.tone.gear,block_.isEffect);
 }
 g.setFont(Fonts::sans(10,true));g.setColour(enabled_ ? theme::kWhite : theme::kMuted);
 g.drawFittedText(block_.tone.title,getLocalBounds().withTrimmedTop(getHeight()-38).reduced(10,0),
                  juce::Justification::centred,2);
}

void ToneTile::paintOverChildren(juce::Graphics& g) {
  if (!block_.isEffect && !block_.tone.local && !dropArmed())
    Brand::drawMark(g, {8, static_cast<float>(getHeight()-12), 24, 8});
  if (dropArmed())
    paint::dashedBorder(g, getLocalBounds().toFloat(), gallery::kTileCorner,
                        gallery::kFileDropBorder, gallery::kAddTileBorderWidth);
}

}  // namespace t3k::ui
