#ifndef WAGGLE_TIMEUTIL_H
#define WAGGLE_TIMEUTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * Returns current time in nanoseconds since Unix epoch.
 * On most Unix-like systems, uses clock_gettime().
 */
int64_t waggle_get_timestamp_ns(void);

/**
 * Returns current time in a human-readable string, e.g. "2025-01-01 10:00:00"
 * Just a helper for debugging/logging. Not required for the main logic.
 */
void waggle_format_time(char *buf, int bufsize);

#ifdef __cplusplus
}
#endif

#endif
