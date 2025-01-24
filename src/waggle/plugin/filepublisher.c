#include "waggle/filepublisher.h"
#include "waggle/wagglemsg.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef DEBUG
  #define DBGPRINT(...) do { fprintf(stderr, "[DEBUG filepublisher] "); fprintf(stderr, __VA_ARGS__); } while(0)
#else
  #define DBGPRINT(...) do {} while(0)
#endif

struct FilePublisher {
    FILE *f;
};

/**
 * Converts nanoseconds -> ISO8601
 */
static void isoformat_time_ns(int64_t ts, char *buf, size_t bufsize) {
    if (!buf || bufsize < 1) return;
    time_t secs = (time_t)(ts / 1000000000LL);
    long nanos = (long)(ts % 1000000000LL);

    struct tm tmv;
    gmtime_r(&secs, &tmv);

    snprintf(buf, bufsize,
             "%04d-%02d-%02dT%02d:%02d:%02d.%09ldZ",
             tmv.tm_year + 1900,
             tmv.tm_mon + 1,
             tmv.tm_mday,
             tmv.tm_hour,
             tmv.tm_min,
             tmv.tm_sec,
             nanos);
}

FilePublisher* filepublisher_new(const char *logdir) {
    DBGPRINT("filepublisher_new(logdir=%s)\n", logdir ? logdir : "NULL");
    if (!logdir) {
        DBGPRINT("No logdir. Returning NULL.\n");
        return NULL;
    }

    FilePublisher *fp = calloc(1, sizeof(FilePublisher));
    if (!fp) {
        DBGPRINT("calloc failed.\n");
        return NULL;
    }

    char path[1024];
    snprintf(path, sizeof(path), "%s/data.ndjson", logdir);

    fp->f = fopen(path, "a");
    if (!fp->f) {
        DBGPRINT("Failed to open file %s\n", path);
        free(fp);
        return NULL;
    }
    DBGPRINT("Opened filepublisher at %s\n", path);
    return fp;
}

void filepublisher_free(FilePublisher *fp) {
    DBGPRINT("filepublisher_free() called.\n");
    if (!fp) return;
    if (fp->f) fclose(fp->f);
    free(fp);
}

int filepublisher_log(FilePublisher *fp, const WaggleMsg *msg) {
    DBGPRINT("filepublisher_log() called.\n");
    if (!fp || !fp->f || !msg) {
        DBGPRINT("Invalid args.\n");
        return -1;
    }

    if (strcmp(msg->name, "upload") == 0) {
        DBGPRINT("Skipping log for msg->name='upload'.\n");
        return 0;
    }

    char *raw_json = wagglemsg_dump_json(msg);
    if (!raw_json) {
        DBGPRINT("wagglemsg_dump_json returned NULL.\n");
        return -2;
    }

    cJSON *root = cJSON_Parse(raw_json);
    free(raw_json);
    if (!root) {
        DBGPRINT("cJSON_Parse failed.\n");
        return -3;
    }

    cJSON_DeleteItemFromObject(root, "ts");

    char iso_buf[64];
    isoformat_time_ns(msg->timestamp, iso_buf, sizeof(iso_buf));
    cJSON_AddStringToObject(root, "timestamp", iso_buf);

    char *final_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!final_str) {
        DBGPRINT("cJSON_PrintUnformatted failed.\n");
        return -4;
    }
    fprintf(fp->f, "%s\n", final_str);
    fflush(fp->f);
    free(final_str);

    return 0;
}
