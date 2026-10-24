/**
 * @file jsonpath.h
 * @brief JSONPath 解析器和查询接口
 *
 * 支持 JSONPath 表达式查询，如：
 * - $.name - 获取根对象的 name 字段
 * - $.store.book[0].title - 获取嵌套对象的字段
 * - $.items[*].price - 获取数组所有元素的 price
 * - $..author - 递归搜索所有 author 字段
 */
#ifndef DB_DOC_ENGING_JSONPATH_H
#define DB_DOC_ENGING_JSONPATH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>  /* for snprintf */

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * JSONPath 结果类型
 * ======================================================================== */

/** JSONPath 结果类型 */
typedef enum {
    JSONPATH_STRING = 0,   /**< 字符串值 */
    JSONPATH_NUMBER = 1,   /**< 数值 */
    JSONPATH_BOOL = 2,     /**< 布尔值 */
    JSONPATH_NULL = 3,     /**< null */
    JSONPATH_ARRAY = 4,    /**< 数组 */
    JSONPATH_OBJECT = 5    /**< 对象 */
} jsonpath_type_t;

/** 单个 JSONPath 结果 */
typedef struct jsonpath_value_s {
    char *path;            /**< 匹配的 JSONPath */
    jsonpath_type_t type;  /**< 值类型 */

    /* 值联合体 */
    union {
        char *str_val;     /**< 字符串值 */
        double num_val;    /**< 数值 */
        bool bool_val;     /**< 布尔值 */
    } value;

    size_t value_len;      /**< 值长度（字符串时） */
} jsonpath_value_t;

/** JSONPath 查询结果集 */
typedef struct jsonpath_result_s {
    jsonpath_value_t *values;  /**< 结果数组 */
    size_t count;              /**< 结果数量 */
    size_t capacity;           /**< 容量 */
} jsonpath_result_t;

/* ========================================================================
 * JSONPath API
 * ======================================================================== */

/**
 * @brief 解析 JSONPath 表达式并从 JSON 中提取数据
 * @param json JSON 文档字符串
 * @param json_len JSON 长度（0 表示自动计算）
 * @param jsonpath JSONPath 表达式（如 "$.store.book[0].title"）
 * @param out_result 输出结果（调用者负责释放）
 * @return 0 成功，-1 失败
 */
int jsonpath_query(const char *json, size_t json_len,
                   const char *jsonpath,
                   jsonpath_result_t *out_result);

/**
 * @brief 释放 JSONPath 查询结果
 * @param result 查询结果
 */
void jsonpath_free_result(jsonpath_result_t *result);

/**
 * @brief 检查 JSONPath 表达式是否有效
 * @param jsonpath JSONPath 表达式
 * @return true 有效，false 无效
 */
bool jsonpath_is_valid(const char *jsonpath);

/**
 * @brief 获取 JSONPath 表达式的描述
 * @param jsonpath JSONPath 表达式
 * @return 描述字符串
 */
const char *jsonpath_describe(const char *jsonpath);

/* ========================================================================
 * 过滤表达式支持
 * ======================================================================== */

/**
 * @brief JSONPath 过滤表达式上下文
 */
typedef struct jsonpath_filter_ctx_s {
    const char *json;      /**< JSON 文档 */
    size_t json_len;       /**< JSON 长度 */
    void *user_data;       /**< 用户数据 */
} jsonpath_filter_ctx_t;

/**
 * @brief 过滤表达式比较操作符
 */
typedef enum {
    FILTER_EQ = 0,         /**< 等于 */
    FILTER_NE = 1,         /**< 不等于 */
    FILTER_LT = 2,         /**< 小于 */
    FILTER_LE = 3,         /**< 小于等于 */
    FILTER_GT = 4,         /**< 大于 */
    FILTER_GE = 5,         /**< 大于等于 */
    FILTER_EXISTS = 6,     /**< 存在 */
    FILTER_MATCH = 7       /**< 正则匹配 */
} filter_op_t;

/**
 * @brief 解析并执行带过滤的 JSONPath 查询
 * @param json JSON 文档
 * @param json_len JSON 长度
 * @param jsonpath JSONPath 表达式（如 "$.items[?(@.price > 10)]"）
 * @param filter_ctx 过滤上下文
 * @param out_result 输出结果
 * @return 0 成功，-1 失败
 */
int jsonpath_query_with_filter(const char *json, size_t json_len,
                               const char *jsonpath,
                               jsonpath_filter_ctx_t *filter_ctx,
                               jsonpath_result_t *out_result);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取 JSON 值类型
 * @param json JSON 字符串
 * @return 值类型
 */
jsonpath_type_t jsonpath_get_type(const char *json);

/**
 * @brief 将 JSON 值转换为字符串
 * @param json JSON 值字符串
 * @param out 输出缓冲区
 * @param out_size 输出缓冲区大小
 * @return 转换后的字符串长度，-1 失败
 */
int jsonpath_to_string(const char *json, char *out, size_t out_size);

/**
 * @brief 获取 JSON 数组长度
 * @param json JSON 数组字符串
 * @return 数组长度，-1 失败
 */
int jsonpath_array_length(const char *json);

/**
 * @brief 获取 JSON 对象字段数量
 * @param json JSON 对象字符串
 * @return 字段数量，-1 失败
 */
int jsonpath_object_size(const char *json);

#ifdef __cplusplus
}
#endif

#endif /* DB_DOC_ENGING_JSONPATH_H */
