#ifndef WAGGLE_FILEPUBLISHER_H
#define WAGGLE_FILEPUBLISHER_H

#include <stdint.h>
#include "wagglemsg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opaque struct for our FilePublisher object.
 */
typedef struct FilePublisher FilePublisher;

/**
 * Creates a new FilePublisher that logs to `<logdir>/data.ndjson`.
 * Returns NULL on error (e.g. can't open file).
 */
FilePublisher* filepublisher_new(const char *logdir);

/**
 * Closes the FilePublisher's file and frees memory.
 * Safe to call with NULL.
 */
void filepublisher_free(FilePublisher *fp);

/**
 * Logs the WaggleMsg to the `.ndjson` file, except when `msg->name == "upload"`.
 * Returns 0 on success, nonzero on error.
 */
int filepublisher_log(FilePublisher *fp, const WaggleMsg *msg);

#ifdef __cplusplus
}
#endif

#endif
