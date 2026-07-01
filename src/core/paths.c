#include "paths.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *clean_rel(const char *path) {
    while (path[0] == '/') {
        path++;
    }
    return path;
}

static const char *clean_proc_rel(const char *path) {
    const char *proc_self = "/proc/self/";
    size_t proc_self_len = strlen(proc_self);

    if (strncmp(path, proc_self, proc_self_len) == 0) {
        return path + proc_self_len;
    }
    return clean_rel(path);
}

static const char *clean_cgroup_mount_rel(const char *mount_point) {
    const char *cgroup_root = "/sys/fs/cgroup";
    size_t cgroup_root_len = strlen(cgroup_root);

    if (strcmp(mount_point, cgroup_root) == 0) {
        return "";
    }
    if (strncmp(mount_point, cgroup_root, cgroup_root_len) == 0 &&
        mount_point[cgroup_root_len] == '/') {
        return mount_point + cgroup_root_len + 1;
    }
    return clean_rel(mount_point);
}

static int append_segment(char *buf, size_t buflen, size_t *used, const char *segment) {
    int written;

    if (segment == NULL || segment[0] == '\0') {
        return 0;
    }
    if (*used > 0 && buf[*used - 1] == '/') {
        written = snprintf(buf + *used, buflen - *used, "%s", clean_rel(segment));
    } else {
        written = snprintf(buf + *used, buflen - *used, "/%s", clean_rel(segment));
    }
    if (written < 0 || (size_t)written >= buflen - *used) {
        return -1;
    }
    *used += (size_t)written;
    return 0;
}

static bool cgroup_path_under_mount_root(const char *mount_root, const char *rel_path, const char **inside_mount) {
    size_t root_len;

    if (mount_root == NULL || mount_root[0] == '\0') {
        mount_root = "/";
    }
    if (rel_path == NULL || rel_path[0] == '\0') {
        rel_path = "/";
    }

    root_len = strlen(mount_root);
    while (root_len > 1 && mount_root[root_len - 1] == '/') {
        root_len--;
    }

    if (root_len == 1 && mount_root[0] == '/') {
        *inside_mount = rel_path;
        return true;
    }
    if (strncmp(rel_path, mount_root, root_len) != 0) {
        return false;
    }
    if (rel_path[root_len] != '\0' && rel_path[root_len] != '/') {
        return false;
    }

    *inside_mount = rel_path[root_len] == '\0' ? "/" : rel_path + root_len;
    return true;
}

int rps_proc_path(char *buf, size_t buflen, const char *path) {
    const char *root = getenv("RUNPOD_SHIM_TEST_PROC_ROOT");
    if (root == NULL || root[0] == '\0') {
        return snprintf(buf, buflen, "%s", path) < (int)buflen ? 0 : -1;
    }
    return snprintf(buf, buflen, "%s/%s", root, clean_proc_rel(path)) < (int)buflen ? 0 : -1;
}

int rps_cgroup_path(char *buf,
                    size_t buflen,
                    const char *mount_point,
                    const char *mount_root,
                    const char *rel_path,
                    const char *file) {
    const char *root = getenv("RUNPOD_SHIM_TEST_CGROUP_ROOT");
    const char *inside_mount;
    size_t used;
    int written;

    if (!cgroup_path_under_mount_root(mount_root, rel_path, &inside_mount)) {
        return -1;
    }

    if (root == NULL || root[0] == '\0') {
        root = mount_point;
        mount_point = "";
    } else {
        mount_point = clean_cgroup_mount_rel(mount_point);
    }

    written = snprintf(buf, buflen, "%s", root);
    if (written < 0 || (size_t)written >= buflen) {
        return -1;
    }
    used = (size_t)written;
    if (append_segment(buf, buflen, &used, mount_point) != 0 ||
        append_segment(buf, buflen, &used, inside_mount) != 0 ||
        append_segment(buf, buflen, &used, file) != 0) {
        return -1;
    }
    return 0;
}
