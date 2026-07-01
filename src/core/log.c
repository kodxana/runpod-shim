#include "log.h"

#include "config.h"

#include <stdarg.h>
#include <stdio.h>

void rps_trace(const char *fmt, ...) {
    if (!rps_config_get()->trace) {
        return;
    }

    va_list ap;
    va_start(ap, fmt);
    fputs("[runpod-shim] ", stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    va_end(ap);
}
