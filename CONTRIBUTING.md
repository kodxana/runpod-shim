# Contributing

Thanks for helping improve Runpod Shim. This project is intentionally small and
operational: changes should make container behavior safer, clearer, or easier
to validate on real Runpod pods.

## Development

Build and run the local test suite before sending changes:

```sh
make clean
make test
```

For release-style Debian packages, build inside the compatibility container:

```sh
make package-deb-compatible
```

That target uses `ubuntu:20.04` by default and rejects packages that import
glibc 2.38 or newer symbols.

## Style

- Use Runpod casing in prose.
- Keep runtime fixes enabled by default and expose environment variables only
  for opt-outs or diagnostics.
- Prefer narrow hooks with fail-open behavior. If the shim cannot build a
  trustworthy pod-scoped view, it should pass through the original host value.
- Add tests for new intercepted APIs, packaging behavior, and documentation
  contracts.
- Keep public commands and environment variables documented in `README.md` or
  `docs/`.

## Reports

Bug reports are most useful when they include:

- Runpod pod image/base OS.
- GPU type and driver/runtime details when relevant.
- The exact command that behaves incorrectly.
- `runpod-shim-probe` output.
- Whether the issue changes with the narrowest relevant opt-out, such as
  `RUNPOD_SHIM_CGROUP_MEMORY=0` or `RUNPOD_SHIM_GPU_IOCTL=0`.
