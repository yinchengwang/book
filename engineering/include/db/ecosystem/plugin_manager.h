/**
 * @file include/db/ecosystem/plugin_manager.h
 * @brief Plugin system public API
 */
#ifndef DB_ECOSYSTEM_PLUGIN_MANAGER_H
#define DB_ECOSYSTEM_PLUGIN_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 类型定义
 * ============================================================ */

/** Plugin type enumeration */
typedef enum {
    PLUGIN_TYPE_STORAGE = 0,  /**< Storage plugin */
    PLUGIN_TYPE_INDEX,        /**< Index plugin */
    PLUGIN_TYPE_AUTH,         /**< Authentication plugin */
    PLUGIN_TYPE_UDF           /**< User-defined function plugin */
} plugin_type_t;

/** Plugin handle (opaque) */
typedef struct plugin plugin_t;

/** Plugin manager handle (opaque) */
typedef struct plugin_manager plugin_manager_t;

/* ============================================================
 * 生命周期
 * ============================================================ */

/**
 * @brief Create a plugin manager
 * @param plugin_dir Directory to load plugins from
 * @return Plugin manager handle, NULL on failure
 */
plugin_manager_t *plugin_manager_create(const char *plugin_dir);

/**
 * @brief Load a plugin by name
 * @param mgr Plugin manager handle
 * @param name Plugin name (without path or extension)
 * @return 0 on success, -1 on failure
 */
int plugin_manager_load(plugin_manager_t *mgr, const char *name);

/**
 * @brief Unload a plugin by name
 * @param mgr Plugin manager handle
 * @param name Plugin name
 * @return 0 on success, -1 on failure
 */
int plugin_manager_unload(plugin_manager_t *mgr, const char *name);

/**
 * @brief Destroy plugin manager and unload all plugins
 * @param mgr Plugin manager handle
 */
void plugin_manager_destroy(plugin_manager_t *mgr);

#ifdef __cplusplus
}
#endif

#endif /* DB_ECOSYSTEM_PLUGIN_MANAGER_H */
