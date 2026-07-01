#include "fd_map.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#define RPS_FD_MAP_CAPACITY 64

struct rps_fd_map_entry {
    bool active;
    int fd;
    char *data;
    size_t len;
    size_t offset;
};

static struct rps_fd_map_entry entries[RPS_FD_MAP_CAPACITY];
static pthread_mutex_t entries_lock = PTHREAD_MUTEX_INITIALIZER;

static void clear_entry(struct rps_fd_map_entry *entry) {
    free(entry->data);
    entry->active = false;
    entry->fd = -1;
    entry->data = NULL;
    entry->len = 0;
    entry->offset = 0;
}

bool rps_fd_map_put(int fd, const char *data, size_t len) {
    char *copy = NULL;
    size_t i;
    struct rps_fd_map_entry *slot = NULL;

    if (fd < 0 || (data == NULL && len > 0)) {
        return false;
    }
    if (len > 0) {
        copy = malloc(len);
        if (copy == NULL) {
            return false;
        }
        memcpy(copy, data, len);
    }

    if (pthread_mutex_lock(&entries_lock) != 0) {
        free(copy);
        return false;
    }

    for (i = 0; i < RPS_FD_MAP_CAPACITY; i++) {
        if (entries[i].active && entries[i].fd == fd) {
            slot = &entries[i];
            clear_entry(slot);
            break;
        }
    }
    if (slot == NULL) {
        for (i = 0; i < RPS_FD_MAP_CAPACITY; i++) {
            if (!entries[i].active) {
                slot = &entries[i];
                break;
            }
        }
    }
    if (slot == NULL) {
        pthread_mutex_unlock(&entries_lock);
        free(copy);
        return false;
    }

    slot->active = true;
    slot->fd = fd;
    slot->data = copy;
    slot->len = len;
    slot->offset = 0;

    pthread_mutex_unlock(&entries_lock);
    return true;
}

bool rps_fd_map_take_read(int fd, void *buf, size_t count, ssize_t *out) {
    size_t i;
    bool found = false;

    if ((buf == NULL && count > 0) || out == NULL) {
        return false;
    }
    if (pthread_mutex_lock(&entries_lock) != 0) {
        return false;
    }

    for (i = 0; i < RPS_FD_MAP_CAPACITY; i++) {
        if (entries[i].active && entries[i].fd == fd) {
            size_t remaining = entries[i].len - entries[i].offset;
            size_t nread = remaining < count ? remaining : count;

            if (nread > 0) {
                memcpy(buf, entries[i].data + entries[i].offset, nread);
                entries[i].offset += nread;
            }
            *out = (ssize_t)nread;
            found = true;
            break;
        }
    }

    pthread_mutex_unlock(&entries_lock);
    return found;
}

void rps_fd_map_remove(int fd) {
    size_t i;

    if (pthread_mutex_lock(&entries_lock) != 0) {
        return;
    }
    for (i = 0; i < RPS_FD_MAP_CAPACITY; i++) {
        if (entries[i].active && entries[i].fd == fd) {
            clear_entry(&entries[i]);
            break;
        }
    }
    pthread_mutex_unlock(&entries_lock);
}
