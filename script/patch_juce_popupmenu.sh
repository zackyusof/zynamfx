#!/bin/sh
# Local fix for the JUCE 9.0.2+ macOS regression where PopupMenu SUBMENUS render
# below/behind the parent menu (upstream commit 5fc8d348 "Raise modal windows to
# the modal window level"; see JUCE forum thread 69655).
#
# The sub-menu windows are ComponentPeer::windowIsTemporary; the macOS peer must
# not re-level them, otherwise they are pushed behind the parent menu. This patch
# skips temporary windows in MacComponentPeer::modalComponentManagerChanged().
#
# libs/juce is fetched (gitignored), so reapply after any clean configure:
#   sh script/patch_juce_popupmenu.sh            # from the repo root
#
# Idempotent: safe to run repeatedly.
set -e
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
F="${1:-$REPO_DIR/libs/juce/modules/juce_gui_basics/native/juce_NSViewComponentPeer_mac.mm}"

if [ ! -f "$F" ]; then
  echo "not found: $F (configure/build first so JUCE is fetched)" >&2
  exit 1
fi

if grep -q "T3K_POPUPMENU_TEMPORARY_LEVEL" "$F"; then
  echo "already patched: $F"
  exit 0
fi

python3 - "$F" <<'PY'
import sys
path = sys.argv[1]
src = open(path).read()
anchor = (
    "            if (isSharedWindow)\n"
    "                return {};\n"
    "\n"
    "            auto* manager = ModalComponentManager::getInstance();\n"
    "\n"
    "            if (manager->getNumModalComponents() > 0)\n"
)
fix = (
    "            if (isSharedWindow)\n"
    "                return {};\n"
    "\n"
    "            // T3K_POPUPMENU_TEMPORARY_LEVEL: PopupMenu sub-menus are\n"
    "            // windowIsTemporary and must not be re-levelled here or they render\n"
    "            // below the parent menu on macOS. See JUCE forum 69655.\n"
    "            if ((getStyleFlags() & ComponentPeer::windowIsTemporary) != 0)\n"
    "                return {};\n"
    "\n"
    "            auto* manager = ModalComponentManager::getInstance();\n"
    "\n"
    "            if (manager->getNumModalComponents() > 0)\n"
)
if anchor not in src:
    sys.exit("anchor not found — JUCE may have changed; patch manually")
open(path, "w").write(src.replace(anchor, fix, 1))
print("patched", path)
PY
