#include "../../src/core/fd_map.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int print_fd(int fd) {
    int rc = 0;
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        rc = 3;
    } else {
        buf[n] = '\0';
        fputs(buf, stdout);
    }
    close(fd);
    return rc;
}

static int print_meminfo(void) {
    int fd = open("/proc/meminfo", O_RDONLY);
    if (fd < 0) {
        return 2;
    }
    return print_fd(fd);
}

static int print_meminfo_stdio(void) {
    char line[1024];
    FILE *fp = fopen("/proc/meminfo", "r");

    if (fp == NULL) {
        return 2;
    }
    while (fgets(line, sizeof(line), fp) != NULL) {
        fputs(line, stdout);
    }
    if (ferror(fp)) {
        fclose(fp);
        return 3;
    }
    fclose(fp);
    return 0;
}

static int dup_over_meminfo(const char *mode, const char *path) {
    int memfd = open("/proc/meminfo", O_RDONLY);
    int realfd;
    int rc;

    if (memfd < 0) {
        return 2;
    }
    realfd = open(path, O_RDONLY);
    if (realfd < 0) {
        close(memfd);
        return 4;
    }

    if (strcmp(mode, "dup2-stale") == 0) {
        rc = dup2(realfd, memfd);
    } else {
        rc = dup3(realfd, memfd, 0);
    }
    close(realfd);
    if (rc < 0) {
        close(memfd);
        return 5;
    }

    return print_fd(memfd);
}

static int fd_map_zero_read(void) {
    ssize_t out = -1;

    if (!rps_fd_map_put(123, "abc", 3)) {
        return 10;
    }
    if (!rps_fd_map_take_read(123, NULL, 0, &out) || out != 0) {
        rps_fd_map_remove(123);
        return 11;
    }
    rps_fd_map_remove(123);
    puts("zero-ok");
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 1) {
        return print_meminfo();
    }
    if (argc == 2 && strcmp(argv[1], "fd-map-zero") == 0) {
        return fd_map_zero_read();
    }
    if (argc == 2 && strcmp(argv[1], "fopen") == 0) {
        return print_meminfo_stdio();
    }
    if (argc == 3 &&
        (strcmp(argv[1], "dup2-stale") == 0 || strcmp(argv[1], "dup3-stale") == 0)) {
        return dup_over_meminfo(argv[1], argv[2]);
    }
    return 64;
}
