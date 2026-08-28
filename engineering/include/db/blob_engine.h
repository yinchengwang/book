/**
 * @file blob_engine.h
 * @brief Blob 存储引擎接口（C3-1）
 */
#ifndef DB_BLOB_ENGINE_H
#define DB_BLOB_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define BLOB_MAX_CHUNK_SIZE (4 * 1024 * 1024)  /* 4MB */
#define BLOB_SHA256_SIZE    32

typedef struct blob_engine_s blob_engine_t;

/* 前向声明（避免循环依赖） */
typedef struct blob_catalog_s blob_catalog_t;

/* ========================================================================
 * 引擎生命周期
 * ======================================================================== */

blob_engine_t *blob_engine_create(const char *data_dir);
blob_engine_t *blob_engine_open(const char *data_dir);
void blob_engine_close(blob_engine_t *engine);

/* ========================================================================
 * 内部访问器（供 GC 等内部模块使用）
 * ======================================================================== */

/** 获取引擎数据目录 */
const char *blob_engine_get_data_dir(const blob_engine_t *engine);

/** 获取 Chunks 目录 */
const char *blob_engine_get_chunks_dir(const blob_engine_t *engine);

/** 获取 Manifests 目录 */
const char *blob_engine_get_manifests_dir(const blob_engine_t *engine);

/** 获取 Catalog 句柄 */
blob_catalog_t *blob_engine_get_catalog(blob_engine_t *engine);

/** 获取读者计数表（内部使用） */
void *blob_engine_get_reader_table(blob_engine_t *engine);

/* ========================================================================
 * 活动读取者保护（GC 使用）
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
void blob_engine_reader_enter(blob_engine_t *engine,
                              const uint8_t chunk_id[BLOB_SHA256_SIZE]);

/**
 * @brief 退出读取者保护
 *
 * 在 Chunk 读取完成后调用，减少该 Chunk 的读者计数。
 *
 * @param engine   Blob 引擎句柄
 * @param chunk_id Chunk ID
 */
void blob_engine_reader_exit(blob_engine_t *engine,
                             const uint8_t chunk_id[BLOB_SHA256_SIZE]);

/* ========================================================================
 * Blob 操作
 * ======================================================================== */

int blob_put(blob_engine_t *engine,
             const void *data, size_t len,
             uint8_t out_blob_id[BLOB_SHA256_SIZE]);

int blob_get(blob_engine_t *engine,
             const uint8_t blob_id[BLOB_SHA256_SIZE],
             void *out_buf, size_t buf_len, size_t *out_read);

int blob_delete(blob_engine_t *engine,
                const uint8_t blob_id[BLOB_SHA256_SIZE]);

int blob_stat(blob_engine_t *engine,
              const uint8_t blob_id[BLOB_SHA256_SIZE],
              size_t *out_len);

int blob_range_get(blob_engine_t *engine,
                   const uint8_t blob_id[BLOB_SHA256_SIZE],
                   size_t offset, size_t len,
                   void *out_buf, size_t buf_len, size_t *out_read);

/* ========================================================================
 * GC 操作
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

#endif /* DB_BLOB_ENGINE_H */
