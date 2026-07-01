#!/bin/sh
set -eu

make
rm -rf tmp/global
mkdir -p tmp/global/etc tmp/global/lib
touch tmp/global/etc/ld.so.preload

root="$PWD/tmp/global"
lib="/usr/lib/runpod-shim/librunpod-shim.so"
preload="$root/etc/ld.so.preload"

run_global() {
    RUNPOD_SHIM_GLOBAL_ROOT="$root" \
    RUNPOD_SHIM_GLOBAL_LIB="$lib" \
    tools/runpod-shim-install-global "$@"
}

run_global enable

grep -Fx "$lib" "$preload"

run_global enable

test "$(grep -Fxc "$lib" "$preload" || true)" -eq 1

test "$(run_global status)" = "enabled: $lib"

run_global disable

if grep -Fx "$lib" "$preload"; then
    echo "library still present after disable" >&2
    exit 1
fi

test "$(run_global status)" = "disabled: $lib"

fresh_root="$PWD/tmp/global/fresh"
fresh_lib="/custom/lib/librunpod-shim.so"
test "$(RUNPOD_SHIM_GLOBAL_ROOT="$fresh_root" RUNPOD_SHIM_GLOBAL_LIB="$fresh_lib" tools/runpod-shim-install-global status)" = "disabled: $fresh_lib"
test ! -e "$fresh_root/etc/ld.so.preload"

noargs_root="$PWD/tmp/global/noargs"
if RUNPOD_SHIM_GLOBAL_ROOT="$noargs_root" RUNPOD_SHIM_GLOBAL_LIB="$fresh_lib" tools/runpod-shim-install-global >/dev/null 2>&1; then
    echo "missing command unexpectedly succeeded" >&2
    exit 1
fi
test ! -e "$noargs_root/etc/ld.so.preload"

invalid_root="$PWD/tmp/global/invalid"
if RUNPOD_SHIM_GLOBAL_ROOT="$invalid_root" RUNPOD_SHIM_GLOBAL_LIB="$fresh_lib" tools/runpod-shim-install-global nope >/dev/null 2>&1; then
    echo "invalid command unexpectedly succeeded" >&2
    exit 1
fi
test ! -e "$invalid_root/etc/ld.so.preload"

preserve_root="$PWD/tmp/global/preserve"
preserve_preload="$preserve_root/etc/ld.so.preload"
mkdir -p "$preserve_root/etc"
cat >"$preserve_preload" <<EOF
/opt/other-one.so
$lib
/opt/other-two.so
EOF
RUNPOD_SHIM_GLOBAL_ROOT="$preserve_root" RUNPOD_SHIM_GLOBAL_LIB="$lib" tools/runpod-shim-install-global disable
if grep -Fx "$lib" "$preserve_preload"; then
    echo "library still present in preserved preload file after disable" >&2
    exit 1
fi
grep -Fx "/opt/other-one.so" "$preserve_preload"
grep -Fx "/opt/other-two.so" "$preserve_preload"

newline_root="$PWD/tmp/global/newline"
newline_preload="$newline_root/etc/ld.so.preload"
mkdir -p "$newline_root/etc"
printf '%s' "/opt/existing.so" >"$newline_preload"
RUNPOD_SHIM_GLOBAL_ROOT="$newline_root" RUNPOD_SHIM_GLOBAL_LIB="$lib" tools/runpod-shim-install-global enable
grep -Fx "/opt/existing.so" "$newline_preload"
grep -Fx "$lib" "$newline_preload"

install_root="$PWD/tmp/global/install-root"
installed_state="$PWD/tmp/global/installed-state"
make DESTDIR="$install_root" install
RUNPOD_SHIM_GLOBAL_ROOT="$installed_state" "$install_root/usr/local/bin/runpod-shim-install-global" enable
grep -Fx "/usr/local/lib/runpod-shim/librunpod-shim.so" "$installed_state/etc/ld.so.preload"
if grep -Fx "/usr/lib/runpod-shim/librunpod-shim.so" "$installed_state/etc/ld.so.preload"; then
    echo "installed helper used source default path" >&2
    exit 1
fi
