#include "waggle/wagglemsg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

#ifdef DEBUG
  #define DBGPRINT(...) do { fprintf(stderr, "[DEBUG wagglemsg] "); fprintf(stderr, __VA_ARGS__); } while(0)
#else
  #define DBGPRINT(...) do {} while(0)
#endif

WaggleMsg* wagglemsg_new(const char *name,
                         const char *value,
                         int64_t timestamp,
                         const char *meta_json) {
    DBGPRINT("wagglemsg_new(name=%s, value=%s, timestamp=%ld)\n",
             name ? name : "NULL", value ? value : "NULL", (long)timestamp);

    if (!name || !value || !meta_json) {
        DBGPRINT("Null arg encountered. Returning NULL.\n");
        return NULL;
    }
    WaggleMsg *m = calloc(1, sizeof(WaggleMsg));
    if (!m) {
        DBGPRINT("calloc failed.\n");
        return NULL;
    }
    m->name = strdup(name);
    m->value = strdup(value);
    m->timestamp = timestamp;
    m->meta = strdup(meta_json);

    if (!m->name || !m->value || !m->meta) {
        DBGPRINT("strdup failure. Freeing.\n");
        wagglemsg_free(m);
        return NULL;
    }
    return m;
}

void wagglemsg_free(WaggleMsg *m) {
    if (!m) return;
    DBGPRINT("wagglemsg_free() called.\n");
    free(m->name);
    free(m->value);
    free(m->meta);
    free(m);
}

char* wagglemsg_dump_json(const WaggleMsg *m) {
    if (!m) {
        DBGPRINT("wagglemsg_dump_json: m is NULL.\n");
        return NULL;
    }
    DBGPRINT("wagglemsg_dump_json: converting to JSON.\n");
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        DBGPRINT("cJSON_CreateObject failed.\n");
        return NULL;
    }

    cJSON_AddStringToObject(root, "name", m->name);
    cJSON_AddStringToObject(root, "val", m->value);
    cJSON_AddNumberToObject(root, "ts", (double)m->timestamp);

    cJSON *meta_obj = cJSON_Parse(m->meta);
    if (!meta_obj) {
        DBGPRINT("Failed to parse meta JSON. Using empty {}.\n");
        meta_obj = cJSON_CreateObject();
    }
    cJSON_AddItemToObject(root, "meta", meta_obj);

    char *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    DBGPRINT("wagglemsg_dump_json: returning JSON string.\n");
    return out;
}

WaggleMsg* wagglemsg_load_json(const char *json_str) {
    DBGPRINT("wagglemsg_load_json() called.\n");
    if (!json_str) {
        DBGPRINT("json_str is NULL.\n");
        return NULL;
    }
    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        DBGPRINT("cJSON_Parse failed.\n");
        return NULL;
    }

    cJSON *name = cJSON_GetObjectItem(root, "name");
    cJSON *val  = cJSON_GetObjectItem(root, "val");
    cJSON *ts   = cJSON_GetObjectItem(root, "ts");
    cJSON *meta = cJSON_GetObjectItem(root, "meta");

    if (!cJSON_IsString(name) || !cJSON_IsString(val) || !cJSON_IsNumber(ts)) {
        DBGPRINT("Invalid JSON fields.\n");
        cJSON_Delete(root);
        return NULL;
    }
    char *meta_str = NULL;
    if (cJSON_IsObject(meta) || cJSON_IsArray(meta)) {
        meta_str = cJSON_PrintUnformatted(meta);
    } else {
        DBGPRINT("meta is missing or invalid. Using {}.\n");
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
