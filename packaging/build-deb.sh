#!/bin/sh
set -eu

if ! command -v dpkg-deb >/dev/null 2>&1; then
    echo "dpkg-deb not found; install dpkg-dev or run on a Debian-compatible system" >&2
    exit 127
fi

script_dir=$(CDPATH= cd "$(dirname "$0")" && pwd)
repo_dir=$(CDPATH= cd "$script_dir/.." && pwd)
cd "$repo_dir"

package="runpod-shim"
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
            echo "unable to derive Debian architecture; set RUNPOD_SHIM_DEB_ARCH" >&2
            return 1
            ;;
    esac
}

arch=$(detect_arch)
build_dir="$repo_dir/build"
root="$build_dir/deb-root"
control_dir="$root/DEBIAN"
deb="$build_dir/${package}_${version}_${arch}.deb"

rm -rf "$root"
install -d "$build_dir" "$control_dir"

make install PREFIX=/usr DESTDIR="$root"

cat >"$control_dir/control" <<EOF
Package: $package
Version: $version
Section: utils
Priority: optional
Architecture: $arch
Maintainer: MadiatorLabs <madiator2011@users.noreply.github.com>
Description: LD_PRELOAD compatibility shim for Runpod workloads
 Runpod Shim wraps Linux processes with optional GPU topology, NVIDIA ioctl,
 and cgroup memory compatibility fixes enabled by default at runtime.
EOF

tmp_dir=$(mktemp -d "${TMPDIR:-/tmp}/runpod-shim-deb.XXXXXX")
trap 'rm -rf "$tmp_dir"' EXIT HUP INT TERM
package_root="$tmp_dir/deb-root"
install -d "$package_root"
cp -R "$root/." "$package_root/"

# Windows-mounted WSL worktrees can report every staged path as 0777, which
# dpkg-deb rejects for DEBIAN/. Normalize modes in a native temp directory.
find "$package_root" -type d -exec chmod 0755 {} +
chmod 0755 \
    "$package_root/usr/bin/runpod-shim" \
    "$package_root/usr/bin/runpod-shim-probe" \
    "$package_root/usr/bin/runpod-shim-install-global" \
    "$package_root/usr/lib/runpod-shim/librunpod-shim.so"
chmod 0644 \
    "$package_root/DEBIAN/control" \
    "$package_root/usr/share/doc/runpod-shim/README.md"

dpkg_args=
if dpkg-deb --help 2>/dev/null | grep -q -- '--root-owner-group'; then
    dpkg_args="--root-owner-group"
elif [ "$(id -u)" -ne 0 ]; then
    echo "dpkg-deb does not support --root-owner-group; run as root to build root-owned package contents" >&2
    exit 1
fi

dpkg-deb $dpkg_args --build "$package_root" "$deb"
echo "$deb"
