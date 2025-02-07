#ifndef WAGGLE_RABBITMQ_H
#define WAGGLE_RABBITMQ_H

#ifdef __cplusplus
extern "C" {
#endif

#include <rabbitmq-c/amqp.h>
#include "config.h"
#include <stdint.h>

typedef struct RabbitMQConn {
    amqp_connection_state_t conn;
    int connected;
} RabbitMQConn;

/**
 * Creates and returns a new RabbitMQ connection using
 * the specified config.
 * Returns NULL on failure.
 */
RabbitMQConn* rabbitmq_conn_create(const PluginConfig *config);

/**
 * Closes and frees a RabbitMQ connection handle.
 * Safe to call with NULL.
 */
void rabbitmq_conn_close(RabbitMQConn *conn);

/**
 * Publishes a message payload to the "to-validator" exchange with
 * the given scope as routing key.
 *
 * Returns 0 on success, nonzero on failure.
 */
int rabbitmq_publish_message(RabbitMQConn *conn,
                     const char *app_id,
                     const char *username,
                     const char *scope,
                     const void *data,
                     int app_id_len,
                     int username_len,
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
