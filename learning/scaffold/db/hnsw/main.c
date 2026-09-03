/**
 * @file main.c
 * @brief HNSW（Hierarchical Navigable Small World）索引原理演示
 *
 * 本程序演示 HNSW 图索引的核心概念：
 * - 分层图结构（上层稀疏，下层密集）
 * - 贪心搜索（从高层逐层下降）
 * - 插入算法（选邻居 + 双向连边）
 * - 扁平化邻接表布局
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/* ============================================================
 * 常量定义
 * ============================================================ */

#define MAX_DIM         4       /* 向量维度（演示用简化为 4D） */
#define MAX_NODES       256     /* 最大节点数 */
#define MAX_LAYER       8       /* 最大层数（实际由指数分布决定） */
#define HNSW_M           4       /* 每层最大邻居数（底层 2*HNSW_M） */
#define EF_CONSTRUCTION 16      /* 构图搜索候选数 */
#define EF_SEARCH       10      /* 搜索候选数 */

/* ============================================================
 * 距离度量
 * ============================================================ */

typedef enum {
    METRIC_L2 = 0,              /* 欧氏距离 */
    METRIC_IP = 1               /* 内积（余弦相似度近似） */
} DistanceMetric;

/* ============================================================
 * HNSW 节点
 * ============================================================ */

/**
 * @brief 向量节点
 */
typedef struct HNSWNode {
    float           vector[MAX_DIM];   /* 向量数据 */
    int             level;             /* 该节点的层高（1 ~ max_layer） */
    int             max_neighbors;     /* 底层为 2*m_param，其他层为 m_param */
    int             neighbor_count;    /* 当前邻居数 */
    int             neighbors[MAX_LAYER * HNSW_M];  /* 各层邻居（简化：共用数组） */
    int             layer_offset;      /* 各层邻居在本数组中的起始偏移 */
} HNSWNode;

/**
 * @brief HNSW 索引结构
 */
typedef struct HNSWIndex {
    HNSWNode        *nodes;            /* 节点数组 */
    int             num_nodes;         /* 已插入节点数 */
    int             max_nodes;         /* 最大节点容量 */
    int             max_level;         /* 当前图的最大层 */
    int             entry_point;       /* 搜索入口节点 ID */
    int             dim;               /* 向量维度 */
    int             m_param;                /* 邻居参数（每层最大邻居数） */
    int             ef_construction;   /* 构图候选数 */
    int             ef_search;         /* 搜索候选数 */
    DistanceMetric  metric;            /* 距离度量 */
} HNSWIndex;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/**
 * 计算两个向量的欧氏距离（L2）
 */
static float euclidean_distance(const float *a, const float *b, int dim) {
    float sum = 0.0f;
    for (int i = 0; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    return sqrtf(sum);
}

/**
 * 计算两个向量的内积（IP）
 */
static float inner_product(const float *a, const float *b, int dim) {
    float sum = 0.0f;
    for (int i = 0; i < dim; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * 计算向量距离
 */
static float compute_distance(const float *a, const float *b, int dim, DistanceMetric metric) {
    if (metric == METRIC_L2) {
        return euclidean_distance(a, b, dim);
    } else {
        return -inner_product(a, b, dim);  /* 取负因为搜索要找最大值 */
    }
}

/**
 * 指数分布采样：决定节点的层高
 * 概率 P(level > L) = exp(-L / lambda)
 * 实际实现：对均匀随机数查概率累积表
 */
static int sample_level(HNSWIndex *idx) {
    double f = (double)rand() / (double)RAND_MAX;
    double lambda = 1.0 / log((double)HNSW_M);

    for (int level = 0; level < MAX_LAYER; level++) {
        double prob = exp(-level / lambda);
        if (f < prob) {
            return level;
        }
        f -= prob;
    }
    return MAX_LAYER - 1;
}

/**
 * 获取某节点在某层的邻居数量上限
 */
static int get_layer_capacity(HNSWIndex *idx, int level) {
    return (level == 0) ? idx->m_param * 2 : idx->m_param;
}

/* ============================================================
 * HNSW 操作实现
 * ============================================================ */

/**
 * 创建 HNSW 索引
 */
HNSWIndex *hnsw_create(int max_nodes, int dim, int M, int ef_c, int ef_s, DistanceMetric metric) {
    HNSWIndex *idx = (HNSWIndex *)calloc(1, sizeof(HNSWIndex));
    if (!idx) return NULL;

    idx->nodes = (HNSWNode *)calloc(max_nodes, sizeof(HNSWNode));
    if (!idx->nodes) {
        free(idx);
        return NULL;
    }

    idx->max_nodes = max_nodes;
    idx->num_nodes = 0;
    idx->max_level = 0;
    idx->entry_point = -1;
    idx->dim = dim;
    idx->m_param = HNSW_M;
    idx->ef_construction = ef_c;
    idx->ef_search = ef_s;
    idx->metric = metric;

    return idx;
}

/**
 * 贪心下降搜索：从入口点出发，在给定层找到最近的邻居
 */
static int greedy_search_layer(HNSWIndex *idx, const float *query, int level) {
    if (idx->entry_point < 0) return -1;

    int nearest = idx->entry_point;
    float d_nearest = compute_distance(query, idx->nodes[nearest].vector, idx->dim, idx->metric);

    int max_iter = 100;
    for (int iter = 0; iter < max_iter; iter++) {
        int prev_nearest = nearest;
        HNSWNode *node = &idx->nodes[nearest];

        /* 遍历该层所有邻居 */
        int capacity = get_layer_capacity(idx, level);
        for (int i = 0; i < node->neighbor_count && i < capacity; i++) {
            int neighbor = node->neighbors[i];
            if (neighbor < 0) break;

            float dist = compute_distance(query, idx->nodes[neighbor].vector, idx->dim, idx->metric);
            if (dist < d_nearest) {
                nearest = neighbor;
                d_nearest = dist;
            }
        }

        /* 本层已收敛 */
        if (nearest == prev_nearest) break;
    }

    printf("[hnsw] 层 %d 贪心下降: 入口=%d, 最近=%d, 距离=%.3f\n", level, idx->entry_point, nearest, d_nearest);
    return nearest;
}

/**
 * 搜索最近邻（简化版：只做贪心下降，无候选堆）
 */
int *hnsw_search(HNSWIndex *idx, const float *query, int k, float *distances) {
    if (!idx || idx->num_nodes == 0) return NULL;

    printf("\n[hnsw] ====== 搜索开始 ======\n");
    printf("[hnsw] 查询向量: [%.2f, %.2f, %.2f, %.2f]\n", query[0], query[1], query[2], query[3]);

    /* 第一阶段：从高层到底层逐层贪心下降 */
    int current = idx->entry_point;
    for (int level = idx->max_level; level > 0; level--) {
        current = greedy_search_layer(idx, query, level);
        if (current < 0) current = idx->entry_point;
    }

    /* 第二阶段：第 0 层搜索（简化为直接找最近邻） */
    float best_dist = FLT_MAX;
    int best_id = 0;
    for (int i = 0; i < idx->num_nodes; i++) {
        float dist = compute_distance(query, idx->nodes[i].vector, idx->dim, idx->metric);
        if (dist < best_dist) {
            best_dist = dist;
            best_id = i;
        }
    }

    printf("[hnsw] 第 0 层搜索完成: 找到最近邻 ID=%d, 距离=%.3f\n", best_id, best_dist);
    printf("[hnsw] ====== 搜索结束 ======\n\n");

    static int result[1];
    result[0] = best_id;
    if (distances) distances[0] = best_dist;
    return result;
}

/**
 * 插入节点到 HNSW 图
 */
int hnsw_insert(HNSWIndex *idx, const float *vector) {
    if (!idx || idx->num_nodes >= idx->max_nodes) return -1;

    int pt_id = idx->num_nodes;
    HNSWNode *node = &idx->nodes[pt_id];

    /* 复制向量数据 */
    memcpy(node->vector, vector, idx->dim * sizeof(float));

    /* 采样层高 */
    node->level = sample_level(idx) + 1;  /* level >= 1 */
    if (node->level > idx->max_level) {
        idx->max_level = node->level - 1;
    }
    node->neighbor_count = 0;
    node->max_neighbors = get_layer_capacity(idx, 0);

    printf("\n[hnsw] ====== 插入节点 %d ======\n", pt_id);
    printf("[hnsw] 向量: [%.2f, %.2f, %.2f, %.2f]\n", vector[0], vector[1], vector[2], vector[3]);
    printf("[hnsw] 随机层高: %d\n", node->level);

    /* 第一个节点：直接作为入口点 */
    if (pt_id == 0) {
        idx->entry_point = 0;
        idx->num_nodes++;
        printf("[hnsw] 首个节点，成为入口点\n");
        printf("[hnsw] ====== 插入完成 ======\n\n");
        return 0;
    }

    /* 从高层到底层逐层插入 */
    for (int level = idx->max_level; level >= 0; level--) {
        int nearest = greedy_search_layer(idx, vector, level);

        /* 双向连边：简化版，只连接贪心找到的最近邻 */
        if (nearest >= 0 && node->neighbor_count < node->max_neighbors) {
            node->neighbors[node->neighbor_count++] = nearest;
            idx->nodes[nearest].neighbors[idx->nodes[nearest].neighbor_count++] = pt_id;
            printf("[hnsw] 层 %d: 连接 (%d <-> %d)\n", level, pt_id, nearest);
        }
    }

    /* 如果新节点层高大于当前最大层，更新入口点 */
    if (node->level - 1 > idx->max_level) {
        idx->entry_point = pt_id;
        printf("[hnsw] 新节点成为最高层入口点\n");
    }

    idx->num_nodes++;
    printf("[hnsw] ====== 插入完成 ======\n\n");
    return 0;
}

/**
 * 打印索引状态
 */
void hnsw_print_stats(HNSWIndex *idx) {
    printf("\n[hnsw] ====== 索引统计 ======\n");
    printf("[hnsw] 节点数: %d / %d\n", idx->num_nodes, idx->max_nodes);
    printf("[hnsw] 最大层: %d\n", idx->max_level);
    printf("[hnsw] 入口点: %d\n", idx->entry_point);
    printf("[hnsw] 参数 m_param=%d, ef_c=%d, ef_s=%d\n", idx->m_param, idx->ef_construction, idx->ef_search);

    /* 打印每层节点分布 */
    int *layer_count = calloc(idx->max_level + 1, sizeof(int));
    for (int i = 0; i < idx->num_nodes; i++) {
        int l = idx->nodes[i].level - 1;
        if (l >= 0 && l <= idx->max_level) {
            layer_count[l]++;
        }
    }
    printf("[hnsw] 各层节点数: ");
    for (int i = 0; i <= idx->max_level; i++) {
        printf("L%d=%d ", i, layer_count[i]);
    }
    printf("\n[hnsw] ====== 统计结束 ======\n\n");
    free(layer_count);
}

/**
 * 销毁 HNSW 索引
 */
void hnsw_destroy(HNSWIndex *idx) {
    if (idx) {
        free(idx->nodes);
        free(idx);
    }
}

/* ============================================================
 * 主函数：演示 HNSW 索引操作
 * ============================================================ */

int main(void) {
    printf("========================================\n");
    printf("      HNSW 索引原理演示\n");
    printf("========================================\n");

    /* 创建索引 */
    HNSWIndex *idx = hnsw_create(MAX_NODES, MAX_DIM, HNSW_M, EF_CONSTRUCTION, EF_SEARCH, METRIC_L2);

    printf("\n--- 1. 分层图结构说明 ---\n");
    printf("[hnsw] HNSW 使用分层图结构:\n");
    printf("  - 上层图稀疏：节点少，连边少，搜索跳得快\n");
    printf("  - 底层图密集：所有节点都在第 0 层，搜索更精确\n");
    printf("  - 节点层高由指数分布决定，越高层概率越低\n");
    printf("  - 搜索时从最高层入口点逐层贪心下降\n");

    printf("\n--- 2. 插入演示 ---\n");
    /* 插入一组 4D 向量 */
    float vectors[][MAX_DIM] = {
        {1.0f, 2.0f, 1.0f, 0.5f},   /* 节点 0 */
        {4.0f, 5.0f, 4.5f, 4.0f},   /* 节点 1 */
        {1.2f, 2.1f, 1.1f, 0.6f},   /* 节点 2（接近节点 0） */
        {4.2f, 5.1f, 4.4f, 4.1f},   /* 节点 3（接近节点 1） */
        {8.0f, 9.0f, 8.5f, 8.0f},   /* 节点 4（远离其他节点） */
        {1.1f, 2.2f, 1.0f, 0.4f},   /* 节点 5（接近节点 0） */
        {3.8f, 4.9f, 4.3f, 3.9f},   /* 节点 6（接近节点 1） */
    };

    int n = sizeof(vectors) / sizeof(vectors[0]);
    for (int i = 0; i < n; i++) {
        hnsw_insert(idx, vectors[i]);
    }

    /* 打印索引统计 */
    hnsw_print_stats(idx);

    printf("--- 3. 搜索演示 ---\n");
    /* 搜索接近节点 0 的向量 */
    float query1[MAX_DIM] = {1.0f, 2.0f, 1.0f, 0.5f};
    float dist1;
    int *result1 = hnsw_search(idx, query1, 1, &dist1);
    if (result1) {
        printf("[hnsw] Top-1 结果: ID=%d, 距离=%.3f\n", result1[0], dist1);
    }

    /* 搜索接近节点 1 的向量 */
    float query2[MAX_DIM] = {4.0f, 5.0f, 4.5f, 4.0f};
    float dist2;
    int *result2 = hnsw_search(idx, query2, 1, &dist2);
    if (result2) {
        printf("[hnsw] Top-1 结果: ID=%d, 距离=%.3f\n", result2[0], dist2);
    }

    printf("--- 4. 插入算法说明 ---\n");
    printf("[hnsw] 插入流程:\n");
    printf("  1. 随机采样层高（指数分布）\n");
    printf("  2. 从入口点出发，逐层贪心下降找到插入位置\n");
    printf("  3. 在每层搜索 ef_construction 个最近邻居\n");
    printf("  4. 启发式选边（shrink）：避免邻居相互遮挡\n");
    printf("  5. 双向连接新节点与选中的邻居\n");

    printf("\n========================================\n");
    printf("      演示完成\n");
    printf("========================================\n");

    hnsw_destroy(idx);
    return 0;
}
