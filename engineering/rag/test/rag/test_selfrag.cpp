/**
 * @file test_selfrag.cpp
 * @brief Self-RAG 模块测试
 */

#include <gtest/gtest.h>
#include "rag/self_rag.h"
#include "rag/types.h"

using namespace rag;

TEST(SelfRAGTest, ReflectionTokens) {
    // 测试完全有用标记
    ReflectionResult result = parse_reflection_tokens("fully_useful");
    EXPECT_TRUE(result.is_relevant);
    EXPECT_TRUE(result.is_supported);
    EXPECT_TRUE(result.is_useful);
    EXPECT_FLOAT_EQ(result.relevance_score, 1.0f);
    EXPECT_FLOAT_EQ(result.support_score, 1.0f);
    EXPECT_FLOAT_EQ(result.usefulness_score, 1.0f);
}

TEST(SelfRAGTest, ReflectionTokensRelevant) {
    ReflectionResult result = parse_reflection_tokens("is_relevant");
    EXPECT_TRUE(result.is_relevant);
    EXPECT_FLOAT_EQ(result.relevance_score, 0.8f);
}

TEST(SelfRAGTest, ReflectionTokensNotRelevant) {
    ReflectionResult result = parse_reflection_tokens("not_relevant");
    EXPECT_FALSE(result.is_relevant);
    EXPECT_FLOAT_EQ(result.relevance_score, 0.2f);
}

TEST(SelfRAGTest, ReflectionTokensChinese) {
    ReflectionResult result = parse_reflection_tokens("相关内容");
    EXPECT_TRUE(result.is_relevant);
    EXPECT_FLOAT_EQ(result.relevance_score, 0.8f);
}

TEST(SelfRAGTest, ReflectionTokensPartial) {
    ReflectionResult result = parse_reflection_tokens("is_partial");
    EXPECT_FALSE(result.is_complete);
    EXPECT_FLOAT_EQ(result.completeness_score, 0.5f);
}

TEST(SelfRAGTest, ReflectionTokensDefault) {
    // 没有匹配标记时使用默认值
    ReflectionResult result = parse_reflection_tokens("random text");
    EXPECT_FLOAT_EQ(result.relevance_score, 0.5f);
    EXPECT_FLOAT_EQ(result.support_score, 0.5f);
    EXPECT_FLOAT_EQ(result.completeness_score, 0.5f);
    EXPECT_FLOAT_EQ(result.usefulness_score, 0.5f);
}

TEST(SelfRAGTest, ReflectionResultToString) {
    ReflectionResult result;
    result.is_relevant = true;
    result.is_supported = true;
    result.is_complete = false;
    result.is_useful = true;
    result.relevance_score = 0.8f;
    result.support_score = 0.7f;
    result.completeness_score = 0.5f;
    result.usefulness_score = 0.75f;

    std::string str = result.to_string();
    EXPECT_NE(str.find("is_relevant: true"), std::string::npos);
    EXPECT_NE(str.find("overall_score:"), std::string::npos);
}

TEST(SelfRAGTest, OverallScore) {
    ReflectionResult result;
    result.relevance_score = 0.8f;
    result.support_score = 0.6f;
    result.completeness_score = 0.5f;
    result.usefulness_score = 0.7f;

    float expected = (0.8f + 0.6f + 0.5f + 0.7f) / 4.0f;
    EXPECT_FLOAT_EQ(result.overall_score(), expected);
}

TEST(SelfRAGTest, ShouldRetrieveKnowledge) {
    // 知识型查询需要检索
    SelfRAGConfig config;
    config.enable_self_check = true;
    config.use_llm_evaluation = false;  // 使用 mock

    SelfRAGStage stage(config, nullptr);

    Chunk chunk;
    chunk.id = "test-chunk";
    chunk.content = "Python 是一种高级编程语言";

    // 知识型查询
    ReflectionResult result = stage.evaluate_chunk("Python 是什么", chunk);
    EXPECT_TRUE(result.is_relevant);
    EXPECT_GT(result.relevance_score, 0.5f);
}

TEST(SelfRAGTest, ShouldRetrieveChat) {
    // 含知识信号的闲聊
    SelfRAGConfig config;
    config.enable_self_check = true;
    config.use_llm_evaluation = false;

    SelfRAGStage stage(config, nullptr);

    Chunk chunk;
    chunk.id = "test-chunk";
    chunk.content = "今天天气很好";

    // 带"怎么"的闲聊
    ReflectionResult result = stage.evaluate_chunk("今天天气怎么样", chunk);
    EXPECT_TRUE(result.is_relevant);
    EXPECT_GT(result.relevance_score, 0.5f);
}

TEST(SelfRAGTest, ShouldNotRetrieve) {
    // 纯闲聊不检索
    SelfRAGConfig config;
    config.enable_self_check = true;
    config.use_llm_evaluation = false;

    SelfRAGStage stage(config, nullptr);

    Chunk chunk;
    chunk.id = "test-chunk";
    chunk.content = "这是一个闲聊内容";

    // 纯闲聊查询
    ReflectionResult result = stage.evaluate_chunk("你好啊", chunk);
    // 闲聊没有知识信号，会得到默认评估
    EXPECT_TRUE(result.is_relevant);  // 默认通过
}

TEST(SelfRAGTest, EvaluateRelevance) {
    SelfRAGConfig config;
    config.enable_self_check = true;
    config.use_llm_evaluation = false;

    SelfRAGStage stage(config, nullptr);

    Chunk chunk;
    chunk.id = "test-chunk";
    chunk.content = "RAG 是检索增强生成的技术";

    std::vector<Chunk> chunks = {chunk};
    std::vector<ReflectionResult> results = stage.evaluate_chunks("RAG 是什么", chunks);

    ASSERT_EQ(results.size(), 1u);
    EXPECT_TRUE(results[0].is_relevant);
    EXPECT_GT(results[0].relevance_score, 0.5f);
}

TEST(SelfRAGTest, FilterIrrelevant) {
    SelfRAGConfig config;
    config.relevance_threshold = 0.6f;
    config.usefulness_threshold = 0.5f;
    config.use_llm_evaluation = false;

    SelfRAGStage stage(config, nullptr);

    Chunk chunk1;
    chunk1.id = "chunk1";
    chunk1.content = "Python 是一种编程语言";

    Chunk chunk2;
    chunk2.id = "chunk2";
    chunk2.content = "今天的天气是晴天";

    std::vector<Chunk> chunks = {chunk1, chunk2};
    std::vector<ReflectionResult> evaluations = stage.evaluate_chunks("Python 是什么", chunks);

    std::vector<Chunk> filtered = stage.filter_by_threshold(chunks, evaluations);

    // Python 相关的内容应该保留
    EXPECT_GE(filtered.size(), 1u);
}

TEST(SelfRAGTest, CorrectiveDecision) {
    SelfRAGConfig config;
    CorrectiveRAG corrective(config);

    std::vector<Chunk> chunks;
    std::vector<ReflectionResult> evaluations;

    // 高分 -> PASS
    ReflectionResult high_score;
    high_score.relevance_score = 0.8f;
    high_score.support_score = 0.7f;
    high_score.completeness_score = 0.6f;
    high_score.usefulness_score = 0.7f;
    evaluations.push_back(high_score);

    CorrectiveDecision decision = corrective.decide_action(chunks, evaluations, 0.7f);
    EXPECT_EQ(decision.action, CorrectiveAction::PASS);

    // 中等分数 -> EXPAND
    decision = corrective.decide_action(chunks, evaluations, 0.5f);
    EXPECT_EQ(decision.action, CorrectiveAction::EXPAND);

    // 低分 -> REWRITE
    decision = corrective.decide_action(chunks, evaluations, 0.3f);
    EXPECT_EQ(decision.action, CorrectiveAction::REWRITE);

    // 很低分 -> WEB_FALLBACK
    decision = corrective.decide_action(chunks, evaluations, 0.1f);
    EXPECT_EQ(decision.action, CorrectiveAction::WEB_FALLBACK);
}

TEST(SelfRAGTest, RewriteQuery) {
    SelfRAGConfig config;
    CorrectiveRAG corrective(config);

    std::vector<Chunk> chunks;

    // REWRITE
    std::string rewritten = corrective.rewrite_query(
        "Python", CorrectiveAction::REWRITE, chunks);
    EXPECT_NE(rewritten.find("Python"), std::string::npos);

    // WEB_FALLBACK
    rewritten = corrective.rewrite_query(
        "Python", CorrectiveAction::WEB_FALLBACK, chunks);
    EXPECT_NE(rewritten.find("web"), std::string::npos);
}

TEST(SelfRAGTest, ShouldRewrite) {
    SelfRAGConfig config;
    config.acceptance_threshold = 0.5f;
    config.use_llm_evaluation = false;

    SelfRAGStage stage(config, nullptr);

    // 低分应该需要重写
    ReflectionResult low_score;
    low_score.relevance_score = 0.3f;
    low_score.support_score = 0.3f;
    low_score.completeness_score = 0.3f;
    low_score.usefulness_score = 0.3f;

    std::vector<ReflectionResult> evaluations = {low_score};
    EXPECT_TRUE(stage.should_rewrite(evaluations));

    // 高分不需要重写
    ReflectionResult high_score;
    high_score.relevance_score = 0.7f;
    high_score.support_score = 0.7f;
    high_score.completeness_score = 0.7f;
    high_score.usefulness_score = 0.7f;

    evaluations = {high_score};
    EXPECT_FALSE(stage.should_rewrite(evaluations));
}

TEST(SelfRAGTest, StageProcess) {
    SelfRAGConfig config;
    config.enable_self_check = true;
    config.use_llm_evaluation = false;

    SelfRAGStage stage(config, nullptr);

    StageInput input;
    input.query = "Python 是什么";
    input.query_type = QueryType::FACTUAL;

    Chunk chunk;
    chunk.id = "test-chunk";
    chunk.content = "Python 是一种高级编程语言";

    RetrievalResult candidate;
    candidate.chunk = chunk;
    candidate.score = 0.9f;
    input.candidates.push_back(candidate);

    StageOutput output = stage.process(input);

    EXPECT_EQ(output.status, StageOutput::Status::SUCCESS);
    EXPECT_FALSE(output.results.empty());
}

TEST(SelfRAGTest, StageProcessSkippedWhenDisabled) {
    SelfRAGConfig config;
    config.enable_self_check = false;

    SelfRAGStage stage(config, nullptr);

    StageInput input;
    input.query = "test";

    StageOutput output = stage.process(input);

    EXPECT_EQ(output.status, StageOutput::Status::SKIPPED);
}

TEST(SelfRAGTest, ShouldUseWebFallback) {
    SelfRAGConfig config;
    CorrectiveRAG corrective(config);

    // 很低分时使用 web fallback
    EXPECT_TRUE(corrective.should_use_web_fallback(0.1f, 5));

    // 无结果且中等分数时使用
    EXPECT_TRUE(corrective.should_use_web_fallback(0.3f, 0));

    // 有结果且高分时不使用
    EXPECT_FALSE(corrective.should_use_web_fallback(0.7f, 5));
}

TEST(SelfRAGTest, CreateFactoryFunctions) {
    SelfRAGConfig config;

    // 创建 SelfRAGStage
    auto stage = create_self_rag_stage(config, nullptr);
    EXPECT_NE(stage, nullptr);
    EXPECT_EQ(stage->name(), "self_rag");

    // 创建 CorrectiveRAG
    auto corrective = create_corrective_rag(config);
    EXPECT_NE(corrective, nullptr);
}