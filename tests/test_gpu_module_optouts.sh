#!/bin/sh
set -eu

make

mkdir -p tmp

RUNPOD_SHIM_TRACE=1 LD_PRELOAD="$PWD/librunpod-shim.so" /bin/true 2>tmp/gpu.default
grep -F 'gpu_video=1 gpu_ioctl=1' tmp/gpu.default

RUNPOD_SHIM_TRACE=1 RUNPOD_SHIM_GPU_VIDEO=0 LD_PRELOAD="$PWD/librunpod-shim.so" /bin/true 2>tmp/gpu.disabled
grep -F 'gpu_video=0' tmp/gpu.disabled

RUNPOD_SHIM_TRACE=1 RUNPOD_SHIM_GPU_IOCTL=0 LD_PRELOAD="$PWD/librunpod-shim.so" /bin/true 2>tmp/gpu.noioctl
grep -F 'gpu_ioctl=0' tmp/gpu.noioctl
