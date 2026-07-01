# Runpod Shim

[![CI](https://github.com/kodxana/runpod-shim/actions/workflows/ci.yml/badge.svg)](https://github.com/kodxana/runpod-shim/actions/workflows/ci.yml)

Runpod Shim is an all-in-one Linux `LD_PRELOAD` compatibility layer for
Runpod containers. It makes common container-visible hardware mismatches behave
like the pod allocation:

- NVIDIA video userspace sees only the mounted `/dev/nvidiaN` devices.
- Memory APIs report the pod cgroup memory limit instead of host RAM.

Features are enabled by default. Environment variables are for opt-outs,
custom library paths, and tracing.

## Status

Runpod Shim is an early public release for Linux containers. It is designed for
Runpod pod workflows where applications accidentally inspect host-level memory
or GPU topology. The Debian package is the recommended distribution artifact
for Ubuntu-based images.

## Quick Start

Build the shim:

```sh
make
```

Run one command with the shim active:

```sh
tools/runpod-shim -- python app.py
```

Inspect the current container view:

```sh
tools/runpod-shim-probe
```

Install into a filesystem prefix:

```sh
make install PREFIX=/usr DESTDIR=/tmp/runpod-shim-root
```

Build a Debian package:

```sh
make package-deb
```

Build a release-style package that works on older Ubuntu-based images:

```sh
make package-deb-compatible
```

## Default Behavior

Runpod Shim currently has two runtime modules:

- GPU video compatibility ports the Runpod-focused `nvscope` NVIDIA topology
  fix. It filters NVIDIA procfs/topology and selected NVIDIA RM ioctl responses
  so video userspace sees only GPUs that are mounted into the container.
- Cgroup memory compatibility discovers cgroup v1, cgroup v2, and hybrid
  memory limits. When a finite limit is confidently discovered, it corrects
  `sysinfo(2)`, `sysconf(_SC_PHYS_PAGES)`,
  `sysconf(_SC_AVPHYS_PAGES)`, and `/proc/meminfo`.

If a limit or topology cannot be read safely, the shim passes through the host
view instead of inventing values.

## Runtime Environment

All modules default to enabled:

```sh
RUNPOD_SHIM_GPU_VIDEO=0       # disable NVIDIA topology/procfs filtering
RUNPOD_SHIM_GPU_IOCTL=0       # disable NVIDIA RM ioctl response filtering
RUNPOD_SHIM_CGROUP_MEMORY=0   # disable cgroup-aware memory reporting
RUNPOD_SHIM_TRACE=1           # write trace logs to stderr
RUNPOD_SHIM_LIB=/custom/path/librunpod-shim.so
```

The wrapper exposes the same opt-outs as flags:

```sh
tools/runpod-shim --disable-cgroup-memory -- command
tools/runpod-shim --disable-gpu-video -- command
tools/runpod-shim --disable-gpu-ioctl -- command
tools/runpod-shim --trace -- command
tools/runpod-shim --lib /custom/path/librunpod-shim.so -- command
```

## Activation Tools

- `tools/runpod-shim` starts a single command with `LD_PRELOAD` set.
- `tools/runpod-shim-probe` prints cgroup, memory, NVIDIA device, procfs, and
  encoder diagnostics.
- `tools/runpod-shim-install-global enable|disable|status` manages global
  preload activation through `/etc/ld.so.preload`.

Global mode affects every dynamically linked process that honors
`/etc/ld.so.preload`. Package installation does not enable global mode
automatically; enable it explicitly only in images or containers where that
system-wide behavior is intended.

For Runpod image entrypoints, enable global mode near the start of the
entrypoint, after package installation and before launching user-facing
services:

```sh
if command -v runpod-shim-install-global >/dev/null 2>&1; then
    runpod-shim-install-global enable
fi
```

## Packaging

`make package-deb` builds `librunpod-shim.so` and writes a `.deb` under
`build/` using `dpkg-deb`. The package builder detects the native Debian
architecture and supports `RUNPOD_SHIM_DEB_ARCH` for explicit overrides.

Use `make package-deb-compatible` for release artifacts. It builds in
`ubuntu:20.04` by default and rejects packages that import glibc 2.38 or newer
symbols.

Install a package inside an image or pod with:

```sh
apt install ./runpod-shim_0.1.0_amd64.deb
```

Then validate the memory view:

```sh
python3 - <<'PY'
import psutil
print(psutil.virtual_memory())
PY

grep -E '^(MemTotal|MemAvailable|MemFree):' /proc/meminfo
```

## Documentation

- [Safety model](docs/safety-model.md)
- [Cgroup memory](docs/cgroup-memory.md)
- [GPU video](docs/gpu-video.md)
- [Packaging](docs/packaging.md)
- [Troubleshooting](docs/troubleshooting.md)

## Contributing And Security

- [Contributing](CONTRIBUTING.md)
- [Changelog](CHANGELOG.md)
- [Security policy](SECURITY.md)
- [License](LICENSE)
