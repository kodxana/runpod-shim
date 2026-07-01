#ifndef RUNPOD_SHIM_GPU_IOCTL_FILTER_H
#define RUNPOD_SHIM_GPU_IOCTL_FILTER_H

#include "proc_filter.h"

void rps_gpu_ioctl_filter_after(const struct rps_gpu_allowed_gpus *allowed,
                                unsigned long request,
                                void *arg,
                                int result);

#ifdef RUNPOD_SHIM_TESTING
void rps_gpu_test_ioctl_filter_reset(void);
int rps_gpu_test_ioctl_allowed_gpu_id_count(void);
#endif

#endif
