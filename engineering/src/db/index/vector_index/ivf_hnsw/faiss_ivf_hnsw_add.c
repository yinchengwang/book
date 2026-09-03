/*
 * HNSW-IVF 向量添加实现。
 */

#include "faiss_ivf_hnsw_private.h"

int32_t faiss_ivf_hnsw_index_add(faiss_ivf_hnsw_t *index, int32_t n, const float *vectors) {
    if (!index || n <= 0 || !vectors) return -1;
    if (!index->trained) return -1;

    int32_t old_total = index->n_total;
    int32_t new_total = old_total + n;

    float *new_vectors = (float *)malloc((size_t)new_total * index->dims * sizeof(float));
    if (!new_vectors) return -1;

    if (old_total > 0 && index->vectors) {
        memcpy(new_vectors, index->vectors, (size_t)old_total * index->dims * sizeof(float));
        free(index->vectors);
    }
    memcpy(&new_vectors[(size_t)old_total * index->dims], vectors, (size_t)n * index->dims * sizeof(float));
    index->vectors = new_vectors;

    if (index->quantizer && index->code_size > 0) {
        uint8_t *new_codes = (uint8_t *)malloc((size_t)new_total * index->code_size * sizeof(uint8_t));
        if (!new_codes) return -1;

        if (old_total > 0 && index->codes) {
            memcpy(new_codes, index->codes, (size_t)old_total * index->code_size * sizeof(uint8_t));
            free(index->codes);
        }
        index->codes = new_codes;

        float *residual = (float *)malloc((size_t)index->dims * sizeof(float));
        if (!residual) return -1;

        for (int32_t i = 0; i < n; i++) {
            int32_t vector_id = old_total + i;
            const float *vec = &vectors[(size_t)i * index->dims];
            int32_t primary = faiss_ivf_hnsw_find_nearest_primary_centroid(index, vec);
            int32_t secondary = faiss_ivf_hnsw_find_nearest_secondary_centroid(index, primary, vec);
            const float *centroid = faiss_ivf_hnsw_get_bucket_centroid_ptr(index, primary, secondary);
            faiss_ivf_hnsw_compute_residual(residual, vec, centroid, index->dims);
            quantizer_encode(index->quantizer, residual, &index->codes[(size_t)vector_id * index->code_size]);
        }
        free(residual);
    }

    if (faiss_ivf_hnsw_expand_assignments(index, new_total) != 0) return -1;

    for (int32_t i = 0; i < n; i++) {
        int32_t vector_id = old_total + i;
        const float *vec = &vectors[(size_t)i * index->dims];
        faiss_ivf_hnsw_assign_to_buckets(index, vec, vector_id, index->max_assignments);
    }

    index->n_total = new_total;
    return 0;
}
