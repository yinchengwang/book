/**
 * @file faiss_hnsw_stub.c
 * @brief FAISS HNSW 桩函数实现
 *
 * 为测试提供 HNSW 函数的桩实现，避免因 HNSW 模块编译问题导致链接失败。
 * 实际功能需要完整实现 HNSW 模块后替换。
 */

#include <stdlib.h>
#include <string.h>
#include <db/index/vector_index/hnsw/faiss_hnsw.h>

/* 桩函数：创建 HNSW 索引 */
faiss_hnsw_t *faiss_hnsw_index_create(int32_t M, int32_t dims, int32_t ef_construction,
                                      distance_metric_t metric,
                                      quantization_type_t quant_type) {
    faiss_hnsw_t *index = (faiss_hnsw_t *)calloc(1, sizeof(faiss_hnsw_t));
    if (!index) {
        return NULL;
    }

    index->M = M;
    index->dims = dims;
    index->ef_construction = ef_construction;
    index->ef_search = ef_construction;
    index->metric = metric;
    index->quantization_type = quant_type;
    index->n_total = 0;
    index->max_level = 0;
    index->entry_pointer = -1;

    return index;
}

/* 桩函数：销毁 HNSW 索引 */
void faiss_hnsw_index_drop(faiss_hnsw_t *index) {
    if (!index) {
        return;
    }

    if (index->vectors) {
        free(index->vectors);
    }
    if (index->levels) {
        free(index->levels);
    }
    if (index->offsets) {
        free(index->offsets);
    }
    if (index->nbs) {
        free(index->nbs);
    }

    free(index);
}

/* 桩函数：添加向量 */
int32_t faiss_hnsw_index_add(faiss_hnsw_t *index, int32_t n, const float *vectors) {
    if (!index || n <= 0 || !vectors) {
        return -1;
    }

    /* 简化实现：扩展向量数组 */
    size_t old_size = (size_t)index->n_total * index->dims;
    size_t add_size = (size_t)n * index->dims;
    size_t new_size = old_size + add_size;

    float *new_vectors = (float *)realloc(index->vectors, new_size * sizeof(float));
    if (!new_vectors) {
        return -1;
    }

    index->vectors = new_vectors;
    memcpy(index->vectors + old_size, vectors, add_size * sizeof(float));
    index->n_total += n;

    return 0;
}

/* 桩函数：搜索最近邻 */
int32_t faiss_hnsw_index_search(faiss_hnsw_t *index, const float *query, int32_t k,
                                int32_t ef_search, float *distance, int32_t *vec_id) {
    if (!index || !query || k <= 0 || !distance || !vec_id) {
        return -1;
    }

    (void)ef_search;  /* 未使用 */

    /* 简化实现：暴力搜索（仅用于测试链接） */
    /* 返回前 k 个向量（未排序，仅用于链接测试） */
    for (int32_t i = 0; i < k && i < index->n_total; i++) {
        vec_id[i] = i;

        /* 计算欧氏距离 */
        float dist = 0.0f;
        const float *vec = index->vectors + (size_t)i * index->dims;
        for (int32_t d = 0; d < index->dims; d++) {
            float diff = query[d] - vec[d];
            dist += diff * diff;
        }
        distance[i] = dist;
    }

    /* 填充剩余位置 */
    for (int32_t i = index->n_total; i < k; i++) {
        vec_id[i] = -1;
        distance[i] = 0.0f;
    }

    return 0;
}

/* 桩函数：量化类型查询 */
quantization_type_t faiss_hnsw_index_quantization_type(const faiss_hnsw_t *index) {
    return index ? index->quantization_type : QUANTIZATION_TYPE_NONE;
}

/* 桩函数：设置量化参数 */
int32_t faiss_hnsw_index_set_quantization_params(faiss_hnsw_t *index,
                                                  const faiss_hnsw_quantization_params_t *params) {
    if (!index || !params) {
        return -1;
    }
    index->quantization_params = *params;
    return 0;
}

/* 桩函数：获取量化参数 */
int32_t faiss_hnsw_index_get_quantization_params(const faiss_hnsw_t *index,
                                                  faiss_hnsw_quantization_params_t *params) {
    if (!index || !params) {
        return -1;
    }
    *params = index->quantization_params;
    return 0;
}

/* 桩函数：删除向量 */
int32_t faiss_hnsw_index_delete(faiss_hnsw_t *index, int32_t id) {
    (void)index;
    (void)id;
    return 0;  /* 桩实现，始终返回成功 */
}

/* 桩函数：批量删除 */
int32_t faiss_hnsw_index_delete_batch(faiss_hnsw_t *index, const int32_t *ids, int32_t n) {
    (void)index;
    (void)ids;
    (void)n;
    return 0;
}

/* 桩函数：取消删除 */
int32_t faiss_hnsw_index_undelete(faiss_hnsw_t *index, int32_t id) {
    (void)index;
    (void)id;
    return 0;
}

/* 桩函数：检查是否已删除 */
bool faiss_hnsw_index_is_deleted(const faiss_hnsw_t *index, int32_t id) {
    (void)index;
    (void)id;
    return false;
}
