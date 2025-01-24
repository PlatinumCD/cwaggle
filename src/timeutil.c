#include "waggle/timeutil.h"
#include <time.h>
#include <stdio.h>

#ifdef DEBUG
  #define DBGPRINT(...) do { fprintf(stderr, "[DEBUG timeutil] "); fprintf(stderr, __VA_ARGS__); } while(0)
#else
  #define DBGPRINT(...) do {} while(0)
#endif

uint64_t waggle_get_timestamp_ns(void) {
    DBGPRINT("waggle_get_timestamp_ns() called.\n");
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

void waggle_format_time(char *buf, int bufsize) {
    DBGPRINT("waggle_format_time() called.\n");
    if (!buf || bufsize < 1) {
        DBGPRINT("Invalid buffer or bufsize.\n");
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
