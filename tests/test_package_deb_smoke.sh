#!/bin/sh
set -eu

if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "skipping package-deb smoke test: dpkg-deb not found"
    exit 0
fi

version="${RUNPOD_SHIM_VERSION:-0.1.0}"

detect_arch() {
    if [ -n "${RUNPOD_SHIM_DEB_ARCH:-}" ]; then
        printf '%s\n' "$RUNPOD_SHIM_DEB_ARCH"
        return 0
    fi

    if command -v dpkg >/dev/null 2>&1; then
        dpkg --print-architecture
        return 0
    fi

    if command -v dpkg-architecture >/dev/null 2>&1; then
        dpkg-architecture -qDEB_HOST_ARCH
        return 0
    fi

    case "$(uname -m)" in
        x86_64|amd64)
            printf '%s\n' "amd64"
            ;;
        aarch64|arm64)
            printf '%s\n' "arm64"
            ;;
        armv7l|armv7*)
            printf '%s\n' "armhf"
            ;;
        i386|i486|i586|i686)
            printf '%s\n' "i386"
            ;;
        ppc64le)
            printf '%s\n' "ppc64el"
            ;;
        riscv64|s390x)
            uname -m
            ;;
        *)
            echo "unable to derive expected Debian architecture; set RUNPOD_SHIM_DEB_ARCH" >&2
            return 1
            ;;
    esac
}

arch=$(detect_arch)

deb="build/runpod-shim_${version}_${arch}.deb"
contents="build/package-deb.contents"
extract_root="build/package-deb-extract"

make package-deb

if [ ! -f "$deb" ]; then
    echo "expected package not found: $deb" >&2
    exit 1
fi

test "$(dpkg-deb -f "$deb" Architecture)" = "$arch"
dpkg-deb --contents "$deb" >"$contents"

has_content() {
    mode="$1"
    path="$2"

    if ! awk -v mode="$mode" -v path="./$path" \
        '$1 == mode && $2 == "root/root" && $6 == path { found = 1 }
         END { exit found ? 0 : 1 }' "$contents"; then
        echo "missing package content metadata: $mode root/root ./$path" >&2
        cat "$contents" >&2
        exit 1
    fi
}

has_content "-rwxr-xr-x" "usr/bin/runpod-shim"
has_content "-rwxr-xr-x" "usr/bin/runpod-shim-probe"
has_content "-rwxr-xr-x" "usr/bin/runpod-shim-install-global"
has_content "-rwxr-xr-x" "usr/lib/runpod-shim/librunpod-shim.so"
has_content "-rw-r--r--" "usr/share/doc/runpod-shim/README.md"

rm -rf "$extract_root"
mkdir -p "$extract_root"
dpkg-deb -x "$deb" "$extract_root"

grep -Fx 'default_lib="/usr/lib/runpod-shim/librunpod-shim.so"' \
    "$extract_root/usr/bin/runpod-shim-install-global"
