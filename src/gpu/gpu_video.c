#include "gpu_video.h"

#include "../core/config.h"

int rps_gpu_video_enabled(void) {
    return rps_config_get()->gpu_video;
}

int rps_gpu_ioctl_enabled(void) {
    const struct rps_config *cfg = rps_config_get();

    return cfg->gpu_video && cfg->gpu_ioctl;
}
