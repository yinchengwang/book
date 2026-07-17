/**
 * @file test_retriever.cpp
 * @brief 检索器测试
 */

#include <gtest/gtest.h>
#include "rag/retriever.h"
#include <memory>

using namespace rag;

class RetrieverTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建 Embedding 服务
        embed_service_ = std::make_unique<SimpleEmbeddingService>(128);

        // 创建 HNSW 配置
        hnsw_config_ = HNSWConfig{};
        hnsw_config_.dim = 128;
        hnsw_config_.max_elements = 1000;
        hnsw_config_.m = 16;
        hnsw_config_.ef_construction = 100;
        hnsw_config_.ef_search = 50;

        // 创建 HNSW 索引
        hnsw_index_ = create_vector_index(hnsw_config_);
        hnsw_index_->init(hnsw_config_);

        // 创建 BM25 配置
        bm25_config_ = BM25Config{};
        bm25_config_.k1 = 1.5f;
        bm25_config_.b = 0.75f;

        // 创建 BM25 索引
        bm25_index_ = create_bm25_index(bm25_config_);
        bm25_index_->init(bm25_config_);

        // 检索配置
        retrieval_config_ = RetrievalConfig{};
        retrieval_config_.top_k = 5;
        retrieval_config_.hybrid = true;
        retrieval_config_.hybrid_weight = 0.7f;
        retrieval_config_.rrf_k = 60;

        // 创建检索器
        retriever_ = create_retriever(
            retrieval_config_,
            hnsw_index_,
            bm25_index_,
            embed_service_
        );
    }

    std::unique_ptr<SimpleEmbeddingService> embed_service_;
    std::unique_ptr<VectorIndex> hnsw_index_;
    std::unique_ptr<BM25Index> bm25_index_;
    HNSWConfig hnsw_config_;
    BM25Config bm25_config_;
    RetrievalConfig retrieval_config_;
    std::shared_ptr<Retriever> retriever_;
};

TEST_F(RetrieverTest, HNSWRetriever) {
    // 添加测试向量
    std::vector<float> vec1(128, 1.0f);
    std::vector<float> vec2(128, 0.5f);
    std::vector<float> vec3(128, 0.0f);

    hnsw_index_->add("doc1", vec1);
    hnsw_index_->add("doc2", vec2);
    hnsw_index_->add("doc3", vec3);

    // 搜索相似向量
    auto results = hnsw_index_->search(vec1, 2);

    EXPECT_EQ(results.size(), 2);
    EXPECT_EQ(results[0].first, "doc1");  // 自身应该排第一
}

TEST_F(RetrieverTest, BM25Retriever) {
    bm25_index_->add("doc1", "RAG system provides retrieval augmented generation");
    bm25_index_->add("doc2", "Vector index enables similarity search");
    bm25_index_->add("doc3", "BM25 is a classic text retrieval algorithm");

    auto results = bm25_index_->search("RAG retrieval", 2);

    EXPECT_GT(results.size(), 0);
    EXPECT_EQ(results[0].first, "doc1");  // 应该匹配 doc1
}

TEST_F(RetrieverTest, HybridRetriever) {
    // 添加向量
    auto vec1 = embed_service_->encode("RAG system");
    auto vec2 = embed_service_->encode("Vector search");
    auto vec3 = embed_service_->encode("BM25 algorithm");

    hnsw_index_->add("doc1", vec1);
    hnsw_index_->add("doc2", vec2);
    hnsw_index_->add("doc3", vec3);

    // 添加文本
    bm25_index_->add("doc1", "RAG system provides retrieval augmented generation");
    bm25_index_->add("doc2", "Vector index enables similarity search");
    bm25_index_->add("doc3", "BM25 is a classic text retrieval algorithm");

    // 混合检索
    auto results = retriever_->retrieve("RAG retrieval", 3);

    EXPECT_GT(results.size(), 0);
    EXPECT_EQ(results[0].source, "hybrid");
}

TEST_F(RetrieverTest, RRFusion) {
    retrieval_config_.hybrid_weight = 0.5f;
    retrieval_config_.rrf_k = 60;

    auto hybrid = std::make_shared<HybridRetriever>(
        std::make_shared<HNSWRetriever>(hnsw_index_, embed_service_, retrieval_config_),
        std::make_shared<BM25Retriever>(bm25_index_, retrieval_config_),
        retrieval_config_
    );

    // 添加数据
    auto vec1 = embed_service_->encode("machine learning");
    hnsw_index_->add("doc1", vec1);
    bm25_index_->add("doc1", "Machine learning is a subset of artificial intelligence");

    auto results = hybrid->retrieve("machine learning", 1);

    EXPECT_GT(results.size(), 0);
}

TEST_F(RetrieverTest, EmptyIndex) {
    auto results = retriever_->retrieve("query", 5);
    EXPECT_EQ(results.size(), 0);
}

TEST_F(RetrieverTest, TopKLimit) {
    // 添加多个文档
    for (int i = 0; i < 20; ++i) {
        auto vec = embed_service_->encode("doc" + std::to_string(i));
        hnsw_index_->add("doc" + std::to_string(i), vec);
        bm25_index_->add("doc" + std::to_string(i), "document " + std::to_string(i));
    }

    auto results = retriever_->retrieve("doc", 5);
    EXPECT_LE(results.size(), 5);
}

TEST_F(RetrieverTest, BM25IndexOperations) {
    bm25_index_->add("doc1", "test content");
    EXPECT_EQ(bm25_index_->size(), 1);

    bm25_index_->remove("doc1");
    EXPECT_EQ(bm25_index_->size(), 0);

    bm25_index_->add("doc2", "another content");
    bm25_index_->clear();
    EXPECT_EQ(bm25_index_->size(), 0);
}
