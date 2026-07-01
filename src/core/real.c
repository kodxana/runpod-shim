#include "real.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

static struct rps_real real_fns;
static int initialized;

static void *load_symbol(const char *name) {
    void *sym = dlsym(RTLD_NEXT, name);
    if (sym == NULL) {
        fprintf(stderr, "[runpod-shim] fatal: missing libc symbol %s\n", name);
        _Exit(127);
    }
    return sym;
}

const struct rps_real *rps_real_get(void) {
    if (!initialized) {
        real_fns.open = load_symbol("open");
        real_fns.open64 = load_symbol("open64");
        real_fns.openat = load_symbol("openat");
        real_fns.fopen = load_symbol("fopen");
        real_fns.fopen64 = load_symbol("fopen64");
        real_fns.fdopen = load_symbol("fdopen");
        real_fns.close = load_symbol("close");
        real_fns.read = load_symbol("read");
        real_fns.dup = load_symbol("dup");
        real_fns.dup2 = load_symbol("dup2");
        real_fns.dup3 = load_symbol("dup3");
        real_fns.fcntl = load_symbol("fcntl");
        real_fns.sysconf = load_symbol("sysconf");
        real_fns.sysinfo = load_symbol("sysinfo");
        real_fns.stat = load_symbol("stat");
        real_fns.fstat = load_symbol("fstat");
        real_fns.ioctl = load_symbol("ioctl");
        real_fns.opendir = load_symbol("opendir");
        real_fns.readdir = load_symbol("readdir");
        real_fns.closedir = load_symbol("closedir");
        initialized = 1;
    }
    return &real_fns;
}
