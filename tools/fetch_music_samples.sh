#!/usr/bin/env bash
# fetch_music_samples.sh -- downloads a free MusicRadar SampleRadar pack (royalty-free, planning.md: test audio by download scripts, not in the
# repository) for the gain curves on music (W11.3f, docs/design/W11-dynamics-plan.md 4.8):
#   PluginLabHost --gain-curves "<folder>/Drum loops/FS_Drumloop_01a(130BPM).wav" report.md <compressor.vst3> ...
#
# Usage:  tools/fetch_music_samples.sh [target-folder]
#   target folder: default $PLUGINLAB_SAMPLE_DIR, or ~/.cache/pluginlab/samples
# Prints the pack's folder on the last line. Nothing is downloaded if it is already there.
set -euo pipefail

PACK_NAME="musicradar-realworld-drum-samples"
PACK_URL="https://cdn.mos.musicradar.com/audio/samples/${PACK_NAME}.zip"

TARGET="${1:-${PLUGINLAB_SAMPLE_DIR:-$HOME/.cache/pluginlab/samples}}"
PACK_DIR="$TARGET/$PACK_NAME"
if [ -d "$PACK_DIR" ] && [ -n "$(ls -A "$PACK_DIR")" ]; then
    echo "$PACK_DIR"
    exit 0
fi

mkdir -p "$PACK_DIR"
ZIP_FILE="$(mktemp)"
trap 'rm -f "$ZIP_FILE"' EXIT
echo "Downloading $PACK_URL" >&2
curl --fail --location --silent --show-error --output "$ZIP_FILE" "$PACK_URL"
unzip -q -o "$ZIP_FILE" -d "$PACK_DIR" -x "__MACOSX/*"
echo "$PACK_DIR"
