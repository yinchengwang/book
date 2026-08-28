/**
 * @file blob_engine_test.cpp
 * @brief Blob 存储引擎测试（Task 1-3）
 *
 * 测试 SHA-256、Chunk 固定格式和 Manifest 编解码。
 */
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

extern "C" {
#include "db/sha256.h"
#include "db/blob_manifest.h"
}

namespace fs = std::filesystem;

namespace {

/**
 * @brief SHA-256 摘要转十六进制字符串
 */
std::string to_hex(const uint8_t digest[SHA256_DIGEST_SIZE]) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (size_t i = 0; i < SHA256_DIGEST_SIZE; ++i) {
        output << std::setw(2) << static_cast<unsigned int>(digest[i]);
    }
    return output.str();
}

}  // 匿名命名空间

/* ========================================================================
 * SHA-256 标准测试向量（Task 1）
 * ======================================================================== */

TEST(Sha256, StandardVectors) {
    uint8_t digest[SHA256_DIGEST_SIZE];

    /* 空字符串 */
    sha256_compute("", 0, digest);
    EXPECT_EQ(to_hex(digest), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    /* "abc" */
    sha256_compute("abc", 3, digest);
    EXPECT_EQ(to_hex(digest), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    /* 100万个 'a' */
    std::string million_a(1000000, 'a');
    sha256_compute(million_a.data(), million_a.size(), digest);
    EXPECT_EQ(to_hex(digest), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(Sha256, StreamingUpdatesMatchOneShot) {
    const std::string input = "流式 SHA-256 测试数据 abc";
    uint8_t expected[SHA256_DIGEST_SIZE];
    uint8_t actual[SHA256_DIGEST_SIZE];
    sha256_compute(input.data(), input.size(), expected);

    /* 逐字节更新 */
    sha256_ctx_t context;
    sha256_init(&context);
    for (size_t offset = 0; offset < input.size(); ++offset) {
        sha256_update(&context, input.data() + offset, 1);
    }
    sha256_final(&context, actual);

    EXPECT_EQ(to_hex(actual), to_hex(expected));
}

/* ========================================================================
 * Chunk 固定格式与原子发布测试（Task 2）
 * ======================================================================== */

class BlobChunkTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建临时测试目录 */
        test_dir_ = fs::temp_directory_path() / "blob_chunk_test";
        fs::create_directories(test_dir_);
        chunks_dir_ = test_dir_ / "chunks";
        fs::create_directories(chunks_dir_);
    }

    void TearDown() override {
        /* 清理测试目录 */
        fs::remove_all(test_dir_);
    }

    fs::path test_dir_;
    fs::path chunks_dir_;
};

/* 测试 Chunk 写入后读取，验证 header、payload 和校验完整 */
TEST_F(BlobChunkTest, WriteThenRead) {
    const size_t data_size = 4 * 1024 * 1024 + 17; /* 4MB + 17 字节 */
    std::vector<uint8_t> data(data_size);
    for (size_t i = 0; i < data_size; i++) {
        data[i] = (uint8_t)(i & 0xFF);
    }

    uint8_t chunk_id[BLOB_CHUNK_ID_SIZE];
    int rc = blob_chunk_write_tmp(chunks_dir_.string().c_str(),
                                  data.data(), data_size,
                                  "upload_test_001",
                                  chunk_id);
    ASSERT_EQ(rc, BLOB_OK) << "blob_chunk_write_tmp 失败";

    /* 读回并校验 */
    std::vector<uint8_t> read_buf(data_size);
    size_t read_len = 0;
    rc = blob_chunk_read_checked(chunks_dir_.string().c_str(),
                                 chunk_id,
                                 read_buf.data(), read_buf.size(),
                                 &read_len);
    ASSERT_EQ(rc, BLOB_OK) << "blob_chunk_read_checked 失败";
    EXPECT_EQ(read_len, data_size);
    EXPECT_EQ(memcmp(read_buf.data(), data.data(), data_size), 0);
}

/* 测试 header 篡改后读取失败 */
TEST_F(BlobChunkTest, CorruptedHeaderFails) {
    const size_t data_size = 1024;
    std::vector<uint8_t> data(data_size, 0xAB);

    uint8_t chunk_id[BLOB_CHUNK_ID_SIZE];
    int rc = blob_chunk_write_tmp(chunks_dir_.string().c_str(),
                                  data.data(), data_size,
                                  "upload_corrupt_001",
                                  chunk_id);
    ASSERT_EQ(rc, BLOB_OK);

    /* 篡改 header: 修改 magic */
    char final_path[1024];
    blob_chunk_final_path(chunks_dir_.string().c_str(), chunk_id,
                          final_path, sizeof(final_path));
    FILE *fp = fopen(final_path, "r+b");
    ASSERT_NE(fp, nullptr);
    uint8_t bad_magic[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    fwrite(bad_magic, 1, 4, fp);
    fclose(fp);

    /* 读取应失败 */
    std::vector<uint8_t> read_buf(data_size);
    size_t read_len = 0;
    rc = blob_chunk_read_checked(chunks_dir_.string().c_str(),
                                 chunk_id,
                                 read_buf.data(), read_buf.size(),
                                 &read_len);
    EXPECT_EQ(rc, BLOB_ERR_CORRUPT);
    EXPECT_EQ(read_len, 0u);
}

/* 测试 payload 篡改后读取失败 */
TEST_F(BlobChunkTest, CorruptedPayloadFails) {
    const size_t data_size = 256;
    std::vector<uint8_t> data(data_size, 0xCD);

    uint8_t chunk_id[BLOB_CHUNK_ID_SIZE];
    int rc = blob_chunk_write_tmp(chunks_dir_.string().c_str(),
                                  data.data(), data_size,
                                  "upload_corrupt_002",
                                  chunk_id);
    ASSERT_EQ(rc, BLOB_OK);

    /* 篡改 payload: 修改第一个字节 */
    char final_path[1024];
    blob_chunk_final_path(chunks_dir_.string().c_str(), chunk_id,
                          final_path, sizeof(final_path));
    FILE *fp = fopen(final_path, "r+b");
    ASSERT_NE(fp, nullptr);
    fseek(fp, BLOB_CHUNK_HEADER_SIZE, SEEK_SET);
    uint8_t bad_byte = 0xFF;
    fwrite(&bad_byte, 1, 1, fp);
    fclose(fp);

    /* 读取应失败 */
    std::vector<uint8_t> read_buf(data_size);
    size_t read_len = 0;
    rc = blob_chunk_read_checked(chunks_dir_.string().c_str(),
                                 chunk_id,
                                 read_buf.data(), read_buf.size(),
                                 &read_len);
    EXPECT_EQ(rc, BLOB_ERR_CORRUPT);
    EXPECT_EQ(read_len, 0u);
}

/* 测试同一数据重复写入时的去重（正式文件已存在且一致） */
TEST_F(BlobChunkTest, DuplicateWriteReuse) {
    const size_t data_size = 512;
    std::vector<uint8_t> data(data_size, 0x42);

    uint8_t chunk_id1[BLOB_CHUNK_ID_SIZE];
    int rc = blob_chunk_write_tmp(chunks_dir_.string().c_str(),
                                  data.data(), data_size,
                                  "upload_dup_001",
                                  chunk_id1);
    ASSERT_EQ(rc, BLOB_OK);

    /* 第二次写入相同数据 */
    uint8_t chunk_id2[BLOB_CHUNK_ID_SIZE];
    rc = blob_chunk_write_tmp(chunks_dir_.string().c_str(),
                              data.data(), data_size,
                              "upload_dup_002",
                              chunk_id2);
    ASSERT_EQ(rc, BLOB_OK);

    /* chunk_id 应完全相同 */
    EXPECT_EQ(memcmp(chunk_id1, chunk_id2, BLOB_CHUNK_ID_SIZE), 0);

    /* 读取验证 */
    std::vector<uint8_t> read_buf(data_size);
    size_t read_len = 0;
    rc = blob_chunk_read_checked(chunks_dir_.string().c_str(),
                                 chunk_id1,
                                 read_buf.data(), read_buf.size(),
                                 &read_len);
    EXPECT_EQ(rc, BLOB_OK);
    EXPECT_EQ(read_len, data_size);
}

/* 测试 exists 检查 */
TEST_F(BlobChunkTest, ExistsCheck) {
    const size_t data_size = 64;
    std::vector<uint8_t> data(data_size, 0x11);

    uint8_t chunk_id[BLOB_CHUNK_ID_SIZE];

    /* 不存在的 chunk */
    sha256_compute("nonexistent", 10, chunk_id);
    EXPECT_EQ(blob_chunk_exists_checked(chunks_dir_.string().c_str(),
                                        chunk_id),
              BLOB_ERR_NOTFOUND);

    /* 写入后存在 */
    int rc = blob_chunk_write_tmp(chunks_dir_.string().c_str(),
                                  data.data(), data_size,
                                  "upload_exists_001",
                                  chunk_id);
    ASSERT_EQ(rc, BLOB_OK);
    EXPECT_EQ(blob_chunk_exists_checked(chunks_dir_.string().c_str(),
                                        chunk_id),
              BLOB_OK);
}

/* 测试空参数 */
TEST_F(BlobChunkTest, NullArguments) {
    uint8_t chunk_id[BLOB_CHUNK_ID_SIZE];
    EXPECT_EQ(blob_chunk_write_tmp(NULL, "x", 1, "id", chunk_id), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_chunk_write_tmp("/tmp", NULL, 1, "id", chunk_id), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_chunk_write_tmp("/tmp", "x", 0, "id", chunk_id), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_chunk_write_tmp("/tmp", "x", 1, NULL, chunk_id), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_chunk_write_tmp("/tmp", "x", 1, "id", NULL), BLOB_ERR_INVAL);

    size_t len;
    EXPECT_EQ(blob_chunk_read_checked(NULL, chunk_id, chunk_id, 32, &len), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_chunk_read_checked("/tmp", NULL, chunk_id, 32, &len), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_chunk_read_checked("/tmp", chunk_id, NULL, 32, &len), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_chunk_read_checked("/tmp", chunk_id, chunk_id, 32, NULL), BLOB_ERR_INVAL);
}

/* 测试 header 校验和计算 */
TEST_F(BlobChunkTest, HeaderChecksum) {
    blob_chunk_header_t hdr1, hdr2;
    memset(&hdr1, 0, sizeof(hdr1));
    memset(&hdr2, 0, sizeof(hdr2));

    hdr1.magic = BLOB_CHUNK_MAGIC;
    hdr1.version = BLOB_CHUNK_VERSION;
    hdr1.payload_size = 100;
    hdr1.header_checksum = blob_chunk_header_checksum(&hdr1);

    hdr2.magic = BLOB_CHUNK_MAGIC;
    hdr2.version = BLOB_CHUNK_VERSION;
    hdr2.payload_size = 100;
    hdr2.header_checksum = blob_chunk_header_checksum(&hdr2);

    /* 相同内容应产生相同校验和 */
    EXPECT_EQ(hdr1.header_checksum, hdr2.header_checksum);

    /* 修改 payload_size 应产生不同校验和 */
    hdr2.payload_size = 200;
    hdr2.header_checksum = blob_chunk_header_checksum(&hdr2);
    EXPECT_NE(hdr1.header_checksum, hdr2.header_checksum);
}

/* ========================================================================
 * Manifest 编解码与校验测试（Task 3）
 * ======================================================================== */

class BlobManifestTest : public ::testing::Test {
protected:
    void SetUp() override {
        /* 创建临时测试目录 */
        test_dir_ = fs::temp_directory_path() / "blob_manifest_test";
        fs::create_directories(test_dir_);
        manifests_dir_ = test_dir_ / "manifests";
        fs::create_directories(manifests_dir_);
        chunks_dir_ = test_dir_ / "chunks";
        fs::create_directories(chunks_dir_);
    }

    void TearDown() override {
        /* 清理测试目录 */
        fs::remove_all(test_dir_);
    }

    fs::path test_dir_;
    fs::path manifests_dir_;
    fs::path chunks_dir_;
};

/* 测试 Manifest 创建与释放 */
TEST_F(BlobManifestTest, CreateAndFree) {
    /* 创建包含 3 个 Chunk 的 Manifest */
    blob_manifest_t *manifest = blob_manifest_create(3, "application/octet-stream", NULL, 0);
    ASSERT_NE(manifest, nullptr);

    EXPECT_EQ(manifest->chunk_count, 3u);
    EXPECT_STREQ(manifest->content_type, "application/octet-stream");
    EXPECT_EQ(manifest->header.content_type_len, 24u);
    EXPECT_EQ(manifest->header.metadata_len, 0u);
    EXPECT_NE(manifest->chunks, nullptr);

    /* 测试带 metadata 的 Manifest */
    const char *meta = "key=value";
    blob_manifest_t *manifest2 = blob_manifest_create(2, "text/plain", meta, strlen(meta));
    ASSERT_NE(manifest2, nullptr);
    EXPECT_EQ(manifest2->header.metadata_len, (uint16_t)strlen(meta));
    EXPECT_NE(manifest2->metadata, nullptr);
    EXPECT_EQ(memcmp(manifest2->metadata, meta, strlen(meta)), 0);

    /* 测试空 metadata */
    blob_manifest_t *manifest3 = blob_manifest_create(1, "image/png", NULL, 0);
    ASSERT_NE(manifest3, nullptr);
    EXPECT_EQ(manifest3->header.metadata_len, 0u);
    EXPECT_EQ(manifest3->metadata, nullptr);

    /* 释放 */
    blob_manifest_free(manifest);
    blob_manifest_free(manifest2);
    blob_manifest_free(manifest3);
}

/* 测试 Manifest 写入后读回并逐字段比较 */
TEST_F(BlobManifestTest, WriteAndReadBack) {
    /* 1. 创建测试数据 */
    const size_t chunk1_size = 1024;
    const size_t chunk2_size = 2048;
    const size_t chunk3_size = 512;
    std::vector<uint8_t> chunk1_data(chunk1_size);
    std::vector<uint8_t> chunk2_data(chunk2_size);
    std::vector<uint8_t> chunk3_data(chunk3_size);

    for (size_t i = 0; i < chunk1_size; i++) chunk1_data[i] = (uint8_t)(i & 0xFF);
    for (size_t i = 0; i < chunk2_size; i++) chunk2_data[i] = (uint8_t)((i + 1) & 0xFF);
    for (size_t i = 0; i < chunk3_size; i++) chunk3_data[i] = (uint8_t)((i + 2) & 0xFF);

    /* 2. 写入 3 个 Chunk */
    uint8_t chunk1_id[BLOB_CHUNK_ID_SIZE];
    uint8_t chunk2_id[BLOB_CHUNK_ID_SIZE];
    uint8_t chunk3_id[BLOB_CHUNK_ID_SIZE];

    ASSERT_EQ(blob_chunk_write_tmp(chunks_dir_.string().c_str(),
                                  chunk1_data.data(), chunk1_size,
                                  "upload_mft_001", chunk1_id), BLOB_OK);
    ASSERT_EQ(blob_chunk_write_tmp(chunks_dir_.string().c_str(),
                                  chunk2_data.data(), chunk2_size,
                                  "upload_mft_002", chunk2_id), BLOB_OK);
    ASSERT_EQ(blob_chunk_write_tmp(chunks_dir_.string().c_str(),
                                  chunk3_data.data(), chunk3_size,
                                  "upload_mft_003", chunk3_id), BLOB_OK);

    /* 3. 创建 Manifest */
    const char *content_type = "application/octet-stream";
    blob_manifest_t *manifest = blob_manifest_create(3, content_type, NULL, 0);
    ASSERT_NE(manifest, nullptr);

    /* 填充 header */
    manifest->header.blob_size = chunk1_size + chunk2_size + chunk3_size;
    manifest->header.chunk_size = BLOB_CHUNK_LOGICAL_SIZE;

    /* 计算 blob 整体 SHA-256 */
    sha256_ctx_t ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, chunk1_data.data(), chunk1_size);
    sha256_update(&ctx, chunk2_data.data(), chunk2_size);
    sha256_update(&ctx, chunk3_data.data(), chunk3_size);
    sha256_final(&ctx, manifest->header.blob_sha256);

    /* 填充 Chunk 条目 */
    manifest->chunks[0].logical_offset = 0;
    manifest->chunks[0].chunk_size = (uint32_t)chunk1_size;
    memcpy(manifest->chunks[0].chunk_sha256, chunk1_id, BLOB_CHUNK_ID_SIZE);
    manifest->chunks[0].chunk_checksum = blob_manifest_chunk_checksum(&manifest->chunks[0]);

    manifest->chunks[1].logical_offset = chunk1_size;
    manifest->chunks[1].chunk_size = (uint32_t)chunk2_size;
    memcpy(manifest->chunks[1].chunk_sha256, chunk2_id, BLOB_CHUNK_ID_SIZE);
    manifest->chunks[1].chunk_checksum = blob_manifest_chunk_checksum(&manifest->chunks[1]);

    manifest->chunks[2].logical_offset = chunk1_size + chunk2_size;
    manifest->chunks[2].chunk_size = (uint32_t)chunk3_size;
    memcpy(manifest->chunks[2].chunk_sha256, chunk3_id, BLOB_CHUNK_ID_SIZE);
    manifest->chunks[2].chunk_checksum = blob_manifest_chunk_checksum(&manifest->chunks[2]);

    /* 4. 写入 Manifest */
    ASSERT_EQ(blob_manifest_write_atomic(manifests_dir_.string().c_str(),
                                        manifest, "upload_mft_final"),
              BLOB_OK);

    /* 5. 读回 Manifest */
    blob_manifest_t *loaded = nullptr;
    ASSERT_EQ(blob_manifest_load_checked(manifests_dir_.string().c_str(),
                                         manifest->header.blob_sha256,
                                         &loaded),
              BLOB_OK);
    ASSERT_NE(loaded, nullptr);

    /* 6. 逐字段比较 */
    EXPECT_EQ(loaded->header.magic, BLOB_MANIFEST_MAGIC);
    EXPECT_EQ(loaded->header.version, BLOB_MANIFEST_VERSION);
    EXPECT_EQ(loaded->header.blob_size, manifest->header.blob_size);
    EXPECT_EQ(loaded->header.chunk_size, manifest->header.chunk_size);
    EXPECT_EQ(loaded->header.chunk_count, manifest->header.chunk_count);
    EXPECT_STREQ(loaded->content_type, content_type);

    /* 比较 Chunk 条目 */
    EXPECT_EQ(memcmp(loaded->chunks[0].chunk_sha256, chunk1_id, BLOB_CHUNK_ID_SIZE), 0);
    EXPECT_EQ(loaded->chunks[0].logical_offset, 0u);
    EXPECT_EQ(loaded->chunks[0].chunk_size, (uint32_t)chunk1_size);

    EXPECT_EQ(memcmp(loaded->chunks[1].chunk_sha256, chunk2_id, BLOB_CHUNK_ID_SIZE), 0);
    EXPECT_EQ(loaded->chunks[1].logical_offset, chunk1_size);
    EXPECT_EQ(loaded->chunks[1].chunk_size, (uint32_t)chunk2_size);

    EXPECT_EQ(memcmp(loaded->chunks[2].chunk_sha256, chunk3_id, BLOB_CHUNK_ID_SIZE), 0);
    EXPECT_EQ(loaded->chunks[2].logical_offset, chunk1_size + chunk2_size);
    EXPECT_EQ(loaded->chunks[2].chunk_size, (uint32_t)chunk3_size);

    /* 清理 */
    blob_manifest_free(manifest);
    blob_manifest_free(loaded);
}

/* 测试 magic 篡改后加载失败 */
TEST_F(BlobManifestTest, CorruptedMagicFails) {
    /* 创建简单的 Manifest */
    blob_manifest_t *manifest = blob_manifest_create(1, "test/plain", NULL, 0);
    ASSERT_NE(manifest, nullptr);
    manifest->header.blob_size = 100;
    sha256_compute("test", 4, manifest->header.blob_sha256);

    /* 写入 */
    ASSERT_EQ(blob_manifest_write_atomic(manifests_dir_.string().c_str(),
                                        manifest, "upload_cmagic"),
              BLOB_OK);

    /* 篡改 magic */
    char final_path[1024];
    blob_manifest_final_path(manifests_dir_.string().c_str(),
                            manifest->header.blob_sha256,
                            final_path, sizeof(final_path));
    FILE *fp = fopen(final_path, "r+b");
    ASSERT_NE(fp, nullptr);
    uint8_t bad_magic[4] = {0xDE, 0xAD, 0xBE, 0xEF};
    fwrite(bad_magic, 1, 4, fp);
    fclose(fp);

    /* 加载应失败 */
    blob_manifest_t *loaded = nullptr;
    EXPECT_EQ(blob_manifest_load_checked(manifests_dir_.string().c_str(),
                                        manifest->header.blob_sha256,
                                        &loaded),
              BLOB_ERR_CORRUPT);

    blob_manifest_free(manifest);
}

/* 测试 version 篡改后加载失败 */
TEST_F(BlobManifestTest, CorruptedVersionFails) {
    blob_manifest_t *manifest = blob_manifest_create(1, "test/plain", NULL, 0);
    ASSERT_NE(manifest, nullptr);
    manifest->header.blob_size = 100;
    sha256_compute("test", 4, manifest->header.blob_sha256);

    ASSERT_EQ(blob_manifest_write_atomic(manifests_dir_.string().c_str(),
                                        manifest, "upload_cver"),
              BLOB_OK);

    /* 篡改 version（偏移 4 字节） */
    char final_path[1024];
    blob_manifest_final_path(manifests_dir_.string().c_str(),
                            manifest->header.blob_sha256,
                            final_path, sizeof(final_path));
    FILE *fp = fopen(final_path, "r+b");
    ASSERT_NE(fp, nullptr);
    fseek(fp, 4, SEEK_SET);
    uint8_t bad_ver[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    fwrite(bad_ver, 1, 4, fp);
    fclose(fp);

    blob_manifest_t *loaded = nullptr;
    EXPECT_EQ(blob_manifest_load_checked(manifests_dir_.string().c_str(),
                                        manifest->header.blob_sha256,
                                        &loaded),
              BLOB_ERR_CORRUPT);

    blob_manifest_free(manifest);
}

/* 测试 chunk_count 篡改后加载失败 */
TEST_F(BlobManifestTest, CorruptedChunkCountFails) {
    blob_manifest_t *manifest = blob_manifest_create(2, "test/plain", NULL, 0);
    ASSERT_NE(manifest, nullptr);
    manifest->header.blob_size = 200;
    sha256_compute("test", 4, manifest->header.blob_sha256);

    ASSERT_EQ(blob_manifest_write_atomic(manifests_dir_.string().c_str(),
                                        manifest, "upload_ccnt"),
              BLOB_OK);

    /* 篡改 chunk_count（偏移 24 字节） */
    char final_path[1024];
    blob_manifest_final_path(manifests_dir_.string().c_str(),
                            manifest->header.blob_sha256,
                            final_path, sizeof(final_path));
    FILE *fp = fopen(final_path, "r+b");
    ASSERT_NE(fp, nullptr);
    fseek(fp, 24, SEEK_SET);
    uint8_t bad_cnt[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    fwrite(bad_cnt, 1, 4, fp);
    fclose(fp);

    blob_manifest_t *loaded = nullptr;
    EXPECT_EQ(blob_manifest_load_checked(manifests_dir_.string().c_str(),
                                        manifest->header.blob_sha256,
                                        &loaded),
              BLOB_ERR_CORRUPT);

    blob_manifest_free(manifest);
}

/* 测试 blob_id 不匹配时加载失败 */
TEST_F(BlobManifestTest, WrongBlobIdFails) {
    blob_manifest_t *manifest = blob_manifest_create(1, "test/plain", NULL, 0);
    ASSERT_NE(manifest, nullptr);
    manifest->header.blob_size = 100;
    sha256_compute("correct", 7, manifest->header.blob_sha256);

    ASSERT_EQ(blob_manifest_write_atomic(manifests_dir_.string().c_str(),
                                        manifest, "upload_cbid"),
              BLOB_OK);

    /* 使用错误的 blob_id 加载 */
    uint8_t wrong_id[BLOB_BLOB_ID_SIZE];
    sha256_compute("wrong_id", 8, wrong_id);

    blob_manifest_t *loaded = nullptr;
    EXPECT_EQ(blob_manifest_load_checked(manifests_dir_.string().c_str(),
                                        wrong_id,
                                        &loaded),
              BLOB_ERR_CORRUPT);

    blob_manifest_free(manifest);
}

/* 测试篡改 blob_sha256 后加载失败 */
TEST_F(BlobManifestTest, CorruptedBlobSha256Fails) {
    blob_manifest_t *manifest = blob_manifest_create(1, "test/plain", NULL, 0);
    ASSERT_NE(manifest, nullptr);
    manifest->header.blob_size = 100;
    sha256_compute("original", 8, manifest->header.blob_sha256);

    ASSERT_EQ(blob_manifest_write_atomic(manifests_dir_.string().c_str(),
                                        manifest, "upload_csha"),
              BLOB_OK);

    /* 篡改 blob_sha256（偏移 32 字节） */
    char final_path[1024];
    blob_manifest_final_path(manifests_dir_.string().c_str(),
                            manifest->header.blob_sha256,
                            final_path, sizeof(final_path));
    FILE *fp = fopen(final_path, "r+b");
    ASSERT_NE(fp, nullptr);
    fseek(fp, 32, SEEK_SET);
    uint8_t bad_sha[32];
    memset(bad_sha, 0xFF, 32);
    fwrite(bad_sha, 1, 32, fp);
    fclose(fp);

    blob_manifest_t *loaded = nullptr;
    EXPECT_EQ(blob_manifest_load_checked(manifests_dir_.string().c_str(),
                                        manifest->header.blob_sha256,
                                        &loaded),
              BLOB_ERR_CORRUPT);

    blob_manifest_free(manifest);
}

/* 测试空 metadata 的 Manifest */
TEST_F(BlobManifestTest, EmptyMetadata) {
    /* 创建没有 metadata 的 Manifest */
    blob_manifest_t *manifest = blob_manifest_create(1, "text/plain", NULL, 0);
    ASSERT_NE(manifest, nullptr);

    EXPECT_EQ(manifest->header.metadata_len, 0u);
    EXPECT_EQ(manifest->metadata, nullptr);
    EXPECT_NE(manifest->content_type, nullptr);
    EXPECT_STREQ(manifest->content_type, "text/plain");

    /* 写入并读回 */
    manifest->header.blob_size = 50;
    sha256_compute("test", 4, manifest->header.blob_sha256);

    ASSERT_EQ(blob_manifest_write_atomic(manifests_dir_.string().c_str(),
                                        manifest, "upload_emeta"),
              BLOB_OK);

    blob_manifest_t *loaded = nullptr;
    ASSERT_EQ(blob_manifest_load_checked(manifests_dir_.string().c_str(),
                                        manifest->header.blob_sha256,
                                        &loaded),
              BLOB_OK);

    EXPECT_EQ(loaded->header.metadata_len, 0u);
    EXPECT_EQ(loaded->metadata, nullptr);

    blob_manifest_free(manifest);
    blob_manifest_free(loaded);
}

/* 测试带 metadata 的 Manifest */
TEST_F(BlobManifestTest, WithMetadata) {
    const char *metadata = "{\"key\":\"value\",\"num\":42}";
    const size_t meta_len = strlen(metadata);

    blob_manifest_t *manifest = blob_manifest_create(1, "application/json",
                                                       metadata, meta_len);
    ASSERT_NE(manifest, nullptr);

    EXPECT_EQ(manifest->header.metadata_len, (uint16_t)meta_len);
    EXPECT_NE(manifest->metadata, nullptr);
    EXPECT_EQ(memcmp(manifest->metadata, metadata, meta_len), 0);

    /* 写入并读回 */
    manifest->header.blob_size = 100;
    sha256_compute("test", 4, manifest->header.blob_sha256);

    ASSERT_EQ(blob_manifest_write_atomic(manifests_dir_.string().c_str(),
                                        manifest, "upload_wmeta"),
              BLOB_OK);

    blob_manifest_t *loaded = nullptr;
    ASSERT_EQ(blob_manifest_load_checked(manifests_dir_.string().c_str(),
                                        manifest->header.blob_sha256,
                                        &loaded),
              BLOB_OK);

    EXPECT_EQ(loaded->header.metadata_len, (uint16_t)meta_len);
    EXPECT_NE(loaded->metadata, nullptr);
    EXPECT_EQ(memcmp(loaded->metadata, metadata, meta_len), 0);

    blob_manifest_free(manifest);
    blob_manifest_free(loaded);
}

/* 测试不存在的 Manifest 文件 */
TEST_F(BlobManifestTest, NotFound) {
    uint8_t fake_id[BLOB_BLOB_ID_SIZE];
    sha256_compute("notexist", 8, fake_id);

    blob_manifest_t *loaded = nullptr;
    EXPECT_EQ(blob_manifest_load_checked(manifests_dir_.string().c_str(),
                                        fake_id,
                                        &loaded),
              BLOB_ERR_NOTFOUND);
}

/* 测试 NULL 参数 */
TEST_F(BlobManifestTest, NullArguments) {
    blob_manifest_t *manifest = blob_manifest_create(1, "test/plain", NULL, 0);
    ASSERT_NE(manifest, nullptr);

    /* write_atomic NULL 参数 */
    EXPECT_EQ(blob_manifest_write_atomic(NULL, manifest, "id"), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_manifest_write_atomic("/tmp", NULL, "id"), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_manifest_write_atomic("/tmp", manifest, NULL), BLOB_ERR_INVAL);

    /* load_checked NULL 参数 */
    blob_manifest_t *loaded = nullptr;
    uint8_t fake_id[BLOB_BLOB_ID_SIZE];
    sha256_compute("test", 4, fake_id);

    EXPECT_EQ(blob_manifest_load_checked(NULL, fake_id, &loaded), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_manifest_load_checked("/tmp", NULL, &loaded), BLOB_ERR_INVAL);
    EXPECT_EQ(blob_manifest_load_checked("/tmp", fake_id, NULL), BLOB_ERR_INVAL);

    blob_manifest_free(manifest);
}

/* 测试 checksum 计算 */
TEST_F(BlobManifestTest, ManifestChecksum) {
    blob_manifest_header_t hdr1, hdr2;
    memset(&hdr1, 0, sizeof(hdr1));
    memset(&hdr2, 0, sizeof(hdr2));

    hdr1.magic = BLOB_MANIFEST_MAGIC;
    hdr1.version = BLOB_MANIFEST_VERSION;
    hdr1.blob_size = 1000;
    hdr1.chunk_size = BLOB_CHUNK_LOGICAL_SIZE;
    hdr1.chunk_count = 3;
    hdr1.content_type_len = 10;
    hdr1.metadata_len = 5;
    sha256_compute("test1", 5, hdr1.blob_sha256);

    uint32_t cksum1 = blob_manifest_header_checksum(&hdr1);

    /* 相同内容应产生相同校验和 */
    hdr2 = hdr1;
    uint32_t cksum2 = blob_manifest_header_checksum(&hdr2);
    EXPECT_EQ(cksum1, cksum2);

    /* 修改 blob_size 应产生不同校验和 */
    hdr2.blob_size = 2000;
    uint32_t cksum3 = blob_manifest_header_checksum(&hdr2);
    EXPECT_NE(cksum1, cksum3);
}

/* 测试 Chunk 条目 checksum */
TEST_F(BlobManifestTest, ChunkEntryChecksum) {
    blob_manifest_chunk_t chunk1, chunk2;
    memset(&chunk1, 0, sizeof(chunk1));
    memset(&chunk2, 0, sizeof(chunk2));

    sha256_compute("chunk1", 6, chunk1.chunk_sha256);
    chunk1.logical_offset = 0;
    chunk1.chunk_size = 1024;

    uint32_t cksum1 = blob_manifest_chunk_checksum(&chunk1);

    /* 相同内容应产生相同校验和 */
    chunk2 = chunk1;
    uint32_t cksum2 = blob_manifest_chunk_checksum(&chunk2);
    EXPECT_EQ(cksum1, cksum2);

    /* 修改 logical_offset 应产生不同校验和 */
    chunk2.logical_offset = 1024;
    uint32_t cksum3 = blob_manifest_chunk_checksum(&chunk2);
    EXPECT_NE(cksum1, cksum3);
}
