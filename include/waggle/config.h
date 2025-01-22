#ifndef WAGGLE_CONFIG_H
#define WAGGLE_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char *username;
    char *password;
    char *host;
    int   port;
    char *app_id;
} PluginConfig;

/**
 * Allocates and initializes a new PluginConfig.
 *
 * The strings are duplicated, so the caller can free them after.
 * Returns NULL on failure.
 */
PluginConfig* plugin_config_new(const char *username,
                                const char *password,
                                const char *host,
                                int port,
                                const char *app_id);

/**
 * Frees a PluginConfig and all its internal strings.
 * Safe to call with NULL.
 */
void plugin_config_free(PluginConfig *config);

#ifdef __cplusplus
}
#endif

#endif
