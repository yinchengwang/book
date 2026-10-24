/**
 * @file blob_gc.h
 * @brief Blob GC 与引用计数管理接口（Task 7）
 *
 * 定义活动读取者保护和延迟 GC 清理接口。
 */
#ifndef DB_BLOB_GC_H
#define DB_BLOB_GC_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct blob_engine_s blob_engine_t;

/* ========================================================================
 * 活动读取者保护
 * ======================================================================== */

/**
 * @brief 进入读取者保护
 *
 * 在打开 Chunk 进行读取前调用，增加该 Chunk 的读者计数。
 * GC 扫描时会检查读者计数，只有计数为 0 时才会删除 Chunk。
 *
 * @param engine   Blob 引擎句柄
 * @param chunk_id Chunk ID
 */
void blob_gc_reader_enter(blob_engine_t *engine,
                          const uint8_t chunk_id[32]);

/**
 * @brief 退出读取者保护
 *
 * 在 Chunk 读取完成后调用，减少该 Chunk 的读者计数。
 *
 * @param engine   Blob 引擎句柄
 * @param chunk_id Chunk ID
 */
void blob_gc_reader_exit(blob_engine_t *engine,
                         const uint8_t chunk_id[32]);

/**
 * @brief 检查 Chunk 是否有活动读取者
 *
 * @param engine   Blob 引擎句柄
 * @param chunk_id Chunk ID
 * @return true 有活动读取者，false 无活动读取者
 */
bool blob_gc_has_active_readers(blob_engine_t *engine,
                                const uint8_t chunk_id[32]);

/* ========================================================================
 * 延迟 GC
 * ======================================================================== */

/**
 * @brief 运行 GC 扫描
 *
 * 扫描所有 refcount=0 且 gc_after 已到期的 Chunk，
 * 确认无活动读取者后原子删除。
 *
 * @param engine Blob 引擎句柄
 * @return 0 成功，负值为错误码
 */
int blob_gc_run(blob_engine_t *engine);

/**
 * @brief 获取 GC 统计信息
 *
 * @param engine          Blob 引擎句柄
 * @param out_pending_gc  输出可 GC 的 Chunk 数量
 * @return 0 成功，负值为错误码
 */
int blob_gc_stats(blob_engine_t *engine, uint64_t *out_pending_gc);

#ifdef __cplusplus
}
#endif

#endif /* DB_BLOB_GC_H */
