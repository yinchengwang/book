/**
 * @file test_evaluator.cpp
 * @brief RAG 评估器测试
 */

#include <gtest/gtest.h>
#include "rag/evaluator.h"
#include "rag/ollama_llm.h"
#include "rag/llm_service.h"

using namespace rag;

// ========== 检索指标测试 ==========

class RetrievalMetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        evaluator_ = std::make_shared<RAGEvaluator>();
    }

    std::shared_ptr<RAGEvaluator> evaluator_;
};

TEST_F(RetrievalMetricsTest, RecallAtK_AllRetrieved) {
    // 所有真实相关文档都被检索到
    std::vector<std::string> retrieved = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入",
        "HNSW 是高效的索引算法"
    };
    std::vector<std::string> ground_truths = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入",
        "HNSW 是高效的索引算法"
    };

    auto metrics = evaluator_->evaluate_retrieval(retrieved, ground_truths);
    EXPECT_EQ(metrics.recall_at_k, 1.0);
}

TEST_F(RetrievalMetricsTest, RecallAtK_PartialRetrieved) {
    // 部分文档被检索到
    std::vector<std::string> retrieved = {
        "RAG 是检索增强生成",
        "其他不相关的内容"
    };
    std::vector<std::string> ground_truths = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入",
        "HNSW 是高效的索引算法"
    };

    auto metrics = evaluator_->evaluate_retrieval(retrieved, ground_truths);
    EXPECT_GT(metrics.recall_at_k, 0.0);
    EXPECT_LT(metrics.recall_at_k, 1.0);
}

TEST_F(RetrievalMetricsTest, RecallAtK_NoneRetrieved) {
    std::vector<std::string> retrieved = {
        "完全不相关的内容"
    };
    std::vector<std::string> ground_truths = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入"
    };

    auto metrics = evaluator_->evaluate_retrieval(retrieved, ground_truths);
    EXPECT_EQ(metrics.recall_at_k, 0.0);
}

TEST_F(RetrievalMetricsTest, MRR_FirstRelevant) {
    std::vector<std::string> retrieved = {
        "RAG 是检索增强生成",  // 相关
        "其他内容",
        "更多内容"
    };
    std::vector<std::string> ground_truths = {
        "RAG 是检索增强生成"
    };

    auto metrics = evaluator_->evaluate_retrieval(retrieved, ground_truths);
    EXPECT_EQ(metrics.mrr, 1.0);  // 第一个就是相关的，MRR = 1/1 = 1
}

TEST_F(RetrievalMetricsTest, MRR_SecondRelevant) {
    std::vector<std::string> retrieved = {
        "不相关",
        "RAG 是检索增强生成",  // 第二个相关
        "更多内容"
    };
    std::vector<std::string> ground_truths = {
        "RAG 是检索增强生成"
    };

    auto metrics = evaluator_->evaluate_retrieval(retrieved, ground_truths);
    EXPECT_DOUBLE_EQ(metrics.mrr, 0.5);  // MRR = 1/2 = 0.5
}

TEST_F(RetrievalMetricsTest, MRR_NoRelevant) {
    std::vector<std::string> retrieved = {
        "不相关1",
        "不相关2",
        "不相关3"
    };
    std::vector<std::string> ground_truths = {
        "RAG 是检索增强生成"
    };

    auto metrics = evaluator_->evaluate_retrieval(retrieved, ground_truths);
    EXPECT_EQ(metrics.mrr, 0.0);
}

TEST_F(RetrievalMetricsTest, NDCG_PerfectRanking) {
    std::vector<std::string> retrieved = {
        "高相关",
        "中相关",
        "低相关"
    };
    std::vector<std::string> ground_truths = {
        "高相关",
        "中相关",
        "低相关"
    };
    std::vector<int> scores = {3, 2, 1};  // 相关性分数

    auto metrics = evaluator_->evaluate_retrieval(retrieved, ground_truths, scores);
    EXPECT_EQ(metrics.ndcg_at_k, 1.0);
}

TEST_F(RetrievalMetricsTest, PrecisionAtK) {
    std::vector<std::string> retrieved = {
        "相关",
        "相关",
        "不相关"
    };
    std::vector<std::string> ground_truths = {
        "相关",
        "其他相关"
    };

    auto metrics = evaluator_->evaluate_retrieval(retrieved, ground_truths);
    EXPECT_DOUBLE_EQ(metrics.precision_at_k, 2.0 / 3.0);
}

TEST_F(RetrievalMetricsTest, EmptyRetrieved) {
    std::vector<std::string> retrieved = {};
    std::vector<std::string> ground_truths = {
        "RAG 是检索增强生成"
    };

    auto metrics = evaluator_->evaluate_retrieval(retrieved, ground_truths);
    EXPECT_EQ(metrics.recall_at_k, 0.0);
    EXPECT_EQ(metrics.mrr, 0.0);
    EXPECT_EQ(metrics.ndcg_at_k, 0.0);
}

// ========== RAGAS 指标测试（无 LLM） ==========

class RAGEvaluationTest : public ::testing::Test {
protected:
    void SetUp() override {
        evaluator_ = std::make_shared<RAGEvaluator>();  // 无 LLM
    }

    std::shared_ptr<RAGEvaluator> evaluator_;
};

TEST_F(RAGEvaluationTest, FaithfulnessWithContext) {
    std::string question = "什么是 RAG？";
    std::string answer = "RAG 是检索增强生成技术。";
    std::vector<std::string> contexts = {
        "RAG 是检索增强生成（Retrieval-Augmented Generation）的缩写。"
    };

    auto eval = evaluator_->evaluate_generation(question, answer, contexts);

    // 由于使用简化计算，关键词匹配应该有较高分数
    EXPECT_GE(eval.faithfulness, 0.0);
    EXPECT_LE(eval.faithfulness, 1.0);
}

TEST_F(RAGEvaluationTest, AnswerRelevance) {
    std::string question = "什么是 RAG？";
    std::string answer = "RAG 是检索增强生成。";  // 包含问题中的关键词

    auto eval = evaluator_->evaluate_generation(question, answer, {});

    // 简化计算应该能识别关键词重叠
    EXPECT_GE(eval.answer_relevance, 0.0);
    EXPECT_LE(eval.answer_relevance, 1.0);
}

TEST_F(RAGEvaluationTest, ContextRelevance) {
    std::string question = "什么是 RAG？";
    std::vector<std::string> contexts = {
        "RAG 是检索增强生成。",
        "向量数据库用于存储嵌入。",
        "这是关于天气的信息。"
    };

    auto eval = evaluator_->evaluate_generation(question, "", contexts);

    // 前两个上下文与问题相关，应该有较高分数
    EXPECT_GE(eval.context_relevance, 0.0);
    EXPECT_LE(eval.context_relevance, 1.0);
}

TEST_F(RAGEvaluationTest, RAGASScore) {
    std::string question = "什么是 RAG？";
    std::string answer = "RAG 是检索增强生成。";
    std::vector<std::string> contexts = {"RAG 是检索增强生成。"};

    auto eval = evaluator_->evaluate_generation(question, answer, contexts);

    // 综合得分应该在合理范围内
    EXPECT_GE(eval.ragas_score, 0.0);
    EXPECT_LE(eval.ragas_score, 1.0);

    // 综合得分应该是各指标的加权平均
    double expected = 0.3 * eval.faithfulness +
                     0.4 * eval.answer_relevance +
                     0.3 * eval.context_relevance;
    EXPECT_DOUBLE_EQ(eval.ragas_score, expected);
}

// ========== 完整评估测试 ==========

TEST(EvaluationTest, FullEvaluation) {
    RAGEvaluator evaluator;

    std::string question = "RAG 是什么？";
    std::string answer = "RAG 是检索增强生成技术。";
    std::vector<std::string> contexts = {
        "RAG 是检索增强生成（Retrieval-Augmented Generation）。"
    };
    std::vector<std::string> ground_truths = {
        "RAG 是检索增强生成"
    };

    auto result = evaluator.evaluate(question, answer, contexts, ground_truths);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.question, question);
    EXPECT_EQ(result.answer, answer);
    EXPECT_GT(result.latency_ms, 0);
    EXPECT_GT(result.retrieval.recall_at_k, 0.0);
    EXPECT_GE(result.ragas.faithfulness, 0.0);
}

TEST(EvaluationTest, SummaryComputation) {
    std::vector<EvaluationResult> results = {
        EvaluationResult{
            .success = true,
            .retrieval = {.recall_at_k = 0.8, .mrr = 0.5, .ndcg_at_k = 0.7},
            .ragas = {.faithfulness = 0.9, .answer_relevance = 0.8, .context_relevance = 0.7, .ragas_score = 0.8},
            .latency_ms = 100
        },
        EvaluationResult{
            .success = true,
            .retrieval = {.recall_at_k = 0.6, .mrr = 0.3, .ndcg_at_k = 0.5},
            .ragas = {.faithfulness = 0.7, .answer_relevance = 0.6, .context_relevance = 0.5, .ragas_score = 0.6},
            .latency_ms = 150
        }
    };

    RAGEvaluator evaluator;
    auto summary = evaluator.compute_summary(results);

    EXPECT_EQ(summary.total_queries, 2);
    EXPECT_EQ(summary.successful_queries, 2);
    EXPECT_EQ(summary.success_rate, 1.0);
    EXPECT_DOUBLE_EQ(summary.avg_recall_at_k, 0.7);
    EXPECT_DOUBLE_EQ(summary.avg_mrr, 0.4);
    EXPECT_GT(summary.avg_latency_ms, 0);
}

TEST(EvaluationTest, JSONReport) {
    RAGEvaluator evaluator;

    EvaluationResult result;
    result.question = "测试问题";
    result.answer = "测试答案";
    result.contexts = {"上下文1"};
    result.success = true;
    result.retrieval = {.recall_at_k = 0.8};
    result.ragas = {.faithfulness = 0.9, .ragas_score = 0.85};

    auto report = evaluator.generate_json_report(result);

    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("测试问题"), std::string::npos);
    EXPECT_NE(report.find("recall_at_k"), std::string::npos);
}

TEST(EvaluationTest, CustomWeights) {
    RAGEvaluator evaluator;
    evaluator.set_ragas_weights(0.5, 0.3, 0.2);

    std::string question = "RAG 是什么？";
    std::string answer = "RAG 是检索增强生成。";
    std::vector<std::string> contexts = {"RAG 是检索增强生成。"};

    auto eval = evaluator.evaluate_generation(question, answer, contexts);

    double expected = 0.5 * eval.faithfulness +
                     0.3 * eval.answer_relevance +
                     0.2 * eval.context_relevance;
    EXPECT_DOUBLE_EQ(eval.ragas_score, expected);
}

TEST(EvaluationTest, RetrievalK) {
    RAGEvaluator evaluator;
    evaluator.set_retrieval_k(5);

    EXPECT_EQ(evaluator.retrieval_k(), 5);
}
