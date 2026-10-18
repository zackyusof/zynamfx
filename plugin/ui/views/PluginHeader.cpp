#include "PluginHeader.h"

#include "core/Brand.h"
#include "core/BuildIdentity.h"
#include "core/Fonts.h"
#include "core/CustomIcons.h"
#include "core/Design.h"
#include "core/Help.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "widgets/Clickable.h"

namespace t3k::ui {

namespace {
constexpr int kPadX = 24;
constexpr int kGroupGap = 40;  // between header items
constexpr int kPairGap = 16;   // tight pairs (undo/redo)
constexpr int kLogoWidth = 176;  // wordmark + ZY monogram + the version string
constexpr int kLogoHeight = 24;  // 160 * 32 / 210, rounded like the browser
}  // namespace

// The wordmark links to the public project repository (see core/BuildIdentity.h).
class PluginHeader::LogoLink : public Clickable {
public:
  LogoLink() : Clickable({}) {
    setTitle("zynamfx");
    const auto channel = build::channel();
    setHelpText((channel == "public" ? juce::String("zynamfx (public build) - open ")
                                     : juce::String("zynamfx (development build) - open ")) +
                build::repoUrl());
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    onClick = [] { juce::URL(build::repoUrl()).launchInDefaultBrowser(); };
  }
  void paintButton(juce::Graphics& g, bool, bool) override {
    const auto bounds = getLocalBounds().toFloat();
    Brand::drawLogo(g, bounds.withWidth(84.0f));
    Brand::drawZyMark(g, {90.0f, 2.0f, 36.0f, 20.0f});
    // The build's version, discretely, beside the ZY monogram (see
    // BuildIdentity.h). The ZY mark ends at x=126; nothing else sits here.
    const auto version = build::displayVersion();
    if (version.isNotEmpty()) {
      g.setFont(Fonts::mono(9.0f));
      g.setColour(theme::kMuted);
      g.drawText("v" + version,
                 juce::Rectangle<int>(128, 0, getWidth() - 128, getHeight()),
                 juce::Justification::centredLeft, false);
    }
  }
};

PluginHeader::PluginHeader(Services& services)
    : services_(services),
      logo_(std::make_unique<LogoLink>()),
      presetBar_(services),
      jam_(custom_icons::kMetronome, 28, 18),
      tuner_(custom_icons::kTuningFork, 28, 18) {
  addAndMakeVisible(*logo_);
  addAndMakeVisible(presetBar_);

  stereo_.onToggle = [this](bool stereo) {
    if (onStereoToggle) onStereoToggle(stereo);
  };
  addAndMakeVisible(stereo_);

  jam_.setHelpText("Metronome & Jam Track Player");
  jam_.setFillWhenActive(true);
  jam_.onClick = [this] {
    if (onToggleJam) onToggleJam(!jamShown_);
  };
  addAndMakeVisible(jam_);

  tuner_.setHelpText(help::text(help::Key::tuner));
  tuner_.setFillWhenActive(true);
  tuner_.onClick = [this] {
    if (onToggleTuner) onToggleTuner(!tunerShown_);
  };
  addAndMakeVisible(tuner_);

  undo_.setHelpText(help::text(help::Key::undo));
  undo_.onClick = [this] {
    if (onUndo) onUndo();
  };
  redo_.setHelpText(help::text(help::Key::redo));
  redo_.onClick = [this] {
    if (onRedo) onRedo();
  };
  addAndMakeVisible(undo_);
  addAndMakeVisible(redo_);

  account_.onOpenSettings = [this] {
    if (onOpenSettings) onOpenSettings();
  };
  account_.onLogin = [this] {
    if (onLogin) onLogin();
  };
  account_.onLogout = [this] {
    if (onLogout) onLogout();
  };
  addAndMakeVisible(account_);

  services_.chain.addListener(this);
  services_.session.addListener(this);
  sessionChanged();
  chainChanged(services_.chain.state());
  setTunerShown(false);
  setJamShown(false);
  setSize(design::kWidth, kHeight);
}

PluginHeader::~PluginHeader() {
  services_.session.removeListener(this);
  services_.chain.removeListener(this);
}

void PluginHeader::sessionChanged() {
  const auto& session = services_.session;
  account_.setAuthenticated(session.authenticated());
  account_.setLocalOnly(services_.backend.localOnly());
  const auto user = session.user();
  const auto url = user ? user->avatarUrl : juce::String();
  if (url == avatarUrl_ && (url.isNotEmpty() || !session.authenticated())) return;
  avatarUrl_ = url;
  account_.setAvatar(services_.images, url);
}

void PluginHeader::setTunerShown(bool shown) {
  tunerShown_ = shown;
  // Lit + HIGHLIGHT fill while the tuner is up; the icon itself stays white.
  tuner_.setActive(true);
  tuner_.setFillWhenActive(shown);
}

void PluginHeader::setJamShown(bool shown) {
  jamShown_ = shown;
  jam_.setActive(true);
  jam_.setFillWhenActive(shown);
}

void PluginHeader::chainChanged(const ChainState& state) {
  undo_.setEnabled(state.canUndo);
  redo_.setEnabled(state.canRedo);
  stereo_.setStereoEnabled(state.stereoEnabled);
}

void PluginHeader::paint(juce::Graphics& g) {
  g.setGradientFill(juce::ColourGradient::vertical(theme::kSurfaceRaised,0,theme::kSurface,static_cast<float>(getHeight())));
  g.fillAll();
  g.setColour(theme::kAccent);
  g.fillRect(0,getHeight()-2,getWidth(),2);
  g.setFont(Fonts::tracked(Fonts::mono(8),0.12f));g.setColour(theme::kMuted);
  g.drawText("Powered by TONE3000",24,45,200,12,juce::Justification::centredLeft,false);
  const auto presetBox=presetBar_.getBounds().expanded(8,5).toFloat();
  paint::fill(g,presetBox,4.0f,theme::kBlack);
  paint::border(g,presetBox,4.0f,theme::kBorder);
  paint::hairlineH(g, 0, static_cast<float>(getWidth()), static_cast<float>(getHeight() - 1),
                   theme::kBorder);
}

void PluginHeader::resized() {
  // The 1px bottom border is inside the 64px box; items centre in the 63px
  // content row like the flex container did.
  auto area = getLocalBounds().withTrimmedBottom(1).reduced(kPadX, 0);
  // 31.5 in the browser; rounding up puts even-height boxes where the
  // browser's anti-aliased edges land.
  const int cy = (area.getHeight() + 1) / 2;
  auto place = [&](juce::Component& c, int right) {
    c.setBounds(right - c.getWidth(), cy - c.getHeight() / 2, c.getWidth(), c.getHeight());
    return c.getX();
  };

  logo_->setBounds(area.getX(), 14, kLogoWidth, kLogoHeight);

  // Right group, laid out from the right edge: account · undo/redo · tuner ·
  // stereo · presets, 40px apart (16px inside the undo/redo pair).
  int x = place(account_, area.getRight()) - kGroupGap;
  x = place(redo_, x) - kPairGap;
  x = place(undo_, x) - kGroupGap;
  x = place(tuner_, x) - kGroupGap;
  x = place(jam_, x) - kGroupGap;
  x = place(stereo_, x) - kGroupGap;
  place(presetBar_, x);
}

}  // namespace t3k::ui
