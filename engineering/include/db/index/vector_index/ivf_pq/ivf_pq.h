/*
 * ivf_pq.h
 *
 * IVF-PQ (Inverted File with Product Quantization) 索引
 */

#ifndef INDEX_VECTOR_IVF_PQ_H
#define INDEX_VECTOR_IVF_PQ_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ivf_pq_index ivf_pq_index_t;

ivf_pq_index_t *ivf_pq_create(int nlist, int pq_m, int pq_bits, int dims);
void ivf_pq_set_nprobe(ivf_pq_index_t *idx, int nprobe);
int ivf_pq_train(ivf_pq_index_t *idx, int n, const float *vectors);
int ivf_pq_add(ivf_pq_index_t *idx, int n, const float *vectors, const int *ids);
int ivf_pq_search(const ivf_pq_index_t *idx, const float *query, int k, int *ids, float *distances);
int ivf_pq_rerank(ivf_pq_index_t *idx, int n, const float *vectors, int k, int *ids, float *distances);
int ivf_pq_save(const ivf_pq_index_t *idx, const char *path);
ivf_pq_index_t *ivf_pq_load(const char *path);
void ivf_pq_stats(const ivf_pq_index_t *idx, int *out_n_vectors, int *out_code_size);
void ivf_pq_destroy(ivf_pq_index_t *idx);

#ifdef __cplusplus
}
#endif

#endif
