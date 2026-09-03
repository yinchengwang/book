/*
 * 桶边缘效应优化实现。
 */

#include "faiss_ivf_hnsw_private.h"
#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

float faiss_ivf_hnsw_compute_boundary_ratio(const faiss_ivf_hnsw_t *index,
                                             const float *vector,
                                             float *nearest, float *second) {
    if (!index || !vector) return 1.0f;

    float dists[2] = {FLT_MAX, FLT_MAX};
    int32_t ids[2] = {-1, -1};

    for (int32_t i = 0; i < index->nlist; i++) {
        float dist = distance_compute(index->metric, vector,
            &index->primary_centroids[(size_t)i * index->dims], index->dims);
        if (dist < dists[0]) {
            dists[1] = dists[0]; ids[1] = ids[0];
            dists[0] = dist; ids[0] = i;
        } else if (dist < dists[1]) {
            dists[1] = dist; ids[1] = i;
        }
    }

    if (nearest) *nearest = dists[0];
    if (second) *second = dists[1];
    if (dists[0] > 0) return dists[1] / dists[0];
    return 1.0f;
}

int faiss_ivf_hnsw_is_boundary_vector(const faiss_ivf_hnsw_t *index,
                                        const float *vector, float threshold) {
    float nearest, second;
    float ratio = faiss_ivf_hnsw_compute_boundary_ratio(index, vector, &nearest, &second);
    return ratio < threshold;
}

int32_t faiss_ivf_hnsw_boundary_aware_search(const faiss_ivf_hnsw_t *index,
                                               const float *query,
                                               int32_t nprobe_base,
                                               faiss_ivf_hnsw_scored_index_t *expanded_clusters,
                                               int32_t max_expanded) {
    if (!index || !query || !expanded_clusters || max_expanded <= 0) return 0;

    int32_t nprobe = nprobe_base;
    if (nprobe > index->nlist) nprobe = index->nlist;

    faiss_ivf_hnsw_scored_index_t *base_clusters =
        (faiss_ivf_hnsw_scored_index_t *)malloc((size_t)index->nlist * sizeof(faiss_ivf_hnsw_scored_index_t));
    if (!base_clusters) return 0;

    for (int32_t i = 0; i < index->nlist; i++) {
        base_clusters[i].id = i;
        base_clusters[i].distance = distance_compute(index->metric, query,
            &index->primary_centroids[(size_t)i * index->dims], index->dims);
    }

    qsort(base_clusters, (size_t)index->nlist, sizeof(faiss_ivf_hnsw_scored_index_t),
          faiss_ivf_hnsw_compare_scored_indices);

    float boundary_ratio, nearest, second;
    const float *sample_vec = &index->primary_centroids[(size_t)base_clusters[0].id * index->dims];
    boundary_ratio = faiss_ivf_hnsw_compute_boundary_ratio(index, sample_vec, &nearest, &second);

    int32_t expanded_count = nprobe_base;
    float expand_factor = 1.5f;
    if (boundary_ratio < 1.1f) expand_factor = 2.0f;
    else if (boundary_ratio < 1.3f) expand_factor = 1.5f;

    expanded_count = (int32_t)(nprobe_base * expand_factor);
    if (expanded_count > max_expanded) expanded_count = max_expanded;
    if (expanded_count > index->nlist) expanded_count = index->nlist;

    for (int32_t i = 0; i < expanded_count; i++) {
        expanded_clusters[i].id = base_clusters[i].id;
        expanded_clusters[i].distance = base_clusters[i].distance;
    }

    free(base_clusters);
    return expanded_count;
}

int32_t faiss_ivf_hnsw_reassign_boundary_vectors_enhanced(
    faiss_ivf_hnsw_t *index, const float *vectors,
    int32_t n, int32_t *labels, float reassign_threshold) {

    if (!index || !vectors || !labels) return 0;

    int32_t reassigned = 0;
    for (int32_t i = 0; i < n; i++) {
        const float *vec = &vectors[(size_t)i * index->dims];
        int32_t current_cluster = labels[i];

        if (current_cluster < 0 || current_cluster >= index->nlist) continue;

        float current_dist = distance_compute(index->metric, vec,
            &index->primary_centroids[(size_t)current_cluster * index->dims], index->dims);

        float best_dist = FLT_MAX;
        int32_t best_cluster = current_cluster;

        for (int32_t c = 0; c < index->nlist; c++) {
            if (c == current_cluster) continue;
            float dist = distance_compute(index->metric, vec,
                &index->primary_centroids[(size_t)c * index->dims], index->dims);
            if (dist < best_dist) {
                best_dist = dist;
                best_cluster = c;
            }
        }

        if (best_cluster != current_cluster && best_dist < current_dist * reassign_threshold) {
            labels[i] = best_cluster;
            reassigned++;
        }
    }
    return reassigned;
}

int32_t faiss_ivf_hnsw_count_boundary_vectors(const faiss_ivf_hnsw_t *index,
                                                const float *vectors, int32_t n,
                                                const int32_t *labels, float boundary_threshold) {
    if (!index || !vectors || !labels) return 0;
    int32_t boundary_count = 0;
    for (int32_t i = 0; i < n; i++) {
        const float *vec = &vectors[(size_t)i * index->dims];
        if (faiss_ivf_hnsw_is_boundary_vector(index, vec, boundary_threshold)) boundary_count++;
    }
    return boundary_count;
}
