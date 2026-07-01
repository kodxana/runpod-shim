#!/bin/sh
set -eu

make librunpod-shim.so tests/fixtures/read_meminfo.exe
rm -rf tmp/proc-meminfo
mkdir -p tmp/proc-meminfo/proc tmp/proc-meminfo/cgroup/pod

cat >tmp/proc-meminfo/proc/mountinfo <<'DATA'
10 9 0:1 / /sys/fs/cgroup rw - cgroup2 cgroup rw
DATA
cat >tmp/proc-meminfo/proc/cgroup <<'DATA'
0::/pod
DATA
cat >tmp/proc-meminfo/proc/meminfo <<'DATA'
MemTotal:       999999999 kB
MemFree:        888888888 kB
MemAvailable:   777777777 kB
Buffers:             1000 kB
Cached:              2000 kB
DATA
printf '1073741824\n' >tmp/proc-meminfo/cgroup/pod/memory.max
printf '268435456\n' >tmp/proc-meminfo/cgroup/pod/memory.current
printf 'real descriptor\n' >tmp/proc-meminfo/real-fd

RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/proc-meminfo/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/proc-meminfo/cgroup" \
LD_PRELOAD="$PWD/librunpod-shim.so" \
tests/fixtures/read_meminfo.exe >tmp/proc-meminfo/out

grep -E '^MemTotal:[[:space:]]+1048576 kB$' tmp/proc-meminfo/out
grep -E '^MemAvailable:[[:space:]]+786432 kB$' tmp/proc-meminfo/out
grep -E '^MemFree:[[:space:]]+786432 kB$' tmp/proc-meminfo/out
grep -F 'Buffers:             1000 kB' tmp/proc-meminfo/out

RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/proc-meminfo/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/proc-meminfo/cgroup" \
LD_PRELOAD="$PWD/librunpod-shim.so" \
tests/fixtures/read_meminfo.exe fopen >tmp/proc-meminfo/fopen

grep -E '^MemTotal:[[:space:]]+1048576 kB$' tmp/proc-meminfo/fopen
grep -E '^MemAvailable:[[:space:]]+786432 kB$' tmp/proc-meminfo/fopen
grep -E '^MemFree:[[:space:]]+786432 kB$' tmp/proc-meminfo/fopen

RUNPOD_SHIM_CGROUP_MEMORY=0 \
RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/proc-meminfo/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/proc-meminfo/cgroup" \
LD_PRELOAD="$PWD/librunpod-shim.so" \
tests/fixtures/read_meminfo.exe >tmp/proc-meminfo/disabled

grep -E '^MemTotal:' tmp/proc-meminfo/disabled
if grep -E '^MemTotal:[[:space:]]+1048576 kB$' tmp/proc-meminfo/disabled ||
   grep -E '^MemAvailable:[[:space:]]+786432 kB$' tmp/proc-meminfo/disabled ||
   grep -E '^MemFree:[[:space:]]+786432 kB$' tmp/proc-meminfo/disabled; then
    echo "disabled mode returned cgroup-rewritten meminfo" >&2
    exit 1
fi
if grep -F '999999999' tmp/proc-meminfo/disabled; then
    echo "disabled mode used fake proc root meminfo" >&2
    exit 1
fi

RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/proc-meminfo/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/proc-meminfo/cgroup" \
LD_PRELOAD="$PWD/librunpod-shim.so" \
tests/fixtures/read_meminfo.exe dup2-stale "$PWD/tmp/proc-meminfo/real-fd" >tmp/proc-meminfo/dup2
grep -Fx 'real descriptor' tmp/proc-meminfo/dup2

RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/proc-meminfo/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/proc-meminfo/cgroup" \
LD_PRELOAD="$PWD/librunpod-shim.so" \
tests/fixtures/read_meminfo.exe dup3-stale "$PWD/tmp/proc-meminfo/real-fd" >tmp/proc-meminfo/dup3
grep -Fx 'real descriptor' tmp/proc-meminfo/dup3

tests/fixtures/read_meminfo.exe fd-map-zero >tmp/proc-meminfo/fd-map-zero
grep -Fx 'zero-ok' tmp/proc-meminfo/fd-map-zero
