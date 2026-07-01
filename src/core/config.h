#ifndef RUNPOD_SHIM_CONFIG_H
#define RUNPOD_SHIM_CONFIG_H

#include <stdbool.h>

struct rps_config {
    bool gpu_video;
    bool gpu_ioctl;
    bool cgroup_memory;
    bool trace;
};

const struct rps_config *rps_config_get(void);
bool rps_env_is_disabled(const char *name);
bool rps_env_is_enabled(const char *name);

#endif
