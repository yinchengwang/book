#ifndef FAISS_HNSW_UTILS_H
#define FAISS_HNSW_UTILS_H

#include <db/index/vector_index/hnsw/faiss_hnsw.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FAISS_HNSW_INVALID_LEVEL_ID -1

static inline int32_t faiss_cum_nb_neighbors(faiss_hnsw_t *index, int32_t layer_no)
{
    /* 边界检查：防止越界访问
     * 当层号 >= 数组大小时，返回最后一层的累计值（即总邻居容量） */
    if (layer_no < 0) {
        return 0;
    }
    if ((uint32_t)layer_no >= index->cum_nneighbor_per_level_size) {
        return index->cum_nneighbor_per_level[index->cum_nneighbor_per_level_size - 1];
    }
    return index->cum_nneighbor_per_level[layer_no];
}

static inline void faiss_neighbor_range(faiss_hnsw_t *index, int32_t no, int32_t layer_no, size_t *begin, size_t *end)
{
    int32_t o = index->offsets[no];
    *begin = o + faiss_cum_nb_neighbors(index, layer_no);
    *end = o + faiss_cum_nb_neighbors(index, layer_no + 1);
}

#ifdef __cplusplus
}
#endif

#endif