# Changelog

All notable changes to zynamfx are recorded here. Dates are ISO (yyyy-mm-dd).

## [2.0.2-beta.1] - 2026-10-17

Beta. Published as a GitHub **prerelease**; it is not offered as an update to anyone, and a beta build
never checks for updates itself.

### Fixed
- **Paste works in every text field.** The text menu was disabled, so right-click -> Paste did nothing
  in the TONE3000 API-key field (Cmd+V worked). The menu is enabled again.
- **A rejected API key now says why.** A secret key (`t3k_cs_...`) or a value without the `t3k_pub_`
  prefix previously vanished silently; the field now shows an inline explanation and keeps the text so
  it can be corrected.
- **Hosted effects are discoverable.** A plain click on the **+** tile now opens the tile's action
  sheet, which contains *Choose Tone...*, **Add AU/VST3 Effect**, Paste, Load File and Load Folder.
  The AU/VST3 chooser used to be reachable only by right-click, which made the feature look missing.
  Right-click is unchanged.

### Added
- **Sign-in diagnostics in the log.** The loopback listener's port and callback parameter names,
  the redirect URI, the parsed callback result (`missing_code`, `state_mismatch`, ...), the token
  exchange outcome and the signed-in confirmation are logged. Parameter *values* are never logged.
- **Build identity as the first log line:** version, OS, CPU architecture and bit width.

### Changed
- Version reporting now supports a prerelease label (`2.0.2-beta.1`) alongside the numeric build
  version used by CMake and the plugin metadata.

## [2.0.1] - 2026-10-14

### Added
- The build **version** is shown discretely beside the ZY monogram in the header.
- **Hosted-FX search.** A search field sits at the top of the Add AU/VST3
  Effect chooser: an empty query keeps the vendor -> effect browsing, and typing
  filters every installed AudioUnit and VST3 into one scrollable list
  (name . vendor (AU/VST3)), case-insensitive tokens that must all match.
  Searching only filters the already-listed bundles; it never scans or
  instantiates.
- **Opt-in AudioUnit validation.** Settings -> Diagnostics -> Validate installed
  Audio Units probes every installed AU out of process with Apple's `auval`
  (each in its own process, with a per-plugin timeout) and writes a resumable
  report to `~/Library/Application Support/zynamfx/hosted-effects-validation.txt`.

### Fixed
- **Multi-bus (side-chain) hosted effects now load and render.** The chain
  drives the plug-in's main bus and feeds any extra buses silence, instead of
  disabling them (which crashed plug-ins that read their side-chain, e.g.
  Plugin Alliance bx_aura) or rejecting them. This unblocks the large set of
  installed effects that declare a second bus.
- **Build identity is a build input.** The release workflow builds the public
  channel, so a published app's in-app repository link points at GitHub.

## [2.0.0] - 2026-10-13
- Initial Version 2 public release (based on TONE3000 plugin v0.0.12).
