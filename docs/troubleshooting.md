# Troubleshooting

Start with the probe:

```sh
tools/runpod-shim-probe
```

It prints active module flags, cgroup mounts, memory summaries, mounted NVIDIA
devices, NVIDIA procfs entries, `nvidia-smi` output when available, and FFmpeg
NVENC encoder visibility.

## The Shim Is Not Loading

Check the wrapper path and library selection:

```sh
make
RUNPOD_SHIM_TRACE=1 tools/runpod-shim -- sh -c 'printf "%s\n" "$LD_PRELOAD"'
```

If the library lives outside the default source or install locations, set:

```sh
RUNPOD_SHIM_LIB=/custom/path/librunpod-shim.so tools/runpod-shim -- command
```

For installed packages, confirm that `runpod-shim` resolves to the expected
binary and that `/usr/lib/runpod-shim/librunpod-shim.so` exists.

## Global Mode Problems

Global mode is controlled by `/etc/ld.so.preload`:

```sh
tools/runpod-shim-install-global status
tools/runpod-shim-install-global disable
```

Package installation does not enable global mode automatically. If a container
starts failing very early after global activation, disable it from a recovery
shell or rebuild the image without the preload entry.

## Memory Looks Like Host RAM

The cgroup memory module passes through host values when it cannot find a
finite, readable memory limit. Check:

```sh
tools/runpod-shim-probe
cat /proc/self/cgroup
grep -E 'cgroup2|memory' /proc/self/mountinfo
```

Compare with the module disabled:

```sh
RUNPOD_SHIM_CGROUP_MEMORY=0 tools/runpod-shim -- command
```

If both views are the same, the process may have an unlimited or unreadable
cgroup limit.

## GPU Video Still Sees Host Devices

Confirm that the intended devices are actually mounted:

```sh
tools/runpod-shim -- sh -c 'ls -l /dev/nvidia* /dev/nvidia-caps 2>/dev/null'
```

Then inspect NVIDIA procfs:

```sh
RUNPOD_SHIM_TRACE=1 tools/runpod-shim -- sh -c 'find /proc/driver/nvidia/gpus -maxdepth 2 -type f -name information -print'
```

If trace says topology detection failed, the shim could not map mounted device
minors to NVIDIA procfs entries and intentionally passed through the host view.

## GPU Application Fails After Filtering

First narrow the scope:

```sh
RUNPOD_SHIM_GPU_IOCTL=0 tools/runpod-shim -- command
RUNPOD_SHIM_GPU_VIDEO=0 tools/runpod-shim -- command
```

If disabling only `RUNPOD_SHIM_GPU_IOCTL` helps, include the failing command,
driver version, and `runpod-shim-probe` output in the report. If disabling
`RUNPOD_SHIM_GPU_VIDEO` is required, include the `/dev/nvidia*` listing and
`/proc/driver/nvidia/gpus/*/information` content visible inside the container.

## Package Build Fails

`make package-deb` requires `dpkg-deb`. Install Debian packaging tools in the
build environment, or run the package step in a Debian-compatible image.

If architecture detection fails, set:

```sh
RUNPOD_SHIM_DEB_ARCH=amd64 make package-deb
```

## Manual Runpod Validation

On a real Runpod pod, run:

```sh
sh tests/manual-runpod-validation.sh
```

The memory section should show pod-sized memory rather than host-sized memory.
The FFmpeg section should complete when NVENC is available to the assigned GPU.
