#!/bin/sh
set -eu

make
mkdir -p tmp

LD_PRELOAD="$PWD/librunpod-shim.so" /bin/true >tmp/quiet.stdout 2>tmp/quiet.stderr
test ! -s tmp/quiet.stdout
test ! -s tmp/quiet.stderr

RUNPOD_SHIM_TRACE=1 LD_PRELOAD="$PWD/librunpod-shim.so" /bin/true >tmp/quiet.stdout 2>tmp/quiet.stderr
test ! -s tmp/quiet.stdout
grep -F "[runpod-shim] loaded" tmp/quiet.stderr
