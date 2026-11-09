#!/bin/bash
set -euo pipefail

DIR="$(cd "$(dirname "$0")" >/dev/null 2>&1 && pwd -P)"
BIN="$DIR/build/lvgl_clock_arm64"

if [[ ! -e /dev/fb0 ]]; then
    echo "No /dev/fb0. Boot QEMU with --gui or --vnc so virtio-gpu provides a framebuffer." >&2
    exit 1
fi

case "$(uname -m)" in
    aarch64|arm64) ;;
    *)
        echo "lvgl_clock_arm64 targets arm64; current architecture is $(uname -m)." >&2
        exit 1
        ;;
esac

if [[ ! -x "$BIN" ]]; then
    echo "Missing $BIN." >&2
    echo "On the host run: make -C linux-app/lvgl-clock ARCH=arm64" >&2
    exit 1
fi

if ! newer="$(find "$DIR" -maxdepth 1 \( -name '*.c' -o -name '*.h' \) ! -name 'test_clock_math.c' -newer "$BIN" -print -quit)"; then
    echo "Cannot inspect $DIR for newer sources." >&2
    exit 1
fi
if [[ -n "$newer" ]]; then
    echo "$BIN is older than its sources." >&2
    echo "On the host run: make -C linux-app/lvgl-clock ARCH=arm64" >&2
    exit 1
fi

exec "$BIN"
