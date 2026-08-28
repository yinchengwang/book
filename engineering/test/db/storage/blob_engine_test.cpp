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

std::string to_hex(const uint8_t digest[SHA256_DIGEST_SIZE]) {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (size_t i = 0; i < SHA256_DIGEST_SIZE; ++i) {
        output << std::setw(2) << static_cast<unsigned int>(digest[i]);
    }
    return output.str();
}

}  // 匿名命名空间

TEST(Sha256, StandardVectors) {
    uint8_t digest[SHA256_DIGEST_SIZE];

    sha256_compute("", 0, digest);
    EXPECT_EQ(to_hex(digest), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");

    sha256_compute("abc", 3, digest);
    EXPECT_EQ(to_hex(digest), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    std::string million_a(1000000, 'a');
    sha256_compute(million_a.data(), million_a.size(), digest);
    EXPECT_EQ(to_hex(digest), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST(Sha256, StreamingUpdatesMatchOneShot) {
    const std::string input = "流式 SHA-256 测试数据 abc";
    uint8_t expected[SHA256_DIGEST_SIZE];
    uint8_t actual[SHA256_DIGEST_SIZE];
    sha256_compute(input.data(), input.size(), expected);

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
