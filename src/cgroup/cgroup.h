#ifndef RUNPOD_SHIM_CGROUP_H
#define RUNPOD_SHIM_CGROUP_H

#include <stdbool.h>
#include <stdint.h>

struct rps_cgroup_memory {
    int version;
    uint64_t limit_bytes;
    uint64_t current_bytes;
    char limit_path[512];
    char current_path[512];
};

bool rps_cgroup_memory_detect(struct rps_cgroup_memory *out);
bool rps_cgroup_memory_refresh(struct rps_cgroup_memory *mem);

#endif
