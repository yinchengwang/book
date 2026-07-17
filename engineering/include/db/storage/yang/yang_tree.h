/**
 * @file yang_tree.h
 * @brief Yang 树模型查询头文件
 *
 * 实现 TREE_ANCESTORS、TREE_DESCENDANTS、TREE_DEPTH、TREE_PATH 等 SQL 函数
 */
#ifndef DB_YANG_TREE_H
#define DB_YANG_TREE_H

#include "db/storage/yang/yang_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 树遍历结果
 * ======================================================================== */

/** 祖先节点信息 */
typedef struct YangAncestor_s {
    char *path;                   /**< 节点路径 */
    char *name;                   /**< 节点名称 */
    yang_node_type_t type;        /**< 节点类型 */
    int depth;                    /**< 深度（根为 0） */
} YangAncestor;

/** 后代节点信息 */
typedef struct YangDescendant_s {
    char *path;                   /**< 节点路径 */
    char *name;                   /**< 节点名称 */
    yang_node_type_t type;        /**< 节点类型 */
    int depth;                    /**< 深度 */
} YangDescendant;

/** 路径信息 */
typedef struct YangPathInfo_s {
    char *full_path;              /**< 完整路径 */
    char **segments;              /**< 路径段数组 */
    size_t num_segments;          /**< 段数量 */
    int depth;                    /**< 深度 */
} YangPathInfo;

/* ========================================================================
 * 祖先查询
 * ======================================================================== */

/**
 * @brief 获取节点的祖先节点（包含自身）
 * @param tree 树引擎句柄
 * @param path 节点路径
 * @param include_self 是否包含自身
 * @param out_count 输出祖先数量
 * @return 祖先数组（调用者负责释放）
 */
YangAncestor *yang_get_ancestors(void *tree,
                                const char *path,
                                bool include_self,
                                size_t *out_count);

/**
 * @brief 获取节点的直接父节点
 * @param tree 树引擎句柄
 * @param path 节点路径
 * @return 父节点（调用者不负责释放），NULL 未找到
 */
yang_node_t *yang_get_parent(void *tree, const char *path);

/**
 * @brief 获取节点的所有祖先路径
 * @param tree 树引擎句柄
 * @param path 节点路径
 * @param separator 路径分隔符
 * @return 祖先路径数组（调用者负责释放）
 */
char **yang_get_ancestor_paths(void *tree,
                               const char *path,
                               const char *separator,
                               size_t *out_count);

/**
 * @brief 释放祖先数组
 */
void yang_free_ancestors(YangAncestor *ancestors, size_t count);

/* ========================================================================
 * 后代查询
 * ======================================================================== */

/**
 * @brief 获取节点的所有后代节点
 * @param tree 树引擎句柄
 * @param path 起始路径
 * @param max_depth 最大深度（-1 表示无限制）
 * @param out_count 输出后代数量
 * @return 后代数组（调用者负责释放）
 */
YangDescendant *yang_get_descendants(void *tree,
                                     const char *path,
                                     int max_depth,
                                     size_t *out_count);

/**
 * @brief 获取节点的直接子节点
 * @param tree 树引擎句柄
 * @param path 节点路径
 * @param out_count 输出子节点数量
 * @return 子节点数组（调用者负责释放）
 */
yang_node_t **yang_get_children(void *tree,
                                const char *path,
                                size_t *out_count);

/**
 * @brief 递归删除子树
 * @param tree 树引擎句柄
 * @param path 根路径
 * @return 删除的节点数量
 */
size_t yang_delete_subtree(void *tree, const char *path);

/**
 * @brief 释放后代数组
 */
void yang_free_descendants(YangDescendant *descendants, size_t count);

/* ========================================================================
 * 深度和路径查询
 * ======================================================================== */

/**
 * @brief 获取节点深度
 * @param tree 树引擎句柄
 * @param path 节点路径
 * @return 深度（根为 0），-1 未找到
 */
int yang_get_depth(void *tree, const char *path);

/**
 * @brief 获取路径深度（不查询树）
 * @param path 路径字符串
 * @param separator 分隔符
 * @return 深度
 */
int yang_path_depth(const char *path, const char *separator);

/**
 * @brief 获取节点路径的父路径
 * @param path 节点路径
 * @param separator 分隔符
 * @return 父路径（调用者负责释放）
 */
char *yang_parent_path(const char *path, const char *separator);

/**
 * @brief 获取路径的最后一个段
 * @param path 节点路径
 * @param separator 分隔符
 * @return 最后一段
 */
char *yang_path_leaf(const char *path, const char *separator);

/**
 * @brief 获取节点路径信息
 * @param tree 树引擎句柄
 * @param path 节点路径
 * @return 路径信息
 */
YangPathInfo *yang_get_path_info(void *tree, const char *path);

/**
 * @brief 释放路径信息
 */
void yang_free_path_info(YangPathInfo *info);

/* ========================================================================
 * XPath/JSONPath 查询
 * ======================================================================== */

/** 路径匹配模式 */
typedef enum YangPathPatternType_e {
    YANG_PATTERN_EXACT = 0,       /**< 精确匹配 */
    YANG_PATTERN_PREFIX,          /**< 前缀匹配 */
    YANG_PATTERN_WILDCARD,        /**< 通配符匹配 */
    YANG_PATTERN_REGEX,           /**< 正则匹配 */
} YangPathPatternType;

/** 路径匹配结果 */
typedef struct YangPatternMatch_s {
    char *path;                   /**< 匹配的路径 */
    yang_node_t *node;            /**< 节点指针 */
    int depth;                    /**< 深度 */
} YangPatternMatch;

/**
 * @brief 前缀路径查询
 * @param tree 树引擎句柄
 * @param prefix 路径前缀
 * @param out_count 输出匹配数量
 * @return 匹配结果数组
 */
YangPatternMatch *yang_match_prefix(void *tree,
                                   const char *prefix,
                                   size_t *out_count);

/**
 * @brief 通配符路径查询
 * @param tree 树引擎句柄
 * @param pattern 通配符模式（支持 * 和 **）
 * @param out_count 输出匹配数量
 * @return 匹配结果数组
 */
YangPatternMatch *yang_match_wildcard(void *tree,
                                     const char *pattern,
                                     size_t *out_count);

/**
 * @brief 释放匹配结果
 */
void yang_free_matches(YangPatternMatch *matches, size_t count);

/* ========================================================================
 * SQL 函数
 * ======================================================================== */

/**
 * @brief TREE_ANCESTORS SQL 函数
 *
 * TREE_ANCESTORS(path)
 *
 * 示例:
 *   SELECT TREE_ANCESTORS('/root/users/alice/profile')
 */
char *yang_sql_ancestors(const char *path);

/**
 * @brief TREE_DESCENDANTS SQL 函数
 *
 * TREE_DESCENDANTS(path, max_depth)
 *
 * 示例:
 *   SELECT TREE_DESCENDANTS('/root/users', 2)
 */
char *yang_sql_descendants(const char *path, int max_depth);

/**
 * @brief TREE_DEPTH SQL 函数
 *
 * TREE_DEPTH(path)
 *
 * 示例:
 *   SELECT TREE_DEPTH('/root/users/alice')
 */
int yang_sql_depth(const char *path);

/**
 * @brief TREE_PATH SQL 函数
 *
 * TREE_PATH(path, separator)
 *
 * 示例:
 *   SELECT TREE_PATH('/root/users/alice', '/')
 */
char *yang_sql_path(const char *path, const char *separator);

/**
 * @brief TREE_LEVEL_PATH SQL 函数
 *
 * TREE_LEVEL_PATH(root, level)
 *
 * 示例:
 *   SELECT TREE_LEVEL_PATH('/root', 2)
 */
char **yang_sql_level_path(const char *root, int level, size_t *out_count);

/**
 * @brief XPATH_SIMILAR SQL 函数
 *
 * XPATH_SIMILAR(path, pattern)
 */
char *yang_sql_xpath(const char *path, const char *pattern);

/* ========================================================================
 * 树操作函数
 * ======================================================================== */

/**
 * @brief 复制子树
 * @param tree 树引擎句柄
 * @param src_path 源路径
 * @param dest_path 目标路径
 * @return 复制的节点数
 */
size_t yang_copy_subtree(void *tree, const char *src_path, const char *dest_path);

/**
 * @brief 移动子树
 * @param tree 树引擎句柄
 * @param src_path 源路径
 * @param dest_path 目标路径
 * @return 0 成功，-1 失败
 */
int yang_move_subtree(void *tree, const char *src_path, const char *dest_path);

/**
 * @brief 获取树统计信息
 * @param tree 树引擎句柄
 * @param out_depth 输出最大深度
 * @param out_nodes 输出节点总数
 */
void yang_get_tree_stats(void *tree, int *out_depth, size_t *out_nodes);

#ifdef __cplusplus
}
#endif

#endif /* DB_YANG_TREE_H */
