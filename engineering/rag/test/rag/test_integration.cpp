/**
 * @file test_integration.cpp
 * @brief 端到端集成测试
 */

#include <gtest/gtest.h>
#include "rag/engine.h"
#include "rag/ollama_embedding.h"
#include "rag/ollama_llm.h"
#include "rag/evaluator.h"

using namespace rag;

class IntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试引擎配置
        config_ = std::make_unique<EngineConfig>();
        config_->config = Config::default_config();
        config_->config.embedding.model_path = "simple";  // 使用简单 Embedding
        config_->load_existing_index = false;
        config_->auto_create_dirs = true;
    }

    std::unique_ptr<EngineConfig> config_;
};

TEST_F(IntegrationTest, EngineWithOllamaEmbedding) {
    // 测试 Ollama Embedding 服务
    OllamaEmbeddingConfig embed_config;
    embed_config.base_url = "http://localhost:11434";
    embed_config.model = "nomic-embed-text";

    auto embed_service = create_ollama_embedding_service(embed_config);

    // 检查服务就绪状态（可能不可用）
    EXPECT_TRUE(embed_service->is_ready() || embed_service->using_fallback());
    EXPECT_EQ(embed_service->dimension(), 768);
}

TEST_F(IntegrationTest, EngineWithOllamaLLM) {
    // 测试 Ollama LLM 服务
    OllamaLLMConfig llm_config;
    llm_config.base_url = "http://localhost:11434";
    llm_config.model = "llama3.2";

    auto llm_service = create_ollama_llm_service(llm_config);

    // 检查服务就绪状态
    EXPECT_TRUE(llm_service->is_loaded() || !llm_service->is_service_ready());
}

TEST_F(IntegrationTest, EvaluatorWithLLM) {
    // 测试评估器与 LLM 集成
    OllamaLLMConfig llm_config;
    auto llm_service = create_ollama_llm_service(llm_config);

    RAGEvaluator evaluator(llm_service);

    // 评估检索质量（不需要 LLM）
    std::vector<std::string> retrieved = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入"
    };
    std::vector<std::string> ground_truths = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入"
    };

    auto metrics = evaluator.evaluate_retrieval(retrieved, ground_truths);
    EXPECT_EQ(metrics.recall_at_k, 1.0);
}

TEST_F(IntegrationTest, EvaluatorRAGASScore) {
    // 测试 RAGAS 评分
    RAGEvaluator evaluator;  // 无 LLM，使用简化计算

    std::string question = "什么是 RAG？";
    std::string answer = "RAG 是检索增强生成技术。";
    std::vector<std::string> contexts = {
        "RAG（Retrieval-Augmented Generation）是检索增强生成技术。"
    };

    auto eval = evaluator.evaluate_generation(question, answer, contexts);

    // 各指标应该在 [0, 1] 范围内
    EXPECT_GE(eval.faithfulness, 0.0);
    EXPECT_LE(eval.faithfulness, 1.0);
    EXPECT_GE(eval.answer_relevance, 0.0);
    EXPECT_LE(eval.answer_relevance, 1.0);
    EXPECT_GE(eval.context_relevance, 0.0);
    EXPECT_LE(eval.context_relevance, 1.0);
    EXPECT_GE(eval.ragas_score, 0.0);
    EXPECT_LE(eval.ragas_score, 1.0);
}

TEST_F(IntegrationTest, EvaluatorJSONReport) {
    // 测试 JSON 报告生成
    RAGEvaluator evaluator;

    EvaluationResult result;
    result.question = "什么是 RAG？";
    result.answer = "RAG 是检索增强生成。";
    result.contexts = {"RAG 是检索增强生成。"};
    result.success = true;
    result.retrieval = {.recall_at_k = 0.9, .mrr = 0.8, .ndcg_at_k = 0.85};
    result.ragas = {.faithfulness = 0.9, .answer_relevance = 0.8,
                    .context_relevance = 0.85, .ragas_score = 0.85};

    auto report = evaluator.generate_json_report(result);

    // 验证 JSON 格式
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("question"), std::string::npos);
    EXPECT_NE(report.find("retrieval"), std::string::npos);
    EXPECT_NE(report.find("ragas"), std::string::npos);
    EXPECT_NE(report.find("recall_at_k"), std::string::npos);
}

TEST_F(IntegrationTest, DatasetLoading) {
    // 测试数据集加载
    RAGEvaluator evaluator;

    // 尝试加载测试数据集
    try {
        auto entries = evaluator.load_dataset("test/data/rag_benchmark.json");

        if (!entries.empty()) {
            EXPECT_FALSE(entries[0].question.empty());
            EXPECT_FALSE(entries[0].ground_truths.empty());

            // 评估第一个条目
            auto eval = evaluator.evaluate_generation(
                entries[0].question,
                "",  // 没有答案
                entries[0].contexts);

            EXPECT_GE(eval.ragas_score, 0.0);
        }
    } catch (const std::exception& e) {
        // 数据集可能不存在，跳过
        GTEST_SKIP() << "Dataset not found: " << e.what();
    }
}

TEST_F(IntegrationTest, FullEvaluationFlow) {
    // 完整的评估流程
    RAGEvaluator evaluator;

    std::vector<EvaluationResult> results;

    // 模拟多个查询的评估
    std::vector<std::string> questions = {
        "什么是 RAG？",
        "HNSW 是什么？",
        "向量嵌入如何生成？"
    };

    std::vector<std::string> answers = {
        "RAG 是检索增强生成技术。",
        "HNSW 是层次可导航小世界图算法。",
        "向量嵌入由 embedding 模型生成。"
    };

    std::vector<std::vector<std::string>> contexts = {
        {"RAG（Retrieval-Augmented Generation）是检索增强生成。"},
        {"HNSW（Hierarchical Navigable Small World）是高效的近似最近邻搜索算法。"},
        {"向量嵌入是将文本转换为稠密向量的技术。"}
    };

    std::vector<std::vector<std::string>> ground_truths = {
        {"RAG 是检索增强生成"},
        {"HNSW 是层次可导航小世界"},
        {"向量嵌入由 embedding 模型生成"}
    };

    for (size_t i = 0; i < questions.size(); i++) {
        auto result = evaluator.evaluate(
            questions[i], answers[i], contexts[i], ground_truths[i]);
        results.push_back(result);
    }

    // 生成汇总报告
    auto summary = evaluator.compute_summary(results);
    EXPECT_EQ(summary.total_queries, 3);
    EXPECT_EQ(summary.successful_queries, 3);
    EXPECT_GT(summary.avg_recall_at_k, 0.0);

    auto report = evaluator.generate_summary_report(results);
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("avg_recall_at_k"), std::string::npos);
}

#ifdef RAG_TEST_INTEGRATION

TEST_F(IntegrationTest, RealOllamaEmbedding) {
    OllamaEmbeddingConfig config;
    auto service = create_ollama_embedding_service(config);

    if (!service->is_ready()) {
        GTEST_SKIP() << "Ollama service not running";
    }

    // 测试真实的语义相似性
    auto emb1 = service->encode("What is artificial intelligence?");
    auto emb2 = service->encode("What is AI?");
    auto emb3 = service->encode("What is the weather today?");

    // 计算相似度
    float sim_same = cosine_similarity(emb1, emb2);
    float sim_diff = cosine_similarity(emb1, emb3);

    // 相似问题应该有更高的相似度
    EXPECT_GT(sim_same, sim_diff);
    EXPECT_GT(sim_same, 0.5);
}

TEST_F(IntegrationTest, RealOllamaLLMGeneration) {
    OllamaLLMConfig config;
    auto service = create_ollama_llm_service(config);

    if (!service->is_loaded()) {
        GTEST_SKIP() << "Ollama service not running";
    }

    GenerateOptions options;
    options.max_tokens = 100;
    options.temperature = 0.7f;

    auto result = service->generate("What is RAG in one sentence.", options);

    EXPECT_FALSE(result.text.empty());
    EXPECT_GT(result.tokens_generated, 0);
}

#endif  // RAG_TEST_INTEGRATION
