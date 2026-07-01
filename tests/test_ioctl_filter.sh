#!/bin/sh
set -eu

root_dir="$(CDPATH= cd "$(dirname "$0")/.." && pwd)"
cd "$root_dir"

mkdir -p build/tests
cat >build/tests/test_ioctl_filter.c <<'EOF'
#include "gpu/ioctl_filter.h"
#include "gpu/nvidia_rm.h"
#include "gpu/proc_filter.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int failures;

static void expect_int(const char *name, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s: got %d want %d\n", name, got, want);
        failures++;
    }
}

static void expect_u32(const char *name, uint32_t got, uint32_t want) {
    if (got != want) {
        fprintf(stderr, "%s: got 0x%x want 0x%x\n", name, got, want);
        failures++;
    }
}

static struct rps_gpu_allowed_gpus allowed_minor_2(void) {
    struct rps_gpu_allowed_gpus allowed;

    memset(&allowed, 0, sizeof(allowed));
    allowed.valid = 1;
    allowed.count = 1;
    allowed.gpus[0].present = 1;
    allowed.gpus[0].device_minor = 2;
    strcpy(allowed.gpus[0].bus_id, "0000:01:00.0");
    strcpy(allowed.gpus[0].uuid, "GPU-allowed");
    return allowed;
}

static void teach_cards(const struct rps_gpu_allowed_gpus *allowed,
                        struct rps_nv_ioctl_card_info cards[RPS_NV_MAX_DEVICES]) {
    memset(cards, 0, sizeof(struct rps_nv_ioctl_card_info) * RPS_NV_MAX_DEVICES);
    cards[0].valid = 1;
    cards[0].gpu_id = 0xaaaa0002U;
    cards[0].minor_number = 2;
    cards[1].valid = 1;
    cards[1].gpu_id = 0xbbbb0003U;
    cards[1].minor_number = 3;
    rps_gpu_ioctl_filter_after(allowed, RPS_IOCTL_CARD_INFO_REQUEST, cards, 0);
}

static void test_card_info_records_and_hides_disallowed_minors(void) {
    struct rps_gpu_allowed_gpus allowed = allowed_minor_2();
    struct rps_nv_ioctl_card_info cards[RPS_NV_MAX_DEVICES];

    rps_gpu_test_ioctl_filter_reset();
    teach_cards(&allowed, cards);

    expect_int("allowed card remains valid", cards[0].valid, 1);
    expect_int("disallowed card hidden", cards[1].valid, 0);
    expect_int("one allowed gpu id learned", rps_gpu_test_ioctl_allowed_gpu_id_count(), 1);
}

static void test_attached_ids_are_filtered_to_learned_allowed_gpu_ids(void) {
    struct rps_gpu_allowed_gpus allowed = allowed_minor_2();
    struct rps_nv_ioctl_card_info cards[RPS_NV_MAX_DEVICES];
    uint32_t gpu_ids[RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS];
    struct rps_nvos54_parameters rm;
    size_t i;

    rps_gpu_test_ioctl_filter_reset();
    teach_cards(&allowed, cards);

    for (i = 0; i < RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS; i++) {
        gpu_ids[i] = RPS_NV0000_CTRL_GPU_INVALID_ID;
    }
    gpu_ids[0] = 0xaaaa0002U;
    gpu_ids[1] = 0xbbbb0003U;

    memset(&rm, 0, sizeof(rm));
    rm.cmd = RPS_NV0000_CTRL_CMD_GPU_GET_ATTACHED_IDS;
    rm.params = (uint64_t)(uintptr_t)gpu_ids;
    rm.paramsSize = sizeof(gpu_ids);
    rm.status = 0;

    rps_gpu_ioctl_filter_after(&allowed, RPS_IOCTL_RM_CONTROL_REQUEST, &rm, 0);

    expect_u32("first id kept", gpu_ids[0], 0xaaaa0002U);
    expect_u32("second id invalidated", gpu_ids[1], RPS_NV0000_CTRL_GPU_INVALID_ID);
    expect_u32("third id invalid", gpu_ids[2], RPS_NV0000_CTRL_GPU_INVALID_ID);
}

static void test_all_denied_attached_ids_are_cleared_after_learning_allowed_ids(void) {
    struct rps_gpu_allowed_gpus allowed = allowed_minor_2();
    struct rps_nv_ioctl_card_info cards[RPS_NV_MAX_DEVICES];
    uint32_t gpu_ids[RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS];
    struct rps_nvos54_parameters rm;
    size_t i;

    rps_gpu_test_ioctl_filter_reset();
    teach_cards(&allowed, cards);

    for (i = 0; i < RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS; i++) {
        gpu_ids[i] = RPS_NV0000_CTRL_GPU_INVALID_ID;
    }
    gpu_ids[0] = 0xbbbb0003U;
    gpu_ids[1] = 0xcccc0004U;

    memset(&rm, 0, sizeof(rm));
    rm.cmd = RPS_NV0000_CTRL_CMD_GPU_GET_ATTACHED_IDS;
    rm.params = (uint64_t)(uintptr_t)gpu_ids;
    rm.paramsSize = sizeof(gpu_ids);
    rm.status = 0;

    rps_gpu_ioctl_filter_after(&allowed, RPS_IOCTL_RM_CONTROL_REQUEST, &rm, 0);

    expect_u32("all denied first id invalidated", gpu_ids[0], RPS_NV0000_CTRL_GPU_INVALID_ID);
    expect_u32("all denied second id invalidated", gpu_ids[1], RPS_NV0000_CTRL_GPU_INVALID_ID);
}

static void test_active_device_ids_are_filtered_to_learned_allowed_gpu_ids(void) {
    struct rps_gpu_allowed_gpus allowed = allowed_minor_2();
    struct rps_nv_ioctl_card_info cards[RPS_NV_MAX_DEVICES];
    struct rps_nv0000_ctrl_gpu_get_active_device_ids_params active;
    struct rps_nvos54_parameters rm;

    rps_gpu_test_ioctl_filter_reset();
    teach_cards(&allowed, cards);

    memset(&active, 0, sizeof(active));
    active.numDevices = 2;
    active.devices[0].gpuId = 0xbbbb0003U;
    active.devices[0].gpuInstanceId = 7;
    active.devices[0].computeInstanceId = 8;
    active.devices[1].gpuId = 0xaaaa0002U;
    active.devices[1].gpuInstanceId = 9;
    active.devices[1].computeInstanceId = 10;

    memset(&rm, 0, sizeof(rm));
    rm.cmd = RPS_NV0000_CTRL_CMD_GPU_GET_ACTIVE_DEVICE_IDS;
    rm.params = (uint64_t)(uintptr_t)&active;
    rm.paramsSize = sizeof(active);
    rm.status = 0;

    rps_gpu_ioctl_filter_after(&allowed, RPS_IOCTL_RM_CONTROL_REQUEST, &rm, 0);

    expect_u32("active count compacted", active.numDevices, 1);
    expect_u32("active allowed gpu kept", active.devices[0].gpuId, 0xaaaa0002U);
    expect_u32("active instance preserved", active.devices[0].gpuInstanceId, 9);
    expect_u32("active compute instance preserved", active.devices[0].computeInstanceId, 10);
}

static void test_all_denied_active_device_ids_are_cleared_after_learning_allowed_ids(void) {
    struct rps_gpu_allowed_gpus allowed = allowed_minor_2();
    struct rps_nv_ioctl_card_info cards[RPS_NV_MAX_DEVICES];
    struct rps_nv0000_ctrl_gpu_get_active_device_ids_params active;
    struct rps_nvos54_parameters rm;

    rps_gpu_test_ioctl_filter_reset();
    teach_cards(&allowed, cards);

    memset(&active, 0, sizeof(active));
    active.numDevices = 2;
    active.devices[0].gpuId = 0xbbbb0003U;
    active.devices[0].gpuInstanceId = 7;
    active.devices[0].computeInstanceId = 8;
    active.devices[1].gpuId = 0xcccc0004U;
    active.devices[1].gpuInstanceId = 9;
    active.devices[1].computeInstanceId = 10;

    memset(&rm, 0, sizeof(rm));
    rm.cmd = RPS_NV0000_CTRL_CMD_GPU_GET_ACTIVE_DEVICE_IDS;
    rm.params = (uint64_t)(uintptr_t)&active;
    rm.paramsSize = sizeof(active);
    rm.status = 0;

    rps_gpu_ioctl_filter_after(&allowed, RPS_IOCTL_RM_CONTROL_REQUEST, &rm, 0);

    expect_u32("all denied active count cleared", active.numDevices, 0);
    expect_u32("all denied active first cleared", active.devices[0].gpuId, 0);
}

static void test_unfiltered_rm_commands_remain_unchanged(void) {
    struct rps_gpu_allowed_gpus allowed = allowed_minor_2();
    struct rps_nv_ioctl_card_info cards[RPS_NV_MAX_DEVICES];
    uint32_t gpu_ids[RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS];
    struct rps_nvos54_parameters rm;
    size_t i;

    rps_gpu_test_ioctl_filter_reset();
    teach_cards(&allowed, cards);

    for (i = 0; i < RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS; i++) {
        gpu_ids[i] = RPS_NV0000_CTRL_GPU_INVALID_ID;
    }
    gpu_ids[0] = 0xaaaa0002U;
    gpu_ids[1] = 0xbbbb0003U;

    memset(&rm, 0, sizeof(rm));
    rm.params = (uint64_t)(uintptr_t)gpu_ids;
    rm.paramsSize = sizeof(gpu_ids);
    rm.status = 0;

    rm.cmd = RPS_NV0000_CTRL_CMD_GPU_GET_PROBED_IDS;
    rps_gpu_ioctl_filter_after(&allowed, RPS_IOCTL_RM_CONTROL_REQUEST, &rm, 0);
    expect_u32("probed first unchanged", gpu_ids[0], 0xaaaa0002U);
    expect_u32("probed second unchanged", gpu_ids[1], 0xbbbb0003U);

    rm.cmd = RPS_NV0000_CTRL_CMD_GPU_ATTACH_IDS;
    rps_gpu_ioctl_filter_after(&allowed, RPS_IOCTL_RM_CONTROL_REQUEST, &rm, 0);
    expect_u32("attach first unchanged", gpu_ids[0], 0xaaaa0002U);
    expect_u32("attach second unchanged", gpu_ids[1], 0xbbbb0003U);
}

static int run_enabled_tests(void) {
    test_card_info_records_and_hides_disallowed_minors();
    test_attached_ids_are_filtered_to_learned_allowed_gpu_ids();
    test_all_denied_attached_ids_are_cleared_after_learning_allowed_ids();
    test_active_device_ids_are_filtered_to_learned_allowed_gpu_ids();
    test_all_denied_active_device_ids_are_cleared_after_learning_allowed_ids();
    test_unfiltered_rm_commands_remain_unchanged();
    return failures == 0 ? 0 : 1;
}

static int run_disabled_tests(void) {
    struct rps_gpu_allowed_gpus allowed = allowed_minor_2();
    struct rps_nv_ioctl_card_info cards[RPS_NV_MAX_DEVICES];
    uint32_t gpu_ids[RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS];
    struct rps_nvos54_parameters rm;
    size_t i;

    rps_gpu_test_ioctl_filter_reset();
    teach_cards(&allowed, cards);
    expect_int("disabled still learns allowed id", rps_gpu_test_ioctl_allowed_gpu_id_count(), 1);

    for (i = 0; i < RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS; i++) {
        gpu_ids[i] = RPS_NV0000_CTRL_GPU_INVALID_ID;
    }
    gpu_ids[0] = 0xaaaa0002U;
    gpu_ids[1] = 0xbbbb0003U;

    memset(&rm, 0, sizeof(rm));
    rm.cmd = RPS_NV0000_CTRL_CMD_GPU_GET_ATTACHED_IDS;
    rm.params = (uint64_t)(uintptr_t)gpu_ids;
    rm.paramsSize = sizeof(gpu_ids);
    rm.status = 0;

    rps_gpu_ioctl_filter_after(&allowed, RPS_IOCTL_RM_CONTROL_REQUEST, &rm, 0);

    expect_u32("first id unchanged", gpu_ids[0], 0xaaaa0002U);
    expect_u32("second id unchanged", gpu_ids[1], 0xbbbb0003U);
    return failures == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc > 1 && strcmp(argv[1], "disabled") == 0) {
        return run_disabled_tests();
    }
    return run_enabled_tests();
}
EOF

cc -std=c11 -D_GNU_SOURCE -DRUNPOD_SHIM_TESTING -Wall -Wextra -Werror -Isrc \
    build/tests/test_ioctl_filter.c src/gpu/ioctl_filter.c src/core/config.c src/core/log.c \
    -pthread -o build/tests/test_ioctl_filter
build/tests/test_ioctl_filter
RUNPOD_SHIM_GPU_IOCTL=0 build/tests/test_ioctl_filter disabled
