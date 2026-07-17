/**
 * @file main.c
 * @brief SQLite 向量扩展（SQLite VSS）概念演示
 *
 * 本程序演示 SQLite VSS 扩展的核心概念：
 * 1. SQLite 扩展函数注册机制
 * 2. 向量存储结构（Embedding 表设计）
 * 3. 近似最近邻搜索（ANN）的 SQL 接口封装
 * 4. 距离函数注册（欧氏距离/余弦距离）
 *
 * 注意：本示例展示概念，实际 SQLite VSS 扩展需编译为 .so/.dll 加载
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================
 * 模拟 SQLite VSS 扩展的向量存储结构
 * ============================================================ */

/**
 * @brief 向量元数据（模拟 embeddings 表结构）
 *
 * SQLite VSS 将向量存储在虚拟表中：
 * - vss_embedding: 存储向量 ID 和维度
 * - vss_embedding_content: 存储实际向量数据（blob）
 */
typedef struct {
    int    id;           /* 向量唯一标识 */
    float  vector[4];    /* 简化：4 维向量 */
    int    dim;          /* 维度 */
    char   metadata[64]; /* 关联的原始数据 ID */
} embedding_t;

/* ============================================================
 * SQLite VSS 核心概念模拟
 * ============================================================ */

/**
 * @brief 模拟向量搜索结果
 */
typedef struct {
    int    id;           /* 向量 ID */
    float  distance;     /* 距离值 */
    float  score;        /* 相似度分数 */
} search_result_t;

/* ============================================================
 * 向量距离计算（SQLite VSS 插件内部实现）
 * ============================================================ */

/**
 * 计算欧氏距离（L2 距离）
 *
 * SQLite VSS 内部使用：
 * CREATE FUNCTION vss Euclidean_distance AS ...
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
 * 计算余弦距离
 *
 * 注意：SQLite VSS 支持多种距离度量
 */
static float cosine_distance(const float *a, const float *b, int dim) {
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (int i = 0; i < dim; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
    norm_a = sqrtf(norm_a);
    norm_b = sqrtf(norm_b);
    if (norm_a < 1e-6f || norm_b < 1e-6f) return 1.0f;
    return 1.0f - dot / (norm_a * norm_b);
}

/* ============================================================
 * 模拟 SQLite 扩展函数注册（VSS 插件入口）
 * ============================================================ */

/**
 * @brief 模拟 SQLite 扩展初始化
 *
 * SQLite VSS 扩展加载时会调用：
 * int sqlite3_extension_init(sqlite3 *db, void **pzErrMsg,
 *                            const sqlite3_api_routines *pApi);
 */
static void simulate_vss_extension_init(void) {
    printf("[VSS 扩展] 注册向量函数...\n");
    printf("  - vss_search: ANN 搜索接口\n");
    printf("  - vss_euclidean_distance: 欧氏距离\n");
    printf("  - vss_cosine_distance: 余弦距离\n");
    printf("  - vss_create_index: 创建向量索引\n");
    printf("[VSS 扩展] 初始化完成\n\n");
}

/* ============================================================
 * 模拟 VSS 虚拟表操作
 * ============================================================ */

/**
 * @brief 模拟向量插入到 VSS 虚拟表
 *
 * 实际 SQL 用法：
 * INSERT INTO vss_embedding VALUES (1, '[0.1, 0.2, 0.3, 0.4]');
 */
static void simulate_insert_vector(embedding_t *emb) {
    printf("[VSS 虚拟表] 插入向量 #%d: [%.2f, %.2f, %.2f, %.2f]\n",
           emb->id,
           emb->vector[0], emb->vector[1],
           emb->vector[2], emb->vector[3]);
    printf("             metadata: %s\n", emb->metadata);
}

/**
 * @brief 模拟 VSS ANN 搜索
 *
 * 实际 SQL 用法：
 * SELECT id, distance FROM vss_embedding
 * WHERE vss_search(vector, '[0.1, 0.2, 0.3, 0.4]')
 * ORDER BY distance LIMIT 5;
 */
static void simulate_ann_search(const float *query, int k) {
    printf("[VSS ANN 搜索] 查询向量: [%.2f, %.2f, %.2f, %.2f]\n",
           query[0], query[1], query[2], query[3]);
    printf("             返回 Top-%d 结果\n\n", k);
}

/* ============================================================
 * SQLite VSS 与 pgvector 的概念对比
 * ============================================================ */

static void print_concept_comparison(void) {
    printf("=== SQLite VSS vs pgvector 概念对比 ===\n\n");

    printf("| 功能           | SQLite VSS            | pgvector           |\n");
    printf("|----------------|------------------------|--------------------|\n");
    printf("| 存储类型       | 虚拟表 (virtual table) | 原生向量列类型     |\n");
    printf("| 距离函数       | vss_euclidean_distance | <=> (欧氏)         |\n");
    printf("|                | vss_cosine_distance    | <=> (余弦)         |\n");
    printf("| 索引类型       | HNSW / IVF             | ivfflat / hnsw     |\n");
    printf("| 搜索语法       | vss_search()           | ORDER BY col <->   |\n");
    printf("| 扩展加载方式   | SELECT load_extension()| CREATE EXTENSION   |\n");
    printf("\n");
}

/* ============================================================
 * 主函数：演示 SQLite VSS 概念
 * ============================================================ */

int main(void) {
    printf("========================================\n");
    printf("   SQLite VSS 向量扩展概念演示\n");
    printf("========================================\n\n");

    /* 1. 模拟 VSS 扩展初始化 */
    printf("--- 1. VSS 扩展加载 ---\n");
    simulate_vss_extension_init();

    /* 2. 模拟向量存储 */
    printf("--- 2. 向量存储结构 ---\n");
    embedding_t emb1 = {
        .id = 1,
        .vector = {0.1f, 0.2f, 0.3f, 0.4f},
        .dim = 4,
        .metadata = "doc:001"
    };
    simulate_insert_vector(&emb1);

    embedding_t emb2 = {
        .id = 2,
        .vector = {0.5f, 0.6f, 0.7f, 0.8f},
        .dim = 4,
        .metadata = "doc:002"
    };
    simulate_insert_vector(&emb2);
    printf("\n");

    /* 3. 模拟距离计算 */
    printf("--- 3. 距离计算函数 ---\n");
    float dist_euclidean = euclidean_distance(emb1.vector, emb2.vector, 4);
    float dist_cosine = cosine_distance(emb1.vector, emb2.vector, 4);
    printf("向量 #1 与 #2 的距离:\n");
    printf("  欧氏距离: %.4f\n", dist_euclidean);
    printf("  余弦距离: %.4f\n", dist_cosine);
    printf("\n");

    /* 4. 模拟 ANN 搜索 */
    printf("--- 4. ANN 搜索接口 ---\n");
    float query[] = {0.15f, 0.25f, 0.35f, 0.45f};
    simulate_ann_search(query, 3);
    printf("  模拟结果:\n");
    printf("  1. id=1, distance=%.4f\n", euclidean_distance(emb1.vector, query, 4));
    printf("  2. id=2, distance=%.4f\n", euclidean_distance(emb2.vector, query, 4));
    printf("\n");

    /* 5. 概念对比 */
    print_concept_comparison();

    printf("========================================\n");
    printf("      演示完成\n");
    printf("========================================\n");

    return 0;
}
