#!/bin/sh
set -eu

make

tmpdir="$PWD/tmp/probe-contract"
trap 'status=$?; if [ "$status" -eq 0 ]; then rm -rf "$tmpdir"; fi; exit "$status"' EXIT
rm -rf "$tmpdir"
mkdir -p "$tmpdir"

tools/runpod-shim-probe >"$tmpdir/source.out" 2>"$tmpdir/source.err"
grep -F 'Runpod Shim Probe' "$tmpdir/source.out"
grep -F 'active modules' "$tmpdir/source.out"
grep -F 'cgroup memory' "$tmpdir/source.out"
grep -F 'nvidia devices' "$tmpdir/source.out"
test ! -s "$tmpdir/source.err"

grep -Fq "grep -H -E 'Bus Location|GPU UUID|Device Minor'" tools/runpod-shim-probe

install_root="$tmpdir/install-root"
make install DESTDIR="$install_root" PREFIX=/opt/runpod
installed_probe="$install_root/opt/runpod/bin/runpod-shim-probe"
test -x "$installed_probe"

"$installed_probe" >"$tmpdir/installed.out" 2>"$tmpdir/installed.err"
grep -F 'Runpod Shim Probe' "$tmpdir/installed.out"
grep -F 'active modules' "$tmpdir/installed.out"
grep -F 'cgroup memory' "$tmpdir/installed.out"
grep -F 'nvidia devices' "$tmpdir/installed.out"
test ! -s "$tmpdir/installed.err"

make uninstall DESTDIR="$install_root" PREFIX=/opt/runpod
test ! -e "$installed_probe"
