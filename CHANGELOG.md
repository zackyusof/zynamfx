# Changelog

All notable changes to zynamfx are recorded here. Dates are ISO (yyyy-mm-dd).

## [2.2.2] - 2026-10-26

### Added
- **External MIDI clock.** zynamfx can follow a 24 PPQN MIDI clock from hardware or a DAW. The hint
  bar shows the incoming tempo beside the CPU readout — `EXT 120.0 LOCKED` when the tempo is stable,
  `EXT 120.0 CLOCK` while it settles, and `EXT - NO SIGNAL` when nothing arrives — with an activity
  lamp that holds for about 125 ms. Hosted effect slots receive the clock as their host tempo, so
  tempo-synced delays and modulators follow the rig.
- **How to use it.** Standalone: `Settings → Audio → MIDI inputs`, tick the device that sends clock,
  then start the clock (zynamfx only opens the inputs you enable). In Logic Pro:
  `Project Settings → Synchronization → MIDI`, transmit MIDI clock to the virtual port you enabled
  (IAC Driver, for example), and run the transport — Logic sends clock only while it is playing.

### Fixed
- Tempo no longer reads half speed on the first lock, no longer ignores a tempo change, and no
  longer mis-reads a halving as dropped ticks. A tempo step is followed within a second or two.
- A hosted effect slot now receives a running transport, so tempo-synced plug-ins follow the clock
  instead of sitting on their internal 120 BPM.
- A hosted effect loaded while the app is running is prepared exactly like one loaded at startup, so
  it syncs without restarting zynamfx.

## [2.2.2-beta.1] - 2026-10-24

Fixes only, plus the mono-in/stereo-out bus layout (a host-contract addition: no new engine, GUI
surface or file-format change).

### Added
- **A mono track can insert zynamfx as mono → stereo.** `isBusesLayoutSupported` only accepted
  `mono/mono` and `stereo/stereo`, so DAWs never offered the mono-in/stereo-out insert on a guitar
  track. The new layout is the mono source mirrored into both lanes before the input fold, so it
  renders exactly like the same take on a stereo bus (both chain modes). `stereo → mono` stays
  unavailable on purpose.

### Fixed
- **Reinstalling no longer leaves the previous AU/VST3 shadowing the new one.** The build script
  wrote its `-backup-` copies next to the live bundle inside `Plug-Ins/{Components,VST3}`, where
  CoreAudio registers them under the same component identity; the stale copy could win the lookup
  (measured: a fresh AU still refused `mono → stereo` because the backup answered the probe). Backups
  now go to `Plug-Ins/Components-backups` and `Plug-Ins/VST3-backups`, and any strays already in the
  scanned folder are moved there on install.
- **The sign-in screen offers a way out of the browser wait.** While the system browser is out, the
  page only offered "Copy Link"; `SignInScreen` now also shows "Sign in on a different device" (the
  phone/device-code flow was already built and wired but never placed on the page). Reported in issue
  #8 as having no escape when the browser never came back.
- **The README's build instructions clone the right repository.** The `git clone` line named an
  unrelated account, so anyone following "Build and verify" fetched someone else's fork.

### Changed
- **The Offset knob's help names its feature.** It reads "Spread — double-track lag…", because
  upstream calls the feature Spread and the knob Offset, and users arriving from either build asked
  which control zynamfx has (issue #4).
- **`docs/PORTING.md` records what a port must satisfy** — toolchain, JUCE version and patches, the
  identity values host sessions are keyed on, and what is untested on Windows/Linux (issue #7).

## [2.2.1] - 2026-10-21

Stable release on the 2.2.0 line: pan corrections, two gallery fixes and a packaging fix.

### Fixed
- **A pan placement after the branch merge no longer folds the rig.** A pan set on a shared-rejoin-tail
  block is now a balance of the *merged* image that **composes with** the two lanes' own placements
  instead of replacing them. Before, both lanes resolved to that one value, the image matrix became
  rank-1 and the two chains were summed - an authored cab split (e.g. JCM900 left / Mesa right)
  collapsed into a single signal with a small L/R tilt.
- **Hosted-effect tiles react to audio again.** The tile's inset energy glow (the blue->yellow->red
  picture driven by the block's output meter) is composited only into `ToneImage`, which
  `ToneTile::syncState` hid for effect blocks; the captured plugin screenshot is now injected into
  `ToneImage`, so it composites the same glow as tone/NAM art and falls back to the effect glyph
  until a screenshot exists.
- **The header shows the full version label.** The version was drawn into a fixed 48 px slot, so a
  prerelease label was clipped mid-glyph (`v2.2.1-beta` instead of `v2.2.1-beta.3`). The logo box is
  now sized from the measured label, so any suffix fits; stable builds keep their exact layout.
- **The shipped AU/VST3 identity code is restored (`Zyfx`)**, so sessions saved by 2.2.0 load.

### Changed
- **The pan strip marks a placement that applies to both lanes**: accent `L`/`R` letters, a two-headed
  marker on the track and an accessibility description, so a shared-tail placement is no longer
  indistinguishable from a lane-local one.
- `VERSION` records the release number (it read `0.0.14` at every 2.x tag before the beta cuts).

## [2.2.0] - 2026-10-18

First stable release since 2.0.1. Everything below ships together.

### Added
- **The TONE3000 browser runs inside the plugin.** Sign-in and the hosted OAuth flow open in an
  in-editor browser panel instead of a separate system-browser window — standalone, AU and VST3.
  **Open in Browser**, a copyable link and a device code stay as fallbacks; no web engine is bundled
  (macOS uses the system WKWebView).
- **Full catalog browsing in the tone browser.** Search, sort, format, tags, makes, creators,
  verified/calibrated filters, the signed-in user's downloaded/favorited/created lists, and
  favourite writes — the same TONE3000 API calls the upstream plugin makes, under your own key.
- **Parallel chains can rejoin — per channel, not summed.** In stereo mode a branch splits at a tone
  block; a second junction rejoins the branch lane at a chosen block. The two paths are **not
  summed**: after the split each lane keeps its own output channel, and the trunk's remaining blocks
  process both — a shared tail that still lets each lane keep its own pan. No correlated +6 dB and no
  comb filtering. A NAM block inside the shared tail still runs one voice and fans ch0 to ch1, so put
  amps before the merge.
- **Every populated slot places the lane, independently.** In stereo mode each populated (non-insert)
  slot carries its own compact L↔R placement, stored per block and shown on the slot: 0..1 runs hard
  left to hard right, and a double-click clears it so the slot inherits whatever is upstream (or the
  rail). A serial lane holds one placement, so the last placed block along a lane's path wins — which
  is what makes the two lanes independent; a slot in a shared rejoin tail places both. The rail knobs
  keep the same full range, with a bipolar arc that grows out of noon. Hidden in mono and mono-sum,
  where the image matrix bypasses pan.
- **The branch row ends at the join.** With a rejoin active, the second row no longer offers empty
  slots past the merge point, and the return elbow is drawn at the merge column.

### Fixed
- **In-app sign-in no longer fails with "redirect_uri must be a valid URL".** JUCE's macOS browser
  percent-encodes the whole URL it is given; the already-escaped authorize URL was double-encoded
  before it reached TONE3000. The panel now hands the webview the decoded URL.
- **Search works again in the tone browser.** The fork had hidden the search field and reduced the
  filter row to gear chips, so only paging was possible.
- **Device-code sign-in works.** Requesting and polling a device code was refused locally; the RFC
  8628 flow is restored.
- **Branch dots are visible.** They were fully transparent until hovered in a 24 px gap.
- Beta-line fixes carried into the stable release: settings persistence (dangling-symlink heal),
  slot click opens the tone browser, right-click opens the AU/VST3 chooser, the publishable key is
  re-read when Settings saves it, prerelease builds never prompt for updates, and right-click paste
  works in the key field.

## [2.1.0-beta.2] - 2026-10-18

Beta fix for v2.1.0-beta.1. Prerelease: never offered as an update.

### Fixed
- **In-app sign-in no longer fails with "redirect_uri must be a valid URL".** JUCE's
  macOS browser percent-encodes the whole URL it is given, so the already-escaped
  authorize URL reached TONE3000 double-encoded (`http%253A%2F%2Flocalhost…`) and the
  server rejected the redirect. The panel now hands the webview the decoded URL, which
  CoreFoundation re-encodes to exactly the redirect the system browser used to send.

## [2.1.0-beta.1] - 2026-10-18

Beta for v2.1.0. Prerelease: never offered as an update.

### Added
- **The TONE3000 browser opens inside the plugin.** Sign-in and the hosted
  Select flow — catalog search, filters, audition and tone pick — now run in an
  in-editor browser panel instead of the system browser, in the standalone app,
  the AU and the VST3. Closing the panel abandons the flow; **Open in Browser**
  stays as an escape hatch, and the sign-in screen keeps the copy-link and
  device-code fallbacks. No engine is bundled: macOS uses the system WKWebView,
  and the redirect back to the local listener is replayed by the app.
- **The build label shows the prerelease.** The version beside the ZY monogram
  and in Settings reads `2.1.0-beta.1`, and a beta never checks for updates.

## [2.0.2-beta.2] - 2026-10-17

Beta fix for v2.0.2-beta.1. Prerelease: never offered as an update.

### Fixed
- **A click on a slot opens the tone browser (NAM profiles) again.** v2.0.2-beta.1 made the plain click open an action sheet instead; that change is reverted. Right-click / ctrl-click now opens the AU/VST3 effect chooser directly, as requested.
- **Settings persist.** The app-data folder heal now handles a dangling symlink (it previously looped on "File exists" and every write failed), probes writability with a real write instead of guessing, falls back to a local folder when the primary one cannot be made writable, and says so in the log.
- **TONE3000 browsing works after entering the key.** The publishable key is re-read when Settings commits it, instead of only at editor construction, so browsing no longer needs an app restart.
- **Slot Copy/Remove** are reachable again as ⌘C / Delete on a focused tile (the context menu that held them is gone).

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
