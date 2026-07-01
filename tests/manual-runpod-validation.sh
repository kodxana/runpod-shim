#!/bin/sh
set -u

script_dir="$(CDPATH= cd "$(dirname "$0")" && pwd)"
repo_root="$(CDPATH= cd "$script_dir/.." && pwd)"

section() {
    printf '\n== %s ==\n' "$1"
}

find_tool() {
    name="$1"
    source_path="$2"

    if command -v "$name" >/dev/null 2>&1; then
        command -v "$name"
    elif [ -x "$source_path" ]; then
        printf '%s\n' "$source_path"
    else
        return 1
    fi
}

run_probe() {
    section "Probe"

    if probe_cmd="$(find_tool runpod-shim-probe "$repo_root/tools/runpod-shim-probe")"; then
        printf 'using %s\n' "$probe_cmd"
        "$probe_cmd" 2>&1 || printf 'probe failed; continuing manual validation\n'
    else
        printf 'runpod-shim-probe not found; install runpod-shim or run from the source tree\n'
    fi
}

run_meminfo_fallback() {
    if [ -n "$shim_cmd" ]; then
        "$shim_cmd" -- sh -c "grep -E '^(MemTotal|MemAvailable|MemFree):' /proc/meminfo" 2>&1 ||
            printf 'meminfo fallback failed under runpod-shim\n'
    else
        grep -E '^(MemTotal|MemAvailable|MemFree):' /proc/meminfo 2>&1 ||
            printf 'meminfo fallback failed\n'
    fi
}

run_memory_check() {
    section "Memory Under Shim"

    if [ -z "$shim_cmd" ]; then
        printf 'runpod-shim not found; showing direct /proc/meminfo fallback instead\n'
        run_meminfo_fallback
        return
    fi

    printf 'using %s\n' "$shim_cmd"

    if ! command -v python3 >/dev/null 2>&1; then
        printf 'python3 not found; using /proc/meminfo fallback\n'
        run_meminfo_fallback
        return
    fi

    "$shim_cmd" -- python3 - <<'PY'
try:
    import psutil
except Exception as exc:
    print("psutil unavailable: %s" % exc)
    raise SystemExit(42)

vm = psutil.virtual_memory()
swap = psutil.swap_memory()
gib = 1024 ** 3
print("virtual_memory.total: %d bytes (%.2f GiB)" % (vm.total, vm.total / gib))
print("virtual_memory.available: %d bytes (%.2f GiB)" % (vm.available, vm.available / gib))
print("swap.total: %d bytes (%.2f GiB)" % (swap.total, swap.total / gib))
PY
    status=$?

    if [ "$status" -eq 42 ]; then
        printf 'using /proc/meminfo fallback under runpod-shim\n'
        run_meminfo_fallback
    elif [ "$status" -ne 0 ]; then
        printf 'python memory check failed with status %s; using /proc/meminfo fallback\n' "$status"
        run_meminfo_fallback
    fi
}

run_ffmpeg_smoke() {
    section "FFmpeg NVENC Smoke"

    if ! command -v ffmpeg >/dev/null 2>&1; then
        printf 'ffmpeg not found; skipping NVENC smoke\n'
        return
    fi

    if [ -z "$shim_cmd" ]; then
        printf 'runpod-shim not found; skipping shimmed FFmpeg smoke\n'
        return
    fi

    printf 'using %s\n' "$shim_cmd"
    "$shim_cmd" -- ffmpeg -hide_banner -loglevel warning \
        -f lavfi -i testsrc=duration=2:size=1280x720:rate=30 \
        -c:v h264_nvenc -f null - </dev/null 2>&1 ||
        printf 'FFmpeg NVENC smoke failed or NVENC is unavailable on this pod\n'
}

printf 'Runpod Shim Manual Validation\n'
printf 'Best-effort checks for a real Runpod pod; skipped or failed optional checks do not stop the script.\n'

if shim_cmd="$(find_tool runpod-shim "$repo_root/tools/runpod-shim")"; then
    :
else
    shim_cmd=""
fi

run_probe
run_memory_check
run_ffmpeg_smoke

section "Summary"
printf 'Manual validation finished. Review any skipped or failed sections above.\n'
