#include "waggle/uploader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

struct Uploader {
    char *root;
};

static int ensure_directory(const char *path) {
    // Very naive approach: try mkdir; ignore errors if path exists.
    int rc = mkdir(path, 0775);
    if (rc != 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }
    return 0;
}

Uploader* uploader_new(const char *root) {
    Uploader *u = (Uploader*)calloc(1, sizeof(Uploader));
    if (!u) return NULL;

    u->root = strdup(root ? root : "/tmp/waggle_uploads");
    if (!u->root) {
        free(u);
        return NULL;
    }
    if (ensure_directory(u->root) != 0) {
        free(u->root);
        free(u);
        return NULL;
    }
    return u;
}

void uploader_free(Uploader *u) {
    if (!u) return;
    free(u->root);
    free(u);
}

/**
 * A simple file-copy function.
 * Returns 0 on success, nonzero on error.
 */
static int copy_file(const char *src, const char *dst) {
    int in_fd = open(src, O_RDONLY);
    if (in_fd < 0) {
        perror("open(src)");
        return -1;
    }
    int out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0664);
    if (out_fd < 0) {
        perror("open(dst)");
        close(in_fd);
        return -2;
    }
    char buf[4096];
    ssize_t r;
    while ((r = read(in_fd, buf, sizeof(buf))) > 0) {
        ssize_t w = write(out_fd, buf, r);
        if (w < 0) {
            perror("write(dst)");
            close(in_fd);
            close(out_fd);
            return -3;
        }
    }
    close(in_fd);
    close(out_fd);
    return (r < 0) ? -4 : 0;
}

int uploader_upload_file(Uploader *u,
                         const char *src_path,
                         int64_t timestamp) {
    if (!u || !src_path) {
        return -1;
    }
    // Create a unique subdirectory named, for example, "timestamp-PID"
    char dirname[512];
    snprintf(dirname, sizeof(dirname), "%s/%ld-%d", u->root, (long)timestamp, getpid());

    if (ensure_directory(dirname) != 0) {
        fprintf(stderr, "uploader_upload_file: ensure_directory failed\n");
        return -2;
    }

    // Destination file is "dirname/data"
    char dst_path[1024];
    snprintf(dst_path, sizeof(dst_path), "%s/data", dirname);

    // Perform copy
    int rc = copy_file(src_path, dst_path);
    if (rc == 0) {
        printf("[Uploader] Copied file '%s' -> '%s'\n", src_path, dst_path);
    }
    return rc;
}
