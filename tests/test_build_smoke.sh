#!/bin/sh
set -eu

make clean
test ! -e src/core/config.d
test ! -e src/core/log.d
test ! -e src/preload.d
make
test -f librunpod-shim.so
file librunpod-shim.so | grep -E 'shared object|dynamically linked'
test -f src/core/config.d
test -f src/core/log.d
test -f src/preload.d

mkdir -p tmp
LD_PRELOAD="$PWD/librunpod-shim.so" /bin/true >tmp/smoke.stdout 2>tmp/smoke.stderr
test ! -s tmp/smoke.stdout
test ! -s tmp/smoke.stderr

RUNPOD_SHIM_TRACE=1 LD_PRELOAD="$PWD/librunpod-shim.so" /bin/true >tmp/smoke.stdout 2>tmp/smoke.stderr
test ! -s tmp/smoke.stdout
grep -Fq '[runpod-shim] loaded gpu_video=1 gpu_ioctl=1 cgroup_memory=1' tmp/smoke.stderr
