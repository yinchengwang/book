/**
 * @file src/db/ecosystem/plugin_manager.c
 * @brief Plugin system implementation
 *
 * Dynamic plugin loading based on LoadLibrary/GetProcAddress on Windows.
 */
#include <db/ecosystem/plugin_manager.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ========================================================================
 * 内部结构
 * ======================================================================== */

/** Loaded plugin entry */
typedef struct plugin_entry {
    plugin_t plugin;                  /**< Plugin descriptor */
    HMODULE handle;                   /**< DLL handle */
    char name[128];                   /**< Plugin name */
    struct plugin_entry *next;        /**< Next entry */
} plugin_entry_t;

/** Plugin manager internal state */
struct plugin_manager {
    char plugin_dir[512];             /**< Plugin directory */
    plugin_entry_t *plugins;          /**< Loaded plugins list */
    pthread_mutex_t mutex;            /**< Mutex for thread safety */
};

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 构建插件完整路径
 */
static void build_plugin_path(char *path, size_t path_size,
                              const char *dir, const char *name) {
    snprintf(path, path_size, "%s/%s.dll", dir, name);
}

/**
 * @brief 从句柄路径获取插件名
 */
static void extract_plugin_name(char *name, size_t name_size, const char *path) {
    const char *base = strrchr(path, '/');
    if (!base) base = strrchr(path, '\\');
    if (!base) base = path;
    else base++;

    size_t len = strlen(base);
    if (len > 4 && strcmp(base + len - 4, ".dll") == 0) {
        len -= 4;
    }

    if (len >= name_size) len = name_size - 1;
    strncpy(name, base, len);
    name[len] = '\0';
}

/* ========================================================================
 * 生命周期实现
 * ======================================================================== */

plugin_manager_t *plugin_manager_create(const char *plugin_dir) {
    if (!plugin_dir) return NULL;

    plugin_manager_t *mgr = calloc(1, sizeof(plugin_manager_t));
    if (!mgr) return NULL;

    strncpy(mgr->plugin_dir, plugin_dir, sizeof(mgr->plugin_dir) - 1);
    mgr->plugin_dir[sizeof(mgr->plugin_dir) - 1] = '\0';
    mgr->plugins = NULL;
    pthread_mutex_init(&mgr->mutex, NULL);

    return mgr;
}

int plugin_manager_load(plugin_manager_t *mgr, const char *name) {
    if (!mgr || !name) return -1;

    pthread_mutex_lock(&mgr->mutex);

    /* 检查是否已加载 */
    plugin_entry_t *entry = mgr->plugins;
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            pthread_mutex_unlock(&mgr->mutex);
            return 0;  /* 已加载 */
        }
        entry = entry->next;
    }

    /* 构建路径 */
    char path[768];
    build_plugin_path(path, sizeof(path), mgr->plugin_dir, name);

    /* 加载 DLL */
    HMODULE handle = LoadLibraryA(path);
    if (!handle) {
        pthread_mutex_unlock(&mgr->mutex);
        return -1;
    }

    /* 获取插件符号 */
    typedef plugin_t *(*get_plugin_t)(void);
    get_plugin_t get_plugin = (get_plugin_t)GetProcAddress(handle, "get_plugin");
    if (!get_plugin) {
        FreeLibrary(handle);
        pthread_mutex_unlock(&mgr->mutex);
        return -1;
    }

    plugin_t *plugin = get_plugin();
    if (!plugin) {
        FreeLibrary(handle);
        pthread_mutex_unlock(&mgr->mutex);
        return -1;
    }

    /* 创建条目 */
    entry = calloc(1, sizeof(plugin_entry_t));
    if (!entry) {
        FreeLibrary(handle);
        pthread_mutex_unlock(&mgr->mutex);
        return -1;
    }

    memcpy(&entry->plugin, plugin, sizeof(plugin_t));
    strncpy(entry->name, name, sizeof(entry->name) - 1);
    entry->handle = handle;

    /* 插入链表 */
    entry->next = mgr->plugins;
    mgr->plugins = entry;

    /* 调用 init */
    if (entry->plugin.init) {
        entry->plugin.init();
    }

    /* 调用 start */
    if (entry->plugin.start) {
        entry->plugin.start();
    }

    pthread_mutex_unlock(&mgr->mutex);
    return 0;
}

int plugin_manager_unload(plugin_manager_t *mgr, const char *name) {
    if (!mgr || !name) return -1;

    pthread_mutex_lock(&mgr->mutex);

    /* 查找插件 */
    plugin_entry_t **prev = &mgr->plugins;
    plugin_entry_t *entry = mgr->plugins;

    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            /* 调用 stop */
            if (entry->plugin.stop) {
                entry->plugin.stop();
            }

            /* 调用 destroy */
            if (entry->plugin.destroy) {
                entry->plugin.destroy();
            }

            /* 从链表移除 */
            *prev = entry->next;

            /* 释放 DLL */
            FreeLibrary(entry->handle);
            free(entry);

            pthread_mutex_unlock(&mgr->mutex);
            return 0;
        }
        prev = &entry->next;
        entry = entry->next;
    }

    pthread_mutex_unlock(&mgr->mutex);
    return -1;
}

void plugin_manager_destroy(plugin_manager_t *mgr) {
    if (!mgr) return;

    pthread_mutex_lock(&mgr->mutex);

    /* 卸载所有插件 */
    plugin_entry_t *entry = mgr->plugins;
    while (entry) {
        plugin_entry_t *next = entry->next;

        if (entry->plugin.stop) {
            entry->plugin.stop();
        }
        if (entry->plugin.destroy) {
            entry->plugin.destroy();
        }

        FreeLibrary(entry->handle);
        free(entry);

        entry = next;
    }

    mgr->plugins = NULL;
    pthread_mutex_unlock(&mgr->mutex);
    pthread_mutex_destroy(&mgr->mutex);
    free(mgr);
}
