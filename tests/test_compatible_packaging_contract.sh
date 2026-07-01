#!/bin/sh
set -eu

test -f packaging/build-compatible-deb-docker.sh
grep -F "package-deb-compatible" Makefile
grep -F "ubuntu:20.04" packaging/build-compatible-deb-docker.sh
grep -F "GLIBC_2\\\\.(3[89]|[4-9][0-9])" packaging/build-compatible-deb-docker.sh
grep -F "make package-deb-compatible" docs/packaging.md
