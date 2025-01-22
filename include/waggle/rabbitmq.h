#ifndef WAGGLE_RABBITMQ_H
#define WAGGLE_RABBITMQ_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include <stdint.h>

/**
 * Opaque handle for a RabbitMQ connection wrapper.
 */
typedef struct RabbitMQConn RabbitMQConn;

/**
 * Creates and returns a new RabbitMQ connection using
 * the specified config.
 * Returns NULL on failure.
 */
RabbitMQConn* rabbitmq_conn_new(const PluginConfig *config);

/**
 * Closes and frees a RabbitMQ connection handle.
 * Safe to call with NULL.
 */
void rabbitmq_conn_free(RabbitMQConn *conn);

/**
 * Publishes a message payload to the "to-validator" exchange with
 * the given scope as routing key.
 *
 * Returns 0 on success, nonzero on failure.
 */
int rabbitmq_publish(RabbitMQConn *conn,
                     const char *scope,
                     const void *data,
                     int data_len);

/**
 * Subscribes to the given topics from the "data.topic" exchange.
 * This is a stub for real subscription logic. For a fully functional
 * consumer, you'd need a receiving thread or callback approach.
 *
 * Returns 0 on success, nonzero on failure.
 */
int rabbitmq_subscribe(RabbitMQConn *conn,
                       const char **topics,
                       int n);

#ifdef __cplusplus
}
#endif

#endif
