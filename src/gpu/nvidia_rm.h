#ifndef RUNPOD_SHIM_NVIDIA_RM_H
#define RUNPOD_SHIM_NVIDIA_RM_H

#include <stddef.h>
#include <stdint.h>
#include <sys/ioctl.h>

#define RPS_NV_IOCTL_MAGIC 'F'
#define RPS_NV_MAX_DEVICES 32U

#define RPS_NV_ESC_RM_CONTROL 0x2AU
#define RPS_NV_ESC_CARD_INFO 200U

#define RPS_NV0000_CTRL_CMD_GPU_GET_ATTACHED_IDS 0x201U
#define RPS_NV0000_CTRL_CMD_GPU_GET_PROBED_IDS 0x214U
#define RPS_NV0000_CTRL_CMD_GPU_ATTACH_IDS 0x215U
#define RPS_NV0000_CTRL_CMD_GPU_GET_ACTIVE_DEVICE_IDS 0x288U

#define RPS_NV0000_CTRL_GPU_MAX_ATTACHED_GPUS 32U
#define RPS_NV0000_CTRL_GPU_MAX_ACTIVE_DEVICES 256U
#define RPS_NV0000_CTRL_GPU_INVALID_ID 0xffffffffU

struct rps_nvos54_parameters {
    uint32_t hClient;
    uint32_t hObject;
    uint32_t cmd;
    uint32_t reserved0;
    uint64_t params;
    uint32_t paramsSize;
    int32_t status;
};

struct rps_nv_pci_info {
    uint32_t domain;
    uint8_t bus;
    uint8_t slot;
    uint8_t function;
    uint8_t reserved0;
    uint16_t vendor_id;
    uint16_t device_id;
};

struct rps_nv_ioctl_card_info {
    uint8_t valid;
    uint8_t reserved0[3];
    struct rps_nv_pci_info pci_info;
    uint32_t gpu_id;
    uint16_t interrupt_line;
    uint8_t reserved1[2];
    uint64_t reg_address;
    uint64_t reg_size;
    uint64_t fb_address;
    uint64_t fb_size;
    uint32_t minor_number;
    uint8_t dev_name[10];
    uint8_t reserved2[2];
};

struct rps_nv0000_ctrl_gpu_active_device {
    uint32_t gpuId;
    uint32_t gpuInstanceId;
    uint32_t computeInstanceId;
};

struct rps_nv0000_ctrl_gpu_get_active_device_ids_params {
    uint32_t numDevices;
    struct rps_nv0000_ctrl_gpu_active_device devices[RPS_NV0000_CTRL_GPU_MAX_ACTIVE_DEVICES];
};

#define RPS_IOCTL_RM_CONTROL_REQUEST \
    _IOWR(RPS_NV_IOCTL_MAGIC, RPS_NV_ESC_RM_CONTROL, struct rps_nvos54_parameters)
#define RPS_IOCTL_CARD_INFO_REQUEST \
    _IOWR(RPS_NV_IOCTL_MAGIC, \
          RPS_NV_ESC_CARD_INFO, \
          struct rps_nv_ioctl_card_info[RPS_NV_MAX_DEVICES])

_Static_assert(sizeof(struct rps_nvos54_parameters) == 32,
               "NVOS54 RM control wrapper layout must stay ABI-compatible");
_Static_assert(offsetof(struct rps_nvos54_parameters, params) == 16,
               "NVOS54 params pointer offset must stay ABI-compatible");
_Static_assert(sizeof(struct rps_nv_ioctl_card_info) == 72,
               "NVIDIA card info entry layout must stay ABI-compatible");

static inline int rps_request_is_rm_control(unsigned long request) {
    return request == (unsigned long)RPS_IOCTL_RM_CONTROL_REQUEST;
}

static inline int rps_request_is_card_info(unsigned long request) {
    return request == (unsigned long)RPS_IOCTL_CARD_INFO_REQUEST;
}

#endif
