#include "cgroup.h"

#include "../core/paths.h"

#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define RPS_CGROUP_UNLIMITED_SENTINEL 0x7ffffffffffff000ULL

struct rps_cgroup_mounts {
    bool has_v2;
    bool has_v1_memory;
    char v2_root[512];
    char v2_mount[512];
    char v1_memory_root[512];
    char v1_memory_mount[512];
};

static void trim_token(char *buf) {
    char *start = buf;
    char *end;

    while (isspace((unsigned char)*start)) {
        start++;
    }
    end = start;
    while (*end != '\0' && !isspace((unsigned char)*end)) {
        end++;
    }
    *end = '\0';

    if (start != buf) {
        memmove(buf, start, strlen(start) + 1);
    }
}

static bool read_token_file(const char *path, char *buf, size_t buflen) {
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        return false;
    }
    if (fgets(buf, (int)buflen, fp) == NULL) {
        fclose(fp);
        return false;
    }
    fclose(fp);
    trim_token(buf);
    return buf[0] != '\0';
}

static bool parse_u64_token(const char *token, uint64_t *out) {
    uint64_t value = 0;
    const unsigned char *p = (const unsigned char *)token;

    if (out == NULL || token == NULL || *token == '\0') {
        return false;
    }
    while (*p != '\0') {
        uint64_t digit;

        if (!isdigit(*p)) {
            return false;
        }
        digit = (uint64_t)(*p - '0');
        if (value > (UINT64_MAX - digit) / 10U) {
            return false;
        }
        value = value * 10U + digit;
        p++;
    }
    *out = value;
    return true;
}

static bool read_limit_file(const char *path, uint64_t *out) {
    char token[64];
    uint64_t value;

    if (!read_token_file(path, token, sizeof(token))) {
        return false;
    }
    if (strcmp(token, "max") == 0) {
        return false;
    }
    if (!parse_u64_token(token, &value)) {
        return false;
    }
    if (value == 0 || value >= RPS_CGROUP_UNLIMITED_SENTINEL) {
        return false;
    }
    *out = value;
    return true;
}

static bool read_counter_file(const char *path, uint64_t *out) {
    char token[64];

    if (!read_token_file(path, token, sizeof(token))) {
        return false;
    }
    return parse_u64_token(token, out);
}

static bool controllers_include(const char *controllers, const char *needle) {
    size_t needle_len = strlen(needle);
    const char *part = controllers;

    while (*part != '\0') {
        const char *comma = strchr(part, ',');
        size_t len = comma == NULL ? strlen(part) : (size_t)(comma - part);

        if (len == needle_len && strncmp(part, needle, needle_len) == 0) {
            return true;
        }
        if (comma == NULL) {
            break;
        }
        part = comma + 1;
    }
    return false;
}

static bool copy_mount(char *buf, size_t buflen, const char *mount_point) {
    return snprintf(buf, buflen, "%s", mount_point) < (int)buflen;
}

static void parse_mountinfo_line(struct rps_cgroup_mounts *mounts, char *line) {
    char *save = NULL;
    char *token;
    char *mount_root = NULL;
    char *mount_point = NULL;
    char *mount_options = NULL;
    char *fstype = NULL;
    char *super_options = NULL;
    int field = 0;

    for (token = strtok_r(line, " ", &save);
         token != NULL;
         token = strtok_r(NULL, " ", &save)) {
        field++;
        if (field == 4) {
            mount_root = token;
        } else if (field == 5) {
            mount_point = token;
        } else if (field == 6) {
            mount_options = token;
        }
        if (strcmp(token, "-") == 0) {
            fstype = strtok_r(NULL, " ", &save);
            (void)strtok_r(NULL, " ", &save);
            super_options = strtok_r(NULL, " ", &save);
            break;
        }
    }

    if (mount_root == NULL || mount_point == NULL || fstype == NULL) {
        return;
    }
    if (!mounts->has_v2 && strcmp(fstype, "cgroup2") == 0 &&
        copy_mount(mounts->v2_root, sizeof(mounts->v2_root), mount_root) &&
        copy_mount(mounts->v2_mount, sizeof(mounts->v2_mount), mount_point)) {
        mounts->has_v2 = true;
    }
    if (!mounts->has_v1_memory && strcmp(fstype, "cgroup") == 0 &&
        (controllers_include(mount_options != NULL ? mount_options : "", "memory") ||
         controllers_include(super_options != NULL ? super_options : "", "memory")) &&
        copy_mount(mounts->v1_memory_root, sizeof(mounts->v1_memory_root), mount_root) &&
        copy_mount(mounts->v1_memory_mount, sizeof(mounts->v1_memory_mount), mount_point)) {
        mounts->has_v1_memory = true;
    }
}

static bool detect_mounts(struct rps_cgroup_mounts *mounts) {
    char mountinfo_path[512];
    char line[1024];
    FILE *fp;

    memset(mounts, 0, sizeof(*mounts));
    if (rps_proc_path(mountinfo_path, sizeof(mountinfo_path), "/proc/self/mountinfo") != 0) {
        return false;
    }
    fp = fopen(mountinfo_path, "r");
    if (fp == NULL) {
        return false;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        parse_mountinfo_line(mounts, line);
    }
    fclose(fp);
    return mounts->has_v2 || mounts->has_v1_memory;
}

static void consider_candidate(struct rps_cgroup_memory *best,
                               bool *found,
                               int version,
                               const char *mount_point,
                               const char *mount_root,
                               const char *rel_path,
                               const char *limit_file,
                               const char *current_file) {
    struct rps_cgroup_memory candidate;

    memset(&candidate, 0, sizeof(candidate));
    candidate.version = version;
    if (rps_cgroup_path(candidate.limit_path, sizeof(candidate.limit_path), mount_point, mount_root, rel_path, limit_file) != 0 ||
        rps_cgroup_path(candidate.current_path, sizeof(candidate.current_path), mount_point, mount_root, rel_path, current_file) != 0) {
        return;
    }
    if (!read_limit_file(candidate.limit_path, &candidate.limit_bytes)) {
        return;
    }
    if (!read_counter_file(candidate.current_path, &candidate.current_bytes)) {
        return;
    }
    if (!*found || candidate.limit_bytes < best->limit_bytes) {
        *best = candidate;
        *found = true;
    }
}

bool rps_cgroup_memory_detect(struct rps_cgroup_memory *out) {
    char cgroup_path[512];
    char line[1024];
    struct rps_cgroup_mounts mounts;
    FILE *fp;
    bool found = false;

    if (out == NULL) {
        return false;
    }
    memset(out, 0, sizeof(*out));
    if (!detect_mounts(&mounts)) {
        return false;
    }
    if (rps_proc_path(cgroup_path, sizeof(cgroup_path), "/proc/self/cgroup") != 0) {
        return false;
    }
    fp = fopen(cgroup_path, "r");
    if (fp == NULL) {
        return false;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        char *first;
        char *second;
        const char *hierarchy;
        const char *controllers;
        const char *rel_path;

        line[strcspn(line, "\r\n")] = '\0';
        first = strchr(line, ':');
        if (first == NULL) {
            continue;
        }
        *first = '\0';
        second = strchr(first + 1, ':');
        if (second == NULL) {
            continue;
        }
        *second = '\0';

        hierarchy = line;
        controllers = first + 1;
        rel_path = second + 1;
        if (rel_path[0] == '\0') {
            rel_path = "/";
        }

        if (strcmp(hierarchy, "0") == 0 && controllers[0] == '\0' && mounts.has_v2) {
            consider_candidate(out, &found, 2, mounts.v2_mount, mounts.v2_root, rel_path, "memory.max", "memory.current");
        }
        if (controllers_include(controllers, "memory") && mounts.has_v1_memory) {
            consider_candidate(out,
                               &found,
                               1,
                               mounts.v1_memory_mount,
                               mounts.v1_memory_root,
                               rel_path,
                               "memory.limit_in_bytes",
                               "memory.usage_in_bytes");
        }
    }

    fclose(fp);
    if (!found) {
        memset(out, 0, sizeof(*out));
    }
    return found;
}

bool rps_cgroup_memory_refresh(struct rps_cgroup_memory *mem) {
    uint64_t current;

    if (mem == NULL || mem->current_path[0] == '\0') {
        return false;
    }
    if (!read_counter_file(mem->current_path, &current)) {
        return false;
    }
    mem->current_bytes = current;
    return true;
}
