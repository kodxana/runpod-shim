# Changelog

All notable changes to Runpod Shim are documented here.

## 0.1.0 - 2026-07-01

### Added

- Added an `LD_PRELOAD` shim with all runtime modules enabled by default.
- Added cgroup-aware memory reporting for cgroup v1, cgroup v2, and hybrid
  layouts.
- Added `/proc/meminfo`, `sysinfo(2)`, and `sysconf(3)` memory correction so
  common Python, shell, and monitoring tools see pod-sized memory.
- Added NVIDIA video topology filtering and selected NVIDIA RM ioctl response
  filtering for FFmpeg/NVENC-style workflows.
- Added `runpod-shim`, `runpod-shim-probe`, and
  `runpod-shim-install-global`.
- Added Debian package generation and Ubuntu 20.04-compatible release builds.

### Fixed

- Avoided newer glibc symbol imports so compatible packages work on older
  Ubuntu-based Runpod images.
- Added stdio `/proc/meminfo` handling so tools such as `htop` see the cgroup
  memory limit.

### Notes

- Package installation does not enable global preload mode. Use
  `runpod-shim-install-global enable` explicitly when an image should preload
  the shim for every dynamically linked process.
