/**
 * @file test_reranker.cpp
 * @brief 重排序器单元测试
 */

#include <gtest/gtest.h>
#include "rag/reranker.h"
#include "rag/types.h"

using namespace rag;

// ========== 测试夹具 ==========

class RerankerTest : public ::testing::Test {
protected:
    void SetUp() override {
        reranker_ = std::make_unique<LightweightReranker>();
    }

    std::unique_ptr<LightweightReranker> reranker_;
};

// ========== 基础测试 ==========

TEST_F(RerankerTest, DefaultConfig) {
    auto config = reranker_->config();

    EXPECT_EQ(config.method, "lightweight");
    EXPECT_EQ(config.top_k, 10);
    EXPECT_GT(config.bm25_weight, 0.0f);
    EXPECT_GT(config.keyword_weight, 0.0f);
    EXPECT_GT(config.vector_weight, 0.0f);
}

TEST_F(RerankerTest, SetConfig) {
    RerankerConfig config;
    config.top_k = 20;
    config.bm25_weight = 0.3f;
    config.keyword_weight = 0.3f;
    config.vector_weight = 0.4f;

    reranker_->set_config(config);

    auto new_config = reranker_->config();
    EXPECT_EQ(new_config.top_k, 20);
    EXPECT_FLOAT_EQ(new_config.bm25_weight, 0.3f);
}

TEST_F(RerankerTest, Name) {
    EXPECT_EQ(reranker_->name(), "lightweight");
}

// ========== 分数计算测试 ==========

TEST_F(RerankerTest, KeywordMatchScore) {
    // 完全匹配
    EXPECT_GT(reranker_->calculate_keyword_score("人工智能", "人工智能"), 0.5f);

    // 部分匹配
    EXPECT_GT(reranker_->calculate_keyword_score("人工智能", "人工智"), 0.0f);

    // 不匹配
    EXPECT_FLOAT_EQ(reranker_->calculate_keyword_score("人工智能", "计算机"), 0.0f);
}

TEST_F(RerankerTest, KeywordScoreNormalization) {
    // 测试分数在 [0, 1] 范围内
    float score1 = reranker_->calculate_keyword_score("test query", "test query exact");
    float score2 = reranker_->calculate_keyword_score("test query", "completely different text");

    EXPECT_GE(score1, 0.0f);
    EXPECT_LE(score1, 1.0f);
    EXPECT_GE(score2, 0.0f);
    EXPECT_LE(score2, 1.0f);
    EXPECT_GE(score1, score2);  // 精确匹配应该分数更高
}

TEST_F(RerankerTest, EmptyQuery) {
    float score = reranker_->calculate_keyword_score("", "some text");
    EXPECT_FLOAT_EQ(score, 0.0f);
}

TEST_F(RerankerTest, EmptyDocument) {
    float score = reranker_->calculate_keyword_score("query", "");
    EXPECT_FLOAT_EQ(score, 0.0f);
}

// ========== 重排序测试 ==========

TEST_F(RerankerTest, RerankResults) {
    std::string query = "人工智能 机器学习";

    std::vector<RerankCandidate> candidates = {
        {"c1", "人工智能是研究、开发用于模拟、延伸和扩展人的智能的理论、方法、技术及应用系统的一门新的技术科学。", 0.8f},
        {"c2", "机器学习是人工智能的一个分支，专注于让计算机从数据中学习。", 0.7f},
        {"c3", "深度学习是机器学习的分支，使用神经网络模型。", 0.6f},
        {"c4", "今天天气很好，适合出去散步。", 0.5f},
    };

    auto results = reranker_->rerank(query, candidates, 3);

    EXPECT_EQ(results.size(), 3);
    EXPECT_GE(results[0].final_score, results[1].final_score);
    EXPECT_GE(results[1].final_score, results[2].final_score);
}

TEST_F(RerankerTest, RerankWithZeroCandidates) {
    std::string query = "test";
    std::vector<RerankCandidate> candidates;

    auto results = reranker_->rerank(query, candidates, 5);

    EXPECT_TRUE(results.empty());
}

TEST_F(RerankerTest, RerankWithFewerCandidatesThanTopK) {
    std::string query = "test";

    std::vector<RerankCandidate> candidates = {
        {"c1", "one candidate", 0.5f},
    };

    auto results = reranker_->rerank(query, candidates, 10);

    EXPECT_EQ(results.size(), 1);
}

TEST_F(RerankerTest, RelevanceScoreCalculation) {
    std::string query = "人工智能 深度学习";
    std::string doc1 = "人工智能和深度学习密切相关。";  // 高相关
    std::string doc2 = "今天天气不错。";  // 低相关

    float score1 = reranker_->calculate_relevance_score(query, doc1);
    float score2 = reranker_->calculate_relevance_score(query, doc2);

    EXPECT_GT(score1, score2);
}

// ========== 分数融合测试 ==========

TEST_F(RerankerTest, ScoreFusion) {
    float bm25_score = 0.8f;
    float keyword_score = 0.7f;
    float vector_score = 0.9f;

    float fused = reranker_->fuse_scores(bm25_score, keyword_score, vector_score);

    // 融合分数应该在各分数的加权范围内
    EXPECT_GE(fused, 0.0f);
    EXPECT_LE(fused, 1.0f);
}

TEST_F(RerankerTest, ScoreFusionWithWeights) {
    RerankerConfig config;
    config.bm25_weight = 0.5f;
    config.keyword_weight = 0.3f;
    config.vector_weight = 0.2f;
    reranker_->set_config(config);

    float fused = reranker_->fuse_scores(1.0f, 0.0f, 0.0f);

    // 应该接近 bm25_score * bm25_weight
    EXPECT_FLOAT_EQ(fused, 0.5f);
}

// ========== 边界条件测试 ==========

TEST_F(RerankerTest, AllZeroScores) {
    std::string query = "test";
    std::vector<RerankCandidate> candidates = {
        {"c1", "unrelated content", 0.0f},
    };

    auto results = reranker_->rerank(query, candidates, 5);

    EXPECT_EQ(results.size(), 1);
    EXPECT_GE(results[0].final_score, 0.0f);
}

TEST_F(RerankerTest, ChineseText) {
    std::string query = "机器学习算法";
    std::string doc = "机器学习算法包括监督学习、无监督学习和强化学习。";

    float score = reranker_->calculate_keyword_score(query, doc);

    EXPECT_GT(score, 0.0f);
}

TEST_F(RerankerTest, MixedLanguage) {
    std::string query = "AI 人工智能";
    std::string doc = "AI (Artificial Intelligence) is 人工智能。";

    float score = reranker_->calculate_keyword_score(query, doc);

    // 混合语言应该也能匹配
    EXPECT_GT(score, 0.0f);
}

// ========== 性能测试 ==========

TEST_F(RerankerTest, LargeCandidateSet) {
    std::string query = "人工智能 机器学习 深度学习";

    std::vector<RerankCandidate> candidates;
    for (int i = 0; i < 1000; ++i) {
        candidates.push_back({
            "c" + std::to_string(i),
            "文档内容 " + std::to_string(i) + " 包含一些相关关键词",
            1.0f - (i * 0.001f)
        });
    }

    auto start = std::chrono::steady_clock::now();
    auto results = reranker_->rerank(query, candidates, 100);
    auto end = std::chrono::steady_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_EQ(results.size(), 100);
    EXPECT_LT(duration.count(), 1000);  // 应该在 1 秒内完成
}
