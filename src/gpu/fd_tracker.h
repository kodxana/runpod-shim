#ifndef RUNPOD_SHIM_GPU_FD_TRACKER_H
#define RUNPOD_SHIM_GPU_FD_TRACKER_H

#define RPS_GPU_PATH_MAX 256
#define RPS_GPU_BUS_ID_MAX 32

enum rps_gpu_fd_kind {
    RPS_GPU_FD_OTHER,
    RPS_GPU_FD_NVIDIA_CTL,
    RPS_GPU_FD_NVIDIA_DEVICE,
    RPS_GPU_FD_NVIDIA_UVM,
    RPS_GPU_FD_PROC_GPUS_DIR,
    RPS_GPU_FD_PROC_GPU_BUS_DIR,
    RPS_GPU_FD_PROC_GPU_INFO
};

struct rps_gpu_fd_record {
    enum rps_gpu_fd_kind kind;
    int device_minor;
    char path[RPS_GPU_PATH_MAX];
    char bus_id[RPS_GPU_BUS_ID_MAX];
};

int rps_gpu_classify_path(const char *path, struct rps_gpu_fd_record *record);
void rps_gpu_fd_map_clear(void);
void rps_gpu_fd_map_set(int fd, const struct rps_gpu_fd_record *record);
int rps_gpu_fd_map_get(int fd, struct rps_gpu_fd_record *record);
void rps_gpu_fd_map_remove(int fd);
void rps_gpu_fd_map_copy(int oldfd, int newfd);

#endif
