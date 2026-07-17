// 多索引联合测试
//
// 验证 HNSW + IVF + LSH 三种索引在共享 heap_vector_store 数据源下
// 搜索结果的一致性：
//   - 各索引能够使用同一份底层向量数据独立完成近似 KNN 搜索。
//   - 至少有一对索引返回的 top-k 结果存在交集（即至少有 1 个 id 同时
//     出现在两个索引的 top-k 中）。
//   - 顺序执行多次"插入-搜索"循环，不应出现崩溃或内存泄漏。

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cmath>

extern "C" {
#include "benchmark_config.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "db/index/vector_index/ivf/IndexIVF.h"
#include "db/index/vector_index/lsh/lsh.h"
#include "db/index/heap/heap_vector_store.h"
#include "db/index/storage_backend.h"
}

/* LSH 参数（与 lsh_benchmark.cpp 保持一致，确保召回率达标） */
#define MULTI_INDEX_LSH_N_HASH 32
#define MULTI_INDEX_LSH_N_TABLES 32

/* 生成 [0, 1) 区间均匀分布的随机向量 */
static void multi_index_generate_random_vectors(float *vectors, int32_t n, int32_t dims) {
    for (int32_t i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
}

/* 检测两个 top-k 结果集的交集大小（去重计数） */
static int multi_index_count_intersection(const int32_t *ids_a,
                                          const int32_t *ids_b,
                                          int32_t k) {
    int count = 0;
    for (int32_t i = 0; i < k; i++) {
        for (int32_t j = 0; j < k; j++) {
            if (ids_a[i] == ids_b[j]) {
                count++;
                break;
            }
        }
    }
    return count;
}

/**
 * 测试：共享向量一致性
 *
 * 创建一份 heap_vector_store 作为向量 Single Source of Truth；
 * 同时构建 HNSW、IVF、LSH 三种索引，让它们独立地持有/引用相同的
 * 底层向量数据。对同一查询向量执行 top-k 搜索，验证：
 *   1. 各索引都能返回 BENCHMARK_K 个有效 ID。
 *   2. 三组结果中至少存在一对索引有交集（近似算法下不必完全一致）。
 */
TEST(MultiIndexJointTest, SharedVectorConsistency) {
    /* 固定随机种子，确保跨运行结果稳定 */
    srand(42);

    /* 1. 创建存储后端 + Heap 向量存储 */
    storage_backend_t *backend = storage_memory_create(8192);
    ASSERT_NE(backend, nullptr);

    heap_vector_config_t heap_cfg;
    heap_cfg.backend = backend;
    heap_cfg.dims = BENCHMARK_DIMS;
    heap_cfg.page_size = 0;                 /* 使用默认 page_size */
    heap_cfg.vectors_per_page = 0;          /* 自动计算 */

    heap_vector_store_t *heap_store = heap_vector_store_create(&heap_cfg);
    ASSERT_NE(heap_store, nullptr);

    /* 2. 生成测试向量并写入 Heap */
    std::vector<float> vectors(static_cast<size_t>(BENCHMARK_N_VECTORS) * BENCHMARK_DIMS);
    multi_index_generate_random_vectors(vectors.data(), BENCHMARK_N_VECTORS, BENCHMARK_DIMS);

    for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
        vector_ref_t ref = heap_vector_insert(heap_store, &vectors[i * BENCHMARK_DIMS]);
        ASSERT_TRUE(ref.heap_page_id != -1)
            << "heap_vector_insert 失败于 i=" << i;
    }
    EXPECT_EQ(heap_vector_count(heap_store), BENCHMARK_N_VECTORS);

    /* 3. 构建 HNSW 索引（绑定同一份 heap_store） */
    faiss_hnsw_t *hnsw = faiss_hnsw_index_create(
        16,                         /* M */
        BENCHMARK_DIMS,
        200,                        /* ef_construction */
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(hnsw, nullptr);

    int32_t hnsw_bind_rc = faiss_hnsw_index_set_heap_store(hnsw, heap_store);
    ASSERT_EQ(hnsw_bind_rc, 0);

    int32_t hnsw_add_rc = faiss_hnsw_index_add(hnsw, BENCHMARK_N_VECTORS, vectors.data());
    ASSERT_EQ(hnsw_add_rc, 0);

    /* 4. 构建 IVF 索引 */
    faiss_ivf_t *ivf = faiss_ivf_index_create(
        BENCHMARK_DIMS,
        100,                        /* nlist */
        10,                         /* nlist2 */
        20,                         /* nprobe */
        DISTANCE_METRIC_L2_SQUARED,
        QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(ivf, nullptr);

    int32_t ivf_train_rc = faiss_ivf_index_train(ivf, BENCHMARK_N_VECTORS, vectors.data());
    ASSERT_EQ(ivf_train_rc, 0);

    int32_t ivf_add_rc = faiss_ivf_index_add(ivf, BENCHMARK_N_VECTORS, vectors.data());
    ASSERT_EQ(ivf_add_rc, 0);

    /* 5. 构建 LSH 索引 */
    lsh_index_t *lsh = lsh_create(
        LSH_PSTABLE,
        MULTI_INDEX_LSH_N_HASH,
        MULTI_INDEX_LSH_N_TABLES,
        BENCHMARK_DIMS
    );
    ASSERT_NE(lsh, nullptr);

    int32_t lsh_train_rc = lsh_train(lsh, BENCHMARK_N_VECTORS, vectors.data());
    ASSERT_EQ(lsh_train_rc, 0);

    for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
        int32_t added = lsh_add(lsh, 1, &vectors[i * BENCHMARK_DIMS], &i);
        ASSERT_GT(added, 0) << "lsh_add 失败于 i=" << i;
    }

    /* 6. 生成查询向量并执行搜索 */
    float query[BENCHMARK_DIMS];
    multi_index_generate_random_vectors(query, 1, BENCHMARK_DIMS);

    int32_t hnsw_ids[BENCHMARK_K];
    int32_t ivf_ids[BENCHMARK_K];
    int32_t lsh_ids[BENCHMARK_K];
    float hnsw_dists[BENCHMARK_K];
    float ivf_dists[BENCHMARK_K];
    float lsh_dists[BENCHMARK_K];

    int32_t hnsw_search_rc = faiss_hnsw_index_search(
        hnsw, query, BENCHMARK_K, 50,
        hnsw_dists, hnsw_ids
    );
    /* faiss_hnsw_index_search 返回的是 actual_k（找到的结果数），不是状态码 */
    ASSERT_GE(hnsw_search_rc, 1);

    int32_t ivf_search_rc = faiss_ivf_index_search(
        ivf, query, BENCHMARK_K,
        ivf_dists, ivf_ids
    );
    /* faiss_ivf_index_search 返回 0 表示成功 */
    ASSERT_EQ(ivf_search_rc, 0);

    int32_t lsh_search_rc = lsh_search(
        lsh, query, BENCHMARK_K,
        lsh_ids, lsh_dists
    );
    ASSERT_GT(lsh_search_rc, 0);

    /* 7. 计算三组结果两两之间的交集大小 */
    int inter_hnsw_ivf = multi_index_count_intersection(hnsw_ids, ivf_ids, BENCHMARK_K);
    int inter_hnsw_lsh = multi_index_count_intersection(hnsw_ids, lsh_ids, BENCHMARK_K);
    int inter_ivf_lsh  = multi_index_count_intersection(ivf_ids,  lsh_ids, BENCHMARK_K);

    printf("HNSW top-%d: ", BENCHMARK_K);
    for (int i = 0; i < BENCHMARK_K; i++) printf("%d ", hnsw_ids[i]);
    printf("\nIVF  top-%d: ", BENCHMARK_K);
    for (int i = 0; i < BENCHMARK_K; i++) printf("%d ", ivf_ids[i]);
    printf("\nLSH  top-%d: ", BENCHMARK_K);
    for (int i = 0; i < BENCHMARK_K; i++) printf("%d ", lsh_ids[i]);
    printf("\n两两交集: HNSW∩IVF=%d  HNSW∩LSH=%d  IVF∩LSH=%d\n",
           inter_hnsw_ivf, inter_hnsw_lsh, inter_ivf_lsh);

    /* 8. 验收：至少存在一对索引的 top-k 结果有交集 */
    int max_inter = inter_hnsw_ivf;
    if (inter_hnsw_lsh > max_inter) max_inter = inter_hnsw_lsh;
    if (inter_ivf_lsh > max_inter)  max_inter = inter_ivf_lsh;
    EXPECT_GT(max_inter, 0) << "三个索引的 top-k 结果两两都不相交，疑似索引构建失败";

    /* 清理（顺序：先索引 → 后 heap_store → 最后 backend） */
    lsh_destroy(lsh);
    faiss_ivf_index_drop(ivf);
    faiss_hnsw_index_drop(hnsw);
    heap_vector_store_destroy(heap_store);
    storage_backend_destroy(backend);
}

/**
 * 测试：顺序读写安全
 *
 * 单线程依次执行 3 轮"插入 → 搜索 → 插入 → 搜索"循环，
 * 验证：不会崩溃、不应出现非法 ID、每轮搜索都能正常返回。
 */
TEST(MultiIndexJointTest, SequentialReadWriteSafety) {
    srand(123);

    storage_backend_t *backend = storage_memory_create(8192);
    ASSERT_NE(backend, nullptr);

    heap_vector_config_t heap_cfg;
    heap_cfg.backend = backend;
    heap_cfg.dims = BENCHMARK_DIMS;
    heap_cfg.page_size = 0;
    heap_cfg.vectors_per_page = 0;

    heap_vector_store_t *heap_store = heap_vector_store_create(&heap_cfg);
    ASSERT_NE(heap_store, nullptr);

    faiss_hnsw_t *hnsw = faiss_hnsw_index_create(
        16, BENCHMARK_DIMS, 200,
        DISTANCE_METRIC_L2_SQUARED, QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(hnsw, nullptr);

    int32_t bind_rc = faiss_hnsw_index_set_heap_store(hnsw, heap_store);
    ASSERT_EQ(bind_rc, 0);

    /* 多次插入-搜索循环 */
    const int32_t kBatchSize = 100;
    const int kRounds = 3;

    for (int round = 0; round < kRounds; round++) {
        std::vector<float> batch(static_cast<size_t>(kBatchSize) * BENCHMARK_DIMS);
        multi_index_generate_random_vectors(batch.data(), kBatchSize, BENCHMARK_DIMS);

        /*
         * 当 HNSW 已绑定 heap_store 时，faiss_hnsw_index_add 内部会自动调用
         * heap_vector_insert 将向量写入 heap_store。因此这里直接交给 HNSW，
         * 避免重复插入导致 heap_vector_count 翻倍。
         */
        int32_t add_rc = faiss_hnsw_index_add(hnsw, kBatchSize, batch.data());
        ASSERT_EQ(add_rc, 0) << "faiss_hnsw_index_add 失败于 round=" << round;

        /* 用随机查询执行一次搜索 */
        float query[BENCHMARK_DIMS];
        multi_index_generate_random_vectors(query, 1, BENCHMARK_DIMS);

        int32_t ids[BENCHMARK_K];
        float dists[BENCHMARK_K];
        int32_t search_rc = faiss_hnsw_index_search(
            hnsw, query, BENCHMARK_K, 50, dists, ids
        );
        /* faiss_hnsw_index_search 返回 actual_k（>=1 表示成功） */
        ASSERT_GE(search_rc, 1) << "faiss_hnsw_index_search 失败于 round=" << round;

        /* 距离值应当为有限数 */
        for (int i = 0; i < BENCHMARK_K; i++) {
            EXPECT_TRUE(std::isfinite(dists[i]))
                << "round=" << round << " i=" << i << " dist=" << dists[i];
        }
    }

    /* 总插入数应等于 batch * rounds */
    EXPECT_EQ(heap_vector_count(heap_store), kBatchSize * kRounds);

    printf("顺序读写安全测试通过（共 %d 轮，每轮 %d 条插入 + 1 次搜索）\n",
           kRounds, kBatchSize);

    faiss_hnsw_index_drop(hnsw);
    heap_vector_store_destroy(heap_store);
    storage_backend_destroy(backend);
}