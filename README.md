# zynamfx — a full guitar rig in one window 🎸

<p align="center">
  <img src="docs/images/zynamfx-screenshot.png" alt="zynamfx standalone: two parallel stereo lanes with lane balance, a hosted AU/VST3 effect in the chain, and the block editor" width="920">
</p>

<p align="center"><em>Two parallel stereo lanes with per-lane balance, a hosted AU/VST3 effect in the chain, and the block editor — one window, no DAW.</em></p>

<p align="center">
  <a href="https://github.com/zackyusof/zynamfx/releases/latest"><img src="https://img.shields.io/github/v/release/zackyusof/zynamfx?style=flat-square&label=release" alt="Latest release"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-AGPL--3.0-blue?style=flat-square" alt="License: AGPL-3.0"></a>
  <img src="https://img.shields.io/badge/platform-macOS-lightgrey?style=flat-square" alt="Platform: macOS">
  <img src="https://img.shields.io/badge/plug--ins-AU%20%7C%20VST3-orange?style=flat-square" alt="Plug-in formats: AU and VST3">
  <a href="https://github.com/zackyusof/zynamfx/stargazers"><img src="https://img.shields.io/github/stars/zackyusof/zynamfx?style=flat-square&label=stars" alt="GitHub stars"></a>
  <img src="https://img.shields.io/badge/status-actively%20maintained-brightgreen?style=flat-square" alt="Actively maintained">
</p>

**zynamfx** is a free, open-source macOS signal-chain app for guitar, bass, synths — anything you plug in. It loads **Neural Amp Modeler (NAM)** captures and impulse responses, hosts **any AU or VST3 effect you already own** in the same chain, and runs the whole rig as a **Standalone app** or as an **AU/VST3 plug-in** inside your DAW. Amp capture → boutique chorus → cabinet IR → studio reverb → console saturation, all in one low-latency chain.

**New in 2.2.0**

- **The TONE3000 browser is inside the app.** Sign-in and the hosted **Select** flow run in an in-editor browser panel in the Standalone, AU and VST3 — no system-browser window, no bundled web engine (macOS uses the system WKWebView). Open in Browser, a copyable link and a device code remain as fallbacks.
- **Search the full catalog in the tone browser.** Search, sort, format, tags, makes, creators, verified/calibrated filters, your downloaded/favorited/created lists, and favourite writes — the same TONE3000 API calls the upstream plug-in makes, under your own key.
- **Parallel chains that rejoin — per channel, not summed.** In stereo mode a branch splits at a tone block and a second junction rejoins it at a chosen block. Each lane keeps its own output channel and pan while the main lane's remaining blocks process both as a shared tail: no correlated +6 dB, no comb filtering.
- **Lane balance on the last slot.** A compact L↔R control on each lane's last real slot places that row anywhere from hard left to hard right, with the rail knobs using the same range and an arc that grows out of centre.
- **Search works again** in the tone browser, **device-code sign-in** is restored, and in-app sign-in no longer fails with `redirect_uri must be a valid URL`.

## What it does

- **Build a signal chain:** arrange NAM amp/capture blocks and IR cabinet blocks, with seven starting slots per lane. Add, remove, reorder, duplicate, bypass, and save blocks in presets.
- **Run parallel chains that rejoin:** split a lane at a tone block, let both rows process, then rejoin at a chosen block. The paths are not summed — each lane keeps its own channel and pan, and the remaining blocks process both as a shared tail. The branch row ends at the join, and each lane's last slot carries its balance control. (A NAM block in the shared tail still runs one voice and feeds both channels, so keep amps before the merge.)
- **Load models:** browse the full TONE3000 catalog in the built-in tone browser, or load your own local `.nam` captures and IR `.wav` files. Models are downloaded only after you choose them; creator attribution and model licences are retained.
- **Shape the sound:** per-block level, mix and six-band EQ, plus global input/output, gate, pitch shift, bass/middle/treble and stereo spread. Presets and session state keep the rig ready.
- **Host installed effects:** drop any installed AU or VST3 into any slot; the plug-in's own editor opens. Side-chain buses are fed silence rather than disabled, so multi-bus effects load and render.
- **Standalone or in your DAW:** the Standalone manages its own audio device; the AU and VST3 run inside Logic, Ableton, Reaper, Pro Tools and other macOS hosts.
- **Resize freely:** the editor window and its contents resize together.

## Why there is nothing like it

NAM players play captures. Plugin hosts host plugins. **zynamfx is the only free, open-source app that does both in one chain built for guitarists:**

- **🧠 NAM captures, every flavour** — local `.nam` profiles or hundreds of thousands of captures on TONE3000, browsable in-app. Your tone library is effectively infinite — and free.
- **🔌 Your plugins live INSIDE the rig** — surgical EQ, studio reverb, 1176-style compression, console warmth: before the amp, after the cab, wherever your ears want them.
- **🎸🎸🎹 Not just guitar** — bass rigs, synth chains through neural saturation, vocals, drums. If it makes sound, zynamfx can chain it.
- **🚀 Standalone or in your DAW** — same chains, same presets, everywhere.
- **💸 Free forever, open source, no catch** — no tiers, no upsells, no locked features, no subscriptions.

> "I mainly built it for my own needs." — zackyusof

**Free and open source, no catch.** Public builds are keyless and carry no paid tier or upsell: bring your own free TONE3000 publishable key for catalog browsing, or work entirely offline with local `.nam`/IR files and third-party effects. The combined app is AGPLv3 with matching source; the TONE3000 code it builds on stays MIT. zynamfx is an independent community project — not an official TONE3000 product, and not endorsed by TONE3000 or Line 6.

**Actively maintained — this is not an abandoned fork.** zynamfx tracks the upstream [TONE3000 plugin](https://github.com/tone-3000/tone3000-plugin). A GitHub Action checks for a new stable TONE3000 release every day; when one lands it merges it, bumps the zynamfx patch version, rebuilds the Standalone, AU and VST3, validates the AU, packages the matching source, and publishes a fresh zynamfx release automatically. Every release names the exact TONE3000 tag and commit it is based on and ships an `UPSTREAM.txt` provenance file (also recorded in [UPSTREAM.md](UPSTREAM.md)). If an upstream change conflicts with a zynamfx-specific file, the workflow stops without publishing until the change is reviewed.

[Download](#install) · [Build from source](#build-from-source) · [Releases](https://github.com/zackyusof/zynamfx/releases) · [Documentation](#documentation) · [Version 2 compared with Version 1](#version-2-compared-with-version-1) · [License](#licensing-and-attribution)

## What 2.2.0 brings

**First stable release since 2.0.1.** Everything from the beta line ships together in one build.

### Added
- **The TONE3000 browser runs inside the plugin** — Standalone, AU and VST3. Open in Browser, copyable link and device code stay as fallbacks; no web engine is bundled.
- **Full catalog browsing in the tone browser** — search, sort, format, tags, makes, creators, verified/calibrated filters, your own lists, and favourite writes, under your own key.
- **Parallel chains can rejoin — per channel, not summed.** Each lane keeps its own output channel and pan; the main lane's remaining blocks process both as a shared tail. No correlated +6 dB, no comb. A NAM block in the shared tail still feeds both channels, so place amps before the merge. The branch row ends at the join.
- **Lane balance on a lane's last slot** — full travel from hard left to hard right, with the rail knobs on the same range and a centre-out arc. Hidden in mono and mono-sum, where pan is bypassed.

### Fixed
- **In-app sign-in** no longer fails with `redirect_uri must be a valid URL`.
- **Search** works again in the tone browser.
- **Device-code sign-in** works (RFC 8628 restored).
- **Branch dots** are visible instead of transparent until hovered.
- Settings persistence (dangling-symlink heal), slot click opens the tone browser, right-click opens the AU/VST3 chooser, the publishable key is re-read when Settings saves it, prerelease builds never prompt for updates, and right-click paste works in the key field.

If sign-in still does not persist, the log names the stage it stopped at. Add it to
[issue #8](https://github.com/zackyusof/zynamfx/issues/8) with your macOS version and artefact.

## Install

The [latest release](https://github.com/zackyusof/zynamfx/releases/latest) contains the macOS Apple Silicon Standalone app, AU/VST3 plug-ins, and the corresponding source archive. Builds are ad-hoc signed and are not notarized. See the release notes for installation instructions and supported formats.

### One-command install (recommended)

Unzip the release archives and `install-macos-release.sh` into one folder, then run:

```sh
bash install-macos-release.sh
```

The script copies the app to `/Applications` (`~/Applications` without admin rights) and the AU/VST3 bundles to the standard user plug-in folders (backing up any existing install first), clears the quarantine flag from only those zynamfx bundles, and refreshes the AU registration. Gatekeeper stays enabled. Only run it for bundles from the official releases; stop if macOS reports malware or a damaged bundle. `bash install-macos-release.sh --help` shows app-only and plug-ins-only options.

### Gatekeeper on first launch

The release builds are ad-hoc signed but not notarized, so macOS may warn that it cannot verify the developer. Only use this exception for a copy downloaded from the [official zynamfx releases](https://github.com/zackyusof/zynamfx/releases), and stop if macOS reports that the app contains malware or is damaged.

To open the Standalone app, try opening `zynamfx.app` once. If macOS blocks it with a developer-verification warning, open **System Settings → Privacy & Security**, scroll to **Security**, choose **Open Anyway** beside zynamfx, and confirm. Apple says this option appears for about an hour after the blocked launch; approving the app this way leaves Gatekeeper enabled.

If an audio host specifically refuses to load an installed AU or VST3 bundle because of its quarantine flag, first verify that the bundle came from the official release. Then remove quarantine from only the affected zynamfx bundle and restart the host:

```sh
# Run only the command for the format you installed.
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/Components/zynamfx.component"
# Or, for VST3:
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/zynamfx.vst3"
```

These commands target the specific user-installed bundle; do not use a wildcard or disable Gatekeeper globally. See Apple's instructions for [opening an app by overriding security settings](https://support.apple.com/en-gb/guide/mac-help/mh40617/27/mac/27) and its overview of [Gatekeeper and runtime protection](https://support.apple.com/en-ie/guide/security/sec5599b66df/web).

## Quick start

1. Download the [latest release](https://github.com/zackyusof/zynamfx/releases/latest) and open `zynamfx.app` (see Gatekeeper above).
2. Add blocks to the chain: a NAM amp/capture, an IR cabinet, and — if you want — a hosted AU/VST3 effect in any slot.
3. To browse the TONE3000 catalog, open **Settings → Plugin**, paste your own free TONE3000 **publishable key (client ID)**, then sign in. Never enter a secret key.
4. Pick a tone, shape it with the per-block controls, and save the rig as a preset. Open the AU or VST3 inside a DAW to use the same chain in a project.

## What hosting third-party effects unlocks

Adding third-party AU and VST support completely changes what you can do with a neural amp player. It transforms a basic player into a fully professional, self-contained guitar rig. In practice, hosting gives you:

1. **Studio-grade time effects, post-cabinet.** The built-in delays and reverbs in standard amp modelers often sound flat or generic. Hosting third-party plug-ins lets you run premium algorithmic or convolution reverbs — such as Valhalla VintageVerb or Eventide Blackhole — directly after your Neural Amp Modeler (NAM) cabinet impulse response, creating massive, cinematic ambient spaces right inside the standalone app.

2. **Intelligent, dynamic problem solving.** Guitar pickups and high-gain amp models frequently create harsh resonant frequencies or unwanted string squeaks, and standard graphic EQs cannot fix that without destroying the tone. With VST hosting you can load a dynamic equalizer or frequency-dependent expander — like FabFilter Pro-Q 3 or TDR Nova — that notches out those ear-piercing frequencies only when you strike the strings hard, keeping the overall tone warm and natural.

3. **True analogue character before the amp.** Real players often push the front end of a tube amplifier into saturation with a specific overdrive pedal. Instead of relying on digital simulations, you can load accurate analogue component-modeling plug-ins — Analog Obsession's free preamps, or Brainworx console emulations — right before the neural amp slot, adding authentic harmonic distortion, console saturation, and warmth to the digital capture.

4. **Advanced dynamics control.** High-gain metal rhythm needs strict control to keep palm-mutes from clipping, while clean funk needs a fast, snappy attack — and the basic compressors in standard modelers rarely offer enough control. Hosting third-party VSTs lets you reach for legendary studio compressors: an 1176 for aggressive limiting, an LA-2A for smooth clean leveling, or a multiband compressor to tighten up only the muddy low-end frequencies.

## Version 2 compared with Version 1

This is the public **Version 2** line. Version 1 was the earlier tone3000-plugin-effects source line and is now archived privately. Version 2 is the separate **zynamfx** product and public repository. It uses its own app name, icon, plug-in identifiers, configuration/settings folder (`zynamfx`), standalone bundle, graphics, and release artifacts. The TONE3000 NAM/IR processing foundation and required third-party notices remain credited and licensed under their applicable terms. The two versions are related source lines with distinct product identities. **Version 1 follows the aesthetics of the original TONE3000 plug-in; Version 2 has its own design language**, expressed through its distinct interface, graphics, and visual direction.

**Starting chain slots:** Version 1 shows five slots per lane in an empty chain; Version 2 shows seven, adding two starting slots per lane. This is the initial layout, not a maximum: both versions add a trailing slot as blocks are added.

## How it differs from the official TONE3000 plug-in

TONE3000 owns and operates its account, catalog, and hosted tone-selection service. zynamfx supplies its own signal-chain app and interface while preserving TONE3000 attribution wherever its integration is used. Full catalog browsing and tone selection use TONE3000's hosted OAuth Select experience under your own key; zynamfx does not mirror the catalog, bundle catalog models, or present TONE3000 services as its own. The tone browser is a client of those documented list endpoints — it does not download a model until you choose one.

Public builds contain no API key, account token, or downloaded catalog model. Configure your own TONE3000 **publishable key (client ID)** in Settings → Plugin → TONE3000 API key, then sign in with your account. Never enter a secret key. The product and its builds are free, open source, and have no paid product or upsell. The integration is designed for the current non-commercial Free tier and its documented OAuth flows and list endpoints; API access and terms remain controlled by TONE3000. See the [API Terms](https://www.tone3000.com/api/terms), [API guidance](https://www.tone3000.com/api), and [integration review](docs/tone3000-integration-2026-10-12.md).

The interface identifies the integration as **Powered by TONE3000** and links to [tone3000.com](https://www.tone3000.com). zynamfx uses its own wordmark and graphics. It does not use TONE3000's logo or claim affiliation, approval, or endorsement. Any creator content remains subject to its creator-selected license and attribution requirements.

## Common questions

Short, factual answers to the questions that come up most. Everything below describes the **current
release** (v2.2.0); check the release notes for the build you are running.

**How do I insert my own AU/VST3 effect into the chain?**
**Right-click the effect list** (or right-click a chain slot) and choose **Add AU/VST3 Effect**. The
chooser lists the AudioUnits and VST3s already installed on the machine; the search field at the top
of the chooser filters that list (empty query keeps the vendor → effect browsing). Searching only
filters what is already listed — it does not scan or instantiate anything. Effects that declare extra
buses (side-chains) load and render: the chain feeds extra buses silence instead of disabling them.

**How does TONE3000 sign-in work, and should it stay signed in?**
Set your own **publishable key** (client_id) in **Settings → Plugin → TONE3000 API key**, then sign in.
Sign-in happens **once**, in an in-editor browser panel that opens TONE3000's hosted **Select** page —
Standalone, AU and VST3 alike; there is no separate system-browser window and no separate "Authorize"
screen. If a host cannot host the panel, **Open in Browser**, a copyable link, and a device code remain
as fallbacks. Your selection is handed back to zynamfx and loaded locally. Sign-in is meant to
**persist** — the app stores its token set in its preferences, so after that first sign-in browsing and
tone selection happen in the plugin, not in a browser. If sign-in does not persist, it is tracked in
[issue #8](https://github.com/zackyusof/zynamfx/issues/8); please add your build version (the string
beside the ZY monogram), the artefact you installed, macOS version, whether you run the Standalone or a
DAW, and the log from `~/Library/Logs/zynamfx/zynamfx.log`.

**Which builds exist, and where do I get them?**
Everything ships from the [Releases page](https://github.com/zackyusof/zynamfx/releases/latest): a
Standalone app, an Audio Unit, a VST3, and a matching source archive, plus `INSTALL.txt` and
`UPSTREAM.txt`. Builds are **macOS Apple Silicon** only, ad-hoc signed and not notarized. No Windows
binary is published.

**Spread or Offset — which control does the plug-in have?**
They are not two separate controls. **Spread is the feature; Offset is its single knob** — the stereo
image slot holds one feature, keyed to one parameter.

**Is there a Windows version?**
No. A Windows build is **not planned by the maintainer**; others are welcome to build or port one, and
the matching source is published for that reason. There is no ETA, because no work is scheduled. If you
need the workflow on Windows today, run the official free TONE3000 plug-in inside a VST3 host and put
your effects beside it — Reaper, Kushview Element, Cantabile, Gig Performer and Blue Cat PatchWork can
all do that. No Windows NAM player hosts third-party VST3s itself, which is the gap zynamfx fills on macOS.

**Why a separate fork instead of changes upstream?**
zynamfx is an independent derivative of the open-source TONE3000 plug-in, not a patch series against it.
It keeps the upstream NAM/IR processing and adds its own interface, a seven-slot chain, resizable
editor, and hosted AU/VST3 effects. Upstream provenance for every release is in `UPSTREAM.txt`. It is
not an official TONE3000 product and claims no endorsement.

**Where should I report a problem?**
[Open an issue](https://github.com/zackyusof/zynamfx/issues) with: the build version (shown beside the ZY
monogram), the artefact you installed (`-standalone`, `-au`, `-vst3`, or a source build), your macOS
version and Mac model, whether you are in the Standalone app or a DAW, the exact steps, what you expected
and what happened, and the relevant part of `~/Library/Logs/zynamfx/zynamfx.log`.

## macOS configuration and data locations

Per-user zynamfx files are stored under `~/Library/Application Support/zynamfx/`:

- `zynamfx.settings` — Standalone audio-device setup and related JUCE host settings.
- `preferences.settings` — machine-wide plug-in defaults, including multi-core processing, the default NAM model size, input calibration, and oversampling. Shared by Standalone, AU, and VST3.
- `ui-preferences.settings` — interface preferences, your optional TONE3000 publishable key, and the remembered TONE3000 sign-in session. Treat this file as private; it contains saved access and refresh tokens. Signing out clears the saved session.
- `Presets/` — presets saved by the user. Factory presets installed for all users are in `/Library/Application Support/zynamfx/Presets/Factory/`.
- `LocalModels/` — copies of local NAM and IR files imported into zynamfx, so rigs can still find them if the original files are moved.

The diagnostic log is `~/Library/Logs/zynamfx/zynamfx.log`. AU and VST3 chain state is saved by the DAW in its project or session; user presets are separate files in `Presets/` and include the active model data needed to reopen them offline.

## Documentation

| Goal | Start here |
| --- | --- |
| See what changed in each release | [CHANGELOG.md](CHANGELOG.md) · [Latest release](https://github.com/zackyusof/zynamfx/releases/latest) |
| Understand the TONE3000 integration | [Integration review](docs/tone3000-integration-2026-10-12.md) · [Hosted effects](docs/hosted-effects.md) |
| See planned hardware effects send/return | [External effects research](docs/hardware-insert-research-2026-10-12.md) |
| Check licensing and corresponding source | [LICENSE-SCOPE.md](LICENSE-SCOPE.md) · [SOURCE.md](SOURCE.md) · [UPSTREAM.md](UPSTREAM.md) |
| Build the app yourself | [Build from source](#build-from-source) (macOS & Windows) |
| Build on Windows (x64) | [Windows build guide](docs/windows-build.md) |
| Report a bug or request a feature | [Issue chooser](https://github.com/zackyusof/zynamfx/issues/new/choose) |

## Build from source

### macOS
Requires macOS, Xcode Command Line Tools, CMake, Ninja, and Git.

```sh
git clone --recurse-submodules https://github.com/zackyusof/zynamfx.git
cd zynamfx
python3 script/build-stadium-prototype.py
```

The build produces `zynamfx.app`, `zynamfx.component` (AU), and `zynamfx.vst3`. Add `--install` to place the app in `/Applications` (or `~/Applications` without admin rights) and the plug-ins in the standard user plug-in folders. The build is keyless by default; enter your own publishable key in the app if you want to use the TONE3000 integration.

### Windows (x64)
Requires Windows 10/11, Visual Studio 2022 (MSVC v143), CMake, and Git. See the detailed [Windows build guide](docs/windows-build.md) for step-by-step instructions and ASIO setup.

```powershell
git clone --recurse-submodules https://github.com/zackyusof/zynamfx.git
cd zynamfx
powershell -ExecutionPolicy Bypass -File .\script\setup-asio-sdk.ps1
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Future hardware send/return support is documented in the [external effects research note](docs/hardware-insert-research-2026-10-12.md). It is a proposed feature, not part of the current release.

GitHub Actions checks the latest stable release of [tone-3000/tone3000-plugin](https://github.com/tone-3000/tone3000-plugin) daily. When it finds a newer release, it merges that upstream tag, updates the recorded upstream commit, increments the zynamfx patch version, builds all three macOS formats, validates the AU, packages corresponding source, and publishes a zynamfx release. Each release title and notes identify the exact TONE3000 release and commit used as its base, and the downloads include an `UPSTREAM.txt` provenance file. If upstream changes conflict with zynamfx-specific files, the workflow stops without publishing so the changes can be reviewed and resolved.

## Licensing and attribution

The combined zynamfx application is distributed under **GNU AGPLv3**. Original TONE3000 source portions remain under their upstream **MIT** license; NAM, AudioDSPTools, JUCE, fonts, icons, and other components keep their own notices. The release includes a source archive matching the shipped build. See [LICENSE-SCOPE.md](LICENSE-SCOPE.md), [SOURCE.md](SOURCE.md), [COPYING](COPYING), and the component license notices.

TONE3000 is credited as the source of the upstream application and the provider of the catalog service. NeuralAmpModelerCore and AudioDSPTools are credited to their upstream authors. zynamfx is an independent community project; all referenced names and marks belong to their respective owners.

## Star history

[![Star History Chart](https://api.star-history.com/svg?repos=zackyusof/zynamfx&type=Date)](https://star-history.com/#zackyusof/zynamfx&Date)
