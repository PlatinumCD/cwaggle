#include "waggle/uploader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#ifdef DEBUG
  #define DBGPRINT(...) do { fprintf(stderr, "[DEBUG uploader] "); fprintf(stderr, __VA_ARGS__); } while(0)
#else
  #define DBGPRINT(...) do {} while(0)
#endif

struct Uploader {
    char *root;
};

static int ensure_directory(const char *path) {
    DBGPRINT("ensure_directory(path=%s)\n", path);
    int rc = mkdir(path, 0775);
    if (rc != 0 && errno != EEXIST) {
        perror("mkdir");
        return -1;
    }
    return 0;
}

Uploader* uploader_new(const char *root) {
    DBGPRINT("uploader_new(root=%s)\n", root ? root : "NULL");
    Uploader *u = calloc(1, sizeof(Uploader));
    if (!u) {
        DBGPRINT("calloc failed.\n");
        return NULL;
    }

    u->root = strdup(root ? root : "/tmp/waggle_uploads");
    if (!u->root) {
        free(u);
        return NULL;
    }
    if (ensure_directory(u->root) != 0) {
        DBGPRINT("Failed to ensure_directory.\n");
        free(u->root);
        free(u);
        return NULL;
    }
    DBGPRINT("uploader_new: success.\n");
    return u;
}

void uploader_free(Uploader *u) {
    DBGPRINT("uploader_free() called.\n");
    if (!u) return;
    free(u->root);
    free(u);
}

static int copy_file(const char *src, const char *dst) {
    DBGPRINT("copy_file(src=%s, dst=%s)\n", src, dst);
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

int uploader_upload_file(Uploader *u, const char *src_path, int64_t timestamp) {
    DBGPRINT("uploader_upload_file(src=%s, ts=%ld)\n", src_path ? src_path : "NULL", (long)timestamp);
    if (!u || !src_path) return -1;

    char dirname[512];
    snprintf(dirname, sizeof(dirname), "%s/%ld-%d", u->root, (long)timestamp, getpid());
    if (ensure_directory(dirname) != 0) {
        fprintf(stderr, "uploader_upload_file: ensure_directory failed\n");
        return -2;
    }

    char dst_path[1024];
    snprintf(dst_path, sizeof(dst_path), "%s/data", dirname);

    int rc = copy_file(src_path, dst_path);
    if (rc == 0) {
        printf("[Uploader] Copied file '%s' -> '%s'\n", src_path, dst_path);
    }
    return rc;
}
