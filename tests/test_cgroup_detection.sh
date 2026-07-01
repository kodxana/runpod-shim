#!/bin/sh
set -eu

make tests/fixtures/cgroup_probe.exe
rm -rf tmp/cgroup-test
mkdir -p tmp/cgroup-test/proc tmp/cgroup-test/cgroup/pod tmp/cgroup-test/cgroup/memory/docker/pod tmp/cgroup-test/cgroup/memory/pod

cat >tmp/cgroup-test/proc/mountinfo <<'DATA'
10 9 0:1 / /sys/fs/cgroup rw - cgroup2 cgroup rw
DATA
cat >tmp/cgroup-test/proc/cgroup <<'DATA'
0::/pod
DATA
printf '1073741824\n' >tmp/cgroup-test/cgroup/pod/memory.max
printf '268435456\n' >tmp/cgroup-test/cgroup/pod/memory.current

RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/v2.out
grep -F 'version=2 limit=1073741824 current=268435456' tmp/cgroup-test/v2.out

printf -- '-1\n' >tmp/cgroup-test/cgroup/pod/memory.current
RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/v2-negative-current.out
grep -F 'unlimited' tmp/cgroup-test/v2-negative-current.out
printf '268435456\n' >tmp/cgroup-test/cgroup/pod/memory.current

cat >tmp/cgroup-test/proc/mountinfo <<'DATA'
20 9 0:2 / /sys/fs/cgroup/memory rw - cgroup cgroup rw,memory
DATA
cat >tmp/cgroup-test/proc/cgroup <<'DATA'
5:memory:/docker/pod
DATA
printf '2147483648\n' >tmp/cgroup-test/cgroup/memory/docker/pod/memory.limit_in_bytes
printf '536870912\n' >tmp/cgroup-test/cgroup/memory/docker/pod/memory.usage_in_bytes

RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/v1.out
grep -F 'version=1 limit=2147483648 current=536870912' tmp/cgroup-test/v1.out

printf '9223372036854771712\n' >tmp/cgroup-test/cgroup/memory/docker/pod/memory.limit_in_bytes
RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/v1-sentinel.out
grep -F 'unlimited' tmp/cgroup-test/v1-sentinel.out

printf '0\n' >tmp/cgroup-test/cgroup/memory/docker/pod/memory.limit_in_bytes
RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/v1-zero.out
grep -F 'unlimited' tmp/cgroup-test/v1-zero.out

printf -- '-1\n' >tmp/cgroup-test/cgroup/memory/docker/pod/memory.limit_in_bytes
RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/v1-negative.out
grep -F 'unlimited' tmp/cgroup-test/v1-negative.out

cat >tmp/cgroup-test/proc/mountinfo <<'DATA'
20 9 0:2 /docker/pod /sys/fs/cgroup/memory rw - cgroup cgroup rw,memory
DATA
cat >tmp/cgroup-test/proc/cgroup <<'DATA'
5:memory:/docker/pod
DATA
printf '2684354560\n' >tmp/cgroup-test/cgroup/memory/memory.limit_in_bytes
printf '671088640\n' >tmp/cgroup-test/cgroup/memory/memory.usage_in_bytes

RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/v1-mount-root-exact.out
grep -F 'version=1 limit=2684354560 current=671088640' tmp/cgroup-test/v1-mount-root-exact.out

cat >tmp/cgroup-test/proc/mountinfo <<'DATA'
20 9 0:2 /docker /sys/fs/cgroup/memory rw - cgroup cgroup rw,memory
DATA
cat >tmp/cgroup-test/proc/cgroup <<'DATA'
5:memory:/docker/pod
DATA
printf '3758096384\n' >tmp/cgroup-test/cgroup/memory/pod/memory.limit_in_bytes
printf '943718400\n' >tmp/cgroup-test/cgroup/memory/pod/memory.usage_in_bytes

RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/v1-mount-root-subtree.out
grep -F 'version=1 limit=3758096384 current=943718400' tmp/cgroup-test/v1-mount-root-subtree.out

printf '2147483648\n' >tmp/cgroup-test/cgroup/memory/docker/pod/memory.limit_in_bytes
cat >tmp/cgroup-test/proc/cgroup <<'DATA'
0::/pod
5:memory:/docker/pod
DATA
cat >tmp/cgroup-test/proc/mountinfo <<'DATA'
10 9 0:1 / /sys/fs/cgroup rw - cgroup2 cgroup rw
20 9 0:2 / /sys/fs/cgroup/memory rw - cgroup cgroup rw,memory
DATA
printf '3221225472\n' >tmp/cgroup-test/cgroup/pod/memory.max
printf '805306368\n' >tmp/cgroup-test/cgroup/pod/memory.current

RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/hybrid.out
grep -F 'version=1 limit=2147483648 current=536870912' tmp/cgroup-test/hybrid.out

printf 'max\n' >tmp/cgroup-test/cgroup/pod/memory.max
cat >tmp/cgroup-test/proc/mountinfo <<'DATA'
10 9 0:1 / /sys/fs/cgroup rw - cgroup2 cgroup rw
DATA
cat >tmp/cgroup-test/proc/cgroup <<'DATA'
0::/pod
DATA
RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/unlimited.out
grep -F 'unlimited' tmp/cgroup-test/unlimited.out

rm -f tmp/cgroup-test/cgroup/pod/memory.max
RUNPOD_SHIM_TEST_PROC_ROOT="$PWD/tmp/cgroup-test/proc" \
RUNPOD_SHIM_TEST_CGROUP_ROOT="$PWD/tmp/cgroup-test/cgroup" \
tests/fixtures/cgroup_probe.exe >tmp/cgroup-test/v2-missing-limit.out
grep -F 'unlimited' tmp/cgroup-test/v2-missing-limit.out
