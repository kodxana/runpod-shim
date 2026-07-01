#include "core/config.h"
#include "core/fd_map.h"
#include "core/guard.h"
#include "core/log.h"
#include "core/real.h"
#include "gpu/fd_tracker.h"
#include "gpu/gpu_video.h"
#include "gpu/ioctl_filter.h"
#include "gpu/proc_filter.h"
#include "memory/memory.h"
#include "memory/proc_meminfo.h"

#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <unistd.h>

#define RPS_MEMINFO_BUFFER_SIZE 65536U
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

static struct rps_gpu_allowed_gpus g_gpu_allowed_gpus;
static int g_gpu_proc_filter_ready;

__attribute__((constructor)) static void runpod_shim_init(void) {
    const struct rps_config *cfg = rps_config_get();
    (void)rps_real_get();
    if (cfg->gpu_video) {
        g_gpu_proc_filter_ready = rps_gpu_proc_filter_runtime_init(NULL, &g_gpu_allowed_gpus);
        if (!g_gpu_proc_filter_ready) {
            rps_trace("gpu proc filter enabled but GPU topology detection failed");
        }
    }
    rps_trace("loaded gpu_video=%d gpu_ioctl=%d cgroup_memory=%d",
              cfg->gpu_video, cfg->gpu_ioctl, cfg->cgroup_memory);
}

static bool encode_sysinfo_memory(const struct rps_memory_snapshot *snapshot,
                                  unsigned long *totalram,
                                  unsigned long *freeram,
                                  unsigned int *mem_unit) {
    uint64_t scale = 1;

    while (snapshot->total / scale > (uint64_t)ULONG_MAX ||
           snapshot->available / scale > (uint64_t)ULONG_MAX) {
        if (scale > (uint64_t)UINT_MAX / 2U) {
            return false;
        }
        scale <<= 1;
    }
    if (scale > (uint64_t)UINT_MAX) {
        return false;
    }

    *totalram = (unsigned long)(snapshot->total / scale);
    *freeram = (unsigned long)(snapshot->available / scale);
    *mem_unit = (unsigned int)scale;
    return true;
}

static bool open_needs_mode(int flags) {
    if ((flags & O_CREAT) != 0) {
        return true;
    }
#ifdef O_TMPFILE
    if ((flags & O_TMPFILE) == O_TMPFILE) {
        return true;
    }
#endif
    return false;
}

static bool should_virtualize_meminfo(const char *path, int flags) {
    if (path == NULL || strcmp(path, "/proc/meminfo") != 0) {
        return false;
    }
    if (!rps_config_get()->cgroup_memory) {
        return false;
    }
    if ((flags & O_ACCMODE) == O_WRONLY) {
        return false;
    }
#ifdef O_PATH
    if ((flags & O_PATH) == O_PATH) {
        return false;
    }
#endif
    return true;
}

static void maybe_attach_meminfo(int fd, const char *path, int flags) {
    char meminfo[RPS_MEMINFO_BUFFER_SIZE];
    size_t len;

    if (fd < 0 || !should_virtualize_meminfo(path, flags)) {
        return;
    }
    if (!rps_guard_enter()) {
        rps_trace("meminfo attach skipped by guard");
        return;
    }
    if (rps_proc_meminfo_build(meminfo, sizeof(meminfo), &len)) {
        rps_trace("meminfo attach fd=%d len=%llu", fd, (unsigned long long)len);
        (void)rps_fd_map_put(fd, meminfo, len);
    } else {
        rps_trace("meminfo build failed");
    }
    rps_guard_leave();
}

static bool meminfo_mode_is_read_only(const char *mode) {
    return mode != NULL && mode[0] == 'r' && strchr(mode, '+') == NULL;
}

static bool write_all_fd(int fd, const char *buf, size_t len) {
    size_t written = 0;

    while (written < len) {
        ssize_t out = (ssize_t)syscall(SYS_write, fd, buf + written, len - written);

        if (out < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (out == 0) {
            return false;
        }
        written += (size_t)out;
    }
    return true;
}

static FILE *real_fopen_common(const char *path, const char *mode, bool use_fopen64) {
    const struct rps_real *real = rps_real_get();

    return use_fopen64 ? real->fopen64(path, mode) : real->fopen(path, mode);
}

static FILE *fopen_common(const char *path, const char *mode, bool use_fopen64) {
    const struct rps_real *real = rps_real_get();
    char meminfo[RPS_MEMINFO_BUFFER_SIZE];
    size_t len;
    FILE *fp = NULL;

    if (!meminfo_mode_is_read_only(mode) ||
        !should_virtualize_meminfo(path, O_RDONLY)) {
        return real_fopen_common(path, mode, use_fopen64);
    }
    if (!rps_guard_enter()) {
        return real_fopen_common(path, mode, use_fopen64);
    }
    if (rps_proc_meminfo_build(meminfo, sizeof(meminfo), &len)) {
#ifdef SYS_memfd_create
        int fd = (int)syscall(SYS_memfd_create, "runpod-shim-meminfo", MFD_CLOEXEC);

        if (fd >= 0) {
            if (write_all_fd(fd, meminfo, len) &&
                syscall(SYS_lseek, fd, 0, SEEK_SET) == 0) {
                fp = real->fdopen(fd, "r");
            }
            if (fp == NULL) {
                real->close(fd);
            }
        }
#endif
    }
    rps_guard_leave();
    if (fp != NULL) {
        rps_trace("meminfo fopen stream len=%llu", (unsigned long long)len);
        return fp;
    }
    return real_fopen_common(path, mode, use_fopen64);
}

static void *optional_next_symbol(const char *name) {
    return dlsym(RTLD_NEXT, name);
}

static bool path_is_absolute(const char *path) {
    return path != NULL && path[0] == '/';
}

static bool path_is_relative(const char *path) {
    return path != NULL && path[0] != '/';
}

enum rps_gpu_effective_path_result {
    RPS_GPU_EFFECTIVE_PASSTHROUGH = 0,
    RPS_GPU_EFFECTIVE_RESOLVED,
    RPS_GPU_EFFECTIVE_DENY
};

static bool gpu_proc_denied(const char *path) {
    return rps_gpu_video_enabled() &&
           g_gpu_proc_filter_ready &&
           path_is_absolute(path) &&
           rps_gpu_proc_path_allowed(&g_gpu_allowed_gpus, path) == RPS_GPU_PROC_DENY;
}

static int gpu_open_denied_path(const char *path, const struct rps_gpu_fd_record *record) {
    if (record != NULL && record->kind != RPS_GPU_FD_OTHER) {
        rps_trace("open %s -> fd -1", path);
    }

    errno = ENOENT;
    return -1;
}

static void gpu_trace_and_track_open(const char *path, int fd) {
    struct rps_gpu_fd_record record;

    if (!rps_gpu_video_enabled()) {
        return;
    }

    if (!rps_gpu_classify_path(path, &record)) {
        if (fd >= 0) {
            rps_gpu_fd_map_remove(fd);
        }
        return;
    }

    rps_trace("open %s -> fd %d", path, fd);
    if (fd >= 0) {
        rps_gpu_fd_map_set(fd, &record);
    }
}

struct rps_gpu_path_segment_ref {
    const char *start;
    size_t len;
};

static bool gpu_resolve_relative_proc_path(const struct rps_gpu_fd_record *record,
                                           const char *path,
                                           char *buffer,
                                           size_t buffer_len) {
    static const char proc_segment[] = "proc";
    static const char driver_segment[] = "driver";
    static const char nvidia_segment[] = "nvidia";
    static const char gpus_segment[] = "gpus";
    struct rps_gpu_path_segment_ref segments[64];
    size_t count = 0;
    size_t min_count = 4;
    size_t offset = 0;
    size_t used = 0;

    if (record == NULL || path == NULL || buffer == NULL || buffer_len < 2) {
        return false;
    }

    segments[count++] = (struct rps_gpu_path_segment_ref){proc_segment, sizeof(proc_segment) - 1};
    segments[count++] = (struct rps_gpu_path_segment_ref){driver_segment, sizeof(driver_segment) - 1};
    segments[count++] = (struct rps_gpu_path_segment_ref){nvidia_segment, sizeof(nvidia_segment) - 1};
    segments[count++] = (struct rps_gpu_path_segment_ref){gpus_segment, sizeof(gpus_segment) - 1};

    if (record->kind == RPS_GPU_FD_PROC_GPU_BUS_DIR) {
        size_t bus_len = strlen(record->bus_id);

        if (bus_len == 0 || count >= sizeof(segments) / sizeof(segments[0])) {
            return false;
        }
        segments[count++] = (struct rps_gpu_path_segment_ref){record->bus_id, bus_len};
    } else if (record->kind != RPS_GPU_FD_PROC_GPUS_DIR) {
        return false;
    }

    while (path[offset] != '\0') {
        const char *segment;
        size_t segment_len;

        while (path[offset] == '/') {
            offset++;
        }
        if (path[offset] == '\0') {
            break;
        }

        segment = path + offset;
        segment_len = 0;
        while (segment[segment_len] != '\0' && segment[segment_len] != '/') {
            segment_len++;
        }

        if (segment_len == 1 && segment[0] == '.') {
            offset += segment_len;
            continue;
        }

        if (segment_len == 2 && segment[0] == '.' && segment[1] == '.') {
            if (count <= min_count) {
                return false;
            }
            count--;
            offset += segment_len;
            continue;
        }

        if (count >= sizeof(segments) / sizeof(segments[0])) {
            return false;
        }
        segments[count++] = (struct rps_gpu_path_segment_ref){segment, segment_len};
        offset += segment_len;
    }

    if (count < min_count) {
        return false;
    }

    for (offset = 0; offset < count; offset++) {
        if (used + 1 + segments[offset].len >= buffer_len) {
            return false;
        }
        buffer[used++] = '/';
        memcpy(buffer + used, segments[offset].start, segments[offset].len);
        used += segments[offset].len;
    }
    buffer[used] = '\0';
    return true;
}

static enum rps_gpu_effective_path_result gpu_effective_openat_path(int dirfd,
                                                                    const char *path,
                                                                    char *buffer,
                                                                    size_t buffer_len,
                                                                    const char **out_path) {
    struct rps_gpu_fd_record record;

    if (out_path == NULL) {
        return RPS_GPU_EFFECTIVE_DENY;
    }
    *out_path = path;

    if (!path_is_relative(path) ||
        buffer == NULL ||
        buffer_len == 0 ||
        !rps_gpu_fd_map_get(dirfd, &record)) {
        return RPS_GPU_EFFECTIVE_PASSTHROUGH;
    }

    if (record.kind != RPS_GPU_FD_PROC_GPUS_DIR &&
        record.kind != RPS_GPU_FD_PROC_GPU_BUS_DIR) {
        return RPS_GPU_EFFECTIVE_PASSTHROUGH;
    }

    if (!gpu_resolve_relative_proc_path(&record, path, buffer, buffer_len)) {
        *out_path = NULL;
        return RPS_GPU_EFFECTIVE_DENY;
    }

    *out_path = buffer;
    return RPS_GPU_EFFECTIVE_RESOLVED;
}

static int gpu_readdir_entry_allowed(int dirfd, const char *name) {
    struct rps_gpu_fd_record record;

    if (!rps_gpu_video_enabled() ||
        !g_gpu_proc_filter_ready ||
        name == NULL ||
        !rps_gpu_fd_map_get(dirfd, &record) ||
        record.kind != RPS_GPU_FD_PROC_GPUS_DIR ||
        strcmp(name, ".") == 0 ||
        strcmp(name, "..") == 0) {
        return 1;
    }

    return rps_gpu_allowed_contains_bus(&g_gpu_allowed_gpus, name);
}

#ifdef RUNPOD_SHIM_TESTING
const char *rps_gpu_test_effective_openat_path(int dirfd,
                                               const char *path,
                                               char *buffer,
                                               size_t buffer_len) {
    const char *effective_path;

    if (gpu_effective_openat_path(dirfd, path, buffer, buffer_len, &effective_path) ==
        RPS_GPU_EFFECTIVE_DENY) {
        return NULL;
    }
    return effective_path;
}

int rps_gpu_test_readdir_entry_allowed(int dirfd, const char *name) {
    return gpu_readdir_entry_allowed(dirfd, name);
}

void rps_gpu_test_set_proc_filter_ready(int ready) {
    g_gpu_proc_filter_ready = ready;
}

void rps_gpu_test_set_allowed_bus(const char *bus_id) {
    memset(&g_gpu_allowed_gpus, 0, sizeof(g_gpu_allowed_gpus));
    g_gpu_allowed_gpus.valid = 1;
    g_gpu_allowed_gpus.count = 1;
    g_gpu_allowed_gpus.gpus[0].present = 1;
    if (bus_id != NULL) {
        strncpy(g_gpu_allowed_gpus.gpus[0].bus_id,
                bus_id,
                sizeof(g_gpu_allowed_gpus.gpus[0].bus_id) - 1);
        g_gpu_allowed_gpus.gpus[0].bus_id[sizeof(g_gpu_allowed_gpus.gpus[0].bus_id) - 1] = '\0';
    }
}

void rps_gpu_test_track_open(const char *path, int fd) {
    gpu_trace_and_track_open(path, fd);
}

int rps_gpu_test_proc_denied(const char *path) {
    return gpu_proc_denied(path) ? 1 : 0;
}
#endif

static int open_common(const char *path,
                       int flags,
                       mode_t mode,
                       bool has_mode,
                       bool use_open64) {
    const struct rps_real *real = rps_real_get();
    struct rps_gpu_fd_record record;
    bool classified;
    int fd;

    classified = rps_gpu_video_enabled() && rps_gpu_classify_path(path, &record);
    if (gpu_proc_denied(path)) {
        return gpu_open_denied_path(path, classified ? &record : NULL);
    }

    if (use_open64) {
        fd = has_mode ? real->open64(path, flags, mode) : real->open64(path, flags);
    } else {
        fd = has_mode ? real->open(path, flags, mode) : real->open(path, flags);
    }

    maybe_attach_meminfo(fd, path, flags);
    gpu_trace_and_track_open(path, fd);
    return fd;
}

static int openat64_via_next(int dirfd,
                             const char *path,
                             int flags,
                             mode_t mode,
                             bool has_mode,
                             int *used_openat64) {
    typedef int (*openat64_fn)(int, const char *, int, ...);
    static openat64_fn real_openat64;
    static int loaded;

    if (!loaded) {
        real_openat64 = (openat64_fn)optional_next_symbol("openat64");
        loaded = 1;
    }

    if (real_openat64 == NULL) {
        *used_openat64 = 0;
        return -1;
    }

    *used_openat64 = 1;
    return has_mode ? real_openat64(dirfd, path, flags, mode) :
                      real_openat64(dirfd, path, flags);
}

static int openat_common(int dirfd,
                         const char *path,
                         int flags,
                         mode_t mode,
                         bool has_mode,
                         bool use_open64) {
    const struct rps_real *real = rps_real_get();
    char effective_buffer[RPS_GPU_PATH_MAX];
    const char *effective_path;
    enum rps_gpu_effective_path_result effective_result;
    int fd;

    effective_result =
        gpu_effective_openat_path(dirfd, path, effective_buffer, sizeof(effective_buffer), &effective_path);
    if (effective_result == RPS_GPU_EFFECTIVE_DENY) {
        errno = ENOENT;
        return -1;
    }
    if (gpu_proc_denied(effective_path)) {
        struct rps_gpu_fd_record record;
        bool classified = rps_gpu_video_enabled() && rps_gpu_classify_path(effective_path, &record);
        return gpu_open_denied_path(effective_path, classified ? &record : NULL);
    }

    if (use_open64) {
        int used_openat64 = 0;
        fd = openat64_via_next(dirfd, path, flags, mode, has_mode, &used_openat64);
        if (!used_openat64) {
            fd = has_mode ? real->openat(dirfd, path, flags, mode) :
                            real->openat(dirfd, path, flags);
        }
    } else {
        fd = has_mode ? real->openat(dirfd, path, flags, mode) :
                        real->openat(dirfd, path, flags);
    }

    if (dirfd == AT_FDCWD) {
        maybe_attach_meminfo(fd, path, flags);
    }
    if (path_is_absolute(effective_path) || dirfd == AT_FDCWD) {
        gpu_trace_and_track_open(effective_path, fd);
    }
    return fd;
}

int open(const char *path, int flags, ...) {
    mode_t mode = 0;
    bool has_mode = open_needs_mode(flags);

    if (has_mode) {
        va_list ap;

        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    return open_common(path, flags, mode, has_mode, false);
}

FILE *fopen(const char *path, const char *mode) {
    return fopen_common(path, mode, false);
}

FILE *fopen64(const char *path, const char *mode) {
    return fopen_common(path, mode, true);
}

int open64(const char *path, int flags, ...) {
    mode_t mode = 0;
    bool has_mode = open_needs_mode(flags);

    if (has_mode) {
        va_list ap;

        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    return open_common(path, flags, mode, has_mode, true);
}

int openat(int dirfd, const char *path, int flags, ...) {
    mode_t mode = 0;
    bool has_mode = open_needs_mode(flags);

    if (has_mode) {
        va_list ap;

        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    return openat_common(dirfd, path, flags, mode, has_mode, false);
}

int openat64(int dirfd, const char *path, int flags, ...) {
    mode_t mode = 0;
    bool has_mode = open_needs_mode(flags);

    if (has_mode) {
        va_list ap;

        va_start(ap, flags);
        mode = va_arg(ap, mode_t);
        va_end(ap);
    }

    return openat_common(dirfd, path, flags, mode, has_mode, true);
}

ssize_t read(int fd, void *buf, size_t count) {
    const struct rps_real *real = rps_real_get();
    ssize_t out;

    if (!rps_guard_enter()) {
        return real->read(fd, buf, count);
    }
    if (rps_fd_map_take_read(fd, buf, count, &out)) {
        rps_guard_leave();
        return out;
    }
    rps_guard_leave();
    return real->read(fd, buf, count);
}

int close(int fd) {
    const struct rps_real *real = rps_real_get();

    rps_fd_map_remove(fd);
    rps_gpu_fd_map_remove(fd);
    return real->close(fd);
}

int dup(int oldfd) {
    const struct rps_real *real = rps_real_get();
    int rc = real->dup(oldfd);

    if (rc >= 0) {
        rps_gpu_fd_map_copy(oldfd, rc);
    }
    return rc;
}

int dup2(int oldfd, int newfd) {
    const struct rps_real *real = rps_real_get();
    int rc = real->dup2(oldfd, newfd);

    if (rc >= 0 && oldfd != newfd) {
        rps_fd_map_remove(newfd);
        rps_gpu_fd_map_copy(oldfd, newfd);
    }
    return rc;
}

int dup3(int oldfd, int newfd, int flags) {
    const struct rps_real *real = rps_real_get();
    int rc = real->dup3(oldfd, newfd, flags);

    if (rc >= 0) {
        rps_fd_map_remove(newfd);
        rps_gpu_fd_map_copy(oldfd, newfd);
    }
    return rc;
}

enum fcntl_arg_kind {
    RPS_FCNTL_ARG_NONE = 0,
    RPS_FCNTL_ARG_INT,
    RPS_FCNTL_ARG_PTR
};

static enum fcntl_arg_kind fcntl_arg_kind(int cmd) {
    switch (cmd) {
#ifdef F_DUPFD
    case F_DUPFD:
        return RPS_FCNTL_ARG_INT;
#endif
#ifdef F_DUPFD_CLOEXEC
    case F_DUPFD_CLOEXEC:
        return RPS_FCNTL_ARG_INT;
#endif
#ifdef F_SETFD
    case F_SETFD:
        return RPS_FCNTL_ARG_INT;
#endif
#ifdef F_SETFL
    case F_SETFL:
        return RPS_FCNTL_ARG_INT;
#endif
#ifdef F_SETOWN
    case F_SETOWN:
        return RPS_FCNTL_ARG_INT;
#endif
#ifdef F_SETSIG
    case F_SETSIG:
        return RPS_FCNTL_ARG_INT;
#endif
#ifdef F_SETLEASE
    case F_SETLEASE:
        return RPS_FCNTL_ARG_INT;
#endif
#ifdef F_NOTIFY
    case F_NOTIFY:
        return RPS_FCNTL_ARG_INT;
#endif
#ifdef F_SETPIPE_SZ
    case F_SETPIPE_SZ:
        return RPS_FCNTL_ARG_INT;
#endif
#ifdef F_ADD_SEALS
    case F_ADD_SEALS:
        return RPS_FCNTL_ARG_INT;
#endif
#ifdef F_GETLK
    case F_GETLK:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_SETLK
    case F_SETLK:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_SETLKW
    case F_SETLKW:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_OFD_GETLK
    case F_OFD_GETLK:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_OFD_SETLK
    case F_OFD_SETLK:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_OFD_SETLKW
    case F_OFD_SETLKW:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_GETOWN_EX
    case F_GETOWN_EX:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_SETOWN_EX
    case F_SETOWN_EX:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_GET_RW_HINT
    case F_GET_RW_HINT:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_SET_RW_HINT
    case F_SET_RW_HINT:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_GET_FILE_RW_HINT
    case F_GET_FILE_RW_HINT:
        return RPS_FCNTL_ARG_PTR;
#endif
#ifdef F_SET_FILE_RW_HINT
    case F_SET_FILE_RW_HINT:
        return RPS_FCNTL_ARG_PTR;
#endif
    default:
        return RPS_FCNTL_ARG_NONE;
    }
}

static bool fcntl_duplicates_fd(int cmd) {
    if (cmd == F_DUPFD) {
        return true;
    }
#ifdef F_DUPFD_CLOEXEC
    if (cmd == F_DUPFD_CLOEXEC) {
        return true;
    }
#endif
    return false;
}

int fcntl(int fd, int cmd, ...) {
    const struct rps_real *real = rps_real_get();
    enum fcntl_arg_kind kind = fcntl_arg_kind(cmd);
    va_list ap;
    int rc;

    if (kind == RPS_FCNTL_ARG_INT) {
        int arg;

        va_start(ap, cmd);
        arg = va_arg(ap, int);
        va_end(ap);
        rc = real->fcntl(fd, cmd, arg);
    } else if (kind == RPS_FCNTL_ARG_PTR) {
        void *arg;

        va_start(ap, cmd);
        arg = va_arg(ap, void *);
        va_end(ap);
        rc = real->fcntl(fd, cmd, arg);
    } else {
        rc = real->fcntl(fd, cmd);
    }

    if (rc >= 0 && fcntl_duplicates_fd(cmd)) {
        rps_gpu_fd_map_copy(fd, rc);
    }
    return rc;
}

int stat(const char *path, struct stat *statbuf) {
    const struct rps_real *real = rps_real_get();

    if (gpu_proc_denied(path)) {
        errno = ENOENT;
        return -1;
    }

    return real->stat(path, statbuf);
}

int fstat(int fd, struct stat *statbuf) {
    const struct rps_real *real = rps_real_get();

    return real->fstat(fd, statbuf);
}

static int fstatat_common(int dirfd, const char *path, struct stat *statbuf, int flags) {
    char effective_buffer[RPS_GPU_PATH_MAX];
    const char *effective_path;
    enum rps_gpu_effective_path_result effective_result;

    effective_result =
        gpu_effective_openat_path(dirfd, path, effective_buffer, sizeof(effective_buffer), &effective_path);
    if (effective_result == RPS_GPU_EFFECTIVE_DENY) {
        errno = ENOENT;
        return -1;
    }
    if (gpu_proc_denied(effective_path)) {
        errno = ENOENT;
        return -1;
    }

    return (int)syscall(SYS_newfstatat, dirfd, path, statbuf, flags);
}

int newfstatat(int dirfd, const char *path, struct stat *statbuf, int flags) {
    return fstatat_common(dirfd, path, statbuf, flags);
}

int fstatat(int dirfd, const char *path, struct stat *statbuf, int flags) {
    return fstatat_common(dirfd, path, statbuf, flags);
}

static DIR *opendir64_via_next(const char *path, int *used_opendir64) {
    typedef DIR *(*opendir64_fn)(const char *);
    static opendir64_fn real_opendir64;
    static int loaded;

    if (!loaded) {
        real_opendir64 = (opendir64_fn)optional_next_symbol("opendir64");
        loaded = 1;
    }

    if (real_opendir64 == NULL) {
        *used_opendir64 = 0;
        return NULL;
    }

    *used_opendir64 = 1;
    return real_opendir64(path);
}

static DIR *opendir_common(const char *path, bool use_opendir64) {
    const struct rps_real *real = rps_real_get();
    DIR *dir;

    if (gpu_proc_denied(path)) {
        errno = ENOENT;
        return NULL;
    }

    if (use_opendir64) {
        int used_opendir64 = 0;
        dir = opendir64_via_next(path, &used_opendir64);
        if (!used_opendir64) {
            dir = real->opendir(path);
        }
    } else {
        dir = real->opendir(path);
    }

    if (dir != NULL) {
        gpu_trace_and_track_open(path, dirfd(dir));
    }
    return dir;
}

DIR *opendir(const char *path) {
    return opendir_common(path, false);
}

DIR *opendir64(const char *path) {
    return opendir_common(path, true);
}

DIR *fdopendir(int fd) {
    typedef DIR *(*fdopendir_fn)(int);
    static fdopendir_fn real_fdopendir;
    static int loaded;

    if (!loaded) {
        real_fdopendir = (fdopendir_fn)optional_next_symbol("fdopendir");
        loaded = 1;
    }

    if (real_fdopendir == NULL) {
        errno = ENOSYS;
        return NULL;
    }

    return real_fdopendir(fd);
}

int closedir(DIR *dirp) {
    const struct rps_real *real = rps_real_get();
    int fd = dirfd(dirp);
    int rc = real->closedir(dirp);

    if (rc == 0 && fd >= 0) {
        rps_gpu_fd_map_remove(fd);
    }
    return rc;
}

struct dirent *readdir(DIR *dirp) {
    const struct rps_real *real = rps_real_get();
    struct dirent *entry;
    int fd;

    entry = real->readdir(dirp);
    if (entry == NULL) {
        return NULL;
    }

    fd = dirfd(dirp);
    while (!gpu_readdir_entry_allowed(fd, entry->d_name)) {
        entry = real->readdir(dirp);
        if (entry == NULL) {
            return NULL;
        }
    }

    return entry;
}

struct dirent64 *readdir64(DIR *dirp) {
    typedef struct dirent64 *(*readdir64_fn)(DIR *);
    static readdir64_fn real_readdir64;
    static int loaded;
    struct dirent64 *entry;
    int fd;

    if (!loaded) {
        real_readdir64 = (readdir64_fn)optional_next_symbol("readdir64");
        loaded = 1;
    }

    if (real_readdir64 == NULL) {
        errno = ENOSYS;
        return NULL;
    }

    entry = real_readdir64(dirp);
    if (entry == NULL) {
        return NULL;
    }

    fd = dirfd(dirp);
    while (!gpu_readdir_entry_allowed(fd, entry->d_name)) {
        entry = real_readdir64(dirp);
        if (entry == NULL) {
            return NULL;
        }
    }

    return entry;
}

ssize_t getdents64(int fd, void *dirp, size_t count) {
    ssize_t result;
    struct rps_gpu_fd_record record;

    result = (ssize_t)syscall(SYS_getdents64, fd, dirp, count);
    if (result > 0 &&
        rps_gpu_video_enabled() &&
        g_gpu_proc_filter_ready &&
        rps_gpu_fd_map_get(fd, &record) &&
        record.kind == RPS_GPU_FD_PROC_GPUS_DIR) {
        result = rps_gpu_filter_dirent64_buffer((char *)dirp, result, &g_gpu_allowed_gpus);
    }

    return result;
}

static bool ioctl_request_has_no_arg(unsigned long request) {
#ifdef FIOCLEX
    if (request == (unsigned long)FIOCLEX) {
        return true;
    }
#endif
#ifdef FIONCLEX
    if (request == (unsigned long)FIONCLEX) {
        return true;
    }
#endif
#ifdef TIOCEXCL
    if (request == (unsigned long)TIOCEXCL) {
        return true;
    }
#endif
#ifdef TIOCNXCL
    if (request == (unsigned long)TIOCNXCL) {
        return true;
    }
#endif
#ifdef TIOCNOTTY
    if (request == (unsigned long)TIOCNOTTY) {
        return true;
    }
#endif
    return false;
}

int ioctl(int fd, unsigned long request, ...) {
    const struct rps_real *real = rps_real_get();
    va_list ap;
    void *arg;
    int result;
    struct rps_gpu_fd_record record;

    if (ioctl_request_has_no_arg(request)) {
        arg = NULL;
        result = real->ioctl(fd, request);
    } else {
        va_start(ap, request);
        arg = va_arg(ap, void *);
        va_end(ap);
        result = real->ioctl(fd, request, arg);
    }
    if (rps_gpu_ioctl_enabled() &&
        rps_gpu_fd_map_get(fd, &record) &&
        record.kind == RPS_GPU_FD_NVIDIA_CTL) {
        rps_gpu_ioctl_filter_after(g_gpu_proc_filter_ready ? &g_gpu_allowed_gpus : NULL,
                                   request,
                                   arg,
                                   result);
    }

    return result;
}

int sysinfo(struct sysinfo *info) {
    const struct rps_real *real = rps_real_get();
    int rc = real->sysinfo(info);
    struct rps_memory_snapshot snapshot;
    unsigned long totalram;
    unsigned long freeram;
    unsigned int mem_unit;

    if (rc != 0 || info == NULL || !rps_config_get()->cgroup_memory) {
        return rc;
    }
    if (!rps_guard_enter()) {
        return rc;
    }
    if (rps_memory_snapshot(&snapshot) &&
        encode_sysinfo_memory(&snapshot, &totalram, &freeram, &mem_unit)) {
        info->mem_unit = mem_unit;
        info->totalram = totalram;
        info->freeram = freeram;
        rps_trace("corrected sysinfo total=%llu available=%llu mem_unit=%u",
                  (unsigned long long)snapshot.total,
                  (unsigned long long)snapshot.available,
                  mem_unit);
    }
    rps_guard_leave();
    return rc;
}

long sysconf(int name) {
    const struct rps_real *real = rps_real_get();
    struct rps_memory_snapshot snapshot;
    uint64_t bytes;
    uint64_t pages;
    long page_size;

    if (name != _SC_PHYS_PAGES && name != _SC_AVPHYS_PAGES) {
        return real->sysconf(name);
    }
    if (!rps_config_get()->cgroup_memory || !rps_guard_enter()) {
        return real->sysconf(name);
    }

    page_size = real->sysconf(_SC_PAGESIZE);
    if (page_size <= 0 || !rps_memory_snapshot(&snapshot)) {
        rps_guard_leave();
        return real->sysconf(name);
    }

    bytes = name == _SC_PHYS_PAGES ? snapshot.total : snapshot.available;
    pages = bytes / (uint64_t)page_size;
    if (pages > (uint64_t)LONG_MAX) {
        rps_guard_leave();
        errno = EOVERFLOW;
        return -1;
    }

    rps_guard_leave();
    return (long)pages;
}
