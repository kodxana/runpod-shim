#include "fd_tracker.h"

#include "proc_filter.h"

#include <limits.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct rps_gpu_fd_node {
    int fd;
    struct rps_gpu_fd_record record;
    struct rps_gpu_fd_node *next;
};

static pthread_mutex_t g_fd_map_lock = PTHREAD_MUTEX_INITIALIZER;
static struct rps_gpu_fd_node *g_fd_map_head;

static void rps_gpu_copy_string(char *dst, size_t dst_size, const char *src) {
    size_t copy_len;

    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    copy_len = strlen(src);
    if (copy_len >= dst_size) {
        copy_len = dst_size - 1;
    }
    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

static void rps_gpu_copy_slice(char *dst, size_t dst_size, const char *src, size_t len) {
    size_t copy_len;

    if (dst == NULL || dst_size == 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    copy_len = len;
    if (copy_len >= dst_size) {
        copy_len = dst_size - 1;
    }

    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

static int rps_gpu_parse_nvidia_minor(const char *path, int *minor) {
    const char prefix[] = "/dev/nvidia";
    const char *cursor;
    int value;
    int digit;

    if (strncmp(path, prefix, sizeof(prefix) - 1) != 0) {
        return 0;
    }

    cursor = path + sizeof(prefix) - 1;
    if (*cursor < '0' || *cursor > '9') {
        return 0;
    }

    value = 0;
    while (*cursor != '\0') {
        if (*cursor < '0' || *cursor > '9') {
            return 0;
        }

        digit = *cursor - '0';
        if (value > (INT_MAX - digit) / 10) {
            return 0;
        }

        value = (value * 10) + digit;
        cursor++;
    }

    *minor = value;
    return 1;
}

static int rps_gpu_classify_proc_gpu_path(const char *path,
                                          struct rps_gpu_fd_record *record) {
    const char base[] = "/proc/driver/nvidia/gpus";
    const char info_name[] = "information";
    char normalized[RPS_GPU_PROC_PATH_MAX];
    const char *rest;
    const char *slash;
    size_t base_len;
    size_t bus_len;

    if (!rps_gpu_proc_normalize_path(path, normalized, sizeof(normalized))) {
        return 0;
    }

    if (strcmp(normalized, base) == 0) {
        record->kind = RPS_GPU_FD_PROC_GPUS_DIR;
        rps_gpu_copy_string(record->path, sizeof(record->path), normalized);
        return 1;
    }

    base_len = sizeof(base) - 1;
    if (strncmp(normalized, base, base_len) != 0 ||
        normalized[base_len] != '/') {
        return 0;
    }

    rest = normalized + base_len + 1;
    slash = strchr(rest, '/');
    bus_len = slash == NULL ? strlen(rest) : (size_t)(slash - rest);
    if (bus_len == 0 || bus_len >= sizeof(record->bus_id)) {
        return 0;
    }

    if (slash == NULL) {
        record->kind = RPS_GPU_FD_PROC_GPU_BUS_DIR;
        rps_gpu_copy_slice(record->bus_id, sizeof(record->bus_id), rest, bus_len);
        rps_gpu_copy_string(record->path, sizeof(record->path), normalized);
        return 1;
    }

    if (strcmp(slash + 1, info_name) == 0) {
        record->kind = RPS_GPU_FD_PROC_GPU_INFO;
        rps_gpu_copy_slice(record->bus_id, sizeof(record->bus_id), rest, bus_len);
        rps_gpu_copy_string(record->path, sizeof(record->path), normalized);
        return 1;
    }

    return 0;
}

int rps_gpu_classify_path(const char *path, struct rps_gpu_fd_record *record) {
    int minor;

    if (record == NULL) {
        return 0;
    }

    memset(record, 0, sizeof(*record));
    record->kind = RPS_GPU_FD_OTHER;
    record->device_minor = -1;
    rps_gpu_copy_string(record->path, sizeof(record->path), path);

    if (path == NULL) {
        return 0;
    }

    if (strcmp(path, "/dev/nvidiactl") == 0) {
        record->kind = RPS_GPU_FD_NVIDIA_CTL;
        return 1;
    }

    if (strcmp(path, "/dev/nvidia-uvm") == 0) {
        record->kind = RPS_GPU_FD_NVIDIA_UVM;
        return 1;
    }

    if (rps_gpu_parse_nvidia_minor(path, &minor)) {
        record->kind = RPS_GPU_FD_NVIDIA_DEVICE;
        record->device_minor = minor;
        return 1;
    }

    return rps_gpu_classify_proc_gpu_path(path, record);
}

void rps_gpu_fd_map_clear(void) {
    struct rps_gpu_fd_node *node;

    pthread_mutex_lock(&g_fd_map_lock);
    node = g_fd_map_head;
    while (node != NULL) {
        struct rps_gpu_fd_node *next = node->next;

        free(node);
        node = next;
    }
    g_fd_map_head = NULL;
    pthread_mutex_unlock(&g_fd_map_lock);
}

void rps_gpu_fd_map_set(int fd, const struct rps_gpu_fd_record *record) {
    struct rps_gpu_fd_node *node;

    if (fd < 0 || record == NULL) {
        return;
    }

    pthread_mutex_lock(&g_fd_map_lock);
    for (node = g_fd_map_head; node != NULL; node = node->next) {
        if (node->fd == fd) {
            node->record = *record;
            pthread_mutex_unlock(&g_fd_map_lock);
            return;
        }
    }

    node = malloc(sizeof(*node));
    if (node != NULL) {
        node->fd = fd;
        node->record = *record;
        node->next = g_fd_map_head;
        g_fd_map_head = node;
    }
    pthread_mutex_unlock(&g_fd_map_lock);
}

int rps_gpu_fd_map_get(int fd, struct rps_gpu_fd_record *record) {
    struct rps_gpu_fd_node *node;

    if (fd < 0 || record == NULL) {
        return 0;
    }

    pthread_mutex_lock(&g_fd_map_lock);
    for (node = g_fd_map_head; node != NULL; node = node->next) {
        if (node->fd == fd) {
            *record = node->record;
            pthread_mutex_unlock(&g_fd_map_lock);
            return 1;
        }
    }
    pthread_mutex_unlock(&g_fd_map_lock);

    return 0;
}

void rps_gpu_fd_map_remove(int fd) {
    struct rps_gpu_fd_node **link;

    if (fd < 0) {
        return;
    }

    pthread_mutex_lock(&g_fd_map_lock);
    link = &g_fd_map_head;
    while (*link != NULL) {
        struct rps_gpu_fd_node *node = *link;

        if (node->fd == fd) {
            *link = node->next;
            free(node);
            break;
        }
        link = &node->next;
    }
    pthread_mutex_unlock(&g_fd_map_lock);
}

void rps_gpu_fd_map_copy(int oldfd, int newfd) {
    struct rps_gpu_fd_node *old_node;
    struct rps_gpu_fd_node *new_node;

    if (oldfd < 0 || newfd < 0) {
        return;
    }

    pthread_mutex_lock(&g_fd_map_lock);
    old_node = NULL;
    new_node = NULL;
    for (struct rps_gpu_fd_node *node = g_fd_map_head; node != NULL; node = node->next) {
        if (node->fd == oldfd) {
            old_node = node;
        }
        if (node->fd == newfd) {
            new_node = node;
        }
    }

    if (old_node != NULL) {
        if (new_node == NULL) {
            new_node = malloc(sizeof(*new_node));
            if (new_node == NULL) {
                pthread_mutex_unlock(&g_fd_map_lock);
                return;
            }
            new_node->fd = newfd;
            new_node->next = g_fd_map_head;
            g_fd_map_head = new_node;
        }
        new_node->record = old_node->record;
        pthread_mutex_unlock(&g_fd_map_lock);
        return;
    }

    if (new_node != NULL) {
        struct rps_gpu_fd_node **link = &g_fd_map_head;

        while (*link != NULL) {
            if (*link == new_node) {
                *link = new_node->next;
                free(new_node);
                break;
            }
            link = &(*link)->next;
        }
    }
    pthread_mutex_unlock(&g_fd_map_lock);
}
