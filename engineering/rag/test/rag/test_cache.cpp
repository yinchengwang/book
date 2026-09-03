// engineering/rag/test/rag/test_cache.cpp

#include <gtest/gtest.h>
#include "rag/retrieval_cache.h"
#include "rag/embedding.h"

namespace rag {
namespace test {

class CacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        CacheConfig config;
        config.max_size = 100;
        config.ttl_seconds = 3600;
        config.semantic_cache = true;
        config.similarity_threshold = 0.95f;

        embed_service_ = std::make_shared<SimpleEmbeddingService>(768);
        cache_ = std::make_shared<SemanticCache>(embed_service_, config);
    }

    std::shared_ptr<EmbeddingService> embed_service_;
    std::shared_ptr<SemanticCache> cache_;
};

TEST_F(CacheTest, EmptyCache) {
    auto result = cache_->get("test query");
    EXPECT_FALSE(result.has_value());
}

TEST_F(CacheTest, PutAndGet) {
    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;
    output.results.push_back({"chunk1", "content1", 0.9f});

    cache_->put("test query", output);

    auto result = cache_->get("test query");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->results.size(), 1);
}

TEST_F(CacheTest, ExactMatchPreferred) {
    StageOutput output1;
    output1.status = StageOutput::Status::SUCCESS;
    output1.results.push_back({"chunk1", "content1", 0.9f});

    StageOutput output2;
    output2.status = StageOutput::Status::SUCCESS;
    output2.results.push_back({"chunk2", "content2", 0.8f});

    cache_->put("original query", output1);
    cache_->put("similar query", output2);

    auto result = cache_->get("original query");
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->results[0].chunk_id, "chunk1");
}

TEST_F(CacheTest, StatsTracking) {
    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;

    cache_->put("query1", output);
    cache_->put("query2", output);
    cache_->get("query1");  // hit
    cache_->get("query3");  // miss

    EXPECT_EQ(cache_->exact_hits(), 1);
    EXPECT_EQ(cache_->misses(), 1);
    EXPECT_GE(cache_->size(), 2);
}

// ========== LruCache Tests ==========

class LruCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        CacheConfig config;
        config.max_size = 3;
        config.ttl_seconds = 3600;
        lru_cache_ = std::make_shared<LruCache>(config);
    }

    std::shared_ptr<LruCache> lru_cache_;
};

TEST_F(LruCacheTest, BasicPutGet) {
    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;

    lru_cache_->put("key1", output);
    auto result = lru_cache_->get("key1");
    EXPECT_TRUE(result.has_value());
}

TEST_F(LruCacheTest, Eviction) {
    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;

    lru_cache_->put("key1", output);
    lru_cache_->put("key2", output);
    lru_cache_->put("key3", output);
    lru_cache_->put("key4", output);  // should evict key1

    EXPECT_FALSE(lru_cache_->get("key1").has_value());
    EXPECT_TRUE(lru_cache_->get("key2").has_value());
    EXPECT_TRUE(lru_cache_->get("key3").has_value());
    EXPECT_TRUE(lru_cache_->get("key4").has_value());
}

TEST_F(LruCacheTest, HitRate) {
    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;

    lru_cache_->put("key1", output);
    lru_cache_->get("key1");  // hit
    lru_cache_->get("key2");  // miss

    EXPECT_EQ(lru_cache_->hits(), 1);
    EXPECT_EQ(lru_cache_->misses(), 1);
}

TEST_F(LruCacheTest, Invalidate) {
    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;

    lru_cache_->put("key1", output);
    lru_cache_->invalidate("key1");

    EXPECT_FALSE(lru_cache_->get("key1").has_value());
    EXPECT_EQ(lru_cache_->size(), 0);
}

TEST_F(LruCacheTest, Clear) {
    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;

    lru_cache_->put("key1", output);
    lru_cache_->put("key2", output);
    lru_cache_->clear();

    EXPECT_EQ(lru_cache_->size(), 0);
}

// ========== MultiLevelCache Tests ==========

class MultiLevelCacheTest : public ::testing::Test {
protected:
    void SetUp() override {
        CacheConfig config;
        config.max_size = 100;
        config.ttl_seconds = 3600;
        config.semantic_cache = true;
        config.similarity_threshold = 0.95f;

        embed_service_ = std::make_shared<SimpleEmbeddingService>(768);
        multi_cache_ = std::make_shared<MultiLevelCache>(embed_service_, config);
    }

    std::shared_ptr<EmbeddingService> embed_service_;
    std::shared_ptr<MultiLevelCache> multi_cache_;
};

TEST_F(MultiLevelCacheTest, BasicPutGet) {
    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;
    output.results.push_back({"chunk1", "content1", 0.9f});

    multi_cache_->put("test query", output);
    auto result = multi_cache_->get("test query");

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(result->results.size(), 1);
}

TEST_F(MultiLevelCacheTest, Stats) {
    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;

    multi_cache_->put("query1", output);
    multi_cache_->get("query1");
    multi_cache_->get("query2");

    auto stats = multi_cache_->get_stats();
    EXPECT_EQ(stats.l1_hits, 1);
    EXPECT_EQ(stats.misses, 1);
}

}  // namespace test
}  // namespace rag

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}