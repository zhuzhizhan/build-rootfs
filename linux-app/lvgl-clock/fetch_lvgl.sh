#!/bin/bash
set -euo pipefail

VERSION="9.3.0"
ROOT="$(cd "$(dirname "$0")" >/dev/null 2>&1 && pwd -P)"
DEST="$ROOT/third_party"
TARGET="$DEST/lvgl"
URL="https://github.com/lvgl/lvgl/archive/refs/tags/v${VERSION}.tar.gz"

lvgl_version_ok() {
    local major minor patch

    major="${VERSION%%.*}"
    minor="${VERSION#*.}"
    minor="${minor%%.*}"
    patch="${VERSION##*.}"

    [[ -f "$TARGET/lv_version.h" ]] || return 1
    grep -q '9\.3\.0' "$TARGET/lv_version.h" && return 0
    grep -q "^#define LVGL_VERSION_MAJOR[[:space:]]\+${major}$" "$TARGET/lv_version.h" &&
        grep -q "^#define LVGL_VERSION_MINOR[[:space:]]\+${minor}$" "$TARGET/lv_version.h" &&
        grep -q "^#define LVGL_VERSION_PATCH[[:space:]]\+${patch}$" "$TARGET/lv_version.h"
}

if [[ -f "$TARGET/lvgl.h" && -d "$TARGET/src" ]] && lvgl_version_ok; then
    echo "LVGL v${VERSION} already present in $TARGET"
    exit 0
fi

if ! command -v curl >/dev/null 2>&1; then
    echo "curl is required to download LVGL." >&2
    exit 1
fi

if ! command -v tar >/dev/null 2>&1; then
    echo "tar is required to extract LVGL." >&2
    exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Downloading LVGL v${VERSION} ..."
curl -fL --retry 3 -o "$TMP/lvgl.tar.gz" "$URL"

SIZE="$(stat -c %s "$TMP/lvgl.tar.gz")"
if [[ "$SIZE" -lt 50000000 ]]; then
    echo "Downloaded archive is too small ($SIZE bytes); refusing to continue." >&2
    exit 1
fi

tar -tzf "$TMP/lvgl.tar.gz" >/dev/null

tar -xzf "$TMP/lvgl.tar.gz" -C "$TMP" \
    "lvgl-${VERSION}/src" \
    "lvgl-${VERSION}/lvgl.h" \
    "lvgl-${VERSION}/lv_version.h" \
    "lvgl-${VERSION}/lvgl_private.h" \
    "lvgl-${VERSION}/LICENCE.txt" \
    "lvgl-${VERSION}/COPYRIGHTS.md" \
    "lvgl-${VERSION}/README.md"

rm -rf "$TARGET"
mkdir -p "$TARGET"
mv "$TMP/lvgl-${VERSION}"/* "$TARGET"/

echo "LVGL v${VERSION} ready in $TARGET"
