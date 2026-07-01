# Cgroup Memory

The cgroup memory module makes common memory APIs report the pod memory limit
instead of host RAM. It is enabled by default and can be disabled with
`RUNPOD_SHIM_CGROUP_MEMORY=0`.

## What Is Corrected

When a finite cgroup limit is discovered, the shim corrects:

- `sysinfo(2)` `totalram`, `freeram`, and `mem_unit`
- `sysconf(_SC_PHYS_PAGES)`
- `sysconf(_SC_AVPHYS_PAGES)`
- `/proc/meminfo` lines for `MemTotal`, `MemAvailable`, and `MemFree`

Other `/proc/meminfo` lines are preserved from the real file.

## Discovery

The shim reads `/proc/self/mountinfo` and `/proc/self/cgroup` for the current
process, then checks memory controller files:

- cgroup v1 uses `memory.limit_in_bytes` and `memory.usage_in_bytes`.
- cgroup v2 uses `memory.max` and `memory.current`.
- Hybrid systems may expose both. The shim considers valid candidates and uses
  the smallest finite limit, which is the most restrictive container view.

The path logic accounts for cgroup mount roots, nested cgroup paths, and the
common `/sys/fs/cgroup` layout.

## Pass-Through Cases

The module keeps host values when the limit is unlimited, unreadable, missing,
malformed, zero, or uses a kernel sentinel value that means "effectively
unlimited." It also passes through if the current usage counter cannot be read.

That behavior is intentional: an uncertain cgroup state is safer than reporting
a guessed memory limit.

## Verify

Run:

```sh
tools/runpod-shim-probe
```

For an application-level check:

```sh
tools/runpod-shim -- sh -c 'grep -E "^(MemTotal|MemAvailable|MemFree):" /proc/meminfo'
```

To compare with the unmodified view, run the same command without
`tools/runpod-shim`, or disable the module:

```sh
RUNPOD_SHIM_CGROUP_MEMORY=0 tools/runpod-shim -- command
```
