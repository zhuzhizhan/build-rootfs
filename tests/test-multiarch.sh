#!/bin/bash
set -e

SCRIPT_DIR="$( cd "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P )"
cd "$SCRIPT_DIR/.."

if [[ $EUID -ne 0 ]]; then
    echo "This script must be run as root"
    exit 1
fi

TYPE="${1:-ubuntu24.04}"

for arch in arm64 arm32 riscv64; do
    echo "=== Building $TYPE for $arch ==="
    ./build-rootfs.sh "$TYPE" --arch "$arch"
    ./tests/qemu-test.sh "$(realpath "rootfs_$arch.ext4")" "$arch"
    rm -f "rootfs_${TYPE}_$arch.ext4"
    mv "rootfs_$arch.ext4" "rootfs_${TYPE}_$arch.ext4"
done

echo "All multiarch tests passed"
