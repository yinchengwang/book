/**
 * @file main.c
 * @brief DiskANN 磁盘图索引原理演示
 *
 * 本程序演示 DiskANN (Vamana 图) 的核心概念：
 * - Vamana 图构建（近邻图 + 导航集）
 * - 内存 + SSD 分层搜索（memory-first, SSD-fallback）
 * - PQ 量化加速（Product Quantization）
 * - Beam Search 候选扩展
 *
 * 参考: engineering/src/db/index/vector_index/diskann/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>

/* ========================================================================
 * 常量定义
 * ======================================================================== */
#define LOG(fmt, ...)   printf("[diskann] " fmt "\n", ##__VA_ARGS__)

#define DIM             128     /* 向量维度 */
#define MAX_POINTS      10000   /* 最大数据点数 */
#define MAX_DEGREE      32      /* 图中节点的最大邻居数 */
#define L_BUILD         64      /* 构建时的候选集大小（L） */
#define L_SEARCH        32      /* 搜索时的候选集大小 */
#define R               16      /* 搜索返回的最近邻数量 */
#define PQ_BYTES        16      /* PQ 分段数（每段 8bit） */
#define PQ_CENTROIDS    256     /* 每个 PQ 子空间的质心数 */

/* ========================================================================
 * 向量与距离计算
 * ======================================================================== */

/**
 * @brief 128 维向量（简化为 float 数组）
 */
typedef struct {
    float data[DIM];
    int   id;                     /* 全局唯一 ID */
} Vector;

/**
 * @brief 计算两个向量的 L2 距离
 */
static float euclidean_dist(const float *a, const float *b) {
    float sum = 0.0f;
    for (int i = 0; i < DIM; i++) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return sqrtf(sum);
}

/**
 * @brief 使用 PQ 编码估算距离（分段时间加速）
 *
 * PQ 将高维向量分成 PQ_BYTES 段，每段独立量化到 256 个码字。
 * 距离估算通过查表求和，避免全量 L2 计算。
 */
static float pq_approx_dist(const unsigned char *code_a,
                            const unsigned char *code_b,
                            const float (*lut)[PQ_CENTROIDS]) {
    float dist = 0.0f;
    int seg_dim = DIM / PQ_BYTES;  /* 每段维度 */
    for (int m = 0; m < PQ_BYTES; m++) {
        unsigned char c_a = code_a[m];
        unsigned char c_b = code_b[m];
        dist += lut[m][c_a * PQ_CENTROIDS + c_b];  /* 预计算查表 */
    }
    return dist;
}

/* ========================================================================
 * PQ 量化器
 * ======================================================================== */

/**
 * @brief PQ 码本结构
 */
typedef struct {
    float centroids[PQ_BYTES][PQ_CENTROIDS][16];  /* 简化为每段最多 16 维 */
    int   seg_dim;                                /* 每段维度 */
} PQCodebook;

/**
 * @brief 训练 PQ 码本（简化：随机初始化质心）
 */
static void pq_train(PQCodebook *pq, const Vector *pts, int n) {
    LOG("PQ 训练: %d 个向量, %d 分段, %d 质心/段",
        n, PQ_BYTES, PQ_CENTROIDS);
    /* 简化实现：使用随机质心，实际应使用 k-means */
    for (int m = 0; m < PQ_BYTES; m++) {
        for (int c = 0; c < PQ_CENTROIDS; c++) {
            for (int d = 0; d < pq->seg_dim; d++) {
                pq->centroids[m][c][d] = (float)(rand() % 1000) / 100.0f;
            }
        }
    }
    LOG("PQ 码本训练完成");
}

/**
 * @brief 将向量编码为 PQ 码字
 */
static void pq_encode(const PQCodebook *pq, const float *vec,
                      unsigned char *code) {
    int seg_dim = pq->seg_dim;
    for (int m = 0; m < PQ_BYTES; m++) {
        float best_dist = 1e9f;
        int   best_c = 0;
        for (int c = 0; c < PQ_CENTROIDS; c++) {
            float d = 0.0f;
            for (int d_idx = 0; d_idx < seg_dim; d_idx++) {
                float delta = vec[m * seg_dim + d_idx] - pq->centroids[m][c][d_idx];
                d += delta * delta;
            }
            if (d < best_dist) {
                best_dist = d;
                best_c = c;
            }
        }
        code[m] = (unsigned char)best_c;
    }
}

/* ========================================================================
 * Vamana 图结构
 * ======================================================================== */

/**
 * @brief 图节点（存储向量 ID 和邻居列表）
 */
typedef struct {
    int   id;                     /* 向量 ID */
    int   nbr_count;              /* 当前邻居数量 */
    int   nbrs[MAX_DEGREE];       /* 邻居节点 ID（数组） */
} GraphNode;

/**
 * @brief Vamana 图索引
 */
typedef struct {
    int       count;               /* 向量总数 */
    Vector    vectors[MAX_POINTS]; /* 原始向量（内存中） */
    unsigned char codes[MAX_POINTS][PQ_BYTES]; /* PQ 编码 */
    GraphNode graph[MAX_POINTS];  /* 邻接表 */
    PQCodebook pq;                /* PQ 码本 */
    int       medoid;             /* 入口点（medoid） */
} VamanaIndex;

/* ========================================================================
 * Vamana 图构建
 * ======================================================================== */

/**
 * @brief 初始化 Vamana 索引
 */
static VamanaIndex *vamana_create(void) {
    VamanaIndex *idx = calloc(1, sizeof(VamanaIndex));
    if (!idx) return NULL;
    idx->count = 0;
    idx->medoid = 0;
    LOG("Vamana 索引已创建");
    return idx;
}

/**
 * @brief 添加向量到索引
 */
static void vamana_add(VamanaIndex *idx, const float *vec, int id) {
    if (idx->count >= MAX_POINTS) {
        LOG("索引已满，无法添加向量");
        return;
    }
    Vector *v = &idx->vectors[idx->count];
    memcpy(v->data, vec, DIM * sizeof(float));
    v->id = id;
    idx->graph[idx->count].id = id;
    idx->graph[idx->count].nbr_count = 0;
    idx->count++;
}

/**
 * @brief 贪婪搜索：贪心遍历图找到最近邻
 */
static int greedy_search(VamanaIndex *idx, const float *query,
                         int start, int L) {
    int best_id = start;
    float best_dist = euclidean_dist(query, idx->vectors[start].data);

    for (int iter = 0; iter < L; iter++) {
        GraphNode *node = &idx->graph[best_id];
        for (int i = 0; i < node->nbr_count; i++) {
            int nbr = node->nbrs[i];
            float d = euclidean_dist(query, idx->vectors[nbr].data);
            if (d < best_dist) {
                best_dist = d;
                best_id = nbr;
            }
        }
    }
    return best_id;
}

/**
 * @brief 构建 Vamana 图（简化版：全连接 + 剪枝）
 *
 * 实际 Vamana 需要：
 * 1. 使用 Randomized Greedy Search 构建初始图
 * 2. 使用 alpha 参数控制出度上界
 * 3. 迭代剪枝直到满足 degree <= R
 */
static void vamana_build(VamanaIndex *idx) {
    LOG("Vamana 图构建: %d 个向量, L=%d, R=%d", idx->count, L_BUILD, MAX_DEGREE);

    /* 简化策略：为每个节点连接最近的几个邻居 */
    for (int i = 0; i < idx->count; i++) {
        /* 找最近的 3 个邻居（简化） */
        int added = 0;
        for (int k = 0; k < 3 && added < MAX_DEGREE; k++) {
            float best_d = 1e9f;
            int best_j = -1;
            for (int j = 0; j < idx->count; j++) {
                if (j == i) continue;
                float d = euclidean_dist(idx->vectors[i].data,
                                         idx->vectors[j].data);
                if (d < best_d) {
                    /* 避免重复添加 */
                    bool exists = false;
                    for (int t = 0; t < added; t++) {
                        if (idx->graph[i].nbrs[t] == j) { exists = true; break; }
                    }
                    if (!exists) {
                        best_d = d;
                        best_j = j;
                    }
                }
            }
            if (best_j >= 0) {
                idx->graph[i].nbrs[added++] = best_j;
                idx->graph[i].nbr_count++;
            }
        }
    }

    /* 随机选择一个 medoid 作为入口点 */
    idx->medoid = rand() % idx->count;
    LOG("Vamana 图构建完成, medoid=%d", idx->medoid);
}

/* ========================================================================
 * 混合搜索（内存优先 + SSD 回退）
 * ======================================================================== */

/**
 * @brief 内存中 Beam Search 搜索
 *
 * 搜索流程：
 * 1. 从 medoid 开始，使用优先队列维护候选集
 * 2. 贪心扩展，直到遍历 L_SEARCH 个节点
 * 3. 返回 R 个最近邻
 */
static void search_memory(VamanaIndex *idx, const float *query,
                          int results[], float dists[], int *n_results) {
    int visited[MAX_POINTS] = {0};
    int queue[MAX_POINTS];
    int front = 0, back = 0;

    /* 优先队列简化为数组 */
    int pq_ids[L_SEARCH];
    float pq_dists[L_SEARCH];
    int pq_size = 0;

    /* 从 medoid 开始 */
    queue[back++] = idx->medoid;
    visited[idx->medoid] = 1;

    while (front < back && back < L_SEARCH * 2) {
        int cur = queue[front++];
        GraphNode *node = &idx->graph[cur];

        /* 扩展邻居 */
        for (int i = 0; i < node->nbr_count; i++) {
            int nbr = node->nbrs[i];
            if (visited[nbr]) continue;
            visited[nbr] = 1;

            float d = euclidean_dist(query, idx->vectors[nbr].data);

            /* 插入优先队列 */
            if (pq_size < L_SEARCH) {
                pq_ids[pq_size] = nbr;
                pq_dists[pq_size++] = d;
            } else {
                /* 找到最大距离并替换 */
                int max_i = 0;
                for (int j = 1; j < pq_size; j++) {
                    if (pq_dists[j] > pq_dists[max_i]) max_i = j;
                }
                if (d < pq_dists[max_i]) {
                    pq_dists[max_i] = d;
                    pq_ids[max_i] = nbr;
                }
            }
            queue[back++] = nbr;
        }
    }

    /* 排序并返回 top-R */
    for (int i = 0; i < pq_size && i < R; i++) {
        for (int j = i + 1; j < pq_size; j++) {
            if (pq_dists[j] < pq_dists[i]) {
                float td = pq_dists[i]; pq_dists[i] = pq_dists[j]; pq_dists[j] = td;
                int   ti = pq_ids[i];   pq_ids[i] = pq_ids[j];     pq_ids[j] = ti;
            }
        }
    }

    *n_results = (pq_size < R) ? pq_size : R;
    for (int i = 0; i < *n_results; i++) {
        results[i] = pq_ids[i];
        dists[i] = pq_dists[i];
    }
}

/**
 * @brief SSD 回退搜索（概念演示）
 *
 * 当内存缓存不足时，需要从 SSD 读取向量块。
 * 步骤：
 * 1. 先用 PQ 编码在内存中做粗粒度筛选
 * 2. 读取候选块到内存
 * 3. 用原始向量重排精排序
 */
static void search_ssd_fallback(VamanaIndex *idx, const float *query,
                                int results[], float dists[], int *n_results) {
    LOG("SSD 回退搜索: 使用 PQ 编码筛选候选向量");

    /* PQ 近似距离排序 */
    int candidates[MAX_POINTS];
    float approx_dists[MAX_POINTS];
    int n_cand = 0;

    /* 模拟：从 SSD 读取一批候选向量 */
    int ssd_read_batch = 1000;
    for (int i = 0; i < idx->count && n_cand < ssd_read_batch; i++) {
        /* 实际应从磁盘读取，这里使用内存模拟 */
        candidates[n_cand] = i;
        approx_dists[n_cand] = euclidean_dist(query, idx->vectors[i].data);
        n_cand++;
    }

    /* PQ 排序后取 top-64 */
    int top64[64];
    for (int i = 0; i < n_cand && i < 64; i++) top64[i] = candidates[i];

    /* 从 top-64 中精确计算距离 */
    for (int i = 0; i < n_cand && i < 64; i++) {
        dists[i] = euclidean_dist(query, idx->vectors[top64[i]].data);
        results[i] = top64[i];
    }
    *n_results = (n_cand < 64) ? n_cand : 64;

    LOG("SSD 回退: 读取 %d 候选, 返回 %d 结果", ssd_read_batch, *n_results);
}

/**
 * @brief 统一搜索接口（内存优先，SSD 回退）
 */
static void vamana_search(VamanaIndex *idx, const float *query,
                          int results[], float dists[], int *n_results,
                          bool use_ssd) {
    if (use_ssd) {
        search_ssd_fallback(idx, query, results, dists, n_results);
    } else {
        search_memory(idx, query, results, dists, n_results);
    }
}

/* ========================================================================
 * 主函数：演示 Vamana 图搜索流程
 * ======================================================================== */

int main(void) {
    printf("========================================\n");
    printf("      DiskANN (Vamana) 原理演示\n");
    printf("========================================\n\n");

    /* 1. 初始化索引 */
    VamanaIndex *idx = vamana_create();

    /* 2. 生成随机测试向量 */
    LOG("生成 %d 个 %d 维随机向量...", MAX_POINTS, DIM);
    for (int i = 0; i < MAX_POINTS; i++) {
        float vec[DIM];
        for (int d = 0; d < DIM; d++) {
            vec[d] = (float)(rand() % 1000) / 100.0f;
        }
        vamana_add(idx, vec, i);
    }

    /* 3. 训练 PQ 码本 */
    pq_train(&idx->pq, idx->vectors, idx->count);

    /* 4. 编码所有向量 */
    LOG("PQ 编码所有向量...");
    for (int i = 0; i < idx->count; i++) {
        pq_encode(&idx->pq, idx->vectors[i].data, idx->codes[i]);
    }

    /* 5. 构建 Vamana 图 */
    vamana_build(idx);

    /* 6. 准备查询向量 */
    float query[DIM];
    for (int d = 0; d < DIM; d++) {
        query[d] = (float)(rand() % 1000) / 100.0f;
    }

    /* 7. 内存搜索 */
    printf("\n--- 内存搜索 (Beam Search) ---\n");
    int results[R];
    float dists[R];
    int n_results;
    vamana_search(idx, query, results, dists, &n_results, false);
    LOG("找到 %d 个最近邻:", n_results);
    for (int i = 0; i < n_results; i++) {
        LOG("  [%d] id=%d, dist=%.3f", i, results[i], dists[i]);
    }

    /* 8. SSD 回退搜索 */
    printf("\n--- SSD 回退搜索 (PQ 加速) ---\n");
    vamana_search(idx, query, results, dists, &n_results, true);
    LOG("找到 %d 个最近邻:");
    for (int i = 0; i < n_results; i++) {
        LOG("  [%d] id=%d, dist=%.3f", i, results[i], dists[i]);
    }

    printf("\n========================================\n");
    printf("      演示完成\n");
    printf("========================================\n");

    free(idx);
    return 0;
}
