#!/bin/sh
set -eu

make

tmpdir="$PWD/tmp/wrapper-contract"
trap 'status=$?; if [ "$status" -eq 0 ]; then rm -rf "$tmpdir"; fi; exit "$status"' EXIT
rm -rf "$tmpdir"
mkdir -p "$tmpdir"

output="$(RUNPOD_SHIM_TRACE=1 tools/runpod-shim -- sh -c 'printf "%s" "$LD_PRELOAD"')"
case "$output" in
    *librunpod-shim.so*) ;;
    *) echo "missing shim in LD_PRELOAD: $output" >&2; exit 1 ;;
esac

RUNPOD_SHIM_TRACE=1 tools/runpod-shim --disable-cgroup-memory -- sh -c 'test "$RUNPOD_SHIM_CGROUP_MEMORY" = 0'
RUNPOD_SHIM_TRACE=1 tools/runpod-shim --disable-gpu-video -- sh -c 'test "$RUNPOD_SHIM_GPU_VIDEO" = 0'
RUNPOD_SHIM_TRACE=1 tools/runpod-shim --disable-gpu-ioctl -- sh -c 'test "$RUNPOD_SHIM_GPU_IOCTL" = 0'

install_root="$tmpdir/install-root"
make install DESTDIR="$install_root" PREFIX=/opt/runpod

"$install_root/opt/runpod/bin/runpod-shim" -- /bin/sh -c 'case "$LD_PRELOAD" in *opt/runpod/lib/runpod-shim/librunpod-shim.so*) exit 0;; *) echo "$LD_PRELOAD" >&2; exit 1;; esac'

make uninstall DESTDIR="$install_root" PREFIX=/opt/runpod
test ! -e "$install_root/opt/runpod/bin/runpod-shim"
test ! -e "$install_root/opt/runpod/lib/runpod-shim/librunpod-shim.so"

if tools/runpod-shim --unknown -- true 2>"$tmpdir/wrapper.err"; then
    echo "unknown option unexpectedly succeeded" >&2
    exit 1
fi
grep -F "unknown option" "$tmpdir/wrapper.err"
