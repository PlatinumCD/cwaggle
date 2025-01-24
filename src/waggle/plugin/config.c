#include "waggle/config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef DEBUG
  #define DBGPRINT(...) do { fprintf(stderr, "[DEBUG config] "); fprintf(stderr, __VA_ARGS__); } while(0)
#else
  #define DBGPRINT(...) do {} while(0)
#endif

PluginConfig* plugin_config_new(const char *username,
                                const char *password,
                                const char *host,
                                int port,
                                const char *app_id) {
    DBGPRINT("plugin_config_new() called.\n");
    PluginConfig *cfg = malloc(sizeof(PluginConfig));
    if (!cfg) {
        DBGPRINT("malloc failed.\n");
        return NULL;
    }

    cfg->username = NULL;
    cfg->password = NULL;
    cfg->host = NULL;
    cfg->app_id = NULL;

    cfg->username = strdup(username ? username : "plugin");
    cfg->password = strdup(password ? password : "plugin");
    cfg->host     = strdup(host ? host : "rabbitmq");
    cfg->port     = port ? port : 5672;
    cfg->app_id   = strdup(app_id ? app_id : "");

    if (!cfg->username || !cfg->password || !cfg->host || !cfg->app_id) {
        DBGPRINT("String duplication failed. Freeing.\n");
        plugin_config_free(cfg);
        return NULL;
    }

    DBGPRINT("plugin_config_new: success.\n");
    return cfg;
}

void plugin_config_free(PluginConfig *config) {
    DBGPRINT("plugin_config_free() called.\n");
    if (!config) return;

    free(config->username);
    free(config->password);
    free(config->host);
    free(config->app_id);
    free(config);
}
