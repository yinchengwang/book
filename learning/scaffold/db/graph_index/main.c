/**
 * @file main.c
 * @brief 图索引原理演示：NSW + HNSW + 搜索剪枝
 *
 * 功能概览：
 * 1. NSW（Navigable Small World）：单层近邻图，贪心搜索
 * 2. HNSW（Hierarchical NSW）：多层跳跃，贪心下降 + 扩展剪枝
 * 3. 搜索剪枝：候选队列 + 距离下界 + 提前终止
 *
 * 编译：gcc -std=c11 -Wall -O2 -o test main.c -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include <time.h>

/* ============================================================
 * 常量与宏
 * ============================================================ */

#define DIM                 4       /* 向量维度 */
#define MAX_NODES           1024    /* 最大节点数 */
#define MAX_NEIGHBORS       3       /* 每节点最大邻居数 */
#define MAX_LEVEL           6       /* HNSW 最大层数 */
#define EF_SEARCH           16      /* 搜索候选队列大小 */
#define EF_CONSTRUCTION     16      /* 构图邻居数 */

#define LOG(fmt, ...)       printf("[graph] " fmt "\n", ##__VA_ARGS__)

/* ============================================================
 * 数据结构
 * ============================================================ */

/** 向量节点 */
typedef struct {
    int     id;                     /* 节点 ID */
    float   vec[DIM];               /* 向量 */
    int     level;                  /* 层高（仅 HNSW） */
    int     neighbors[MAX_NEIGHBORS];
    int     num_neighbors;
} VectorNode;

/** 图索引 */
typedef struct {
    VectorNode  nodes[MAX_NODES];
    int         count;              /* 已插入节点数 */
    int         entry_point;        /* 入口节点 */
    bool        hierarchical;       /* 是否 HNSW 模式 */
} GraphIndex;

/* ============================================================
 * 辅助函数
 * ============================================================ */

/** 计算欧氏距离 */
static float euclidean_dist(const float *a, const float *b) {
    float sum = 0.0f;
    for (int i = 0; i < DIM; i++) {
        float d = a[i] - b[i];
        sum += d * d;
    }
    return sqrtf(sum);
}

/** 采样层高（指数分布） */
static int random_level(void) {
    int level = 0;
    while (level < MAX_LEVEL - 1 && (float)rand() / RAND_MAX < 0.5f) {
        level++;
    }
    return level;
}

/* ============================================================
 * NSW：单层近邻图
 * ============================================================ */

static void nsw_init(GraphIndex *idx) {
    memset(idx, 0, sizeof(GraphIndex));
    idx->hierarchical = false;
}

/**
 * NSW 贪心搜索：遍历所有节点，按距离排序取 top-k
 */
static void nsw_search(GraphIndex *idx, const float *query,
                        int k, int *results, int *actual_k) {
    typedef struct { int id; float dist; } Cand;
    Cand cands[MAX_NODES];

    for (int i = 0; i < idx->count; i++) {
        cands[i].id = i;
        cands[i].dist = euclidean_dist(query, idx->nodes[i].vec);
    }

    /* 选择排序取 top-k */
    int found = 0;
    for (int iter = 0; iter < idx->count && found < k; iter++) {
        int best = iter;
        for (int j = iter + 1; j < idx->count; j++) {
            if (cands[j].dist < cands[best].dist) best = j;
        }
        if (best != iter) {
            Cand tmp = cands[iter]; cands[iter] = cands[best]; cands[best] = tmp;
        }
        results[found++] = cands[iter].id;
    }
    *actual_k = found;
}

/**
 * NSW 插入：贪心找最近邻居，双向连边
 */
static void nsw_insert(GraphIndex *idx, const float *vec) {
    int nid = idx->count;
    VectorNode *node = &idx->nodes[nid];
    node->id = nid;
    memcpy(node->vec, vec, DIM * sizeof(float));
    node->num_neighbors = 0;

    if (nid == 0) {
        idx->entry_point = 0;
        idx->count = 1;
        return;
    }

    /* 贪心找最近邻居 */
    int cands[EF_CONSTRUCTION];
    int num_cands = 0;
    nsw_search(idx, vec, EF_CONSTRUCTION, cands, &num_cands);

    int num_conn = (num_cands < MAX_NEIGHBORS) ? num_cands : MAX_NEIGHBORS;
    for (int i = 0; i < num_conn; i++) {
        int nid2 = cands[i];
        if (node->num_neighbors < MAX_NEIGHBORS)
            node->neighbors[node->num_neighbors++] = nid2;
        VectorNode *n2 = &idx->nodes[nid2];
        if (n2->num_neighbors < MAX_NEIGHBORS)
            n2->neighbors[n2->num_neighbors++] = nid;
    }
    idx->entry_point = cands[0];
    idx->count++;
}

/* ============================================================
 * HNSW：分层近邻图
 * ============================================================ */

static void hnsw_init(GraphIndex *idx) {
    memset(idx, 0, sizeof(GraphIndex));
    idx->hierarchical = true;
}

/**
 * 贪心下降：沿当前层遍历邻居，找更近的点
 */
static int hnsw_greedy_descent(GraphIndex *idx, const float *query, int target_level) {
    int cur = idx->entry_point;
    float best = euclidean_dist(query, idx->nodes[cur].vec);
    bool improved = true;

    while (improved) {
        improved = false;
        VectorNode *n = &idx->nodes[cur];
        for (int i = 0; i < n->num_neighbors; i++) {
            int nid = n->neighbors[i];
            if (idx->nodes[nid].level > target_level) continue;
            float d = euclidean_dist(query, idx->nodes[nid].vec);
            if (d < best) {
                best = d; cur = nid; improved = true;
            }
        }
    }
    return cur;
}

/**
 * 第 0 层扩展搜索：候选队列 + visited 去重 + 提前终止
 *
 * 剪枝策略：
 * 1. visited 集合：每个节点最多入队一次，避免重复扩展
 * 2. 距离下界：结果集最远距离作为剪枝阈值
 * 3. efSearch 上限：超过后提前终止（精度换速度）
 */
static void hnsw_search_layer0(GraphIndex *idx, const float *query,
                               int k, int *results, int *actual_k) {
    typedef struct { int id; float dist; } Cand;
    Cand queue[MAX_NODES];
    bool visited[MAX_NODES] = { false };
    Cand results_list[MAX_NODES];
    int qh = 0, qt = 0, rc = 0;

    /* 贪心下降定位起始点 */
    int start = hnsw_greedy_descent(idx, query, 0);
    queue[qt++] = (Cand){ start, euclidean_dist(query, idx->nodes[start].vec) };
    visited[start] = true;

    /* 扩展搜索 */
    while (qh < qt && rc < EF_SEARCH) {
        /* 找队列中最近的候选 */
        int best_idx = qh;
        for (int i = qh + 1; i < qt; i++)
            if (queue[i].dist < queue[best_idx].dist) best_idx = i;
        Cand best = queue[best_idx];
        queue[best_idx] = queue[qh++]; /* 移出队列 */

        /* 距离下界剪枝 */
        if (rc >= k && best.dist > results_list[rc - 1].dist) break;

        results_list[rc++] = best;

        /* 遍历邻居，加入候选队列 */
        VectorNode *n = &idx->nodes[best.id];
        for (int i = 0; i < n->num_neighbors; i++) {
            int nid = n->neighbors[i];
            if (!visited[nid]) {
                visited[nid] = true;
                queue[qt++] = (Cand){ nid, euclidean_dist(query, idx->nodes[nid].vec) };
            }
        }
    }

    int out_k = (k < rc) ? k : rc;
    for (int i = 0; i < out_k; i++) results[i] = results_list[i].id;
    *actual_k = out_k;
}

/**
 * HNSW 插入：随机层高，从高层到低层逐层连边
 */
static void hnsw_insert(GraphIndex *idx, const float *vec) {
    int nid = idx->count;
    VectorNode *node = &idx->nodes[nid];
    node->id = nid;
    memcpy(node->vec, vec, DIM * sizeof(float));
    node->level = random_level();
    node->num_neighbors = 0;

    if (nid == 0) {
        idx->entry_point = 0;
        idx->count = 1;
        return;
    }

    /* 先在高层做贪心下降（模拟多层搜索） */
    for (int lvl = MAX_LEVEL - 1; lvl > node->level; lvl--)
        (void)hnsw_greedy_descent(idx, vec, lvl);

    /* 在节点的层及以下层进行连接 */
    for (int lvl = node->level; lvl >= 0; lvl--) {
        int nearest = hnsw_greedy_descent(idx, vec, lvl);
        if (node->num_neighbors < MAX_NEIGHBORS)
            node->neighbors[node->num_neighbors++] = nearest;
        VectorNode *n2 = &idx->nodes[nearest];
        if (n2->num_neighbors < MAX_NEIGHBORS)
            n2->neighbors[n2->num_neighbors++] = nid;
    }
    idx->entry_point = hnsw_greedy_descent(idx, vec, MAX_LEVEL - 1);
    idx->count++;
}

/* ============================================================
 * 统一搜索接口
 * ============================================================ */

static void graph_search(GraphIndex *idx, const float *query,
                         int k, int *results, int *actual_k) {
    if (idx->hierarchical) {
        LOG("HNSW 搜索 (ef=%d)", EF_SEARCH);
        hnsw_search_layer0(idx, query, k, results, actual_k);
    } else {
        LOG("NSW 搜索");
        nsw_search(idx, query, k, results, actual_k);
    }
}

/* ============================================================
 * 演示
 * ============================================================ */

int main(void) {
    srand((unsigned)time(NULL));

    printf("========================================\n");
    printf("   图索引演示：NSW + HNSW + 搜索剪枝\n");
    printf("========================================\n\n");

    /* 测试数据 */
    float points[][DIM] = {
        {0.0f, 0.0f, 0.0f, 0.0f},
        {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 0.0f, 0.0f},
        {2.0f, 0.0f, 0.0f, 0.0f},
        {2.0f, 2.0f, 0.0f, 0.0f},
        {0.5f, 0.5f, 0.0f, 0.0f},
        {1.5f, 0.5f, 0.0f, 0.0f},
    };
    int n = sizeof(points) / sizeof(points[0]);

    /* 查询向量 */
    float query[DIM] = {0.6f, 0.6f, 0.0f, 0.0f};

    /* ----- 演示 1：NSW ----- */
    printf("--- 1. NSW（单层近邻图） ---\n");
    GraphIndex nsw;
    nsw_init(&nsw);
    for (int i = 0; i < n; i++) nsw_insert(&nsw, points[i]);

    int results[5];
    int actual_k = 5;
    graph_search(&nsw, query, 5, results, &actual_k);

    printf("查询点 (0.6, 0.6)，结果:\n");
    for (int i = 0; i < actual_k; i++) {
        int id = results[i];
        printf("  [%d] id=%d dist=%.3f\n", i, id,
               euclidean_dist(query, nsw.nodes[id].vec));
    }

    /* ----- 演示 2：HNSW ----- */
    printf("\n--- 2. HNSW（分层近邻图） ---\n");
    GraphIndex hnsw;
    hnsw_init(&hnsw);
    for (int i = 0; i < n; i++) hnsw_insert(&hnsw, points[i]);

    actual_k = 5;
    graph_search(&hnsw, query, 5, results, &actual_k);

    printf("查询点 (0.6, 0.6)，结果:\n");
    for (int i = 0; i < actual_k; i++) {
        int id = results[i];
        printf("  [%d] id=%d dist=%.3f\n", i, id,
               euclidean_dist(query, hnsw.nodes[id].vec));
    }

    /* ----- 演示 3：剪枝原理 ----- */
    printf("\n--- 3. 搜索剪枝原理 ---\n");
    printf("  候选队列限制: visited 集合避免重复扩展\n");
    printf("  距离下界剪枝: 结果集最远距离 > 当前候选时跳过\n");
    printf("  efSearch 上限: 超过阈值后提前终止\n");
    printf("  贪心下降: 高层稀疏图快速定位大致区域\n");

    /* ----- 演示 4：图统计 ----- */
    printf("\n--- 4. 图统计 ---\n");
    int edges = 0, layer_dist[MAX_LEVEL] = {0};
    for (int i = 0; i < nsw.count; i++) edges += nsw.nodes[i].num_neighbors;
    printf("NSW: %d 节点, %d 条边, 平均度=%.2f\n",
           nsw.count, edges, (float)edges / nsw.count);

    edges = 0;
    for (int i = 0; i < hnsw.count; i++) {
        edges += hnsw.nodes[i].num_neighbors;
        layer_dist[hnsw.nodes[i].level]++;
    }
    printf("HNSW: %d 节点, %d 条边\n", hnsw.count, edges);
    printf("  层高分布: ");
    for (int i = 0; i < MAX_LEVEL; i++)
        if (layer_dist[i] > 0) printf("L%d=%d ", i, layer_dist[i]);
    printf("\n");

    printf("\n========================================\n");
    printf("   演示完成\n");
    printf("========================================\n");
    return 0;
}
