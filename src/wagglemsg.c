#include "waggle/wagglemsg.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

WaggleMsg* wagglemsg_new(const char *name,
                         int64_t value,
                         uint64_t timestamp,
                         const char *meta_json) {
    if (!name || !meta_json) {
        return NULL;
    }
    WaggleMsg *m = (WaggleMsg*)calloc(1, sizeof(WaggleMsg));
    if (!m) {
        return NULL;
    }
    m->name = strdup(name);
    m->value = value;
    m->timestamp = timestamp;
    m->meta = strdup(meta_json);
    if (!m->name || !m->meta) {
        free(m->name);
        free(m->meta);
        free(m);
        return NULL;
    }
    return m;
}

void wagglemsg_free(WaggleMsg *m) {
    if (m) {
        free(m->name);
        free(m->meta);
        free(m);
    }
}

char* wagglemsg_dump_json(const WaggleMsg *m) {
    if (!m) {
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }

    // Add name as a string
    cJSON_AddStringToObject(root, "name", m->name);

    // For val and ts, add them as "raw" text to avoid scientific notation
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%" PRId64, m->value);
        cJSON_AddRawToObject(root, "val", buf);
    }
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%" PRIu64, (uint64_t)m->timestamp);
        cJSON_AddRawToObject(root, "ts", buf);
    }

    // Parse meta JSON and attach
    cJSON *meta_obj = cJSON_Parse(m->meta);
    if (!meta_obj) {
        meta_obj = cJSON_CreateObject();
    }
    cJSON_AddItemToObject(root, "meta", meta_obj);

    // Convert to unformatted string
    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out; // caller must free
}

WaggleMsg* wagglemsg_load_json(const char *json_str) {
    if (!json_str) {
        return NULL;
    }

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        return NULL;
    }

    cJSON *name = cJSON_GetObjectItem(root, "name");
    cJSON *val  = cJSON_GetObjectItem(root, "val");
    cJSON *ts   = cJSON_GetObjectItem(root, "ts");
    cJSON *meta = cJSON_GetObjectItem(root, "meta");

    // name must be string, val/ts can be raw or number
    if (!cJSON_IsString(name) || !val || !ts) {
        cJSON_Delete(root);
        return NULL;
    }

    // Extract val
    int64_t int64_val = 0;
    if (cJSON_IsNumber(val)) {
        int64_val = (int64_t)val->valuedouble; // fallback if cJSON parsed as number
    } else if (cJSON_IsRaw(val) || cJSON_IsString(val)) {
        int64_val = strtoll(val->valuestring, NULL, 10);
    } else {
        cJSON_Delete(root);
        return NULL;
    }

    // Extract timestamp
    uint64_t uint64_ts = 0;
    if (cJSON_IsNumber(ts)) {
        uint64_ts = (uint64_t)ts->valuedouble;
    } else if (cJSON_IsRaw(ts) || cJSON_IsString(ts)) {
        uint64_ts = strtoull(ts->valuestring, NULL, 10);
    } else {
        cJSON_Delete(root);
        return NULL;
    }

    // Convert meta to string
    char *meta_str = NULL;
    if (meta && (cJSON_IsObject(meta) || cJSON_IsArray(meta))) {
        meta_str = cJSON_PrintUnformatted(meta);
    } else {
        meta_str = strdup("{}");
    }

    WaggleMsg *m = wagglemsg_new(
        name->valuestring,
        int64_val,
        uint64_ts,
        meta_str ? meta_str : "{}"
    );
    free(meta_str);

    cJSON_Delete(root);
    return m;
}
