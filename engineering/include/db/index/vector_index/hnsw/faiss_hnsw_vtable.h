/**
 * @file faiss_hnsw_vtable.h
 * @brief 统一 faiss_hnsw 索引虚表（C4-2 T7）
 *
 * 所有索引实现（Flat / IVFFlat / HNSW 等）通过同一 vtable 接口暴露
 * build / add / search / serialize。
 */
#ifndef DB_FAISS_HNSW_VTABLE_H
#define DB_FAISS_HNSW_VTABLE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct faiss_hnsw_vtable_s {
    int (*add)(void *index, int32_t n, const float *vectors);
    int (*search)(void *index, const float *query, int32_t k,
                  float *distances, int32_t *ids);
    int (*save)(void *index, const char *path);
    int (*load)(void *index, const char *path);
    void (*destroy)(void *index);
    size_t (*ntotal)(void *index);
} faiss_hnsw_vtable_t;

const faiss_hnsw_vtable_t *faiss_hnsw_default_vtable(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_FAISS_HNSW_VTABLE_H */