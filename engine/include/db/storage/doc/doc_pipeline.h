/**
 * @file doc_pipeline.h
 * @brief 文档聚合管道头文件
 *
 * 实现 MongoDB 风格的聚合管道，支持 $match/$group/$sort/$limit/$skip 等操作符。
 *
 * 管道执行流程:
 *   文档输入 -> $match(过滤) -> $group(分组) -> $sort(排序) -> $limit/$skip(分页) -> 输出
 */
#ifndef DB_DOC_PIPELINE_H
#define DB_DOC_PIPELINE_H

#include "db/storage/doc/doc_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 管道阶段类型
 * ======================================================================== */

/**
 * @brief 管道阶段类型
 */
typedef enum DocPipelineStageType_e {
    DOC_STAGE_MATCH = 0,      /**< $match: 文档过滤 */
    DOC_STAGE_GROUP,           /**< $group: 文档分组 */
    DOC_STAGE_SORT,            /**< $sort: 排序 */
    DOC_STAGE_LIMIT,           /**< $limit: 限制数量 */
    DOC_STAGE_SKIP,            /**< $skip: 跳过数量 */
    DOC_STAGE_PROJECT,         /**< $project: 字段投影 */
    DOC_STAGE_UNWIND,          /**< $unwind: 数组展开 */
    DOC_STAGE_COUNT,           /**< $count: 计数 */
    DOC_STAGE_MAX,             /**< $max: 取最大值 */
    DOC_STAGE_MIN,             /**< $min: 取最小值 */
    DOC_STAGE_SUM,             /**< $sum: 求和 */
    DOC_STAGE_AVG,             /**< $avg: 求平均 */
} DocPipelineStageType;

/* ========================================================================
 * 表达式类型
 * ======================================================================== */

/**
 * @brief 表达式操作符
 */
typedef enum DocExprOp_e {
    DOC_EXPR_FIELD = 0,        /**< 字段访问 */
    DOC_EXPR_CONST,            /**< 常量值 */
    DOC_EXPR_EQ,               /**< 等于 == */
    DOC_EXPR_NE,               /**< 不等于 != */
    DOC_EXPR_LT,               /**< 小于 < */
    DOC_EXPR_LE,               /**< 小于等于 <= */
    DOC_EXPR_GT,               /**< 大于 > */
    DOC_EXPR_GE,               /**< 大于等于 >= */
    DOC_EXPR_AND,              /**< 逻辑与 && */
    DOC_EXPR_OR,               /**< 逻辑或 || */
    DOC_EXPR_NOT,              /**< 逻辑非 ! */
    DOC_EXPR_IN,               /**< 包含于 in */
    DOC_EXPR_NIN,              /**< 不包含于 nin */
    DOC_EXPR_REGEX,            /**< 正则匹配 */
    DOC_EXPR_EXISTS,           /**< 字段存在 */
    DOC_EXPR_ADD,              /**< 加法 + */
    DOC_EXPR_SUB,              /**< 减法 - */
    DOC_EXPR_MUL,              /**< 乘法 * */
    DOC_EXPR_DIV,              /**< 除法 / */
    DOC_EXPR_MOD,              /**< 取模 % */
} DocExprOp;

/**
 * @brief 表达式类型
 */
typedef enum DocExprType_e {
    DOC_EXPR_TYPE_BOOL = 0,    /**< 布尔值 */
    DOC_EXPR_TYPE_INT,         /**< 整数 */
    DOC_EXPR_TYPE_DOUBLE,      /**< 浮点数 */
    DOC_EXPR_TYPE_STRING,      /**< 字符串 */
    DOC_EXPR_TYPE_ARRAY,       /**< 数组 */
    DOC_EXPR_TYPE_NULL,        /**< 空值 */
} DocExprType;

/* ========================================================================
 * 表达式结构
 * ======================================================================== */

/**
 * @brief 表达式节点
 */
typedef struct DocExpr_s {
    DocExprOp op;              /**< 操作符 */
    DocExprType type;          /**< 结果类型 */
    union {
        char *field_name;      /**< 字段名 (DOC_EXPR_FIELD) */
        char *str_value;       /**< 字符串值 (DOC_EXPR_CONST) */
        double num_value;      /**< 数值 (DOC_EXPR_CONST) */
        int64_t int_value;     /**< 整数值 (DOC_EXPR_CONST) */
        bool bool_value;       /**< 布尔值 (DOC_EXPR_CONST) */
    };
    struct DocExpr_s **args;   /**< 子表达式数组 */
    size_t num_args;           /**< 子表达式数量 */
} DocExpr;

/**
 * @brief 表达式求值上下文
 */
typedef struct DocExprContext_s {
    const char *doc_json;      /**< 当前文档 JSON */
    size_t doc_len;            /**< 文档长度 */
    void *json_parser;         /**< JSON 解析器状态 */
} DocExprContext;

/**
 * @brief 创建字段表达式
 */
DocExpr *doc_expr_create_field(const char *field_name);

/**
 * @brief 创建常量表达式
 */
DocExpr *doc_expr_create_const_int(int64_t value);
DocExpr *doc_expr_create_const_double(double value);
DocExpr *doc_expr_create_const_string(const char *value);
DocExpr *doc_expr_create_const_bool(bool value);

/**
 * @brief 创建二元表达式
 */
DocExpr *doc_expr_create_binary(DocExprOp op, DocExpr *left, DocExpr *right);

/**
 * @brief 创建一元表达式
 */
DocExpr *doc_expr_create_unary(DocExprOp op, DocExpr *arg);

/**
 * @brief 释放表达式
 */
void doc_expr_free(DocExpr *expr);

/**
 * @brief 复制表达式
 */
DocExpr *doc_expr_clone(const DocExpr *expr);

/**
 * @brief 求值表达式
 * @param expr 表达式
 * @param ctx 求值上下文
 * @param result 输出结果
 * @return 0 成功，-1 失败
 */
int doc_expr_evaluate(const DocExpr *expr, const DocExprContext *ctx, void *result);

/* ========================================================================
 * $match 阶段
 * ======================================================================== */

/**
 * @brief $match 阶段
 */
typedef struct DocMatchStage_s {
    DocExpr *filter;           /**< 过滤表达式 */
    bool use_index;            /**< 是否使用索引 */
} DocMatchStage;

/**
 * @brief 创建 $match 阶段
 */
DocMatchStage *doc_match_stage_create(const DocExpr *filter);

/**
 * @brief 释放 $match 阶段
 */
void doc_match_stage_free(DocMatchStage *stage);

/* ========================================================================
 * $group 阶段
 * ======================================================================== */

/**
 * @brief 分组累加器类型
 */
typedef enum DocGroupAccumulator_e {
    DOC_ACC_SUM = 0,           /**< 求和 */
    DOC_ACC_AVG,               /**< 平均 */
    DOC_ACC_MIN,               /**< 最小值 */
    DOC_ACC_MAX,               /**< 最大值 */
    DOC_ACC_FIRST,             /**< 第一个值 */
    DOC_ACC_LAST,              /**< 最后一个值 */
    DOC_ACC_PUSH,              /**< 推入数组 */
    DOC_ACC_ADD_TO_SET,        /**< 添加到集合 */
    DOC_ACC_COUNT,             /**< 计数 */
} DocGroupAccumulator;

/**
 * @brief 分组累加器定义
 */
typedef struct DocGroupAccumulatorDef_s {
    char name[64];             /**< 累加器名称 */
    DocGroupAccumulator type;   /**< 累加器类型 */
    DocExpr *expr;             /**< 表达式 */
} DocGroupAccumulatorDef;

/**
 * @brief $group 阶段
 */
typedef struct DocGroupStage_s {
    DocExpr *group_id;         /**< 分组 ID 表达式 */
    char group_id_field[64];    /**< 分组 ID 字段名 */
    DocGroupAccumulatorDef *accumulators; /**< 累加器数组 */
    size_t num_accumulators;   /**< 累加器数量 */
} DocGroupStage;

/**
 * @brief 创建 $group 阶段
 */
DocGroupStage *doc_group_stage_create(const DocExpr *group_id, const char *group_id_field);

/**
 * @brief 添加累加器
 */
int doc_group_stage_add_accumulator(DocGroupStage *stage,
                                    const char *name,
                                    DocGroupAccumulator type,
                                    const DocExpr *expr);

/**
 * @brief 释放 $group 阶段
 */
void doc_group_stage_free(DocGroupStage *stage);

/* ========================================================================
 * $sort 阶段
 * ======================================================================== */

/**
 * @brief 排序字段
 */
typedef struct DocSortField_s {
    char field[64];            /**< 字段名 */
    int direction;             /**< 排序方向: 1 升序, -1 降序 */
} DocSortField;

/**
 * @brief $sort 阶段
 */
typedef struct DocSortStage_s {
    DocSortField *fields;      /**< 排序字段数组 */
    size_t num_fields;         /**< 排序字段数量 */
} DocSortStage;

/**
 * @brief 创建 $sort 阶段
 */
DocSortStage *doc_sort_stage_create(const DocSortField *fields, size_t num_fields);

/**
 * @brief 添加排序字段
 */
int doc_sort_stage_add_field(DocSortStage *stage, const char *field, int direction);

/**
 * @brief 释放 $sort 阶段
 */
void doc_sort_stage_free(DocSortStage *stage);

/* ========================================================================
 * $limit 和 $skip 阶段
 * ======================================================================== */

/**
 * @brief $limit 阶段
 */
typedef struct DocLimitStage_s {
    uint64_t limit;            /**< 限制数量 */
} DocLimitStage;

/**
 * @brief 创建 $limit 阶段
 */
DocLimitStage *doc_limit_stage_create(uint64_t limit);

/**
 * @brief 释放 $limit 阶段
 */
void doc_limit_stage_free(DocLimitStage *stage);

/**
 * @brief $skip 阶段
 */
typedef struct DocSkipStage_s {
    uint64_t skip;             /**< 跳过数量 */
} DocSkipStage;

/**
 * @brief 创建 $skip 阶段
 */
DocSkipStage *doc_skip_stage_create(uint64_t skip);

/**
 * @brief 释放 $skip 阶段
 */
void doc_skip_stage_free(DocSkipStage *stage);

/* ========================================================================
 * $project 阶段
 * ======================================================================== */

/**
 * @brief 投影字段
 */
typedef struct DocProjectField_s {
    char name[64];             /**< 输出字段名 */
    DocExpr *expr;             /**< 表达式 */
    bool include;              /**< 是否包含（false 表示排除） */
} DocProjectField;

/**
 * @brief $project 阶段
 */
typedef struct DocProjectStage_s {
    DocProjectField *fields;   /**< 投影字段数组 */
    size_t num_fields;         /**< 投影字段数量 */
    bool exclude_id;           /**< 是否排除 _id */
} DocProjectStage;

/**
 * @brief 创建 $project 阶段
 */
DocProjectStage *doc_project_stage_create(const DocProjectField *fields, size_t num_fields);

/**
 * @brief 释放 $project 阶段
 */
void doc_project_stage_free(DocProjectStage *stage);

/* ========================================================================
 * 管道阶段通用结构
 * ======================================================================== */

/**
 * @brief 管道阶段
 */
typedef struct DocPipelineStage_s {
    DocPipelineStageType type; /**< 阶段类型 */
    char name[32];             /**< 阶段名称 */
    union {
        DocMatchStage match;    /**< $match */
        DocGroupStage group;    /**< $group */
        DocSortStage sort;      /**< $sort */
        DocLimitStage limit;    /**< $limit */
        DocSkipStage skip;      /**< $skip */
        DocProjectStage project;/**< $project */
    };
    struct DocPipelineStage_s *next; /**< 下一阶段 */
} DocPipelineStage;

/**
 * @brief 创建管道阶段
 */
DocPipelineStage *doc_pipeline_stage_create(DocPipelineStageType type);

/**
 * @brief 释放管道阶段
 */
void doc_pipeline_stage_free(DocPipelineStage *stage);

/* ========================================================================
 * 聚合管道
 * ======================================================================== */

/**
 * @brief 聚合管道
 */
typedef struct DocPipeline_s {
    DocPipelineStage *head;    /**< 管道头 */
    DocPipelineStage *tail;    /**< 管道尾 */
    size_t num_stages;         /**< 阶段数量 */
    void *mem_pool;            /**< 内存池 */
} DocPipeline;

/**
 * @brief 聚合管道配置
 */
typedef struct DocPipelineConfig_s {
    bool allow_disk_use;       /**< 允许使用磁盘 */
    uint64_t batch_size;       /**< 批处理大小 */
    uint64_t max_time_ms;      /**< 最大执行时间（毫秒） */
    bool bypass_document_validation; /**< 绕过文档验证 */
} DocPipelineConfig;

/**
 * @brief 创建聚合管道
 */
DocPipeline *doc_pipeline_create(const DocPipelineConfig *config);

/**
 * @brief 添加管道阶段
 */
int doc_pipeline_add_stage(DocPipeline *pipeline, DocPipelineStage *stage);

/**
 * @brief 添加 $match 阶段
 */
int doc_pipeline_add_match(DocPipeline *pipeline, const DocExpr *filter);

/**
 * @brief 添加 $group 阶段
 */
int doc_pipeline_add_group(DocPipeline *pipeline,
                           const DocExpr *group_id,
                           const char *group_id_field);

/**
 * @brief 添加 $sort 阶段
 */
int doc_pipeline_add_sort(DocPipeline *pipeline,
                          const DocSortField *fields,
                          size_t num_fields);

/**
 * @brief 添加 $limit 阶段
 */
int doc_pipeline_add_limit(DocPipeline *pipeline, uint64_t limit);

/**
 * @brief 添加 $skip 阶段
 */
int doc_pipeline_add_skip(DocPipeline *pipeline, uint64_t skip);

/**
 * @brief 添加 $project 阶段
 */
int doc_pipeline_add_project(DocPipeline *pipeline,
                             const DocProjectField *fields,
                             size_t num_fields);

/**
 * @brief 释放聚合管道
 */
void doc_pipeline_free(DocPipeline *pipeline);

/* ========================================================================
 * 管道执行器
 * ======================================================================== */

/**
 * @brief 管道执行器
 */
typedef struct DocPipelineExecutor_s {
    DocPipeline *pipeline;     /**< 聚合管道 */
    void *input_docs;          /**< 输入文档 */
    size_t num_input_docs;     /**< 输入文档数量 */
    void *output_docs;         /**< 输出文档 */
    size_t num_output_docs;    /**< 输出文档数量 */
    size_t output_capacity;   /**< 输出容量 */
    void *mem_pool;            /**< 内存池 */
} DocPipelineExecutor;

/**
 * @brief 创建管道执行器
 */
DocPipelineExecutor *doc_pipeline_executor_create(DocPipeline *pipeline, void *mem_pool);

/**
 * @brief 执行管道
 * @param exec 执行器
 * @param docs 输入文档数组（JSON 字符串数组）
 * @param num_docs 输入文档数量
 * @param results 输出文档数组（调用者负责释放）
 * @return 输出文档数量，-1 失败
 */
int doc_pipeline_execute(DocPipelineExecutor *exec,
                         const char **docs,
                         size_t num_docs,
                         char ***results);

/**
 * @brief 释放管道执行器
 */
void doc_pipeline_executor_free(DocPipelineExecutor *exec);

/* ========================================================================
 * 便捷函数
 * ======================================================================== */

/**
 * @brief 解析管道 JSON 并创建管道
 *
 * JSON 格式:
 * [
 *   {"$match": {"status": "active", "age": {"$gte": 18}}},
 *   {"$group": {"_id": "$category", "count": {"$sum": 1}}},
 *   {"$sort": {"count": -1}},
 *   {"$limit": 10}
 * ]
 *
 * @param pipeline_json 管道 JSON
 * @return 聚合管道，NULL 失败
 */
DocPipeline *doc_pipeline_parse(const char *pipeline_json);

/**
 * @brief 管道转 JSON 字符串
 * @param pipeline 管道
 * @return JSON 字符串（调用者负责释放）
 */
char *doc_pipeline_to_json(const DocPipeline *pipeline);

#ifdef __cplusplus
}
#endif

#endif /* DB_DOC_PIPELINE_H */
