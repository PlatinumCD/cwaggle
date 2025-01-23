#include "waggle/filepublisher.h"
#include "waggle/wagglemsg.h"
#include <cjson/cJSON.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct FilePublisher {
    FILE *f;
};

/**
 * Converts nanosecond-since-epoch to ISO8601 with up to 9-digit nanos, e.g.:
 *   "2025-01-01T12:34:56.123456789Z"
 */
static void isoformat_time_ns(int64_t ts, char *buf, size_t bufsize) {
    if (!buf || bufsize < 1) {
        return;
    }
    time_t secs = (time_t)(ts / 1000000000LL);
    long nanos = (long)(ts % 1000000000LL);

    struct tm tmv;
    gmtime_r(&secs, &tmv); // convert to UTC

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
    if (!logdir) {
        return NULL;
    }

    FilePublisher *fp = (FilePublisher*)calloc(1, sizeof(FilePublisher));
    if (!fp) {
        return NULL;
    }

    // Build path "<logdir>/data.ndjson"
    char path[1024];
    snprintf(path, sizeof(path), "%s/data.ndjson", logdir);

    fp->f = fopen(path, "a"); // append mode
    if (!fp->f) {
        free(fp);
        return NULL;
    }

    return fp;
}

void filepublisher_free(FilePublisher *fp) {
    if (!fp) {
        return;
    }
    if (fp->f) {
        fclose(fp->f);
    }
    free(fp);
}

int filepublisher_log(FilePublisher *fp, const WaggleMsg *msg) {
    if (!fp || !fp->f || !msg) {
        return -1;
    }

    // Skip logging if name="upload" to match Python plugin behavior
    if (strcmp(msg->name, "upload") == 0) {
        return 0;
    }

    // 1) Convert WaggleMsg -> JSON string
    //    e.g. { "name":..., "val":..., "ts":..., "meta":{...} }
    char *raw_json = wagglemsg_dump_json(msg);
    if (!raw_json) {
        return -2;
    }

    // 2) Parse that JSON so we can replace "ts" with "timestamp" in ISO8601
    cJSON *root = cJSON_Parse(raw_json);
    free(raw_json);
    if (!root) {
        return -3;
    }

    // Remove old numeric "ts" 
    cJSON_DeleteItemFromObject(root, "ts");

    // Add "timestamp" field as ISO8601
    char iso_buf[64];
    isoformat_time_ns(msg->timestamp, iso_buf, sizeof(iso_buf));
    cJSON_AddStringToObject(root, "timestamp", iso_buf);

    // 3) Write final line to the file as NDJSON
    char *final_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (!final_str) {
        return -4;
    }
    fprintf(fp->f, "%s\n", final_str);
    fflush(fp->f);
    free(final_str);

    return 0;
}
