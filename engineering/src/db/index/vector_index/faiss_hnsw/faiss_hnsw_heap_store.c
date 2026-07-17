/**
 * @file faiss_hnsw_heap_store.c
 * @brief faiss_hnsw_index_set_heap_store 占位实现
 *
 * faiss_hnsw 内存版 HNSW 当前不支持 heap_vector_store 绑定，
 * 此文件提供空实现，满足链接需要。待 hnsw 持久化版完善后，
 * 此实现将被替换为完整实现。
 */
#include "faiss_hnsw_internal.h"
#include <db/index/heap/heap_vector_store.h>

int faiss_hnsw_index_set_heap_store(faiss_hnsw_t *index, heap_vector_store_t *heap_store) {
    (void)index;
    (void)heap_store;
    /* 内存版 faiss_hnsw 暂不支持 heap 存储绑定 */
    return 0;
}
