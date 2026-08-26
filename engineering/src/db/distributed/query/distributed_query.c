/**
 * @file distributed_query.c
 * @brief 分布式查询执行实现
 *
 * 实现跨节点分布式查询的解析、规划、并行执行和结果合并。
 */
#include "db/distributed/distributed_query.h"
#include "db/core/log.h"
#include "db/sharding/sharding.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* ========================================================================
 * 内部数据结构
 * ======================================================================== */

/**
 * @brief 查询片段执行上下文 (内部)
 */
typedef struct fragment_exec_ctx_s {
    query_fragment_t *fragment;      /**< 片段 */
    query_result_fragment_t result;  /**< 结果 */
    bool completed;                  /**< 是否完成 */
} fragment_exec_ctx_t;

/**
 * @brief 并行执行上下文 (内部)
 */
typedef struct parallel_exec_ctx_s {
    distributed_query_ctx_t *query_ctx;     /**< 查询上下文 */
    fragment_exec_ctx_t *fragment_ctxs;     /**< 片段执行上下文数组 */
    uint32_t fragment_count;                /**< 片段数量 */
    pthread_mutex_t lock;                   /**< 互斥锁 */
    pthread_cond_t cond;                    /**< 条件变量 */
    uint32_t completed_count;               /**< 已完成片段数 */
} parallel_exec_ctx_t;

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 深拷贝字符串
 */
static char* strdup_safe(const char *s)
{
    if (s == NULL) return NULL;
    size_t len = strlen(s) + 1;
    char *copy = (char *)malloc(len);
    if (copy != NULL) {
        memcpy(copy, s, len);
    }
    return copy;
}

/* ========================================================================
 * 查询片段实现
 * ======================================================================== */

query_fragment_t* query_fragment_create(uint64_t fragment_id, const char *sql,
                                        uint64_t node_id, uint32_t stage)
{
    if (sql == NULL) {
        LOG_ERROR("创建查询片段失败: sql 为空");
        return NULL;
    }

    query_fragment_t *fragment = (query_fragment_t *)calloc(1, sizeof(query_fragment_t));
    if (fragment == NULL) {
        LOG_ERROR("查询片段内存分配失败");
        return NULL;
    }

    fragment->fragment_id = fragment_id;
    fragment->sql = strdup_safe(sql);
    if (fragment->sql == NULL) {
        LOG_ERROR("查询片段 SQL 复制失败");
        free(fragment);
        return NULL;
    }

    fragment->node_id = node_id;
    fragment->stage = stage;

    LOG_DEBUG("创建查询片段: id=%lu, node=%lu, stage=%u",
              fragment_id, node_id, stage);
    return fragment;
}

void query_fragment_destroy(query_fragment_t *fragment)
{
    if (fragment == NULL) return;
    if (fragment->sql != NULL) {
        free(fragment->sql);
    }
    free(fragment);
}

/* ========================================================================
 * 执行计划实现
 * ======================================================================== */

distributed_plan_t* distributed_plan_alloc(uint64_t coordinator_id)
{
    distributed_plan_t *plan = (distributed_plan_t *)calloc(1, sizeof(distributed_plan_t));
    if (plan == NULL) {
        LOG_ERROR("执行计划内存分配失败");
        return NULL;
    }

    plan->coordinator_id = coordinator_id;
    plan->fragment_count = 0;
    plan->fragments = NULL;

    return plan;
}

void distributed_plan_destroy(distributed_plan_t *plan)
{
    if (plan == NULL) return;

    if (plan->fragments != NULL) {
        for (uint32_t i = 0; i < plan->fragment_count; i++) {
            query_fragment_destroy(&plan->fragments[i]);
        }
        free(plan->fragments);
    }
    free(plan);
}

int distributed_plan_add_fragment(distributed_plan_t *plan, query_fragment_t *fragment)
{
    if (plan == NULL || fragment == NULL) {
        LOG_ERROR("添加片段失败: 参数为空");
        return -1;
    }

    if (plan->fragment_count >= DQ_MAX_FRAGMENTS) {
        LOG_ERROR("片段数量超过上限: %u >= %d", plan->fragment_count, DQ_MAX_FRAGMENTS);
        return -1;
    }

    /* 扩展数组 */
    uint32_t new_count = plan->fragment_count + 1;
    query_fragment_t *new_fragments = (query_fragment_t *)realloc(
        plan->fragments, new_count * sizeof(query_fragment_t));

    if (new_fragments == NULL) {
        LOG_ERROR("片段数组扩展失败");
        return -1;
    }

    plan->fragments = new_fragments;

    /* 复制片段 */
    memcpy(&plan->fragments[plan->fragment_count], fragment, sizeof(query_fragment_t));
    /* 转移 SQL 所有权 */
    fragment->sql = NULL;

    plan->fragment_count = new_count;

    LOG_DEBUG("添加片段到执行计划: fragment_id=%lu, 总数=%u",
              fragment->fragment_id, plan->fragment_count);
    return 0;
}

/* ========================================================================
 * 分布式查询上下文实现
 * ======================================================================== */

distributed_query_ctx_t* distributed_query_create(uint64_t query_id, const char *sql)
{
    if (sql == NULL) {
        LOG_ERROR("创建分布式查询失败: sql 为空");
        return NULL;
    }

    distributed_query_ctx_t *ctx = (distributed_query_ctx_t *)calloc(
        1, sizeof(distributed_query_ctx_t));
    if (ctx == NULL) {
        LOG_ERROR("分布式查询上下文分配失败");
        return NULL;
    }

    ctx->query_id = query_id;
    ctx->state = QUERY_PENDING;
    ctx->timeout_ms = DQ_DEFAULT_TIMEOUT_MS;
    ctx->results = NULL;
    ctx->result_count = 0;

    LOG_INFO("创建分布式查询: query_id=%lu", query_id);
    return ctx;
}

void distributed_query_free(distributed_query_ctx_t *ctx)
{
    if (ctx == NULL) return;

    if (ctx->plan != NULL) {
        distributed_plan_destroy(ctx->plan);
    }

    if (ctx->error_message != NULL) {
        free(ctx->error_message);
    }

    if (ctx->results != NULL) {
        for (uint32_t i = 0; i < ctx->result_count; i++) {
            if (ctx->results[i].data != NULL) {
                free(ctx->results[i].data);
            }
            if (ctx->results[i].error_message != NULL) {
                free(ctx->results[i].error_message);
            }
        }
        free(ctx->results);
    }

    free(ctx);
}

int distributed_plan_create(distributed_query_ctx_t *ctx, const distributed_plan_t *plan)
{
    if (ctx == NULL || plan == NULL) {
        LOG_ERROR("创建执行计划失败: 参数为空");
        return -1;
    }

    if (ctx->plan != NULL) {
        LOG_WARN("覆盖已有执行计划: query_id=%lu", ctx->query_id);
        distributed_plan_destroy(ctx->plan);
        ctx->plan = NULL;
    }

    /* 复制计划 */
    ctx->plan = distributed_plan_alloc(plan->coordinator_id);
    if (ctx->plan == NULL) {
        LOG_ERROR("执行计划分配失败");
        return -1;
    }

    /* 复制所有片段 */
    for (uint32_t i = 0; i < plan->fragment_count; i++) {
        query_fragment_t *copy = query_fragment_create(
            plan->fragments[i].fragment_id,
            plan->fragments[i].sql,
            plan->fragments[i].node_id,
            plan->fragments[i].stage);
        if (copy == NULL) {
            LOG_ERROR("片段复制失败");
            distributed_plan_destroy(ctx->plan);
            ctx->plan = NULL;
            return -1;
        }
        if (distributed_plan_add_fragment(ctx->plan, copy) != 0) {
            query_fragment_destroy(copy);
            distributed_plan_destroy(ctx->plan);
            ctx->plan = NULL;
            return -1;
        }
    }

    LOG_INFO("设置执行计划: query_id=%lu, fragment_count=%u",
             ctx->query_id, plan->fragment_count);
    return 0;
}

/* ========================================================================
 * 片段执行 (模拟)
 * ======================================================================== */

/**
 * @brief 模拟执行单个片段
 *
 * 在实际分布式系统中，这里会发送 RPC 到目标节点。
 * 当前实现为模拟执行，用于框架验证。
 */
static int execute_fragment_simulation(query_fragment_t *fragment,
                                       query_result_fragment_t *result)
{
    if (fragment == NULL || result == NULL) {
        return -1;
    }

    LOG_DEBUG("模拟执行片段: fragment_id=%lu, node=%lu, sql=%s",
              fragment->fragment_id, fragment->node_id, fragment->sql);

    /* 模拟结果 */
    result->node_id = fragment->node_id;
    result->fragment_id = fragment->fragment_id;
    result->error_code = 0;
    result->error_message = NULL;

    /* 模拟返回一些数据 */
    const char *mock_data = "{\"rows\": 0}";
    size_t data_len = strlen(mock_data) + 1;
    result->data = malloc(data_len);
    if (result->data == NULL) {
        result->error_code = -1;
        result->error_message = strdup_safe("内存分配失败");
        return -1;
    }
    memcpy(result->data, mock_data, data_len);
    result->data_size = data_len;

    return 0;
}

/* ========================================================================
 * 并行执行
 * ======================================================================== */

/**
 * @brief 片段执行线程函数
 */
static void* fragment_thread_func(void *arg)
{
    parallel_exec_ctx_t *pctx = (parallel_exec_ctx_t *)arg;

    /* 找到一个未完成的片段 */
    query_fragment_t *fragment = NULL;
    query_result_fragment_t *result = NULL;

    pthread_mutex_lock(&pctx->lock);
    for (uint32_t i = 0; i < pctx->fragment_count; i++) {
        if (!pctx->fragment_ctxs[i].completed) {
            fragment = pctx->fragment_ctxs[i].fragment;
            result = &pctx->fragment_ctxs[i].result;
            break;
        }
    }
    pthread_mutex_unlock(&pctx->lock);

    if (fragment == NULL) {
        return NULL;
    }

    /* 执行片段 */
    int rc = execute_fragment_simulation(fragment, result);

    /* 标记完成 */
    pthread_mutex_lock(&pctx->lock);
    pctx->fragment_ctxs[fragment->fragment_id % pctx->fragment_count].completed = true;
    pctx->completed_count++;
    pthread_cond_broadcast(&pctx->cond);
    pthread_mutex_unlock(&pctx->lock);

    return NULL;
}

int distributed_execute(distributed_query_ctx_t *ctx)
{
    if (ctx == NULL || ctx->plan == NULL) {
        LOG_ERROR("执行失败: 上下文或执行计划为空");
        return -1;
    }

    if (ctx->state == QUERY_RUNNING) {
        LOG_WARN("查询正在执行中: query_id=%lu", ctx->query_id);
        return -1;
    }

    ctx->state = QUERY_RUNNING;
    LOG_INFO("开始执行分布式查询: query_id=%lu, fragments=%u",
             ctx->query_id, ctx->plan->fragment_count);

    if (ctx->plan->fragment_count == 0) {
        ctx->state = QUERY_COMPLETED;
        LOG_INFO("无片段需要执行: query_id=%lu", ctx->query_id);
        return 0;
    }

    /* 初始化并行执行上下文 */
    parallel_exec_ctx_t pctx;
    memset(&pctx, 0, sizeof(pctx));
    pctx.query_ctx = ctx;
    pctx.fragment_count = ctx->plan->fragment_count;
    pctx.completed_count = 0;
    pthread_mutex_init(&pctx.lock, NULL);
    pthread_cond_init(&pctx.cond, NULL);

    /* 分配片段执行上下文 */
    pctx.fragment_ctxs = (fragment_exec_ctx_t *)calloc(
        pctx.fragment_count, sizeof(fragment_exec_ctx_t));
    if (pctx.fragment_ctxs == NULL) {
        LOG_ERROR("片段执行上下文分配失败");
        ctx->state = QUERY_FAILED;
        pthread_mutex_destroy(&pctx.lock);
        pthread_cond_destroy(&pctx.cond);
        return -1;
    }

    /* 初始化每个片段的上下文 */
    for (uint32_t i = 0; i < pctx.fragment_count; i++) {
        pctx.fragment_ctxs[i].fragment = &ctx->plan->fragments[i];
        pctx.fragment_ctxs[i].completed = false;
        memset(&pctx.fragment_ctxs[i].result, 0, sizeof(query_result_fragment_t));
    }

    /* 创建线程执行片段 (简单并行: 每个片段一个线程) */
    uint32_t thread_count = pctx.fragment_count;
    if (thread_count > 32) {
        thread_count = 32;  /* 限制最大线程数 */
    }

    pthread_t *threads = (pthread_t *)calloc(thread_count, sizeof(pthread_t));
    if (threads == NULL) {
        LOG_ERROR("线程数组分配失败");
        free(pctx.fragment_ctxs);
        ctx->state = QUERY_FAILED;
        pthread_mutex_destroy(&pctx.lock);
        pthread_cond_destroy(&pctx.cond);
        return -1;
    }

    /* 启动线程 */
    for (uint32_t i = 0; i < thread_count; i++) {
        int rc = pthread_create(&threads[i], NULL, fragment_thread_func, &pctx);
        if (rc != 0) {
            LOG_ERROR("线程创建失败: %u", i);
            /* 等待已启动的线程完成 */
            for (uint32_t j = 0; j < i; j++) {
                pthread_join(threads[j], NULL);
            }
            free(threads);
            free(pctx.fragment_ctxs);
            ctx->state = QUERY_FAILED;
            pthread_mutex_destroy(&pctx.lock);
            pthread_cond_destroy(&pctx.cond);
            return -1;
        }
    }

    /* 等待所有线程完成 */
    for (uint32_t i = 0; i < thread_count; i++) {
        pthread_join(threads[i], NULL);
    }

    /* 收集结果到查询上下文 */
    ctx->result_count = pctx.fragment_count;
    ctx->results = (query_result_fragment_t *)calloc(
        ctx->result_count, sizeof(query_result_fragment_t));
    if (ctx->results == NULL) {
        LOG_ERROR("结果数组分配失败");
        free(pctx.fragment_ctxs);
        free(threads);
        ctx->state = QUERY_FAILED;
        pthread_mutex_destroy(&pctx.lock);
        pthread_cond_destroy(&pctx.cond);
        return -1;
    }

    /* 复制结果 */
    for (uint32_t i = 0; i < ctx->result_count; i++) {
        ctx->results[i] = pctx.fragment_ctxs[i].result;
        pctx.fragment_ctxs[i].result.data = NULL;  /* 转移所有权 */
        ctx->results[i].error_message = NULL;
    }

    /* 清理 */
    free(pctx.fragment_ctxs);
    free(threads);
    pthread_mutex_destroy(&pctx.lock);
    pthread_cond_destroy(&pctx.cond);

    ctx->state = QUERY_COMPLETED;
    LOG_INFO("分布式查询执行完成: query_id=%lu, results=%u",
             ctx->query_id, ctx->result_count);
    return 0;
}

/* ========================================================================
 * 结果收集
 * ======================================================================== */

int distributed_collect_results(distributed_query_ctx_t *ctx)
{
    if (ctx == NULL) {
        LOG_ERROR("收集结果失败: 上下文为空");
        return -1;
    }

    if (ctx->state != QUERY_COMPLETED) {
        LOG_ERROR("收集结果失败: 查询未完成, state=%d", ctx->state);
        return -1;
    }

    /* 检查是否有失败的片段 */
    uint32_t failed_count = 0;
    for (uint32_t i = 0; i < ctx->result_count; i++) {
        if (ctx->results[i].error_code != 0) {
            failed_count++;
            LOG_WARN("片段执行失败: fragment_id=%lu, node=%lu, error=%d",
                     ctx->results[i].fragment_id,
                     ctx->results[i].node_id,
                     ctx->results[i].error_code);
        }
    }

    if (failed_count > 0) {
        LOG_WARN("部分片段执行失败: %u/%u", failed_count, ctx->result_count);
        /* 不设置失败状态，允许部分结果返回 */
    }

    LOG_INFO("收集结果完成: query_id=%lu, total=%u, failed=%u",
             ctx->query_id, ctx->result_count, failed_count);
    return 0;
}

/* ========================================================================
 * 状态查询
 * ======================================================================== */

query_state_t distributed_query_get_state(const distributed_query_ctx_t *ctx)
{
    if (ctx == NULL) return QUERY_FAILED;
    return ctx->state;
}

const char* distributed_query_get_error(const distributed_query_ctx_t *ctx)
{
    if (ctx == NULL) return NULL;
    return ctx->error_message;
}

void distributed_query_set_timeout(distributed_query_ctx_t *ctx, uint64_t timeout_ms)
{
    if (ctx == NULL) return;
    ctx->timeout_ms = timeout_ms;
    LOG_DEBUG("设置查询超时: query_id=%lu, timeout=%lums", ctx->query_id, timeout_ms);
}
