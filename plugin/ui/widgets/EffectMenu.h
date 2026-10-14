// Add-effect browser: a two-column popover — vendors on the left, that vendor's
// plugins on the right. When a vendor is picked the plugin column opens and the
// vendor column is blurred (backdrop blur), so the "background" menu recedes.
// A persistent search field across the top filters the already-listed bundles
// into one global results list (name · vendor · format) — no scanning, no
// plug-in instantiation. Drawn with our own components (not a native PopupMenu)
// so we can blur it.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <memory>
#include <vector>

#include "EffectSearch.h"
#include "IconButton.h"
#include "MenuRow.h"
#include "Popover.h"

namespace t3k::ui {

class EffectMenu : public Popover, private juce::KeyListener {
public:
  // A hosted bundle: display metadata plus the exact bundle path as identity.
  using Entry = EffectEntry;

  explicit EffectMenu(std::vector<Entry> entries);
  ~EffectMenu() override;

  // Chosen bundle: path + display name (called before the menu dismisses).
  std::function<void(const juce::String& path, const juce::String& name)> onChoose;

  // Centre the panel over `anchor` (and keep it inside the overlay). Call after open().
  void centreOn(juce::Component& anchor);

  // Focus the search field (call after open()/centreOn()).
  void focusSearch();

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  // Paints a (blurred) snapshot of the vendor column over it.
  class Frost : public juce::Component {
  public:
    void setImage(juce::Image image);
    void paint(juce::Graphics& g) override;

  private:
    juce::Image image_;
  };

  void buildVendors();
  void buildPlugins(const juce::String& vendor);
  void pickVendor(const juce::String& vendor);
  void placeClamped(int x, int y);
  // Never taller than the overlay; applied on every size change so a mode or
  // vendor switch cannot undo centreOn()'s clamp.
  void clampHeightToOverlay();
  // Set the panel width, keeping it centred and clamped.
  void resizeKeepingCentre(int newWidth);

  // Search / view state.
  bool searchActive() const { return query_.trim().isNotEmpty(); }
  void applyQuery(const juce::String& text);  // rebuild + switch mode from a query
  void clearQuery();
  void buildResults();
  void syncMode();
  void focusFirstResult();
  void focusNextControl();

  // KeyListener on the search field: Down/Return leave the field, Escape and
  // everything else fall through to the editor and then the Popover.
  bool keyPressed(const juce::KeyPress& key, juce::Component* origin) override;

  std::vector<Entry> entries_;
  std::vector<MenuRow*> resultRows_;  // owned by resultsView_'s viewed component

  juce::TextEditor search_;
  std::unique_ptr<IconButton> clear_;
  juce::Viewport resultsView_;
  juce::Rectangle<int> searchBox_;  // paints the field's rounded box + magnifier

  juce::Component leftColumn_, rightColumn_;
  juce::Viewport leftView_, rightView_;
  Frost frost_;
  bool rightShown_ = false;
  juce::String query_;   // live search text
  juce::String vendor_;  // last chosen vendor, restored when the query clears

  static constexpr int kPad = 8;
  static constexpr int kLeftW = 210;
  static constexpr int kRightW = 290;
  static constexpr int kHeight = 420;
  static constexpr int kRowH = MenuRow::kContext.height;
  static constexpr int kScrollW = 8;
  static constexpr int kBlurRadius = 9;
  static constexpr int kSearchH = 32;
  static constexpr int kSearchPadL = 30;  // room for the magnifier
  static constexpr int kSearchPadR = 28;  // room for the clear button
  static constexpr int kHeaderGap = 8;
  static constexpr float kSearchCorner = 8.0f;
};

}  // namespace t3k::ui
