#ifndef FAISS_IVF_HNSW_INDEX_H
#define FAISS_IVF_HNSW_INDEX_H

#include <stdint.h>
#include <stdbool.h>

#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct faiss_ivf_hnsw faiss_ivf_hnsw_t;

typedef struct faiss_ivf_hnsw_params {
    int32_t dims;
    int32_t nlist;
    int32_t nlist2;
    int32_t nprobe;
    int32_t max_assignments;
    int32_t M;
    int32_t ef_construction;
    int32_t ef_search;
    distance_metric_t metric;
    quantization_type_t quantization_type;
    int32_t pq_m;
    int32_t pq_bits;
    int32_t train_max_vectors;
    int32_t kmeans_max_iter;
    double kmeans_tol;
    int32_t random_state;
} faiss_ivf_hnsw_params_t;

typedef struct faiss_ivf_hnsw_training_params {
    int32_t n_init;
    int32_t max_iter;
    double tol;
    int32_t random_state;
    bool verbose;
} faiss_ivf_hnsw_training_params_t;

typedef struct faiss_ivf_hnsw_quantization_params {
    int32_t pq_m;
    int32_t pq_bits;
} faiss_ivf_hnsw_quantization_params_t;

faiss_ivf_hnsw_t *faiss_ivf_hnsw_index_create(const faiss_ivf_hnsw_params_t *params);
void faiss_ivf_hnsw_index_drop(faiss_ivf_hnsw_t *index);
void faiss_ivf_hnsw_index_reset(faiss_ivf_hnsw_t *index);
bool faiss_ivf_hnsw_index_is_trained(const faiss_ivf_hnsw_t *index);
int32_t faiss_ivf_hnsw_index_size(const faiss_ivf_hnsw_t *index);
int32_t faiss_ivf_hnsw_index_train(faiss_ivf_hnsw_t *index, int32_t n, const float *vectors);
int32_t faiss_ivf_hnsw_index_add(faiss_ivf_hnsw_t *index, int32_t n, const float *vectors);
int32_t faiss_ivf_hnsw_index_search(faiss_ivf_hnsw_t *index, const float *query, int32_t k, float *distances, int32_t *labels);
int32_t faiss_ivf_hnsw_index_search_with_count(faiss_ivf_hnsw_t *index, const float *query, int32_t k, float *distances, int32_t *labels, int32_t *hit_count);
int32_t faiss_ivf_hnsw_index_set_nprobe(faiss_ivf_hnsw_t *index, int32_t nprobe);
int32_t faiss_ivf_hnsw_index_get_nprobe(const faiss_ivf_hnsw_t *index);
int32_t faiss_ivf_hnsw_index_set_ef_search(faiss_ivf_hnsw_t *index, int32_t ef_search);
int32_t faiss_ivf_hnsw_index_get_ef_search(const faiss_ivf_hnsw_t *index);
int32_t faiss_ivf_hnsw_index_set_max_assignments(faiss_ivf_hnsw_t *index, int32_t max_assignments);
int32_t faiss_ivf_hnsw_index_get_max_assignments(const faiss_ivf_hnsw_t *index);
int32_t faiss_ivf_hnsw_index_set_quantization_params(faiss_ivf_hnsw_t *index, const faiss_ivf_hnsw_quantization_params_t *params);
int32_t faiss_ivf_hnsw_index_get_quantization_params(faiss_ivf_hnsw_t *index, faiss_ivf_hnsw_quantization_params_t *params);
int32_t faiss_ivf_hnsw_index_set_training_params(faiss_ivf_hnsw_t *index, const faiss_ivf_hnsw_training_params_t *params);
int32_t faiss_ivf_hnsw_index_get_training_params(const faiss_ivf_hnsw_t *index, faiss_ivf_hnsw_training_params_t *params);
int32_t faiss_ivf_hnsw_index_memory_size(const faiss_ivf_hnsw_t *index);
int32_t faiss_ivf_hnsw_index_nlist(const faiss_ivf_hnsw_t *index);
quantization_type_t faiss_ivf_hnsw_index_quantization_type(const faiss_ivf_hnsw_t *index);

#ifdef __cplusplus
}
#endif

#endif /* FAISS_IVF_HNSW_INDEX_H */
