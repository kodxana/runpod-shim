#ifndef RUNPOD_SHIM_LOG_H
#define RUNPOD_SHIM_LOG_H

#if defined(__GNUC__) || defined(__clang__)
#define RPS_PRINTF_FORMAT(fmt_index, arg_index) \
    __attribute__((format(printf, fmt_index, arg_index)))
#else
#define RPS_PRINTF_FORMAT(fmt_index, arg_index)
#endif

void rps_trace(const char *fmt, ...) RPS_PRINTF_FORMAT(1, 2);

#endif
