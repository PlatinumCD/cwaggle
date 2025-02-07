/**
 * rabbitmq.c
 *
 * Purpose:
 *   Manages a persistent RabbitMQ connection (no heartbeats).
 *   Allows repeated connect, publish, and close logic.
 */

#include "waggle/config.h"
#include "waggle/rabbitmq.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <rabbitmq-c/amqp.h>
#include <rabbitmq-c/tcp_socket.h>

#ifdef DEBUG
  #define DBGPRINT(...) \
    do { fprintf(stderr, "[DEBUG rabbitmq] "); fprintf(stderr, __VA_ARGS__); } while (0)
#else
  #define DBGPRINT(...) do {} while(0)
#endif


// -----------------------------------------------------------------------------
static void print_amqp_error(amqp_rpc_reply_t r, const char *ctx) {
    switch (r.reply_type) {
    case AMQP_RESPONSE_NORMAL:
        DBGPRINT("%s: normal.\n", ctx);
        break;
    case AMQP_RESPONSE_NONE:
        fprintf(stderr, "%s: missing RPC reply type!\n", ctx);
        break;
    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        fprintf(stderr, "%s: %s\n", ctx, amqp_error_string2(r.library_error));
        break;
    case AMQP_RESPONSE_SERVER_EXCEPTION:
        switch (r.reply.id) {
        case AMQP_CONNECTION_CLOSE_METHOD: {
            amqp_connection_close_t *m = (amqp_connection_close_t*) r.reply.decoded;
            fprintf(stderr, "%s: connection error %u, msg: %.*s\n",
                    ctx, m->reply_code, (int) m->reply_text.len,
                    (char*) m->reply_text.bytes);
            break;
        }
        case AMQP_CHANNEL_CLOSE_METHOD: {
            amqp_channel_close_t *m = (amqp_channel_close_t*) r.reply.decoded;
            fprintf(stderr, "%s: channel error %u, msg: %.*s\n",
                    ctx, m->reply_code, (int) m->reply_text.len,
                    (char*) m->reply_text.bytes);
            break;
        }
        default:
            fprintf(stderr, "%s: unknown server error, method id 0x%08X\n", ctx, r.reply.id);
            break;
        }
        break;
    }
}

// -----------------------------------------------------------------------------
RabbitMQConn* rabbitmq_conn_create(const PluginConfig *config) {
    if (!config) {
        fprintf(stderr, "rabbitmq_conn_create: config is NULL\n");
        return NULL;
    }

    DBGPRINT("rabbitmq_conn_create(%s:%d)\n", config->host, config->port);

    RabbitMQConn *rc = calloc(1, sizeof(RabbitMQConn));
    if (!rc) {
        fprintf(stderr, "rabbitmq_conn_create: out of memory\n");
        return NULL;
    }

    rc->conn = amqp_new_connection();
    amqp_socket_t *sock = amqp_tcp_socket_new(rc->conn);
    if (!sock) {
        fprintf(stderr, "Cannot create TCP socket.\n");
        free(rc);
        return NULL;
    }

    if (amqp_socket_open(sock, config->host, config->port)) {
        fprintf(stderr, "Cannot open socket to %s:%d.\n", config->host, config->port);
        free(rc);
        return NULL;
    }

    // No heartbeat => request = 0
    amqp_rpc_reply_t r = amqp_login(rc->conn, "/", 0, 131072, 0,
                                    AMQP_SASL_METHOD_PLAIN,
                                    config->username,
                                    config->password);
    if (r.reply_type != AMQP_RESPONSE_NORMAL) {
        print_amqp_error(r, "amqp_login");
        free(rc);
        return NULL;
    }

    // Open channel #1
    amqp_channel_open(rc->conn, 1);
    r = amqp_get_rpc_reply(rc->conn);
    if (r.reply_type != AMQP_RESPONSE_NORMAL) {
        print_amqp_error(r, "amqp_channel_open");
        free(rc);
        return NULL;
    }

    // Enable publisher confirms
    amqp_confirm_select(rc->conn, 1);
    r = amqp_get_rpc_reply(rc->conn);
    if (r.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "Failed to enable publisher confirms.\n");
        free(rc);
        return NULL;
    }

    rc->connected = 1;
    DBGPRINT("rabbitmq_conn_create: connection established.\n");
    return rc;
}

// -----------------------------------------------------------------------------
void rabbitmq_conn_close(RabbitMQConn *rc) {
    DBGPRINT("rabbitmq_conn_close() called.\n");
    if (!rc) {
        return;
    }
    if (rc->connected) {
        amqp_channel_close(rc->conn, 1, AMQP_REPLY_SUCCESS);
        amqp_connection_close(rc->conn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(rc->conn);
    }
    free(rc);
}

// -----------------------------------------------------------------------------
int rabbitmq_publish_message(
    RabbitMQConn *rc,
    const char *app_id,
    const char *username,
    const char *scope,
    const void *data,
    int app_id_len,
    int username_len,
    int data_len
) {

    if (!rc || !rc->connected) return -1;
    if (!scope || !data) return -2;

    amqp_bytes_t msg_bytes = { .len = data_len, .bytes = (void*) data };
    amqp_bytes_t app_bytes = { .len = app_id_len, .bytes = (void*) app_id };
    amqp_bytes_t usr_bytes = { .len = username_len, .bytes = (void*) username };

    // Basic properties
    amqp_basic_properties_t props;
    memset(&props, 0, sizeof(props));
    props._flags = (AMQP_BASIC_DELIVERY_MODE_FLAG |
                    AMQP_BASIC_USER_ID_FLAG       |
                    AMQP_BASIC_APP_ID_FLAG);
    props.delivery_mode = 2; // persistent
    props.app_id = app_bytes;
    props.user_id = usr_bytes;

    int status = amqp_basic_publish(
        rc->conn,
        1, // channel
        amqp_cstring_bytes("to-validator"),
        amqp_cstring_bytes(scope),
        0, // mandatory
        0, // immediate
        &props,
        msg_bytes
    );
    if (status != AMQP_STATUS_OK) {
        fprintf(stderr, "amqp_basic_publish failed: %d\n", status);
        return -3;
    }

    // Wait for confirm
    struct timeval timeout = {1, 0};
    amqp_publisher_confirm_t cresult;
    amqp_rpc_reply_t r = amqp_publisher_confirm_wait(rc->conn, &timeout, &cresult);
    if (r.reply_type != AMQP_RESPONSE_NORMAL) {
        print_amqp_error(r, "publisher_confirm_wait");
        return -4;
    }

    DBGPRINT("rabbitmq_publish_message: success.\n");
    return 0;
}

// -----------------------------------------------------------------------------
int rabbitmq_subscribe_topics(RabbitMQConn *rc, const char **topics, int n) {
    if (!rc || !rc->connected) return -1;
    if (!topics || n <= 0) return 0;

    DBGPRINT("rabbitmq_subscribe_topics: %d topics\n", n);
    // Real usage would declare queues and bind them to each topic
    for (int i = 0; i < n; i++) {
        printf("[RabbitMQ] Subscribing to topic: %s\n", topics[i]);
    }
    return 0;
}
