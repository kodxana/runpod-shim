#ifndef RUNPOD_SHIM_GUARD_H
#define RUNPOD_SHIM_GUARD_H

#include <stdbool.h>

bool rps_guard_enter(void);
void rps_guard_leave(void);

#endif
