#include "../../src/cgroup/cgroup.h"

#include <stdio.h>

int main(void) {
    struct rps_cgroup_memory mem;
    if (!rps_cgroup_memory_detect(&mem)) {
        puts("unlimited");
        return 0;
    }
    printf("version=%d limit=%llu current=%llu\n",
           mem.version,
           (unsigned long long)mem.limit_bytes,
           (unsigned long long)mem.current_bytes);
    return 0;
}
