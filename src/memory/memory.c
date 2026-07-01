#include "memory.h"

#include "../cgroup/cgroup.h"

#include <pthread.h>
#include <string.h>

bool rps_memory_snapshot(struct rps_memory_snapshot *out) {
    static struct rps_cgroup_memory cgmem;
    static bool detected;
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    bool ok = false;

    if (out == NULL) {
        return false;
    }
    if (pthread_mutex_lock(&lock) != 0) {
        return false;
    }
    if (!detected) {
        if (!rps_cgroup_memory_detect(&cgmem)) {
            goto done;
        }
        detected = true;
    }
    if (!rps_cgroup_memory_refresh(&cgmem)) {
        goto done;
    }

    memset(out, 0, sizeof(*out));
    out->total = cgmem.limit_bytes;
    if (cgmem.current_bytes < cgmem.limit_bytes) {
        out->available = cgmem.limit_bytes - cgmem.current_bytes;
    }
    ok = true;

done:
    pthread_mutex_unlock(&lock);
    return ok;
}
