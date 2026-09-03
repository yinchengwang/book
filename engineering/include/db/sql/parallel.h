/**
 * @file parallel.h
 * @brief 并行执行框架
 *
 * 实现并行查询执行的基础设施：
 *   - ParallelContext：协调多 worker 执行
 *   - DSM（动态共享内存）：worker 间通信
 *   - 并行 Hash 表：共享内存哈希表
 *
 * PostgreSQL 9.6+ 引入并行查询，通过 Gather 节点协调多个并行 worker
 * 执行计划子树，实现大表并行扫描和处理。
 */

#ifndef DB_SQL_PARALLEL_H
#define DB_SQL_PARALLEL_H

#include "nodes/nodetags.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 并行协调器
 * ======================================================================== */

/**
 * @brief 并行协调器
 *
 * 协调多个并行 worker 的执行状态。
 */
typedef struct ParallelCoordinator {
    NodeTag type;               /**< T_ParallelCoordinator */
    int     nworkers;           /**< 期望的 worker 数量 */
    int     nworkers_attached;  /**< 已附加的 worker 数量 */
    bool    finished;           /**< 是否完成 */
} ParallelCoordinator;

/* ========================================================================
 * 并行上下文
 * ======================================================================== */

/**
 * @brief 并行上下文
 *
 * 封装一次并行执行所需的所有状态。
 */
typedef struct ParallelContext {
    NodeTag             type;           /**< T_ParallelContext */
    int                 nworkers;       /**< 期望的 worker 数量 */
    int                 nworkers_to_launch; /**< 实际启动数量 */
    int                 nworkers_launched;  /**< 已启动数量 */
    ParallelCoordinator *coordinator;   /**< 协调器 */
    void                *dsm_handle;    /**< DSM 句柄 */
} ParallelContext;

/* ========================================================================
 * 并行工作线程状态
 * ======================================================================== */

/**
 * @brief 并行工作线程状态
 */
typedef struct ParallelWorkerState {
    int                 worker_id;      /**< worker ID */
    ParallelContext     *pcxt;          /**< 并行上下文 */
    void                *dsm_segment;   /**< DSM 段 */
} ParallelWorkerState;

/* ========================================================================
 * 并行哈希表
 * ======================================================================== */

/**
 * @brief 并行哈希表状态
 *
 * 支持多个 worker 并行构建和探测的共享内存哈希表。
 */
typedef struct ParallelHashJoinState {
    NodeTag             type;           /**< T_ParallelHashJoinState */
    void                *hashtable;     /**< 共享哈希表 */
    uint64_t            nbuckets;       /**< 桶数量 */
    uint64_t            ntuples;        /**< 元组数量 */
    int                 nparticipants;  /**< 参与者数量 */
    void                *buckets;       /**< 共享桶数组 */
    void                *chunks;        /**< 共享内存块 */
} ParallelHashJoinState;

/* ========================================================================
 * API
 * ======================================================================== */

/**
 * @brief 创建并行上下文
 *
 * @param nworkers worker 数量
 * @return 新创建的 ParallelContext；失败返回 NULL
 */
ParallelContext *CreateParallelContext(int nworkers);

/**
 * @brief 销毁并行上下文
 *
 * @param pcxt ParallelContext（可为 NULL）
 */
void DestroyParallelContext(ParallelContext *pcxt);

/**
 * @brief 重置并行执行模块
 *
 * 释放模块级 MemoryContext 中的所有分配。
 */
void ResetParallelContext(void);

/**
 * @brief 从并行上下文分配共享内存
 *
 * @param pcxt ParallelContext
 * @param size 分配大小
 * @return 分配的内存指针；失败返回 NULL
 */
void *ParallelAlloc(ParallelContext *pcxt, size_t size);

/**
 * @brief 获取 worker 视角的地址
 *
 * 简化实现：所有 worker 共享同一地址空间。
 *
 * @param pcxt ParallelContext
 * @param addr 地址
 * @param worker_id worker ID
 * @return 地址
 */
void *ParallelGetWorkerAddr(ParallelContext *pcxt, void *addr, int worker_id);

/**
 * @brief 启动并行 worker
 *
 * @param pcxt ParallelContext
 * @return 实际启动的 worker 数量
 */
int LaunchParallelWorkers(ParallelContext *pcxt);

/**
 * @brief 等待所有并行 worker 完成
 *
 * @param pcxt ParallelContext
 */
void WaitForParallelWorkers(ParallelContext *pcxt);

/**
 * @brief 标记协调器完成
 *
 * @param coordinator ParallelCoordinator
 */
void ParallelCoordinatorSetFinished(ParallelCoordinator *coordinator);

/**
 * @brief 检查协调器是否完成
 *
 * @param coordinator ParallelCoordinator
 * @return true 表示完成
 */
bool ParallelCoordinatorIsFinished(ParallelCoordinator *coordinator);

/**
 * @brief 附加到协调器
 *
 * @param coordinator ParallelCoordinator
 */
void ParallelCoordinatorAttach(ParallelCoordinator *coordinator);

/* ========================================================================
 * 并行 Hash API
 * ======================================================================== */

/**
 * @brief 创建并行哈希表
 *
 * @param nparticipants 参与者数量
 * @param nbuckets 桶数量
 * @return 新创建的 ParallelHashJoinState；失败返回 NULL
 */
ParallelHashJoinState *CreateParallelHash(int nparticipants, uint64_t nbuckets);

/**
 * @brief 销毁并行哈希表
 *
 * @param state ParallelHashJoinState（可为 NULL）
 */
void DestroyParallelHash(ParallelHashJoinState *state);

/**
 * @brief 并行哈希表插入
 *
 * @param state ParallelHashJoinState
 * @param key 键
 * @param value 值
 */
void ParallelHashInsert(ParallelHashJoinState *state, void *key, void *value);

/**
 * @brief 并行哈希表查找
 *
 * @param state ParallelHashJoinState
 * @param key 键
 * @return 找到的值；未找到返回 NULL
 */
void *ParallelHashLookup(ParallelHashJoinState *state, void *key);

#ifdef __cplusplus
}
#endif

#endif /* DB_SQL_PARALLEL_H */