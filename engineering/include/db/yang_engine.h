/**
 * @file yang_engine.h
 * @brief Yang 树存储引擎头文件
 *
 * 定义 Yang 树存储引擎的接口和数据结构，支持层次结构数据存储和遍历。
 */
#ifndef DB_YANG_ENGINE_H
#define DB_YANG_ENGINE_H

#include "storage_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Yang 树相关类型定义
 * ======================================================================== */

/**
 * @brief 节点类型
 */
typedef enum {
    YANG_NODE_ELEMENT = 0,     /**< 元素节点 */
    YANG_NODE_ATTRIBUTE = 1,   /**< 属性节点 */
    YANG_NODE_TEXT = 2,        /**< 文本节点 */
    YANG_NODE_ROOT = 3,        /**< 根节点 */
} yang_node_type_t;

/**
 * @brief 树节点
 */
typedef struct yang_node_s {
    char *path;                /**< 节点路径 */
    char *name;                /**< 节点名称 */
    yang_node_type_t type;     /**< 节点类型 */
    char *value;               /**< 节点值 */

    struct yang_node_s *parent;    /**< 父节点 */
    struct yang_node_s *first_child;   /**< 第一个子节点 */
    struct yang_node_s *next_sibling;  /**< 下一个兄弟节点 */
} yang_node_t;

/**
 * @brief Yang 引擎数据库
 */
typedef struct yang_engine_db_s {
    char name[256];            /**< 树名称 */
    char data_dir[512];        /**< 数据目录 */
    AccessMode mode;           /**< 访问模式 */

    yang_node_t *root;         /**< 根节点 */
    uint64_t num_nodes;        /**< 节点数量 */
} yang_engine_db_t;

/**
 * @brief 获取 Yang 引擎操作表
 */
const storage_ops_t *yang_engine_get_ops(void);

/**
 * @brief 初始化 Yang 引擎
 */
int yang_engine_init(const char *data_dir);

/**
 * @brief 关闭 Yang 引擎
 */
int yang_engine_shutdown(void);

/**
 * @brief 创建树
 */
int yang_engine_create(const char *name, const storage_schema_t *schema);

/**
 * @brief 打开树
 */
void *yang_engine_open(const char *name, AccessMode mode);

/**
 * @brief 关闭树
 */
int yang_engine_close(void *rel);

/**
 * @brief 删除树
 */
int yang_engine_drop(const char *name);

/**
 * @brief 插入节点
 */
int yang_engine_insert(void *rel, const void *data, size_t len);

/**
 * @brief 获取节点
 */
int yang_engine_get(void *rel, const void *key, size_t key_len,
                    void **out_data, size_t *out_len);

/**
 * @brief 获取统计信息
 */
int yang_engine_stats(const char *name, storage_stats_t *stats);

/* ========================================================================
 * 节点操作 API
 * ======================================================================== */

/**
 * @brief 创建节点
 */
yang_node_t *yang_node_create(const char *name, yang_node_type_t type,
                               const char *value);

/**
 * @brief 释放节点
 */
void yang_node_free(yang_node_t *node);

/**
 * @brief 添加子节点
 */
int yang_add_child(yang_node_t *parent, yang_node_t *child);

/**
 * @brief 获取子节点
 */
yang_node_t *yang_get_child(const yang_node_t *node, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* DB_YANG_ENGINE_H */
