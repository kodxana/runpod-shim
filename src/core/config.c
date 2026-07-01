#include "config.h"

#include <stdlib.h>
#include <string.h>

static struct rps_config config;
static int initialized;

bool rps_env_is_disabled(const char *name) {
    const char *value = getenv(name);
    return value != NULL &&
           (strcmp(value, "0") == 0 || strcmp(value, "false") == 0 ||
            strcmp(value, "FALSE") == 0 || strcmp(value, "off") == 0 ||
            strcmp(value, "OFF") == 0 || strcmp(value, "no") == 0 ||
            strcmp(value, "NO") == 0);
}

bool rps_env_is_enabled(const char *name) {
    const char *value = getenv(name);
    return value != NULL &&
           (strcmp(value, "1") == 0 || strcmp(value, "true") == 0 ||
            strcmp(value, "TRUE") == 0 || strcmp(value, "on") == 0 ||
            strcmp(value, "ON") == 0 || strcmp(value, "yes") == 0 ||
            strcmp(value, "YES") == 0);
}

const struct rps_config *rps_config_get(void) {
    if (!initialized) {
        config.gpu_video = !rps_env_is_disabled("RUNPOD_SHIM_GPU_VIDEO");
        config.gpu_ioctl = !rps_env_is_disabled("RUNPOD_SHIM_GPU_IOCTL");
        config.cgroup_memory = !rps_env_is_disabled("RUNPOD_SHIM_CGROUP_MEMORY");
        config.trace = rps_env_is_enabled("RUNPOD_SHIM_TRACE");
        initialized = 1;
    }
    return &config;
}
