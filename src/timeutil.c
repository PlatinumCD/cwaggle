#include "waggle/timeutil.h"
#include <time.h>
#include <stdio.h>

int64_t waggle_get_timestamp_ns(void) {
    // Use clock_gettime for nanosecond resolution
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void waggle_format_time(char *buf, int bufsize) {
    if (!buf || bufsize < 1) {
        return;
    }
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    snprintf(buf, bufsize, "%04d-%02d-%02d %02d:%02d:%02d",
             tmv.tm_year + 1900,
             tmv.tm_mon + 1,
             tmv.tm_mday,
             tmv.tm_hour,
             tmv.tm_min,
             tmv.tm_sec);
}
