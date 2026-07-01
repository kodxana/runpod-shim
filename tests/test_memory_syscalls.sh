#!/bin/sh
set -eu

make librunpod-shim.so tests/fixtures/memory_probe.exe
rm -rf tmp/memory-syscalls
mkdir -p tmp/memory-syscalls/proc tmp/memory-syscalls/cgroup/pod

cat >tmp/memory-syscalls/proc/mountinfo <<'DATA'
10 9 0:1 / /sys/fs/cgroup rw - cgroup2 cgroup rw
DATA
cat >tmp/memory-syscalls/proc/cgroup <<'DATA'
0::/pod
DATA
printf '1073741824\n' >tmp/memory-syscalls/cgroup/pod/memory.max
printf '268435456\n' >tmp/memory-syscalls/cgroup/pod/memory.current

RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/memory-syscalls/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/memory-syscalls/cgroup" \
LD_PRELOAD="$PWD/librunpod-shim.so" \
tests/fixtures/memory_probe.exe >tmp/memory-syscalls/out

grep -F 'totalram=1073741824' tmp/memory-syscalls/out
grep -F 'freeram=805306368' tmp/memory-syscalls/out
grep -F 'pages=262144' tmp/memory-syscalls/out
grep -F 'avail=196608' tmp/memory-syscalls/out

RUNPOD_SHIM_CGROUP_MEMORY=0 \
RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/memory-syscalls/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/memory-syscalls/cgroup" \
LD_PRELOAD="$PWD/librunpod-shim.so" \
tests/fixtures/memory_probe.exe >tmp/memory-syscalls/disabled

if grep -F 'totalram=1073741824' tmp/memory-syscalls/disabled; then
    echo "disabled memory module still corrected totalram" >&2
    exit 1
fi
if grep -F 'pages=262144' tmp/memory-syscalls/disabled; then
    echo "disabled memory module still corrected pages" >&2
    exit 1
fi
if grep -F 'avail=196608' tmp/memory-syscalls/disabled; then
    echo "disabled memory module still corrected available pages" >&2
    exit 1
fi
