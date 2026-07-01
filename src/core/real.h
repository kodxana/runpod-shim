#ifndef RUNPOD_SHIM_REAL_H
#define RUNPOD_SHIM_REAL_H

#include <dirent.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

struct rps_real {
    int (*open)(const char *path, int flags, ...);
    int (*open64)(const char *path, int flags, ...);
    int (*openat)(int dirfd, const char *path, int flags, ...);
    FILE *(*fopen)(const char *path, const char *mode);
    FILE *(*fopen64)(const char *path, const char *mode);
    FILE *(*fdopen)(int fd, const char *mode);
    int (*close)(int fd);
    ssize_t (*read)(int fd, void *buf, size_t count);
    int (*dup)(int oldfd);
    int (*dup2)(int oldfd, int newfd);
    int (*dup3)(int oldfd, int newfd, int flags);
    int (*fcntl)(int fd, int cmd, ...);
    long (*sysconf)(int name);
    int (*sysinfo)(struct sysinfo *info);
    int (*stat)(const char *path, struct stat *statbuf);
    int (*fstat)(int fd, struct stat *statbuf);
    int (*ioctl)(int fd, unsigned long request, ...);
    DIR *(*opendir)(const char *name);
    struct dirent *(*readdir)(DIR *dirp);
    int (*closedir)(DIR *dirp);
};

const struct rps_real *rps_real_get(void);

#endif
