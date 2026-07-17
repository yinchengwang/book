/*
 * faiss_ivf_search.c
 *
 * IVF 索引搜索实现。
 *
 * === 搜索五阶段 ===
 *
 * 第 1 阶段 — 粗排 (Coarse Search):
 *   计算查询向量到所有 nlist 个一级中心的距离，排序取最近 nprobe 个簇。
 *   复杂度 O(nlist × dims)，由于 nlist 通常较小（~1000），可接受。
 *
 * 第 2 阶段 — 桶展开 (Bucket Expansion):
 *   将选中的 nprobe 个一级簇下的所有二级桶展开为扁平列表，
 *   按桶中心到查询向量的距离升序排列。桶数最多 nprobe × nlist2。
 *   排序是为了让更近的桶被优先扫描，提升 early termination 效果。
 *
 * 第 3 阶段 — 结果堆初始化:
 *   分配大小为 k 的大顶堆（result_dists[0] 始终是堆中最大距离 = 第 k 近）。
 *   初始堆为空，num_results 从 0 逐步增长到 k。
 *
 * 第 4 阶段 — 桶扫描 (Bucket Scan):
 *   按距离升序遍历桶，对桶内每个有效向量计算精确距离。
 *   用量化模式时使用 ADC (Asymmetric Distance Computation) 查表加速。
 *   维护大顶堆: 新距离 < 堆顶时替换堆顶并下滤。
 *
 * 第 5 阶段 — Heap 重排序（Heap 模式专属）:
 *   从 Heap 存储取出每个候选的完整向量，用 distance_compute 计算精确距离，
 *   排序取 top-k。兼容模式直接使用 index->vectors 中的连续数组。
 *
 * 第 6 阶段 — 结果输出:
 *   大顶堆 → 选择排序转为升序，填充 distances/labels。
 *
 * === 堆与 HNSW 的关系 ===
 * result_dists[0] = 最差（最大）距离，便于 O(1) 剪枝判断:
 *   if (dist < result_dists[0]) { ... }  // 值得进入 top-k
 * 这与 HNSW 搜索中 MinimaxHeap 的语义一致。
 */

#include "faiss_ivf_private.h"

#include <db/index/heap/heap_vector_store.h>
#include <db/index/vector_ref.h>
#include <algo-prod/distance/distance.h>

/*
 * faiss_ivf_load_vector - 从存储取出指定 ID 的向量
 *
 * Heap 模式: 通过 vector_refs[id] 定位页面和偏移，
 * 调用 heap_vector_get 取回完整向量。
 * 兼容模式: 直接从 index->vectors 复制。
 */
static int faiss_ivf_load_vector(const faiss_ivf_t *index,
                                 int32_t vec_id,
                                 float *out)
{
    if (index == NULL || vec_id < 0 || out == NULL) {
        return -1;
    }

    if (index->heap_store != NULL) {
        if ((uint32_t)vec_id >= index->vector_refs_size) {
            return -1;
        }
        return heap_vector_get(index->heap_store,
                               &index->vector_refs[vec_id],
                               out);
    }

    /* 兼容路径：直接从连续数组读取 */
    if (index->vectors == NULL || vec_id >= index->n_total) {
        return -1;
    }
    memcpy(out, &index->vectors[vec_id * index->dims],
           (size_t)index->dims * sizeof(float));
    return 0;
}

int32_t faiss_ivf_index_search_with_count(faiss_ivf_t *index,
                                          const float *query,
                                          int32_t k,
                                          float *distances,
                                          int32_t *labels,
                                          int32_t *hit_count)
{
    int32_t dims;
    int32_t nlist;
    int32_t nprobe;
    faiss_ivf_scored_index_t *primary_clusters;
    faiss_ivf_scored_index_t *bucket_queue;
    int32_t bucket_count = 0;
    float *query_residual = NULL;
    float *distance_table = NULL;
    int32_t i;
    int32_t err;

    if (!index || !index->trained || !query || k <= 0 || !distances || !labels || !hit_count) {
        return -1;
    }

    *hit_count = 0;

    /* 空索引直接返回 */
    if (index->n_total <= 0) {
        for (i = 0; i < k; i++) {
            distances[i] = FLT_MAX;
            labels[i] = -1;
        }
        return 0;
    }

    dims = index->dims;
    nlist = index->nlist;
    nprobe = index->nprobe;
    if (nprobe > nlist) {
        nprobe = nlist;
    }

    /* ── 第 1 阶段: 粗排 — 选最近 nprobe 个一级簇 ── */
    primary_clusters = (faiss_ivf_scored_index_t *)malloc((size_t)nlist * sizeof(faiss_ivf_scored_index_t));
    if (!primary_clusters) {
        return -1;
    }

    for (i = 0; i < nlist; i++) {
        primary_clusters[i].distance = distance_compute(
            index->metric, query, &index->primary_centroids[i * dims], dims);
        primary_clusters[i].id = i;
    }
    /* 按距离升序排序，前 nprobe 个即为最近的一级簇 */
    qsort(primary_clusters, (size_t)nlist, sizeof(faiss_ivf_scored_index_t),
          faiss_ivf_compare_scored_indices);

    /* ── 第 2 阶段: 桶展开 — 展开选中簇的所有二级桶 ── */
    int32_t max_buckets = nprobe * index->nlist2;
    bucket_queue = (faiss_ivf_scored_index_t *)malloc(
        (size_t)max_buckets * sizeof(faiss_ivf_scored_index_t));
    if (!bucket_queue) {
        free(primary_clusters);
        return -1;
    }

    for (int32_t p = 0; p < nprobe; p++) {
        int32_t cluster = primary_clusters[p].id;
        int32_t secondary_count = index->secondary_counts[cluster];

        if (secondary_count <= 0) {
            continue;
        }

        /* 展开该一级簇下的所有二级桶 */
        for (int32_t s = 0; s < secondary_count; s++) {
            int32_t bucket_id = faiss_ivf_get_bucket_id(index, cluster, s);
            float centroid_dist = distance_compute(
                index->metric, query,
                faiss_ivf_get_bucket_centroid_ptr(index, cluster, s), dims);
            bucket_queue[bucket_count].id = bucket_id;
            bucket_queue[bucket_count].distance = centroid_dist;
            bucket_count++;
        }
    }

    free(primary_clusters);

    /* 按桶中心距离升序: 近桶优先扫描 */
    qsort(bucket_queue, (size_t)bucket_count, sizeof(faiss_ivf_scored_index_t),
          faiss_ivf_compare_scored_indices);

    /* ── 第 3 阶段: 初始化结果大顶堆 ──
     * result_dists[0] 始终是 top-k 中最差的（最大的）距离，便于 O(1) 比较剪枝 */
    int32_t num_results = 0;
    int32_t *result_ids = (int32_t *)malloc((size_t)k * sizeof(int32_t));
    float *result_dists = (float *)malloc((size_t)k * sizeof(float));
    if (!result_ids || !result_dists) {
        free(result_ids);
        free(result_dists);
        free(bucket_queue);
        return -1;
    }

    /* 初始化 result_dists[0] 为 FLT_MAX，避免首次比较时读取未初始化值 */
    result_dists[0] = FLT_MAX;

    /* 量化模式: 预分配残差缓冲区和距离表（每个桶复用） */
    if (index->quantizer) {
        query_residual = (float *)malloc((size_t)dims * sizeof(float));
        distance_table = (float *)malloc(
            (size_t)quantizer_distance_table_size(index->quantizer) * sizeof(float));
        if (!query_residual || !distance_table) {
            free(query_residual);
            free(distance_table);
            free(result_ids);
            free(result_dists);
            free(bucket_queue);
            return -1;
        }
    }

    /* ── 第 4 阶段: 桶扫描 — 遍历选中桶计算距离 ── */
    for (int32_t bi = 0; bi < bucket_count; bi++) {
        int32_t bucket_id = bucket_queue[bi].id;

        /* 量化模式: 为该桶计算距离表。
         * ADC 原理: 对查询残差计算距离表，向量距离 = 查表累加各子段距离。
         * 每个桶的中心不同 → 残差不同 → 距离表需要逐桶重算。 */
        if (index->quantizer) {
            int32_t primary_cluster = bucket_id / index->nlist2;
            int32_t secondary = bucket_id % index->nlist2;

            faiss_ivf_compute_residual(
                query_residual, query,
                faiss_ivf_get_bucket_centroid_ptr(index, primary_cluster, secondary), dims);
            err = quantizer_compute_distance_table(
                index->quantizer, index->metric, query_residual, distance_table);
            if (err != 0) {
                free(query_residual);
                free(distance_table);
                free(result_ids);
                free(result_dists);
                free(bucket_queue);
                return -1;
            }
        }

        size_t list_size = faiss_inverted_lists_list_size(index->invlists, (size_t)bucket_id);
        const int32_t *list_ids = faiss_inverted_lists_get_ids(index->invlists, (size_t)bucket_id);

        for (size_t idx = 0; idx < list_size; idx++) {
            int32_t vec_id = list_ids[idx];

            /* 跳过墓碑（已删除的向量，id = -1） */
            if (vec_id < 0) {
                continue;
            }

            float dist;
            if (index->quantizer) {
                /* ADC 距离 = 查距离表累加（无需解压完整向量） */
                dist = quantizer_adc_distance(
                    index->quantizer, &index->codes[vec_id * index->code_size], distance_table);
            } else {
                /* 非量化模式: 完整距离计算 */
                float *vec_buf = (float *)malloc((size_t)dims * sizeof(float));
                if (vec_buf == NULL) {
                    continue;
                }
                if (faiss_ivf_load_vector(index, vec_id, vec_buf) != 0) {
                    free(vec_buf);
                    continue;
                }
                dist = distance_compute(index->metric, query, vec_buf, dims);
                free(vec_buf);
            }

            /* ── 维护 top-k 大顶堆 ──
             * 两个阶段: (A) 堆未满时追加并冒泡, (B) 堆满后替换堆顶并下滤 */
            if (dist < result_dists[0] || num_results < k) {
                if (num_results < k) {
                    /* 阶段 A: 追加到末尾，然后冒泡将最大者推到位置 0 */
                    result_ids[num_results] = vec_id;
                    result_dists[num_results] = dist;
                    num_results++;

                    for (int32_t p = num_results - 1; p > 0; p--) {
                        if (result_dists[p] > result_dists[p - 1]) {
                            float tmp_d = result_dists[p];
                            int32_t tmp_id = result_ids[p];
                            result_dists[p] = result_dists[p - 1];
                            result_ids[p] = result_ids[p - 1];
                            result_dists[p - 1] = tmp_d;
                            result_ids[p - 1] = tmp_id;
                        }
                    }
                } else {
                    /* 阶段 B: 替换堆顶（最差元素），然后下滤让最大值冒回位置 0 */
                    result_ids[0] = vec_id;
                    result_dists[0] = dist;

                    for (int32_t p = 1; p < k; p++) {
                        if (result_dists[p] > result_dists[0]) {
                            float tmp_d = result_dists[0];
                            int32_t tmp_id = result_ids[0];
                            result_dists[0] = result_dists[p];
                            result_ids[0] = result_ids[p];
                            result_dists[p] = tmp_d;
                            result_ids[p] = tmp_id;
                        }
                    }
                }
            }
        }
    }

    /* ── 第 5 阶段（Heap 模式专属重排序）: ──
     * 对阶段 1-4 收集的 num_results 个候选，从 Heap 取出完整向量，
     * 用 distance_compute 计算精确距离，再按精确距离选 top-k。
     *
     * 注意: 即使量化被启用，这里也强制走原始距离以保证正确性。
     * 兼容模式下 index->vectors 中已经是完整向量，无需额外重排序。 */
    if (index->heap_store != NULL && num_results > 0) {
        float *candidate_vectors = (float *)malloc((size_t)num_results * dims * sizeof(float));
        if (candidate_vectors == NULL) {
            free(query_residual);
            free(distance_table);
            free(result_ids);
            free(result_dists);
            free(bucket_queue);
            return -1;
        }

        /* 从 Heap 批量加载候选向量 */
        for (i = 0; i < num_results; i++) {
            int32_t vec_id = result_ids[i];
            if (vec_id < 0 || (uint32_t)vec_id >= index->vector_refs_size) {
                continue;
            }
            if (heap_vector_get(index->heap_store,
                                &index->vector_refs[vec_id],
                                &candidate_vectors[(size_t)i * dims]) != 0) {
                continue;
            }
            /* 用精确距离覆盖阶段 4 的近似距离 */
            result_dists[i] = distance_compute(
                index->metric,
                query,
                &candidate_vectors[(size_t)i * dims],
                dims);
        }
        free(candidate_vectors);
    }

    /* ── 第 6 阶段: 结果排序输出 ──
     * 大顶堆 → 选择排序转为升序（堆顶=最差=最大距离）。
     * k 通常很小（10~100），简单选择排序足够。 */
    *hit_count = num_results;

    for (int32_t out = 0; out < num_results; out++) {
        int32_t best_idx = out;
        for (int32_t j = out + 1; j < num_results; j++) {
            if (result_dists[j] < result_dists[best_idx]) {
                best_idx = j;
            }
        }
        if (best_idx != out) {
            float tmp_d = result_dists[out];
            int32_t tmp_id = result_ids[out];
            result_dists[out] = result_dists[best_idx];
            result_ids[out] = result_ids[best_idx];
            result_dists[best_idx] = tmp_d;
            result_ids[best_idx] = tmp_id;
        }
    }

    for (i = 0; i < num_results; i++) {
        distances[i] = result_dists[i];
        labels[i] = result_ids[i];
    }
    /* 不足 k 个时补 FLT_MAX / -1 */
    for (i = num_results; i < k; i++) {
        distances[i] = FLT_MAX;
        labels[i] = -1;
    }

    free(query_residual);
    free(distance_table);
    free(result_ids);
    free(result_dists);
    free(bucket_queue);

    return 0;
}

/* 简化版搜索: 内部调用 search_with_count，丢弃 hit_count */
int32_t faiss_ivf_index_search(faiss_ivf_t *index, const float *query, int32_t k, float *distances, int32_t *labels)
{
    int32_t hit_count;

    return faiss_ivf_index_search_with_count(index, query, k, distances, labels, &hit_count);
}
