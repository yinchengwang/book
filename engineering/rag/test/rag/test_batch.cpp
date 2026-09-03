// engineering/rag/test/rag/test_batch.cpp

#include <gtest/gtest.h>
#include "rag/batch_processor.h"
#include "rag/config.h"
#include "rag/chunker.h"
#include "rag/types.h"

namespace rag {
namespace test {

class BatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        BatchConfig config;
        config.gpu_batch_size = 32;
        config.cpu_prefetch_threads = 4;
        config.use_fp16 = true;
        processor_ = std::make_shared<BatchProcessor>(config);

        // 设置 chunker
        ChunkingConfig chunk_config;
        chunk_config.chunk_size = 100;
        chunk_config.chunk_overlap = 20;
        chunker_ = std::make_unique<RecursiveChunker>(chunk_config);
        processor_->set_chunker(chunker_);
    }

    std::shared_ptr<BatchProcessor> processor_;
    std::unique_ptr<Chunker> chunker_;
};

TEST_F(BatchTest, ProcessEmptyBatch) {
    std::vector<Document> docs;
    auto result = processor_->process_batch(docs);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.documents_processed, 0);
    EXPECT_EQ(result.chunks_created, 0);
}

TEST_F(BatchTest, ProcessSingleDocument) {
    Document doc;
    doc.id = "test-doc-1";
    doc.content = "This is a test document with some content for batch processing.";
    doc.metadata["source"] = "test";

    std::vector<Document> docs = {doc};
    auto result = processor_->process_batch(docs);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.documents_processed, 1);
    EXPECT_GE(result.chunks_created, 1);
}

TEST_F(BatchTest, AsyncProcessing) {
    std::vector<Document> docs;
    for (int i = 0; i < 5; i++) {
        Document doc;
        doc.id = "test-doc-" + std::to_string(i);
        doc.content = "Test document " + std::to_string(i);
        docs.push_back(doc);
    }

    auto future = processor_->process_batch_async(docs);
    auto result = future.get();

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.documents_processed, 5);
}

TEST_F(BatchTest, ConfigUpdate) {
    BatchConfig new_config;
    new_config.gpu_batch_size = 64;
    new_config.use_fp16 = false;

    processor_->update_config(new_config);

    EXPECT_EQ(processor_->config().gpu_batch_size, 64);
    EXPECT_EQ(processor_->config().use_fp16, false);
}

TEST_F(BatchTest, StatsTracking) {
    std::vector<Document> docs;
    for (int i = 0; i < 3; i++) {
        Document doc;
        doc.id = "test-doc-" + std::to_string(i);
        doc.content = "Test content " + std::to_string(i);
        docs.push_back(doc);
    }

    processor_->process_batch(docs);
    auto stats = processor_->get_stats();

    EXPECT_EQ(stats.total_batches, 1);
    EXPECT_EQ(stats.total_documents, 3);
}

TEST_F(BatchTest, ProcessMultipleDocuments) {
    std::vector<Document> docs;
    for (int i = 0; i < 10; i++) {
        Document doc;
        doc.id = "test-doc-" + std::to_string(i);
        doc.content = "Test document number " + std::to_string(i) + " with some content";
        docs.push_back(doc);
    }

    auto result = processor_->process_batch(docs);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.documents_processed, 10);
    EXPECT_GE(result.chunks_created, 1);
    EXPECT_FALSE(result.chunk_ids.empty());
}

TEST_F(BatchTest, ChunkIdCollection) {
    std::vector<Document> docs;
    Document doc;
    doc.id = "test-doc-1";
    doc.content = "This is a test document with some content for batch processing.";
    docs.push_back(doc);

    auto result = processor_->process_batch(docs);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.chunk_ids.size(), static_cast<size_t>(result.chunks_created));
}

}  // namespace test
}  // namespace rag

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}