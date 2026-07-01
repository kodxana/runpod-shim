#!/bin/sh
set -eu

if ! command -v objdump >/dev/null 2>&1; then
    echo "skip: objdump not available"
    exit 0
fi

make

symbols="tmp/glibc-symbols.out"
mkdir -p tmp
objdump -T librunpod-shim.so >"$symbols"

if grep -F "__isoc23_strtoull" "$symbols"; then
    echo "librunpod-shim.so imports glibc 2.38 C23 strtoull symbol" >&2
    exit 1
fi
