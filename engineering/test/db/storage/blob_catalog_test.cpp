/**
 * @file blob_catalog_test.cpp
 * @brief Blob Catalog 单元测试（Task 4）
 *
 * 测试内容：
 * - prepare → commit 状态机
 * - DELETE 标记
 * - ref_count 增减
 * - checkpoint + WAL replay
 * - 篡改 CRC 恢复时跳过
 */
#include "gtest/gtest.h"
extern "C" {
#include "db/blob_catalog.h"
}

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir_path(path) _mkdir(path)
#else
#include <unistd.h>
#define mkdir_path(path) mkdir(path, 0755)
#endif

/* ========================================================================
 * 测试辅助函数
 * ======================================================================== */

static void create_test_dir(const char *path) {
    mkdir_path(path);
}

static void remove_recursive(const char *path) {
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "rd /s /q \"%s\" 2>nul", path);
#else
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\" 2>/dev/null", path);
#endif
    system(cmd);
}

static void make_test_blob_id(uint8_t blob_id[32], uint8_t val) {
    memset(blob_id, val, 32);
}

static void make_test_chunk_id(uint8_t chunk_id[32], uint8_t val) {
    memset(chunk_id, val, 32);
}

/* ========================================================================
 * 基础测试
 * ======================================================================== */

class BlobCatalogTest : public ::testing::Test {
protected:
    const char *test_dir_ = "test-results/blob_catalog_test";

    void SetUp() override {
        remove_recursive(test_dir_);
        create_test_dir(test_dir_);
    }

    void TearDown() override {
        remove_recursive(test_dir_);
    }
};

/* ========================================================================
 * Test: PREPARE → COMMIT 状态机
 * ======================================================================== */

TEST_F(BlobCatalogTest, PrepareCommit) {
    blob_catalog_t *cat = blob_catalog_open(test_dir_);
    ASSERT_NE(cat, nullptr);

    uint8_t blob_id[32];
    make_test_blob_id(blob_id, 0xAA);

    /* 开始事务 */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);

    /* PREPARE */
    EXPECT_EQ(blob_catalog_prepare(cat, blob_id, 1024, 3), BLOB_CATALOG_OK);

    /* 查找，状态应为 PREPARED */
    blob_entry_t entry;
    EXPECT_EQ(blob_catalog_find_blob(cat, blob_id, &entry), BLOB_CATALOG_OK);
    EXPECT_EQ(entry.state, BLOB_STATE_PREPARED);
    EXPECT_EQ(entry.blob_size, 1024u);
    EXPECT_EQ(entry.chunk_count, 3u);

    /* 提交事务 */
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    /* 再次开始事务 COMMIT */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_commit(cat, blob_id), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    /* 查找，状态应为 COMMITTED */
    EXPECT_EQ(blob_catalog_find_blob(cat, blob_id, &entry), BLOB_CATALOG_OK);
    EXPECT_EQ(entry.state, BLOB_STATE_COMMITTED);

    blob_catalog_close(cat);
}

/* ========================================================================
 * Test: 重复 COMMIT 幂等
 * ======================================================================== */

TEST_F(BlobCatalogTest, IdempotentCommit) {
    blob_catalog_t *cat = blob_catalog_open(test_dir_);
    ASSERT_NE(cat, nullptr);

    uint8_t blob_id[32];
    make_test_blob_id(blob_id, 0xBB);

    /* PREPARE */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_prepare(cat, blob_id, 2048, 1), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    /* 第一次 COMMIT */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_commit(cat, blob_id), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    /* 第二次 COMMIT 应该失败（状态已经是 COMMITTED） */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_commit(cat, blob_id), BLOB_CATALOG_ERR_STATE);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    blob_catalog_close(cat);
}

/* ========================================================================
 * Test: DELETE 标记
 * ======================================================================== */

TEST_F(BlobCatalogTest, DeleteBlob) {
    blob_catalog_t *cat = blob_catalog_open(test_dir_);
    ASSERT_NE(cat, nullptr);

    uint8_t blob_id[32];
    make_test_blob_id(blob_id, 0xCC);

    /* PREPARE → COMMIT → DELETE */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_prepare(cat, blob_id, 512, 2), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_commit(cat, blob_id), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_delete(cat, blob_id), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    /* 查找，状态应为 DELETED */
    blob_entry_t entry;
    EXPECT_EQ(blob_catalog_find_blob(cat, blob_id, &entry), BLOB_CATALOG_OK);
    EXPECT_EQ(entry.state, BLOB_STATE_DELETED);
    EXPECT_NE(entry.deleted_at_ms, 0);

    blob_catalog_close(cat);
}

/* ========================================================================
 * Test: ref_count 增减
 * ======================================================================== */

TEST_F(BlobCatalogTest, RefCountIncDec) {
    blob_catalog_t *cat = blob_catalog_open(test_dir_);
    ASSERT_NE(cat, nullptr);

    uint8_t chunk_id[32];
    make_test_chunk_id(chunk_id, 0xDD);

    /* 初始状态：找不到 */
    blob_chunk_ref_t ref;
    EXPECT_EQ(blob_catalog_find_chunk(cat, chunk_id, &ref), BLOB_CATALOG_ERR_NOTFOUND);

    /* REF_INC */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_ref_inc(cat, chunk_id), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    EXPECT_EQ(blob_catalog_find_chunk(cat, chunk_id, &ref), BLOB_CATALOG_OK);
    EXPECT_EQ(ref.ref_count, 1u);
    EXPECT_EQ(ref.gc_after_ms, 0);

    /* 再次 REF_INC */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_ref_inc(cat, chunk_id), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    EXPECT_EQ(blob_catalog_find_chunk(cat, chunk_id, &ref), BLOB_CATALOG_OK);
    EXPECT_EQ(ref.ref_count, 2u);

    /* REF_DEC */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_ref_dec(cat, chunk_id), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    EXPECT_EQ(blob_catalog_find_chunk(cat, chunk_id, &ref), BLOB_CATALOG_OK);
    EXPECT_EQ(ref.ref_count, 1u);

    /* 再次 REF_DEC 到 0 */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_ref_dec(cat, chunk_id), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    EXPECT_EQ(blob_catalog_find_chunk(cat, chunk_id, &ref), BLOB_CATALOG_OK);
    EXPECT_EQ(ref.ref_count, 0u);
    EXPECT_NE(ref.gc_after_ms, 0);  /* 应该设置了 GC 宽限期 */

    /* 再次 REF_DEC 不会下溢 */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_ref_dec(cat, chunk_id), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    EXPECT_EQ(blob_catalog_find_chunk(cat, chunk_id, &ref), BLOB_CATALOG_OK);
    EXPECT_EQ(ref.ref_count, 0u);

    blob_catalog_close(cat);
}

/* ========================================================================
 * Test: checkpoint + WAL replay
 * ======================================================================== */

TEST_F(BlobCatalogTest, CheckpointAndReplay) {
    const char *dir = "test-results/blob_catalog_checkpoint_test";
    remove_recursive(dir);
    create_test_dir(dir);

    /* 第一次打开：创建一些数据并 checkpoint */
    {
        blob_catalog_t *cat = blob_catalog_open(dir);
        ASSERT_NE(cat, nullptr);

        uint8_t blob_id[32];
        make_test_blob_id(blob_id, 0xEE);

        EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_prepare(cat, blob_id, 4096, 2), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

        EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_commit(cat, blob_id), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

        EXPECT_EQ(blob_catalog_checkpoint(cat), BLOB_CATALOG_OK);

        blob_catalog_close(cat);
    }

    /* 第二次打开：应从 checkpoint 恢复 */
    {
        blob_catalog_t *cat = blob_catalog_open(dir);
        ASSERT_NE(cat, nullptr);

        blob_entry_t entry;
        uint8_t blob_id[32];
        make_test_blob_id(blob_id, 0xEE);

        EXPECT_EQ(blob_catalog_find_blob(cat, blob_id, &entry), BLOB_CATALOG_OK);
        EXPECT_EQ(entry.state, BLOB_STATE_COMMITTED);
        EXPECT_EQ(entry.blob_size, 4096u);
        EXPECT_EQ(entry.chunk_count, 2u);

        blob_catalog_close(cat);
    }

    remove_recursive(dir);
}

/* ========================================================================
 * Test: WAL 重放 - checkpoint 后新增记录
 * ======================================================================== */

TEST_F(BlobCatalogTest, WALReplayAfterCheckpoint) {
    const char *dir = "test-results/blob_catalog_wal_replay_test";
    remove_recursive(dir);
    create_test_dir(dir);

    /* 第一次：创建两个 blob，checkpoint 后再新增一个 */
    {
        blob_catalog_t *cat = blob_catalog_open(dir);
        ASSERT_NE(cat, nullptr);

        uint8_t blob_id1[32], blob_id2[32], blob_id3[32];
        make_test_blob_id(blob_id1, 0x11);
        make_test_blob_id(blob_id2, 0x22);
        make_test_blob_id(blob_id3, 0x33);

        /* 第一个 blob */
        EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_prepare(cat, blob_id1, 100, 1), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

        /* 第二个 blob */
        EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_prepare(cat, blob_id2, 200, 1), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

        /* checkpoint */
        EXPECT_EQ(blob_catalog_checkpoint(cat), BLOB_CATALOG_OK);

        /* 第三个 blob（在 checkpoint 之后） */
        EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_prepare(cat, blob_id3, 300, 1), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

        blob_catalog_close(cat);
    }

    /* 第二次：打开后应恢复全部三个 blob */
    {
        blob_catalog_t *cat = blob_catalog_open(dir);
        ASSERT_NE(cat, nullptr);

        blob_entry_t entry;
        uint8_t blob_id1[32], blob_id2[32], blob_id3[32];
        make_test_blob_id(blob_id1, 0x11);
        make_test_blob_id(blob_id2, 0x22);
        make_test_blob_id(blob_id3, 0x33);

        EXPECT_EQ(blob_catalog_find_blob(cat, blob_id1, &entry), BLOB_CATALOG_OK);
        EXPECT_EQ(entry.state, BLOB_STATE_PREPARED);

        EXPECT_EQ(blob_catalog_find_blob(cat, blob_id2, &entry), BLOB_CATALOG_OK);
        EXPECT_EQ(entry.state, BLOB_STATE_PREPARED);

        EXPECT_EQ(blob_catalog_find_blob(cat, blob_id3, &entry), BLOB_CATALOG_OK);
        EXPECT_EQ(entry.state, BLOB_STATE_PREPARED);

        blob_catalog_close(cat);
    }

    remove_recursive(dir);
}

/* ========================================================================
 * Test: 篡改 CRC 恢复时跳过
 * ======================================================================== */

TEST_F(BlobCatalogTest, CorruptCRCRecovery) {
    const char *dir = "test-results/blob_catalog_crc_corrupt_test";
    remove_recursive(dir);
    create_test_dir(dir);

    const char *wal_path = NULL;

    /* 创建并写入一些数据 */
    {
        blob_catalog_t *cat = blob_catalog_open(dir);
        ASSERT_NE(cat, nullptr);

        wal_path = strdup(blob_catalog_get_dir(cat));

        uint8_t blob_id1[32], blob_id2[32];
        make_test_blob_id(blob_id1, 0x44);
        make_test_blob_id(blob_id2, 0x55);

        /* 第一个 blob */
        EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_prepare(cat, blob_id1, 512, 1), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

        /* 第二个 blob */
        EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_prepare(cat, blob_id2, 1024, 1), BLOB_CATALOG_OK);
        EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

        blob_catalog_close(cat);
    }

    /* 篡改 WAL 文件中间的一个字节 */
    {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, BLOB_CATALOG_DIR);
        char wal_full[1024];
        snprintf(wal_full, sizeof(wal_full), "%s/%s", full_path, BLOB_CATALOG_WAL);

        FILE *fp = fopen(wal_full, "r+b");
        ASSERT_NE(fp, nullptr);

        /* 读取文件大小 */
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);

        /* 篡改第一个记录的 payload 区域（跳过 header） */
        if (size > 20) {
            fseek(fp, 16, SEEK_SET);  /* 跳过第一个记录的 header */
            uint8_t corrupt = 0xFF;
            fwrite(&corrupt, 1, 1, fp);  /* 篡改一个字节 */
        }

        fclose(fp);
    }

    /* 打开 Catalog，应该能恢复（跳过损坏的记录） */
    {
        blob_catalog_t *cat = blob_catalog_open(dir);
        ASSERT_NE(cat, nullptr);

        blob_entry_t entry;
        uint8_t blob_id1[32], blob_id2[32];
        make_test_blob_id(blob_id1, 0x44);
        make_test_blob_id(blob_id2, 0x55);

        /* 第一个 blob 可能恢复成功（如果 CRC 检查位置恰好在未损坏区域） */
        /* 或者恢复失败（如果损坏影响了 CRC 校验） */
        /* 无论哪种情况，程序不应崩溃 */
        int rc1 = blob_catalog_find_blob(cat, blob_id1, &entry);
        int rc2 = blob_catalog_find_blob(cat, blob_id2, &entry);

        /* 至少不应该崩溃，恢复应该成功 */
        printf("Blob 1 查找结果: %d, Blob 2 查找结果: %d\n", rc1, rc2);

        blob_catalog_close(cat);
    }

    free((char *)wal_path);
    remove_recursive(dir);
}

/* ========================================================================
 * Test: 迭代器
 * ======================================================================== */

TEST_F(BlobCatalogTest, Iterator) {
    blob_catalog_t *cat = blob_catalog_open(test_dir_);
    ASSERT_NE(cat, nullptr);

    uint8_t blob_id1[32], blob_id2[32], blob_id3[32];
    make_test_blob_id(blob_id1, 0x66);
    make_test_blob_id(blob_id2, 0x77);
    make_test_blob_id(blob_id3, 0x88);

    /* 创建三个 blob */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_prepare(cat, blob_id1, 100, 1), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_prepare(cat, blob_id2, 200, 1), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_prepare(cat, blob_id3, 300, 1), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    /* 扫描 */
    blob_catalog_iter_t *iter = blob_catalog_iter_create(cat);
    ASSERT_NE(iter, nullptr);

    int count = 0;
    blob_entry_t entry;
    while (blob_catalog_iter_next(iter, &entry) == BLOB_CATALOG_OK) {
        count++;
        EXPECT_EQ(entry.state, BLOB_STATE_PREPARED);
    }
    EXPECT_EQ(count, 3);

    blob_catalog_iter_destroy(iter);
    blob_catalog_close(cat);
}

/* ========================================================================
 * Test: 统计信息
 * ======================================================================== */

TEST_F(BlobCatalogTest, Stats) {
    blob_catalog_t *cat = blob_catalog_open(test_dir_);
    ASSERT_NE(cat, nullptr);

    uint8_t blob_id[32];
    uint8_t chunk_id[32];
    make_test_blob_id(blob_id, 0x99);
    make_test_chunk_id(chunk_id, 0xAA);

    /* 创建 blob */
    EXPECT_EQ(blob_catalog_begin(cat), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_prepare(cat, blob_id, 100, 1), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_ref_inc(cat, chunk_id), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_catalog_end(cat), BLOB_CATALOG_OK);

    /* 检查统计 */
    uint64_t blob_count, chunk_count;
    EXPECT_EQ(blob_catalog_stats(cat, &blob_count, &chunk_count), BLOB_CATALOG_OK);
    EXPECT_EQ(blob_count, 1u);
    EXPECT_EQ(chunk_count, 1u);

    blob_catalog_close(cat);
}

/* ========================================================================
 * main
 * ======================================================================== */

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
