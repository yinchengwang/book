/**
 * @file main.c
 * @brief IVF 变体原理演示：IVF-Flat / IVF-PQ / IVF-HNSW / 倒排索引
 *
 * 组件：聚类中心、倒排桶、量化编码、复合索引
 * 参考: engineering/src/db/index/vector_index/ivf/ 与 ivf_pq/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

/* ============================================================
 * 常量定义
 * ============================================================ */
#define DIMS        4       /* 向量维度（简化） */
#define NLIST       4       /* 一级聚类数 */
#define NLIST2      2       /* 每个一级簇下的二级中心数 */
#define NPROBE      2       /* 查询时探测的一级簇数 */
#define MAX_VEC     64      /* 最大向量数 */

/* ============================================================
 * 距离计算
 * ============================================================ */

/** L2 距离平方 */
static float dist_l2(const float *a, const float *b, int d) {
    float s = 0.0f;
    for (int i = 0; i < d; i++) {
        float diff = a[i] - b[i];
        s += diff * diff;
    }
    return s;
}

/* ============================================================
 * IVF 核心结构
 * ============================================================ */

/** 聚类中心（IVF 一级 + 二级） */
typedef struct {
    float centroids[NLIST][DIMS];          /* 一级中心 */
    float sec_centroids[NLIST][NLIST2][DIMS]; /* 二级中心 */
} centroids_t;

/** 倒排桶（每个桶存向量 ID 列表） */
typedef struct {
    int32_t ids[32];   /* 简化的 ID 列表 */
    int     count;     /* 当前元素数 */
} inv_list_t;

/** IVF 索引状态 */
typedef struct {
    centroids_t  centers;
    inv_list_t   buckets[NLIST * NLIST2]; /* 扁平化桶数组 */
    float        vectors[MAX_VEC][DIMS];
    int          n_total;
    int          trained;
} ivf_index_t;

/* ============================================================
 * [ivf-flat] IVF-Flat 模式
 * ============================================================ */

static void demo_ivf_flat(ivf_index_t *idx)
{
    printf("[ivf-flat] ===== IVF-Flat =====\n");

    /* 初始化模拟聚类中心 */
    float c0[DIMS] = {0.0f, 0.0f, 0.0f, 0.0f};
    float c1[DIMS] = {5.0f, 5.0f, 5.0f, 5.0f};
    float c2[DIMS] = {10.0f, 0.0f, 10.0f, 0.0f};
    float c3[DIMS] = {0.0f, 10.0f, 0.0f, 10.0f};

    memcpy(idx->centers.centroids[0], c0, sizeof(c0));
    memcpy(idx->centers.centroids[1], c1, sizeof(c1));
    memcpy(idx->centers.centroids[2], c2, sizeof(c2));
    memcpy(idx->centers.centroids[3], c3, sizeof(c3));

    /* 模拟向量添加 */
    float v0[DIMS] = {1.0f, 1.0f, 1.0f, 1.0f};
    float v1[DIMS] = {6.0f, 6.0f, 6.0f, 6.0f};
    float v2[DIMS] = {2.0f, 0.0f, 2.0f, 0.0f};

    memcpy(idx->vectors[0], v0, sizeof(v0));
    memcpy(idx->vectors[1], v1, sizeof(v1));
    memcpy(idx->vectors[2], v2, sizeof(v2));
    idx->n_total = 3;

    /* 分配到最近的一级簇 */
    for (int i = 0; i < idx->n_total; i++) {
        int best = 0;
        float best_d = dist_l2(idx->vectors[i], idx->centers.centroids[0], DIMS);
        for (int c = 1; c < NLIST; c++) {
            float d = dist_l2(idx->vectors[i], idx->centers.centroids[c], DIMS);
            if (d < best_d) { best_d = d; best = c; }
        }
        int bidx = best * NLIST2; /* 用第一个二级桶简化 */
        inv_list_t *blk = &idx->buckets[bidx];
        if (blk->count < 32) blk->ids[blk->count++] = i;
        printf("[ivf-flat] 向量[%d] -> 桶 #%d (距离=%.2f)\n", i, bidx, sqrtf(best_d));
    }

    /* 搜索演示 */
    float query[DIMS] = {1.2f, 1.2f, 1.2f, 1.2f};
    printf("[ivf-flat] 查询向量: [1.2, 1.2, 1.2, 1.2]\n");

    /* 找最近的 nprobe 个一级簇 */
    printf("[ivf-flat] 探测最近的 %d 个簇:\n", NPROBE);
    for (int p = 0; p < NPROBE; p++) {
        int best_c = 0;
        float best_d = dist_l2(query, idx->centers.centroids[0], DIMS);
        for (int c = 1; c < NLIST; c++) {
            float d = dist_l2(query, idx->centers.centroids[c], DIMS);
            if (d < best_d) { best_d = d; best_c = c; }
        }
        printf("[ivf-flat]   簇 #%d: 距离=%.2f\n", best_c, sqrtf(best_d));
    }

    printf("[ivf-flat] 特点: 向量完整存储，无压缩，精度最高但内存占用大\n\n");
}

/* ============================================================
 * [ivf-pq] IVF-PQ 模式
 * ============================================================ */

static void demo_ivf_pq(void)
{
    printf("[ivf-pq] ===== IVF-PQ (Product Quantization) =====\n");

    /* PQ 将向量分成 M 段，每段独立量化 */
    int M = 2; /* 分段数 */
    int bits = 4; /* 每段 4bit，k=16 */
    int k = 1 << bits; /* 16 个码本中心 */

    printf("[ivf-pq] Product Quantization 参数:\n");
    printf("[ivf-pq]   维度 D=%d, 分段数 M=%d, 每段 %d bit, 码本大小 k=%d\n",
           DIMS, M, bits, k);
    printf("[ivf-pq]   原始向量: %d bytes\n", (int)(DIMS * sizeof(float)));
    printf("[ivf-pq]   PQ 编码后: %d bytes (压缩比: %.1fx)\n",
           M, (float)(DIMS * sizeof(float)) / (float)M);

    /* 模拟 PQ 编码过程 */
    printf("[ivf-pq] PQ 编码流程:\n");
    printf("[ivf-pq]   1. 向量分 M 段: vec = [seg0, seg1, ..., seg{M-1}]\n");
    printf("[ivf-pq]   2. 每段独立 K-Means 聚类，生成 M 个子码本\n");
    printf("[ivf-pq]   3. 每段用 %d bit 索引表示 → 存储 %d 个码字索引\n", bits, M);

    /* ADC 距离计算 */
    printf("[ivf-pq] ADC (Asymmetric Distance Computation):\n");
    printf("[ivf-pq]   查询向量 q 完整存储，与码字 c_i^j 查表计算距离\n");
    printf("[ivf-pq]   d(q, code) = sum_j  dist_table[j][code[j]]\n");
    printf("[ivf-pq]   查表代替逐元素计算，大幅加速距离计算\n");

    printf("[ivf-pq] 特点: 有损压缩，内存占用低，精度可通过增加 M 提升\n\n");
}

/* ============================================================
 * [ivf-hnsw] IVF-HNSW 复合索引
 * ============================================================ */

static void demo_ivf_hnsw(void)
{
    printf("[ivf-hnsw] ===== IVF-HNSW 复合索引 =====\n");

    printf("[ivf-hnsw] 索引结构:\n");
    printf("[ivf-hnsw]   IVF 层: 粗粒度聚类，缩小搜索范围\n");
    printf("[ivf-hnsw]   HNSW 层: 细粒度图搜索，精确定位\n");

    printf("[ivf-hnsw] HNSW 跳表结构:\n");
    printf("[ivf-hnsw]   Level 2: [entry] ------> [node_x]\n");
    printf("[ivf-hnsw]                    \\                \\\n");
    printf("[ivf-hnsw]   Level 1: [entry] --> [node_a] --> [node_b] --> [node_x]\n");
    printf("[ivf-hnsw]                    \\                \\\n");
    printf("[ivf-hnsw]   Level 0: [entry] --> [n0] --> [n1] --> [n2] --> ... --> [n_x]\n");

    printf("[ivf-hnsw] 搜索流程:\n");
    printf("[ivf-hnsw]   1. IVF: 查询向量找最近的 nprobe 个簇\n");
    printf("[ivf-hnsw]   2. HNSW: 在每个簇的 HNSW 图中搜索 top-k\n");
    printf("[ivf-hnsw]   3. 全局排序合并，返回最终结果\n");

    printf("[ivf-hnsw] 适用场景:\n");
    printf("[ivf-hnsw]   - 超大规模向量检索（百万~十亿级）\n");
    printf("[ivf-hnsw]   - 高召回率要求（>95%%）\n");
    printf("[ivf-hnsw]   - 查询延迟 < 100ms\n\n");
}

/* ============================================================
 * [inv-index] 经典倒排索引
 * ============================================================ */

static void demo_inverted_index(void)
{
    printf("[inv-index] ===== 倒排索引概念 =====\n");

    /* 模拟倒排索引结构 */
    printf("[inv-index] 正排 vs 倒排:\n");
    printf("[inv-index]   正排(forward):  doc_id -> [term1, term2, term3, ...]\n");
    printf("[inv-index]   倒排(inverted): term -> [doc_id1, doc_id2, ...]\n");

    printf("[inv-index] 倒排索引结构:\n");
    printf("[inv-index]   Term    | Posting List (doc_ids)\n");
    printf("[inv-index]   ----    | -------------------\n");
    printf("[inv-index]   '数据库' | [1, 3, 5, 8, ...]\n");
    printf("[inv-index]   '索引'   | [1, 2, 4, 7, ...]\n");
    printf("[inv-index]   '存储'   | [1, 3, 6, 9, ...]\n");

    printf("[inv-index] 查询处理:\n");
    printf("[inv-index]   AND: 交集 -> 两个 term 的 posting list 取交集\n");
    printf("[inv-index]   OR:  并集 -> 两个 term 的 posting list 取并集\n");
    printf("[inv-index]   NOT: 差集 -> 排除包含某 term 的文档\n");

    printf("[inv-index] 优化技术:\n");
    printf("[inv-index]   - 跳表(Skip List): 加速 posting list 遍历\n");
    printf("[inv-index]   - 压缩(Compression): Variable Byte, FOR-Delta\n");
    printf("[inv-index]   - 缓存(Cache): 热点 posting list 驻留内存\n\n");
}

/* ============================================================
 * 主函数
 * ============================================================ */

int main(void)
{
    printf("========================================\n");
    printf("   IVF 变体原理演示\n");
    printf("   IVF-Flat / IVF-PQ / IVF-HNSW / 倒排索引\n");
    printf("========================================\n\n");

    ivf_index_t idx;
    memset(&idx, 0, sizeof(idx));

    demo_ivf_flat(&idx);
    demo_ivf_pq();
    demo_ivf_hnsw();
    demo_inverted_index();

    printf("========================================\n");
    printf("   演示结束\n");
    printf("========================================\n");
    return 0;
}
