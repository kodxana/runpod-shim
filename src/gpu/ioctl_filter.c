#include "ioctl_filter.h"

#include "nvidia_rm.h"
#include "../core/config.h"
#include "../core/log.h"

#include <pthread.h>
#include <stdint.h>
#include <string.h>

static pthread_mutex_t g_ioctl_filter_lock = PTHREAD_MUTEX_INITIALIZER;
static uint32_t g_allowed_gpu_ids[RPS_GPU_MAX_GPUS];
static size_t g_allowed_gpu_id_count;

static int rps_gpu_allowed_contains_minor(const struct rps_gpu_allowed_gpus *allowed,
                                          uint32_t minor) {
    size_t i;

    if (allowed == NULL || !allowed->valid) {
        return 0;
    }

    for (i = 0; i < allowed->count; i++) {
        if (allowed->gpus[i].present &&
            allowed->gpus[i].device_minor >= 0 &&
            (uint32_t)allowed->gpus[i].device_minor == minor) {
            return 1;
        }
    }

    return 0;
}

static int rps_gpu_id_is_allowed_locked(uint32_t gpu_id) {
    size_t i;

    for (i = 0; i < g_allowed_gpu_id_count; i++) {
        if (g_allowed_gpu_ids[i] == gpu_id) {
            return 1;
        }
    }

    return 0;
}

static void rps_gpu_remember_allowed_gpu_id_locked(uint32_t gpu_id) {
    if (gpu_id == RPS_NV0000_CTRL_GPU_INVALID_ID ||
        rps_gpu_id_is_allowed_locked(gpu_id) ||
        g_allowed_gpu_id_count >= RPS_GPU_MAX_GPUS) {
        return;
    }

    g_allowed_gpu_ids[g_allowed_gpu_id_count] = gpu_id;
    g_allowed_gpu_id_count++;
}

static void rps_gpu_record_card_info_gpu_ids(const struct rps_gpu_allowed_gpus *allowed,
                                             const struct rps_nv_ioctl_card_info *cards,
                                             size_t count) {
    size_t i;

    if (allowed == NULL || !allowed->valid || cards == NULL) {
        return;
    }

    pthread_mutex_lock(&g_ioctl_filter_lock);
    for (i = 0; i < count; i++) {
        if (cards[i].valid && rps_gpu_allowed_contains_minor(allowed, cards[i].minor_number)) {
            rps_gpu_remember_allowed_gpu_id_locked(cards[i].gpu_id);
        }
    }
    pthread_mutex_unlock(&g_ioctl_filter_lock);
}

static void rps_gpu_filter_card_info(const struct rps_gpu_allowed_gpus *allowed,
                                     struct rps_nv_ioctl_card_info *cards,
                                     size_t count) {
    size_t i;

    if (allowed == NULL || !allowed->valid || cards == NULL) {
        return;
    }

    for (i = 0; i < count; i++) {
        if (cards[i].valid && !rps_gpu_allowed_contains_minor(allowed, cards[i].minor_number)) {
            cards[i].valid = 0;
        }
    }
}

static int rps_gpu_ioctl_filter_enabled(const struct rps_gpu_allowed_gpus *allowed,
                                        int result,
                                        const struct rps_nvos54_parameters *rm) {
    const struct rps_config *cfg = rps_config_get();

    return cfg->gpu_video &&
           cfg->gpu_ioctl &&
           allowed != NULL &&
           allowed->valid &&
           result == 0 &&
           rm != NULL &&
           rm->status == 0 &&
           rm->params != 0;
}

static int rps_gpu_filter_gpu_id_array(uint32_t *gpu_ids, size_t count, size_t *kept_out) {
    uint32_t kept[RPS_GPU_MAX_GPUS];
    size_t kept_count;
    size_t i;

    if (kept_out != NULL) {
        *kept_out = 0;
    }
    if (gpu_ids == NULL || count == 0) {
        return 0;
    }

    pthread_mutex_lock(&g_ioctl_filter_lock);
    if (g_allowed_gpu_id_count == 0) {
        pthread_mutex_unlock(&g_ioctl_filter_lock);
        return 0;
    }

    kept_count = 0;
    for (i = 0; i < count; i++) {
        uint32_t gpu_id = gpu_ids[i];

        if (gpu_id == RPS_NV0000_CTRL_GPU_INVALID_ID) {
            break;
        }

        if (rps_gpu_id_is_allowed_locked(gpu_id) && kept_count < RPS_GPU_MAX_GPUS) {
            kept[kept_count] = gpu_id;
            kept_count++;
        }
    }
    pthread_mutex_unlock(&g_ioctl_filter_lock);

    for (i = 0; i < count; i++) {
        gpu_ids[i] = i < kept_count ? kept[i] : RPS_NV0000_CTRL_GPU_INVALID_ID;
    }

    if (kept_out != NULL) {
        *kept_out = kept_count;
    }
    return 1;
}

static int rps_gpu_filter_active_devices(
    struct rps_nv0000_ctrl_gpu_get_active_device_ids_params *params,
    uint32_t *kept_out) {
    struct rps_nv0000_ctrl_gpu_active_device kept[RPS_NV0000_CTRL_GPU_MAX_ACTIVE_DEVICES];
    uint32_t kept_count;
    uint32_t i;
    uint32_t count;

    if (kept_out != NULL) {
        *kept_out = 0;
    }
    if (params == NULL) {
        return 0;
    }

    pthread_mutex_lock(&g_ioctl_filter_lock);
    if (g_allowed_gpu_id_count == 0) {
        pthread_mutex_unlock(&g_ioctl_filter_lock);
        return 0;
    }

    count = params->numDevices;
    if (count > RPS_NV0000_CTRL_GPU_MAX_ACTIVE_DEVICES) {
        count = RPS_NV0000_CTRL_GPU_MAX_ACTIVE_DEVICES;
    }

    kept_count = 0;
    for (i = 0; i < count; i++) {
        if (rps_gpu_id_is_allowed_locked(params->devices[i].gpuId)) {
            kept[kept_count] = params->devices[i];
            kept_count++;
        }
    }
    pthread_mutex_unlock(&g_ioctl_filter_lock);

    if (kept_count > 0) {
        memcpy(params->devices, kept, sizeof(kept[0]) * kept_count);
    }
    if (kept_count < RPS_NV0000_CTRL_GPU_MAX_ACTIVE_DEVICES) {
        memset(params->devices + kept_count,
               0,
               sizeof(params->devices[0]) *
                   (RPS_NV0000_CTRL_GPU_MAX_ACTIVE_DEVICES - kept_count));
    }
    params->numDevices = kept_count;
    if (kept_out != NULL) {
        *kept_out = kept_count;
    }
    return 1;
}

static void rps_gpu_filter_rm_control(const struct rps_gpu_allowed_gpus *allowed,
                                      struct rps_nvos54_parameters *rm,
                                      int result) {
    if (!rps_gpu_ioctl_filter_enabled(allowed, result, rm)) {
        return;
    }

    if (rm->cmd == RPS_NV0000_CTRL_CMD_GPU_GET_ATTACHED_IDS &&
        rm->paramsSize >= sizeof(uint32_t) * RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS) {
        uint32_t *gpu_ids = (uint32_t *)(uintptr_t)rm->params;
        size_t kept;

        if (rps_gpu_filter_gpu_id_array(gpu_ids,
                                        RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS,
                                        &kept)) {
            rps_trace("filtered RM attached gpuIds to %zu allowed entries", kept);
        }
    } else if (rm->cmd == RPS_NV0000_CTRL_CMD_GPU_GET_ACTIVE_DEVICE_IDS &&
               rm->paramsSize >=
                   sizeof(struct rps_nv0000_ctrl_gpu_get_active_device_ids_params)) {
        uint32_t kept;

        if (rps_gpu_filter_active_devices(
                (struct rps_nv0000_ctrl_gpu_get_active_device_ids_params *)(uintptr_t)rm->params,
                &kept)) {
            rps_trace("filtered RM active device ids to %u allowed entries", kept);
        }
    }
}

void rps_gpu_ioctl_filter_after(const struct rps_gpu_allowed_gpus *allowed,
                                unsigned long request,
                                void *arg,
                                int result) {
    const struct rps_config *cfg;

    if (result != 0 || arg == NULL) {
        return;
    }

    cfg = rps_config_get();
    if (rps_request_is_card_info(request)) {
        struct rps_nv_ioctl_card_info *cards = (struct rps_nv_ioctl_card_info *)arg;

        rps_gpu_record_card_info_gpu_ids(allowed, cards, RPS_NV_MAX_DEVICES);
        if (cfg->gpu_video && cfg->gpu_ioctl) {
            rps_gpu_filter_card_info(allowed, cards, RPS_NV_MAX_DEVICES);
        }
        return;
    }

    if (rps_request_is_rm_control(request)) {
        rps_gpu_filter_rm_control(allowed, (struct rps_nvos54_parameters *)arg, result);
    }
}

#ifdef RUNPOD_SHIM_TESTING
void rps_gpu_test_ioctl_filter_reset(void) {
    pthread_mutex_lock(&g_ioctl_filter_lock);
    memset(g_allowed_gpu_ids, 0, sizeof(g_allowed_gpu_ids));
    g_allowed_gpu_id_count = 0;
    pthread_mutex_unlock(&g_ioctl_filter_lock);
}

int rps_gpu_test_ioctl_allowed_gpu_id_count(void) {
    int result;

    pthread_mutex_lock(&g_ioctl_filter_lock);
    result = (int)g_allowed_gpu_id_count;
    pthread_mutex_unlock(&g_ioctl_filter_lock);
    return result;
}
#endif
