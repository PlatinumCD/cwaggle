#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "waggle/plugin.h"
#include "waggle/config.h"
#include "waggle/timeutil.h"

int main() {
    // Retrieve environment variables with defaults
    const char *username = getenv("WAGGLE_PLUGIN_USERNAME");
    if (!username) username = "plugin";

    const char *password = getenv("WAGGLE_PLUGIN_PASSWORD");
    if (!password) password = "plugin";

    const char *host = getenv("WAGGLE_PLUGIN_HOST");
    if (!host) host = "rabbitmq";

    const char *port_str = getenv("WAGGLE_PLUGIN_PORT");
    int port = port_str ? atoi(port_str) : 5672;

    const char *app_id = getenv("WAGGLE_APP_ID");
    if (!app_id) app_id = "";

    // Create plugin configuration
    PluginConfig *cfg = plugin_config_new(username, password, host, port, app_id);
    if (!cfg) {
        fprintf(stderr, "Failed to create PluginConfig.\n");
        return 1;
    }

    // Create plugin instance
    Plugin *p = plugin_new(cfg);
    if (!p) {
        fprintf(stderr, "Failed to create Plugin.\n");
        return 2;
    }

    // Publish an example message
    plugin_publish(p, "all", "test.metric", 123, waggle_get_timestamp_ns(), "{\"example\":\"meta\"}");

    // Cleanup
    plugin_free(p);

    return 0;
}
