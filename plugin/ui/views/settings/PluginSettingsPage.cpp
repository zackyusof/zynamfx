#include "PluginSettingsPage.h"

#include "services/ForkKeyStore.h"  // fork-only: bring-your-own API key
#include "InlineChrome.h"
#include "core/BuildIdentity.h"
#include "core/Fonts.h"
#include "core/Paint.h"
#include "core/Theme.h"
#include "widgets/TextField.h"

namespace t3k::ui {

namespace {
// External docs: how to measure your rig's calibration levels.
const char* const kCalibrationDocsUrl =
    "https://neural-amp-modeler.readthedocs.io/en/latest/tutorials/calibration.html";

// Oversampling rate choices: the osFactor parameter's choice indices; the
// DSP maps index i to 2^(i+1).
const std::vector<SelectField::Option> kOsFactorOptions = {
    {"0", "2X - Default", {}}, {"1", "4X", {}}, {"2", "8X", {}}};

RichText copy(std::initializer_list<TextRun> runs) { return RichText(runs); }
}  // namespace

// DbuField
// The calibration level: an outlined number field with a bold grey "dBu"
// suffix. Edits are a draft while focused (committing every keystroke would
// reformat the value under the cursor); blur or Enter commits.
class PluginSettingsPage::DbuField : public FormItem {
public:
  static constexpr int kSuffixPad = 52;
  static constexpr float kSuffixPx = 14;

  // `onCommit` runs after a value is written to the parameter.
  DbuField(ParamBinding& param, std::function<void()> onCommit) : param_(param), onCommit_(std::move(onCommit)) {
    field_.setBackground(juce::Colours::transparentBlack);
    field_.setBorder(form::kFieldBorder);
    field_.setCornerRadius(form::kFieldRadius);
    field_.setFontSize(form::kFieldPx);
    field_.setPadding(form::kFieldPadY, form::kFieldPadX, kSuffixPad);
    field_.setPlaceholder("Value");
    field_.setName("Calibration level");
    field_.onEnter = [this] { commit(); };
    field_.onBlur = [this] { commit(); };
    addAndMakeVisible(field_);
    sync();
  }

  // Reflect the parameter unless the user is mid-edit.
  void sync() {
    if (field_.hasFocus()) return;
    field_.setText(juce::String(value(), 1));
  }

  float heightFor(float) const override { return static_cast<float>(form::fieldHeight()); }

  void paint(juce::Graphics& g) override {
    const auto font = Fonts::sans(kSuffixPx, true);
    const float lineH = static_cast<float>(Fonts::normalLineHeight(kSuffixPx));
    const float w = Fonts::width(font, "dBu");
    paint::cssLine(g, "dBu", getWidth() - form::kFieldPadX - w, std::round((getHeight() - lineH) / 2), lineH,
                   w + 2, font, theme::kGray);
  }

  void resized() override { field_.setBounds(getLocalBounds()); }

private:
  float value() const { return param_.normalised() * (kDbuMax - kDbuMin) + kDbuMin; }

  void commit() {
    const auto text = field_.text().trim();
    if (text.isNotEmpty() && text.containsOnly("-+.0123456789")) {
      const float normalised = (text.getFloatValue() - kDbuMin) / (kDbuMax - kDbuMin);
      param_.set(juce::jlimit(0.0f, 1.0f, normalised));
      if (onCommit_) onCommit_();
    }
    field_.setText(juce::String(value(), 1));
  }

  ParamBinding& param_;
  std::function<void()> onCommit_;
  TextField field_;
};

// ApiKeyField (fork-only)
// The user's own TONE3000 publishable key: a plain text field with the same
// draft-while-focused commit semantics as DbuField (Enter or blur commits to
// prefs; ForkKeyStore drops stale tokens when the value changes).
class PluginSettingsPage::ApiKeyField : public FormItem {
public:
  ApiKeyField(UiPrefs& prefs, std::function<void()> onCommit)
      : prefs_(prefs), onCommit_(std::move(onCommit)) {
    field_.setBackground(juce::Colours::transparentBlack);
    field_.setBorder(form::kFieldBorder);
    field_.setCornerRadius(form::kFieldRadius);
    field_.setFontSize(form::kFieldPx);
    field_.setPadding(form::kFieldPadY, form::kFieldPadX, form::kFieldPadX);
    field_.setPlaceholder("t3k_pub_...");
    field_.setName("TONE3000 API key");
    field_.onEnter = [this] { commit(); };
    field_.onBlur = [this] { commit(); };
    error_.setFont(Fonts::sans(11.0f));
    error_.setColour(juce::Label::textColourId, juce::Colour(0xffe5484d));
    error_.setJustificationType(juce::Justification::centredLeft);
    error_.setInterceptsMouseClicks(false, false);
    addChildComponent(error_);
    addAndMakeVisible(field_);
    sync();
  }

  // Reflect prefs unless the user is mid-edit.
  void sync() {
    if (field_.hasFocus()) return;
    field_.setText(fork::customPublishableKey(prefs_));
  }

  float heightFor(float) const override {
    // The inline error sits under the field; hidden it takes no space and the form re-flows.
    return static_cast<float>(form::fieldHeight() + (error_.isVisible() ? kErrorHeight : 0));
  }

  void resized() override {
    auto area = getLocalBounds();
    field_.setBounds(error_.isVisible() ? area.removeFromTop(form::fieldHeight()) : area);
    if (error_.isVisible()) error_.setBounds(area.removeFromTop(kErrorHeight));
  }

private:
  static constexpr int kErrorHeight = 18;

  // A key is either usable or the user is told why. The setter silently drops t3k_cs_ (server)
  // secrets, which reads as "the field rejects my key" — the report in issue #1.
  void commit() {
    const auto text = field_.text().trim();
    juce::String error;
    if (text.startsWithIgnoreCase("t3k_cs_"))
      error = "That is a secret key (t3k_cs_). Use the publishable key (t3k_pub_).";
    else if (text.isNotEmpty() && !text.startsWithIgnoreCase("t3k_pub_"))
      error = "Publishable keys start with t3k_pub_.";
    else
      fork::setCustomPublishableKey(prefs_, text);

    const bool wasVisible = error_.isVisible();
    error_.setText(error, juce::dontSendNotification);
    error_.setVisible(error.isNotEmpty());
    if (wasVisible != error_.isVisible()) {
      resized();
      heightChanged();
    }
    if (onCommit_) onCommit_();
    if (error.isEmpty()) sync();  // keep the user's text on screen while they fix it
  }

  UiPrefs& prefs_;
  std::function<void()> onCommit_;
  TextField field_;
  juce::Label error_;
};

// PluginSettingsPage
PluginSettingsPage::PluginSettingsPage(Services& services)
    : FormStack(form::kSectionGap),
      services_(services),
      infoBar_("Show Info Bar",
               "Shows help text on hover explaining what elements do, plus keyboard shortcuts and a CPU meter."),
      showGate_("Show Gate", "Reduces unwanted noise like hum and hiss while you're playing."),
      showPitch_("Show Pitch Shift",
                 "Transpose your instrument up or down without retuning or use a MIDI expression pedal for "
                 "whammy-style effects."),
      namSize_("NAM A2 Size",
               "Default size for new NAM blocks. Existing blocks keep their own, so presets load as saved."),
      lite_("A2-Lite", "Sounds great and uses less CPU"),
      full_("A2-Full", "Maximum accuracy model"),
      blockSize_("Per-Block NAM Size",
                 copy({TextRun::plain("Adds a "), TextRun::inlineBox(inline_chrome::liteFullChip()),
                       TextRun::plain(" toggle to every NAM block. When off, a block only shows its size if it "
                                      "differs from your default.")})),
      blockSizeTip_(copy({TextRun::plain("Look for "), TextRun::inlineBox(inline_chrome::liteFullChip()),
                          TextRun::plain(" next to each block's power button.")})),
      normalize_("Per-Block Normalization",
                 "Each block has normalization enabled, which levels output for consistent volume across signal "
                 "blocks. Turning this on reveals an optional control that lets you disable normalization per block."),
      normalizeTip_(copy({TextRun::plain("Normalization is now controlled per block. Look for the "),
                          TextRun::inlineBox(inline_chrome::icon(Icon::Equal)),
                          TextRun::plain(" icon on each block, enabled by default.")})),
      calibrateParam_(services.backend, "calibrateInput"),
      dbuParam_(services.backend, "inputCalibrationLevel"),
      osEnabledParam_(services.backend, "osEnabled"),
      osFactorParam_(services.backend, "osFactor"),
      calibration_("Calibration",
                   "Matches your input level to the capture's original recording level, for accurate gain staging."),
      dbu_(std::make_unique<DbuField>(dbuParam_, [this] { rememberMachineDefault(dbuParam_); })),
      calibrationHelp_(copy({TextRun::plain("Set the dBu level that matches your DAW's max digital level. Typical "
                                            "values: +12 dBu (professional gear), +4 dBu (semi-pro). "),
                             TextRun::link("Learn More", kCalibrationDocsUrl, theme::kLinkBlue)})),
      calibrationTip_(copy({TextRun::plain("Captures that include calibration data show a "),
                            TextRun::inlineBox(inline_chrome::icon(Icon::Gauge)),
                            TextRun::plain(juce::String::fromUTF8(
                                " icon on their block and it\xe2\x80\x99s enabled by default."))})),
      calibrationHandoff_("When one calibrated NAM feeds another, output calibration data also sets the handoff "
                          "level between them."),
      oversampling_("Oversampling", "Reduces aliasing. Higher rates improve quality but use more CPU."),
      rateLabel_("Rate", form::kBodyPx, false, theme::kMuted),
      multiCore_("Multi-Core Processing",
                 "Spreads the work across CPU cores for more headroom: stereo chains process in parallel, and "
                 "oversampled NAM models split across cores. Doesn't change the sound."),
      midi_("MIDI Mapping",
            "Control the plugin from pedals and knobs. Mappings are saved with the plugin and work in your DAW too."),
      midiSection_(services),
      apiKey_("TONE3000 API key",
              "Your TONE3000 publishable key (client_id). Never paste a secret key here."),
      apiKeyField_(std::make_unique<ApiKeyField>(services_.prefs, [this] { syncApiKey(); })),
      apiKeyHelp_(copy({TextRun::plain("Sign-in and catalog downloads run under this key. Get a free one at "),
                        TextRun::link("Get a free API key", "https://www.tone3000.com/settings",
                                      theme::kLinkBlue),
                        TextRun::plain(" (sign in first), paste it above, then reopen this window. Leave empty "
                                       "to use the key baked into this build, if it has one.")})),
      apiKeyStatus_(juce::String(), form::kSmallPx, theme::kSubtle),
      presets_("Presets",
               "Your saved presets are files in a folder you can back up, share, or copy to another computer."),
      openPresets_("Open presets folder", FormButton::text(form::kBodyPx, false, theme::kLinkBlue)),
      openPresetsBox_(openPresets_, static_cast<float>(openPresets_.preferredHeight())),
      diagnostics_("Diagnostics", "Copy recent diagnostic logs to the clipboard and paste them into a bug report."),
      copyLogs_("Copy Logs", FormButton::cta()),
      revealLogs_("Reveal log file on disk", FormButton::text(form::kBodyPx, false, theme::kLinkBlue)),
      validateAus_("Validate installed Audio Units", FormButton::text(form::kBodyPx, false, theme::kLinkBlue)),
      revealValidation_("Reveal validation report", FormButton::text(form::kBodyPx, false, theme::kLinkBlue)),
      copyLogsBox_(copyLogs_, static_cast<float>(copyLogs_.preferredHeight())),
      revealLogsBox_(revealLogs_, static_cast<float>(revealLogs_.preferredHeight())),
      validateAusBox_(validateAus_, static_cast<float>(validateAus_.preferredHeight())),
      revealValidationBox_(revealValidation_,
                           static_cast<float>(revealValidation_.preferredHeight())),
      logStatus_(juce::String(), form::kSmallPx),
      update_({}, FormButton::cta()),
      updateBox_(update_, static_cast<float>(update_.preferredHeight())),
      version_(juce::String(), form::kSmallPx, theme::kSubtle),
      licenses_("Licenses and source", FormButton::text(form::kBodyPx, false, theme::kLinkBlue)),
      licensesBox_(licenses_, static_cast<float>(licenses_.preferredHeight())),
      licenseNotice_("Powered by TONE3000. Free-tier integration through the hosted Select flow. Combined application: GNU AGPLv3; no warranty. "
                     "Original components retain their copyright and license notices.",
                     form::kSmallPx, theme::kSubtle) {
  // Info Bar.
  infoBar_.onChange = [this](bool on) { services_.hints.setEnabled(on); };
  add(infoBar_);

  // Gate / pitch: view settings the faceplate reads from the same keys.
  showGate_.onChange = [this](bool on) { services_.prefs.setBool(UiPrefs::kShowGateControl, on); };
  showPitch_.onChange = [this](bool on) { services_.prefs.setBool(UiPrefs::kShowPitchControl, on); };
  add(showGate_);
  add(showPitch_);

  // NAM A2 Size.
  namSize_.setInlineLabel();
  namSize_.content().setGap(form::kControlGap);
  namSize_.content().add(lite_);
  namSize_.content().add(full_);
  lite_.onSelect = [this] { services_.chain.setNamSlimSizeDefault(kSlimSizeLite); };
  full_.onSelect = [this] { services_.chain.setNamSlimSizeDefault(kSlimSizeFull); };
  add(namSize_);

  // Per-block controls.
  blockSize_.content().add(blockSizeTip_);
  blockSize_.onChange = [this](bool on) { services_.prefs.setBool(UiPrefs::kShowBlockSizeControl, on); };
  add(blockSize_);
  normalize_.content().add(normalizeTip_);
  normalize_.onChange = [this](bool on) { services_.prefs.setBool(UiPrefs::kShowBlockNormalizeControl, on); };
  add(normalize_);

  // Calibration.
  calibration_.content().add(*dbu_);
  calibration_.content().add(calibrationHelp_);
  calibration_.content().add(calibrationTip_);
  calibration_.content().add(calibrationHandoff_);
  calibrationHelp_.onLink = [](const juce::String& href) { juce::URL(href).launchInDefaultBrowser(); };
  calibration_.onChange = [this](bool on) { setAndRemember(calibrateParam_, on ? 1.0f : 0.0f); };
  add(calibration_);

  // Oversampling.
  oversampling_.content().setGap(8);
  oversampling_.content().add(rateLabel_);
  oversampling_.content().add(osRate_);
  osRate_.setOptions(kOsFactorOptions);
  osRate_.onChange = [this](const juce::String& v) {
    if (auto* p = services_.backend.parameter("osFactor"))
      setAndRemember(osFactorParam_, p->convertTo0to1(v.getFloatValue()));
  };
  oversampling_.onChange = [this](bool on) { setAndRemember(osEnabledParam_, on ? 1.0f : 0.0f); };
  add(oversampling_);

  // Multi-core.
  multiCore_.onChange = [this](bool on) { services_.chain.setMultiCore(on); };
  add(multiCore_);

  // MIDI Mapping.
  midi_.setInlineLabel();
  midi_.content().add(midiSection_);
  add(midi_);

  // Fork-only: personal API key. Placed after MIDI so account-adjacent
  // settings sit together.
  apiKey_.content().setGap(form::kControlGap);
  apiKey_.content().add(*apiKeyField_);
  apiKey_.content().add(apiKeyHelp_);
  apiKey_.content().add(apiKeyStatus_);
  apiKeyHelp_.onLink = [](const juce::String& href) { juce::URL(href).launchInDefaultBrowser(); };
  add(apiKey_);

  // Presets: a link to the folder the files live in. Hidden where there is
  // no file browser to open (iOS).
  if (services_.backend.canOpenPresetsFolder()) {
    presets_.setInlineLabel();
    presets_.content().add(openPresetsBox_);
    openPresets_.onClick = [this] { services_.backend.openPresetsFolder(); };
    add(presets_);
  }

  // Diagnostics.
  diagnostics_.setInlineLabel();
  diagnostics_.content().setGap(form::kControlGap);
  diagnostics_.content().add(copyLogsBox_);
  diagnostics_.content().add(revealLogsBox_);
  diagnostics_.content().add(validateAusBox_);
  diagnostics_.content().add(revealValidationBox_);
  diagnostics_.content().add(logStatus_);
  logStatus_.setVisible(false);
  copyLogs_.onClick = [this] {
    showLogStatus(services_.backend.copyLogs() ? "Logs copied to clipboard" : "No log file found yet");
  };
  revealLogs_.onClick = [this] {
    showLogStatus(services_.backend.revealLogs().isNotEmpty() ? "Revealed log file" : "No log file found yet");
  };
  // Opt-in hosted-AudioUnit validation, out of process (see the AU validator in
  // the processor). It runs in the background; the report updates as it goes.
  validateAus_.onClick = [this] {
    services_.backend.validateInstalledAudioUnits();
    showLogStatus("Validating installed Audio Units in the background (a few seconds each; the report updates as it goes)");
  };
  revealValidation_.onClick = [this] {
    const auto report = services_.backend.hostedValidationReportPath();
    if (report.existsAsFile()) {
      report.revealToUser();
      showLogStatus("Revealed validation report");
    } else {
      showLogStatus("No validation report yet - run Validate installed Audio Units first");
    }
  };
  add(diagnostics_);

  // Version / update footer.
  footer_.add(updateBox_);
  footer_.add(version_, form::kControlGap);
  {
    footer_.add(licenseNotice_, form::kControlGap);
    footer_.add(licensesBox_, form::kControlGap);
    licenses_.onClick = [] {
      const auto app = juce::File::getSpecialLocation(juce::File::currentApplicationFile);
      app.getChildFile("Contents/Resources/Licenses").revealToUser();
    };
  }
  update_.onClick = [this] {
    if (const auto& u = services_.updates.update()) juce::URL(u->url).launchInDefaultBrowser();
  };
  add(footer_);

  for (auto* p : {&calibrateParam_, &dbuParam_, &osEnabledParam_, &osFactorParam_})
    p->onChange = [this] { syncParams(); };
  services_.prefs.addListener(this);
  services_.chain.addListener(this);
  services_.updates.addListener(this);
  syncPrefs();
  syncParams();
  syncApiKey();
  syncFooter();
  chainChanged(services_.chain.state());
}

PluginSettingsPage::~PluginSettingsPage() {
  services_.updates.removeListener(this);
  services_.chain.removeListener(this);
  services_.prefs.removeListener(this);
}

void PluginSettingsPage::prefChanged(const juce::String& key) {
  if (key == UiPrefs::kShowHints || key == UiPrefs::kShowGateControl || key == UiPrefs::kShowPitchControl ||
      key == UiPrefs::kShowBlockSizeControl || key == UiPrefs::kShowBlockNormalizeControl)
    syncPrefs();
  if (key == fork::kCustomPublishableKey) syncApiKey();
}

void PluginSettingsPage::syncApiKey() {
  apiKeyField_->sync();
  apiKeyStatus_.setText(fork::customPublishableKey(services_.prefs).isNotEmpty()
                            ? "Personal key saved. Reopen this window to sign in with it."
                            : "No personal key. The build's key is used (if it has one).");
}

void PluginSettingsPage::syncPrefs() {
  infoBar_.setValue(services_.hints.enabled());
  showGate_.setValue(services_.prefs.getBool(UiPrefs::kShowGateControl, true));
  showPitch_.setValue(services_.prefs.getBool(UiPrefs::kShowPitchControl, false));
  const bool size = services_.prefs.getBool(UiPrefs::kShowBlockSizeControl, false);
  blockSize_.setValue(size);
  blockSize_.setExpanded(size);
  const bool normalize = services_.prefs.getBool(UiPrefs::kShowBlockNormalizeControl, false);
  normalize_.setValue(normalize);
  normalize_.setExpanded(normalize);
}

// Calibration and oversampling are host parameters (so a project reopens as
// saved) that also seed new instances machine-wide. Only a user edit here
// updates that default; a host restore or automation moving the same
// parameter does not (see Processor::persistParameterAsMachineDefault).
void PluginSettingsPage::rememberMachineDefault(const ParamBinding& param) {
  services_.backend.persistParamAsMachineDefault(param.id());
}

void PluginSettingsPage::setAndRemember(ParamBinding& param, float normalised) {
  param.set(normalised);
  rememberMachineDefault(param);
}

void PluginSettingsPage::syncParams() {
  const bool calibrate = calibrateParam_.boolValue();
  calibration_.setValue(calibrate);
  calibration_.setExpanded(calibrate);
  dbu_->sync();
  const bool os = osEnabledParam_.boolValue();
  oversampling_.setValue(os);
  oversampling_.setExpanded(os);
  if (auto* p = services_.backend.parameter("osFactor"))
    osRate_.setValue(juce::String(juce::roundToInt(p->convertFrom0to1(osFactorParam_.normalised()))));
}

void PluginSettingsPage::chainChanged(const ChainState& state) {
  const bool full = isSlimSizeFull(state.namSlimSizeDefault);
  lite_.setSelected(!full);
  full_.setSelected(full);
  multiCore_.setValue(state.multiCore);
}

// Version / update sit last so diagnostics stay above the footer.
void PluginSettingsPage::syncFooter() {
  const auto& update = services_.updates.update();
  const auto& version = services_.updates.localVersion();
  if (update) update_.setLabel("Update to v" + update->version);
  footer_.setShown(updateBox_, update.has_value());
  version_.setText("zynamfx v" + version + "  -  " + build::label());
  footer_.setShown(version_, version.isNotEmpty());
  const bool footer = update.has_value() || version.isNotEmpty();
  setShown(footer_, footer);
  // Every section carries SECTION_GAP below it; only the footer doesn't.
  setTrailing(footer ? 0 : form::kSectionGap);
}

void PluginSettingsPage::showLogStatus(const juce::String& status) {
  logStatus_.setText(status);
  diagnostics_.content().setShown(logStatus_, true);
  logStatusClear_.start(kLogStatusMs, [this] { diagnostics_.content().setShown(logStatus_, false); });
}

}  // namespace t3k::ui
