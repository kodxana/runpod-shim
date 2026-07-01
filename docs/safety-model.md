# Safety Model

Runpod Shim is designed to fix container-visible mismatches without requiring
application changes. It is conservative by default: modules are enabled, but
each module passes through host behavior when it cannot build a trustworthy
container-scoped view.

## Defaults

- GPU video filtering is enabled unless `RUNPOD_SHIM_GPU_VIDEO=0`.
- NVIDIA RM ioctl filtering is enabled unless `RUNPOD_SHIM_GPU_IOCTL=0`.
- Cgroup memory reporting is enabled unless `RUNPOD_SHIM_CGROUP_MEMORY=0`.
- Trace logging is disabled unless `RUNPOD_SHIM_TRACE=1`.

Opt-outs accept common disabled values such as `0`, `false`, `off`, and `no`.

## Fail-Open Rules

The shim does not fabricate hardware or memory state:

- If cgroup memory limits are missing, unlimited, malformed, unreadable, or
  ambiguous, memory APIs keep returning the original host values.
- If NVIDIA topology discovery cannot match mounted `/dev/nvidiaN` devices to
  procfs entries, GPU procfs and ioctl responses are left alone.
- If an intercepted call is outside the shim's scope, it is forwarded to the
  real libc or driver call.

This avoids turning an uncertain environment into a hard failure. Use
`RUNPOD_SHIM_TRACE=1` when you need to see why a module passed through.

## Activation Scope

Per-command activation with `tools/runpod-shim -- command` is the safest way to
test and deploy. It sets `LD_PRELOAD` only for the target process tree.

Global activation uses `tools/runpod-shim-install-global enable`, which writes
the shim library path to `/etc/ld.so.preload`. That affects every dynamically
linked process that honors the system preload file, so use it only inside
controlled container images or pods. Package installation does not enable
global mode automatically.

## Operational Guidance

- Prefer wrapper activation while validating a new image.
- Keep `runpod-shim-probe` in bug reports; it captures the cgroup and NVIDIA
  view seen by the process.
- Disable the narrowest module first when bisecting an issue. For example,
  try `RUNPOD_SHIM_GPU_IOCTL=0` before disabling all GPU video filtering.
- Avoid using global mode on shared hosts. The shim is intended for container
  workloads where the image owner controls the process environment.
