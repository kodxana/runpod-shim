#!/bin/sh
set -eu

root_dir="$(CDPATH= cd "$(dirname "$0")/.." && pwd)"
cd "$root_dir"

mkdir -p build/tests
cat >build/tests/test_gpu_ioctl_noarg_smoke.c <<'EOF'
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>

static int check_noarg_ioctl(const char *name, unsigned long request) {
    int rc;

    errno = 0;
    rc = ioctl(-1, request);
    if (rc != -1 || errno != EBADF) {
        fprintf(stderr, "ioctl(-1, %s): rc=%d errno=%d\n", name, rc, errno);
        return 1;
    }
    return 0;
}

int main(void) {
    int failures = 0;

#ifdef FIOCLEX
    failures += check_noarg_ioctl("FIOCLEX", FIOCLEX);
#endif
#ifdef FIONCLEX
    failures += check_noarg_ioctl("FIONCLEX", FIONCLEX);
#endif
#ifdef TIOCEXCL
    failures += check_noarg_ioctl("TIOCEXCL", TIOCEXCL);
#endif
#ifdef TIOCNXCL
    failures += check_noarg_ioctl("TIOCNXCL", TIOCNXCL);
#endif
#ifdef TIOCNOTTY
    failures += check_noarg_ioctl("TIOCNOTTY", TIOCNOTTY);
#endif
    return failures == 0 ? 0 : 1;
}
EOF

make
cc -std=c11 -D_GNU_SOURCE -Wall -Wextra -Werror \
    build/tests/test_gpu_ioctl_noarg_smoke.c \
    -o build/tests/test_gpu_ioctl_noarg_smoke
LD_PRELOAD="$PWD/librunpod-shim.so" build/tests/test_gpu_ioctl_noarg_smoke
