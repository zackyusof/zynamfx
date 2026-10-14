# Changelog

All notable changes to zynamfx are recorded here. Dates are ISO (yyyy-mm-dd).

## [2.0.1] - 2026-10-14

### Added
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
