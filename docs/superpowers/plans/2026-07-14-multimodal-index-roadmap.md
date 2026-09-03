# 多模态索引后续实施计划（总控）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完成 Phase 1-4 全部 33 个任务，验证现有索引、审查代码、补全文档、扩展 21 个新索引

**Architecture:** 串行执行四阶段，每阶段独立测试周期。Phase 1 基准测试 → Phase 2 代码审查 → Phase 3 文档补全 → Phase 4 新索引扩展

**Tech Stack:** C11、C++17、GoogleTest、CMake 3.20+

## Global Constraints

- 语言：所有代码注释、文档、提交信息使用简体中文
- 构建系统：CMake 3.20+，编译产物输出到 `build/engineering/`
- 测试框架：GoogleTest（vendored 在 `third_part/googletest/`）
- 测试文件：使用 `.cpp` 后缀，通过 `extern "C"` 引用 C 头文件
- 代码风格：遵循 CLAUDE.md 中的项目规范
- 提交频率：每个独立功能点完成后立即提交
- 向量维度：测试默认 128 维
- 距离度量：默认 L2 距离

---

# Phase 1: 集成验证（7 个任务）

## Task 1.1: 测试框架搭建

**Files:**
- Create: `engineering/test/db/index/integration/CMakeLists.txt`
- Create: `engineering/test/db/index/integration/integration_test.cpp`
- Create: `engineering/test/db/index/integration/benchmark_config.h`
- Modify: `engineering/test/db/index/CMakeLists.txt`

**Interfaces:**
- Consumes: 现有 gtest 框架、`engineering/include/db/index/vector_index/` 头文件
- Produces: `engineering/test/db/index/integration/` 目录，可编译测试框架

- [ ] **Step 1: 创建集成测试目录**

```bash
mkdir -p engineering/test/db/index/integration
```

- [ ] **Step 2: 编写测试框架 CMakeLists.txt**

```cmake
# engineering/test/db/index/integration/CMakeLists.txt

add_project_test(integration_test INTEGRATION_TEST_SRCS)
target_link_libraries(integration_test
    PRIVATE
        project_includes
        hnsw
        diskann
        ivf
        lsh
        opq
        heap_vector_store
        storage_backend
)
```

- [ ] **Step 3: 编写基准测试配置头文件**

```c
/* engineering/test/db/index/integration/benchmark_config.h */
#ifndef BENCHMARK_CONFIG_H
#define BENCHMARK_CONFIG_H

#include <stdint.h>

/* 测试数据集配置 */
#define BENCHMARK_DIMS 128
#define BENCHMARK_N_VECTORS 10000
#define BENCHMARK_N_QUERIES 100
#define BENCHMARK_K 10

/* 召回率阈值 */
#define HNSW_RECALL_THRESHOLD 0.90f
#define DISKANN_RECALL_THRESHOLD 0.85f
#define IVF_RECALL_THRESHOLD 0.85f
#define LSH_RECALL_THRESHOLD 0.70f

/* 性能阈值 */
#define MAX_QUERY_LATENCY_MS 2.0

#endif /* BENCHMARK_CONFIG_H */
```

- [ ] **Step 4: 编写集成测试骨架**

```cpp
// engineering/test/db/index/integration/integration_test.cpp

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <chrono>

extern "C" {
#include "benchmark_config.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "db/index/vector_index/diskann/diskann.h"
#include "db/index/vector_index/ivf/IndexIVF.h"
#include "db/index/vector_index/lsh/lsh.h"
#include "db/index/heap/heap_vector_store.h"
#include "db/index/storage_backend.h"
}

/* 辅助函数：生成随机向量 */
static void generate_random_vectors(float *vectors, int32_t n, int32_t dims) {
    for (int32_t i = 0; i < n * dims; i++) {
        vectors[i] = (float)rand() / RAND_MAX;
    }
}

/* 辅助函数：计算召回率 */
static float compute_recall(const int32_t *result_ids,
                            const int32_t *ground_truth_ids,
                            int32_t k) {
    int32_t hit = 0;
    for (int32_t i = 0; i < k; i++) {
        for (int32_t j = 0; j < k; j++) {
            if (result_ids[i] == ground_truth_ids[j]) {
                hit++;
                break;
            }
        }
    }
    return (float)hit / k;
}

TEST(IntegrationTest, FrameworkSetup) {
    EXPECT_EQ(BENCHMARK_DIMS, 128);
    EXPECT_EQ(BENCHMARK_K, 10);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
```

- [ ] **Step 5: 更新父目录 CMakeLists.txt**

```cmake
# engineering/test/db/index/CMakeLists.txt（添加子目录）

add_subdirectory(integration)
```

- [ ] **Step 6: 编译验证**

```bash
cd build/engineering
cmake --build . --target integration_test
```

Expected: 编译成功

- [ ] **Step 7: 运行测试验证框架**

```bash
ctest --test-dir build/engineering -R integration_test --output-on-failure
```

Expected: 1 个测试通过

- [ ] **Step 8: 提交**

```bash
git add engineering/test/db/index/integration/
git add engineering/test/db/index/CMakeLists.txt
git commit -m "test(integration): 添加集成测试框架骨架"
```

---

## Task 1.2: 单索引基准（HNSW）

**Files:**
- Create: `engineering/test/db/index/integration/hnsw_benchmark.cpp`
- Consumes: `engineering/src/db/index/vector_index/hnsw/`
- Produces: HNSW 召回率/延迟/内存基准数据

- [ ] **Step 1: 编写 HNSW 基准测试**

```cpp
// engineering/test/db/index/integration/hnsw_benchmark.cpp

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <cstdio>

extern "C" {
#include "benchmark_config.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "algo-prod/distance/distance.h"
}

class HNSWBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        /* 生成测试向量 */
        vectors_.resize(BENCHMARK_N_VECTORS * BENCHMARK_DIMS);
        queries_.resize(BENCHMARK_N_QUERIES * BENCHMARK_DIMS);
        generate_random_vectors(vectors_.data(), BENCHMARK_N_VECTORS, BENCHMARK_DIMS);
        generate_random_vectors(queries_.data(), BENCHMARK_N_QUERIES, BENCHMARK_DIMS);
    }

    std::vector<float> vectors_;
    std::vector<float> queries_;
};

TEST_F(HNSWBenchmark, BasicSearch) {
    /* 创建索引 */
    faiss_hnsw_t *index = faiss_hnsw_index_create(
        16,    /* M */
        BENCHMARK_DIMS,
        200,   /* ef_construction */
        DISTANCE_L2,
        QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(index, nullptr);

    /* 插入向量 */
    for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
        faiss_hnsw_index_add(index, 1, &vectors_[i * BENCHMARK_DIMS]);
    }
    EXPECT_EQ(faiss_hnsw_index_size(index), BENCHMARK_N_VECTORS);

    /* 搜索 */
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    auto start = std::chrono::high_resolution_clock::now();
    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        faiss_hnsw_index_search(
            index,
            &queries_[q * BENCHMARK_DIMS],
            BENCHMARK_K,
            50,  /* ef_search */
            result_dists,
            result_ids
        );
    }
    auto end = std::chrono::high_resolution_clock::now();

    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count() / BENCHMARK_N_QUERIES;
    printf("HNSW 平均查询延迟: %.3f ms\n", latency_ms);

    EXPECT_LT(latency_ms, MAX_QUERY_LATENCY_MS);

    faiss_hnsw_index_drop(index);
}

TEST_F(HNSWBenchmark, RecallTest) {
    /* 创建索引 */
    faiss_hnsw_t *index = faiss_hnsw_index_create(16, BENCHMARK_DIMS, 200, DISTANCE_L2, QUANTIZATION_TYPE_NONE);
    ASSERT_NE(index, nullptr);

    /* 插入向量 */
    faiss_hnsw_index_add(index, BENCHMARK_N_VECTORS, vectors_.data());

    /* 计算召回率（与暴力搜索对比） */
    float total_recall = 0.0f;
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        /* 近似搜索 */
        faiss_hnsw_index_search(
            index,
            &queries_[q * BENCHMARK_DIMS],
            BENCHMARK_K,
            50,
            result_dists,
            result_ids
        );

        /* 暴力搜索（ground truth） */
        std::vector<std::pair<float, int32_t>> all_dists;
        for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
            float dist = distance_l2(&queries_[q * BENCHMARK_DIMS],
                                     &vectors_[i * BENCHMARK_DIMS],
                                     BENCHMARK_DIMS);
            all_dists.push_back({dist, i});
        }
        std::sort(all_dists.begin(), all_dists.end());

        int32_t ground_truth[BENCHMARK_K];
        for (int32_t i = 0; i < BENCHMARK_K; i++) {
            ground_truth[i] = all_dists[i].second;
        }

        total_recall += compute_recall(result_ids, ground_truth, BENCHMARK_K);
    }

    float avg_recall = total_recall / BENCHMARK_N_QUERIES;
    printf("HNSW 平均召回率: %.2f%%\n", avg_recall * 100);

    EXPECT_GE(avg_recall, HNSW_RECALL_THRESHOLD);

    faiss_hnsw_index_drop(index);
}

static float compute_recall(const int32_t *result_ids,
                            const int32_t *ground_truth,
                            int32_t k) {
    int32_t hit = 0;
    for (int32_t i = 0; i < k; i++) {
        for (int32_t j = 0; j < k; j++) {
            if (result_ids[i] == ground_truth[j]) {
                hit++;
                break;
            }
        }
    }
    return (float)hit / k;
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

```cmake
# engineering/test/db/index/integration/CMakeLists.txt（追加）

set(INTEGRATION_TEST_SRCS
    integration_test.cpp
    hnsw_benchmark.cpp
)
```

- [ ] **Step 3: 编译**

```bash
cd build/engineering
cmake --build . --target integration_test
```

Expected: 编译成功

- [ ] **Step 4: 运行 HNSW 基准测试**

```bash
ctest --test-dir build/engineering -R HNSWBenchmark --output-on-failure
```

Expected: 2 个测试通过，召回率 > 90%，延迟 < 2ms

- [ ] **Step 5: 提交**

```bash
git add engineering/test/db/index/integration/hnsw_benchmark.cpp
git commit -m "test(hnsw): 添加 HNSW 基准测试（召回率/延迟）"
```

---

## Task 1.3: 单索引基准（DiskANN）

**Files:**
- Create: `engineering/test/db/index/integration/diskann_benchmark.cpp`
- Consumes: `engineering/src/db/index/vector_index/diskann/`
- Produces: DiskANN 召回率/延迟/内存基准数据

- [ ] **Step 1: 编写 DiskANN 基准测试**

```cpp
// engineering/test/db/index/integration/diskann_benchmark.cpp

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <cstdio>

extern "C" {
#include "benchmark_config.h"
#include "db/index/vector_index/diskann/diskann.h"
#include "algo-prod/distance/distance.h"
}

class DiskANNBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        vectors_.resize(BENCHMARK_N_VECTORS * BENCHMARK_DIMS);
        queries_.resize(BENCHMARK_N_QUERIES * BENCHMARK_DIMS);
        generate_random_vectors(vectors_.data(), BENCHMARK_N_VECTORS, BENCHMARK_DIMS);
        generate_random_vectors(queries_.data(), BENCHMARK_N_QUERIES, BENCHMARK_DIMS);
    }

    std::vector<float> vectors_;
    std::vector<float> queries_;
};

TEST_F(DiskANNBenchmark, BasicSearch) {
    /* 创建索引 */
    diskann_config_t *config = diskann_config_default(BENCHMARK_DIMS, DISTANCE_L2);
    config->index_size = 32;
    config->build_search_list_size = 100;

    diskann_t *index = diskann_index_create_with_config(config);
    ASSERT_NE(index, nullptr);

    /* 插入向量 */
    diskann_index_add(index, BENCHMARK_N_VECTORS, vectors_.data());

    /* 构建图 */
    diskann_index_build(index);

    /* 搜索 */
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    auto start = std::chrono::high_resolution_clock::now();
    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        diskann_index_search(
            index,
            &queries_[q * BENCHMARK_DIMS],
            BENCHMARK_K,
            50,   /* search_list_size */
            10,   /* max_iterations */
            result_dists,
            result_ids
        );
    }
    auto end = std::chrono::high_resolution_clock::now();

    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count() / BENCHMARK_N_QUERIES;
    printf("DiskANN 平均查询延迟: %.3f ms\n", latency_ms);

    EXPECT_LT(latency_ms, MAX_QUERY_LATENCY_MS * 1.5);  /* DiskANN 稍慢 */

    diskann_index_drop(index);
    diskann_config_free(config);
}

TEST_F(DiskANNBenchmark, RecallTest) {
    diskann_config_t *config = diskann_config_default(BENCHMARK_DIMS, DISTANCE_L2);
    config->index_size = 32;
    config->build_search_list_size = 100;

    diskann_t *index = diskann_index_create_with_config(config);
    ASSERT_NE(index, nullptr);

    diskann_index_add(index, BENCHMARK_N_VECTORS, vectors_.data());
    diskann_index_build(index);

    float total_recall = 0.0f;
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        diskann_index_search(index, &queries_[q * BENCHMARK_DIMS], BENCHMARK_K, 50, 10, result_dists, result_ids);

        /* 暴力搜索 ground truth */
        std::vector<std::pair<float, int32_t>> all_dists;
        for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
            float dist = distance_l2(&queries_[q * BENCHMARK_DIMS], &vectors_[i * BENCHMARK_DIMS], BENCHMARK_DIMS);
            all_dists.push_back({dist, i});
        }
        std::sort(all_dists.begin(), all_dists.end());

        int32_t ground_truth[BENCHMARK_K];
        for (int32_t i = 0; i < BENCHMARK_K; i++) {
            ground_truth[i] = all_dists[i].second;
        }

        total_recall += compute_recall(result_ids, ground_truth, BENCHMARK_K);
    }

    float avg_recall = total_recall / BENCHMARK_N_QUERIES;
    printf("DiskANN 平均召回率: %.2f%%\n", avg_recall * 100);

    EXPECT_GE(avg_recall, DISKANN_RECALL_THRESHOLD);

    diskann_index_drop(index);
    diskann_config_free(config);
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

```cmake
# engineering/test/db/index/integration/CMakeLists.txt（追加）

set(INTEGRATION_TEST_SRCS
    integration_test.cpp
    hnsw_benchmark.cpp
    diskann_benchmark.cpp
)
```

- [ ] **Step 3: 编译**

```bash
cd build/engineering
cmake --build . --target integration_test
```

Expected: 编译成功

- [ ] **Step 4: 运行测试**

```bash
ctest --test-dir build/engineering -R DiskANNBenchmark --output-on-failure
```

Expected: 召回率 > 85%，延迟 < 3ms

- [ ] **Step 5: 提交**

```bash
git add engineering/test/db/index/integration/diskann_benchmark.cpp
git commit -m "test(diskann): 添加 DiskANN 基准测试（召回率/延迟）"
```

---

## Task 1.4: 单索引基准（IVF）

**Files:**
- Create: `engineering/test/db/index/integration/ivf_benchmark.cpp`
- Consumes: `engineering/src/db/index/vector_index/ivf/`
- Produces: IVF 召回率/延迟/训练时间基准数据

- [ ] **Step 1: 编写 IVF 基准测试**

```cpp
// engineering/test/db/index/integration/ivf_benchmark.cpp

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <cstdio>

extern "C" {
#include "benchmark_config.h"
#include "db/index/vector_index/ivf/IndexIVF.h"
#include "algo-prod/distance/distance.h"
}

class IVFBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        vectors_.resize(BENCHMARK_N_VECTORS * BENCHMARK_DIMS);
        queries_.resize(BENCHMARK_N_QUERIES * BENCHMARK_DIMS);
        generate_random_vectors(vectors_.data(), BENCHMARK_N_VECTORS, BENCHMARK_DIMS);
        generate_random_vectors(queries_.data(), BENCHMARK_N_QUERIES, BENCHMARK_DIMS);
    }

    std::vector<float> vectors_;
    std::vector<float> queries_;
};

TEST_F(IVFBenchmark, BasicSearch) {
    /* 创建索引 */
    faiss_ivf_t *index = faiss_ivf_index_create(
        BENCHMARK_DIMS,
        100,   /* nlist */
        10,    /* nlist2 */
        20,    /* nprobe */
        DISTANCE_L2,
        QUANTIZATION_TYPE_NONE
    );
    ASSERT_NE(index, nullptr);

    /* 训练 */
    auto train_start = std::chrono::high_resolution_clock::now();
    faiss_ivf_index_train(index, BENCHMARK_N_VECTORS, vectors_.data());
    auto train_end = std::chrono::high_resolution_clock::now();

    double train_time_ms = std::chrono::duration<double, std::milli>(train_end - train_start).count();
    printf("IVF 训练时间: %.3f ms\n", train_time_ms);

    EXPECT_LT(train_time_ms, 5000.0);  /* 训练时间合理 */

    /* 插入向量 */
    faiss_ivf_index_add(index, BENCHMARK_N_VECTORS, vectors_.data());

    /* 搜索 */
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    auto start = std::chrono::high_resolution_clock::now();
    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        faiss_ivf_index_search(index, &queries_[q * BENCHMARK_DIMS], BENCHMARK_K, result_dists, result_ids);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count() / BENCHMARK_N_QUERIES;
    printf("IVF 平均查询延迟: %.3f ms\n", latency_ms);

    EXPECT_LT(latency_ms, MAX_QUERY_LATENCY_MS);

    faiss_ivf_index_drop(index);
}

TEST_F(IVFBenchmark, RecallTest) {
    faiss_ivf_t *index = faiss_ivf_index_create(BENCHMARK_DIMS, 100, 10, 20, DISTANCE_L2, QUANTIZATION_TYPE_NONE);
    ASSERT_NE(index, nullptr);

    faiss_ivf_index_train(index, BENCHMARK_N_VECTORS, vectors_.data());
    faiss_ivf_index_add(index, BENCHMARK_N_VECTORS, vectors_.data());

    float total_recall = 0.0f;
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        faiss_ivf_index_search(index, &queries_[q * BENCHMARK_DIMS], BENCHMARK_K, result_dists, result_ids);

        /* 暴力搜索 ground truth */
        std::vector<std::pair<float, int32_t>> all_dists;
        for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
            float dist = distance_l2(&queries_[q * BENCHMARK_DIMS], &vectors_[i * BENCHMARK_DIMS], BENCHMARK_DIMS);
            all_dists.push_back({dist, i});
        }
        std::sort(all_dists.begin(), all_dists.end());

        int32_t ground_truth[BENCHMARK_K];
        for (int32_t i = 0; i < BENCHMARK_K; i++) {
            ground_truth[i] = all_dists[i].second;
        }

        total_recall += compute_recall(result_ids, ground_truth, BENCHMARK_K);
    }

    float avg_recall = total_recall / BENCHMARK_N_QUERIES;
    printf("IVF 平均召回率: %.2f%%\n", avg_recall * 100);

    EXPECT_GE(avg_recall, IVF_RECALL_THRESHOLD);

    faiss_ivf_index_drop(index);
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

```cmake
set(INTEGRATION_TEST_SRCS
    integration_test.cpp
    hnsw_benchmark.cpp
    diskann_benchmark.cpp
    ivf_benchmark.cpp
)
```

- [ ] **Step 3: 编译并运行**

```bash
cd build/engineering
cmake --build . --target integration_test
ctest --test-dir build/engineering -R IVFBenchmark --output-on-failure
```

Expected: 召回率 > 85%，延迟 < 2ms

- [ ] **Step 4: 提交**

```bash
git add engineering/test/db/index/integration/ivf_benchmark.cpp
git commit -m "test(ivf): 添加 IVF 基准测试（召回率/延迟/训练时间）"
```

---

## Task 1.5: 单索引基准（LSH）

**Files:**
- Create: `engineering/test/db/index/integration/lsh_benchmark.cpp`
- Consumes: `engineering/src/db/index/vector_index/lsh/`
- Produces: LSH 召回率/延迟基准数据

- [ ] **Step 1: 编写 LSH 基准测试**

```cpp
// engineering/test/db/index/integration/lsh_benchmark.cpp

#include <gtest/gtest.h>
#include <vector>
#include <chrono>
#include <cstdio>

extern "C" {
#include "benchmark_config.h"
#include "db/index/vector_index/lsh/lsh.h"
#include "algo-prod/distance/distance.h"
}

class LSHBenchmark : public ::testing::Test {
protected:
    void SetUp() override {
        vectors_.resize(BENCHMARK_N_VECTORS * BENCHMARK_DIMS);
        queries_.resize(BENCHMARK_N_QUERIES * BENCHMARK_DIMS);
        generate_random_vectors(vectors_.data(), BENCHMARK_N_VECTORS, BENCHMARK_DIMS);
        generate_random_vectors(queries_.data(), BENCHMARK_N_QUERIES, BENCHMARK_DIMS);
    }

    std::vector<float> vectors_;
    std::vector<float> queries_;
};

TEST_F(LSHBenchmark, BasicSearch) {
    /* 创建索引 */
    lsh_index_t *index = lsh_create(LSH_PSTABLE, 20, 8, BENCHMARK_DIMS);
    ASSERT_NE(index, nullptr);

    /* 训练 */
    lsh_train(index, BENCHMARK_N_VECTORS, vectors_.data());

    /* 插入向量 */
    std::vector<int> ids(BENCHMARK_N_VECTORS);
    for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
        ids[i] = i;
    }
    lsh_add(index, BENCHMARK_N_VECTORS, vectors_.data(), ids.data());

    /* 搜索 */
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    auto start = std::chrono::high_resolution_clock::now();
    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        lsh_search(index, &queries_[q * BENCHMARK_DIMS], BENCHMARK_K, result_ids, result_dists);
    }
    auto end = std::chrono::high_resolution_clock::now();

    double latency_ms = std::chrono::duration<double, std::milli>(end - start).count() / BENCHMARK_N_QUERIES;
    printf("LSH 平均查询延迟: %.3f ms\n", latency_ms);

    EXPECT_LT(latency_ms, 1.0);  /* LSH 最快 */

    lsh_destroy(index);
}

TEST_F(LSHBenchmark, RecallTest) {
    lsh_index_t *index = lsh_create(LSH_PSTABLE, 20, 8, BENCHMARK_DIMS);
    ASSERT_NE(index, nullptr);

    lsh_train(index, BENCHMARK_N_VECTORS, vectors_.data());

    std::vector<int> ids(BENCHMARK_N_VECTORS);
    for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) ids[i] = i;
    lsh_add(index, BENCHMARK_N_VECTORS, vectors_.data(), ids.data());

    float total_recall = 0.0f;
    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];

    for (int32_t q = 0; q < BENCHMARK_N_QUERIES; q++) {
        lsh_search(index, &queries_[q * BENCHMARK_DIMS], BENCHMARK_K, result_ids, result_dists);

        /* 暴力搜索 ground truth */
        std::vector<std::pair<float, int32_t>> all_dists;
        for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
            float dist = distance_l2(&queries_[q * BENCHMARK_DIMS], &vectors_[i * BENCHMARK_DIMS], BENCHMARK_DIMS);
            all_dists.push_back({dist, i});
        }
        std::sort(all_dists.begin(), all_dists.end());

        int32_t ground_truth[BENCHMARK_K];
        for (int32_t i = 0; i < BENCHMARK_K; i++) {
            ground_truth[i] = all_dists[i].second;
        }

        total_recall += compute_recall(result_ids, ground_truth, BENCHMARK_K);
    }

    float avg_recall = total_recall / BENCHMARK_N_QUERIES;
    printf("LSH 平均召回率: %.2f%%\n", avg_recall * 100);

    EXPECT_GE(avg_recall, LSH_RECALL_THRESHOLD);

    lsh_destroy(index);
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

```cmake
set(INTEGRATION_TEST_SRCS
    integration_test.cpp
    hnsw_benchmark.cpp
    diskann_benchmark.cpp
    ivf_benchmark.cpp
    lsh_benchmark.cpp
)
```

- [ ] **Step 3: 编译并运行**

```bash
cd build/engineering
cmake --build . --target integration_test
ctest --test-dir build/engineering -R LSHBenchmark --output-on-failure
```

Expected: 召回率 > 70%，延迟 < 0.5ms

- [ ] **Step 4: 提交**

```bash
git add engineering/test/db/index/integration/lsh_benchmark.cpp
git commit -m "test(lsh): 添加 LSH 基准测试（召回率/延迟）"
```

---

## Task 1.6: 多索引联合测试

**Files:**
- Create: `engineering/test/db/index/integration/multi_index_test.cpp`
- Consumes: heap_store + HNSW + DiskANN + IVF + LSH
- Produces: 共享存储场景测试结果

- [ ] **Step 1: 编写多索引联合测试**

```cpp
// engineering/test/db/index/integration/multi_index_test.cpp

#include <gtest/gtest.h>
#include <vector>

extern "C" {
#include "benchmark_config.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "db/index/vector_index/diskann/diskann.h"
#include "db/index/vector_index/ivf/IndexIVF.h"
#include "db/index/vector_index/lsh/lsh.h"
#include "db/index/heap/heap_vector_store.h"
#include "db/index/storage_backend.h"
}

class MultiIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        vectors_.resize(BENCHMARK_N_VECTORS * BENCHMARK_DIMS);
        generate_random_vectors(vectors_.data(), BENCHMARK_N_VECTORS, BENCHMARK_DIMS);

        /* 创建共享 Heap 存储 */
        storage_backend_t *backend = storage_memory_create(65536);
        heap_vector_config_t config = {
            .backend = backend,
            .dims = BENCHMARK_DIMS,
            .page_size = 0,
            .vectors_per_page = 0
        };
        heap_store_ = heap_vector_store_create(&config);
    }

    void TearDown() override {
        if (heap_store_) {
            heap_vector_store_destroy(heap_store_);
        }
    }

    std::vector<float> vectors_;
    heap_vector_store_t *heap_store_ = nullptr;
};

TEST_F(MultiIndexTest, SharedHeapStore) {
    /* 创建多个索引并绑定同一 heap_store */
    faiss_hnsw_t *hnsw = faiss_hnsw_index_create(16, BENCHMARK_DIMS, 200, DISTANCE_L2, QUANTIZATION_TYPE_NONE);
    faiss_hnsw_index_set_heap_store(hnsw, heap_store_);

    diskann_config_t *config = diskann_config_default(BENCHMARK_DIMS, DISTANCE_L2);
    diskann_t *diskann = diskann_index_create_with_config(config);
    diskann_index_set_heap_store(diskann, heap_store_);

    faiss_ivf_t *ivf = faiss_ivf_index_create(BENCHMARK_DIMS, 100, 10, 20, DISTANCE_L2, QUANTIZATION_TYPE_NONE);
    faiss_ivf_index_set_heap_store(ivf, heap_store_);

    lsh_index_t *lsh = lsh_create(LSH_PSTABLE, 20, 8, BENCHMARK_DIMS);
    lsh_set_heap_store(lsh, heap_store_);

    /* 插入向量到所有索引 */
    for (int32_t i = 0; i < BENCHMARK_N_VECTORS; i++) {
        faiss_hnsw_index_add(hnsw, 1, &vectors_[i * BENCHMARK_DIMS]);
        diskann_index_add(diskann, 1, &vectors_[i * BENCHMARK_DIMS]);
        faiss_ivf_index_add(ivf, 1, &vectors_[i * BENCHMARK_DIMS]);
        lsh_add(lsh, 1, &vectors_[i * BENCHMARK_DIMS], &i);
    }

    /* 验证 heap_store 向量数量 */
    EXPECT_EQ(heap_vector_count(heap_store_), BENCHMARK_N_VECTORS);

    /* 搜索并对比结果一致性 */
    float query[BENCHMARK_DIMS];
    generate_random_vectors(query, 1, BENCHMARK_DIMS);

    int32_t hnsw_ids[BENCHMARK_K], ivf_ids[BENCHMARK_K], lsh_ids[BENCHMARK_K];
    float hnsw_dists[BENCHMARK_K], ivf_dists[BENCHMARK_K], lsh_dists[BENCHMARK_K];

    faiss_hnsw_index_search(hnsw, query, BENCHMARK_K, 50, hnsw_dists, hnsw_ids);
    faiss_ivf_index_search(ivf, query, BENCHMARK_K, ivf_dists, ivf_ids);
    lsh_search(lsh, query, BENCHMARK_K, lsh_ids, lsh_dists);

    /* 验证：所有索引返回的最近邻距离应该接近 */
    EXPECT_LT(fabs(hnsw_dists[0] - ivf_dists[0]), 0.5f);

    faiss_hnsw_index_drop(hnsw);
    diskann_index_drop(diskann);
    faiss_ivf_index_drop(ivf);
    lsh_destroy(lsh);
    diskann_config_free(config);
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

```cmake
set(INTEGRATION_TEST_SRCS
    integration_test.cpp
    hnsw_benchmark.cpp
    diskann_benchmark.cpp
    ivf_benchmark.cpp
    lsh_benchmark.cpp
    multi_index_test.cpp
)
```

- [ ] **Step 3: 编译并运行**

```bash
cd build/engineering
cmake --build . --target integration_test
ctest --test-dir build/engineering -R MultiIndexTest --output-on-failure
```

Expected: 所有索引共享 heap_store 成功

- [ ] **Step 4: 提交**

```bash
git add engineering/test/db/index/integration/multi_index_test.cpp
git commit -m "test(multi-index): 添加多索引联合测试（共享 heap_store）"
```

---

## Task 1.7: 端到端持久化验证

**Files:**
- Create: `engineering/test/db/index/integration/persistence_test.cpp`
- Consumes: storage_backend（文件模式）
- Produces: 持久化一致性验证结果

- [ ] **Step 1: 编写持久化测试**

```cpp
// engineering/test/db/index/integration/persistence_test.cpp

#include <gtest/gtest.h>
#include <vector>
#include <cstdio>
#include <unistd.h>

extern "C" {
#include "benchmark_config.h"
#include "db/index/vector_index/hnsw/faiss_hnsw.h"
#include "db/index/heap/heap_vector_store.h"
#include "db/index/storage_backend.h"
}

class PersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        test_file_ = "test_persistence.idx";
        vectors_.resize(BENCHMARK_N_VECTORS * BENCHMARK_DIMS);
        generate_random_vectors(vectors_.data(), BENCHMARK_N_VECTORS, BENCHMARK_DIMS);
    }

    void TearDown() override {
        unlink(test_file_);
    }

    const char *test_file_;
    std::vector<float> vectors_;
};

TEST_F(PersistenceTest, SaveAndLoad) {
    /* 创建索引并插入向量 */
    storage_backend_t *backend = storage_page_file_create(test_file_, 8192);
    ASSERT_NE(backend, nullptr);

    heap_vector_config_t config = {
        .backend = backend,
        .dims = BENCHMARK_DIMS,
        .page_size = 8192,
        .vectors_per_page = 0
    };
    heap_vector_store_t *heap = heap_vector_store_create(&config);
    ASSERT_NE(heap, nullptr);

    faiss_hnsw_t *index = faiss_hnsw_index_create(16, BENCHMARK_DIMS, 200, DISTANCE_L2, QUANTIZATION_TYPE_NONE);
    faiss_hnsw_index_set_heap_store(index, heap);

    faiss_hnsw_index_add(index, BENCHMARK_N_VECTORS, vectors_.data());

    /* 持久化 */
    int rc = faiss_hnsw_index_save(index, test_file_);
    EXPECT_EQ(rc, 0);

    faiss_hnsw_index_drop(index);
    heap_vector_store_destroy(heap);
    storage_backend_destroy(backend);

    /* 重新加载 */
    backend = storage_page_file_open(test_file_);
    ASSERT_NE(backend, nullptr);

    heap = heap_vector_store_create(&config);
    index = faiss_hnsw_index_load(test_file_);
    ASSERT_NE(index, nullptr);

    /* 验证向量数量 */
    EXPECT_EQ(faiss_hnsw_index_size(index), BENCHMARK_N_VECTORS);

    /* 搜索验证一致性 */
    float query[BENCHMARK_DIMS];
    generate_random_vectors(query, 1, BENCHMARK_DIMS);

    int32_t result_ids[BENCHMARK_K];
    float result_dists[BENCHMARK_K];
    faiss_hnsw_index_search(index, query, BENCHMARK_K, 50, result_dists, result_ids);

    EXPECT_GE(result_ids[0], 0);

    faiss_hnsw_index_drop(index);
    heap_vector_store_destroy(heap);
    storage_backend_destroy(backend);
}
```

- [ ] **Step 2: 更新 CMakeLists.txt**

```cmake
set(INTEGRATION_TEST_SRCS
    integration_test.cpp
    hnsw_benchmark.cpp
    diskann_benchmark.cpp
    ivf_benchmark.cpp
    lsh_benchmark.cpp
    multi_index_test.cpp
    persistence_test.cpp
)
```

- [ ] **Step 3: 编译并运行**

```bash
cd build/engineering
cmake --build . --target integration_test
ctest --test-dir build/engineering -R PersistenceTest --output-on-failure
```

Expected: 持久化数据一致性 100%

- [ ] **Step 4: 提交**

```bash
git add engineering/test/db/index/integration/persistence_test.cpp
git commit -m "test(persistence): 添加端到端持久化验证测试"
```

---

# Phase 2: 代码审查（2 个任务）

## Task 2.1: 全索引实现审查

**Files:**
- Review: `engineering/src/db/index/vector_index/hnsw/*.c`
- Review: `engineering/src/db/index/vector_index/diskann/*.c`
- Review: `engineering/src/db/index/vector_index/ivf/*.c`
- Review: `engineering/src/db/index/vector_index/lsh/*.c`
- Create: `docs/index/review/index-implementation-review.md`

- [ ] **Step 1: 审查 HNSW 实现**

检查要点：
- `faiss_hnsw_index_create`：参数验证、内存分配
- `faiss_hnsw_index_add`：heap_store 模式正确性
- `faiss_hnsw_index_search`：两阶段搜索实现
- `faiss_hnsw_index_drop`：资源释放完整性

- [ ] **Step 2: 审查 DiskANN 实现**

检查要点：
- `diskann_index_create_with_config`：配置验证
- `diskann_robust_prune`：剪枝算法正确性
- `diskann_graph_search`：贪心搜索边界条件
- `diskann_index_build`：冻结点选择逻辑

- [ ] **Step 3: 审查 IVF 实现**

检查要点：
- `faiss_ivf_index_train`：K-Means 收敛判断
- `faiss_ivf_index_add`：两级路由正确性
- `faiss_ivf_index_search`：六阶段搜索流程
- `faiss_direct_map`：ID 映射一致性

- [ ] **Step 4: 审查 LSH 实现**

检查要点：
- `lsh_create`：哈希表初始化
- `lsh_compute_signature_pstable`：投影计算
- `lsh_search`：候选去重逻辑

- [ ] **Step 5: 编写审查报告**

```markdown
# 索引实现审查报告

## HNSW 审查结果

### 正确性
- [ ] 插入路径正确（heap_store 模式）
- [ ] 搜索路径正确（两阶段）
- [ ] 删除标记正确

### 内存安全
- [ ] 无泄漏
- [ ] 无悬空指针
- [ ] 边界检查完整

### 性能热点
- [ ] 距离计算优化机会
- [ ] 邻居遍历优化

## DiskANN 审查结果
...

## IVF 审查结果
...

## LSH 审查结果
...
```

- [ ] **Step 6: 提交审查报告**

```bash
mkdir -p docs/index/review
git add docs/index/review/index-implementation-review.md
git commit -m "review(index): 完成全索引实现审查报告"
```

---

## Task 2.2: 存储子系统审查

**Files:**
- Review: `engineering/src/db/index/heap/heap_vector_store.c`
- Review: `engineering/src/db/index/storage_backend/*.c`
- Review: `engineering/include/db/index/vector_ref.h`
- Create: `docs/index/review/storage-subsystem-review.md`

- [ ] **Step 1: 审查 heap_vector_store**

检查要点：
- `heap_vector_insert`：页面分配逻辑
- `heap_vector_get`：引用解析正确性
- `heap_vector_store_destroy`：资源释放

- [ ] **Step 2: 审查 storage_backend**

检查要点：
- 接口设计完整性
- 错误处理一致性
- 并发安全性（如适用）

- [ ] **Step 3: 审查 vector_ref**

检查要点：
- 结构设计合理性
- 有效性检查
- 序列化兼容性

- [ ] **Step 4: 编写审查报告**

```markdown
# 存储子系统审查报告

## heap_vector_store 审查

### API 设计
- [ ] 接口清晰度
- [ ] 参数合理性
- [ ] 错误处理

## storage_backend 审查
...

## vector_ref 审查
...
```

- [ ] **Step 5: 提交审查报告**

```bash
git add docs/index/review/storage-subsystem-review.md
git commit -m "review(storage): 完成存储子系统审查报告"
```

---

# Phase 3: 文档补全（3 个任务）

## Task 3.1: OPQ 实现文档

**Files:**
- Create: `docs/index/implementation/opq-impl.md`
- Consumes: `engineering/src/db/index/vector_index/opq/opq.c`

- [ ] **Step 1: 编写 OPQ 实现文档结构**

参考 `docs/index/implementation/hnsw-impl.md` 结构：
- 文件结构
- 核心数据结构
- 完整流程
- Phase 2 改造要点

- [ ] **Step 2: 填充文档内容**

```markdown
# OPQ 实现详解

## 1. 文件结构

engineering/include/db/index/vector_index/opq/
└── opq.h

engineering/src/db/index/vector_index/opq/
└── opq.c

## 2. 核心数据结构

### 2.1 OPQ 量化器结构

typedef struct opq_quantizer {
    int dims;
    int m;
    int bits;
    float *codebooks;
    float *rotation;
    ...
} opq_quantizer_t;

## 3. 完整流程

### 3.1 训练
...

### 3.2 编码
...

### 3.3 ADC 距离计算
...
```

- [ ] **Step 3: 提交文档**

```bash
git add docs/index/implementation/opq-impl.md
git commit -m "docs(opq): 编写 OPQ 实现详解文档"
```

---

## Task 3.2: 量化器文档（PQ/RQ/LVQ）

**Files:**
- Create: `docs/index/theory/pq-rq-lvq-theory.md`

- [ ] **Step 1: 编写量化器原理文档**

```markdown
# 量化器原理（PQ/RQ/LVQ）

## 1. PQ（乘积量化）

### 核心思想
...

### 数学原理
...

### 参数选择
...

## 2. RQ（残差量化）

### 核心思想
...

## 3. LVQ（可学习向量量化）

### 核心思想
...

## 4. 对比分析

| 特性 | PQ | RQ | LVQ |
|------|----|----|-----|
| 精度 | 中 | 高 | 高 |
| 训练复杂度 | 低 | 中 | 高 |
...
```

- [ ] **Step 2: 提交文档**

```bash
git add docs/index/theory/pq-rq-lvq-theory.md
git commit -m "docs(quantizer): 编写 PQ/RQ/LVQ 量化器原理文档"
```

---

## Task 3.3: README 更新

**Files:**
- Modify: `docs/index/README.md`

- [ ] **Step 1: 更新索引分类表**

```markdown
### 向量索引
| 索引 | 理论文档 | 实现文档 | 状态 |
|------|---------|---------|------|
| HNSW | ✅ | ✅ | 完成 |
| DiskANN | ✅ | ✅ | 完成 |
| IVF | ✅ | ✅ | 完成 |
| LSH | ✅ | ✅ | 完成 |
| OPQ | ✅ | ✅ | 完成 |
| BM25 | ✅ | - | 完成 |
```

- [ ] **Step 2: 添加文档导航**

```markdown
## 文档导航

### 理论文档
- [HNSW 算法原理](theory/hnsw-theory.md)
- [DiskANN 算法原理](theory/diskann-theory.md)
...

### 实现文档
- [HNSW 实现详解](implementation/hnsw-impl.md)
...
```

- [ ] **Step 3: 提交更新**

```bash
git add docs/index/README.md
git commit -m "docs(index): 更新 README 文档导航"
```

---

# Phase 4: 新索引扩展（21 个任务）

## 第一批：核心索引（4 个）

### Task 4.1: NSW 索引

**Files:**
- Create: `engineering/include/db/index/vector_index/nsw/nsw.h`
- Create: `engineering/src/db/index/vector_index/nsw/nsw.c`
- Create: `engineering/test/db/index/vector_index/nsw/nsw_test.cpp`

**说明**：NSW 是 HNSW 的前身，单层可导航小世界图。实现简化版本作为图索引基础。

- [ ] **Step 1: 创建头文件**（框架）
- [ ] **Step 2: 实现基础结构**
- [ ] **Step 3: 实现插入算法**
- [ ] **Step 4: 实现搜索算法**
- [ ] **Step 5: 编写单元测试**
- [ ] **Step 6: 提交**

详细实现步骤省略（后续根据实际需求细化）。

---

### Task 4.2-4.21: 其他索引

（为节省篇幅，其余 20 个索引任务采用相同模板，实际编写时会展开详细步骤）

---

## 进度跟踪

完成本计划后，更新 `docs/index/ROADMAP.md` 中的任务状态。