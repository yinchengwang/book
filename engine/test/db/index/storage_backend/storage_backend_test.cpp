/**
 * @file storage_backend_test.cpp
 * @brief 存储后端抽象层单元测试
 *
 * 测试覆盖：
 * - Memory 后端：创建、页面分配、读写、销毁
 * - PageFile 后端：创建、页面分配、读写、持久化、销毁
 * - MMap 后端：创建、页面分配、读写、持久化、销毁
 * - 参数校验：无效参数、边界条件
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

extern "C" {
#include <db/index/storage_backend.h>
}

// 测试常量
static constexpr size_t PAGE_SIZE = 8192;
static constexpr const char *TEST_DIR = "test_results/engineering/storage_backend";

class StorageBackendTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试目录 - 使用跨平台方式
#ifdef _WIN32
        _mkdir("test_results");
        _mkdir("test_results\\engineering");
        _mkdir("test_results\\engineering\\storage_backend");
#else
        mkdir("test_results/engineering/storage_backend", 0755);
#endif
    }

    void TearDown() override {
        // 清理测试文件
        remove("test_results/engineering/storage_backend/test_pagefile.idx");
        remove("test_results/engineering/storage_backend/test_mmap.idx");
    }
};

// ============================================================
// Memory 后端测试
// ============================================================

TEST_F(StorageBackendTest, MemoryBackendCreateAndDestroy)
{
    storage_backend_t *backend = storage_memory_create(PAGE_SIZE);
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type, STORAGE_BACKEND_MEMORY);
    EXPECT_EQ(backend->page_size, PAGE_SIZE);
    EXPECT_NE(backend->ops, nullptr);

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, MemoryBackendCreateWithDefaultPageSize)
{
    storage_backend_t *backend = storage_memory_create(0);
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->page_size, 8192);  // 默认页面大小

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, MemoryBackendCreateWithInvalidPageSize)
{
    // 页面大小太小
    storage_backend_t *backend1 = storage_memory_create(STORAGE_BACKEND_MIN_PAGE_SIZE - 1);
    EXPECT_EQ(backend1, nullptr);

    // 页面大小太大
    storage_backend_t *backend2 = storage_memory_create(STORAGE_BACKEND_MAX_PAGE_SIZE + 1);
    EXPECT_EQ(backend2, nullptr);
}

TEST_F(StorageBackendTest, MemoryBackendAllocPage)
{
    storage_backend_t *backend = storage_memory_create(PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    page_id_t page1 = backend->ops->alloc_page(backend->ctx);
    EXPECT_NE(page1, INVALID_PAGE_ID);
    EXPECT_EQ(page1, 0);  // 第一个页面 ID 应为 0

    page_id_t page2 = backend->ops->alloc_page(backend->ctx);
    EXPECT_NE(page2, INVALID_PAGE_ID);
    EXPECT_EQ(page2, 1);  // 第二个页面 ID 应为 1

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, MemoryBackendReadWritePage)
{
    storage_backend_t *backend = storage_memory_create(PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    // 分配页面
    page_id_t page_id = backend->ops->alloc_page(backend->ctx);
    ASSERT_NE(page_id, INVALID_PAGE_ID);

    // 准备测试数据
    char write_data[PAGE_SIZE];
    memset(write_data, 0xAB, PAGE_SIZE);
    strncpy(write_data, "test data", PAGE_SIZE - 1);

    // 写入页面
    int write_ret = backend->ops->write_page(backend->ctx, page_id,
                                              (page_t *)write_data);
    EXPECT_EQ(write_ret, 0);

    // 读取页面
    char read_data[PAGE_SIZE];
    int read_ret = backend->ops->read_page(backend->ctx, page_id,
                                            (page_t *)read_data);
    EXPECT_EQ(read_ret, 0);

    // 验证数据
    EXPECT_EQ(memcmp(write_data, read_data, PAGE_SIZE), 0);

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, MemoryBackendReadInvalidPage)
{
    storage_backend_t *backend = storage_memory_create(PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    char read_data[PAGE_SIZE];
    int ret = backend->ops->read_page(backend->ctx, 999, (page_t *)read_data);
    EXPECT_NE(ret, 0);  // 读取无效页面应失败

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, MemoryBackendBatchWrite)
{
    storage_backend_t *backend = storage_memory_create(PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    // 分配多个页面
    page_id_t page_ids[3];
    for (int i = 0; i < 3; i++) {
        page_ids[i] = backend->ops->alloc_page(backend->ctx);
        ASSERT_NE(page_ids[i], INVALID_PAGE_ID);
    }

    // 准备数据
    char pages[3][PAGE_SIZE];
    const page_t *page_ptrs[3] = {
        (const page_t *)pages[0],
        (const page_t *)pages[1],
        (const page_t *)pages[2]
    };

    for (int i = 0; i < 3; i++) {
        memset(pages[i], (char)('A' + i), PAGE_SIZE);
    }

    // 批量写入
    int ret = backend->ops->batch_write(backend->ctx, page_ids, page_ptrs, 3);
    EXPECT_EQ(ret, 0);

    // 验证每个页面
    for (int i = 0; i < 3; i++) {
        char read_data[PAGE_SIZE];
        backend->ops->read_page(backend->ctx, page_ids[i], (page_t *)read_data);
        EXPECT_EQ(memcmp(pages[i], read_data, PAGE_SIZE), 0);
    }

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, MemoryBackendSyncAndFlush)
{
    storage_backend_t *backend = storage_memory_create(PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    // 内存后端的 sync 和 flush 应该总是成功
    EXPECT_EQ(backend->ops->sync(backend->ctx), 0);

    page_id_t page_id = backend->ops->alloc_page(backend->ctx);
    EXPECT_EQ(backend->ops->flush_page(backend->ctx, page_id), 0);

    storage_backend_destroy(backend);
}

// ============================================================
// PageFile 后端测试
// ============================================================

TEST_F(StorageBackendTest, PageFileBackendCreateAndDestroy)
{
    const char *path = "test_results/engineering/storage_backend/test_pagefile.idx";

    storage_backend_t *backend = storage_page_file_create(path, PAGE_SIZE);
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type, STORAGE_BACKEND_PAGE_FILE);
    EXPECT_EQ(backend->page_size, PAGE_SIZE);
    EXPECT_NE(backend->ops, nullptr);

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, PageFileBackendCreateWithNullPath)
{
    storage_backend_t *backend = storage_page_file_create(nullptr, PAGE_SIZE);
    EXPECT_EQ(backend, nullptr);
}

TEST_F(StorageBackendTest, PageFileBackendAllocPage)
{
    const char *path = "test_results/engineering/storage_backend/test_pagefile.idx";

    storage_backend_t *backend = storage_page_file_create(path, PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    page_id_t page1 = backend->ops->alloc_page(backend->ctx);
    EXPECT_NE(page1, INVALID_PAGE_ID);

    page_id_t page2 = backend->ops->alloc_page(backend->ctx);
    EXPECT_NE(page2, INVALID_PAGE_ID);

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, PageFileBackendReadWritePage)
{
    const char *path = "test_results/engineering/storage_backend/test_pagefile.idx";

    storage_backend_t *backend = storage_page_file_create(path, PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    // 分配页面
    page_id_t page_id = backend->ops->alloc_page(backend->ctx);
    ASSERT_NE(page_id, INVALID_PAGE_ID);

    // 准备测试数据
    char write_data[PAGE_SIZE];
    memset(write_data, 0, PAGE_SIZE);
    strncpy(write_data, "pagefile test data", PAGE_SIZE - 1);

    // 写入页面
    int write_ret = backend->ops->write_page(backend->ctx, page_id,
                                              (page_t *)write_data);
    EXPECT_EQ(write_ret, 0);

    // 刷盘
    EXPECT_EQ(backend->ops->flush_page(backend->ctx, page_id), 0);

    // 读取页面
    char read_data[PAGE_SIZE];
    int read_ret = backend->ops->read_page(backend->ctx, page_id,
                                            (page_t *)read_data);
    EXPECT_EQ(read_ret, 0);

    // 验证数据
    EXPECT_EQ(memcmp(write_data, read_data, PAGE_SIZE), 0);

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, PageFileBackendSync)
{
    const char *path = "test_results/engineering/storage_backend/test_pagefile.idx";

    storage_backend_t *backend = storage_page_file_create(path, PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    // 分配并写入页面
    page_id_t page_id = backend->ops->alloc_page(backend->ctx);
    char data[PAGE_SIZE] = {0};
    backend->ops->write_page(backend->ctx, page_id, (page_t *)data);

    // 同步
    int ret = backend->ops->sync(backend->ctx);
    EXPECT_EQ(ret, 0);

    storage_backend_destroy(backend);
}

// ============================================================
// MMap 后端测试
// ============================================================

TEST_F(StorageBackendTest, MMapBackendCreateAndDestroy)
{
    const char *path = "test_results/engineering/storage_backend/test_mmap.idx";

    storage_backend_t *backend = storage_mmap_create(path, PAGE_SIZE);
    ASSERT_NE(backend, nullptr);
    EXPECT_EQ(backend->type, STORAGE_BACKEND_MMAP);
    EXPECT_EQ(backend->page_size, PAGE_SIZE);
    EXPECT_NE(backend->ops, nullptr);

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, MMapBackendCreateWithNullPath)
{
    storage_backend_t *backend = storage_mmap_create(nullptr, PAGE_SIZE);
    EXPECT_EQ(backend, nullptr);
}

TEST_F(StorageBackendTest, MMapBackendAllocPage)
{
    const char *path = "test_results/engineering/storage_backend/test_mmap.idx";

    storage_backend_t *backend = storage_mmap_create(path, PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    page_id_t page1 = backend->ops->alloc_page(backend->ctx);
    EXPECT_NE(page1, INVALID_PAGE_ID);

    page_id_t page2 = backend->ops->alloc_page(backend->ctx);
    EXPECT_NE(page2, INVALID_PAGE_ID);

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, MMapBackendReadWritePage)
{
    const char *path = "test_results/engineering/storage_backend/test_mmap.idx";

    storage_backend_t *backend = storage_mmap_create(path, PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    // 分配页面
    page_id_t page_id = backend->ops->alloc_page(backend->ctx);
    ASSERT_NE(page_id, INVALID_PAGE_ID);

    // 准备测试数据
    char write_data[PAGE_SIZE];
    memset(write_data, 0, PAGE_SIZE);
    strncpy(write_data, "mmap test data", PAGE_SIZE - 1);

    // 写入页面
    int write_ret = backend->ops->write_page(backend->ctx, page_id,
                                              (page_t *)write_data);
    EXPECT_EQ(write_ret, 0);

    // 读取页面
    char read_data[PAGE_SIZE];
    int read_ret = backend->ops->read_page(backend->ctx, page_id,
                                            (page_t *)read_data);
    EXPECT_EQ(read_ret, 0);

    // 验证数据
    EXPECT_EQ(memcmp(write_data, read_data, PAGE_SIZE), 0);

    storage_backend_destroy(backend);
}

TEST_F(StorageBackendTest, MMapBackendSync)
{
    const char *path = "test_results/engineering/storage_backend/test_mmap.idx";

    storage_backend_t *backend = storage_mmap_create(path, PAGE_SIZE);
    ASSERT_NE(backend, nullptr);

    // 分配并写入页面
    page_id_t page_id = backend->ops->alloc_page(backend->ctx);
    char data[PAGE_SIZE] = {0};
    backend->ops->write_page(backend->ctx, page_id, (page_t *)data);

    // 同步
    int ret = backend->ops->sync(backend->ctx);
    EXPECT_EQ(ret, 0);

    storage_backend_destroy(backend);
}

// ============================================================
// 工具函数测试
// ============================================================

TEST_F(StorageBackendTest, StorageBackendTypeName)
{
    EXPECT_STREQ(storage_backend_type_name(STORAGE_BACKEND_MEMORY), "MEMORY");
    EXPECT_STREQ(storage_backend_type_name(STORAGE_BACKEND_PAGE_FILE), "PAGE_FILE");
    EXPECT_STREQ(storage_backend_type_name(STORAGE_BACKEND_MMAP), "MMAP");
    EXPECT_STREQ(storage_backend_type_name(STORAGE_BACKEND_FAISS), "FAISS");
}

// ============================================================
// 销毁空指针测试
// ============================================================

TEST_F(StorageBackendTest, DestroyNullBackend)
{
    // 销毁 NULL 指针应该是安全的空操作
    storage_backend_destroy(nullptr);
    SUCCEED();
}
