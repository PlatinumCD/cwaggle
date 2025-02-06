#include "waggle/plugin.h"
#include "waggle/config.h"
#include "waggle/rabbitmq.h"
#include "waggle/filepublisher.h"
#include "waggle/wagglemsg.h"
#include "waggle/timeutil.h"
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
#include <inttypes.h>
#include <errno.h>

#ifdef DEBUG
  #define DBGPRINT(...) do { fprintf(stderr, "[DEBUG plugin] "); fprintf(stderr, __VA_ARGS__); } while(0)
#else
  #define DBGPRINT(...) do {} while(0)
#endif

typedef struct PublishItem {
    char *scope;
    char *data;
    int   data_len;
    struct PublishItem *next;
} PublishItem;

typedef struct {
    PublishItem *head;
    PublishItem *tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
    uint32_t length;
} PublishQueue;

// Initialize
static void publish_queue_init(PublishQueue *q) {
    DBGPRINT("publish_queue_init() called.\n");
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->length = 0;
}

// Destroy
static void publish_queue_destroy(PublishQueue *q) {
    DBGPRINT("publish_queue_destroy() called.\n");
    pthread_mutex_lock(&q->lock);
    PublishItem *item = q->head;
    while (item) {
        PublishItem *tmp = item;
        item = item->next;
        free(tmp->scope);
        free(tmp->data);
        free(tmp);
    }
    pthread_mutex_unlock(&q->lock);

    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->cond);
}

// Push
static void publish_queue_push(PublishQueue *q, const char *scope, const char *data, int data_len) {
    DBGPRINT("publish_queue_push(scope=%s, data_len=%d)\n", scope, data_len);
    if (!scope || !data || data_len < 0) return;

    PublishItem *item = malloc(sizeof(PublishItem));
    if (!item) {
        DBGPRINT("publish_queue_push: malloc failed.\n");
        return;
    }

    item->scope = strdup(scope);
    item->data = malloc(data_len);
    if (!item->scope || !item->data) {
        DBGPRINT("publish_queue_push: strdup/malloc failed.\n");
        free(item->scope);
        free(item->data);
        free(item);
        return;
    }

    memcpy(item->data, data, data_len);
    item->data_len = data_len;
    item->next = NULL;

    pthread_mutex_lock(&q->lock);
    if (!q->tail) {
        q->head = item;
        q->tail = item;
    } else {
        q->tail->next = item;
        q->tail = item;
    }
    q->length++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

// Pop
static PublishItem* publish_queue_pop(PublishQueue *q) {
    DBGPRINT("publish_queue_pop() waiting for item...\n");
    pthread_mutex_lock(&q->lock);
    while (!q->head) {
        pthread_cond_wait(&q->cond, &q->lock);
    }
    PublishItem *item = q->head;
    q->head = item->next;
    if (!q->head) {
        q->tail = NULL;
    }
    q->length--;
    pthread_mutex_unlock(&q->lock);
    DBGPRINT("publish_queue_pop() got item.\n");
    return item;
}

struct Plugin {
    PluginConfig  *config;
    FilePublisher *filepub;
    RabbitMQConn  *mq;
    PublishQueue   queue;
    pthread_t thread;
    _Atomic int stop_flag;
};

// Publisher thread
static void* plugin_publisher_thread(void *arg) {
    DBGPRINT("plugin_publisher_thread started.\n");
    Plugin *p = (Plugin*)arg;

    while (1) {
        // If stop_flag is set and queue is empty, we're done.
        if (atomic_load(&p->stop_flag)) {
            pthread_mutex_lock(&p->queue.lock);
            int empty = (p->queue.length == 0);
            pthread_mutex_unlock(&p->queue.lock);
            if (empty) {
                DBGPRINT("plugin_publisher_thread: queue empty, stopping.\n");
                break;
            }
        }

        // Get next item from queue
        PublishItem *item = publish_queue_pop(&p->queue);
        if (!item) {
            DBGPRINT("plugin_publisher_thread: Null item popped?\n");
            continue;
        }

        // Publish
        DBGPRINT("Publishing item with scope=%s.\n", item->scope);
        if (p->mq) {
            rabbitmq_publish(
                p->mq,
                p->config->app_id,
                p->config->username,
                item->scope,
                item->data,
                (int)strlen(p->config->app_id),
                (int)strlen(p->config->username),
                item->data_len
            );
        }

        free(item->scope);
        free(item->data);
        free(item);
    }

    DBGPRINT("plugin_publisher_thread: exiting.\n");
    return NULL;
}

Plugin* plugin_new(PluginConfig *config) {
    DBGPRINT("plugin_new() called.\n");
    if (!config) {
        fprintf(stderr, "plugin_new: config is NULL\n");
        return NULL;
    }

    Plugin *p = calloc(1, sizeof(Plugin));
    if (!p) {
        DBGPRINT("calloc failed for Plugin.\n");
        return NULL;
    }
    p->config = config;

    // Optional file-based logging
    const char *logdir = getenv("PYWAGGLE_LOG_DIR");
    if (logdir) {
        DBGPRINT("PYWAGGLE_LOG_DIR=%s\n", logdir);
        p->filepub = filepublisher_new(logdir);
        if (!p->filepub) {
            fprintf(stderr, "Warning: Could not open FilePublisher in %s\n", logdir);
        }
    }

    // Init queue
    publish_queue_init(&p->queue);

    // Connect to RabbitMQ
    p->mq = rabbitmq_conn_new(config);
    if (!p->mq) {
        fprintf(stderr, "plugin_new: failed to connect to RabbitMQ\n");
        plugin_free(p);
        return NULL;
    }

    // Start publisher thread
    atomic_store(&p->stop_flag, 0);
    if (pthread_create(&p->thread, NULL, plugin_publisher_thread, p) != 0) {
        fprintf(stderr, "plugin_new: could not create publisher thread\n");
        plugin_free(p);
        return NULL;
    }

    DBGPRINT("plugin_new() success.\n");
    return p;
}

void plugin_free(Plugin *plugin) {
    DBGPRINT("plugin_free() called.\n");
    if (!plugin) return;

    // Signal thread to stop and wait
    atomic_store(&plugin->stop_flag, 1);
    pthread_join(plugin->thread, NULL);

    // Cleanup
    publish_queue_destroy(&plugin->queue);
    rabbitmq_conn_free(plugin->mq);
    filepublisher_free(plugin->filepub);
    plugin_config_free(plugin->config);
    free(plugin);

    DBGPRINT("plugin_free() done.\n");
}

int plugin_publish(Plugin *plugin,
                   const char *scope,
                   const char *name,
                   int64_t value,
                   uint64_t timestamp,
                   const char *meta_json) {
    DBGPRINT("plugin_publish(scope=%s, name=%s, value=%" PRId64 ", timestamp=%" PRIu64 ")\n",
             scope ? scope : "NULL",
             name  ? name  : "NULL",
             value,
             timestamp);
    if (!plugin || !name) {
        DBGPRINT("plugin_publish: invalid args.\n");
        return -1;
    }

    // Create WaggleMsg
    WaggleMsg *msg = wagglemsg_new(name, value, timestamp, meta_json ? meta_json : "{}");
    if (!msg) {
        DBGPRINT("Failed to create WaggleMsg.\n");
        return -2;
    }

    // Log to file
    if (plugin->filepub) {
        DBGPRINT("Logging to filepublisher.\n");
        filepublisher_log(plugin->filepub, msg);
    }

    // Convert to JSON
    char *json_str = wagglemsg_dump_json(msg);
    wagglemsg_free(msg);
    if (!json_str) {
        DBGPRINT("Failed to dump WaggleMsg to JSON.\n");
        return -3;
    }

    // Push to publish queue
    DBGPRINT("plugin_publish pushing:\n\t%s\n", json_str);
    publish_queue_push(&plugin->queue, scope ? scope : "all", json_str, (int)strlen(json_str));
    free(json_str);

    return 0;
}

int plugin_subscribe(Plugin *plugin, const char **topics, int n) {
    DBGPRINT("plugin_subscribe() called with %d topics.\n", n);
    if (!plugin || !plugin->mq) return -1;
    // For real usage, you'd do queue binding & consumption here.
    return rabbitmq_subscribe(plugin->mq, topics, n);
}
