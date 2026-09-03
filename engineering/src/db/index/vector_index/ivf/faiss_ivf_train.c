/*
 * faiss_ivf_train.c
 *
 * IVF 索引训练流程。
 *
 * === 训练三阶段 ===
 *
 * 第 1 阶段 — 一级 K-Means:
 *   对所有训练向量跑 K-Means，得到 nlist 个一级中心。
 *   同时记录每个样本属于哪个一级簇（primary_labels），
 *   后续二级训练和残差训练都依赖这个分组信息。
 *
 * 第 2 阶段 — 二级 K-Means:
 *   对每个一级簇内的样本再跑一次 K-Means，得到该簇下的 nlist2 个二级中心。
 *   如果某个簇的样本数 < nlist2，则二级中心数缩减为样本数。
 *   如果样本数只有 1 个，则直接复用一级中心作为唯一的二级中心，
 *   避免对单点跑 K-Means 的退化情况。
 *
 * 第 3 阶段 — PQ 量化器训练（可选）:
 *   计算每个样本与其所属二级中心之间的残差向量，
 *   用这些残差训练 PQ 量化器，使得量化器学习的是"残差的分布"
 *   而非原始向量的分布，残差的动态范围更小，量化精度更高。
 *
 * === 训练后才能 add ===
 * 训练完成后 index->trained = true，此后不能再调用 train。
 * 如果需要重新训练，必须先 reset。
 */

#include "faiss_ivf_private.h"

int32_t faiss_ivf_index_train(faiss_ivf_t *index, int32_t n, const float *vectors)
{
    int32_t nlist;
    int32_t *primary_labels;
    int32_t *primary_counts;
    int32_t cluster;
    int32_t vector_i;

    if (!index || n <= 0 || !vectors) {
        return -1;
    }
    /* 不支持增量训练：已有数据时禁止重新训练 */
    if (index->n_total > 0) {
        return -1;
    }

    /* 第一步: 修正一级聚类数，避免簇数超过训练样本数 */
    nlist = index->nlist;
    if (nlist > n) {
        nlist = n;
        index->nlist = n;
    }

    /* primary_labels[n]: 每个样本归属的一级簇编号
     * primary_counts[nlist]: 每个一级簇内的样本数，供二级 K-Means 使用 */
    primary_labels = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    primary_counts = (int32_t *)calloc((size_t)index->nlist, sizeof(int32_t));
    if (!primary_labels || !primary_counts) {
        free(primary_labels);
        free(primary_counts);
        return -1;
    }

    /* 第二步: 清空旧的二级中心与计数信息（用 calloc/memset 覆盖） */
    memset(index->secondary_centroids, 0, (size_t)index->total_bucket_count * index->dims * sizeof(float));
    memset(index->secondary_counts, 0, (size_t)index->nlist * sizeof(int32_t));

    /* ── 第 1 阶段: 训练一级中心 ── */
    if (!faiss_ivf_run_kmeans_training(index, n, vectors, nlist, index->primary_centroids, primary_labels)) {
        free(primary_labels);
        free(primary_counts);
        return -1;
    }

    /* 统计每个一级簇的样本数 */
    for (vector_i = 0; vector_i < n; ++vector_i) {
        if (primary_labels[vector_i] >= 0 && primary_labels[vector_i] < nlist) {
            primary_counts[primary_labels[vector_i]]++;
        }
    }

    /* ── 第 2 阶段: 对每个一级簇训练二级中心 ── */
    for (cluster = 0; cluster < nlist; ++cluster) {
        /* 二级中心数取 min(nlist2, 该簇样本数) */
        int32_t secondary_count = primary_counts[cluster] < index->nlist2 ? primary_counts[cluster] : index->nlist2;
        int32_t bucket_id = faiss_ivf_get_bucket_id(index, cluster, 0);

        if (secondary_count <= 0) {
            secondary_count = 1;
        }

        index->secondary_counts[cluster] = secondary_count;

        if (secondary_count == 1) {
            /* 样本过少：退化为单个二级中心，直接复用一级中心。
             * 避免对单点跑 K-Means，也避免分配空数组。 */
            memcpy(&index->secondary_centroids[bucket_id * index->dims],
                   &index->primary_centroids[cluster * index->dims],
                   (size_t)index->dims * sizeof(float));
        } else {
            float *cluster_vectors;
            int32_t cluster_offset = 0;

            /* 收集该一级簇下所有样本到连续数组 */
            cluster_vectors = (float *)malloc((size_t)primary_counts[cluster] * index->dims * sizeof(float));
            if (!cluster_vectors) {
                free(primary_labels);
                free(primary_counts);
                return -1;
            }

            for (vector_i = 0; vector_i < n; ++vector_i) {
                if (primary_labels[vector_i] != cluster) {
                    continue;
                }
                memcpy(&cluster_vectors[cluster_offset * index->dims],
                       &vectors[vector_i * index->dims],
                       (size_t)index->dims * sizeof(float));
                cluster_offset++;
            }

            /* 对该簇样本跑 K-Means，labels 参数为 NULL（不再需要） */
            if (!faiss_ivf_run_kmeans_training(index,
                                               primary_counts[cluster],
                                               cluster_vectors,
                                               secondary_count,
                                               &index->secondary_centroids[bucket_id * index->dims],
                                               NULL)) {
                free(cluster_vectors);
                free(primary_labels);
                free(primary_counts);
                return -1;
            }

            free(cluster_vectors);
        }
    }

    /* ── 第 3 阶段: 训练 PQ 量化器（若启用）── */
    if (faiss_ivf_train_quantizer_residuals(index, n, vectors, primary_labels) != 0) {
        free(primary_labels);
        free(primary_counts);
        return -1;
    }

    free(primary_labels);
    free(primary_counts);
    index->trained = true;
    return 0;
}
