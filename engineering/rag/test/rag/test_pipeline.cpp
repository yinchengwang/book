/**
 * @file test_pipeline.cpp
 * @brief Pipeline 单元测试
 */

#include "rag/pipeline.h"
#include "rag/query_classifier.h"
#include "rag/query_decomposer.h"
#include "rag/self_rag.h"
#include <gtest/gtest.h>
#include <memory>
#include <string>

using namespace rag;

// ========== Test Stages ==========

/**
 * @brief 测试用 Mock Stage
 */
class MockStage : public RetrievalStage {
public:
    std::string name() const override { return "mock_stage"; }
    StageType type() const override { return StageType::CUSTOM; }

    StageOutput process(const StageInput& input) override {
        call_count_++;
        last_query_ = input.query;

        StageOutput output;
        output.status = StageOutput::Status::SUCCESS;

        // 添加一个 mock 结果
        Chunk chunk;
        chunk.content = "Mock response for: " + input.query;
        output.results.push_back({chunk, 0.8f});

        return output;
    }

    int call_count() const { return call_count_; }
    const std::string& last_query() const { return last_query_; }

private:
    int call_count_ = 0;
    std::string last_query_;
};

/**
 * @brief 测试用 Mock Retriever
 */
class MockRetriever : public Retriever {
public:
    std::string name() const override { return "mock_retriever"; }

    std::vector<RetrievalResult> retrieve(
        const std::string& query, int top_k) override {

        retrieve_count_++;

        std::vector<RetrievalResult> results;
        for (int i = 0; i < std::min(top_k, 3); ++i) {
            Chunk chunk;
            chunk.content = "Result " + std::to_string(i) + " for: " + query;
            chunk.metadata["rank"] = std::to_string(i);
            results.push_back({chunk, 1.0f - i * 0.1f});
        }

        return results;
    }

    int retrieve_count() const { return retrieve_count_; }

private:
    int retrieve_count_ = 0;
};

// ========== Tests ==========

class PipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        pipeline_ = std::make_unique<RetrievalPipeline>();
    }

    void TearDown() override {
        pipeline_.reset();
    }

    std::unique_ptr<RetrievalPipeline> pipeline_;
};

TEST_F(PipelineTest, AddStage) {
    auto stage = std::make_shared<MockStage>();

    pipeline_->add_stage(stage);

    auto stages = pipeline_->get_stages();
    EXPECT_EQ(stages.size(), 1u);
    EXPECT_EQ(stages[0]->name(), "mock_stage");
}

TEST_F(PipelineTest, RemoveStage) {
    auto stage1 = std::make_shared<MockStage>();
    auto stage2 = std::make_shared<MockStage>();

    pipeline_->add_stage(stage1);
    pipeline_->add_stage(stage2);

    pipeline_->remove_stage("mock_stage");

    auto stages = pipeline_->get_stages();
    EXPECT_EQ(stages.size(), 1u);
}

TEST_F(PipelineTest, ClearStages) {
    pipeline_->add_stage(std::make_shared<MockStage>());
    pipeline_->add_stage(std::make_shared<MockStage>());

    pipeline_->clear_stages();

    EXPECT_TRUE(pipeline_->get_stages().empty());
}

TEST_F(PipelineTest, ExecuteWithStages) {
    auto stage = std::make_shared<MockStage>();
    pipeline_->add_stage(stage);

    auto result = pipeline_->execute("test query");

    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.results.empty());
    EXPECT_EQ(stage->call_count(), 1);
}

TEST_F(PipelineTest, StageTypeRouting) {
    class FactualOnlyStage : public MockStage {
    public:
        bool supports(QueryType type) const override {
            return type == QueryType::FACTUAL;
        }
    };

    auto stage = std::make_shared<FactualOnlyStage>();
    pipeline_->add_stage(stage);

    // FACTUAL 应该执行
    pipeline_->execute("什么是 RAG");
    EXPECT_EQ(stage->call_count(), 1);

    // CHAT 不应该执行
    pipeline_->clear_stages();
    pipeline_->add_stage(stage);
    pipeline_->execute("你好");
    // 注意: 需要 classifier 才能正确路由
}

TEST_F(PipelineTest, QueryClassifier) {
    auto classifier = create_query_classifier(QueryClassifierConfig{});
    pipeline_->set_query_classifier(classifier);

    EXPECT_EQ(pipeline_->classify_query("什么是 RAG"), QueryType::FACTUAL);
    EXPECT_EQ(pipeline_->classify_query("比较 A 和 B"), QueryType::COMPARATIVE);
    EXPECT_EQ(pipeline_->classify_query("总结一下"), QueryType::SUMMARY);
}

// ========== Query Classifier Tests ==========

class QueryClassifierTest : public ::testing::Test {};

TEST_F(QueryClassifierTest, RuleBasedClassifier) {
    auto classifier = std::make_shared<RuleBasedQueryClassifier>();

    EXPECT_EQ(classifier->classify("什么是 RAG"), QueryType::FACTUAL);
    EXPECT_EQ(classifier->classify("how many users"), QueryType::FACTUAL);
    EXPECT_EQ(classifier->classify("A 和 B 的区别"), QueryType::COMPARATIVE);
    EXPECT_EQ(classifier->classify("为什么系统崩溃"), QueryType::ANALYTICAL);
    EXPECT_EQ(classifier->classify("总结这篇文章"), QueryType::SUMMARY);
}

TEST_F(QueryClassifierTest, KeywordClassifier) {
    auto classifier = std::make_shared<KeywordQueryClassifier>();

    auto [type, confidence] = classifier->classify_with_confidence("什么是 RAG");
    EXPECT_EQ(type, QueryType::FACTUAL);
    EXPECT_GT(confidence, 0.0f);
}

TEST_F(QueryClassifierTest, ExtractKeywords) {
    auto classifier = std::make_shared<KeywordQueryClassifier>();

    auto keywords = classifier->extract_keywords("什么是 RAG 检索增强生成");
    EXPECT_FALSE(keywords.empty());
}

// ========== Query Decomposer Tests ==========

class QueryDecomposerTest : public ::testing::Test {};

TEST_F(QueryDecomposerTest, RuleBasedDecomposer) {
    auto decomposer = std::make_shared<RuleBasedQueryDecomposer>();

    auto result = decomposer->decompose("张三和李四在哪个公司工作");
    EXPECT_TRUE(result.success);
    EXPECT_GT(result.sub_queries.size(), 0u);
}

TEST_F(QueryDecomposerTest, SimpleQuery) {
    auto decomposer = std::make_shared<RuleBasedQueryDecomposer>();

    auto result = decomposer->decompose("什么是 RAG");
    EXPECT_TRUE(result.success);
    // 简单查询应该返回单个子查询
    EXPECT_EQ(result.sub_queries.size(), 1u);
}

// ========== Self-RAG Tests ==========

class SelfRAGTest : public ::testing::Test {};

TEST_F(SelfRAGTest, ReflectionTokenParsing) {
    std::string llm_output = R"(
[IS_RELEVANT] The content is relevant to the query.
[IS_SUPPORTED] The content supports the answer.
[USEFUL] This is useful information.
)";

    auto result = parse_reflection_tokens(llm_output);
    EXPECT_TRUE(result.is_relevant);
    EXPECT_TRUE(result.is_supported);
    EXPECT_TRUE(result.is_useful);
}

TEST_F(SelfRAGTest, SelfRAGStage) {
    auto stage = create_self_rag_stage(SelfRAGConfig{});

    EXPECT_EQ(stage->name(), "self_rag");
    EXPECT_EQ(stage->type(), StageType::SELF_RAG);
    EXPECT_TRUE(stage->is_ready());
}

// ========== Metadata Filter Tests ==========

class MetadataFilterTest : public ::testing::Test {};

TEST_F(MetadataFilterTest, EmptyFilter) {
    MetadataFilter filter;
    EXPECT_TRUE(filter.empty());
}

TEST_F(MetadataFilterTest, SqlGeneration) {
    MetadataFilter filter;
    filter.file_types = {"pdf", "md"};

    std::string sql = filter.to_sql_where();
    EXPECT_FALSE(sql.empty());
    EXPECT_NE(sql.find("pdf"), std::string::npos);
    EXPECT_NE(sql.find("md"), std::string::npos);
}

TEST_F(MetadataFilterTest, FilterBuilder) {
    auto filter = FilterBuilder()
        .with_file_types({"pdf"})
        .with_date_range("2024-01-01", "2024-12-31")
        .with_authors({"张三"})
        .build();

    EXPECT_FALSE(filter->empty());
    EXPECT_EQ(filter->file_types.size(), 1u);
    EXPECT_EQ(filter->authors.size(), 1u);
}

// ========== Tests ==========

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
