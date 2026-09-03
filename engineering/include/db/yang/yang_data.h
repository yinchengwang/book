/**
 * @file yang_data.h
 * @brief YANG 数据节点结构定义
 *
 * 定义 YANG 数据树（Data Tree）的节点结构与操作接口。
 * 数据树由运行/启动/候选数据存储（datastore）维护，
 * 节点之间构成层次结构（container / list / leaf-list / leaf）。
 */
#ifndef DB_YANG_DATA_H
#define DB_YANG_DATA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 最大节点名称长度 */
#define YANG_MAX_NAME_LEN      128
/** 最大节点路径长度 */
#define YANG_MAX_PATH_LEN      512
/** 最大节点值长度 */
#define YANG_MAX_VALUE_LEN     4096

/* ============================================================
 * 节点类型
 * ============================================================ */

/**
 * @brief YANG 数据节点类型
 */
typedef enum yang_node_kind_e {
    YANG_KIND_UNKNOWN = 0,    /**< 未知类型 */
    YANG_KIND_CONTAINER,      /**< container：内部节点 */
    YANG_KIND_LIST,           /**< list：有键列表 */
    YANG_KIND_LEAF,           /**< leaf：标量叶节点 */
    YANG_KIND_LEAF_LIST,      /**< leaf-list：标量列表 */
} yang_node_kind_t;

/**
 * @brief 数据类型
 *
 * 与 YANG 内置类型对齐，便于后续校验与编解码。
 */
typedef enum yang_value_type_e {
    YANG_TYPE_EMPTY = 0,      /**< 空（容器等无值节点） */
    YANG_TYPE_INT8,
    YANG_TYPE_INT16,
    YANG_TYPE_INT32,
    YANG_TYPE_INT64,
    YANG_TYPE_UINT8,
    YANG_TYPE_UINT16,
    YANG_TYPE_UINT32,
    YANG_TYPE_UINT64,
    YANG_TYPE_STRING,
    YANG_TYPE_BOOLEAN,
} yang_type_value_t;

/* ============================================================
 * 数据节点结构
 * ============================================================ */

/**
 * @brief YANG 数据节点（双向链表 + 孩子链表）
 */
typedef struct yang_data_node_s {
    char name[YANG_MAX_NAME_LEN];           /**< 节点名（不含命名空间） */
    yang_node_kind_t kind;                  /**< 节点类型 */
    yang_type_value_t value_type;           /**< 值类型 */

    /** 节点值（仅 leaf / leaf-list 有效；其它节点置空字符串） */
    char value[YANG_MAX_VALUE_LEN];

    /** list 节点的键值（最多 8 个键；-1 表示该位未使用） */
    char keys[8][YANG_MAX_NAME_LEN];

    struct yang_data_node_s *parent;        /**< 父节点 */
    struct yang_data_node_s *first_child;   /**< 首个子节点 */
    struct yang_data_node_s *next_sibling;  /**< 下一个兄弟节点 */
    struct yang_data_node_s *prev_sibling;  /**< 上一个兄弟节点 */
} yang_data_node_t;

/* ============================================================
 * 数据树（datastore 内部使用）
 * ============================================================ */

/**
 * @brief 数据存储（datastore）
 *
 * 多实例互不影响，分别表示 running / startup / candidate。
 */
typedef struct yang_datastore_s {
    char name[YANG_MAX_NAME_LEN];       /**< datastore 名称 */
    yang_data_node_t *root;             /**< 根节点 */
    size_t node_count;                  /**< 节点数量 */
} yang_datastore_t;

/* ============================================================
 * 数据节点操作
 * ============================================================ */

/**
 * @brief 创建数据节点
 * @param name 节点名
 * @param kind 节点类型
 * @param value_type 值类型
 * @return 新节点指针，失败返回 NULL
 *
 * 节点创建后默认 value 字段为空字符串，不挂入任何树。
 */
yang_data_node_t *yang_data_node_create(const char *name,
                                        yang_node_kind_t kind,
                                        yang_type_value_t value_type);

/**
 * @brief 递归释放子树
 * @param node 根节点（允许为 NULL）
 */
void yang_data_node_free(yang_data_node_t *node);

/**
 * @brief 添加子节点到父节点尾部
 * @param parent 父节点
 * @param child 子节点
 * @return 0 成功，-1 失败
 */
int yang_data_add_child(yang_data_node_t *parent, yang_data_node_t *child);

/**
 * @brief 按名字查找直接子节点
 * @param parent 父节点
 * @param name 子节点名
 * @return 节点指针，未找到返回 NULL
 */
yang_data_node_t *yang_data_find_child(yang_data_node_t *parent,
                                       const char *name);

/**
 * @brief 通过完整路径获取节点
 * @param root 根节点
 * @param path 路径，例如 "/network/interface[name='eth0']/mtu"
 * @return 节点指针，未找到返回 NULL
 */
yang_data_node_t *yang_data_get_node(yang_data_node_t *root, const char *path);

/**
 * @brief 创建（或获取）节点：沿路径逐层创建缺失节点
 * @param root 根节点
 * @param path 节点路径
 * @param kind 新建节点的类型（仅在需要创建时使用）
 * @return 末端节点指针，失败返回 NULL
 */
yang_data_node_t *yang_data_create_node(yang_data_node_t *root,
                                        const char *path,
                                        yang_node_kind_t kind);

/**
 * @brief 删除子树
 * @param parent 父节点
 * @param name 子节点名
 * @return 0 成功，-1 未找到
 */
int yang_data_remove_child(yang_data_node_t *parent, const char *name);

/**
 * @brief 复制节点（深拷贝，含整棵子树）
 * @param src 源节点
 * @return 新节点指针
 */
yang_data_node_t *yang_data_clone(const yang_data_node_t *src);

/**
 * @brief 字符串转 yang_type_value_t
 * @param s 类型名，例如 "int32" "string" "boolean"
 * @return 对应枚举，未知返回 YANG_TYPE_EMPTY
 */
yang_type_value_t yang_data_type_from_string(const char *s);

/**
 * @brief yang_type_value_t 转字符串
 * @param t 类型枚举
 * @return 类型名，例如 "int32"
 */
const char *yang_data_type_to_string(yang_type_value_t t);

/**
 * @brief 将字符串解析为目标类型并写入节点
 * @param node 目标节点
 * @param text 源字符串
 * @return 0 成功，-1 类型不匹配
 */
int yang_data_set_value_from_string(yang_data_node_t *node, const char *text);

/**
 * @brief 输出节点值的规范化字符串
 * @param node 节点
 * @param out 输出缓冲区
 * @param out_len 缓冲区长度
 * @return 写入长度（不含终止符），-1 失败
 */
int yang_data_format_value(const yang_data_node_t *node,
                           char *out, size_t out_len);

/* ============================================================
 * Datastore 操作
 * ============================================================ */

/**
 * @brief 创建 datastore
 * @param name datastore 名称
 * @return datastore 指针，失败返回 NULL
 */
yang_datastore_t *yang_datastore_create(const char *name);

/**
 * @brief 释放 datastore（含整棵树）
 * @param ds datastore
 */
void yang_datastore_free(yang_datastore_t *ds);

/**
 * @brief 清空 datastore（保留根容器，重新初始化）
 * @param ds datastore
 * @return 0 成功，-1 失败
 */
int yang_datastore_clear(yang_datastore_t *ds);

/**
 * @brief 获取 datastore 根节点
 * @param ds datastore
 * @return 根节点指针
 */
yang_data_node_t *yang_datastore_root(yang_datastore_t *ds);

#ifdef __cplusplus
}
#endif

#endif /* DB_YANG_DATA_H */