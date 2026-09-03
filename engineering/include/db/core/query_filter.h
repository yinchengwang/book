/**
 * @file query_filter.h
 * @brief 标量过滤表达式解析和执行
 *
 * 支持的语法：
 *   - 比较操作符: =, !=, <, <=, >, >=
 *   - 逻辑操作符: AND, OR, NOT
 *   - 括号: ( )
 *   - 类型: 整数、浮点、字符串、布尔
 *
 * 示例：
 *   price < 100 AND category = "electronics"
 *   (status = "active" OR status = "pending") AND count > 0
 */
#ifndef DB_QUERY_FILTER_H
#define DB_QUERY_FILTER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 过滤值类型
 * ======================================================================== */

/** 过滤值类型 */
#ifndef FILTER_VALUE_TYPE_DEFINED
#define FILTER_VALUE_TYPE_DEFINED
typedef enum VDBFilterValueType_e {
    VDB_FILTER_VALUE_NULL = 0,   /**< NULL 值 */
    VDB_FILTER_VALUE_INT,        /**< 整数 */
    VDB_FILTER_VALUE_FLOAT,      /**< 浮点 */
    VDB_FILTER_VALUE_STRING,     /**< 字符串 */
    VDB_FILTER_VALUE_BOOL        /**< 布尔 */
} VDBFilterValueType;
#endif

/** 过滤值类型别名（兼容 hybrid_search.h 等旧代码） */
typedef VDBFilterValueType FilterValueType;



/** 过滤值联合体 */
typedef union VDBFilterValueData_u {
    int64_t int_val;         /**< 整数值 */
    double float_val;        /**< 浮点值 */
    char *str_val;           /**< 字符串值（需释放） */
    bool bool_val;           /**< 布尔值 */
} VDBFilterValueData;

/** 过滤值 */
typedef struct VDBFilterValue_s {
    VDBFilterValueType type;     /**< 值类型 */
    VDBFilterValueData data;     /**< 值数据 */
} VDBFilterValue;

/** 过滤值别名（兼容 hybrid_search.h 等旧代码） */
typedef VDBFilterValue FilterValue;
typedef VDBFilterValueData FilterValueData;

/* ========================================================================
 * 过滤操作符
 * ======================================================================== */

/** 过滤操作符 */
#ifndef VDB_FILTER_OP_DEFINED
#define VDB_FILTER_OP_DEFINED
typedef enum VDBFilterOp_e {
    VDB_FILTER_OP_INVALID = -1,

    /* 比较操作符 */
    VDB_FILTER_OP_EQ = 0,        /**< 等于 (=) */
    VDB_FILTER_OP_NE,            /**< 不等于 (!=) */
    VDB_FILTER_OP_LT,            /**< 小于 (<) */
    VDB_FILTER_OP_LE,            /**< 小于等于 (<=) */
    VDB_FILTER_OP_GT,            /**< 大于 (>) */
    VDB_FILTER_OP_GE,            /**< 大于等于 (>=) */

    /* 逻辑操作符 */
    VDB_FILTER_OP_AND,           /**< 逻辑与 */
    VDB_FILTER_OP_OR,            /**< 逻辑或 */
    VDB_FILTER_OP_NOT            /**< 逻辑非 */
} VDBFilterOp;
#endif

/** 过滤操作符别名（兼容 hybrid_search.h 等旧代码） */
#ifndef FILTER_OPERATOR_ALIAS_DEFINED
#define FILTER_OPERATOR_ALIAS_DEFINED
typedef VDBFilterOp FilterOperator;
typedef VDBFilterOp FilterOperator2;
#endif

/* ========================================================================
 * 过滤表达式 AST 节点类型
 * ======================================================================== */

/** AST 节点类型 */
typedef enum FilterNodeType_e {
    FILTER_NODE_INVALID = -1,
    FILTER_NODE_COMPARISON,  /**< 比较表达式 */
    FILTER_NODE_LOGICAL,     /**< 逻辑表达式 */
    FILTER_NODE_VALUE        /**< 值节点（叶子节点） */
} FilterNodeType;

/* ========================================================================
 * 过滤表达式 AST
 * ======================================================================== */

/** 比较表达式节点 */
typedef struct FilterComparison_s {
    char *field_name;        /**< 字段名 */
    VDBFilterOp op;          /**< 比较操作符 */
    VDBFilterValue value;    /**< 比较值 */
} FilterComparison;

/** 逻辑表达式节点 */
typedef struct FilterLogical_s {
    VDBFilterOp op;          /**< 逻辑操作符 (AND/OR/NOT) */
    struct FilterNode_s *left;   /**< 左操作数 */
    struct FilterNode_s *right;  /**< 右操作数（NOT 时为 NULL） */
} FilterLogical;

/** AST 节点 */
typedef struct FilterNode_s {
    FilterNodeType type;     /**< 节点类型 */
    union {
        FilterComparison comparison; /**< 比较表达式 */
        FilterLogical logical;       /**< 逻辑表达式 */
        VDBFilterValue value;        /**< 值节点 */
    } data;
} FilterNode;

/* ========================================================================
 * 记录访问接口
 * ======================================================================== */

/**
 * @brief 获取记录中字段值的回调函数类型
 * @param ctx 用户上下文
 * @param field_name 字段名
 * @param out_value 输出值
 * @return 0 成功，-1 字段不存在
 */
typedef int (*FilterGetFieldCallback)(void *ctx, const char *field_name, VDBFilterValue *out_value);

/**
 * @brief 记录过滤器
 */
typedef struct FilterContext_s {
    void *record_ctx;                    /**< 记录上下文 */
    FilterGetFieldCallback get_field;    /**< 获取字段值回调 */
} FilterContext;

/* ========================================================================
 * 解析器
 * ======================================================================== */

/** 解析器错误码 */
typedef enum FilterParseError_e {
    FILTER_PARSE_OK = 0,
    FILTER_PARSE_ERR_INVALID_TOKEN,      /**< 无效的 token */
    FILTER_PARSE_ERR_UNEXPECTED_TOKEN,   /**< 意外的 token */
    FILTER_PARSE_ERR_UNTERMINATED_STRING,/**< 未终止的字符串 */
    FILTER_PARSE_ERR_INVALID_FIELD,      /**< 无效的字段名 */
    FILTER_PARSE_ERR_INVALID_OP,         /**< 无效的操作符 */
    FILTER_PARSE_ERR_MISSING_VALUE,      /**< 缺少值 */
    FILTER_PARSE_ERR_MISMATCHED_PAREN,   /**< 括号不匹配 */
    FILTER_PARSE_ERR_EMPTY_EXPRESSION,   /**< 空表达式 */
    FILTER_PARSE_ERR_OUT_OF_MEMORY       /**< 内存不足 */
} FilterParseError;

/** 解析错误信息 */
typedef struct FilterParseResult_s {
    FilterNode *ast;             /**< 解析得到的 AST */
    FilterParseError error;      /**< 错误码 */
    int error_pos;               /**< 错误位置 */
    char error_msg[256];         /**< 错误消息 */
} FilterParseResult;

/**
 * @brief 解析过滤表达式字符串
 * @param expr 表达式字符串
 * @return 解析结果（需调用 filter_parse_result_free 释放）
 */
FilterParseResult *filter_parse(const char *expr);

/**
 * @brief 释放解析结果
 * @param result 解析结果
 */
void filter_parse_result_free(FilterParseResult *result);

/* ========================================================================
 * 表达式执行
 * ======================================================================== */

/** 过滤执行统计 */
typedef struct FilterStats_s {
    int64_t total_records;       /**< 总记录数 */
    int64_t matched_records;     /**< 匹配记录数 */
    int64_t filtered_records;    /**< 过滤掉的记录数 */
} FilterStats;

/**
 * @brief 执行过滤表达式
 * @param ast AST 根节点
 * @param ctx 过滤上下文
 * @return true 记录匹配，false 不匹配
 */
bool filter_evaluate(FilterNode *ast, FilterContext *ctx);

/**
 * @brief 批量执行过滤
 * @param ast AST 根节点
 * @param ctx 过滤上下文
 * @param record_ids 记录 ID 数组
 * @param num_records 记录数量
 * @param results 结果数组（输出，调用方分配）
 * @param stats 统计信息（输出，可为 NULL）
 * @return 匹配数量
 */
int filter_evaluate_batch(FilterNode *ast, FilterContext *ctx,
                          const int64_t *record_ids, int num_records,
                          bool *results, FilterStats *stats);

/**
 * @brief 释放 AST
 * @param ast AST 根节点
 */
void filter_ast_free(FilterNode *ast);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取操作符字符串表示
 * @param op 操作符
 * @return 操作符字符串
 */
const char *filter_op_to_string(VDBFilterOp op);

/**
 * @brief 获取错误信息
 * @param error 错误码
 * @return 错误消息
 */
const char *filter_error_to_string(FilterParseError error);

/**
 * @brief 创建字符串值
 * @param str 字符串
 * @return VDBFilterValue（需调用 filter_value_free 释放）
 */
VDBFilterValue *filter_value_create_string(const char *str);

/**
 * @brief 创建整数值
 * @param val 整数值
 * @return VDBFilterValue
 */
VDBFilterValue *filter_value_create_int(int64_t val);

/**
 * @brief 创建浮点值
 * @param val 浮点值
 * @return VDBFilterValue
 */
VDBFilterValue *filter_value_create_float(double val);

/**
 * @brief 创建布尔值
 * @param val 布尔值
 * @return VDBFilterValue
 */
VDBFilterValue *filter_value_create_bool(bool val);

/**
 * @brief 释放过滤值
 * @param value 过滤值
 */
void filter_value_free(VDBFilterValue *value);

/* ========================================================================
 * 便捷宏
 * ======================================================================== */

/** 检查表达式是否有效 */
#define FILTER_AST_IS_VALID(ast) ((ast) != NULL && (ast)->type != FILTER_NODE_INVALID)

/** 获取比较表达式的字段名 */
#define FILTER_COMPARISON_FIELD(comp) ((comp)->comparison.field_name)

/** 获取比较表达式的值 */
#define FILTER_COMPARISON_VALUE(comp) (&(comp)->comparison.value)

#ifdef __cplusplus
}
#endif

#endif /* DB_QUERY_FILTER_H */
