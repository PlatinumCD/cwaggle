/**
 * plugin.c
 *
 * Purpose:
 *   Provides a high-level interface for publishing messages to RabbitMQ in a loop,
 *   reconnecting on failures, etc.
 */

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
#include <time.h>
#include <unistd.h> // for sleep()

#ifdef DEBUG
  #define DBGPRINT(...) \
    do { fprintf(stderr, "[DEBUG plugin] "); fprintf(stderr, __VA_ARGS__); } while(0)
#else
  #define DBGPRINT(...) do {} while(0)
#endif

// -----------------------------------------------------------------------------
// PublishItem: a single message
// -----------------------------------------------------------------------------
typedef struct PublishItem {
    char *scope;
    char *data;
    int   data_len;
    struct PublishItem *next;
} PublishItem;

// -----------------------------------------------------------------------------
// PublishQueue: thread-safe queue
// -----------------------------------------------------------------------------
typedef struct {
    PublishItem *head;
    PublishItem *tail;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
    uint32_t length;
} PublishQueue;

// queue helpers
static void publish_queue_init(PublishQueue *q) {
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->length = 0;
}

static void publish_queue_destroy(PublishQueue *q) {
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

static void publish_queue_push(PublishQueue *q, const char *scope, const char *data, int len) {
    if (!scope || !data || len < 0) return;

    PublishItem *item = malloc(sizeof(PublishItem));
    if (!item) return;

    item->scope = strdup(scope);
    item->data = malloc(len);
    if (!item->scope || !item->data) {
        free(item->scope);
        free(item->data);
        free(item);
        return;
    }
    memcpy(item->data, data, len);
    item->data_len = len;
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

// Pop with 1s timeout. Returns NULL if no item in that time.
static PublishItem* publish_queue_pop_timeout(PublishQueue *q, int timeout_sec) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout_sec;

    pthread_mutex_lock(&q->lock);
    while (!q->head) {
        int ret = pthread_cond_timedwait(&q->cond, &q->lock, &ts);
        if (ret == ETIMEDOUT) {
            pthread_mutex_unlock(&q->lock);
            return NULL;
        }
    }

    PublishItem *item = q->head;
    q->head = item->next;
    if (!q->head) {
        q->tail = NULL;
    }
    q->length--;
    pthread_mutex_unlock(&q->lock);
    return item;
}

// -----------------------------------------------------------------------------
// Plugin: main struct
// -----------------------------------------------------------------------------
struct Plugin {
    PluginConfig  *config;
    FilePublisher *filepub;
    PublishQueue   queue;
    pthread_t      thread;
    _Atomic int    stop_flag;
};

// forward declarations
static void* plugin_thread_main(void *arg);
static int connect_and_flush_messages(Plugin *plugin);
static int flush_queued_messages(Plugin *plugin, RabbitMQConn *rc);

// -----------------------------------------------------------------------------
// plugin_new
// -----------------------------------------------------------------------------
Plugin* plugin_new(PluginConfig *config) {
    DBGPRINT("plugin_new()\n");
    if (!config) {
        fprintf(stderr, "plugin_new: config is NULL\n");
        return NULL;
    }

    Plugin *p = calloc(1, sizeof(Plugin));
    if (!p) {
        fprintf(stderr, "plugin_new: out of memory\n");
        return NULL;
    }
    p->config = config;

    // optional local logging
    const char *logdir = getenv("PYWAGGLE_LOG_DIR");
    if (logdir) {
        p->filepub = filepublisher_new(logdir);
        if (!p->filepub) {
            fprintf(stderr, "plugin_new: Could not open FilePublisher in %s\n", logdir);
        }
    }

    publish_queue_init(&p->queue);
    atomic_store(&p->stop_flag, 0);

    // start publisher thread
    if (pthread_create(&p->thread, NULL, plugin_thread_main, p) != 0) {
        fprintf(stderr, "plugin_new: could not create publisher thread\n");
        plugin_free(p);
        return NULL;
    }

    return p;
}

// -----------------------------------------------------------------------------
// plugin_free
// -----------------------------------------------------------------------------
void plugin_free(Plugin *plugin) {
    if (!plugin) return;
    atomic_store(&plugin->stop_flag, 1);
    pthread_join(plugin->thread, NULL);

    publish_queue_destroy(&plugin->queue);
    filepublisher_free(plugin->filepub);
    plugin_config_free(plugin->config);

    free(plugin);
}

// -----------------------------------------------------------------------------
// plugin_publish
// -----------------------------------------------------------------------------
int plugin_publish(Plugin *plugin,
                   const char *scope,
                   const char *name,
                   int64_t value,
                   uint64_t timestamp,
                   const char *meta_json) {
    if (!plugin || !name) return -1;

    WaggleMsg *msg = wagglemsg_new(name, value, timestamp, meta_json ? meta_json : "{}");
    if (!msg) return -2;

    // optionally log to file
    if (plugin->filepub) {
        filepublisher_log(plugin->filepub, msg);
    }

    char *json_str = wagglemsg_dump_json(msg);
    wagglemsg_free(msg);
    if (!json_str) return -3;

    publish_queue_push(&plugin->queue, scope ? scope : "all", json_str, (int)strlen(json_str));
    free(json_str);
    return 0;
}

// -----------------------------------------------------------------------------
// plugin_subscribe - optional stub
// -----------------------------------------------------------------------------
int plugin_subscribe(Plugin *plugin, const char **topics, int n) {
    if (!plugin) return -1;
    // To avoid "unused parameter" warnings:
    // Option 1: Actually call rabbitmq_subscribe_topics if we have a connection
    // Option 2: Mark parameters as unused
    // We'll do the real call here for demonstration:
    if (!plugin->config) return -2;

    // We would typically store a RabbitMQConn in our plugin struct if needed.
    // For demonstration, just do nothing if no real persistent connection object.
    // (void)topics;
    // (void)n;
    // return 0;

    // If you'd stored a RabbitMQConn in plugin, you'd do:
    // return rabbitmq_subscribe_topics(plugin->some_rabbitmq_conn, topics, n);

    // But let's just declare we do nothing:
    (void)topics;
    (void)n;
    return 0;
}

// -----------------------------------------------------------------------------
// Publisher thread: repeatedly connect, flush queue, reconnect on error
// -----------------------------------------------------------------------------
static void* plugin_thread_main(void *arg) {
    Plugin *p = (Plugin*)arg;
    DBGPRINT("publisher thread started.\n");

    while (!atomic_load(&p->stop_flag)) {
        int ret = connect_and_flush_messages(p);
        if (ret != 0) {
            DBGPRINT("connect_and_flush_messages failed. Retrying in 1s...\n");
            sleep(1);
        }
    }

    DBGPRINT("publisher thread stopped.\n");
    return NULL;
}

// -----------------------------------------------------------------------------
// Connect to RabbitMQ, flush messages until stop, then close
// -----------------------------------------------------------------------------
static int connect_and_flush_messages(Plugin *plugin) {
    RabbitMQConn *rc = rabbitmq_conn_create(plugin->config);
    if (!rc) {
        DBGPRINT("Failed to connect.\n");
        return -1;
    }

    DBGPRINT("Connection established. Flushing messages...\n");
    while (!atomic_load(&plugin->stop_flag)) {
        int ret = flush_queued_messages(plugin, rc);
        if (ret != 0) {
            DBGPRINT("Error flushing messages. Closing connection.\n");
            rabbitmq_conn_close(rc);
            return -1; // reconnect
        }
    }

    DBGPRINT("Stop signaled. Flushing leftover messages...\n");
    flush_queued_messages(plugin, rc);
    rabbitmq_conn_close(rc);
    return 0;
}

// -----------------------------------------------------------------------------
// flush_queued_messages: pop items with timeout, publish, requeue on error
// -----------------------------------------------------------------------------
static int flush_queued_messages(Plugin *plugin, RabbitMQConn *rc) {
    while (1) {
        // try to pop an item within 1 second
	DBGPRINT("Popping item off of publish queue...\n");
        PublishItem *item = publish_queue_pop_timeout(&plugin->queue, 1);
        if (!item) {
            // no new messages arrived in 1s
            return 0;
        }

	DBGPRINT("Publishing message...\n");

	// Check rc first
	if (!rc) {
	    fprintf(stderr, "Error: rc (RabbitMQConn) is NULL\n");
	    abort();
	}

	// Check before calling strlen()
	int app_id_len = plugin->config->app_id ? (int)strlen(plugin->config->app_id) : -1;
	int username_len = plugin->config->username ? (int)strlen(plugin->config->username) : -1;

	if (app_id_len == -1 || username_len == -1) {
	    fprintf(stderr, "Error: app_id or username is NULL, aborting before segfault.\n");
	    abort();
	}

        int pub_res = rabbitmq_publish_message(
            rc,
	    plugin->config->app_id,
	    plugin->config->username,
            item->scope,
            item->data,
            (int)strlen(plugin->config->app_id),
            (int)strlen(plugin->config->username),
            item->data_len
        );

	DBGPRINT("Retrigger connection\n");
        if (pub_res != 0) {
            // requeue item and fail => triggers reconnect
            publish_queue_push(&plugin->queue, item->scope, item->data, item->data_len);
            free(item->scope);
            free(item->data);
            free(item);
            return -1;
        }

        free(item->scope);
        free(item->data);
        free(item);
    }
}
