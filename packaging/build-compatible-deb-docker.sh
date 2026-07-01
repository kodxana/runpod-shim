#!/bin/sh
set -eu

if ! command -v docker >/dev/null 2>&1; then
    echo "docker not found; install Docker or run make package-deb inside an older distro image" >&2
    exit 127
fi

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
repo_dir=$(CDPATH= cd "$script_dir/.." && pwd)
image="${RUNPOD_SHIM_BUILD_IMAGE:-ubuntu:20.04}"

docker run --rm \
    -e DEBIAN_FRONTEND=noninteractive \
    -e RUNPOD_SHIM_VERSION="${RUNPOD_SHIM_VERSION:-0.1.0}" \
    -e RUNPOD_SHIM_DEB_ARCH="${RUNPOD_SHIM_DEB_ARCH:-}" \
    -v "$repo_dir:/src" \
    -w /src \
    "$image" \
    sh -lc '
        set -eu
        if ! command -v make >/dev/null 2>&1 ||
           ! command -v cc >/dev/null 2>&1 ||
           ! command -v dpkg-deb >/dev/null 2>&1 ||
           ! command -v objdump >/dev/null 2>&1; then
            apt-get update
            apt-get install -y --no-install-recommends \
                build-essential \
                binutils \
                ca-certificates \
                dpkg-dev \
                file
        fi

        make clean
        make package-deb
        deb=$(ls build/runpod-shim_*.deb | head -1)
        tmp=$(mktemp -d)
        trap "rm -rf \"$tmp\"" EXIT HUP INT TERM
        dpkg-deb -x "$deb" "$tmp"
        if objdump -T "$tmp/usr/lib/runpod-shim/librunpod-shim.so" | grep -E "GLIBC_2\\.(3[89]|[4-9][0-9])"; then
            echo "package imports too-new glibc symbols; use an older RUNPOD_SHIM_BUILD_IMAGE" >&2
            exit 1
        fi
        printf "%s\n" "$deb"
    '
