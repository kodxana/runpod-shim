#ifndef RUNPOD_SHIM_PROC_MEMINFO_H
#define RUNPOD_SHIM_PROC_MEMINFO_H

#include <stdbool.h>
#include <stddef.h>

bool rps_proc_meminfo_build(char *buf, size_t buflen, size_t *out_len);

#endif
