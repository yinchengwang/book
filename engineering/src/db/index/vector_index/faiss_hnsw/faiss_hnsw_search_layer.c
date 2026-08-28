// faiss_hnsw_search_layer.c
// 实现 faiss_hnsw_search_layer：HNSW 核心搜索层算法
// 与 FAISS HNSW::search_from_candidates 语义一致
//
// 算法流程（参考 FAISS HNSW.cpp:search_from_candidates）：
//   1. 将 ep 压入堆（堆容量 ef，维护当前 ef 个最近候选）
//   2. 循环：
//      - 在堆中线性扫描找"未扩展"的最小距离候选
//      - 若其距离 >= 堆顶（最大），且堆已满，停止（所有剩余均 >= 已扩展的）
//      - 获取该候选的邻居，计算距离，压入堆
//   3. 收集堆中所有候选作为结果
//
// 关键设计（P6-M1.3 修复）：
//   - 堆充当"results"（ef 个最近候选），不再从中 pop 元素以免丢失候选
//   - 用单独的 expanded[] 数组标记"已扩展"vs"已加入堆"
//   - 终止条件：最小未扩展候选 >= 堆顶最大距离（无改进空间）
//   - 原算法缺陷：堆在扩展循环中被 pop 空，导致收集阶段无候选可返回
//
// 不变量：
//   - 堆保持 ef 个最近候选（满后插入更小值会替换最大）
//   - 邻居数组通过 cum_nneighbor 偏移定位每层

#include "faiss_hnsw_internal.h"

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// P6-M1.3：AVX2 intrinsics 用于 SIMD L2 距离
#if defined(__AVX2__) || defined(_MSC_VER)
#include <immintrin.h>
#endif

// =============================================================================
// 内部辅助函数
// =============================================================================

// 计算两点距离（L2 平方 / Cosine）
// 与 FAISS 行为一致：L2 时返回平方距离（用于排序无影响）
/* C4-1 T1：导出 SIMD-enabled compute_distance，供 search.c 复用 */
float faiss_hnsw_compute_distance(const faiss_hnsw_t *idx, const float *query, int32_t vec_id);

float faiss_hnsw_compute_distance(const faiss_hnsw_t *idx, const float *query, int32_t vec_id) {
    if (vec_id < 0 || vec_id >= idx->n_total || !idx->vectors || !query) {
        return FLT_MAX;
    }

    const float *v = idx->vectors + (size_t)vec_id * (size_t)idx->dims;
    float dist = 0.0f;

    if (idx->metric == DISTANCE_METRIC_L2_SQUARED) {
        // P6-M1.3：使用 SIMD (AVX2) 加速 L2 距离计算
        // 每个循环处理 8 个 float，吞吐量提升 ~8x
#if defined(__AVX2__) || defined(_MSC_VER)
        size_t i = 0;
        size_t limit = idx->dims & ~(size_t)7;  // 对齐到 8 的倍数
        __m256 acc = _mm256_setzero_ps();
        for (; i < limit; i += 8) {
            __m256 va = _mm256_loadu_ps(query + i);
            __m256 vb = _mm256_loadu_ps(v + i);
            __m256 diff = _mm256_sub_ps(va, vb);
            acc = _mm256_add_ps(acc, _mm256_mul_ps(diff, diff));
        }
        // 水平求和
        __m128 hi = _mm256_extractf128_ps(acc, 1);
        __m128 lo = _mm256_castps256_ps128(acc);
        __m128 sum = _mm_add_ps(lo, hi);
        sum = _mm_hadd_ps(sum, sum);
        sum = _mm_hadd_ps(sum, sum);
        dist = _mm_cvtss_f32(sum);
        // 处理剩余元素
        for (; i < idx->dims; i++) {
            float d = query[i] - v[i];
            dist += d * d;
        }
#else
        for (int32_t i = 0; i < idx->dims; i++) {
            float d = query[i] - v[i];
            dist += d * d;
        }
#endif
    } else if (idx->metric == DISTANCE_METRIC_COSINE) {
        // cosine 距离：1 - cos(q, v) = 1 - dot(q, v) / (||q|| * ||v||)
        float dot = 0.0f, norm_q = 0.0f, norm_v = 0.0f;
        for (int32_t i = 0; i < idx->dims; i++) {
            dot += query[i] * v[i];
            norm_q += query[i] * query[i];
            norm_v += v[i] * v[i];
        }
        if (norm_q > 0.0f && norm_v > 0.0f) {
            dist = 1.0f - dot / (sqrtf(norm_q) * sqrtf(norm_v));
        } else {
            dist = 1.0f;
        }
    } else {
        // 内积 / 汉明等其他度量：fallback 到 L2 平方
        for (int32_t i = 0; i < idx->dims; i++) {
            float d = query[i] - v[i];
            dist += d * d;
        }
    }
    return dist;
}

// 获取第 level 层第 i 个邻居的 vec_id
// 内部布局：idx->neighbors 是扁平数组，idx->offsets[vec_id] 是 vec_id 邻居的起始位置
//           idx->cum_nneighbor[level-1] 是 level 层的起始偏移（level=0 时偏移=0）
static int32_t get_neighbor(const faiss_hnsw_t *idx, int32_t vec_id, int32_t level, int32_t i) {
    if (!idx || !idx->neighbors || !idx->offsets || !idx->cum_nneighbor) {
        return -1;
    }
    if (vec_id < 0 || vec_id >= idx->n_total) {
        return -1;
    }

    int32_t level_offset = (level > 0) ? idx->cum_nneighbor[level - 1] : 0;
    int32_t vec_offset = idx->offsets[vec_id];
    return idx->neighbors[vec_offset + level_offset + i];
}

// 获取 vec_id 在 level 层的有效邻居数（邻居以 -1 结尾）
static int32_t get_neighbor_count(const faiss_hnsw_t *idx, int32_t vec_id, int32_t level) {
    if (!idx || !idx->levels || vec_id < 0) {
        return 0;
    }

    int32_t vec_level = idx->levels[vec_id];
    if (level > vec_level) {
        return 0;  // 该节点在此层不存在
    }

    // level 0 的最大邻居数为 2*M，其他层为 M
    int32_t max_neighbors = (level == 0) ? (2 * idx->M) : idx->M;

    int32_t count = 0;
    for (int32_t i = 0; i < max_neighbors; i++) {
        int32_t n = get_neighbor(idx, vec_id, level, i);
        if (n >= 0) {
            count++;
        } else {
            break;  // 遇到 -1 即终止
        }
    }
    return count;
}

// =============================================================================
// 公共 API：faiss_hnsw_search_layer
// =============================================================================

int32_t faiss_hnsw_search_layer(const faiss_hnsw_t *idx, int32_t level, const float *query,
                                 int32_t ef, int32_t start_id, float start_dist,
                                 int32_t *result_ids, float *result_dist,
                                 int32_t result_capacity) {
    // 参数校验
    if (!idx || !query || !result_ids || !result_dist) {
        return -1;
    }
    if (ef <= 0 || result_capacity <= 0) {
        return -1;
    }

    // 空索引或无入口点
    if (idx->n_total == 0 || idx->entry_point < 0) {
        for (int32_t i = 0; i < result_capacity; i++) {
            result_ids[i] = -1;
            result_dist[i] = FLT_MAX;
        }
        return 0;
    }

    // 1. 创建 MinimaxHeap（容量 ef，充当"results"——ef 个最近候选）
    faiss_hnsw_minimax_heap_t *heap = NULL;
    if (faiss_hnsw_minimax_heap_create(&heap, ef) != 0 || !heap) {
        return -1;
    }

    // 2. 创建 VisitedTable（大小足够覆盖所有可能的 vec_id）
    faiss_hnsw_visited_table_t *visited = NULL;
    int32_t vt_size = idx->n_total + ef + 100;
    if (faiss_hnsw_visited_table_create(&visited, vt_size) != 0 || !visited) {
        faiss_hnsw_minimax_heap_drop(heap);
        return -1;
    }

    // 3. expanded[] 数组：标记"已扩展"节点（避免重复扩展）
    uint8_t *expanded = (uint8_t *)calloc((size_t)vt_size, sizeof(uint8_t));
    if (!expanded) {
        faiss_hnsw_minimax_heap_drop(heap);
        faiss_hnsw_visited_table_drop(visited);
        return -1;
    }

    // 4. 从 greedy descent 给定的局部最优节点出发
    int32_t ep = start_id;
    float ep_dist = start_dist;
    // 边界保护：若 start_id 越界则回退到 entry_point
    if (ep < 0 || ep >= idx->n_total) {
        ep = idx->entry_point;
        ep_dist = FLT_MAX;
        const float *v = idx->vectors + (size_t)ep * (size_t)idx->dims;
        float dist = 0.0f;
        for (int32_t i = 0; i < idx->dims; i++) {
            float d = query[i] - v[i];
            dist += d * d;
        }
        ep_dist = dist;
    }
    faiss_hnsw_minimax_heap_push(heap, ep, ep_dist);
    faiss_hnsw_visited_table_set(visited, ep);

    // 5. beam search：堆保留 ef 个最近候选（results），不从中 pop（避免丢失）
    //    通过 expanded[] 标记"已扩展"，线性扫描找下一个未扩展的最小候选
    while (1) {
        // 5a. 在堆中找未扩展的最小距离候选
        int32_t cur_id = -1;
        float cur_dist = FLT_MAX;
        for (int32_t i = 0; i < heap->k; i++) {
            int32_t id = heap->ids[i];
            if (id < 0) continue;  // pop_min 留下的 -1 槽位
            if (id >= vt_size) continue;
            if (expanded[id]) continue;  // 已扩展
            if (heap->dis[i] < cur_dist) {
                cur_dist = heap->dis[i];
                cur_id = id;
            }
        }

        // 没有未扩展候选，停止
        if (cur_id < 0) break;

        // 5b. 终止条件：当前最小未扩展 >= 堆顶最大距离（无改进空间）
        //     注意：堆已满（k >= ef）才有意义；堆未满时仍需继续扩展
        if (heap->k >= ef && cur_dist >= heap->dis[0]) {
            break;
        }

        // 5c. 标记为已扩展
        expanded[cur_id] = 1;

        // 5d. 扩展其邻居
        int32_t neighbor_count = get_neighbor_count(idx, cur_id, level);
        for (int32_t i = 0; i < neighbor_count; i++) {
            int32_t nbr = get_neighbor(idx, cur_id, level, i);
            if (nbr < 0) {
                continue;
            }
            // 边界检查：避免访问未分配的 vec_id
            if (nbr >= vt_size) {
                continue;
            }
            // 跳过已加入堆的节点
            if (faiss_hnsw_visited_table_get(visited, nbr)) {
                continue;
            }

            faiss_hnsw_visited_table_set(visited, nbr);
            float nbr_dist = faiss_hnsw_compute_distance(idx, query, nbr);

            // push 内部自动处理堆满时的替换（保留 ef 个最近候选）
            faiss_hnsw_minimax_heap_push(heap, nbr, nbr_dist);
        }
    }

    // 6. 收集结果：从堆中 pop 所有候选按距离升序
    int32_t result_count = 0;
    while (result_count < result_capacity && heap->k > 0) {
        float dist = 0.0f;
        int32_t id = faiss_hnsw_minimax_heap_pop_min(heap, &dist);
        if (id >= 0) {
            result_ids[result_count] = id;
            result_dist[result_count] = dist;
            result_count++;
        }
    }

    // 填充剩余位置
    for (int32_t i = result_count; i < result_capacity; i++) {
        result_ids[i] = -1;
        result_dist[i] = FLT_MAX;
    }

    // 7. 清理
    free(expanded);
    faiss_hnsw_minimax_heap_drop(heap);
    faiss_hnsw_visited_table_drop(visited);

    return result_count;
}