/**
 * @file yang_model.h
 * @brief YANG 模型解析接口
 *
 * 支持 YANG 简化子集解析：
 *   module / submodule / container / leaf / leaf-list / list
 *   typedef / grouping / uses
 *   type (基本类型: int8/16/32/64、uint8/16/32/64、string、boolean)
 *   must / when / default / mandatory
 *   config / description / reference
 *
 * 解析结果以 schema 树形式存放在 yang_model_t 中，
 * 供 NETCONF/RPC 层用于校验数据与构建 datastore。
 */
#ifndef DB_YANG_MODEL_H
#define DB_YANG_MODEL_H

#include "db/yang/yang_data.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 模式节点类型（与数据节点区分）
 * ============================================================ */

/**
 * @brief YANG schema 节点类型
 */
typedef enum yang_schema_kind_e {
    YANG_SCHEMA_MODULE = 0,    /**< module 顶层容器 */
    YANG_SCHEMA_CONTAINER,     /**< container */
    YANG_SCHEMA_LEAF,          /**< leaf */
    YANG_SCHEMA_LEAF_LIST,      /**< leaf-list */
    YANG_SCHEMA_LIST,          /**< list */
    YANG_SCHEMA_TYPEDEF,       /**< typedef（仅作占位） */
    YANG_SCHEMA_GROUPING,      /**< grouping（仅作占位） */
    YANG_SCHEMA_USES,          /**< uses（已展开） */
} yang_schema_kind_t;

/**
 * @brief YANG schema 节点
 */
typedef struct yang_schema_node_s {
    char name[YANG_MAX_NAME_LEN];          /**< 节点名 */
    yang_schema_kind_t kind;               /**< schema 类型 */

    /** 数据类型（仅 leaf / leaf-list 有效） */
    yang_type_value_t value_type;
    /** 缺省值（仅 leaf / leaf-list，可选） */
    char default_value[YANG_MAX_VALUE_LEN];
    /** 描述（可选） */
    char description[512];
    /** 引用（可选） */
    char reference[256];

    /** 必填标记（仅 leaf 有效） */
    bool mandatory;
    /** 配置数据标记（true 表示可写配置；false 表示只读状态） */
    bool config;

    /** list 节点键名（最多 8 个，以 '\0' 结束的空字符串结束） */
    char keys[8][YANG_MAX_NAME_LEN];

    struct yang_schema_node_s *parent;        /**< 父节点 */
    struct yang_schema_node_s *first_child;   /**< 首个子节点 */
    struct yang_schema_node_s *next_sibling;  /**< 兄弟节点 */
} yang_schema_node_t;

/**
 * @brief YANG 模型对象
 */
typedef struct yang_model_s {
    char module_name[YANG_MAX_NAME_LEN];     /**< module 名 */
    char ns_uri[256];                        /**< 命名空间 URI */
    yang_schema_node_t *root;                /**< 根（module 节点） */
    size_t node_count;                       /**< schema 节点数 */
} yang_model_t;

/* ============================================================
 * 模型生命周期
 * ============================================================ */

/**
 * @brief 创建空模型
 * @param module_name module 名称
 * @return 模型指针，失败返回 NULL
 */
yang_model_t *yang_model_create(const char *module_name);

/**
 * @brief 释放模型（含整棵 schema 树）
 * @param model 模型
 */
void yang_model_free(yang_model_t *model);

/**
 * @brief 解析 YANG 模型字符串
 * @param model 目标模型（需已 yang_model_create）
 * @param text YANG 源码
 * @param len 字节数（不含终止符，传 0 表示 strlen）
 * @return 0 成功，-1 解析失败
 *
 * 支持的语句（详见文件头注释）。
 */
int yang_model_parse(yang_model_t *model, const char *text, size_t len);

/**
 * @brief 加载并解析 YANG 文件
 * @param path 文件路径
 * @return 模型指针，失败返回 NULL
 */
yang_model_t *yang_model_load_file(const char *path);

/* ============================================================
 * Schema 节点操作
 * ============================================================ */

/**
 * @brief 创建 schema 节点
 * @param name 节点名
 * @param kind schema 类型
 * @return 新节点指针
 */
yang_schema_node_t *yang_schema_node_create(const char *name,
                                            yang_schema_kind_t kind);

/**
 * @brief 释放 schema 节点子树
 * @param node 根节点
 */
void yang_schema_node_free(yang_schema_node_t *node);

/**
 * @brief 向父节点追加子节点
 * @param parent 父节点
 * @param child 子节点
 * @return 0 成功，-1 失败
 */
int yang_schema_add_child(yang_schema_node_t *parent,
                          yang_schema_node_t *child);

/**
 * @brief 按路径查找 schema 节点
 * @param root 根节点
 * @param path 路径字符串
 * @return 节点指针，未找到返回 NULL
 */
yang_schema_node_t *yang_schema_find(yang_schema_node_t *root,
                                     const char *path);

#ifdef __cplusplus
}
#endif

#endif /* DB_YANG_MODEL_H */