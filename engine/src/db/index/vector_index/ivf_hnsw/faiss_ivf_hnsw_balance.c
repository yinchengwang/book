/*
 * 聚簇大小平衡优化实现。
 */

#include "faiss_ivf_hnsw_private.h"
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void faiss_ivf_hnsw_compute_cluster_stats(const int32_t *counts, int32_t nlist,
                                          float *out_avg, float *out_std,
                                          int32_t *out_max, int32_t *out_min) {
    if (!counts || nlist <= 0) return;

    int32_t total = 0;
    int32_t max_count = counts[0];
    int32_t min_count = counts[0];

    for (int32_t i = 0; i < nlist; i++) {
        total += counts[i];
        if (counts[i] > max_count) max_count = counts[i];
        if (counts[i] < min_count) min_count = counts[i];
    }

    float avg = (float)total / nlist;
    float variance = 0.0f;
    for (int32_t i = 0; i < nlist; i++) {
        float diff = counts[i] - avg;
        variance += diff * diff;
    }
    variance /= nlist;
    float std = sqrtf(variance);

    if (out_avg) *out_avg = avg;
    if (out_std) *out_std = std;
    if (out_max) *out_max = max_count;
    if (out_min) *out_min = min_count;
}

int faiss_ivf_hnsw_check_cluster_balance(const int32_t *counts, int32_t nlist, float max_imbalance) {
    if (!counts || nlist <= 0) return 1;
    float avg, std;
    int32_t max_count, min_count;
    faiss_ivf_hnsw_compute_cluster_stats(counts, nlist, &avg, &std, &max_count, &min_count);
    if (avg > 0 && (float)max_count > avg * (1.0f + max_imbalance)) return 0;
    return 1;
}

int faiss_ivf_hnsw_split_cluster(faiss_ivf_hnsw_t *index, int32_t cluster_to_split,
                                   const float *vectors, int32_t n,
                                   const int32_t *labels, int32_t n_sub_clusters) {
    if (!index || cluster_to_split < 0 || cluster_to_split >= index->nlist) return -1;

    int32_t cluster_size = 0;
    for (int32_t i = 0; i < n; i++) {
        if (labels[i] == cluster_to_split) cluster_size++;
    }

    if (cluster_size < n_sub_clusters * 2) return -1;

    float *cluster_vectors = (float *)malloc((size_t)cluster_size * index->dims * sizeof(float));
    if (!cluster_vectors) return -1;

    int32_t offset = 0;
    for (int32_t i = 0; i < n; i++) {
        if (labels[i] == cluster_to_split) {
            memcpy(&cluster_vectors[(size_t)offset * index->dims], &vectors[(size_t)i * index->dims],
                   (size_t)index->dims * sizeof(float));
            offset++;
        }
    }

    float *sub_centroids = (float *)malloc((size_t)n_sub_clusters * index->dims * sizeof(float));
    if (!sub_centroids) { free(cluster_vectors); return -1; }

    int32_t *sub_labels = (int32_t *)malloc((size_t)cluster_size * sizeof(int32_t));
    if (!sub_labels) { free(cluster_vectors); free(sub_centroids); return -1; }

    if (!faiss_ivf_hnsw_run_kmeans_training(index, cluster_size, cluster_vectors,
                                             n_sub_clusters, sub_centroids, sub_labels)) {
        free(cluster_vectors); free(sub_centroids); free(sub_labels); return -1;
    }

    if (index->nlist2 > 1) {
        int32_t base_bucket_id = faiss_ivf_hnsw_get_bucket_id(index, cluster_to_split, 0);
        for (int32_t s = 0; s < n_sub_clusters && s < index->nlist2; s++) {
            memcpy(&index->secondary_centroids[(size_t)(base_bucket_id + s) * index->dims],
                   &sub_centroids[(size_t)s * index->dims],
                   (size_t)index->dims * sizeof(float));
        }
        index->secondary_counts[cluster_to_split] = n_sub_clusters;
    }

    free(cluster_vectors);
    free(sub_centroids);
    free(sub_labels);
    return 0;
}

int32_t faiss_ivf_hnsw_find_boundary_vectors(const faiss_ivf_hnsw_t *index,
                                              int32_t cluster, const float *vectors,
                                              int32_t n, const int32_t *labels,
                                              float threshold_dist,
                                              int32_t *boundary_ids, int32_t max_boundary) {
    if (!index || cluster < 0 || !vectors || !labels || !boundary_ids) return 0;

    const float *centroid = &index->primary_centroids[(size_t)cluster * index->dims];
    int32_t count = 0;
    float total_dist = 0.0f;
    typedef struct { int32_t id; float dist; } dist_pair_t;
    dist_pair_t *dists = (dist_pair_t *)malloc((size_t)n * sizeof(dist_pair_t));
    if (!dists) return 0;

    for (int32_t i = 0; i < n; i++) {
        if (labels[i] == cluster) {
            float dist = distance_compute(index->metric, &vectors[(size_t)i * index->dims], centroid, index->dims);
            dists[count].id = i;
            dists[count].dist = dist;
            total_dist += dist;
            count++;
        }
    }

    if (count == 0) { free(dists); return 0; }

    float avg_dist = total_dist / count;
    float threshold = avg_dist * (1.0f + threshold_dist);
    int32_t boundary_count = 0;
    for (int32_t i = 0; i < count && boundary_count < max_boundary; i++) {
        if (dists[i].dist > threshold) {
            boundary_ids[boundary_count++] = dists[i].id;
        }
    }

    free(dists);
    return boundary_count;
}

int32_t faiss_ivf_hnsw_reassign_boundary_vectors(faiss_ivf_hnsw_t *index,
                                                   const int32_t *boundary_ids,
                                                   int32_t n_boundary,
                                                   const float *vectors, int32_t *labels) {
    if (!index || !boundary_ids || !vectors || !labels) return 0;

    int32_t reassigned = 0;
    for (int32_t i = 0; i < n_boundary; i++) {
        int32_t vec_id = boundary_ids[i];
        const float *vec = &vectors[(size_t)vec_id * index->dims];
        int32_t best_cluster = 0;
        float best_dist = FLT_MAX;

        for (int32_t c = 0; c < index->nlist; c++) {
            if (c == labels[vec_id]) continue;
            float dist = distance_compute(index->metric, vec, &index->primary_centroids[(size_t)c * index->dims], index->dims);
            if (dist < best_dist) {
                best_dist = dist;
                best_cluster = c;
            }
        }

        if (best_dist < FLT_MAX) {
            labels[vec_id] = best_cluster;
            reassigned++;
        }
    }
    return reassigned;
}

int faiss_ivf_hnsw_balanced_training(faiss_ivf_hnsw_t *index, const float *vectors,
                                       int32_t n, int32_t *labels, float balance_threshold) {
    if (!index || !vectors || !labels) return -1;

    int32_t *counts = (int32_t *)calloc((size_t)index->nlist, sizeof(int32_t));
    if (!counts) return -1;

    for (int32_t i = 0; i < n; i++) {
        if (labels[i] >= 0 && labels[i] < index->nlist) counts[labels[i]]++;
    }

    int is_balanced = faiss_ivf_hnsw_check_cluster_balance(counts, index->nlist, balance_threshold);
    int32_t split_count = 0;

    if (!is_balanced) {
        float avg, std;
        int32_t max_count, min_count;
        faiss_ivf_hnsw_compute_cluster_stats(counts, index->nlist, &avg, &std, &max_count, &min_count);
        printf("[Balance] Clusters unbalanced: max=%d, avg=%.1f, std=%.1f\n", max_count, avg, std);

        for (int32_t c = 0; c < index->nlist; c++) {
            if ((float)counts[c] > avg * (1.0f + balance_threshold)) {
                if (faiss_ivf_hnsw_split_cluster(index, c, vectors, n, labels, 2) == 0) {
                    split_count++;
                    printf("[Balance] Split cluster %d (size=%d)\n", c, counts[c]);
                }
            }
        }
    }

    free(counts);
    return split_count >= 0 ? 0 : -1;
}
