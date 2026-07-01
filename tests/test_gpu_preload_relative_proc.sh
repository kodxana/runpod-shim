#!/bin/sh
set -eu

root_dir="$(CDPATH= cd "$(dirname "$0")/.." && pwd)"
cd "$root_dir"

mkdir -p build/tests
cat >build/tests/test_gpu_preload_relative_proc.c <<'EOF'
#include "gpu/fd_tracker.h"

#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *rps_gpu_test_effective_openat_path(int dirfd,
                                               const char *path,
                                               char *buffer,
                                               size_t buffer_len);
int rps_gpu_test_readdir_entry_allowed(int dirfd, const char *name);
void rps_gpu_test_set_proc_filter_ready(int ready);
void rps_gpu_test_set_allowed_bus(const char *bus_id);
void rps_gpu_test_track_open(const char *path, int fd);
int rps_gpu_test_proc_denied(const char *path);

static int failures;

static void expect_string(const char *name, const char *got, const char *want) {
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "%s: got '%s' want '%s'\n", name, got, want);
        failures++;
    }
}

static void expect_int(const char *name, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s: got %d want %d\n", name, got, want);
        failures++;
    }
}

static void expect_same_pointer(const char *name, const char *got, const char *want) {
    if (got != want) {
        fprintf(stderr, "%s: got %p want %p\n", name, (const void *)got, (const void *)want);
        failures++;
    }
}

static void expect_tracked_kind(const char *name, int fd, enum rps_gpu_fd_kind want) {
    struct rps_gpu_fd_record got;

    if (!rps_gpu_fd_map_get(fd, &got)) {
        fprintf(stderr, "%s: fd %d was not tracked\n", name, fd);
        failures++;
        return;
    }
    expect_int(name, got.kind, want);
}

static void expect_untracked(const char *name, int fd) {
    struct rps_gpu_fd_record got;

    expect_int(name, rps_gpu_fd_map_get(fd, &got), 0);
}

static void test_duplicate_fd_tracking(void) {
    struct rps_gpu_fd_record ctl = {0};
    int base;
    int copied;

    base = open("/dev/null", O_RDONLY);
    if (base < 0) {
        perror("open /dev/null");
        failures++;
        return;
    }

    rps_gpu_classify_path("/dev/nvidiactl", &ctl);
    rps_gpu_fd_map_set(base, &ctl);

    copied = dup(base);
    if (copied < 0) {
        perror("dup");
        failures++;
    } else {
        expect_tracked_kind("dup copied gpu fd tracking", copied, RPS_GPU_FD_NVIDIA_CTL);
        close(copied);
    }

    copied = fcntl(base, F_DUPFD, 0);
    if (copied < 0) {
        perror("fcntl F_DUPFD");
        failures++;
    } else {
        expect_tracked_kind("F_DUPFD copied gpu fd tracking", copied, RPS_GPU_FD_NVIDIA_CTL);
        close(copied);
    }

#ifdef F_DUPFD_CLOEXEC
    copied = fcntl(base, F_DUPFD_CLOEXEC, 0);
    if (copied < 0) {
        perror("fcntl F_DUPFD_CLOEXEC");
        failures++;
    } else {
        expect_tracked_kind("F_DUPFD_CLOEXEC copied gpu fd tracking", copied, RPS_GPU_FD_NVIDIA_CTL);
        close(copied);
    }
#endif

    close(base);
}

static void test_closedir_clears_gpu_fd_tracking(void) {
    struct rps_gpu_fd_record ctl = {0};
    DIR *dir;
    int fd;

    dir = opendir(".");
    if (dir == NULL) {
        perror("opendir .");
        failures++;
        return;
    }

    fd = dirfd(dir);
    rps_gpu_classify_path("/dev/nvidiactl", &ctl);
    rps_gpu_fd_map_set(fd, &ctl);
    closedir(dir);

    expect_untracked("closedir cleared gpu fd tracking", fd);
}

int main(void) {
    struct rps_gpu_fd_record proc_gpus = {0};
    struct rps_gpu_fd_record got = {0};
    char buffer[128];
    char long_dotted[768];
    size_t long_used;
    size_t i;
    const char *relative = "0000:05:00.0/information";
    const char *leading_dot = "./0000:05:00.0/information";
    const char *middle_dot = "0000:05:00.0/./information";
    const char *parent_path = "0000:04:00.0/../0000:05:00.0/information";
    const char *absolute = "/proc/driver/nvidia/gpus/0000:05:00.0/information";
    const char *unknown = "0000:06:00.0/information";

    rps_gpu_fd_map_clear();
    proc_gpus.kind = RPS_GPU_FD_PROC_GPUS_DIR;
    rps_gpu_fd_map_set(42, &proc_gpus);

    long_used = 0;
    for (i = 0; i < 260; i++) {
        memcpy(long_dotted + long_used, "./", 2);
        long_used += 2;
    }
    strcpy(long_dotted + long_used, "0000:05:00.0/information");

    expect_string("relative proc info path",
                  rps_gpu_test_effective_openat_path(42, relative, buffer, sizeof(buffer)),
                  "/proc/driver/nvidia/gpus/0000:05:00.0/information");
    expect_string("leading dot proc info path",
                  rps_gpu_test_effective_openat_path(42, leading_dot, buffer, sizeof(buffer)),
                  "/proc/driver/nvidia/gpus/0000:05:00.0/information");
    expect_string("middle dot proc info path",
                  rps_gpu_test_effective_openat_path(42, middle_dot, buffer, sizeof(buffer)),
                  "/proc/driver/nvidia/gpus/0000:05:00.0/information");
    expect_string("parent segment proc info path",
                  rps_gpu_test_effective_openat_path(42, parent_path, buffer, sizeof(buffer)),
                  "/proc/driver/nvidia/gpus/0000:05:00.0/information");
    expect_same_pointer("absolute path unchanged",
                        rps_gpu_test_effective_openat_path(42, absolute, buffer, sizeof(buffer)),
                        absolute);
    expect_same_pointer("unknown dirfd unchanged",
                        rps_gpu_test_effective_openat_path(43, unknown, buffer, sizeof(buffer)),
                        unknown);
    expect_string("long dotted proc info path",
                  rps_gpu_test_effective_openat_path(42, long_dotted, buffer, sizeof(buffer)),
                  "/proc/driver/nvidia/gpus/0000:05:00.0/information");

    rps_gpu_test_set_allowed_bus("0000:05:00.0");
    rps_gpu_test_set_proc_filter_ready(1);
    expect_string("dir dot allowed",
                  rps_gpu_test_readdir_entry_allowed(42, ".") ? "yes" : "no",
                  "yes");
    expect_string("dir dotdot allowed",
                  rps_gpu_test_readdir_entry_allowed(42, "..") ? "yes" : "no",
                  "yes");
    expect_string("dir allowed bus kept",
                  rps_gpu_test_readdir_entry_allowed(42, "0000:05:00.0") ? "yes" : "no",
                  "yes");
    expect_string("dir denied bus skipped",
                  rps_gpu_test_readdir_entry_allowed(42, "0000:06:00.0") ? "yes" : "no",
                  "no");
    expect_string("untracked dirfd pass through",
                  rps_gpu_test_readdir_entry_allowed(43, "0000:06:00.0") ? "yes" : "no",
                  "yes");
    rps_gpu_test_set_proc_filter_ready(0);
    expect_string("filter disabled pass through",
                  rps_gpu_test_readdir_entry_allowed(42, "0000:06:00.0") ? "yes" : "no",
                  "yes");

    rps_gpu_test_set_allowed_bus("0000:04:00.0");
    rps_gpu_test_set_proc_filter_ready(1);
    expect_int("denies dotted denied path",
               rps_gpu_test_proc_denied("/proc/driver/nvidia/gpus/0000:05:00.0/./information"),
               1);
    expect_int("denies doubled slash denied path",
               rps_gpu_test_proc_denied("/proc/driver/nvidia/gpus//0000:05:00.0//information"),
               1);
    expect_int("allows dotted allowed path",
               rps_gpu_test_proc_denied("/proc/driver/nvidia/gpus/0000:04:00.0/./information"),
               0);
    expect_int("denies denied bus dir",
               rps_gpu_test_proc_denied("/proc/driver/nvidia/gpus/0000:05:00.0"),
               1);
    expect_int("allows allowed bus dir",
               rps_gpu_test_proc_denied("/proc/driver/nvidia/gpus/0000:04:00.0"),
               0);

    memset(&proc_gpus, 0, sizeof(proc_gpus));
    proc_gpus.kind = RPS_GPU_FD_PROC_GPU_BUS_DIR;
    strcpy(proc_gpus.bus_id, "0000:05:00.0");
    rps_gpu_fd_map_set(45, &proc_gpus);
    expect_string("bus dir relative information path",
                  rps_gpu_test_effective_openat_path(45, "information", buffer, sizeof(buffer)),
                  "/proc/driver/nvidia/gpus/0000:05:00.0/information");
    expect_string("bus dir parent path",
                  rps_gpu_test_effective_openat_path(45, "../0000:04:00.0/information", buffer, sizeof(buffer)),
                  "/proc/driver/nvidia/gpus/0000:04:00.0/information");

    rps_gpu_fd_map_set(44, &proc_gpus);
    rps_gpu_test_track_open("/tmp/not-nvidia", 44);
    expect_int("unclassified open clears stale gpu fd", rps_gpu_fd_map_get(44, &got), 0);

    test_duplicate_fd_tracking();
    test_closedir_clears_gpu_fd_tracking();

    return failures == 0 ? 0 : 1;
}
EOF

cc -std=c11 -D_GNU_SOURCE -DRUNPOD_SHIM_TESTING -Wall -Wextra -Werror -Isrc \
    build/tests/test_gpu_preload_relative_proc.c \
    src/preload.c \
    src/cgroup/cgroup.c \
    src/core/config.c \
    src/core/fd_map.c \
    src/core/guard.c \
    src/core/log.c \
    src/core/paths.c \
    src/core/real.c \
    src/gpu/fd_tracker.c \
    src/gpu/gpu_video.c \
    src/gpu/ioctl_filter.c \
    src/gpu/proc_filter.c \
    src/memory/memory.c \
    src/memory/proc_meminfo.c \
    -ldl -pthread -o build/tests/test_gpu_preload_relative_proc
build/tests/test_gpu_preload_relative_proc
