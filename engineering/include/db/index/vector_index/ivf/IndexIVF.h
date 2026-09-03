#ifndef FAISS_IVF_INDEX_H
#define FAISS_IVF_INDEX_H

#include <stdint.h>
#include <stdbool.h>

#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>
#include <db/index/vector_index/ivf/inverted_lists.h>
#include <db/index/vector_index/ivf/direct_map.h>
#include <db/index/storage_backend.h>
#include <db/index/heap/heap_vector_store.h>
#include <db/index/vector_ref.h>
#include <db/index/index_persist_control.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Phase 1 基础设施前向声明 */
typedef struct storage_backend storage_backend_t;
typedef struct heap_vector_store heap_vector_store_t;
typedef struct persist_control persist_control_t;
typedef struct vector_ref vector_ref_t;

typedef struct faiss_ivf faiss_ivf_t;

typedef struct faiss_ivf_training_params {
	int32_t n_init;
	int32_t max_iter;
	double tol;
	int32_t random_state;
	bool verbose;
} faiss_ivf_training_params_t;

typedef struct faiss_ivf_quantization_params {
	int32_t pq_m;
	int32_t pq_bits;
} faiss_ivf_quantization_params_t;

faiss_ivf_t *faiss_ivf_index_create(int32_t dims,
									int32_t nlist,
									int32_t nlist2,
									int32_t nprobe,
									distance_metric_t metric,
									quantization_type_t quantization_type);
int32_t faiss_ivf_index_train(faiss_ivf_t *index, int32_t n, const float *vectors);
int32_t faiss_ivf_index_add(faiss_ivf_t *index, int32_t n, const float *vectors);
int32_t faiss_ivf_index_search(faiss_ivf_t *index, const float *query, int32_t k, float *distances, int32_t *labels);
int32_t faiss_ivf_index_search_with_count(faiss_ivf_t *index,
									  const float *query,
									  int32_t k,
									  float *distances,
									  int32_t *labels,
									  int32_t *hit_count);
void faiss_ivf_index_reset(faiss_ivf_t *index);
bool faiss_ivf_index_is_trained(const faiss_ivf_t *index);
int32_t faiss_ivf_index_size(const faiss_ivf_t *index);
int32_t faiss_ivf_index_nlist(const faiss_ivf_t *index);
int32_t faiss_ivf_index_nlist2(const faiss_ivf_t *index);
quantization_type_t faiss_ivf_index_quantization_type(const faiss_ivf_t *index);
int32_t faiss_ivf_index_set_nprobe(faiss_ivf_t *index, int32_t nprobe);
int32_t faiss_ivf_index_get_nprobe(const faiss_ivf_t *index);
int32_t faiss_ivf_index_set_quantization_params(faiss_ivf_t *index,
										const faiss_ivf_quantization_params_t *params);
int32_t faiss_ivf_index_get_quantization_params(const faiss_ivf_t *index,
										faiss_ivf_quantization_params_t *params);
int32_t faiss_ivf_index_set_training_params(faiss_ivf_t *index, const faiss_ivf_training_params_t *params);
int32_t faiss_ivf_index_get_training_params(const faiss_ivf_t *index, faiss_ivf_training_params_t *params);
void faiss_ivf_index_drop(faiss_ivf_t *index);

/* Phase 2: DirectMap + 删除 + 重构 */
int32_t faiss_ivf_index_remove_ids(faiss_ivf_t *index, const int32_t *ids_to_remove, int32_t n_remove);
int32_t faiss_ivf_index_reconstruct(const faiss_ivf_t *index, int32_t id, float *recons);
void faiss_ivf_index_compact(faiss_ivf_t *index);

#ifdef __cplusplus
}
#endif

#endif
