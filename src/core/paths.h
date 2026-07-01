#ifndef RUNPOD_SHIM_PATHS_H
#define RUNPOD_SHIM_PATHS_H

#include <stddef.h>

int rps_proc_path(char *buf, size_t buflen, const char *path);
int rps_cgroup_path(char *buf,
                    size_t buflen,
                    const char *mount_point,
                    const char *mount_root,
                    const char *rel_path,
                    const char *file);

#endif
