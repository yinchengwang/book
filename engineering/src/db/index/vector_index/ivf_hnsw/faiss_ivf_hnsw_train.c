/*
 * HNSW-IVF 训练流程实现。
 */

#include "faiss_ivf_hnsw_private.h"
#include "db/log.h"
#include <stdio.h>

int32_t faiss_ivf_hnsw_index_train(faiss_ivf_hnsw_t *index, int32_t n, const float *vectors) {
    if (!index || n <= 0 || !vectors) return -1;
    if (index->n_total > 0) return -1;

    int32_t nlist = index->nlist;
    if (nlist > n) { nlist = n; index->nlist = n; }

    int32_t *primary_labels = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    int32_t *primary_counts = (int32_t *)calloc((size_t)index->nlist, sizeof(int32_t));
    if (!primary_labels || !primary_counts) { free(primary_labels); free(primary_counts); return -1; }

    memset(index->secondary_centroids, 0, (size_t)index->total_bucket_count * index->dims * sizeof(float));
    memset(index->secondary_counts, 0, (size_t)index->nlist * sizeof(int32_t));

    if (!faiss_ivf_hnsw_run_kmeans_training(index, n, vectors, nlist,
                                               index->primary_centroids, primary_labels)) {
        free(primary_labels); free(primary_counts); return -1;
    }

    for (int32_t i = 0; i < n; i++) {
        if (primary_labels[i] >= 0 && primary_labels[i] < nlist) primary_counts[primary_labels[i]]++;
    }

    for (int32_t cluster = 0; cluster < nlist; cluster++) {
        int32_t secondary_count = primary_counts[cluster] < index->nlist2 ? primary_counts[cluster] : index->nlist2;
        int32_t bucket_id = faiss_ivf_hnsw_get_bucket_id(index, cluster, 0);
        if (secondary_count <= 0) secondary_count = 1;
        index->secondary_counts[cluster] = secondary_count;

        if (secondary_count == 1) {
            memcpy(&index->secondary_centroids[(size_t)bucket_id * index->dims],
                   &index->primary_centroids[(size_t)cluster * index->dims],
                   (size_t)index->dims * sizeof(float));
        } else {
            float *cluster_vectors = (float *)malloc((size_t)primary_counts[cluster] * index->dims * sizeof(float));
            if (!cluster_vectors) { free(primary_labels); free(primary_counts); return -1; }

            int32_t offset = 0;
            for (int32_t i = 0; i < n; i++) {
                if (primary_labels[i] == cluster) {
                    memcpy(&cluster_vectors[(size_t)offset * index->dims], &vectors[(size_t)i * index->dims],
                           (size_t)index->dims * sizeof(float));
                    offset++;
                }
            }

            if (!faiss_ivf_hnsw_run_kmeans_training(index, primary_counts[cluster], cluster_vectors,
                                                    secondary_count,
                                                    &index->secondary_centroids[(size_t)bucket_id * index->dims],
                                                    NULL)) {
                free(cluster_vectors); free(primary_labels); free(primary_counts); return -1;
            }
            free(cluster_vectors);
        }
    }

    index->hnsw_entry_point = -1;
    index->hnsw_max_level = 0;
    int32_t max_level = 0;
    if (hnsw_prepare_level_tab(index, nlist, &max_level) != 0) {
        LOG_ERROR("HNSW 图准备失败");
        index->hnsw_entry_point = -1;
        index->hnsw_max_level = 0;
        index->hnsw_nbs_size = 0;
    } else {
        LOG_INFO("HNSW 图已准备: max_level=%d, nbs_size=%u", max_level, index->hnsw_nbs_size);
        for (int32_t i = 0; i < nlist; i++) {
            if (hnsw_insert_vector(index, i, index->hnsw_levels[i], NULL) != 0) {
                LOG_WARN("插入质心 %d 失败", i);
            }
        }
        LOG_INFO("HNSW 图构建完成: entry_point=%d, max_level=%d",
               index->hnsw_entry_point, index->hnsw_max_level);
    }

    float avg, std;
    int32_t max_count, min_count;
    int32_t *cluster_counts = (int32_t *)calloc((size_t)index->nlist, sizeof(int32_t));
    if (cluster_counts) {
        for (int32_t i = 0; i < n; i++) {
            if (primary_labels[i] >= 0 && primary_labels[i] < nlist) cluster_counts[primary_labels[i]]++;
        }
        faiss_ivf_hnsw_compute_cluster_stats(cluster_counts, nlist, &avg, &std, &max_count, &min_count);
        float imbalance_ratio = (avg > 0) ? (float)max_count / avg - 1.0f : 0.0f;
        printf("[Balance] Cluster stats: avg=%.1f, std=%.1f, max=%d, imbalance=%.2f\n", avg, std, max_count, imbalance_ratio);
        if (imbalance_ratio > 0.3f) {
            printf("[Balance] Attempting boundary reassignment...\n");
            int32_t reassigned = faiss_ivf_hnsw_reassign_boundary_vectors_enhanced(index, vectors, n, primary_labels, 0.9f);
            printf("[Balance] Reassigned %d boundary vectors\n", reassigned);
        }
        free(cluster_counts);
    }

    if (faiss_ivf_hnsw_train_quantizer_residuals(index, n, vectors, primary_labels) != 0) {
        free(primary_labels); free(primary_counts); return -1;
    }

    free(primary_labels);
    free(primary_counts);
    index->trained = true;
    return 0;
}
