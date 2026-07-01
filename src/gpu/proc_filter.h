#ifndef RUNPOD_SHIM_GPU_PROC_FILTER_H
#define RUNPOD_SHIM_GPU_PROC_FILTER_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define RPS_GPU_MAX_GPUS 32
#define RPS_GPU_PROC_PATH_MAX 256
#define RPS_GPU_PROC_BUS_ID_MAX 32
#define RPS_GPU_PROC_UUID_MAX 96

struct rps_gpu_info {
    int present;
    int device_minor;
    char bus_id[RPS_GPU_PROC_BUS_ID_MAX];
    char uuid[RPS_GPU_PROC_UUID_MAX];
};

struct rps_gpu_allowed_gpus {
    int valid;
    size_t count;
    struct rps_gpu_info gpus[RPS_GPU_MAX_GPUS];
};

enum rps_gpu_proc_decision {
    RPS_GPU_PROC_IGNORE = 0,
    RPS_GPU_PROC_ALLOW,
    RPS_GPU_PROC_DENY
};

int rps_gpu_parse_gpu_information(const char *path,
                                  const char *text,
                                  struct rps_gpu_info *out);
int rps_gpu_build_allowed_gpus(const struct rps_gpu_info *infos,
                               size_t info_count,
                               const int *mounted_minors,
                               size_t mounted_count,
                               const char *explicit_uuid,
                               struct rps_gpu_allowed_gpus *out);
int rps_gpu_proc_filter_runtime_init(const char *explicit_uuid,
                                     struct rps_gpu_allowed_gpus *out);
int rps_gpu_proc_normalize_path(const char *path, char *out, size_t out_size);
int rps_gpu_allowed_contains_bus(const struct rps_gpu_allowed_gpus *allowed,
                                 const char *bus_id);
enum rps_gpu_proc_decision rps_gpu_proc_path_allowed(
    const struct rps_gpu_allowed_gpus *allowed,
    const char *path);
ssize_t rps_gpu_filter_dirent64_buffer(char *buffer,
                                       ssize_t len,
                                       const struct rps_gpu_allowed_gpus *allowed);

#endif
