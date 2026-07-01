# GPU Video

The GPU video module ports the Runpod-focused `nvscope` NVIDIA topology fix
into the all-in-one shim. It helps video stacks such as FFmpeg/NVENC see the
same GPU set that the container actually has mounted.

## Problem

Runpod containers may mount only a subset of `/dev/nvidiaN` devices while
NVIDIA procfs and driver queries still expose host-global topology. Some video
userspace follows that topology, chooses a GPU that is not mounted into the
container, and then fails later with confusing encoder or device errors.

## What Is Filtered

At startup, the shim reads mounted NVIDIA minors from `/dev` and matches them
to `/proc/driver/nvidia/gpus/*/information` entries. When it has a valid
allowed set, it filters:

- `/proc/driver/nvidia/gpus` directory entries
- opens and stats for hidden GPU procfs paths
- relative procfs walks below an already opened NVIDIA procfs directory
- `getdents64`, `readdir`, and `readdir64` results for NVIDIA GPU procfs
- selected NVIDIA RM ioctl responses from `/dev/nvidiactl`, including card
  info, attached GPU IDs, and active device IDs

The module does not create device nodes or grant access to GPUs. It only hides
host GPUs that are not mounted into the container.

## Opt-Outs

Disable all GPU video filtering:

```sh
RUNPOD_SHIM_GPU_VIDEO=0 tools/runpod-shim -- command
```

Disable only NVIDIA RM ioctl filtering while keeping procfs/topology filtering:

```sh
RUNPOD_SHIM_GPU_IOCTL=0 tools/runpod-shim -- command
```

If topology discovery fails, the module passes through the original host view.
Use `RUNPOD_SHIM_TRACE=1` to see discovery and filtering decisions.

## Verify

Start with:

```sh
tools/runpod-shim-probe
```

Useful manual checks:

```sh
tools/runpod-shim -- sh -c 'ls -l /dev/nvidia* /dev/nvidia-caps 2>/dev/null'
tools/runpod-shim -- sh -c 'find /proc/driver/nvidia/gpus -maxdepth 2 -type f -name information -print'
tools/runpod-shim -- ffmpeg -hide_banner -encoders
```

When reporting an issue, include the mounted `/dev/nvidia*` list, the probe
output, and whether `RUNPOD_SHIM_GPU_IOCTL=0` changes the result.
