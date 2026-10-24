/**
 * @file doc_nested.h
 * @brief 嵌套文档存储头文件
 *
 * 支持嵌套对象、数组存储、增强 JSONPath 查询、嵌套字段索引和部分更新
 */
#ifndef DB_DOC_NESTED_H
#define DB_DOC_NESTED_H

#include "db/storage/doc/doc_engine.h"
#include "db/mm_pool.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 嵌套文档节点类型
 * ======================================================================== */

/**
 * @brief 嵌套值类型
 */
typedef enum DocNestedType_e {
    DOC_NESTED_NULL = 0,      /**< 空值 */
    DOC_NESTED_BOOL,          /**< 布尔值 */
    DOC_NESTED_INT,           /**< 整数 */
    DOC_NESTED_DOUBLE,        /**< 浮点数 */
    DOC_NESTED_STRING,        /**< 字符串 */
    DOC_NESTED_ARRAY,         /**< 数组 */
    DOC_NESTED_OBJECT,        /**< 对象 */
} DocNestedType;

/* ========================================================================
 * 嵌套文档解析
 * ======================================================================== */

/**
 * @brief 嵌套文档节点
 */
typedef struct DocNestedNode_s {
    DocNestedType type;       /**< 节点类型 */
    char *path;               /**< 完整路径 */
    char *key;                /**< 当前键名 */
    int index;                /**< 数组索引（-1 表示非数组元素） */

    /* 值联合体 */
    union {
        bool bool_val;
        int64_t int_val;
        double double_val;
        char *str_val;
        struct DocNestedNode_s **array_val;
        struct DocNestedNode_s **object_val;
    } value;

    size_t num_children;      /**< 子节点数量（数组/对象） */
    struct DocNestedNode_s *parent; /**< 父节点 */
} DocNestedNode;

/**
 * @brief 嵌套文档解析器
 */
typedef struct DocNestedParser_s {
    char *doc_json;           /**< 原始 JSON */
    size_t doc_len;           /**< JSON 长度 */
    DocNestedNode *root;      /**< 根节点 */
    void *mem_pool;           /**< 内存池 */
} DocNestedParser;

/**
 * @brief 创建嵌套文档解析器
 * @param json JSON 字符串
 * @param len JSON 长度（0 表示自动计算）
 * @return 解析器句柄，失败返回 NULL
 */
DocNestedParser *doc_nested_parser_create(const char *json, size_t len);

/**
 * @brief 销毁嵌套文档解析器
 */
void doc_nested_parser_destroy(DocNestedParser *parser);

/**
 * @brief 获取根节点
 */
DocNestedNode *doc_nested_get_root(DocNestedParser *parser);

/**
 * @brief 查找嵌套字段
 * @param parser 解析器
 * @param path JSONPath 路径（如 "$.store.book[0].title"）
 * @return 匹配的节点数组（调用者负责释放），NULL 表示未找到
 */
DocNestedNode **doc_nested_find(DocNestedParser *parser, const char *path,
                                size_t *out_count);

/**
 * @brief 获取节点值字符串
 * @param node 节点
 * @param out_len 输出长度
 * @return 值字符串（调用者不负责释放）
 */
const char *doc_nested_get_string(DocNestedNode *node, size_t *out_len);

/**
 * @brief 释放节点数组
 */
void doc_nested_free_nodes(DocNestedNode **nodes, size_t count);

/* ========================================================================
 * 嵌套文档序列化
 * ======================================================================== */

/**
 * @brief 节点转 JSON 字符串
 * @param node 节点
 * @param pretty 是否格式化输出
 * @return JSON 字符串（调用者负责释放）
 */
char *doc_nested_to_json(DocNestedNode *node, bool pretty);

/**
 * @brief 部分更新嵌套文档
 * @param parser 解析器
 * @param path 要更新的路径
 * @param value 新的值（JSON 格式）
 * @return 0 成功，-1 失败
 */
int doc_nested_update(DocNestedParser *parser, const char *path,
                     const char *value);

/**
 * @brief 删除嵌套字段
 * @param parser 解析器
 * @param path 要删除的路径
 * @return 0 成功，-1 失败
 */
int doc_nested_delete(DocNestedParser *parser, const char *path);

/**
 * @brief 添加嵌套字段
 * @param parser 解析器
 * @param path 路径
 * @param value JSON 值
 * @return 0 成功，-1 失败
 */
int doc_nested_insert(DocNestedParser *parser, const char *path,
                     const char *value);

/* ========================================================================
 * 嵌套文档索引
 * ======================================================================== */

/** 索引字段定义 */
typedef struct DocNestedField_s {
    char path[256];           /**< JSONPath 路径 */
    char field_name[64];      /**< 索引字段名 */
    int32_t data_type;        /**< 数据类型 */
    bool is_indexed;          /**< 是否已索引 */
} DocNestedField;

/** 嵌套字段索引 */
typedef struct DocNestedIndex_s {
    char index_name[128];     /**< 索引名 */
    DocNestedField *fields;   /**< 字段数组 */
    size_t num_fields;        /**< 字段数量 */
    void *index_impl;         /**< 底层索引实现 */
    void *mem_pool;           /**< 内存池 */
} DocNestedIndex;

/** 索引管理器 */
typedef struct DocNestedIndexMgr_s {
    DocNestedIndex **indexes; /**< 索引数组 */
    size_t num_indexes;       /**< 索引数量 */
    void *mem_pool;           /**< 内存池 */
} DocNestedIndexMgr;

/**
 * @brief 创建嵌套索引管理器
 */
DocNestedIndexMgr *doc_nested_index_mgr_create(void *mem_pool);

/**
 * @brief 销毁嵌套索引管理器
 */
void doc_nested_index_mgr_destroy(DocNestedIndexMgr *mgr);

/**
 * @brief 创建嵌套字段索引
 * @param mgr 索引管理器
 * @param index_name 索引名
 * @param field_paths 字段路径数组
 * @param num_fields 字段数量
 * @return 0 成功，-1 失败
 */
int doc_nested_index_create(DocNestedIndexMgr *mgr,
                           const char *index_name,
                           const char **field_paths,
                           size_t num_fields);

/**
 * @brief 删除索引
 */
int doc_nested_index_drop(DocNestedIndexMgr *mgr, const char *index_name);

/**
 * @brief 为文档建立索引
 * @param mgr 索引管理器
 * @param index_name 索引名
 * @param doc_id 文档 ID
 * @param parser 嵌套文档解析器
 * @return 0 成功，-1 失败
 */
int doc_nested_index_document(DocNestedIndexMgr *mgr,
                             const char *index_name,
                             const char *doc_id,
                             DocNestedParser *parser);

/**
 * @brief 从索引删除文档
 */
int doc_nested_index_remove(DocNestedIndexMgr *mgr,
                           const char *index_name,
                           const char *doc_id);

/**
 * @brief 索引查询
 * @param mgr 索引管理器
 * @param index_name 索引名
 * @param conditions 查询条件
 * @param out_doc_ids 输出文档 ID 数组
 * @param max_results 最大结果数
 * @return 实际结果数
 */
size_t doc_nested_index_search(DocNestedIndexMgr *mgr,
                               const char *index_name,
                               const char *conditions,
                               char ***out_doc_ids,
                               size_t max_results);

/* ========================================================================
 * 增强 JSONPath 查询
 * ======================================================================== */

/** JSONPath 查询模式 */
typedef enum DocJsonpathMode_e {
    DOC_JP_MODE_BASIC = 0,    /**< 基础模式 */
    DOC_JP_MODE_WILDCARD,     /**< 通配符模式 */
    DOC_JP_MODE_RECURSIVE,    /**< 递归搜索 */
    DOC_JP_MODE_FILTER,       /**< 过滤表达式 */
} DocJsonpathMode;

/** JSONPath 查询上下文 */
typedef struct DocJsonpathCtx_s {
    char *expression;         /**< JSONPath 表达式 */
    DocJsonpathMode mode;     /**< 查询模式 */
    char *filter_predicate;   /**< 过滤谓词 */
    int depth_limit;          /**< 深度限制（-1 表示无限制） */
    bool include_matches;     /**< 是否包含匹配信息 */
} DocJsonpathCtx;

/**
 * @brief 增强 JSONPath 解析
 * @param jsonpath JSONPath 表达式
 * @return 解析上下文，失败返回 NULL
 */
DocJsonpathCtx *doc_jsonpath_parse(const char *jsonpath);

/**
 * @brief 释放 JSONPath 上下文
 */
void doc_jsonpath_free(DocJsonpathCtx *ctx);

/**
 * @brief 执行增强 JSONPath 查询
 * @param json 文档 JSON
 * @param ctx 查询上下文
 * @param results 输出结果
 * @param max_results 最大结果数
 * @return 匹配数量
 */
size_t doc_jsonpath_execute(const char *json,
                           DocJsonpathCtx *ctx,
                           doc_jsonpath_result_t **results,
                           size_t max_results);

/**
 * @brief 支持通配符的 JSONPath 查询
 * @param json 文档 JSON
 * @param jsonpath 支持 * 和 $.. 的 JSONPath
 * @param results 输出结果
 * @param max_results 最大结果数
 * @return 匹配数量
 */
size_t doc_jsonpath_query_wildcard(const char *json,
                                   const char *jsonpath,
                                   doc_jsonpath_result_t **results,
                                   size_t max_results);

/**
 * @brief 支持谓词的 JSONPath 查询
 * @param json 文档 JSON
 * @param jsonpath 包含 [?(@.field op value)] 的 JSONPath
 * @param results 输出结果
 * @param max_results 最大结果数
 * @return 匹配数量
 */
size_t doc_jsonpath_query_predicate(const char *json,
                                    const char *jsonpath,
                                    doc_jsonpath_result_t **results,
                                    size_t max_results);

/* ========================================================================
 * 文档部分更新
 * ======================================================================== */

/** 部分更新操作类型 */
typedef enum DocPartialUpdateOp_e {
    DOC_UPDATE_SET = 0,       /**< 设置值 */
    DOC_UPDATE_UNSET,         /**< 删除字段 */
    DOC_UPDATE_INCREMENT,     /**< 递增 */
    DOC_UPDATE_APPEND,        /**< 追加到数组 */
    DOC_UPDATE_PUSH,          /**< 推入数组 */
    DOC_UPDATE_PULL,          /**< 从数组移除 */
} DocPartialUpdateOp;

/** 部分更新操作 */
typedef struct DocPartialUpdate_s {
    DocPartialUpdateOp op;    /**< 操作类型 */
    char path[256];           /**< JSONPath 路径 */
    char *value;              /**< 值（JSON 格式） */
} DocPartialUpdate;

/**
 * @brief 应用部分更新到文档
 * @param doc_json 原始文档
 * @param updates 更新操作数组
 * @param num_updates 更新数量
 * @param out_json 输出 JSON（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int doc_partial_update(const char *doc_json,
                      const DocPartialUpdate *updates,
                      size_t num_updates,
                      char **out_json);

/**
 * @brief 创建 SET 更新操作
 */
DocPartialUpdate doc_update_set(const char *path, const char *value);

/**
 * @brief 创建 UNSET 更新操作
 */
DocPartialUpdate doc_update_unset(const char *path);

/**
 * @brief 创建 INCREMENT 更新操作
 */
DocPartialUpdate doc_update_increment(const char *path, double delta);

/**
 * @brief 创建 APPEND 更新操作
 */
DocPartialUpdate doc_update_append(const char *path, const char *value);

/**
 * @brief 创建 PUSH 更新操作
 */
DocPartialUpdate doc_update_push(const char *path, const char *value);

/**
 * @brief 创建 PULL 更新操作
 */
DocPartialUpdate doc_update_pull(const char *path, const char *value);

/**
 * @brief 释放更新操作
 */
void doc_partial_update_free(DocPartialUpdate *updates, size_t count);

#ifdef __cplusplus
}
#endif

#endif /* DB_DOC_NESTED_H */
