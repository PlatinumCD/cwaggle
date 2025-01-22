#ifndef WAGGLE_WAGGLEMSG_H
#define WAGGLE_WAGGLEMSG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * A lightweight structure to represent the Waggle message:
 *   name, value, timestamp, meta
 */
typedef struct WaggleMsg {
    char   *name;
    char   *value;     // For simplicity, store everything as string. (Could expand for numeric, etc.)
    int64_t timestamp;
    // For the meta dictionary, we store as a single JSON-encoded string for simplicity.
    // Real code might store as a map or cJSON pointer, etc.
    char   *meta;
} WaggleMsg;

/**
 * Allocates a new WaggleMsg with given fields, duplicating strings.
 * Returns NULL on failure.
 */
WaggleMsg* wagglemsg_new(const char *name,
                         const char *value,
                         int64_t timestamp,
                         const char *meta_json);

/**
 * Frees all memory for a WaggleMsg.
 */
void wagglemsg_free(WaggleMsg *m);

/**
 * Serializes a WaggleMsg into JSON. Caller must free the returned string.
 * Returns NULL on failure.
 *
 * Example JSON:
 * {
 *   "name": "some.metric",
 *   "val":  "1234",
 *   "ts":   1234567890000000000,
 *   "meta": { "label1": "foo", ... }
 * }
 */
char* wagglemsg_dump_json(const WaggleMsg *m);

/**
 * Deserializes JSON into a WaggleMsg structure.
 * Returns NULL on failure.
 */
WaggleMsg* wagglemsg_load_json(const char *json_str);

#ifdef __cplusplus
}
#endif

#endif
