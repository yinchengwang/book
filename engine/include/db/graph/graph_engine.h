/**
 * @file graph_engine.h
 * @brief 图存储引擎头文件
 *
 * 定义图存储引擎的接口和数据结构，扩展现有图实现以支持 storage_ops_t 接口。
 */
#ifndef DB_GRAPH_ENGINE_H
#define DB_GRAPH_ENGINE_H

#include "storage_engine.h"
#include "db/graph/graph.h"
#include "db/mm_pool.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct lock_manager_s lock_manager_t;

/**
 * @brief 图引擎数据库
 */
typedef struct graph_engine_db_s {
    graph_t *graph;              /**< 图数据库句柄 */
    char name[256];              /**< 图名称 */
    char data_dir[512];          /**< 数据目录 */
    AccessMode mode;             /**< 访问模式 */

    /* 内存池相关 */
    void *mem_pool;              /**< 内存池指针 */
    bool use_mem_pool;           /**< 是否使用内存池 */

    /* CSR 存储相关 */
    void *csr_storage;           /**< CSR 存储指针 */
    bool use_csr;                /**< 是否使用 CSR 存储 */

    /* 并发控制 */
    lock_manager_t *lockmgr;     /**< 锁管理器 */
    void *rwlock;                /**< 读写锁 */
    bool use_lock;               /**< 是否启用锁 */
} graph_engine_db_t;

/**
 * @brief 获取图引擎操作表
 */
const storage_ops_t *graph_engine_get_ops(void);

/**
 * @brief 初始化图引擎
 */
int graph_engine_init(const char *data_dir);

/**
 * @brief 关闭图引擎
 */
int graph_engine_shutdown(void);

/**
 * @brief 创建图
 */
int graph_engine_create(const char *name, const storage_schema_t *schema);

/**
 * @brief 打开图
 */
void *graph_engine_open(const char *name, AccessMode mode);

/**
 * @brief 关闭图
 */
int graph_engine_close(void *rel);

/**
 * @brief 删除图
 */
int graph_engine_drop(const char *name);

/**
 * @brief 添加顶点
 */
graph_vertex_id_t graph_engine_add_vertex(void *rel,
                                          const void *data, size_t len);

/**
 * @brief 添加边
 */
graph_edge_id_t graph_engine_add_edge(void *rel,
                                      const void *data, size_t len);

/**
 * @brief 获取顶点
 */
int graph_engine_get_vertex(void *rel, const void *key, size_t key_len,
                            void **out_data, size_t *out_len);

/**
 * @brief 获取统计信息
 */
int graph_engine_stats(const char *name, storage_stats_t *stats);

/* ========================================================================
 * CSR 存储 API
 * ======================================================================== */

/**
 * @brief 启用 CSR 存储格式
 *
 * @param rel 图引擎句柄
 * @param max_vertices 最大顶点数
 * @return 0 成功，-1 失败
 */
int graph_engine_enable_csr(void *rel, uint64_t max_vertices);

/**
 * @brief 合并 COO 缓冲区到 CSR
 *
 * @param rel 图引擎句柄
 * @return 0 成功，-1 失败
 */
int graph_engine_csr_compact(void *rel);

/**
 * @brief 检查是否需要合并
 *
 * @param rel 图引擎句柄
 * @return true 需要合并
 */
bool graph_engine_csr_needs_compact(void *rel);

/**
 * @brief 获取出边（使用 CSR 格式）
 *
 * @param rel 图引擎句柄
 * @param src 源顶点 ID
 * @param out_count 输出边数量
 * @return 出边数组（内部数据，勿释放）
 */
const void *graph_engine_get_out_edges(void *rel, uint64_t src, uint32_t *out_count);

/**
 * @brief 获取入边（使用 CSR 格式）
 *
 * @param rel 图引擎句柄
 * @param dst 目标顶点 ID
 * @param out_count 输出边数量
 * @return 入边数组（内部数据，勿释放）
 */
const void *graph_engine_get_in_edges(void *rel, uint64_t dst, uint32_t *out_count);

/**
 * @brief 保存 CSR 存储到磁盘
 *
 * @param rel 图引擎句柄
 * @return 0 成功，-1 失败
 */
int graph_engine_save_csr(void *rel);

/**
 * @brief 从磁盘加载 CSR 存储
 *
 * @param rel 图引擎句柄
 * @return 0 成功，-1 失败
 */
int graph_engine_load_csr(void *rel);

/**
 * @brief 禁用 CSR 存储
 *
 * @param rel 图引擎句柄
 */
void graph_engine_disable_csr(void *rel);

/* ========================================================================
 * 内存池管理 API
 * ======================================================================== */

/**
 * @brief 启用/禁用内存池
 *
 * @param rel 图引擎句柄
 * @param use_pool true 启用内存池，false 禁用
 * @return 0 成功，-1 失败
 */
int graph_engine_enable_mem_pool(void *rel, bool use_pool);

/**
 * @brief 获取内存池统计信息
 *
 * @param rel 图引擎句柄
 * @param stats 输出统计信息
 * @return 0 成功，-1 失败
 */
int graph_engine_get_mem_pool_stats(void *rel, mm_pool_stats_t *stats);

/* ========================================================================
 * 并发锁控制 API
 * ======================================================================== */

/**
 * @brief 启用/禁用并发锁
 *
 * @param rel 图引擎句柄
 * @param use_lock true 启用锁，false 禁用
 * @return 0 成功，-1 失败
 */
int graph_engine_enable_lock(void *rel, bool use_lock);

/**
 * @brief 获取图引擎的读锁
 *
 * @param rel 图引擎句柄
 * @return 0 成功，-1 失败
 */
int graph_engine_read_lock(void *rel);

/**
 * @brief 释放图引擎的读锁
 *
 * @param rel 图引擎句柄
 */
void graph_engine_read_unlock(void *rel);

/**
 * @brief 获取图引擎的写锁
 *
 * @param rel 图引擎句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 成功，LOCK_TIMEOUT 超时，-1 失败
 */
int graph_engine_write_lock(void *rel, uint32_t timeout_ms);

/**
 * @brief 释放图引擎的写锁
 *
 * @param rel 图引擎句柄
 */
void graph_engine_write_unlock(void *rel);

#ifdef __cplusplus
}
#endif

#endif /* DB_GRAPH_ENGINE_H */
