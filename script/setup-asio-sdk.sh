#!/usr/bin/env bash
# Downloads and extracts the Steinberg ASIO SDK into libs/asiosdk
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TARGET_DIR="$REPO_ROOT/libs/asiosdk"
EXPECTED_HEADER="$TARGET_DIR/ASIOSDK/common/iasiodrv.h"

if [ -f "$EXPECTED_HEADER" ]; then
    echo "ASIO SDK already installed at: $TARGET_DIR/ASIOSDK"
    exit 0
fi

echo "Downloading Steinberg ASIO SDK..."
mkdir -p "$REPO_ROOT/libs"
ZIP_PATH="$REPO_ROOT/libs/asiosdk-download.zip"

cleanup() {
    rm -f "$ZIP_PATH"
}
trap cleanup EXIT

curl -sSL "https://www.steinberg.net/asiosdk" -o "$ZIP_PATH"

echo "Extracting ASIO SDK to $TARGET_DIR..."
rm -rf "$TARGET_DIR"
unzip -q "$ZIP_PATH" -d "$TARGET_DIR"

if [ -f "$EXPECTED_HEADER" ]; then
    echo "Successfully installed Steinberg ASIO SDK to $TARGET_DIR/ASIOSDK"
else
    echo "Error: Extraction completed, but $EXPECTED_HEADER not found." >&2
    exit 1
fi
