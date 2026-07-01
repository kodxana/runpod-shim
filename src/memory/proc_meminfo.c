#include "proc_meminfo.h"

#include "memory.h"
#include "../core/log.h"
#include "../core/paths.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bool append_bytes(char *buf, size_t buflen, size_t *used, const char *data, size_t len) {
    if (len > buflen || *used > buflen - len) {
        return false;
    }
    memcpy(buf + *used, data, len);
    *used += len;
    return true;
}

static bool append_rewritten_line(char *buf,
                                  size_t buflen,
                                  size_t *used,
                                  const char *key,
                                  uint64_t value_kb,
                                  bool newline) {
    char line[128];
    int written = snprintf(line,
                           sizeof(line),
                           newline ? "%s%16llu kB\n" : "%s%16llu kB",
                           key,
                           (unsigned long long)value_kb);

    if (written < 0 || (size_t)written >= sizeof(line)) {
        return false;
    }
    return append_bytes(buf, buflen, used, line, (size_t)written);
}

static FILE *open_meminfo(char *path, size_t path_len) {
    FILE *fp;

    if (rps_proc_path(path, path_len, "/proc/meminfo") != 0) {
        rps_trace("proc meminfo build path overflow");
        return NULL;
    }

    fp = fopen(path, "r");
    if (fp == NULL && strcmp(path, "/proc/meminfo") != 0 &&
        rps_proc_path(path, path_len, "/proc/self/meminfo") == 0) {
        fp = fopen(path, "r");
    }
    if (fp == NULL) {
        rps_trace("proc meminfo build open failed path=%s", path);
    }
    return fp;
}

bool rps_proc_meminfo_build(char *buf, size_t buflen, size_t *out_len) {
    struct rps_memory_snapshot snapshot;
    char path[512];
    char line[1024];
    FILE *fp;
    size_t used = 0;
    uint64_t total_kb = 0;
    uint64_t available_kb = 0;

    if (buf == NULL || out_len == NULL) {
        rps_trace("proc meminfo build invalid arguments");
        return false;
    }
    if (!rps_memory_snapshot(&snapshot)) {
        rps_trace("proc meminfo build missing memory snapshot");
        return false;
    }

    fp = open_meminfo(path, sizeof(path));
    if (fp == NULL) {
        return false;
    }

    total_kb = snapshot.total / 1024U;
    available_kb = snapshot.available / 1024U;

    while (fgets(line, sizeof(line), fp) != NULL) {
        bool newline = strchr(line, '\n') != NULL;

        if (strncmp(line, "MemTotal:", 9) == 0) {
            if (!append_rewritten_line(buf, buflen, &used, "MemTotal:", total_kb, newline)) {
                rps_trace("proc meminfo build overflow rewriting MemTotal");
                fclose(fp);
                return false;
            }
        } else if (strncmp(line, "MemAvailable:", 13) == 0) {
            if (!append_rewritten_line(buf, buflen, &used, "MemAvailable:", available_kb, newline)) {
                rps_trace("proc meminfo build overflow rewriting MemAvailable");
                fclose(fp);
                return false;
            }
        } else if (strncmp(line, "MemFree:", 8) == 0) {
            if (!append_rewritten_line(buf, buflen, &used, "MemFree:", available_kb, newline)) {
                rps_trace("proc meminfo build overflow rewriting MemFree");
                fclose(fp);
                return false;
            }
        } else if (!append_bytes(buf, buflen, &used, line, strlen(line))) {
            rps_trace("proc meminfo build overflow preserving line");
            fclose(fp);
            return false;
        }
    }

    if (ferror(fp)) {
        rps_trace("proc meminfo build read error");
        fclose(fp);
        return false;
    }
    fclose(fp);
    *out_len = used;
    return true;
}
