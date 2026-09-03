/**
 * @file test_types.cpp
 * @brief 核心数据结构和工具函数测试
 */

#include <gtest/gtest.h>
#include "rag/types.h"

using namespace rag;

TEST(UtilsTest, GenerateUUID) {
    std::string uuid1 = generate_uuid();
    std::string uuid2 = generate_uuid();

    // UUID 应该非空
    EXPECT_FALSE(uuid1.empty());
    EXPECT_FALSE(uuid2.empty());

    // UUID 应该唯一
    EXPECT_NE(uuid1, uuid2);

    // UUID 应该有正确的格式 (8-4-4-4-12)
    EXPECT_EQ(uuid1.size(), 36);
    EXPECT_EQ(uuid2.size(), 36);
}

TEST(UtilsTest, CurrentTimestamp) {
    int64_t ts = current_timestamp();
    int64_t ts_ms = current_timestamp_ms();

    EXPECT_GT(ts, 0);
    EXPECT_GT(ts_ms, 0);

    // 毫秒时间戳应该大于秒时间戳
    EXPECT_GT(ts_ms, ts * 1000);
}

TEST(UtilsTest, TruncateString) {
    std::string long_str = "This is a very long string that needs to be truncated";
    std::string result = truncate_string(long_str, 20);

    EXPECT_EQ(result.size(), 20);
    EXPECT_EQ(result.substr(result.size() - 3), "...");

    // 短字符串不应该被截断
    std::string short_str = "Short";
    EXPECT_EQ(truncate_string(short_str, 20), short_str);
}

TEST(DocumentTest, CreateDocument) {
    Document doc;
    doc.id = generate_uuid();
    doc.content = "Test content";
    doc.metadata.file_path = "/test/doc.md";
    doc.metadata.file_name = "doc.md";
    doc.metadata.file_type = "markdown";
    doc.status = Document::Status::PENDING;

    EXPECT_FALSE(doc.id.empty());
    EXPECT_EQ(doc.content, "Test content");
    EXPECT_EQ(doc.metadata.file_name, "doc.md");
    EXPECT_EQ(doc.status, Document::Status::PENDING);
}

TEST(ChunkTest, CreateChunk) {
    Chunk chunk;
    chunk.id = generate_uuid();
    chunk.document_id = generate_uuid();
    chunk.content = "This is a test chunk";
    chunk.chunk_index = 0;
    chunk.start_char = 0;
    chunk.end_char = 20;
    chunk.score = 0.95f;

    EXPECT_FALSE(chunk.id.empty());
    EXPECT_EQ(chunk.content, "This is a test chunk");
    EXPECT_EQ(chunk.chunk_index, 0);
    EXPECT_FLOAT_EQ(chunk.score, 0.95f);
}

TEST(RetrievalResultTest, CreateResult) {
    Chunk chunk;
    chunk.id = generate_uuid();
    chunk.content = "Test content";

    RetrievalResult result;
    result.chunk = chunk;
    result.score = 0.85f;
    result.vector_score = 0.9f;
    result.bm25_score = 0.8f;
    result.source = "hybrid";

    EXPECT_EQ(result.chunk.id, chunk.id);
    EXPECT_FLOAT_EQ(result.score, 0.85f);
    EXPECT_EQ(result.source, "hybrid");
}

TEST(QueryResultTest, CreateResult) {
    QueryResult result;
    result.answer = "This is the answer";
    result.confidence = 0.9f;
    result.query_time_ms = 150;
    result.request_id = generate_uuid();

    EXPECT_EQ(result.answer, "This is the answer");
    EXPECT_FLOAT_EQ(result.confidence, 0.9f);
    EXPECT_EQ(result.request_id.size(), 36);
}

TEST(IndexStatusTest, CreateStatus) {
    IndexStatus status;
    status.index_name = "main";
    status.document_count = 100;
    status.chunk_count = 500;
    status.vector_count = 500;
    status.status = IndexStatus::Status::READY;

    EXPECT_EQ(status.index_name, "main");
    EXPECT_EQ(status.document_count, 100);
    EXPECT_EQ(status.status, IndexStatus::Status::READY);
}
