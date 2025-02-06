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
  #define DBGPRINT(...) do {} while (0)
#endif

struct RabbitMQConn {
    amqp_connection_state_t conn;
    int connected;
};

// Print AMQP errors
static void print_amqp_error(amqp_rpc_reply_t x, const char *ctx) {
    switch (x.reply_type) {
    case AMQP_RESPONSE_NORMAL:
        DBGPRINT("%s: normal.\n", ctx);
        break;
    case AMQP_RESPONSE_NONE:
        fprintf(stderr, "%s: missing RPC reply type!\n", ctx);
        break;
    case AMQP_RESPONSE_LIBRARY_EXCEPTION:
        fprintf(stderr, "%s: %s\n", ctx, amqp_error_string2(x.library_error));
        break;
    case AMQP_RESPONSE_SERVER_EXCEPTION:
        switch (x.reply.id) {
        case AMQP_CONNECTION_CLOSE_METHOD: {
            amqp_connection_close_t *m = (amqp_connection_close_t*) x.reply.decoded;
            fprintf(stderr, "%s: connection error %uh, msg: %.*s\n",
                    ctx, m->reply_code, (int) m->reply_text.len,
                    (char*) m->reply_text.bytes);
            break;
        }
        case AMQP_CHANNEL_CLOSE_METHOD: {
            amqp_channel_close_t *m = (amqp_channel_close_t*) x.reply.decoded;
            fprintf(stderr, "%s: channel error %uh, msg: %.*s\n",
                    ctx, m->reply_code, (int) m->reply_text.len,
                    (char*) m->reply_text.bytes);
            break;
        }
        default:
            fprintf(stderr, "%s: unknown server error, method id 0x%08X\n",
                    ctx, x.reply.id);
            break;
        }
        break;
    }
}

// Drain all pending frames (heartbeats, etc.) until no more are available
static void rabbitmq_conn_drain_frames(amqp_connection_state_t conn) {
    while (1) {
        amqp_frame_t frame;
        struct timeval timeout = {0, 10000}; // ~10ms
        int rc = amqp_simple_wait_frame_noblock(conn, &frame, &timeout);

        if (rc == AMQP_STATUS_TIMEOUT) {
            // No more frames waiting
            break;
        }
        if (rc != AMQP_STATUS_OK) {
            fprintf(stderr, "[rabbitmq] error draining frames: %d\n", rc);
            break;
        }
        // If you need to handle certain frame types, you can do so here.
    }
}

RabbitMQConn* rabbitmq_conn_new(const PluginConfig *config) {
    DBGPRINT("rabbitmq_conn_new(host=%s, port=%d)\n", config->host, config->port);
    if (!config) return NULL;

    RabbitMQConn *rc = calloc(1, sizeof(RabbitMQConn));
    if (!rc) {
        DBGPRINT("calloc failed.\n");
        return NULL;
    }

    rc->conn = amqp_new_connection();
    amqp_socket_t *sock = amqp_tcp_socket_new(rc->conn);
    if (!sock) {
        fprintf(stderr, "rabbitmq_conn_new: cannot create TCP socket\n");
        free(rc);
        return NULL;
    }

    int status = amqp_socket_open(sock, config->host, config->port);
    if (status) {
        fprintf(stderr, "rabbitmq_conn_new: socket_open failed %d\n", status);
        free(rc);
        return NULL;
    }

    // Request heartbeat = 10s
    amqp_rpc_reply_t r = amqp_login(
        rc->conn, "/", 0, 131072, 10,
        AMQP_SASL_METHOD_PLAIN,
        config->username, config->password
    );
    if (r.reply_type != AMQP_RESPONSE_NORMAL) {
        print_amqp_error(r, "amqp_login");
        rabbitmq_conn_free(rc);
        return NULL;
    }

    amqp_channel_open(rc->conn, 1);
    r = amqp_get_rpc_reply(rc->conn);
    if (r.reply_type != AMQP_RESPONSE_NORMAL) {
        print_amqp_error(r, "amqp_channel_open");
        rabbitmq_conn_free(rc);
        return NULL;
    }

    // Optional: enable publisher confirms
    amqp_confirm_select(rc->conn, 1);
    r = amqp_get_rpc_reply(rc->conn);
    if (r.reply_type != AMQP_RESPONSE_NORMAL) {
        fprintf(stderr, "Failed to enable publisher confirms\n");
        rabbitmq_conn_free(rc);
        return NULL;
    }

    rc->connected = 1;
    DBGPRINT("Connection established.\n");
    return rc;
}

void rabbitmq_conn_free(RabbitMQConn *conn) {
    DBGPRINT("rabbitmq_conn_free() called.\n");
    if (!conn) return;
    if (conn->connected) {
        amqp_channel_close(conn->conn, 1, AMQP_REPLY_SUCCESS);
        amqp_connection_close(conn->conn, AMQP_REPLY_SUCCESS);
        amqp_destroy_connection(conn->conn);
    }
    free(conn);
}

int rabbitmq_publish(
    RabbitMQConn *conn,
    const char *app_id,
    const char *username,
    const char *scope,
    const void *data,
    int app_id_len,
    int username_len,
    int data_len
) {
    DBGPRINT("rabbitmq_publish(scope=%s, app_id=%s, user_id=%s)\n",
             scope ? scope : "NULL",
             app_id ? app_id : "NULL",
             username ? username : "NULL");

    if (!conn || !conn->connected) return -1;
    if (!scope || !data) return -2;

    amqp_bytes_t msg_bytes = { .len = data_len, .bytes = (void*) data };
    amqp_bytes_t app_bytes = { .len = app_id_len, .bytes = (void*) app_id };
    amqp_bytes_t usr_bytes = { .len = username_len, .bytes = (void*) username };

    amqp_basic_properties_t props;
    memset(&props, 0, sizeof(props));
    props._flags = (AMQP_BASIC_DELIVERY_MODE_FLAG |
                    AMQP_BASIC_USER_ID_FLAG       |
                    AMQP_BASIC_APP_ID_FLAG);
    props.delivery_mode = 2; // persistent
    props.app_id        = app_bytes;
    props.user_id       = usr_bytes;

    // Publish
    int status = amqp_basic_publish(
        conn->conn, 1,
        amqp_cstring_bytes("to-validator"),
        amqp_cstring_bytes(scope),
        0, 0,
        &props,
        msg_bytes
    );
    if (status != AMQP_STATUS_OK) {
        fprintf(stderr, "[rabbitmq] amqp_basic_publish failed: %d\n", status);
        return -3;
    }

    // Wait for confirm
    struct timeval timeout = {1, 0};
    amqp_publisher_confirm_t cresult;
    amqp_rpc_reply_t reply = amqp_publisher_confirm_wait(conn->conn, &timeout, &cresult);
    if (reply.reply_type != AMQP_RESPONSE_NORMAL) {
        print_amqp_error(reply, "amqp_publisher_confirm_wait");
        return -4;
    }

    // Drain extra frames so broker never times out
    rabbitmq_conn_drain_frames(conn->conn);

    DBGPRINT("Publish success.\n");
    return 0;
}

int rabbitmq_subscribe(RabbitMQConn *conn, const char **topics, int n) {
    DBGPRINT("rabbitmq_subscribe: %d topics.\n", n);
    if (!conn || !conn->connected) return -1;
    if (n <= 0) return 0;

    // Real usage would bind + consume in a loop that also calls rabbitmq_conn_drain_frames.
    printf("[RabbitMQ] Subscribing to %d topic(s):\n", n);
    for (int i = 0; i < n; i++) {
        printf("  -> %s\n", topics[i]);
    }
    return 0;
}
