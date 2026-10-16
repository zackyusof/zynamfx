#!/usr/bin/env bash
# Install zynamfx from a downloaded GitHub release on macOS Apple Silicon.
#
# One command after unzipping the release archives next to this script:
#
#   bash install-macos-release.sh
#
# What it does:
#   1. Copies zynamfx.app to /Applications (~/Applications if not writable),
#      zynamfx.component to ~/Library/Audio/Plug-Ins/Components,
#      zynamfx.vst3 to ~/Library/Audio/Plug-Ins/VST3 (backs up any
#      existing install first, newest backup only).
#   2. Removes the quarantine flag from ONLY these zynamfx bundles, so
#      Gatekeeper stops blocking the app and hosts stop refusing the
#      plug-ins. Nothing else on the system is touched and Gatekeeper
#      stays enabled.
#   3. Restarts AudioComponentRegistrar so hosts see the fresh AU.
#
# Only run this for bundles downloaded from the official zynamfx GitHub
# releases. Stop if macOS reports malware or a damaged bundle.
#
# Options:
#   --app-only       install only the Standalone app
#   --plugins-only   install only the AU and VST3 bundles
#   --no-quarantine  skip the quarantine removal (manual Open Anyway instead)
#   --help           show this help
set -euo pipefail

APP_ONLY=0
PLUGINS_ONLY=0
DO_QUARANTINE=1

for arg in "$@"; do
  case "$arg" in
    --app-only) APP_ONLY=1 ;;
    --plugins-only) PLUGINS_ONLY=1 ;;
    --no-quarantine) DO_QUARANTINE=0 ;;
    --help|-h)
      sed -n '2,28p' "$0"
      exit 0
      ;;
    *) echo "Unknown option: $arg (try --help)" >&2; exit 1 ;;
  esac
done

SRC_DIR="$(cd "$(dirname "$0")" && pwd)"

install_bundle() {
  local src="$1" dest="$2"
  local name
  name="$(basename "$dest")"
  if [ ! -e "$src" ]; then
    echo "SKIP: $name not found next to this script ($src)" >&2
    return 0
  fi
  mkdir -p "$(dirname "$dest")"
  if [ -e "$dest" ]; then
    local backup="${dest}.bak"
    rm -rf "$backup"
    mv "$dest" "$backup"
    echo "Backed up existing $name to $backup"
  fi
  cp -R "$src" "$dest"
  echo "Installed $name to $dest"
  if [ "$DO_QUARANTINE" -eq 1 ]; then
    xattr -dr com.apple.quarantine "$dest" 2>/dev/null || true
    echo "Cleared quarantine on $name"
  fi
}

if [ "$PLUGINS_ONLY" -eq 0 ]; then
  APP_DEST="${ZYNAMFX_APP_DIR:-/Applications/zynamfx.app}"
  if [ -z "${ZYNAMFX_APP_DIR:-}" ] && [ ! -w "/Applications" ]; then
    APP_DEST="$HOME/Applications/zynamfx.app"
  fi
  install_bundle "$SRC_DIR/zynamfx.app" "$APP_DEST"
fi

if [ "$APP_ONLY" -eq 0 ]; then
  install_bundle "$SRC_DIR/zynamfx.component" \
    "$HOME/Library/Audio/Plug-Ins/Components/zynamfx.component"
  install_bundle "$SRC_DIR/zynamfx.vst3" \
    "$HOME/Library/Audio/Plug-Ins/VST3/zynamfx.vst3"
  killall -9 AudioComponentRegistrar 2>/dev/null || true
  echo "Restarted AudioComponentRegistrar; rescan plug-ins in your DAW."
fi

echo "Done. First launch: right-click zynamfx.app > Open > Open (once)."
