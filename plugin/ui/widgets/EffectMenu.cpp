#include "EffectMenu.h"

#include <algorithm>
#include <iterator>

#include "core/Blur.h"
#include "core/Fonts.h"
#include "core/Icons.h"
#include "core/Paint.h"
#include "core/Theme.h"

namespace t3k::ui {

void EffectMenu::Frost::setImage(juce::Image image) {
  image_ = std::move(image);
  repaint();
}

void EffectMenu::Frost::paint(juce::Graphics& g) {
  if (image_.isValid())
    g.drawImageAt(image_, 0, 0);
}

EffectMenu::EffectMenu(std::vector<Entry> entries) : entries_(std::move(entries)) {
  setTitle("Add effect");

  // Search field across the top. The editor is chrome-less: paint() draws its
  // rounded box and magnifier, so the field matches the design and stays
  // outside the frost. Added before the columns so it is the first Tab stop.
  search_.setMultiLine(false);
  search_.setReturnKeyStartsNewLine(false);
  search_.setScrollbarsShown(false);
  search_.setPopupMenuEnabled(false);
  search_.setBorder({});
  search_.setIndents(0, 0);
  search_.setJustification(juce::Justification::centredLeft);
  search_.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
  search_.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
  search_.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
  search_.setColour(juce::TextEditor::shadowColourId, juce::Colours::transparentBlack);
  search_.setColour(juce::TextEditor::textColourId, theme::kWhite);
  search_.setColour(juce::TextEditor::highlightColourId, theme::kHighlight);
  search_.setColour(juce::TextEditor::highlightedTextColourId, theme::kWhite);
  search_.setColour(juce::CaretComponent::caretColourId, theme::kWhite);
  search_.setMouseCursor(juce::MouseCursor::IBeamCursor);
  search_.setTextToShowWhenEmpty(juce::String::fromUTF8("Search effects or vendors\xe2\x80\xa6"),
                                 theme::kGray);
  search_.setTitle(juce::String::fromUTF8("Search effects or vendors"));  // screen-reader name
  // Escape must reach the Popover's own handling so the chooser dismisses as
  // everywhere else; Return/Down/Up are taken by our KeyListener below.
  search_.setEscapeAndReturnKeysConsumed(false);
  search_.addKeyListener(this);
  search_.onTextChange = [this] { applyQuery(search_.getText()); };
  addAndMakeVisible(search_);

  clear_ = std::make_unique<IconButton>(Icon::X, 18, 14);
  clear_->setActive(false);
  clear_->setHelpText("Clear search");
  clear_->onClick = [this] { clearQuery(); };
  addChildComponent(*clear_);

  resultsView_.setScrollBarsShown(true, false);
  resultsView_.setScrollBarThickness(kScrollW);
  addChildComponent(resultsView_);

  leftColumn_.addAndMakeVisible(leftView_);
  rightColumn_.addAndMakeVisible(rightView_);
  addAndMakeVisible(leftColumn_);
  addChildComponent(rightColumn_);
  addChildComponent(frost_);
  frost_.setInterceptsMouseClicks(false, false);

  leftView_.setScrollBarsShown(true, false);
  leftView_.setScrollBarThickness(kScrollW);
  rightView_.setScrollBarsShown(true, false);
  rightView_.setScrollBarThickness(kScrollW);

  buildVendors();
  syncMode();
  setSize(2 * (kBorder + kPad) + kLeftW, 2 * (kBorder + kPad) + kHeight);
}

EffectMenu::~EffectMenu() = default;

void EffectMenu::centreOn(juce::Component& anchor) {
  auto* overlay = getParentComponent();
  if (overlay == nullptr)
    return;
  // Never taller than the overlay (the panel is taller than most plugin windows).
  clampHeightToOverlay();
  const auto a = overlay->getLocalArea(&anchor, anchor.getLocalBounds().toFloat());
  placeClamped(juce::roundToInt(a.getCentreX()) - getWidth() / 2,
               juce::roundToInt(a.getBottom()) + 8);
}

void EffectMenu::clampHeightToOverlay() {
  auto* overlay = getParentComponent();
  if (overlay == nullptr)
    return;
  const int maxH = juce::jmax(160, overlay->getHeight() - 24);
  if (getHeight() > maxH)
    setSize(getWidth(), maxH);
}

void EffectMenu::resizeKeepingCentre(int newWidth) {
  if (getParentComponent() == nullptr) {  // not shown yet: just take the size
    setSize(newWidth, getHeight());
    clampHeightToOverlay();
    return;
  }
  const int centreX = getX() + getWidth() / 2;
  setSize(newWidth, getHeight());
  clampHeightToOverlay();
  placeClamped(centreX - getWidth() / 2, getY());
}

void EffectMenu::placeClamped(int x, int y) {
  auto* overlay = getParentComponent();
  if (overlay == nullptr)
    return;
  const int maxX = juce::jmax(0, overlay->getWidth() - getWidth());
  const int maxY = juce::jmax(0, overlay->getHeight() - getHeight());
  setTopLeftPosition(juce::jlimit(0, maxX, x), juce::jlimit(0, maxY, y));
}

void EffectMenu::focusSearch() {
  if (!search_.isShowing())  // offscreen (testbed capture) has no peer
    return;
  search_.grabKeyboardFocus();
  search_.selectAll();
}

void EffectMenu::paint(juce::Graphics& g) {
  const auto box = getLocalBounds().toFloat();
  paint::fill(g, box, theme::kPanelCorner, theme::kPanelBg);
  paint::border(g, box, theme::kPanelCorner, theme::kBorder);

  if (searchBox_.isEmpty())
    return;
  // The search field's own rounded box + leading magnifier. Kept above the
  // columns and outside the frost, so it never blurs.
  paint::fill(g, searchBox_.toFloat(), kSearchCorner, theme::kSurface);
  paint::border(g, searchBox_.toFloat(), kSearchCorner, theme::kBorder);
  const float icon = 14.0f;
  Icons::draw(g, Icon::Search,
              juce::Rectangle<float>(icon, icon)
                  .withCentre({static_cast<float>(searchBox_.getX() + kSearchPadL / 2),
                               static_cast<float>(searchBox_.getCentreY())}),
              theme::kGray);
}

void EffectMenu::resized() {
  auto r = contentBounds().reduced(kPad);

  // Persistent search field across the top: outside the frost and above both
  // columns, so it stays visible and unblurred in every mode.
  searchBox_ = r.removeFromTop(kSearchH);
  auto editorBox = searchBox_.withTrimmedLeft(kSearchPadL).withTrimmedRight(kSearchPadR);
  search_.setBounds(editorBox.withSizeKeepingCentre(editorBox.getWidth(),
                                                    juce::jmin(editorBox.getHeight(), 22)));
  if (clear_ != nullptr)
    clear_->setCentrePosition(searchBox_.getRight() - kSearchPadR / 2, searchBox_.getCentreY());

  r.removeFromTop(kHeaderGap);
  const int fullH = r.getHeight();
  if (searchActive()) {
    // One global, scrollable results list across all vendors.
    resultsView_.setBounds(r.removeFromLeft(kLeftW + kRightW));
  } else {
    leftColumn_.setBounds(r.removeFromLeft(kLeftW).withHeight(fullH));
    if (rightShown_)
      rightColumn_.setBounds(r.removeFromLeft(kRightW).withHeight(fullH));
    leftView_.setBounds(leftColumn_.getLocalBounds());
    rightView_.setBounds(rightColumn_.getLocalBounds());
    frost_.setBounds(leftColumn_.getBounds());
  }
}

void EffectMenu::buildVendors() {
  juce::StringArray vendors;
  for (const auto& e : entries_)
    if (!vendors.contains(e.vendor))
      vendors.add(e.vendor);
  vendors.sort(true);

  auto* content = new juce::Component();
  const int w = kLeftW - kScrollW;
  content->setSize(w, juce::jmax(kRowH, vendors.size() * kRowH));
  int y = 0;
  for (const auto& vendor : vendors) {
    auto* row = new MenuRow(vendor, std::nullopt, MenuRow::kContext);
    row->setLabelColour(theme::kWhite);
    row->setBounds(0, y, w, kRowH);
    row->onClick = [this, vendor] { pickVendor(vendor); };
    content->addAndMakeVisible(row);
    y += kRowH;
  }
  leftView_.setViewedComponent(content, true);
  leftView_.setViewPosition(0, 0);
}

void EffectMenu::buildPlugins(const juce::String& vendor) {
  std::vector<const Entry*> items;
  for (const auto& e : entries_)
    if (e.vendor == vendor)
      items.push_back(&e);
  std::sort(items.begin(), items.end(), [](const Entry* a, const Entry* b) {
    return a->name.compareIgnoreCase(b->name) < 0;
  });

  auto* content = new juce::Component();
  const int w = kRightW - kScrollW;
  content->setSize(w, juce::jmax(kRowH, static_cast<int>(items.size()) * kRowH));
  int y = 0;
  for (size_t i = 0; i < items.size(); ++i) {
    const auto* entry = items[i];
    juce::String label = entry->name;
    for (size_t j = 0; j < items.size(); ++j)
      if (j != i && items[j]->name == entry->name) {
        label << " (" << entry->format << ")";
        break;
      }
    auto* row = new MenuRow(label, std::nullopt, MenuRow::kContext);
    row->setLabelColour(theme::kWhite);
    row->setBounds(0, y, w, kRowH);
    const auto path = entry->path;
    const auto name = entry->name;
    row->onClick = [this, path, name] {
      // Close the menu before handing off; the handler defers the actual work so
      // nothing tears this menu (or the tiles) down while the button is mid-click.
      dismiss();
      if (onChoose)
        onChoose(path, name);
    };
    content->addAndMakeVisible(row);
    y += kRowH;
  }
  rightView_.setViewedComponent(content, true);
  rightView_.setViewPosition(0, 0);
}

void EffectMenu::pickVendor(const juce::String& vendor) {
  vendor_ = vendor;
  buildPlugins(vendor);
  rightShown_ = true;
  const int centreX = getX() + getWidth() / 2;  // keep the panel centred as it widens
  // Widen, but keep the (possibly clamped) height: resetting it to kHeight here
  // is exactly what used to undo centreOn()'s overlay clamp.
  setSize(2 * (kBorder + kPad) + kLeftW + kRightW, getHeight());
  clampHeightToOverlay();
  rightColumn_.setVisible(true);

  // Blur the (now in the background) vendor column. Snapshot before revealing the
  // plugin column so the blur does not capture the plugin rows.
  auto snapshot = leftColumn_.createComponentSnapshot(leftColumn_.getLocalBounds());
  blurImage(snapshot, kBlurRadius);
  frost_.setImage(std::move(snapshot));
  frost_.setVisible(true);
  frost_.toFront(false);
  rightColumn_.toFront(false);

  placeClamped(centreX - getWidth() / 2, getY());
}

// --- Search mode ----------------------------------------------------------

void EffectMenu::applyQuery(const juce::String& text) {
  query_ = text;
  if (searchActive())
    buildResults();
  syncMode();
  // Search results are one wide list; clearing restores the vendor layout
  // (plugin column included, when a vendor was already chosen).
  const int w = searchActive() ? 2 * (kBorder + kPad) + kLeftW + kRightW
                               : 2 * (kBorder + kPad) + kLeftW + (rightShown_ ? kRightW : 0);
  resizeKeepingCentre(w);
}

void EffectMenu::clearQuery() {
  search_.setText({}, /*sendChangeMessage=*/false);
  applyQuery({});
  focusSearch();  // the press dropped the field's focus; clearing is a prelude to retyping
}

void EffectMenu::syncMode() {
  const bool searching = searchActive();
  leftColumn_.setVisible(!searching);
  rightColumn_.setVisible(!searching && rightShown_);
  frost_.setVisible(!searching && rightShown_);
  resultsView_.setVisible(searching);
  if (clear_ != nullptr)
    clear_->setVisible(searching);
  if (searching)
    resultsView_.toFront(false);  // the results replace the (hidden) columns
}

void EffectMenu::buildResults() {
  const auto matches = searchEffectEntries(entries_, query_);
  const int w = kLeftW + kRightW - kScrollW;

  auto* content = new juce::Component();
  content->setSize(w, juce::jmax(kRowH, static_cast<int>(matches.size()) * kRowH));
  resultRows_.clear();

  int y = 0;
  if (matches.empty()) {
    // Zero matches: a plain, non-focusable label so the list still paints the
    // message without becoming a Tab stop.
    auto* label = new juce::Label({}, "No matching effects");
    label->setColour(juce::Label::textColourId, theme::kMuted);
    label->setFont(Fonts::sans(13.0f));
    label->setBorderSize({0, MenuRow::kContext.padX, 0, MenuRow::kContext.padX});
    label->setBounds(0, 0, w, kRowH);
    label->setInterceptsMouseClicks(false, false);
    content->addAndMakeVisible(label);
  } else {
    const auto dot = juce::String::fromUTF8("\xc2\xb7");
    for (size_t i = 0; i < matches.size(); ++i) {
      const auto& e = entries_[matches[i]];
      // Name · vendor (format): vendor + format make duplicate names unambiguous.
      const juce::String label =
          e.name + "  " + dot + "  " + e.vendor + " (" + e.format + ")";
      auto* row = new MenuRow(label, std::nullopt, MenuRow::kContext);
      row->setLabelColour(theme::kWhite);
      row->setBounds(0, y, w, kRowH);
      const auto path = e.path;  // exact bundle path is the identity
      const auto name = e.name;
      row->onClick = [this, path, name] {
        // Same handoff as the plugin rows. Rows are only rebuilt on a query
        // edit, never from inside this callback.
        dismiss();
        if (onChoose)
          onChoose(path, name);
      };
      content->addAndMakeVisible(row);
      resultRows_.push_back(row);
      y += kRowH;
    }
  }
  resultsView_.setViewedComponent(content, /*deleteComponentWhenNoLongerNeeded=*/true);
  resultsView_.setViewPosition(0, 0);  // every query edit starts at the top
}

void EffectMenu::focusFirstResult() {
  for (auto* row : resultRows_)
    if (row->isVisible()) {
      row->grabKeyboardFocus();
      resultsView_.setViewPosition(0, 0);  // the first row is already at the top
      return;
    }
}

void EffectMenu::focusNextControl() {
  const auto comps = juce::KeyboardFocusTraverser().getAllComponents(this);
  if (comps.empty())
    return;
  const auto at = std::find(comps.begin(), comps.end(), getCurrentlyFocusedComponent());
  juce::Component* next = (at == comps.end() || std::next(at) == comps.end())
                             ? comps.front()
                             : *std::next(at);
  if (next != nullptr)
    next->grabKeyboardFocus();
}

bool EffectMenu::keyPressed(const juce::KeyPress& key, juce::Component* origin) {
  if (origin != &search_)
    return false;
  if (key == juce::KeyPress::downKey || key == juce::KeyPress::returnKey) {
    // Down (or Return) leaves the field for the first result; with no query it
    // steps to the first visible row below the field.
    if (searchActive())
      focusFirstResult();
    else
      focusNextControl();
    return true;
  }
  // Escape and everything else fall through: the editor returns them (see
  // setEscapeAndReturnKeysConsumed above) and the Popover's own key handling
  // dismisses on Escape / walks the rows on Tab, unchanged.
  return false;
}

}  // namespace t3k::ui
