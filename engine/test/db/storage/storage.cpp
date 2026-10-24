/**
 * @file test_storage.cpp
 * @brief 存储引擎测试
 */

#include <gtest/gtest.h>
#include "db/disk.h"
#include "db/buffer.h"
#include "db/page.h"
#include <cstdio>
#include <vector>

// 测试夹具
class StorageTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove("test_db.bin");
    }

    void TearDown() override {
        std::remove("test_db.bin");
    }
};

// ============================================================
// 页面测试
// ============================================================

TEST(PageTest, CreateAndFree) {
    page_t *page = page_create(1, PAGE_DATA);
    ASSERT_NE(nullptr, page);
    EXPECT_EQ(1u, page->header.page_id);
    EXPECT_EQ(PAGE_DATA, page->header.page_type);
    EXPECT_TRUE(page_verify_checksum(page));
    page_free(page);
}

TEST(PageTest, WriteAndReadData) {
    page_t *page = page_create(1, PAGE_DATA);
    ASSERT_NE(nullptr, page);

    const char *test_data = "Hello, Database!";
    size_t data_len = strlen(test_data) + 1;

    uint16_t offset = page_alloc_space(page, data_len);
    ASSERT_NE((uint16_t)-1, offset);

    page_write_data(page, offset, test_data, data_len);

    char read_data[64] = {0};
    page_read_data(page, offset, read_data, data_len);

    EXPECT_STREQ(test_data, read_data);
    page_free(page);
}

// ============================================================
// 磁盘文件测试
// ============================================================

TEST(DiskTest, CreateAndOpen) {
    // 先创建
    {
        db_file_t *file = disk_open("test_db.bin", 8192);
        ASSERT_NE(nullptr, file);
        disk_close(file);
    }

    // 再打开
    {
        db_file_t *file = disk_open("test_db.bin", 8192);
        ASSERT_NE(nullptr, file);
        EXPECT_EQ(8192u, disk_get_page_size(file));
        disk_close(file);
    }
}

TEST(DiskTest, PageAllocation) {
    db_file_t *file = disk_open("test_db.bin", 8192);
    ASSERT_NE(nullptr, file);

    // 分配页面
    page_id_t page_id1;
    page_t *page1 = disk_alloc_page(file, PAGE_DATA, &page_id1);
    ASSERT_NE(nullptr, page1);
    EXPECT_EQ(PAGE_DATA, page1->header.page_type);
    page_free(page1);

    // 再分配一个
    page_id_t page_id2;
    page_t *page2 = disk_alloc_page(file, PAGE_INDEX, &page_id2);
    ASSERT_NE(nullptr, page2);
    EXPECT_GT(page_id2, page_id1);  // 后分配的 ID 更大
    page_free(page2);

    disk_close(file);
}

TEST(DiskTest, PageReadWrite) {
    db_file_t *file = disk_open("test_db.bin", 8192);
    ASSERT_NE(nullptr, file);

    // 分配页面
    page_id_t page_id;
    page_t *orig_page = disk_alloc_page(file, PAGE_DATA, &page_id);
    ASSERT_NE(nullptr, orig_page);

    // 写入数据
    const char *test_data = "Test data for disk I/O";
    size_t data_len = strlen(test_data) + 1;
    uint16_t offset = page_alloc_space(orig_page, data_len);
    page_write_data(orig_page, offset, test_data, data_len);

    ASSERT_EQ(0, disk_write_page(file, page_id, orig_page));
    page_free(orig_page);

    // 读取页面
    page_t *read_page = disk_read_page(file, page_id);
    ASSERT_NE(nullptr, read_page);

    char read_data[64] = {0};
    page_read_data(read_page, offset, read_data, data_len);
    EXPECT_STREQ(test_data, read_data);

    page_free(read_page);
    disk_close(file);
}

// ============================================================
// 缓存池测试
// ============================================================

TEST(BufferTest, CreateAndDestroy) {
    db_file_t *file = disk_open("test_db.bin", 8192);
    ASSERT_NE(nullptr, file);

    buffer_pool_t *pool = buffer_create(file, 100);
    ASSERT_NE(nullptr, pool);

    buffer_destroy(pool);
    disk_close(file);
}

TEST(BufferTest, GetAndUnpin) {
    db_file_t *file = disk_open("test_db.bin", 8192);
    ASSERT_NE(nullptr, file);

    buffer_pool_t *pool = buffer_create(file, 100);
    ASSERT_NE(nullptr, pool);

    // 分配一个页面
    page_id_t page_id;
    page_t *page = buffer_alloc_page(pool, PAGE_DATA, &page_id);
    ASSERT_NE(nullptr, page);

    // 写入数据
    const char *test_data = "Buffer pool test";
    size_t data_len = strlen(test_data) + 1;
    uint16_t offset = page_alloc_space(page, data_len);
    page_write_data(page, offset, test_data, data_len);
    buffer_mark_dirty(pool, page_id);

    // 释放页面
    buffer_unpin_page(pool, page_id);

    // 再次获取
    page_t *read_page = buffer_get_page(pool, page_id);
    ASSERT_NE(nullptr, read_page);

    char read_data[64] = {0};
    page_read_data(read_page, offset, read_data, data_len);
    EXPECT_STREQ(test_data, read_data);

    buffer_unpin_page(pool, page_id);
    buffer_destroy(pool);
    disk_close(file);
}

TEST(BufferTest, FlushDirtyPages) {
    db_file_t *file = disk_open("test_db.bin", 8192);
    ASSERT_NE(nullptr, file);

    buffer_pool_t *pool = buffer_create(file, 100);
    ASSERT_NE(nullptr, pool);

    // 分配多个页面并标记为脏
    std::vector<page_id_t> page_ids;
    for (int i = 0; i < 5; i++) {
        page_id_t page_id;
        page_t *page = buffer_alloc_page(pool, PAGE_DATA, &page_id);
        ASSERT_NE(nullptr, page);
        buffer_mark_dirty(pool, page_id);
        buffer_unpin_page(pool, page_id);
        page_ids.push_back(page_id);
    }

    // 刷所有脏页
    ASSERT_EQ(0, buffer_flush_all(pool));

    // 验证脏页数
    size_t dirty_count;
    buffer_get_stats(pool, nullptr, nullptr, &dirty_count);
    EXPECT_EQ(0u, dirty_count);

    buffer_destroy(pool);
    disk_close(file);
}
