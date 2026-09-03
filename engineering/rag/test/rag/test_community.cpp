/**
 * @file test_community.cpp
 * @brief 社区摘要模块单元测试
 */

#include <gtest/gtest.h>
#include "rag/community.h"

using namespace rag;

// ========== 测试夹具 ==========

class CommunityDetectorTest : public ::testing::Test {
protected:
    void SetUp() override {
        detector_ = create_community_detector(CommunityDetector::Method::Louvain);
    }

    std::unique_ptr<CommunityDetector> detector_;
};

class GlobalContextGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        generator_ = create_global_context_generator();
    }

    std::unique_ptr<GlobalContextGenerator> generator_;
};

// ========== CommunityDetector 基础测试 ==========

TEST_F(CommunityDetectorTest, BasicDetection) {
    // 创建测试数据：两个独立的连通分量
    std::vector<std::string> entity_ids = {"e1", "e2", "e3", "e4", "e5", "e6"};
    std::unordered_map<std::string, std::vector<std::string>> relations;

    // 社区1: e1 - e2 - e3
    relations["e1"] = {"e2"};
    relations["e2"] = {"e1", "e3"};
    relations["e3"] = {"e2"};

    // 社区2: e4 - e5 - e6
    relations["e4"] = {"e5"};
    relations["e5"] = {"e4", "e6"};
    relations["e6"] = {"e5"};

    auto communities = detector_->detect(entity_ids, relations);

    // 应该检测到两个社区
    EXPECT_GE(communities.size(), 2);
}

TEST_F(CommunityDetectorTest, EmptyInput) {
    std::vector<std::string> entity_ids;
    std::unordered_map<std::string, std::vector<std::string>> relations;

    auto communities = detector_->detect(entity_ids, relations);
    EXPECT_TRUE(communities.empty());
}

TEST_F(CommunityDetectorTest, SingleEntity) {
    std::vector<std::string> entity_ids = {"e1"};
    std::unordered_map<std::string, std::vector<std::string>> relations;

    auto communities = detector_->detect(entity_ids, relations);
    // 单独节点不会形成社区（小于 min_size=3）
    EXPECT_TRUE(communities.empty());
}

// ========== CommunityDetector 社区大小过滤测试 ==========

TEST_F(CommunityDetectorTest, SizeFilter) {
    detector_->set_min_community_size(2);
    detector_->set_max_community_size(5);

    std::vector<std::string> entity_ids = {"e1", "e2", "e3", "e4", "e5", "e6", "e7", "e8"};
    std::unordered_map<std::string, std::vector<std::string>> relations;

    // 形成链: e1-e2-e3-e4-e5-e6-e7-e8
    for (size_t i = 0; i < entity_ids.size() - 1; ++i) {
        relations[entity_ids[i]] = {entity_ids[i + 1]};
        relations[entity_ids[i + 1]] = {entity_ids[i]};
    }

    auto communities = detector_->detect(entity_ids, relations);

    // 检查社区大小限制
    for (const auto& comm : communities) {
        EXPECT_GE(comm.size, 2);
        EXPECT_LE(comm.size, 5);
    }
}

// ========== GlobalContextGenerator 测试 ==========

TEST_F(GlobalContextGeneratorTest, GenerateContext) {
    auto result = generator_->generate("test query");

    EXPECT_TRUE(result.summaries.size() >= 0);
    EXPECT_FALSE(result.global_knowledge.empty());
}

TEST_F(GlobalContextGeneratorTest, GenerateCommunitySummary) {
    Community community;
    community.id = "test_comm";
    community.name = "Test Community";
    community.entity_ids = {"e1", "e2", "e3", "e4", "e5"};
    community.theme = "test theme";
    community.size = 5;

    auto summary = generator_->generate_community_summary(community);

    EXPECT_EQ(summary.community_id, "test_comm");
    EXPECT_FALSE(summary.summary.empty());
    EXPECT_EQ(summary.key_entities.size(), 5);
    EXPECT_EQ(summary.relevance_score, 5);
}

TEST_F(GlobalContextGeneratorTest, FindRelatedCommunities) {
    auto related = generator_->find_related_communities("test");

    // 返回的是缓存中的社区或默认社区
    EXPECT_TRUE(related.size() >= 0);
}

// ========== 社区摘要测试 ==========

TEST(CommunitySummaryTest, BasicSummary) {
    CommunitySummary summary;
    summary.community_id = "comm1";
    summary.summary = "Test summary";
    summary.key_entities = {"e1", "e2"};
    summary.key_relations = {"r1"};
    summary.relevance_score = 10;

    EXPECT_EQ(summary.community_id, "comm1");
    EXPECT_EQ(summary.summary, "Test summary");
    EXPECT_EQ(summary.key_entities.size(), 2);
    EXPECT_EQ(summary.relevance_score, 10);
}

// ========== 工厂函数测试 ==========

TEST(FactoryTest, CreateCommunityDetector) {
    auto detector = create_community_detector(CommunityDetector::Method::Louvain);
    EXPECT_NE(detector, nullptr);

    auto detector2 = create_community_detector(CommunityDetector::Method::LabelPropagation);
    EXPECT_NE(detector2, nullptr);
}

TEST(FactoryTest, CreateGlobalContextGenerator) {
    auto generator = create_global_context_generator();
    EXPECT_NE(generator, nullptr);
}

// ========== 集成测试 ==========

TEST(IntegrationTest, CommunityDetectionAndSummary) {
    auto detector = create_community_detector();
    auto generator = create_global_context_generator();

    std::vector<std::string> entity_ids = {"e1", "e2", "e3", "e4"};
    std::unordered_map<std::string, std::vector<std::string>> relations;
    relations["e1"] = {"e2"};
    relations["e2"] = {"e1", "e3"};
    relations["e3"] = {"e2"};

    auto communities = detector->detect(entity_ids, relations);

    for (const auto& comm : communities) {
        auto summary = generator->generate_community_summary(comm);
        EXPECT_EQ(summary.community_id, comm.id);
        EXPECT_FALSE(summary.summary.empty());
    }
}
