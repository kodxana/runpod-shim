#!/bin/sh
set -eu

require_file() {
    if [ ! -f "$1" ]; then
        echo "missing required documentation file: $1" >&2
        exit 1
    fi
}

require_text() {
    file="$1"
    text="$2"

    if ! grep -F "$text" "$file" >/dev/null 2>&1; then
        echo "missing required text in $file: $text" >&2
        exit 1
    fi
}

require_file README.md
require_file CHANGELOG.md
require_file CONTRIBUTING.md
require_file LICENSE
require_file SECURITY.md
require_file .github/workflows/ci.yml
require_file .github/workflows/release.yml
require_file docs/safety-model.md
require_file docs/cgroup-memory.md
require_file docs/gpu-video.md
require_file docs/packaging.md
require_file docs/troubleshooting.md

bad_runpod='Run''Pod'
bad_runpod_lower='run''Pod'
bad_runpods='Run''Pods'
bad_runpods_lower='run''Pods'
if grep -R -n \
    -e "$bad_runpod" \
    -e "$bad_runpod_lower" \
    -e "$bad_runpods" \
    -e "$bad_runpods_lower" \
    README.md CHANGELOG.md CONTRIBUTING.md SECURITY.md docs packaging tools tests .github; then
    echo "use Runpod casing in prose" >&2
    exit 1
fi

require_text README.md "RUNPOD_SHIM_CGROUP_MEMORY=0"
require_text README.md "/etc/ld.so.preload"
require_text README.md "make package-deb-compatible"
require_text CHANGELOG.md "0.1.0"
require_text CONTRIBUTING.md "Use Runpod casing in prose"
require_text SECURITY.md "not a security boundary"
require_text docs/cgroup-memory.md "cgroup v1"
require_text docs/cgroup-memory.md "cgroup v2"
require_text docs/gpu-video.md "nvscope"
require_text docs/packaging.md "Release Checklist"
require_text docs/packaging.md "GitHub Release Workflow"
require_text docs/packaging.md "does not run make test"
require_text .github/workflows/ci.yml "name: Build"
require_text docs/troubleshooting.md "runpod-shim-probe"
require_text .github/workflows/release.yml "workflow_dispatch"
require_text .github/workflows/release.yml "contents: write"
require_text .github/workflows/release.yml "make package-deb-compatible"
require_text .github/workflows/release.yml "gh \"\${args[@]}\""

if grep -F "make test" .github/workflows/ci.yml; then
    echo "GitHub Build workflow should only build packages; do not run tests there" >&2
    exit 1
fi
