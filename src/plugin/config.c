#include "waggle/config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

PluginConfig* plugin_config_new(const char *username,
                                const char *password,
                                const char *host,
                                int port,
                                const char *app_id) {
    PluginConfig *cfg = (PluginConfig*)malloc(sizeof(PluginConfig));
    if (!cfg) {
        return NULL;
    }

    cfg->username = NULL;
    cfg->password = NULL;
    cfg->host = NULL;
    cfg->app_id = NULL;

    // Duplicate strings safely
    cfg->username = strdup(username ? username : "plugin");
    cfg->password = strdup(password ? password : "plugin");
    cfg->host     = strdup(host ? host : "rabbitmq");
    cfg->port     = port ? port : 5672;
    cfg->app_id   = strdup(app_id ? app_id : "");

    if (!cfg->username || !cfg->password || !cfg->host || !cfg->app_id) {
        // Out of memory or similar
        plugin_config_free(cfg);
        return NULL;
    }

    return cfg;
}

void plugin_config_free(PluginConfig *config) {
    if (!config) {
        return;
    }

    if (config->username) free(config->username);
    if (config->password) free(config->password);
    if (config->host) free(config->host);
    if (config->app_id) free(config->app_id);

    free(config);
}
