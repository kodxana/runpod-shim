#ifndef RUNPOD_SHIM_FD_MAP_H
#define RUNPOD_SHIM_FD_MAP_H

#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

bool rps_fd_map_put(int fd, const char *data, size_t len);
bool rps_fd_map_take_read(int fd, void *buf, size_t count, ssize_t *out);
void rps_fd_map_remove(int fd);

#endif
