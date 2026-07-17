#ifndef FAISS_IVF_HNSW_PRIVATE_H
#define FAISS_IVF_HNSW_PRIVATE_H

#include <db/index/vector_index/ivf_hnsw/IndexIVFHNSW.h>
#include <db/index/vector_index/ivf/inverted_lists.h>
#include <db/index/vector_index/ivf/direct_map.h>

#include <algo-prod/Kmeans/kmeans.h>
#include <algo-prod/distance/distance.h>
#include <algo-prod/quantization/quantization.h>

#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define FAISS_IVF_HNSW_RAND_LEVEL_SEED 12345
#define FAISS_IVF_HNSW_SWAP_POSITIONS_SEED 789
#define FAISS_IVF_HNSW_MAX_CANDIDATES_ON_STACK 256
#define FAISS_IVF_HNSW_DEFAULT_EF_SEARCH 50
#define FAISS_IVF_HNSW_DEFAULT_M 16
#define FAISS_IVF_HNSW_DEFAULT_EF_CONSTRUCTION 100
#define FAISS_IVF_HNSW_DEFAULT_MAX_ASSIGNMENTS 1

typedef struct faiss_ivf_hnsw_assign_entry {
    int32_t bucket_id;
    int32_t offset;
} faiss_ivf_hnsw_assign_entry_t;

typedef struct faiss_ivf_hnsw_scored_index {
    float distance;
    int32_t id;
} faiss_ivf_hnsw_scored_index_t;

struct faiss_ivf_hnsw {
    float *vectors;
    uint8_t *codes;
    int32_t n_total;
    int32_t dims;
    int32_t nlist;
    int32_t nlist2;
    int32_t nprobe;
    int32_t code_size;
    int32_t total_bucket_count;
    distance_metric_t metric;
    quantization_type_t quantization_type;
    bool trained;
    faiss_ivf_hnsw_training_params_t training_params;
    faiss_ivf_hnsw_quantization_params_t quantization_params;
    quantizer_config_t quantizer_config;
    quantizer_t *quantizer;
    int32_t *hnsw_levels;
    uint32_t hnsw_levels_size;
    int32_t *hnsw_offsets;
    uint32_t hnsw_offsets_size;
    int32_t *hnsw_nbs;
    uint32_t hnsw_nbs_size;
    int32_t hnsw_entry_point;
    int32_t hnsw_max_level;
    float *hnsw_assign_probas;
    uint32_t hnsw_assign_probas_size;
    int32_t *hnsw_cum_nneighbor_per_level;
    uint32_t hnsw_cum_nneighbor_per_level_size;
    int32_t M;
    int32_t ef_construction;
    int32_t ef_search;
    faiss_inverted_lists_t *invlists;
    faiss_direct_map_t direct_map;
    int32_t max_assignments;
    faiss_ivf_hnsw_assign_entry_t *assignments;
    float *primary_centroids;
    float *secondary_centroids;
    int32_t *secondary_counts;
};

static inline size_t hnsw_neighbor_offset(const faiss_ivf_hnsw_t *index, int32_t node_id, int32_t layer) {
    if (node_id < 0 || layer < 0) return 0;
    return (size_t)(index->hnsw_offsets[node_id] + (layer == 0 ? index->M * 2 : index->M * (1 + layer)));
}

static inline void hnsw_neighbor_range(const faiss_ivf_hnsw_t *index, int32_t no, int32_t layer_no, size_t *begin, size_t *end) {
    if (no < 0 || layer_no < 0) { *begin = *end = 0; return; }
    if (layer_no > index->hnsw_levels[no]) { *begin = *end = 0; return; }
    size_t base_offset = (size_t)index->hnsw_offsets[no];
    if (layer_no == 0) {
        *begin = base_offset;
        *end = base_offset + (size_t)(index->M * 2);
    } else {
        *begin = base_offset + (size_t)(index->M * 2) + (size_t)(index->M * (layer_no - 1));
        *end = *begin + (size_t)index->M;
    }
}

int32_t hnsw_set_default_probas(faiss_ivf_hnsw_t *index);
int32_t hnsw_random_level(faiss_ivf_hnsw_t *index);
int32_t hnsw_greedy_update_nearest(const faiss_ivf_hnsw_t *index, const float *query, int32_t level, int32_t *nearest, float *d_nearest);
int32_t hnsw_prepare_level_tab(faiss_ivf_hnsw_t *index, int32_t n, int32_t *max_level);
void hnsw_connect_points(faiss_ivf_hnsw_t *index, int32_t pt_id, int32_t neighbor, int32_t layer_no);
int32_t hnsw_insert_vector(faiss_ivf_hnsw_t *index, int32_t pt_id, int32_t pt_level, faiss_ivf_hnsw_t *visited_marker);
int32_t hnsw_search_layer_candidates(const faiss_ivf_hnsw_t *index, const float *query, int32_t layer_no, int32_t entry, int32_t ef, int32_t *result_ids, float *result_dist, int32_t *result_count);

void faiss_ivf_hnsw_set_default_training_params(faiss_ivf_hnsw_training_params_t *params);
int faiss_ivf_hnsw_training_params_are_valid(const faiss_ivf_hnsw_training_params_t *params);
int faiss_ivf_hnsw_quantization_params_are_valid(const faiss_ivf_hnsw_t *index, const faiss_ivf_hnsw_quantization_params_t *params);
int faiss_ivf_hnsw_rebuild_quantizer(faiss_ivf_hnsw_t *index);
int faiss_ivf_hnsw_compare_scored_indices(const void *left, const void *right);
int32_t faiss_ivf_hnsw_get_bucket_id(const faiss_ivf_hnsw_t *index, int32_t primary_cluster, int32_t secondary_cluster);
const float *faiss_ivf_hnsw_get_bucket_centroid_ptr(const faiss_ivf_hnsw_t *index, int32_t primary_cluster, int32_t secondary_cluster);
void faiss_ivf_hnsw_compute_residual(float *residual, const float *vector, const float *centroid, int32_t dims);
int faiss_ivf_hnsw_run_kmeans_training(const faiss_ivf_hnsw_t *index, int32_t n, const float *vectors, int32_t n_clusters, float *centroids_out, int32_t *labels_out);
int32_t faiss_ivf_hnsw_find_nearest_primary_centroid(const faiss_ivf_hnsw_t *index, const float *vector);
int32_t faiss_ivf_hnsw_find_nearest_secondary_centroid(const faiss_ivf_hnsw_t *index, int32_t primary_cluster, const float *vector);
int faiss_ivf_hnsw_train_quantizer_residuals(faiss_ivf_hnsw_t *index, int32_t n, const float *vectors, const int32_t *primary_labels);
int32_t faiss_ivf_hnsw_coarse_search(const faiss_ivf_hnsw_t *index, const float *query, int32_t nprobe, faiss_ivf_hnsw_scored_index_t *primary_clusters);
void faiss_ivf_hnsw_assign_to_buckets(faiss_ivf_hnsw_t *index, const float *vector, int32_t vector_id, int32_t k);
int32_t faiss_ivf_hnsw_expand_assignments(faiss_ivf_hnsw_t *index, int32_t new_total);

void faiss_ivf_hnsw_compute_cluster_stats(const int32_t *counts, int32_t nlist, float *out_avg, float *out_std, int32_t *out_max, int32_t *out_min);
int faiss_ivf_hnsw_check_cluster_balance(const int32_t *counts, int32_t nlist, float max_imbalance);
int32_t faiss_ivf_hnsw_reassign_boundary_vectors_enhanced(faiss_ivf_hnsw_t *index, const float *vectors, int32_t n, int32_t *labels, float reassign_threshold);

#endif
