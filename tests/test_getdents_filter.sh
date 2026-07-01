#!/bin/sh
set -eu

root_dir="$(CDPATH= cd "$(dirname "$0")/.." && pwd)"
cd "$root_dir"

mkdir -p build/tests
cat >build/tests/test_getdents_filter.c <<'EOF'
#include "gpu/proc_filter.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

struct linux_dirent64_test {
    uint64_t d_ino;
    int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};

static int failures;

static void expect_int(const char *name, int got, int want) {
    if (got != want) {
        fprintf(stderr, "%s: got %d want %d\n", name, got, want);
        failures++;
    }
}

static size_t append_dirent(char *buffer, size_t offset, const char *name) {
    struct linux_dirent64_test *entry = (struct linux_dirent64_test *)(void *)(buffer + offset);
    size_t name_len = strlen(name) + 1u;
    size_t reclen = sizeof(*entry) + name_len;

    reclen = (reclen + 7u) & ~7u;
    entry->d_ino = offset + 1u;
    entry->d_off = (int64_t)(offset + reclen);
    entry->d_reclen = (unsigned short)reclen;
    entry->d_type = 0;
    memcpy(entry->d_name, name, name_len);
    memset(entry->d_name + name_len, 0, reclen - sizeof(*entry) - name_len);

    return offset + reclen;
}

static int buffer_has_name(const char *buffer, ssize_t len, const char *name) {
    ssize_t offset = 0;

    while (offset < len) {
        const struct linux_dirent64_test *entry =
            (const struct linux_dirent64_test *)(const void *)(buffer + offset);
        if (strcmp(entry->d_name, name) == 0) {
            return 1;
        }
        offset += entry->d_reclen;
    }

    return 0;
}

static void test_filters_disallowed_gpu_bus_entries(void) {
    char buffer[512];
    size_t len = 0;
    ssize_t filtered;
    struct rps_gpu_allowed_gpus allowed;

    memset(&allowed, 0, sizeof(allowed));
    allowed.valid = 1;
    allowed.count = 1;
    allowed.gpus[0].present = 1;
    allowed.gpus[0].device_minor = 4;
    strcpy(allowed.gpus[0].bus_id, "0000:04:00.0");

    len = append_dirent(buffer, len, ".");
    len = append_dirent(buffer, len, "..");
    len = append_dirent(buffer, len, "0000:03:00.0");
    len = append_dirent(buffer, len, "0000:04:00.0");
    len = append_dirent(buffer, len, "0000:05:00.0");

    filtered = rps_gpu_filter_dirent64_buffer(buffer, (ssize_t)len, &allowed);

    expect_int("keeps dot", buffer_has_name(buffer, filtered, "."), 1);
    expect_int("keeps dotdot", buffer_has_name(buffer, filtered, ".."), 1);
    expect_int("removes first denied bus", buffer_has_name(buffer, filtered, "0000:03:00.0"), 0);
    expect_int("keeps allowed bus", buffer_has_name(buffer, filtered, "0000:04:00.0"), 1);
    expect_int("removes second denied bus", buffer_has_name(buffer, filtered, "0000:05:00.0"), 0);
}

int main(void) {
    test_filters_disallowed_gpu_bus_entries();
    return failures == 0 ? 0 : 1;
}
EOF

cc -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror -Isrc \
    build/tests/test_getdents_filter.c src/gpu/proc_filter.c \
    -o build/tests/test_getdents_filter
build/tests/test_getdents_filter
