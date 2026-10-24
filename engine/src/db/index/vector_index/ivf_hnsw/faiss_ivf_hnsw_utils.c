/*
 * HNSW-IVF 内部工具函数实现。
 */

#include "faiss_ivf_hnsw_private.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

void faiss_ivf_hnsw_set_default_training_params(faiss_ivf_hnsw_training_params_t *params) {
    if (!params) return;
    params->n_init = 1;
    params->max_iter = 5;
    params->tol = 1e-4;
    params->random_state = 42;
    params->verbose = false;
}

int faiss_ivf_hnsw_training_params_are_valid(const faiss_ivf_hnsw_training_params_t *params) {
    if (!params) return 0;
    if (params->n_init <= 0) return 0;
    if (params->max_iter <= 0) return 0;
    if (params->tol < 0) return 0;
    return 1;
}

int faiss_ivf_hnsw_quantization_params_are_valid(const faiss_ivf_hnsw_t *index,
                                                   const faiss_ivf_hnsw_quantization_params_t *params) {
    if (!index || !params || index->quantization_type == QUANTIZATION_TYPE_NONE) return 0;
    if (params->pq_m <= 0 || params->pq_bits <= 0 || params->pq_bits > 8) return 0;
    if (index->dims % params->pq_m != 0) return 0;
    return 1;
}

int faiss_ivf_hnsw_rebuild_quantizer(faiss_ivf_hnsw_t *index) {
    if (!index) return -1;
    quantizer_drop(index->quantizer);
    index->quantizer = NULL;
    index->code_size = 0;
    if (index->quantization_type == QUANTIZATION_TYPE_NONE) return 0;
    index->quantizer = quantizer_create(&index->quantizer_config);
    return index->quantizer ? 0 : -1;
}

int faiss_ivf_hnsw_compare_scored_indices(const void *left, const void *right) {
    const faiss_ivf_hnsw_scored_index_t *a = (const faiss_ivf_hnsw_scored_index_t *)left;
    const faiss_ivf_hnsw_scored_index_t *b = (const faiss_ivf_hnsw_scored_index_t *)right;
    if (a->distance < b->distance) return -1;
    if (a->distance > b->distance) return 1;
    if (a->id < b->id) return -1;
    if (a->id > b->id) return 1;
    return 0;
}

int32_t faiss_ivf_hnsw_get_bucket_id(const faiss_ivf_hnsw_t *index,
                                       int32_t primary_cluster, int32_t secondary_cluster) {
    return primary_cluster * index->nlist2 + secondary_cluster;
}

const float *faiss_ivf_hnsw_get_bucket_centroid_ptr(const faiss_ivf_hnsw_t *index,
                                                      int32_t primary_cluster, int32_t secondary_cluster) {
    int32_t bucket_id = faiss_ivf_hnsw_get_bucket_id(index, primary_cluster, secondary_cluster);
    return &index->secondary_centroids[(size_t)bucket_id * index->dims];
}

void faiss_ivf_hnsw_compute_residual(float *residual, const float *vector,
                                       const float *centroid, int32_t dims) {
    for (int32_t i = 0; i < dims; i++) residual[i] = vector[i] - centroid[i];
}

int faiss_ivf_hnsw_run_kmeans_training(const faiss_ivf_hnsw_t *index,
                                         int32_t n, const float *vectors,
                                         int32_t n_clusters, float *centroids_out,
                                         int32_t *labels_out) {
    if (!index || !vectors || !centroids_out || n <= 0 || n_clusters <= 0 || n_clusters > n) return 0;

    double *training_data = (double *)malloc((size_t)n * index->dims * sizeof(double));
    if (!training_data) return 0;

    for (int32_t i = 0; i < n * index->dims; i++) training_data[i] = (double)vectors[i];

    KMeansParams params;
    memset(&params, 0, sizeof(params));
    params.n_clusters = n_clusters;
    params.n_init = 1;
    params.max_iter = index->training_params.max_iter > 0 ? index->training_params.max_iter : 5;
    params.tol = index->training_params.tol > 0 ? index->training_params.tol : 1e-4;
    params.verbose = index->training_params.verbose ? 1 : 0;
    params.random_state = index->training_params.random_state > 0 ? index->training_params.random_state : 42;
    params.init = "k-means++";
    params.algorithm = "lloyd";
    params.n_samples = n;
    params.n_features = index->dims;
    params.data = training_data;

    Kmeans(&params);
    free(training_data);

    if (!params.success || !params.cluster_centers) { KmeansFree(&params); return 0; }

    for (int32_t i = 0; i < n_clusters * index->dims; i++) centroids_out[i] = (float)params.cluster_centers[i];

    if (labels_out && params.labels) {
        for (int32_t i = 0; i < n; i++) labels_out[i] = params.labels[i];
    }

    KmeansFree(&params);
    return 1;
}

int32_t faiss_ivf_hnsw_find_nearest_primary_centroid(const faiss_ivf_hnsw_t *index, const float *vector) {
    int32_t best = 0;
    float best_dist = distance_compute(index->metric, vector, index->primary_centroids, index->dims);
    for (int32_t i = 1; i < index->nlist; i++) {
        float dist = distance_compute(index->metric, vector, &index->primary_centroids[(size_t)i * index->dims], index->dims);
        if (dist < best_dist) { best_dist = dist; best = i; }
    }
    return best;
}

int32_t faiss_ivf_hnsw_find_nearest_secondary_centroid(const faiss_ivf_hnsw_t *index,
                                                          int32_t primary_cluster, const float *vector) {
    int32_t secondary_count = index->secondary_counts[primary_cluster];
    if (secondary_count <= 1) return 0;

    int32_t bucket_id = faiss_ivf_hnsw_get_bucket_id(index, primary_cluster, 0);
    int32_t best = 0;
    float best_dist = distance_compute(index->metric, vector, &index->secondary_centroids[(size_t)bucket_id * index->dims], index->dims);

    for (int32_t i = 1; i < secondary_count; i++) {
        int32_t bid = faiss_ivf_hnsw_get_bucket_id(index, primary_cluster, i);
        float dist = distance_compute(index->metric, vector, &index->secondary_centroids[(size_t)bid * index->dims], index->dims);
        if (dist < best_dist) { best_dist = dist; best = i; }
    }
    return best;
}

int faiss_ivf_hnsw_train_quantizer_residuals(faiss_ivf_hnsw_t *index,
                                               int32_t n, const float *vectors,
                                               const int32_t *primary_labels) {
    if (!index || index->quantization_type == QUANTIZATION_TYPE_NONE || !index->quantizer) return 0;

    float *residuals = (float *)malloc((size_t)n * index->dims * sizeof(float));
    if (!residuals) return -1;

    for (int32_t i = 0; i < n; i++) {
        faiss_ivf_hnsw_compute_residual(&residuals[(size_t)i * index->dims],
            &vectors[(size_t)i * index->dims],
            &index->primary_centroids[(size_t)primary_labels[i] * index->dims], index->dims);
    }

    int ret = quantizer_train(index->quantizer, n, residuals);
    if (ret != 0) { free(residuals); return -1; }
    index->code_size = quantizer_code_size(index->quantizer);
    free(residuals);
    return 0;
}

int32_t hnsw_set_default_probas(faiss_ivf_hnsw_t *index) {
    if (!index) return -1;
    float levelMult = 1.0f / logf((float)index->M);
    int count = 0;
    for (int level = 0; ; level++) {
        float proba = expf(-level / levelMult) * (1.0f - expf(-1.0f / levelMult));
        if (proba < 1e-9f) break;
        count++;
    }

    index->hnsw_assign_probas_size = (uint32_t)count;
    index->hnsw_cum_nneighbor_per_level_size = (uint32_t)(count + 1);
    index->hnsw_assign_probas = (float *)malloc((size_t)count * sizeof(float));
    index->hnsw_cum_nneighbor_per_level = (int32_t *)malloc((size_t)(count + 1) * sizeof(int32_t));

    if (!index->hnsw_assign_probas || !index->hnsw_cum_nneighbor_per_level) {
        free(index->hnsw_assign_probas); free(index->hnsw_cum_nneighbor_per_level); return -1;
    }

    int32_t neibors_sum = 0;
    index->hnsw_cum_nneighbor_per_level[0] = 0;
    for (int level = 0; ; level++) {
        float proba = expf(-level / levelMult) * (1.0f - expf(-1.0f / levelMult));
        if (proba < 1e-9f) break;
        index->hnsw_assign_probas[level] = proba;
        neibors_sum += (level == 0) ? index->M * 2 : index->M;
        index->hnsw_cum_nneighbor_per_level[level + 1] = neibors_sum;
    }
    return 0;
}

int32_t hnsw_random_level(faiss_ivf_hnsw_t *index) {
    double cum_sum = 0.0;
    for (uint32_t level = 0; level < index->hnsw_assign_probas_size; level++) cum_sum += index->hnsw_assign_probas[level];
    double f = (double)rand() / (double)(RAND_MAX + 1.0) * cum_sum;
    cum_sum = 0.0;
    for (uint32_t level = 0; level < index->hnsw_assign_probas_size; level++) {
        cum_sum += index->hnsw_assign_probas[level];
        if (f < cum_sum) return (int32_t)level;
    }
    return (int32_t)(index->hnsw_assign_probas_size - 1);
}

static inline float hnsw_distance_centroids(const faiss_ivf_hnsw_t *index, const float *query, int32_t centroid_id) {
    if (!query) return 0.0f;
    return distance_compute(index->metric, query, &index->primary_centroids[(size_t)centroid_id * index->dims], index->dims);
}

int32_t hnsw_greedy_update_nearest(const faiss_ivf_hnsw_t *index, const float *query,
                                     int32_t level, int32_t *nearest, float *d_nearest) {
    for (;;) {
        int32_t prev_nearest = *nearest;
        size_t begin, end;
        hnsw_neighbor_range(index, *nearest, level, &begin, &end);
        for (size_t j = begin; j < end; j++) {
            int32_t neighbor = index->hnsw_nbs[j];
            if (neighbor < 0) break;
            float dist = hnsw_distance_centroids(index, query, neighbor);
            if (dist < *d_nearest) { *d_nearest = dist; *nearest = neighbor; }
        }
        if (*nearest == prev_nearest) break;
    }
    return *nearest;
}

int32_t hnsw_prepare_level_tab(faiss_ivf_hnsw_t *index, int32_t n, int32_t *max_level) {
    *max_level = -1;
    int32_t running_offset = 0;
    for (int32_t i = 0; i < n; i++) {
        int32_t pt_level = hnsw_random_level(index);
        index->hnsw_levels[i] = pt_level;
        index->hnsw_offsets[i] = running_offset;
        int32_t total_slots = index->M * 2 + pt_level * index->M;
        running_offset += total_slots;
        if (pt_level > *max_level) *max_level = pt_level;
    }

    int32_t total_size = running_offset > 0 ? running_offset : index->M * 2;
    int32_t *new_nbs = (int32_t *)realloc(index->hnsw_nbs, (size_t)total_size * sizeof(int32_t));
    if (!new_nbs) return -1;
    for (int32_t i = 0; i < total_size; i++) new_nbs[i] = -1;
    index->hnsw_nbs = new_nbs;
    index->hnsw_nbs_size = (uint32_t)total_size;
    return 0;
}

void hnsw_connect_points(faiss_ivf_hnsw_t *index, int32_t pt_id, int32_t neighbor, int32_t layer_no) {
    size_t begin, end;
    hnsw_neighbor_range(index, pt_id, layer_no, &begin, &end);
    if (begin >= end) return;

    int32_t empty_pos = -1;
    for (size_t j = begin; j < end; j++) {
        if (index->hnsw_nbs[j] < 0) { if (empty_pos < 0) empty_pos = (int32_t)j; }
        else if (index->hnsw_nbs[j] == neighbor) return;
    }

    if (empty_pos >= 0) {
        index->hnsw_nbs[empty_pos] = neighbor;
    }
}

int32_t hnsw_search_layer_candidates(const faiss_ivf_hnsw_t *index, const float *query,
                                     int32_t layer_no, int32_t entry, int32_t ef,
                                     int32_t *result_ids, float *result_dist, int32_t *result_count) {
    int32_t num_results = 0;
    int32_t candidates[256];
    float cand_dist[256];
    int32_t num_candidates = 0;

    if (!index || entry < 0 || entry >= index->nlist || ef <= 0) { *result_count = 0; return -1; }

    bool *visited = (bool *)calloc((size_t)index->nlist, sizeof(bool));
    if (!visited) return -1;

    candidates[0] = entry;
    cand_dist[0] = hnsw_distance_centroids(index, query, entry);
    num_candidates = 1;
    visited[entry] = true;
    result_ids[0] = entry;
    result_dist[0] = cand_dist[0];
    num_results = 1;

    while (num_results < ef && num_results <= num_candidates) {
        int32_t best_idx = -1;
        float best_dist = FLT_MAX;
        for (int32_t i = 0; i < num_candidates; i++) {
            if (!visited[candidates[i]] && cand_dist[i] < best_dist) { best_dist = cand_dist[i]; best_idx = i; }
        }
        if (best_idx < 0) break;

        int32_t current = candidates[best_idx];
        visited[current] = true;
        size_t begin, end;
        hnsw_neighbor_range(index, current, layer_no, &begin, &end);
        if (begin >= index->hnsw_nbs_size) break;
        if (end > index->hnsw_nbs_size) end = index->hnsw_nbs_size;

        for (size_t j = begin; j < end && num_candidates < 256; j++) {
            int32_t neighbor = index->hnsw_nbs[j];
            if (neighbor < 0 || neighbor >= index->nlist) continue;
            if (!visited[neighbor]) {
                candidates[num_candidates] = neighbor;
                cand_dist[num_candidates] = hnsw_distance_centroids(index, query, neighbor);
                num_candidates++;
                if (num_results < ef) { result_ids[num_results] = neighbor; result_dist[num_results] = cand_dist[num_candidates - 1]; num_results++; }
            }
        }
    }

    int32_t sort_count = num_results < ef ? num_results : ef;
    for (int32_t i = 0; i < sort_count - 1; i++) {
        for (int32_t j = i + 1; j < sort_count; j++) {
            if (result_dist[j] < result_dist[i]) {
                float tmp_d = result_dist[i]; result_dist[i] = result_dist[j]; result_dist[j] = tmp_d;
                int32_t tmp_id = result_ids[i]; result_ids[i] = result_ids[j]; result_ids[j] = tmp_id;
            }
        }
    }

    *result_count = sort_count;
    free(visited);
    return 0;
}

int32_t hnsw_insert_vector(faiss_ivf_hnsw_t *index, int32_t pt_id, int32_t pt_level, faiss_ivf_hnsw_t *visited_marker) {
    (void)visited_marker;
    if (pt_id < 0 || pt_id >= index->nlist) return -1;
    if (!index->hnsw_nbs || index->hnsw_nbs_size == 0) {
        index->hnsw_entry_point = pt_id; index->hnsw_max_level = pt_level; return 0;
    }
    int32_t nearest = index->hnsw_entry_point;
    if (nearest < 0) { index->hnsw_entry_point = pt_id; index->hnsw_max_level = pt_level; return 0; }

    const float *pt_vec = &index->primary_centroids[(size_t)pt_id * index->dims];
    float d_nearest = hnsw_distance_centroids(index, pt_vec, nearest);

    for (int32_t level = index->hnsw_max_level; level > pt_level; level--) {
        nearest = hnsw_greedy_update_nearest(index, pt_vec, level, &nearest, &d_nearest);
    }

    int32_t ef = index->ef_construction > 0 ? index->ef_construction : index->M * 2;
    if (ef > 256) ef = 256;

    int32_t candidate_ids[256];
    float candidate_dist[256];
    int32_t candidate_count = 0;
    int32_t last_nearest = nearest;

    for (int32_t layer = pt_level; layer >= 0; layer--) {
        if (last_nearest < 0 || last_nearest >= index->nlist) break;
        hnsw_search_layer_candidates(index, pt_vec, layer, last_nearest, ef, candidate_ids, candidate_dist, &candidate_count);
        if (candidate_count > 0) {
            int32_t max_neighbors = (layer == 0) ? index->M * 2 : index->M;
            int32_t neighbors_to_connect = candidate_count < max_neighbors ? candidate_count : max_neighbors;
            for (int32_t i = 0; i < neighbors_to_connect; i++) {
                if (candidate_ids[i] >= 0 && candidate_ids[i] < index->nlist) {
                    hnsw_connect_points(index, pt_id, candidate_ids[i], layer);
                    hnsw_connect_points(index, candidate_ids[i], pt_id, layer);
                }
            }
            last_nearest = candidate_ids[0];
        }
    }

    if (pt_level > index->hnsw_max_level) { index->hnsw_entry_point = pt_id; index->hnsw_max_level = pt_level; }
    return 0;
}

void faiss_ivf_hnsw_assign_to_buckets(faiss_ivf_hnsw_t *index, const float *vector,
                                        int32_t vector_id, int32_t k) {
    typedef struct { int32_t cluster; float dist; } cluster_dist_t;
    cluster_dist_t *dists = (cluster_dist_t *)malloc((size_t)index->nlist * sizeof(cluster_dist_t));
    if (!dists) return;

    for (int32_t i = 0; i < index->nlist; i++) {
        dists[i].cluster = i;
        dists[i].dist = distance_compute(index->metric, vector, &index->primary_centroids[(size_t)i * index->dims], index->dims);
    }

    for (int32_t i = 0; i < k && i < index->nlist; i++) {
        int32_t min_idx = i;
        for (int32_t j = i + 1; j < index->nlist; j++) if (dists[j].dist < dists[min_idx].dist) min_idx = j;
        cluster_dist_t tmp = dists[i]; dists[i] = dists[min_idx]; dists[min_idx] = tmp;
    }

    for (int32_t i = 0; i < k && i < index->nlist; i++) {
        int32_t cluster = dists[i].cluster;
        int32_t secondary = faiss_ivf_hnsw_find_nearest_secondary_centroid(index, cluster, vector);
        int32_t bucket_id = faiss_ivf_hnsw_get_bucket_id(index, cluster, secondary);
        faiss_inverted_lists_add_entry(index->invlists, (size_t)bucket_id, vector_id, NULL);
        size_t list_size = faiss_inverted_lists_list_size(index->invlists, (size_t)bucket_id);
        faiss_direct_map_add(&index->direct_map, vector_id, bucket_id, (int64_t)(list_size - 1));
        if (index->assignments && vector_id >= 0) {
            index->assignments[(size_t)vector_id * index->max_assignments + i].bucket_id = bucket_id;
            index->assignments[(size_t)vector_id * index->max_assignments + i].offset = (int32_t)(list_size - 1);
        }
    }
    free(dists);
}

int32_t faiss_ivf_hnsw_expand_assignments(faiss_ivf_hnsw_t *index, int32_t new_total) {
    if (!index || new_total <= 0) return -1;
    faiss_ivf_hnsw_assign_entry_t *new_assignments = (faiss_ivf_hnsw_assign_entry_t *)realloc(
        index->assignments, (size_t)new_total * index->max_assignments * sizeof(faiss_ivf_hnsw_assign_entry_t));
    if (!new_assignments) return -1;
    index->assignments = new_assignments;
    return 0;
}

int32_t faiss_ivf_hnsw_coarse_search(const faiss_ivf_hnsw_t *index, const float *query,
                                       int32_t nprobe, faiss_ivf_hnsw_scored_index_t *primary_clusters) {
    if (!index || !query || !primary_clusters || nprobe <= 0) return -1;

    if (index->hnsw_entry_point < 0 || index->hnsw_max_level < 0 || index->hnsw_nbs_size == 0 || index->nlist <= 0) {
        for (int32_t i = 0; i < index->nlist; i++) {
            primary_clusters[i].id = i;
            primary_clusters[i].distance = hnsw_distance_centroids(index, query, i);
        }
        qsort(primary_clusters, (size_t)index->nlist, sizeof(faiss_ivf_hnsw_scored_index_t), faiss_ivf_hnsw_compare_scored_indices);
        return 0;
    }

    int32_t nearest = index->hnsw_entry_point;
    float d_nearest = hnsw_distance_centroids(index, query, nearest);

    for (int32_t level = index->hnsw_max_level; level >= 1; level--) {
        nearest = hnsw_greedy_update_nearest(index, query, level, &nearest, &d_nearest);
    }

    int32_t candidate_ids[256];
    float candidate_dist[256];
    int32_t candidate_count = 0;
    int32_t ef = index->ef_search > 0 ? index->ef_search : nprobe * 2;
    if (ef > 256) ef = 256;

    hnsw_search_layer_candidates(index, query, 0, nearest, ef, candidate_ids, candidate_dist, &candidate_count);

    if (candidate_count < nprobe) {
        typedef struct { int32_t id; float dist; } scored_t;
        scored_t *all = (scored_t *)malloc((size_t)index->nlist * sizeof(scored_t));
        if (all) {
            bool *used = (bool *)calloc((size_t)index->nlist, sizeof(bool));
            for (int32_t i = 0; i < candidate_count && i < 256; i++) if (candidate_ids[i] >= 0 && candidate_ids[i] < index->nlist) used[candidate_ids[i]] = true;
            int32_t count = 0;
            for (int32_t i = 0; i < index->nlist && count < index->nlist; i++) {
                if (!used[i]) { all[count].id = i; all[count].dist = hnsw_distance_centroids(index, query, i); count++; }
            }
            for (int32_t i = 0; i < count - 1; i++) for (int32_t j = i + 1; j < count; j++) if (all[j].dist < all[i].dist) { scored_t tmp = all[i]; all[i] = all[j]; all[j] = tmp; }
            int32_t idx = 0;
            for (int32_t i = 0; i < candidate_count && idx < index->nlist; i++) if (candidate_ids[i] >= 0 && candidate_ids[i] < index->nlist) { primary_clusters[idx].id = candidate_ids[i]; primary_clusters[idx].distance = candidate_dist[i]; idx++; }
            for (int32_t i = 0; i < count && idx < index->nlist && idx < nprobe; i++) { primary_clusters[idx].id = all[i].id; primary_clusters[idx].distance = all[i].dist; idx++; }
            free(used); free(all);
            qsort(primary_clusters, (size_t)idx, sizeof(faiss_ivf_hnsw_scored_index_t), faiss_ivf_hnsw_compare_scored_indices);
        }
    } else {
        for (int32_t i = 0; i < candidate_count && i < index->nlist; i++) { primary_clusters[i].id = candidate_ids[i]; primary_clusters[i].distance = candidate_dist[i]; }
        qsort(primary_clusters, (size_t)candidate_count, sizeof(faiss_ivf_hnsw_scored_index_t), faiss_ivf_hnsw_compare_scored_indices);
    }
    return 0;
}
