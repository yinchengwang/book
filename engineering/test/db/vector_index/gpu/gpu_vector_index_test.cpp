/**
 * @file gpu_vector_index_test.cpp
 * @brief GPU 向量索引测试用例
 *
 * 使用 GoogleTest 框架测试 GPU 索引功能。
 * 由于测试环境可能无 GPU，所有测试使用 CPU 存根实现。
 */
#include <gtest/gtest.h>
#include "db/index/vector_index/gpu/gpu_vector_index.h"
#include "db/index/vector_index/gpu/gpu_simd.h"
#include <cstdlib>
#include <cstring>

/* ========================================================================
 * 测试夹具
 * ======================================================================== */

class GpuVectorIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 初始化 GPU 子系统 */
        int ret = gpu_init();
        ASSERT_EQ(0, ret) << "GPU 初始化失败";
    }

    void TearDown() override {
        gpu_shutdown();
    }
};

/* ========================================================================
 * GPU 设备管理测试
 * ======================================================================== */

TEST_F(GpuVectorIndexTest, GpuInitShutdown)
{
    /* gpu_init 和 gpu_shutdown 已在 SetUp/TearDown 中测试 */
    SUCCEED();
}

TEST_F(GpuVectorIndexTest, GetDeviceList)
{
    gpu_device_list_t *list = gpu_get_device_list(GPU_BACKEND_CUDA);
    ASSERT_NE(nullptr, list) << "获取设备列表失败";

    EXPECT_GE(list->count, 1) << "至少应有 1 个设备";
    EXPECT_GE(list->selected_device, 0) << "应有选中的设备";

    printf("发现 %d 个 GPU 设备\n", list->count);
    for (int i = 0; i < list->count; i++) {
        printf("  设备 %d: %s, 内存: %zu MB\n",
               list->devices[i].device_id,
               list->devices[i].name,
               list->devices[i].total_memory / (1024 * 1024));
    }

    gpu_free_device_list(list);
}

TEST_F(GpuVectorIndexTest, SelectDevice)
{
    gpu_device_list_t *list = gpu_get_device_list(GPU_BACKEND_CUDA);
    ASSERT_NE(nullptr, list);

    if (list->count > 0) {
        int ret = gpu_select_device(0);
        EXPECT_EQ(0, ret) << "选择设备失败";

        const gpu_device_info_t *info = gpu_get_current_device();
        EXPECT_NE(nullptr, info) << "获取当前设备失败";
        EXPECT_EQ(0, info->device_id) << "设备 ID 不匹配";
    }

    gpu_free_device_list(list);
}

/* ========================================================================
 * GPU 内存管理测试
 * ======================================================================== */

TEST_F(GpuVectorIndexTest, GpuMallocFree)
{
    const size_t test_size = 1024 * 1024;  /* 1 MB */

    gpu_memory_t *mem = gpu_malloc(test_size, GPU_MEM_READ_WRITE);
    ASSERT_NE(nullptr, mem) << "GPU 内存分配失败";
    EXPECT_EQ(test_size, mem->size) << "内存大小不匹配";

    gpu_free(mem);
    SUCCEED();
}

TEST_F(GpuVectorIndexTest, GpuMemcpyH2D)
{
    const size_t test_size = 4096;
    const int test_value = 42;

    /* 创建 GPU 内存 */
    gpu_memory_t *mem = gpu_malloc(test_size, GPU_MEM_READ_WRITE);
    ASSERT_NE(nullptr, mem);

    /* 创建 Host 数据 */
    std::vector<int> host_data(test_size / sizeof(int), test_value);

    /* 传输数据 */
    int ret = gpu_memcpy_h2d(mem, host_data.data(), test_size);
    EXPECT_EQ(0, ret) << "H2D 传输失败";

    gpu_free(mem);
}

TEST_F(GpuVectorIndexTest, GpuMemcpyD2H)
{
    const size_t test_size = 4096;
    const int test_value = 123;

    /* 创建 GPU 内存 */
    gpu_memory_t *mem = gpu_malloc(test_size, GPU_MEM_READ_WRITE);
    ASSERT_NE(nullptr, mem);

    /* 写入数据到 GPU */
    std::vector<int> host_src(test_size / sizeof(int), test_value);
    gpu_memcpy_h2d(mem, host_src.data(), test_size);

    /* 读回数据 */
    std::vector<int> host_dst(test_size / sizeof(int), 0);
    int ret = gpu_memcpy_d2h(host_dst.data(), mem, test_size);
    EXPECT_EQ(0, ret) << "D2H 传输失败";

    /* 验证数据 */
    EXPECT_EQ(test_value, host_dst[0]) << "数据不匹配";

    gpu_free(mem);
}

/* ========================================================================
 * GPU-HNSW 索引测试
 * ======================================================================== */

class GpuHnswIndexTest : public ::testing::Test {
protected:
    gpu_hnsw_index_t *index = nullptr;

    void SetUp() override {
        gpu_init();

        gpu_hnsw_config_t config;
        config.dim = 128;
        config.M = 16;
        config.ef_construction = 200;
        config.ef_search = 100;
        config.max_elements = 1000;
        config.metric = 0;  /* L2 */

        index = gpu_hnsw_create(&config);
        ASSERT_NE(nullptr, index) << "HNSW 索引创建失败";
    }

    void TearDown() override {
        if (index != nullptr) {
            gpu_hnsw_destroy(index);
        }
        gpu_shutdown();
    }
};

TEST_F(GpuHnswIndexTest, CreateAndDestroy)
{
    /* 在 SetUp 中已创建， TearDown 中已销毁 */
    SUCCEED();
}

TEST_F(GpuHnswIndexTest, InsertAndSearch)
{
    const int32_t dim = 128;
    const int32_t n = 100;
    const int32_t k = 5;

    /* 生成随机向量 */
    std::vector<float> vectors(n * dim);
    for (int i = 0; i < n * dim; i++) {
        vectors[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    /* 插入向量 */
    int32_t inserted = gpu_hnsw_insert(index, vectors.data(), n, nullptr);
    EXPECT_EQ(n, inserted) << "插入数量不匹配";

    /* 验证统计 */
    int32_t num_vectors;
    size_t mem_usage;
    gpu_hnsw_get_stats(index, &num_vectors, &mem_usage);
    EXPECT_EQ(n, num_vectors) << "向量数量不匹配";

    /* 搜索 */
    float query[128];
    for (int i = 0; i < 128; i++) {
        query[i] = vectors[i];  /* 使用第一个向量作为查询 */
    }

    gpu_search_results_t *results = gpu_hnsw_search(index, query, k);
    ASSERT_NE(nullptr, results) << "搜索失败";
    EXPECT_GE(results->count, 1) << "应有搜索结果";
    EXPECT_LE(results->count, k) << "结果数量不应超过 k";

    printf("搜索返回 %d 个结果，耗时 %.3f ms\n",
           results->count, results->total_time_ms);

    gpu_free_results(results);
}

TEST_F(GpuHnswIndexTest, BatchSearch)
{
    const int32_t dim = 128;
    const int32_t n_vectors = 100;
    const int32_t n_queries = 10;
    const int32_t k = 5;

    /* 插入向量 */
    std::vector<float> vectors(n_vectors * dim);
    for (int i = 0; i < n_vectors * dim; i++) {
        vectors[i] = static_cast<float>(rand()) / RAND_MAX;
    }
    gpu_hnsw_insert(index, vectors.data(), n_vectors, nullptr);

    /* 批量搜索 */
    std::vector<float> queries(n_queries * dim);
    for (int i = 0; i < n_queries * dim; i++) {
        queries[i] = vectors[i];  /* 使用前 n_queries 个向量作为查询 */
    }

    std::vector<int32_t> ids(n_queries * k, -1);
    std::vector<float> distances(n_queries * k, -1.0f);

    int32_t success = gpu_hnsw_search_batch(
        index, queries.data(), n_queries, k, ids.data(), distances.data());

    EXPECT_EQ(n_queries, success) << "批量搜索应全部成功";

    /* 验证结果 */
    for (int i = 0; i < n_queries; i++) {
        EXPECT_GE(ids[i * k], 0) << "ID 应为非负数";
        EXPECT_GE(distances[i * k], 0.0f) << "距离应为非负数";
    }
}

TEST_F(GpuHnswIndexTest, SetEfSearch)
{
    gpu_hnsw_set_ef_search(index, 500);

    int32_t num_vectors;
    size_t mem_usage;
    gpu_hnsw_get_stats(index, &num_vectors, &mem_usage);

    SUCCEED();  /* 主要是验证不崩溃 */
}

/* ========================================================================
 * GPU-IVF 索引测试
 * ======================================================================== */

class GpuIvfIndexTest : public ::testing::Test {
protected:
    gpu_ivf_index_t *index = nullptr;

    void SetUp() override {
        gpu_init();

        gpu_ivf_config_t config;
        config.dim = 128;
        config.nlist = 10;
        config.nprobe = 3;
        config.max_elements = 1000;
        config.metric = 0;  /* L2 */

        index = gpu_ivf_create(&config);
        ASSERT_NE(nullptr, index) << "IVF 索引创建失败";
    }

    void TearDown() override {
        if (index != nullptr) {
            gpu_ivf_destroy(index);
        }
        gpu_shutdown();
    }
};

TEST_F(GpuIvfIndexTest, TrainAndInsert)
{
    const int32_t dim = 128;
    const int32_t n = 100;

    /* 生成训练数据 */
    std::vector<float> vectors(n * dim);
    for (int i = 0; i < n * dim; i++) {
        vectors[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    /* 训练 */
    int32_t ret = gpu_ivf_train(index, vectors.data(), n);
    EXPECT_EQ(0, ret) << "训练失败";

    /* 插入 */
    ret = gpu_ivf_insert(index, vectors.data(), n, nullptr);
    EXPECT_EQ(n, ret) << "插入数量不匹配";
}

TEST_F(GpuIvfIndexTest, Search)
{
    const int32_t dim = 128;
    const int32_t n = 100;
    const int32_t k = 5;

    /* 生成并插入数据 */
    std::vector<float> vectors(n * dim);
    for (int i = 0; i < n * dim; i++) {
        vectors[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    gpu_ivf_train(index, vectors.data(), n);
    gpu_ivf_insert(index, vectors.data(), n, nullptr);

    /* 搜索 */
    gpu_search_results_t *results = gpu_ivf_search(index, vectors.data(), k);
    ASSERT_NE(nullptr, results);
    EXPECT_GE(results->count, 1);

    printf("IVF 搜索返回 %d 个结果\n", results->count);

    gpu_free_results(results);
}

TEST_F(GpuIvfIndexTest, SetNprobe)
{
    gpu_ivf_set_nprobe(index, 5);
    SUCCEED();  /* 验证不崩溃 */
}

/* ========================================================================
 * GPU-IVF-PQ 索引测试
 * ======================================================================== */

class GpuIvfPqIndexTest : public ::testing::Test {
protected:
    gpu_ivf_pq_index_t *index = nullptr;

    void SetUp() override {
        gpu_init();

        gpu_ivf_pq_config_t config;
        config.dim = 128;
        config.nlist = 10;
        config.nprobe = 3;
        config.pq_m = 8;       /* 8 个子空间 */
        config.pq_nbits = 8;   /* 每子空间 256 个聚类中心 */
        config.max_elements = 1000;
        config.metric = 0;     /* L2 */

        index = gpu_ivf_pq_create(&config);
        ASSERT_NE(nullptr, index) << "IVF-PQ 索引创建失败";
    }

    void TearDown() override {
        if (index != nullptr) {
            gpu_ivf_pq_destroy(index);
        }
        gpu_shutdown();
    }
};

TEST_F(GpuIvfPqIndexTest, TrainAndInsert)
{
    const int32_t dim = 128;
    const int32_t n = 100;

    std::vector<float> vectors(n * dim);
    for (int i = 0; i < n * dim; i++) {
        vectors[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    int32_t ret = gpu_ivf_pq_train(index, vectors.data(), n);
    EXPECT_EQ(0, ret);

    ret = gpu_ivf_pq_insert(index, vectors.data(), n, nullptr);
    EXPECT_EQ(n, ret);
}

TEST_F(GpuIvfPqIndexTest, Search)
{
    const int32_t dim = 128;
    const int32_t n = 100;
    const int32_t k = 5;

    std::vector<float> vectors(n * dim);
    for (int i = 0; i < n * dim; i++) {
        vectors[i] = static_cast<float>(rand()) / RAND_MAX;
    }

    gpu_ivf_pq_train(index, vectors.data(), n);
    gpu_ivf_pq_insert(index, vectors.data(), n, nullptr);

    gpu_search_results_t *results = gpu_ivf_pq_search(index, vectors.data(), k);
    ASSERT_NE(nullptr, results);
    EXPECT_GE(results->count, 1);

    printf("IVF-PQ 搜索返回 %d 个结果\n", results->count);

    gpu_free_results(results);
}

/* ========================================================================
 * SIMD 优化测试
 * ======================================================================== */

class SimdTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* SIMD 在首次调用时自动初始化 */
    }
};

TEST_F(SimdTest, GetAvailableSimd)
{
    simd_type_t simd = simd_get_available();
    EXPECT_GE(simd, SIMD_NONE) << "SIMD 类型应有效";
    printf("可用 SIMD: %s\n", simd_get_name(simd));
}

TEST_F(SimdTest, L2Distance)
{
    const int32_t dim = 128;
    float a[128], b[128];

    for (int i = 0; i < dim; i++) {
        a[i] = static_cast<float>(i);
        b[i] = static_cast<float>(i + 1);
    }

    float dist = simd_l2_distance(a, b, dim);
    EXPECT_NEAR(dist, dim, 0.01f) << "L2 距离应为 dim (每维差值为 1)";

    /* 验证距离计算正确性 */
    float expected = 0.0f;
    for (int i = 0; i < dim; i++) {
        expected += (a[i] - b[i]) * (a[i] - b[i]);
    }
    EXPECT_NEAR(dist, expected, 0.001f) << "距离计算应与预期一致";
}

TEST_F(SimdTest, InnerProduct)
{
    const int32_t dim = 128;
    float a[128], b[128];

    for (int i = 0; i < dim; i++) {
        a[i] = 1.0f;
        b[i] = 1.0f;
    }

    float dot = simd_inner_product(a, b, dim);
    EXPECT_NEAR(dot, dim, 0.01f) << "内积应为 dim";
}

TEST_F(SimdTest, Normalize)
{
    const int32_t dim = 128;
    float v[128];

    for (int i = 0; i < dim; i++) {
        v[i] = static_cast<float>(i + 1);
    }

    simd_normalize_l2(v, dim);

    /* 验证 L2 范数为 1 */
    float sum_sq = 0.0f;
    for (int i = 0; i < dim; i++) {
        sum_sq += v[i] * v[i];
    }
    EXPECT_NEAR(sum_sq, 1.0f, 0.001f) << "归一化后 L2 范数应为 1";
}

TEST_F(SimdTest, ReduceSum)
{
    const int32_t dim = 128;
    float v[128];

    for (int i = 0; i < dim; i++) {
        v[i] = 1.0f;
    }

    float sum = simd_reduce_sum(v, dim);
    EXPECT_NEAR(sum, dim, 0.01f) << "求和应为 dim";
}

TEST_F(SimdTest, ReduceMax)
{
    const int32_t dim = 128;
    float v[128];

    for (int i = 0; i < dim; i++) {
        v[i] = static_cast<float>(i);
    }

    float max_val = simd_reduce_max(v, dim);
    EXPECT_NEAR(max_val, dim - 1, 0.01f) << "最大值应为 dim-1";
}

TEST_F(SimdTest, TopK)
{
    const int32_t n = 100;
    const int32_t k = 10;
    float values[100];
    float top_values[10];
    int32_t top_indices[10];

    for (int i = 0; i < n; i++) {
        values[i] = static_cast<float>(n - i);  /* 递减序列 */
    }

    simd_topk(values, n, k, top_values, top_indices);

    /* 验证结果正确性 */
    EXPECT_NEAR(top_values[0], n, 0.01f) << "最大值应为 n";
    for (int i = 1; i < k; i++) {
        EXPECT_GE(top_values[i-1], top_values[i]) << "结果应递减";
    }
}

TEST_F(SimdTest, TopKMin)
{
    const int32_t n = 100;
    const int32_t k = 10;
    float values[100];
    float top_values[10];
    int32_t top_indices[10];

    for (int i = 0; i < n; i++) {
        values[i] = static_cast<float>(i);  /* 递增序列 */
    }

    simd_topk_min(values, n, k, top_values, top_indices);

    /* 验证结果正确性 */
    EXPECT_NEAR(top_values[0], 0, 0.01f) << "最小值应为 0";
    for (int i = 1; i < k; i++) {
        EXPECT_LE(top_values[i-1], top_values[i]) << "结果应递增";
    }
}

TEST_F(SimdTest, VecAdd)
{
    const int32_t dim = 128;
    float a[128], b[128], c[128];

    for (int i = 0; i < dim; i++) {
        a[i] = 1.0f;
        b[i] = 2.0f;
    }

    simd_vec_add(a, b, c, dim);

    for (int i = 0; i < dim; i++) {
        EXPECT_NEAR(c[i], 3.0f, 0.01f) << "加法结果应为 3";
    }
}

TEST_F(SimdTest, VecSub)
{
    const int32_t dim = 128;
    float a[128], b[128], c[128];

    for (int i = 0; i < dim; i++) {
        a[i] = 5.0f;
        b[i] = 2.0f;
    }

    simd_vec_sub(a, b, c, dim);

    for (int i = 0; i < dim; i++) {
        EXPECT_NEAR(c[i], 3.0f, 0.01f) << "减法结果应为 3";
    }
}
