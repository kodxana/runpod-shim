#include "guard.h"

static __thread int depth;

bool rps_guard_enter(void) {
    if (depth != 0) {
        return false;
    }
    depth++;
    return true;
}

void rps_guard_leave(void) {
    if (depth > 0) {
        depth--;
    }
}
