/**
 * @file distributed_query.h
 * @brief 分布式查询执行接口
 *
 * 提供跨节点分布式查询的执行框架，支持查询片段化、并行执行和结果合并。
 * 依赖分片路由和 Raft 共识模块实现跨节点协调。
 */
#ifndef DB_DISTRIBUTED_QUERY_H
#define DB_DISTRIBUTED_QUERY_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 查询片段最大数量 */
#define DQ_MAX_FRAGMENTS 256

/** 查询结果最大大小 (10MB) */
#define DQ_MAX_RESULT_SIZE (10 * 1024 * 1024)

/** 默认查询超时时间 (30s) */
#define DQ_DEFAULT_TIMEOUT_MS 30000

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 查询片段
 *
 * 表示分布式查询在单个节点上执行的任务单元
 */
typedef struct query_fragment_s {
    uint64_t fragment_id;      /**< 片段唯一 ID */
    char *sql;                 /**< 片段 SQL 语句 (持有所有权) */
    uint64_t node_id;          /**< 目标节点 ID */
    uint32_t stage;            /**< 执行阶段 (用于流水线并行) */
} query_fragment_t;

/**
 * @brief 执行计划
 *
 * 描述查询的分布式执行策略
 */
typedef struct distributed_plan_s {
    query_fragment_t *fragments;   /**< 片段数组 */
    uint32_t fragment_count;       /**< 片段数量 */
    uint64_t coordinator_id;       /**< 协调节点 ID (当前节点) */
} distributed_plan_t;

/**
 * @brief 查询状态枚举
 */
typedef enum {
    QUERY_PENDING = 0,     /**< 等待执行 */
    QUERY_RUNNING,         /**< 执行中 */
    QUERY_COMPLETED,       /**< 执行完成 */
    QUERY_FAILED           /**< 执行失败 */
} query_state_t;

/**
 * @brief 查询结果片段
 *
 * 单个节点返回的查询结果
 */
typedef struct query_result_fragment_s {
    uint64_t node_id;           /**< 返回结果的节点 ID */
    uint64_t fragment_id;       /**< 对应的片段 ID */
    void *data;                 /**< 结果数据 (持有所有权) */
    uint64_t data_size;         /**< 结果数据大小 */
    int error_code;             /**< 错误码 (0 表示成功) */
    char *error_message;        /**< 错误信息 (可选) */
} query_result_fragment_t;

/**
 * @brief 查询上下文
 *
 * 维护分布式查询的完整状态
 */
typedef struct distributed_query_ctx_s {
    uint64_t query_id;                      /**< 查询唯一 ID */
    distributed_plan_t *plan;               /**< 执行计划 */
    query_state_t state;                    /**< 当前状态 */
    char *error_message;                    /**< 错误信息 (持有所有权) */
    query_result_fragment_t *results;       /**< 结果片段数组 */
    uint32_t result_count;                  /**< 已收集的结果数量 */
    uint64_t timeout_ms;                    /**< 超时时间 */
    void *priv_data;                        /**< 私有数据 (可选) */
} distributed_query_ctx_t;

/* ========================================================================
 * API 声明
 * ======================================================================== */

/**
 * @brief 创建分布式查询上下文
 *
 * @param query_id 查询 ID
 * @param sql 原始 SQL 语句 (会被复制)
 * @return 成功返回上下文指针，失败返回 NULL
 */
distributed_query_ctx_t* distributed_query_create(uint64_t query_id, const char *sql);

/**
 * @brief 销毁分布式查询上下文
 *
 * @param ctx 查询上下文
 */
void distributed_query_free(distributed_query_ctx_t *ctx);

/**
 * @brief 设置执行计划
 *
 * @param ctx 查询上下文
 * @param plan 执行计划 (内容会被复制)
 * @return 0 成功，-1 失败
 */
int distributed_plan_create(distributed_query_ctx_t *ctx, const distributed_plan_t *plan);

/**
 * @brief 执行分布式查询
 *
 * 并行向各节点发送查询片段并等待结果
 *
 * @param ctx 查询上下文
 * @return 0 成功，-1 失败
 */
int distributed_execute(distributed_query_ctx_t *ctx);

/**
 * @brief 收集查询结果
 *
 * 合并各节点返回的结果片段
 *
 * @param ctx 查询上下文
 * @return 0 成功，-1 失败
 */
int distributed_collect_results(distributed_query_ctx_t *ctx);

/**
 * @brief 获取查询状态
 *
 * @param ctx 查询上下文
 * @return 当前状态
 */
query_state_t distributed_query_get_state(const distributed_query_ctx_t *ctx);

/**
 * @brief 获取错误信息
 *
 * @param ctx 查询上下文
 * @return 错误信息字符串，无错误返回 NULL
 */
const char* distributed_query_get_error(const distributed_query_ctx_t *ctx);

/**
 * @brief 设置查询超时时间
 *
 * @param ctx 查询上下文
 * @param timeout_ms 超时时间 (毫秒)
 */
void distributed_query_set_timeout(distributed_query_ctx_t *ctx, uint64_t timeout_ms);

/**
 * @brief 创建查询片段
 *
 * @param fragment_id 片段 ID
 * @param sql SQL 语句
 * @param node_id 目标节点
 * @param stage 执行阶段
 * @return 成功返回片段指针，失败返回 NULL
 */
query_fragment_t* query_fragment_create(uint64_t fragment_id, const char *sql,
                                        uint64_t node_id, uint32_t stage);

/**
 * @brief 销毁查询片段
 *
 * @param fragment 片段
 */
void query_fragment_destroy(query_fragment_t *fragment);

/**
 * @brief 创建执行计划
 *
 * @param coordinator_id 协调节点 ID
 * @return 成功返回计划指针，失败返回 NULL
 */
distributed_plan_t* distributed_plan_alloc(uint64_t coordinator_id);

/**
 * @brief 销毁执行计划
 *
 * @param plan 执行计划
 */
void distributed_plan_destroy(distributed_plan_t *plan);

/**
 * @brief 添加片段到执行计划
 *
 * @param plan 执行计划
 * @param fragment 片段 (所有权转移)
 * @return 0 成功，-1 失败
 */
int distributed_plan_add_fragment(distributed_plan_t *plan, query_fragment_t *fragment);

#ifdef __cplusplus
}
#endif

#endif /* DB_DISTRIBUTED_QUERY_H */
