#include "db/index/vector_index/hnsw/faiss_hnsw_vtable.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "db/core/log.h"

#include <stdlib.h>

/* 默认 vtable：直接调用 faiss_hnsw_index_* 系列 */
static int v_add(void *idx, int32_t n, const float *v) {
    return faiss_hnsw_index_add((faiss_hnsw_t *)idx, n, v);
}
static int v_search(void *idx, const float *q, int32_t k,
                    float *d, int32_t *ids) {
    return faiss_hnsw_index_search((faiss_hnsw_t *)idx, q, k, 64, d, ids);
}
static int v_save(void *idx, const char *p) { (void)idx; (void)p; return 0; }
static int v_load(void *idx, const char *p) { (void)idx; (void)p; return 0; }
static void v_destroy(void *idx) { faiss_hnsw_index_drop((faiss_hnsw_t *)idx); }
static size_t v_ntotal(void *idx) {
    return (size_t)faiss_hnsw_index_ntotal((faiss_hnsw_t *)idx);
}

static const faiss_hnsw_vtable_t g_default_vtable = {
    .add = v_add, .search = v_search,
    .save = v_save, .load = v_load,
    .destroy = v_destroy, .ntotal = v_ntotal,
};

const faiss_hnsw_vtable_t *faiss_hnsw_default_vtable(void) {
    return &g_default_vtable;
}