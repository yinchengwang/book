/**
 * @file test_pipeline.cpp
 * @brief Modular RAG Pipeline 单元测试
 */

#include <gtest/gtest.h>
#include "rag/modular/pipeline/pipeline_base.h"
#include "rag/modular/pipeline/naive_pipeline.h"
#include "rag/modular/pipeline/advanced_pipeline.h"
#include "rag/modular/pipeline/hybrid_pipeline.h"
#include "rag/modular/config.h"
#include "rag/modular/types.h"
#include "rag/retriever.h"
#include "rag/types.h"
#include <memory>

using namespace rag::modular;

/**
 * @brief 测试夹具：NaivePipeline 基本功能测试
 */
class NaivePipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建默认配置
        config_.default_pipeline = PipelineType::NAIVE;
        config_.llm.model_name = "test-model";
        config_.retrieval.top_k = 5;
    }

    ModularConfig config_;
};

/**
 * @brief 测试夹具：AdvancedPipeline 基本功能测试
 */
class AdvancedPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.default_pipeline = PipelineType::ADVANCED;
        config_.llm.model_name = "test-model";
        config_.retrieval.top_k = 5;
    }

    ModularConfig config_;
};

/**
 * @brief 测试夹具：HybridPipeline 基本功能测试
 */
class HybridPipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        config_.default_pipeline = PipelineType::HYBRID;
        config_.llm.model_name = "test-model";
        config_.retrieval.top_k = 5;
    }

    ModularConfig config_;
};

// ========== NaivePipeline 测试 ==========

/**
 * @brief 测试 NaivePipeline 初始化
 */
TEST_F(NaivePipelineTest, Init) {
    NaivePipeline pipeline;

    // 初始状态未初始化
    EXPECT_FALSE(pipeline.is_ready());

    // 使用空配置初始化应该失败（因为缺少必需组件）
    // 但如果提供了 mock 组件则应该成功
    bool init_result = pipeline.init(config_);

    // NaivePipeline 不需要 LLM 也可以初始化
    EXPECT_TRUE(init_result);
    EXPECT_TRUE(pipeline.is_ready());
}

/**
 * @brief 测试 NaivePipeline 类型和名称
 */
TEST_F(NaivePipelineTest, TypeAndName) {
    NaivePipeline pipeline;

    EXPECT_EQ(pipeline.type(), PipelineType::NAIVE);
    EXPECT_EQ(pipeline.name(), "NaivePipeline");
}

/**
 * @brief 测试 NaivePipeline 查询（空检索器）
 */
TEST_F(NaivePipelineTest, QueryWithEmptyRetriever) {
    NaivePipeline pipeline;
    pipeline.init(config_);

    ModularQuery query;
    query.text = "测试查询";
    query.top_k = 5;

    // 无检索器时应该返回空结果
    auto result = pipeline.query(query);
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.context.empty());
}

/**
 * @brief 测试 NaivePipeline 未初始化状态
 */
TEST_F(NaivePipelineTest, QueryWithoutInit) {
    NaivePipeline pipeline;

    ModularQuery query;
    query.text = "测试查询";

    // 未初始化时调用 query 可能出现问题
    // Pipeline 应该优雅处理
    auto result = pipeline.query(query);
    // 期望返回失败或空结果
    EXPECT_FALSE(result.success || result.context.empty());
}

// ========== AdvancedPipeline 测试 ==========

/**
 * @brief 测试 AdvancedPipeline 初始化
 */
TEST_F(AdvancedPipelineTest, Init) {
    AdvancedPipeline pipeline;

    EXPECT_FALSE(pipeline.is_ready());

    bool init_result = pipeline.init(config_);
    EXPECT_TRUE(init_result);
    EXPECT_TRUE(pipeline.is_ready());
}

/**
 * @brief 测试 AdvancedPipeline 类型和名称
 */
TEST_F(AdvancedPipelineTest, TypeAndName) {
    AdvancedPipeline pipeline;

    EXPECT_EQ(pipeline.type(), PipelineType::ADVANCED);
    EXPECT_EQ(pipeline.name(), "AdvancedPipeline");
}

/**
 * @brief 测试 AdvancedPipeline 查询流程
 */
TEST_F(AdvancedPipelineTest, QueryFlow) {
    AdvancedPipeline pipeline;
    pipeline.init(config_);

    ModularQuery query;
    query.text = "什么是人工智能";
    query.top_k = 3;

    auto result = pipeline.query(query);
    EXPECT_TRUE(result.success);
}

/**
 * @brief 测试 AdvancedPipeline 未初始化状态
 */
TEST_F(AdvancedPipelineTest, QueryWithoutInit) {
    AdvancedPipeline pipeline;

    ModularQuery query;
    query.text = "测试查询";

    auto result = pipeline.query(query);
    EXPECT_FALSE(result.success);
}

// ========== HybridPipeline 测试 ==========

/**
 * @brief 测试 HybridPipeline 初始化
 */
TEST_F(HybridPipelineTest, Init) {
    HybridPipeline pipeline;

    EXPECT_FALSE(pipeline.is_ready());

    bool init_result = pipeline.init(config_);
    EXPECT_TRUE(init_result);
    EXPECT_TRUE(pipeline.is_ready());
}

/**
 * @brief 测试 HybridPipeline 类型和名称
 */
TEST_F(HybridPipelineTest, TypeAndName) {
    HybridPipeline pipeline;

    EXPECT_EQ(pipeline.type(), PipelineType::HYBRID);
    EXPECT_EQ(pipeline.name(), "HybridPipeline");
}

/**
 * @brief 测试 HybridPipeline 查询流程
 */
TEST_F(HybridPipelineTest, QueryFlow) {
    HybridPipeline pipeline;
    pipeline.init(config_);

    ModularQuery query;
    query.text = "机器学习的应用场景";
    query.top_k = 5;

    auto result = pipeline.query(query);
    EXPECT_TRUE(result.success);
}

/**
 * @brief 测试 HybridPipeline 未初始化状态
 */
TEST_F(HybridPipelineTest, QueryWithoutInit) {
    HybridPipeline pipeline;

    ModularQuery query;
    query.text = "测试查询";

    auto result = pipeline.query(query);
    EXPECT_FALSE(result.success);
}

// ========== Pipeline 基类测试 ==========

/**
 * @brief 测试 PipelineType 枚举值
 */
TEST(PipelineTypeTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(PipelineType::NAIVE), 0);
    EXPECT_EQ(static_cast<int>(PipelineType::ADVANCED), 1);
    EXPECT_EQ(static_cast<int>(PipelineType::HYBRID), 2);
    EXPECT_EQ(static_cast<int>(PipelineType::HYDE), 3);
    EXPECT_EQ(static_cast<int>(PipelineType::GRAPH), 4);
    EXPECT_EQ(static_cast<int>(PipelineType::CORRECTIVE), 5);
    EXPECT_EQ(static_cast<int>(PipelineType::REACT), 6);
    EXPECT_EQ(static_cast<int>(PipelineType::ITERATIVE), 7);
    EXPECT_EQ(static_cast<int>(PipelineType::RECURSIVE), 8);
}

/**
 * @brief 测试 PipelineType 字符串转换
 */
TEST(PipelineTypeTest, StringConversion) {
    EXPECT_EQ(pipeline_type_to_string(PipelineType::NAIVE), "naive");
    EXPECT_EQ(pipeline_type_to_string(PipelineType::ADVANCED), "advanced");
    EXPECT_EQ(pipeline_type_to_string(PipelineType::HYBRID), "hybrid");
}

/**
 * @brief 测试字符串转 PipelineType
 */
TEST(PipelineTypeTest, FromString) {
    EXPECT_EQ(string_to_pipeline_type("naive"), PipelineType::NAIVE);
    EXPECT_EQ(string_to_pipeline_type("advanced"), PipelineType::ADVANCED);
    EXPECT_EQ(string_to_pipeline_type("hybrid"), PipelineType::HYBRID);
}

/**
 * @brief 测试列出所有 Pipeline 类型
 */
TEST(PipelineTypeTest, ListTypes) {
    auto types = list_pipeline_types();
    EXPECT_GT(types.size(), 0);
    EXPECT_FALSE(std::find(types.begin(), types.end(), "naive") == types.end());
    EXPECT_FALSE(std::find(types.begin(), types.end(), "advanced") == types.end());
    EXPECT_FALSE(std::find(types.begin(), types.end(), "hybrid") == types.end());
}

// ========== ModularQuery 和 ModularQueryResult 测试 ==========

/**
 * @brief 测试 ModularQuery 默认值
 */
TEST(ModularQueryTest, DefaultValues) {
    ModularQuery query;

    EXPECT_TRUE(query.text.empty());
    EXPECT_EQ(query.pipeline_type, PipelineType::ADVANCED);
    EXPECT_EQ(query.top_k, 5);
    EXPECT_TRUE(query.options.empty());
}

/**
 * @brief 测试 ModularQuery 设置
 */
TEST(ModularQueryTest, Setters) {
    ModularQuery query;
    query.text = "测试查询";
    query.pipeline_type = PipelineType::HYBRID;
    query.top_k = 10;
    query.options["key"] = "value";

    EXPECT_EQ(query.text, "测试查询");
    EXPECT_EQ(query.pipeline_type, PipelineType::HYBRID);
    EXPECT_EQ(query.top_k, 10);
    EXPECT_EQ(query.options["key"], "value");
}

/**
 * @brief 测试 ModularQueryResult 默认值
 */
TEST(ModularQueryResultTest, DefaultValues) {
    ModularQueryResult result;

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.answer.empty());
    EXPECT_TRUE(result.context.empty());
    EXPECT_EQ(result.retrieval_time_ms, 0);
    EXPECT_EQ(result.generation_time_ms, 0);
    EXPECT_EQ(result.total_time_ms, 0);
    EXPECT_EQ(result.total_tokens, 0);
    EXPECT_TRUE(result.error_message.empty());
}

/**
 * @brief 测试 ModularQueryResult 设置
 */
TEST(ModularQueryResultTest, Setters) {
    ModularQueryResult result;
    result.success = true;
    result.answer = "这是回答";
    result.retrieval_time_ms = 100;
    result.generation_time_ms = 200;
    result.total_time_ms = 300;
    result.total_tokens = 150;

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.answer, "这是回答");
    EXPECT_EQ(result.retrieval_time_ms, 100);
    EXPECT_EQ(result.generation_time_ms, 200);
    EXPECT_EQ(result.total_time_ms, 300);
    EXPECT_EQ(result.total_tokens, 150);
}

// ========== ModularConfig 测试 ==========

/**
 * @brief 测试 ModularConfig 默认值
 */
TEST(ModularConfigTest, DefaultValues) {
    ModularConfig config;

    EXPECT_EQ(config.default_pipeline, PipelineType::ADVANCED);
    EXPECT_TRUE(config.model_path.empty());
    EXPECT_TRUE(config.embedding_model_path.empty());
    EXPECT_TRUE(config.data_dir.empty());
    EXPECT_TRUE(config.index_dir.empty());
}

/**
 * @brief 测试 ModularConfig 设置
 */
TEST(ModularConfigTest, Setters) {
    ModularConfig config;
    config.default_pipeline = PipelineType::HYBRID;
    config.model_path = "/models/llm";
    config.embedding_model_path = "/models/embedding";
    config.data_dir = "/data";
    config.index_dir = "/index";
    config.agent.max_iterations = 20;

    EXPECT_EQ(config.default_pipeline, PipelineType::HYBRID);
    EXPECT_EQ(config.model_path, "/models/llm");
    EXPECT_EQ(config.embedding_model_path, "/models/embedding");
    EXPECT_EQ(config.data_dir, "/data");
    EXPECT_EQ(config.index_dir, "/index");
    EXPECT_EQ(config.agent.max_iterations, 20);
}
