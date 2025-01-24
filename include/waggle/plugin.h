#ifndef WAGGLE_PLUGIN_H
#define WAGGLE_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include <stdint.h>

/**
 * Opaque struct for the Plugin object.
 */
typedef struct Plugin Plugin;

/**
 * Creates a new Plugin instance with the given config.
 * Takes ownership of the config pointer (frees it on plugin_free).
 * Returns NULL on failure.
 *
 * This sets up internal queues and spawns a background thread
 * that connects to RabbitMQ and publishes messages from the queue.
 */
Plugin* plugin_new(PluginConfig *config);

/**
 * Frees the Plugin and all resources. Waits for background threads to
 * join. Safe to call with NULL. Also frees the config.
 */
void plugin_free(Plugin *plugin);

/**
 * Publishes a message to the plugin's outgoing queue. The scope is
 * e.g. "all", "dev", etc. name, value, meta are string data. The
 * timestamp is nanoseconds since epoch.
 *
 * Returns 0 on success, nonzero on error.
 */
int plugin_publish(Plugin *plugin,
                   const char *scope,
                   const char *name,
                   int64_t value,
                   uint64_t timestamp,
                   const char *meta_json);

/**
 * Subscribes to one or more topics. Real consumption logic would be
 * implemented in a separate thread or callback approach. For now,
 * this function only calls rabbitmq_subscribe in the background
 * connection.
 *
 * Returns 0 on success, nonzero on error.
 */
int plugin_subscribe(Plugin *plugin, const char **topics, int n);

#ifdef __cplusplus
}
#endif

#endif
