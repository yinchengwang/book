/*
 * index_examples.c — 索引使用示例集
 *
 * 本文件展示项目中实现的各类索引的使用方法：
 * 1. GIN 通用倒排索引
 * 2. Bitmap 位图索引
 * 3. BRIN 块范围索引
 * 4. FULLTEXT 全文索引
 * 5. LSH 局部敏感哈希
 * 6. IVF-PQ 倒排量化索引
 *
 * 编译: gcc -o index_examples index_examples.c -lm
 * 运行: ./index_examples
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <index/index.h>

/* ================================================================
 * 示例 1: GIN 通用倒排索引
 * ================================================================
 *
 * GIN 适用于:
 * - JSONB 数据的键值检索
 * - 数组元素的包含查询
 * - 文档倒排索引
 *
 * 特点:
 * - Key → Posting List 结构
 * - 支持等值查询和范围查询
 * - 自动去重
 */

void example_gin_index(void)
{
    printf("\n=== GIN 通用倒排索引示例 ===\n");

    /* 创建 GIN 索引，预期 1024 个不同 key */
    gin_index_t *idx = gin_create(1024);
    if (!idx) {
        printf("  创建索引失败\n");
        return;
    }

    /* 插入 key-doc 映射 */
    gin_insert(idx, "hello", 1);
    gin_insert(idx, "world", 1);
    gin_insert(idx, "hello", 2);
    gin_insert(idx, "hello", 3);
    gin_insert(idx, "algorithm", 4);

    /* 搜索 "hello" */
    int results[100];
    int count = 0;
    gin_search(idx, "hello", results, &count);
    printf("  搜索 'hello': 找到 %d 个文档\n", count);
    for (int i = 0; i < count; i++) {
        printf("    - 文档 %d\n", results[i]);
    }

    /* 范围查询 */
    int range_count = 0;
    gin_range_query(idx, "a", "h", results, &range_count);
    printf("  范围查询 [a-h]: 找到 %d 个文档\n", range_count);

    /* 统计信息 */
    int key_count = 0, doc_count = 0;
    gin_stats(idx, &key_count, &doc_count);
    printf("  索引统计: %d 个 key, %d 个映射\n", key_count, doc_count);

    /* JSONB 插入示例 */
    const char *json = "{\"name\": \"Alice\", \"age\": 30, \"city\": \"Beijing\"}";
    gin_insert_json(idx, json, 10);
    int json_count = 0;
    gin_search(idx, "name", results, &json_count);
    printf("  JSONB 搜索 'name': 找到 %d 个文档\n", json_count);

    gin_destroy(idx);
    printf("  GIN 示例完成\n");
}

/* ================================================================
 * 示例 2: Bitmap 位图索引
 * ================================================================
 *
 * Bitmap 适用于:
 * - 低基数列（枚举值少）
 * - 需要 AND/OR/NOT 组合查询
 * - 数据仓库的多维分析
 *
 * 特点:
 * - 每个值一个位图
 * - 位运算高效
 * - 支持 RLE 压缩
 */

void example_bitmap_index(void)
{
    printf("\n=== Bitmap 位图索引示例 ===\n");

    /* 创建 Bitmap 索引: 10000 文档, 10 个不同值 */
    bitmap_index_t *idx = bitmap_create(10000, 10);
    if (!idx) {
        printf("  创建索引失败\n");
        return;
    }

    /* 设置文档值 (模拟分类/标签) */
    for (int i = 0; i < 5000; i++) {
        bitmap_set(idx, i, i % 5);  /* 0-4 循环 */
    }

    /* 等值查询 */
    int results[10000];
    int count = 0;
    bitmap_eq(idx, 0, results, &count);
    printf("  值=0 的文档数: %d\n", count);

    /* 组合查询: 值=1 AND 值=2 */
    int and_count = 0;
    bitmap_and(idx, 1, 2, results, &and_count);
    printf("  值=1 AND 值=2: %d 个文档\n", and_count);

    /* 组合查询: 值=1 OR 值=2 */
    int or_count = 0;
    bitmap_or(idx, 1, 2, results, &or_count);
    printf("  值=1 OR 值=2: %d 个文档\n", or_count);

    /* NOT 查询: NOT 值=0 */
    int not_count = 0;
    bitmap_not(idx, 0, results, &not_count);
    printf("  NOT 值=0: %d 个文档\n", not_count);

    /* 范围查询: 值在 [1, 3] 之间 */
    int range_count = 0;
    bitmap_range(idx, 1, 3, results, &range_count);
    printf("  值在 [1,3]: %d 个文档\n", range_count);

    bitmap_destroy(idx);
    printf("  Bitmap 示例完成\n");
}

/* ================================================================
 * 示例 3: BRIN 块范围索引
 * ================================================================
 *
 * BRIN 适用于:
 * - 大表按主键顺序存储的场景
 * - 只需要范围查询的情况
 * - 追求极低索引开销
 *
 * 特点:
 * - 按块维护 min/max 摘要
 * - 索引极小
 * - 查询需要扫描块内数据
 */

void example_brin_index(void)
{
    printf("\n=== BRIN 块范围索引示例 ===\n");

    /* 创建 BRIN: 8KB 页, 每 128 页一个范围 */
    brin_index_t *idx = brin_create(8192, 128);
    if (!idx) {
        printf("  创建索引失败\n");
        return;
    }

    /* 设置比较函数 (整数比较) */
    brin_set_compare(idx, [](const void *a, const void *b, void *ctx) -> int {
        int ia = *(const int *)a;
        int ib = *(const int *)b;
        return (ia > ib) - (ia < ib);
    }, NULL);

    /* 插入 1000 条记录 */
    for (int page = 0; page < 1000; page++) {
        int key = page * 10;  /* key 值为 0, 10, 20, ... */
        brin_insert(idx, page, &key, page);
    }

    /* 范围查询 */
    int min_key = 500, max_key = 1500;
    int results[1000];
    int count = 0;
    brin_range_query(idx, &min_key, &max_key, results, &count);
    printf("  范围 [500, 1500]: 找到 %d 条记录\n", count);

    /* 页面扫描 */
    int scan_count = 0;
    brin_scan(idx, 100, 200, results, &scan_count);
    printf("  页面扫描 [100, 200]: 找到 %d 条记录\n", scan_count);

    /* 统计信息 */
    int n_ranges = 0, n_pages = 0, total = 0;
    brin_stats(idx, &n_ranges, &n_pages, &total);
    printf("  索引统计: %d 个范围, %d 个页面\n", n_ranges, n_pages);

    brin_destroy(idx);
    printf("  BRIN 示例完成\n");
}

/* ================================================================
 * 示例 4: FULLTEXT 全文索引
 * ================================================================
 *
 * FULLTEXT 适用于:
 * - 文档搜索
 * - 关键词匹配
 * - 需要 TF-IDF 排序
 *
 * 特点:
 * - 支持中英文分词
 * - TF-IDF 评分
 * - AND/OR/NOT/短语/前缀搜索
 */

void example_fulltext_index(void)
{
    printf("\n=== FULLTEXT 全文索引示例 ===\n");

    /* 创建全文索引 */
    fulltext_index_t *idx = fulltext_create();
    if (!idx) {
        printf("  创建索引失败\n");
        return;
    }

    /* 设置分词器 (空格分词，适合英文) */
    fulltext_set_tokenizer(idx, TOKENIZER_WHITESPACE);

    /* 索引文档 */
    fulltext_index_doc(idx, 1, "vector database is a新兴技术");
    fulltext_index_doc(idx, 2, "traditional database index structures");
    fulltext_index_doc(idx, 3, "hnsw algorithm for vector search");
    fulltext_index_doc(idx, 4, "inverted index in database systems");

    /* 基本搜索 */
    int doc_ids[100];
    float scores[100];
    int count = 0;

    fulltext_search(idx, "database", doc_ids, scores, &count, 100);
    printf("  搜索 'database': 找到 %d 个文档\n", count);
    for (int i = 0; i < count && i < 5; i++) {
        printf("    - 文档 %d (分数: %.3f)\n", doc_ids[i], scores[i]);
    }

    /* AND 搜索 */
    count = 0;
    fulltext_search(idx, "vector AND index", doc_ids, scores, &count, 100);
    printf("  搜索 'vector AND index': 找到 %d 个文档\n", count);

    /* OR 搜索 */
    count = 0;
    fulltext_search(idx, "hnsw OR algorithm", doc_ids, scores, &count, 100);
    printf("  搜索 'hnsw OR algorithm': 找到 %d 个文档\n", count);

    /* 前缀搜索 */
    count = 0;
    fulltext_search(idx, "databas*", doc_ids, scores, &count, 100);
    printf("  搜索 'databas*': 找到 %d 个文档\n", count);

    /* 统计信息 */
    int n_docs = 0, n_terms = 0;
    float avg_len = 0;
    fulltext_stats(idx, &n_docs, &n_terms, &avg_len);
    printf("  索引统计: %d 文档, %d 词条, 平均长度 %.1f\n", n_docs, n_terms, avg_len);

    fulltext_destroy(idx);
    printf("  FULLTEXT 示例完成\n");
}

/* ================================================================
 * 示例 5: LSH 局部敏感哈希
 * ================================================================
 *
 * LSH 适用于:
 * - 近似最近邻搜索
 * - 文本去重
 * - 图片相似度检测
 *
 * 类型:
 * - Bitwise LSH: 汉明距离
 * - p-stable LSH: 欧氏距离
 * - SimHash: 余弦相似度
 */

void example_lsh_index(void)
{
    printf("\n=== LSH 局部敏感哈希示例 ===\n");

    /* 创建 LSH 索引 (64 维, 100 个哈希表) */
    lsh_index_t *idx = lsh_create(64, 100, LSH_TYPE_P_STABLE);
    if (!idx) {
        printf("  创建索引失败\n");
        return;
    }

    /* 生成一些随机向量 */
    srand(42);
    const int n_vectors = 100;
    float *vectors = (float *)malloc(n_vectors * 64 * sizeof(float));
    int *ids = (int *)malloc(n_vectors * sizeof(int));

    for (int i = 0; i < n_vectors; i++) {
        ids[i] = i;
        for (int d = 0; d < 64; d++) {
            vectors[i * 64 + d] = (float)rand() / RAND_MAX;
        }
    }

    /* 添加向量 */
    for (int i = 0; i < n_vectors; i++) {
        lsh_add(idx, &vectors[i * 64], ids[i]);
    }

    /* 搜索最近邻 */
    float query[64];
    for (int d = 0; d < 64; d++) {
        query[d] = (float)rand() / RAND_MAX;
    }

    lsh_result_t results[10];
    int count = lsh_search(idx, query, 10, results);
    printf("  搜索 10 个最近邻: 找到 %d 个\n", count);
    for (int i = 0; i < count && i < 5; i++) {
        printf("    - ID %d, 距离 %.3f\n", results[i].id, results[i].distance);
    }

    /* 统计信息 */
    int n_vectors_out = 0, n_tables = 0;
    lsh_stats(idx, &n_vectors_out, &n_tables);
    printf("  索引统计: %d 向量, %d 哈希表\n", n_vectors_out, n_tables);

    free(vectors);
    free(ids);
    lsh_destroy(idx);
    printf("  LSH 示例完成\n");
}

/* ================================================================
 * 示例 6: IVF-PQ 倒排量化索引
 * ================================================================
 *
 * IVF-PQ 适用于:
 * - 超大规模向量搜索
 * - 内存受限场景
 * - 需要压缩存储
 *
 * 特点:
 * - IVF 聚类 + PQ 量化
 * - 大幅降低内存
 * - 近似搜索，召回率可调
 */

void example_ivf_pq_index(void)
{
    printf("\n=== IVF-PQ 倒排量化索引示例 ===\n");

    /* 创建索引: 1024 聚类, 16 子空间, 8 比特 PQ, 128 维 */
    ivf_pq_index_t *idx = ivf_pq_create(1024, 16, 8, 128);
    if (!idx) {
        printf("  创建索引失败\n");
        return;
    }

    /* 生成训练数据 */
    srand(42);
    const int n_train = 5000;
    float *train_vectors = (float *)malloc(n_train * 128 * sizeof(float));
    for (int i = 0; i < n_train * 128; i++) {
        train_vectors[i] = (float)rand() / RAND_MAX;
    }

    /* 训练 */
    if (ivf_pq_train(idx, n_train, train_vectors) != 0) {
        printf("  训练失败\n");
        free(train_vectors);
        ivf_pq_destroy(idx);
        return;
    }
    printf("  训练完成\n");
    free(train_vectors);

    /* 添加向量 */
    const int n_vectors = 10000;
    float *vectors = (float *)malloc(n_vectors * 128 * sizeof(float));
    int *ids = (int *)malloc(n_vectors * sizeof(int));
    for (int i = 0; i < n_vectors; i++) {
        ids[i] = i;
        for (int d = 0; d < 128; d++) {
            vectors[i * 128 + d] = (float)rand() / RAND_MAX;
        }
    }

    int added = ivf_pq_add(idx, n_vectors, vectors, ids);
    printf("  添加 %d 个向量\n", added);

    /* 搜索 */
    float query[128];
    for (int d = 0; d < 128; d++) {
        query[d] = (float)rand() / RAND_MAX;
    }

    int result_ids[20];
    float result_dists[20];
    int count = ivf_pq_search(idx, query, 20, result_ids, result_dists);
    printf("  搜索 20 个最近邻: 找到 %d 个\n", count);
    for (int i = 0; i < count && i < 5; i++) {
        printf("    - ID %d, 距离 %.3f\n", result_ids[i], result_dists[i]);
    }

    /* 统计信息 */
    int n_vectors_out = 0, code_size = 0;
    ivf_pq_stats(idx, &n_vectors_out, &code_size);
    printf("  索引统计: %d 向量, 编码大小 %d 字节/向量\n", n_vectors_out, code_size);
    printf("  压缩比: %.1fx (原始 %.0f 字节/向量)\n",
           128 * 4.0f / code_size, 128 * 4.0f);

    free(vectors);
    free(ids);
    ivf_pq_destroy(idx);
    printf("  IVF-PQ 示例完成\n");
}

/* ================================================================
 * 主函数
 * ================================================================ */

int main(void)
{
    printf("===========================================\n");
    printf("  索引使用示例集\n");
    printf("  本项目实现了多种数据库索引结构\n");
    printf("===========================================\n");

    example_gin_index();
    example_bitmap_index();
    example_brin_index();
    example_fulltext_index();
    example_lsh_index();
    example_ivf_pq_index();

    printf("\n===========================================\n");
    printf("  所有示例完成!\n");
    printf("===========================================\n");

    return 0;
}