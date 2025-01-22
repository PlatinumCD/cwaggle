#include "waggle/wagglemsg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

WaggleMsg* wagglemsg_new(const char *name,
                         const char *value,
                         int64_t timestamp,
                         const char *meta_json) {
    if (!name || !value || !meta_json) {
        return NULL;
    }
    WaggleMsg *m = (WaggleMsg*)calloc(1, sizeof(WaggleMsg));
    if (!m) {
        return NULL;
    }
    m->name = strdup(name);
    m->value = strdup(value);
    m->timestamp = timestamp;
    m->meta = strdup(meta_json);
    if (!m->name || !m->value || !m->meta) {
        wagglemsg_free(m);
        return NULL;
    }
    return m;
}

void wagglemsg_free(WaggleMsg *m) {
    if (!m) {
        return;
    }
    free(m->name);
    free(m->value);
    free(m->meta);
    free(m);
}

char* wagglemsg_dump_json(const WaggleMsg *m) {
    if (!m) {
        return NULL;
    }
    // Create cJSON object
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        return NULL;
    }
    cJSON_AddStringToObject(root, "name", m->name);
    cJSON_AddStringToObject(root, "val", m->value);
    cJSON_AddNumberToObject(root, "ts", (double)m->timestamp);

    // parse meta as JSON
    cJSON *meta_obj = cJSON_Parse(m->meta);
    if (!meta_obj) {
        // If meta parse fails, use empty object
        meta_obj = cJSON_CreateObject();
    }
    cJSON_AddItemToObject(root, "meta", meta_obj);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    // cJSON_PrintUnformatted returns a malloc'd string that we must free.
    // We'll return it, so the caller can free it.
    return out;
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

    if (!cJSON_IsString(name) || !cJSON_IsString(val) || !cJSON_IsNumber(ts)) {
        cJSON_Delete(root);
        return NULL;
    }
    // convert meta to string
    char *meta_str = NULL;
    if (cJSON_IsObject(meta) || cJSON_IsArray(meta)) {
        meta_str = cJSON_PrintUnformatted(meta);
    } else {
        // if meta is absent or invalid, create empty object
        meta_str = strdup("{}");
    }
    WaggleMsg *m = wagglemsg_new(name->valuestring,
                                 val->valuestring,
                                 (int64_t)(ts->valuedouble),
                                 meta_str ? meta_str : "{}");
    free(meta_str);
    cJSON_Delete(root);
    return m;
}
