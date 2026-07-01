# Packaging

Runpod Shim can be installed from the build tree or packaged as a Debian
archive.

## Build And Install

Compile the shared library:

```sh
make
```

Install into a prefix:

```sh
make install PREFIX=/usr DESTDIR=/tmp/runpod-shim-root
```

The install target places:

- `runpod-shim`
- `runpod-shim-probe`
- `runpod-shim-install-global`
- `librunpod-shim.so`
- `README.md`

## Debian Package

Build a `.deb` with:

```sh
make package-deb
```

The package is written under `build/` by `packaging/build-deb.sh` using
`dpkg-deb`. The target first builds `librunpod-shim.so`, then stages the same
files as `make install` under a Debian package root.

For release builds that need to run across older Runpod images, build inside an
older distro image instead of the local host:

```sh
make package-deb-compatible
```

That wrapper uses Docker with `ubuntu:20.04` by default and rejects packages
that import glibc 2.38 or newer symbols. Override the build image when needed:

```sh
RUNPOD_SHIM_BUILD_IMAGE=ubuntu:22.04 make package-deb-compatible
```

Package installation does not enable global preload mode. After installing,
activate per command with `runpod-shim -- command`, or explicitly manage
global mode:

```sh
runpod-shim-install-global status
runpod-shim-install-global enable
runpod-shim-install-global disable
```

## Architecture Detection

The Debian package builder uses the native Debian architecture when possible:

1. `RUNPOD_SHIM_DEB_ARCH`, if set
2. `dpkg --print-architecture`
3. `dpkg-architecture -qDEB_HOST_ARCH`
4. a small `uname -m` fallback map

Use an override when cross-building or when the local architecture cannot be
derived:

```sh
RUNPOD_SHIM_DEB_ARCH=amd64 make package-deb
```

## Release Checklist

Before publishing a release:

1. Update `CHANGELOG.md`.
2. Run `make clean && make test`.
3. Run `make clean && make package-deb-compatible`.
4. Inspect the package contents with `dpkg-deb --contents build/*.deb`.
5. Smoke-test the package on a real Runpod pod with
   `sh tests/manual-runpod-validation.sh`.
6. Confirm global mode only turns on when explicitly requested with
   `runpod-shim-install-global enable`.

## Clean

Remove build products and temporary package output:

```sh
make clean
```
