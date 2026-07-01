#ifndef RUNPOD_SHIM_MEMORY_H
#define RUNPOD_SHIM_MEMORY_H

#include <stdbool.h>
#include <stdint.h>

struct rps_memory_snapshot {
    uint64_t total;
    uint64_t available;
};

bool rps_memory_snapshot(struct rps_memory_snapshot *out);

#endif
