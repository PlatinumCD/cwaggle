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

// A small struct for queued messages: scope + payload data
typedef struct PublishItem {
    char *scope;
    char *data;
    int   data_len;
    struct PublishItem *next;
} PublishItem;

/**
 * A simple lock-based queue for PublishItems.
 */
typedef struct {
    PublishItem *head;
    PublishItem *tail;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} PublishQueue;

static void publish_queue_init(PublishQueue *q) {
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
}

static void publish_queue_destroy(PublishQueue *q) {
    // free any leftover items
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
    if (!scope || !data) {
        return;
    }
    PublishItem *item = (PublishItem*)malloc(sizeof(PublishItem));
    if (!item) return;

    item->scope = strdup(scope);
    item->data = (char*)malloc(data_len);
    if (item->scope == NULL || item->data == NULL) {
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

/**
 * Blocking pop. If the queue is empty, we wait on cond.
 * Must free the returned item when done.
 */
static PublishItem* publish_queue_pop(PublishQueue *q) {
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
    return item;
}

// The Plugin struct
struct Plugin {
    PluginConfig  *config;
    FilePublisher *filepub;
    RabbitMQConn  *mq;      // a single rabbitmq connection
    PublishQueue   queue;

    // Background thread to flush messages
    pthread_t thread;
    _Atomic int stop_flag;
};

/**
 * The background thread: connects to RabbitMQ (already done in plugin_new),
 * then repeatedly pops messages from the queue and publishes them.
 */
static void* plugin_publisher_thread(void *arg) {
    Plugin *p = (Plugin*)arg;
    while (!atomic_load(&p->stop_flag)) {
        // pop next message
        PublishItem *item = publish_queue_pop(&p->queue);
        if (!item) {
            continue;
        }
        // publish
        if (p->mq) {
            rabbitmq_publish(p->mq, item->scope, item->data, item->data_len);
        }
        // cleanup
        free(item->scope);
        free(item->data);
        free(item);
    }
    return NULL;
}

Plugin* plugin_new(PluginConfig *config) {
    if (!config) {
        fprintf(stderr, "plugin_new: config is NULL\n");
        return NULL;
    }
    Plugin *p = (Plugin*)calloc(1, sizeof(Plugin));
    if (!p) {
        return NULL;
    }
    p->config = config;

    const char *logdir = getenv("PYWAGGLE_LOG_DIR");
    if (logdir) {
        p->filepub = filepublisher_new(logdir);
        if (!p->filepub) {
            fprintf(stderr, "Warning: Could not open FilePublisher in %s\n", logdir);
        }
    }

    publish_queue_init(&p->queue);

    // connect to RabbitMQ
    p->mq = rabbitmq_conn_new(config);
    if (!p->mq) {
        fprintf(stderr, "plugin_new: failed to connect to RabbitMQ\n");
        plugin_free(p);
        return NULL;
    }

    atomic_store(&p->stop_flag, 0);
    // create the background thread
    if (pthread_create(&p->thread, NULL, plugin_publisher_thread, p) != 0) {
        fprintf(stderr, "plugin_new: could not create publisher thread\n");
        plugin_free(p);
        return NULL;
    }

    return p;
}

void plugin_free(Plugin *plugin) {
    if (!plugin) {
        return;
    }


    // signal the thread to stop
    atomic_store(&plugin->stop_flag, 1);
    // push a dummy item to wake thread
    publish_queue_push(&plugin->queue, "dummy", "dummy", 5);

    // wait for thread to finish
    pthread_join(plugin->thread, NULL);

    // free queue
    publish_queue_destroy(&plugin->queue);

    // close rabbitmq
    rabbitmq_conn_free(plugin->mq);

    // free publisher
    filepublisher_free(plugin->filepub);

    // free config
    plugin_config_free(plugin->config);

    // free plugin
    free(plugin);
}

int plugin_publish(Plugin *plugin,
                   const char *scope,
                   const char *name,
                   const char *value,
                   int64_t timestamp,
                   const char *meta_json) {
    if (!plugin || !name || !value) {
        return -1;
    }
    // build WaggleMsg
    WaggleMsg *msg = wagglemsg_new(name, value, timestamp, meta_json ? meta_json : "{}");
    if (!msg) {
        return -2;
    }

    // Log to puplisher
    if (plugin->filepub) {
        filepublisher_log(plugin->filepub, msg);
    }

    // serialize to JSON
    char *json_str = wagglemsg_dump_json(msg);
    wagglemsg_free(msg);

    if (!json_str) {
        return -3;
    }

    // push to queue
    publish_queue_push(&plugin->queue, scope ? scope : "all", json_str, (int)strlen(json_str));
    free(json_str);

    return 0;
}

int plugin_subscribe(Plugin *plugin, const char **topics, int n) {
    if (!plugin || !plugin->mq) {
        return -1;
    }
    return rabbitmq_subscribe(plugin->mq, topics, n);
}
