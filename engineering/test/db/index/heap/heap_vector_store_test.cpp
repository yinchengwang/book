/**
 * @file heap_vector_store_test.cpp
 * @brief heap_vector_store 单元测试
 *
 * 测试覆盖：
 * - 创建/销毁
 * - 单向量插入与读取
 * - 批量插入与批量读取
 * - 跨页向量存储
 * - 错误处理（无效参数、越界访问）
 */

#include <gtest/gtest.h>

extern "C" {
#include <db/index/heap/heap_vector_store.h>
#include <db/index/storage_backend.h>
}

// 测试常量
static constexpr int32_t DIMS = 128;
static constexpr size_t PAGE_SIZE = 8192;

class HeapVectorStoreTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建内存存储后端
        backend = storage_memory_create(PAGE_SIZE);
        ASSERT_NE(backend, nullptr);

        // 创建 Heap 向量存储
        heap_vector_config_t config = {};
        config.backend = backend;
        config.dims = DIMS;
        config.page_size = PAGE_SIZE;
        config.vectors_per_page = 0;  // 自动计算

        store = heap_vector_store_create(&config);
        ASSERT_NE(store, nullptr);
    }

    void TearDown() override {
        if (store) {
            heap_vector_store_destroy(store);
        }
        if (backend) {
            storage_backend_destroy(backend);
        }
    }

    storage_backend_t *backend = nullptr;
    heap_vector_store_t *store = nullptr;
};

// ============================================================
// 基础测试：创建与销毁
// ============================================================

TEST_F(HeapVectorStoreTest, CreateAndDestroy)
{
    // SetUp/TearDown 已验证基本创建/销毁
    EXPECT_NE(store, nullptr);
    EXPECT_EQ(heap_vector_dims(store), DIMS);
    EXPECT_GT(heap_vector_capacity_per_page(store), 0);
    EXPECT_EQ(heap_vector_count(store), 0);
}

TEST_F(HeapVectorStoreTest, CreateWithNullConfig)
{
    heap_vector_store_t *s = heap_vector_store_create(nullptr);
    EXPECT_EQ(s, nullptr);
}

TEST_F(HeapVectorStoreTest, CreateWithNullBackend)
{
    heap_vector_config_t config = {};
    config.backend = nullptr;
    config.dims = DIMS;
    config.page_size = PAGE_SIZE;

    heap_vector_store_t *s = heap_vector_store_create(&config);
    EXPECT_EQ(s, nullptr);
}

TEST_F(HeapVectorStoreTest, CreateWithInvalidDims)
{
    heap_vector_config_t config = {};
    config.backend = backend;
    config.dims = 0;
    config.page_size = PAGE_SIZE;

    heap_vector_store_t *s = heap_vector_store_create(&config);
    EXPECT_EQ(s, nullptr);
}

TEST_F(HeapVectorStoreTest, CreateWithMismatchedPageSize)
{
    heap_vector_config_t config = {};
    config.backend = backend;
    config.dims = DIMS;
    config.page_size = 4096;  // 与 backend 的 8192 不匹配

    heap_vector_store_t *s = heap_vector_store_create(&config);
    EXPECT_EQ(s, nullptr);
}

// ============================================================
// 单向量插入与读取
// ============================================================

TEST_F(HeapVectorStoreTest, InsertAndGetSingleVector)
{
    // 构造测试向量
    float vec[DIMS];
    for (int i = 0; i < DIMS; i++) {
        vec[i] = static_cast<float>(i);
    }

    // 插入向量
    vector_ref_t ref = heap_vector_insert(store, vec);
    EXPECT_TRUE(vector_ref_is_valid(&ref));
    EXPECT_EQ(heap_vector_count(store), 1);

    // 读取向量
    float retrieved[DIMS];
    int ret = heap_vector_get(store, &ref, retrieved);
    EXPECT_EQ(ret, 0);

    // 验证数据一致性
    for (int i = 0; i < DIMS; i++) {
        EXPECT_FLOAT_EQ(vec[i], retrieved[i]);
    }
}

TEST_F(HeapVectorStoreTest, InsertWithNullVector)
{
    vector_ref_t ref = heap_vector_insert(store, nullptr);
    EXPECT_FALSE(vector_ref_is_valid(&ref));
    EXPECT_EQ(heap_vector_count(store), 0);
}

TEST_F(HeapVectorStoreTest, GetWithInvalidRef)
{
    float vec[DIMS] = {0};
    vector_ref_t invalid_ref = vector_ref_make_invalid();

    int ret = heap_vector_get(store, &invalid_ref, vec);
    EXPECT_NE(ret, 0);
}

TEST_F(HeapVectorStoreTest, GetWithNullOutput)
{
    float vec[DIMS];
    for (int i = 0; i < DIMS; i++) {
        vec[i] = static_cast<float>(i);
    }

    vector_ref_t ref = heap_vector_insert(store, vec);
    EXPECT_TRUE(vector_ref_is_valid(&ref));

    int ret = heap_vector_get(store, &ref, nullptr);
    EXPECT_NE(ret, 0);
}

// ============================================================
// 批量操作
// ============================================================

TEST_F(HeapVectorStoreTest, InsertBatch)
{
    const int32_t COUNT = 100;

    // 构造批量向量
    float vectors[COUNT * DIMS];
    for (int i = 0; i < COUNT * DIMS; i++) {
        vectors[i] = static_cast<float>(i);
    }

    // 批量插入
    vector_ref_t refs[COUNT];
    int ret = heap_vector_insert_batch(store, vectors, COUNT, refs);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(heap_vector_count(store), COUNT);

    // 验证所有引用有效
    for (int i = 0; i < COUNT; i++) {
        EXPECT_TRUE(vector_ref_is_valid(&refs[i]));
    }
}

TEST_F(HeapVectorStoreTest, GetBatch)
{
    const int32_t COUNT = 50;

    // 构造并插入向量
    float vectors[COUNT * DIMS];
    for (int i = 0; i < COUNT * DIMS; i++) {
        vectors[i] = static_cast<float>(i);
    }

    vector_ref_t refs[COUNT];
    int ret = heap_vector_insert_batch(store, vectors, COUNT, refs);
    EXPECT_EQ(ret, 0);

    // 批量读取
    float retrieved[COUNT * DIMS];
    ret = heap_vector_get_batch(store, refs, COUNT, retrieved);
    EXPECT_EQ(ret, 0);

    // 验证数据一致性
    for (int i = 0; i < COUNT * DIMS; i++) {
        EXPECT_FLOAT_EQ(vectors[i], retrieved[i]);
    }
}

TEST_F(HeapVectorStoreTest, InsertBatchWithNullPointers)
{
    const int32_t COUNT = 10;
    vector_ref_t refs[COUNT];

    int ret = heap_vector_insert_batch(store, nullptr, COUNT, refs);
    EXPECT_NE(ret, 0);

    ret = heap_vector_insert_batch(store, nullptr, COUNT, nullptr);
    EXPECT_NE(ret, 0);
}

// ============================================================
// 跨页测试
// ============================================================

TEST_F(HeapVectorStoreTest, MultipleInsertAcrossPages)
{
    // 计算每页可容纳的向量数
    int vpp = heap_vector_capacity_per_page(store);
    EXPECT_GT(vpp, 0);

    // 插入超过一页容量的向量
    const int32_t TOTAL = vpp * 3 + 10;  // 约 3 页多
    float *vectors = new float[TOTAL * DIMS];

    for (int i = 0; i < TOTAL * DIMS; i++) {
        vectors[i] = static_cast<float>(i);
    }

    // 逐个插入
    vector_ref_t *refs = new vector_ref_t[TOTAL];
    for (int i = 0; i < TOTAL; i++) {
        refs[i] = heap_vector_insert(store, vectors + i * DIMS);
        EXPECT_TRUE(vector_ref_is_valid(&refs[i])) << "Failed at vector " << i;
    }

    EXPECT_EQ(heap_vector_count(store), TOTAL);

    // 验证所有向量可正确读取
    float *retrieved = new float[DIMS];
    for (int i = 0; i < TOTAL; i++) {
        int ret = heap_vector_get(store, &refs[i], retrieved);
        EXPECT_EQ(ret, 0) << "Failed to read vector " << i;

        for (int j = 0; j < DIMS; j++) {
            EXPECT_FLOAT_EQ(vectors[i * DIMS + j], retrieved[j])
                << "Mismatch at vector " << i << " dim " << j;
        }
    }

    delete[] retrieved;
    delete[] refs;
    delete[] vectors;
}

TEST_F(HeapVectorStoreTest, VerifyPageBoundary)
{
    int vpp = heap_vector_capacity_per_page(store);

    // 插入恰好填满一页的向量
    float *vectors = new float[vpp * DIMS];
    for (int i = 0; i < vpp * DIMS; i++) {
        vectors[i] = static_cast<float>(i);
    }

    vector_ref_t *refs = new vector_ref_t[vpp];
    for (int i = 0; i < vpp; i++) {
        refs[i] = heap_vector_insert(store, vectors + i * DIMS);
        EXPECT_TRUE(vector_ref_is_valid(&refs[i]));
    }

    // 验证第一个和最后一个向量（页边界）
    float retrieved[DIMS];

    // 第一个向量
    int ret = heap_vector_get(store, &refs[0], retrieved);
    EXPECT_EQ(ret, 0);
    for (int j = 0; j < DIMS; j++) {
        EXPECT_FLOAT_EQ(vectors[j], retrieved[j]);
    }

    // 最后一个向量（该页边界）
    ret = heap_vector_get(store, &refs[vpp - 1], retrieved);
    EXPECT_EQ(ret, 0);
    for (int j = 0; j < DIMS; j++) {
        EXPECT_FLOAT_EQ(vectors[(vpp - 1) * DIMS + j], retrieved[j]);
    }

    // 下一个向量应该在新页
    float next_vec[DIMS];
    for (int i = 0; i < DIMS; i++) {
        next_vec[i] = static_cast<float>(vpp * DIMS + i);
    }
    vector_ref_t next_ref = heap_vector_insert(store, next_vec);
    EXPECT_TRUE(vector_ref_is_valid(&next_ref));

    // 新页的向量应该有不同的 page_id
    EXPECT_NE(refs[0].heap_page_id, next_ref.heap_page_id);

    delete[] refs;
    delete[] vectors;
}

// ============================================================
// 引用有效性检查
// ============================================================

TEST_F(HeapVectorStoreTest, VectorRefValidity)
{
    float vec[DIMS];
    for (int i = 0; i < DIMS; i++) {
        vec[i] = static_cast<float>(i);
    }

    vector_ref_t ref = heap_vector_insert(store, vec);
    EXPECT_TRUE(vector_ref_is_valid(&ref));
    EXPECT_GE(ref.heap_page_id, 0);
    EXPECT_GT(ref.length, 0u);
    EXPECT_GE(ref.offset, HEAP_VECTOR_PAGE_HEADER_SIZE);
}

TEST_F(HeapVectorStoreTest, VectorRefEquality)
{
    float vec[DIMS];
    for (int i = 0; i < DIMS; i++) {
        vec[i] = static_cast<float>(i);
    }

    vector_ref_t ref1 = heap_vector_insert(store, vec);
    vector_ref_t ref2 = heap_vector_insert(store, vec);

    // 两个不同的插入应产生不同的引用
    EXPECT_FALSE(vector_ref_equal(&ref1, &ref2));
}

// ============================================================
// 查询接口测试
// ============================================================

TEST_F(HeapVectorStoreTest, QueryDimensions)
{
    EXPECT_EQ(heap_vector_dims(store), DIMS);
    EXPECT_EQ(heap_vector_dims(nullptr), 0);
}

TEST_F(HeapVectorStoreTest, QueryCount)
{
    EXPECT_EQ(heap_vector_count(store), 0);

    float vec[DIMS] = {0};
    heap_vector_insert(store, vec);
    EXPECT_EQ(heap_vector_count(store), 1);

    heap_vector_insert(store, vec);
    EXPECT_EQ(heap_vector_count(store), 2);

    EXPECT_EQ(heap_vector_count(nullptr), 0);
}

TEST_F(HeapVectorStoreTest, QueryCapacityPerPage)
{
    int vpp = heap_vector_capacity_per_page(store);
    EXPECT_GT(vpp, 0);

    // 验证计算：页面大小 - 头部 = 可用空间，可用空间 / 向量大小 = 槽数
    size_t vector_bytes = static_cast<size_t>(DIMS) * sizeof(float);
    size_t available = PAGE_SIZE - HEAP_VECTOR_PAGE_HEADER_SIZE;
    int expected = static_cast<int>(available / vector_bytes);
    EXPECT_EQ(vpp, expected);

    EXPECT_EQ(heap_vector_capacity_per_page(nullptr), 0);
}

// ============================================================
// 边界条件测试
// ============================================================

TEST_F(HeapVectorStoreTest, ReadNonExistentPage)
{
    vector_ref_t fake_ref;
    fake_ref.heap_page_id = 9999;  // 不存在的页面
    fake_ref.offset = HEAP_VECTOR_PAGE_HEADER_SIZE;
    fake_ref.length = static_cast<uint32_t>(DIMS * sizeof(float));

    float retrieved[DIMS];
    int ret = heap_vector_get(store, &fake_ref, retrieved);
    EXPECT_NE(ret, 0);  // 应该失败
}

TEST_F(HeapVectorStoreTest, ReadWithWrongLength)
{
    float vec[DIMS] = {0};
    vector_ref_t ref = heap_vector_insert(store, vec);

    // 篡改长度
    vector_ref_t bad_ref = ref;
    bad_ref.length = 100;  // 错误长度

    float retrieved[DIMS];
    int ret = heap_vector_get(store, &bad_ref, retrieved);
    EXPECT_NE(ret, 0);
}

TEST_F(HeapVectorStoreTest, ReadWithWrongOffset)
{
    float vec[DIMS] = {0};
    vector_ref_t ref = heap_vector_insert(store, vec);

    // 篡改偏移（非槽位对齐）
    vector_ref_t bad_ref = ref;
    bad_ref.offset = HEAP_VECTOR_PAGE_HEADER_SIZE + 7;  // 非对齐偏移

    float retrieved[DIMS];
    int ret = heap_vector_get(store, &bad_ref, retrieved);
    EXPECT_NE(ret, 0);
}

// ============================================================
// 性能相关测试
// ============================================================

TEST_F(HeapVectorStoreTest, LargeBatchInsert)
{
    const int32_t COUNT = 1000;

    float *vectors = new float[COUNT * DIMS];
    for (int i = 0; i < COUNT * DIMS; i++) {
        vectors[i] = static_cast<float>(i);
    }

    vector_ref_t *refs = new vector_ref_t[COUNT];

    // 批量插入
    int ret = heap_vector_insert_batch(store, vectors, COUNT, refs);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(heap_vector_count(store), COUNT);

    // 随机抽查几个向量
    float retrieved[DIMS];
    int check_indices[] = {0, COUNT / 4, COUNT / 2, COUNT * 3 / 4, COUNT - 1};

    for (int idx : check_indices) {
        ret = heap_vector_get(store, &refs[idx], retrieved);
        EXPECT_EQ(ret, 0);

        for (int j = 0; j < DIMS; j++) {
            EXPECT_FLOAT_EQ(vectors[idx * DIMS + j], retrieved[j]);
        }
    }

    delete[] refs;
    delete[] vectors;
}
