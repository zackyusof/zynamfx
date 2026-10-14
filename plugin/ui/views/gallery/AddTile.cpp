#include "AddTile.h"

#include "GalleryGeometry.h"
#include "core/Help.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "core/Fonts.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include <algorithm>
#include <map>

#include "widgets/HostedEffectWindow.h"

namespace t3k::ui {

AddTile::Routing AddTile::routingFor(int index, int count) {
  if (count <= 1) return Routing::none;
  if (index == 0) return Routing::right;
  if (index == count - 1) return Routing::left;
  return Routing::both;
}

AddTile::AddTile(Services& services, std::string blockId, int size)
    : GalleryTile(services, std::move(blockId), size) {
  setHelpText(help::text(help::Key::addTile));
  setTitle("Add Tone");
}

void AddTile::setRouting(Routing routing) {
  if (routing_ == routing) return;
  routing_ = routing;
  repaint();
}

void AddTile::setCanPaste(bool canPaste) { canPaste_ = canPaste; }

void AddTile::open() {
  if (onAdd) onAdd(blockId());
}

std::vector<ContextMenu::Item> AddTile::menuItems() {
  std::vector<ContextMenu::Item> items{
      {"Add AU/VST3 Effect", Icon::File, help::Key::loadFileTile,
       [this] { chooseInstalledEffect(); }},
      {"Paste", Icon::ClipboardPaste, help::Key::pasteBlock,
       [this] { if (onPaste) onPaste(blockId()); }, /*disabled=*/!canPaste_},
  };
  for (auto& item : localLoadItems()) items.push_back(std::move(item));
  return items;
}

void AddTile::chooseInstalledEffect() {
  // The bundles come from a plain directory listing (no scan/instantiation), so
  // this is instant. EffectMenu is our own two-column popover (vendors -> that
  // vendor's plugins) rather than a native PopupMenu, which lets us blur the
  // vendor column while the plugin column is active.
  const auto listed = services().backend.listHostedEffectBundles();
  const auto* array = listed.getArray();
  if (array == nullptr || array->isEmpty()) {
    services().toast.show("No AU/VST3 effects found in /Library/Audio/Plug-Ins");
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
  Services* svc = &services();
  const std::string target = blockId();
  effectMenu_->onChoose = [svc, target](const juce::String& path, const juce::String& name) {
    juce::MessageManager::callAsync([svc, target, path, name] {
      const auto id = svc->backend.addHostedEffectFromFile(juce::File(path), target);
      if (id.empty()) {
        svc->toast.show("Could not load this effect");
        return;
      }
      svc->chain.refresh(true);
      // The effect block's detail embeds the plugin's GUI (open the tile to see it).
      juce::ignoreUnused(name);
    });
  };
  effectMenu_->open(*this, Popover::Align::left, 8);
  effectMenu_->centreOn(*this);  // centre over the tile and keep inside the window
  effectMenu_->focusSearch();    // the search field takes the keyboard on open
}

void AddTile::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  const bool armed = dropArmed();
  paint::fill(g, box, gallery::kTileCorner, theme::kSurfaceRaised);
  paint::dashedBorder(g, box, gallery::kTileCorner,
                      armed ? gallery::kFileDropBorder : gallery::kAddTileBorder,
                      gallery::kAddTileBorderWidth);

  if (armed) {
    const float s = gallery::kFileDropGlyphSize;
    Icons::draw(g, Icon::Upload, box.withSizeKeepingCentre(s, s), theme::kGray);
    return;
  }

  const int size = tileSize();
  const int icon = gallery::plusIconSize(size);
  // Routing lines are anchored inside the border (absolute children position
  // against the padding box), so the run to the plus ring is a border-width
  // shorter than measured from the tile's outer edge.
  if (!travelling() && routing_ != Routing::none) {
    const float run = size / 2.0f - icon / 2.0f + gallery::plusCircleInset(icon) -
                      gallery::kAddTileBorderWidth;
    const float y = box.getCentreY() - gallery::kLineWidth / 2;
    g.setColour(theme::kWhite);
    if (routing_ == Routing::left || routing_ == Routing::both)
      g.fillRect(juce::Rectangle<float>(gallery::kAddTileBorderWidth, y, run, gallery::kLineWidth));
    if (routing_ == Routing::right || routing_ == Routing::both)
      g.fillRect(juce::Rectangle<float>(box.getRight() - gallery::kAddTileBorderWidth - run, y, run,
                                        gallery::kLineWidth));
  }
  Icons::draw(g, Icon::PlusCircle, box.withSizeKeepingCentre(icon, icon), theme::kEffect,
              /*strokeWidth=*/1.0f);
  g.setFont(Fonts::mono(9));g.setColour(theme::kMuted);
  g.drawText("ADD BLOCK",8,getHeight()-29,getWidth()-16,16,juce::Justification::centred,false);
}

}  // namespace t3k::ui
