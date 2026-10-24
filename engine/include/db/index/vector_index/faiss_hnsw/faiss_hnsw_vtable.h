// faiss_hnsw_vtable.h
// faiss_hnsw 虚函数表（默认实现为直接调用 faiss_hnsw_index_* 系列）
//
// 当前实现非常薄：g_default_vtable 仅做一层间接调用，便于将来
// 注入 Mock、Tracing 或量化路径时直接替换表项而无需改 .c 调用方。
//
// 此头文件被 faiss_hnsw_vtable.c 内部使用。

#ifndef FAISS_HNSW_VTABLE_H
#define FAISS_HNSW_VTABLE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// HNSW 虚函数表：每个函数指针都是 thin wrapper，签名与 public API 对齐
typedef struct faiss_hnsw_vtable {
    int (*add)(void *idx, int32_t n, const float *v);
    int (*search)(void *idx, const float *q, int32_t k,
                  float *d, int32_t *ids);
    int (*save)(void *idx, const char *p);
    int (*load)(void *idx, const char *p);
    void (*destroy)(void *idx);
    size_t (*ntotal)(void *idx);
} faiss_hnsw_vtable_t;

/**
 * 获取默认 vtable（直接调用 faiss_hnsw_index_* 系列）
 *
 * @return 指向全局静态 vtable 的指针（生命周期 == 进程生命周期）
 */
const faiss_hnsw_vtable_t *faiss_hnsw_default_vtable(void);

#ifdef __cplusplus
}
#endif

#endif  // FAISS_HNSW_VTABLE_H