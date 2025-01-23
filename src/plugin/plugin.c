#include "waggle/plugin.h"
#include "waggle/config.h"
#include "waggle/rabbitmq.h"
#include "waggle/filepublisher.h"
#include "waggle/wagglemsg.h"
#include "waggle/timeutil.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdatomic.h>
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
} PublishQueue;

static void publish_queue_init(PublishQueue *q) {
    DBGPRINT("publish_queue_init() called.\n");
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
}

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

static void publish_queue_push(PublishQueue *q, const char *scope, const char *data, int data_len) {
    DBGPRINT("publish_queue_push(scope=%s, data_len=%d)\n", scope, data_len);
    if (!scope || !data) return;

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
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
}

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

static void* plugin_publisher_thread(void *arg) {
    DBGPRINT("plugin_publisher_thread started.\n");
    Plugin *p = (Plugin*)arg;

    while (!atomic_load(&p->stop_flag)) {
        PublishItem *item = publish_queue_pop(&p->queue);
        if (!item) {
            DBGPRINT("plugin_publisher_thread: Null item popped?\n");
            continue;
        }
        DBGPRINT("Publishing item with scope=%s.\n", item->scope);
        if (p->mq) {
            rabbitmq_publish(p->mq, item->scope, item->data, item->data_len);
        }
        free(item->scope);
        free(item->data);
        free(item);
    }
    DBGPRINT("plugin_publisher_thread: stop_flag set, exiting.\n");
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

    const char *logdir = getenv("PYWAGGLE_LOG_DIR");
    if (logdir) {
        DBGPRINT("PYWAGGLE_LOG_DIR=%s\n", logdir);
        p->filepub = filepublisher_new(logdir);
        if (!p->filepub) {
            fprintf(stderr, "Warning: Could not open FilePublisher in %s\n", logdir);
        }
    }

    publish_queue_init(&p->queue);

    p->mq = rabbitmq_conn_new(config);
    if (!p->mq) {
        fprintf(stderr, "plugin_new: failed to connect to RabbitMQ\n");
        plugin_free(p);
        return NULL;
    }

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

    atomic_store(&plugin->stop_flag, 1);
    publish_queue_push(&plugin->queue, "dummy", "dummy", 5);

    pthread_join(plugin->thread, NULL);

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
                   const char *value,
                   int64_t timestamp,
                   const char *meta_json) {
    DBGPRINT("plugin_publish(scope=%s, name=%s)\n", scope ? scope : "NULL", name ? name : "NULL");
    if (!plugin || !name || !value) {
        DBGPRINT("plugin_publish: invalid args.\n");
        return -1;
    }

    WaggleMsg *msg = wagglemsg_new(name, value, timestamp, meta_json ? meta_json : "{}");
    if (!msg) {
        DBGPRINT("Failed to create WaggleMsg.\n");
        return -2;
    }
    if (plugin->filepub) {
        DBGPRINT("Logging to filepublisher.\n");
        filepublisher_log(plugin->filepub, msg);
    }

    char *json_str = wagglemsg_dump_json(msg);
    wagglemsg_free(msg);
    if (!json_str) {
        DBGPRINT("Failed to dump WaggleMsg to JSON.\n");
        return -3;
    }

    publish_queue_push(&plugin->queue, scope ? scope : "all", json_str, (int)strlen(json_str));
    free(json_str);
    return 0;
}

int plugin_subscribe(Plugin *plugin, const char **topics, int n) {
    DBGPRINT("plugin_subscribe() called with %d topics.\n", n);
    if (!plugin || !plugin->mq) return -1;
    return rabbitmq_subscribe(plugin->mq, topics, n);
}
