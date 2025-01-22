#ifndef WAGGLE_UPLOADER_H
#define WAGGLE_UPLOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct Uploader Uploader;

/**
 * Creates a new Uploader. The root path is where files
 * will be uploaded.
 */
Uploader* uploader_new(const char *root);

/**
 * Frees the Uploader object.
 */
void uploader_free(Uploader *u);

/**
 * Uploads a file (copy from `src_path` into the upload root).
 * The timestamp can be used to name or meta-tag the file.
 *
 * Returns 0 on success, nonzero on failure.
 */
int uploader_upload_file(Uploader *u,
                         const char *src_path,
                         int64_t timestamp);

#ifdef __cplusplus
}
#endif

#endif
