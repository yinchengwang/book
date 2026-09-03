/**
 * @file test_evaluator.cpp
 * @brief RAG 评估器测试
 */

#include <gtest/gtest.h>
#include "rag/evaluator.h"

using namespace rag;

// ========== 召回率计算测试 ==========

class RecallComputation : public ::testing::Test {
protected:
    void SetUp() override {
        evaluator_ = std::make_unique<RAGEvaluator>();
    }

    std::unique_ptr<RAGEvaluator> evaluator_;
};

TEST_F(RecallComputation, AllRetrieved) {
    std::vector<std::string> retrieved = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入",
        "HNSW 是高效的索引算法"
    };
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入",
        "HNSW 是高效的索引算法"
    };

    double recall = evaluator_->compute_recall(retrieved, relevant, 3);
    EXPECT_EQ(recall, 1.0);
}

TEST_F(RecallComputation, PartialRetrieved) {
    std::vector<std::string> retrieved = {
        "RAG 是检索增强生成",
        "其他不相关的内容"
    };
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入",
        "HNSW 是高效的索引算法"
    };

    double recall = evaluator_->compute_recall(retrieved, relevant, 10);
    EXPECT_GT(recall, 0.0);
    EXPECT_LT(recall, 1.0);
}

TEST_F(RecallComputation, NoneRetrieved) {
    std::vector<std::string> retrieved = {
        "完全不相关的内容"
    };
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入"
    };

    double recall = evaluator_->compute_recall(retrieved, relevant, 10);
    EXPECT_EQ(recall, 0.0);
}

TEST_F(RecallComputation, EmptyRetrieved) {
    std::vector<std::string> retrieved = {};
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成"
    };

    double recall = evaluator_->compute_recall(retrieved, relevant, 10);
    EXPECT_EQ(recall, 0.0);
}

// ========== 精确率计算测试 ==========

class PrecisionComputation : public ::testing::Test {
protected:
    void SetUp() override {
        evaluator_ = std::make_unique<RAGEvaluator>();
    }

    std::unique_ptr<RAGEvaluator> evaluator_;
};

TEST_F(PrecisionComputation, PerfectPrecision) {
    std::vector<std::string> retrieved = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入"
    };
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成",
        "向量数据库用于存储嵌入"
    };

    double precision = evaluator_->compute_precision(retrieved, relevant, 2);
    EXPECT_EQ(precision, 1.0);
}

TEST_F(PrecisionComputation, PartialPrecision) {
    std::vector<std::string> retrieved = {
        "RAG 是检索增强生成",
        "其他不相关的内容",
        "更多不相关"
    };
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成"
    };

    double precision = evaluator_->compute_precision(retrieved, relevant, 3);
    EXPECT_GT(precision, 0.0);
    EXPECT_LT(precision, 1.0);
}

TEST_F(PrecisionComputation, EmptyRetrieved) {
    std::vector<std::string> retrieved = {};
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成"
    };

    double precision = evaluator_->compute_precision(retrieved, relevant, 10);
    EXPECT_EQ(precision, 0.0);
}

// ========== MRR 计算测试 ==========

class MRRComputation : public ::testing::Test {
protected:
    void SetUp() override {
        evaluator_ = std::make_unique<RAGEvaluator>();
    }

    std::unique_ptr<RAGEvaluator> evaluator_;
};

TEST_F(MRRComputation, FirstRelevant) {
    std::vector<std::string> retrieved = {
        "RAG 是检索增强生成",  // 相关
        "其他内容",
        "更多内容"
    };
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成"
    };

    double mrr = evaluator_->compute_mrr(retrieved, relevant);
    EXPECT_EQ(mrr, 1.0);  // 第一个就是相关的，MRR = 1/1 = 1
}

TEST_F(MRRComputation, SecondRelevant) {
    std::vector<std::string> retrieved = {
        "不相关",
        "RAG 是检索增强生成",  // 第二个相关
        "更多内容"
    };
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成"
    };

    double mrr = evaluator_->compute_mrr(retrieved, relevant);
    EXPECT_DOUBLE_EQ(mrr, 0.5);  // MRR = 1/2 = 0.5
}

TEST_F(MRRComputation, NoRelevant) {
    std::vector<std::string> retrieved = {
        "不相关1",
        "不相关2",
        "不相关3"
    };
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成"
    };

    double mrr = evaluator_->compute_mrr(retrieved, relevant);
    EXPECT_EQ(mrr, 0.0);
}

TEST_F(MRRComputation, EmptyRetrieved) {
    std::vector<std::string> retrieved = {};
    std::vector<std::string> relevant = {
        "RAG 是检索增强生成"
    };

    double mrr = evaluator_->compute_mrr(retrieved, relevant);
    EXPECT_EQ(mrr, 0.0);
}

// ========== NDCG 计算测试 ==========

class NDCGComputation : public ::testing::Test {
protected:
    void SetUp() override {
        evaluator_ = std::make_unique<RAGEvaluator>();
    }

    std::unique_ptr<RAGEvaluator> evaluator_;
};

TEST_F(NDCGComputation, PerfectRanking) {
    std::vector<std::string> retrieved = {
        "高相关文档",
        "中相关文档",
        "低相关文档"
    };
    std::vector<std::string> relevant = {
        "高相关文档",
        "中相关文档",
        "低相关文档"
    };

    double ndcg = evaluator_->compute_ndcg(retrieved, relevant, 3);
    EXPECT_EQ(ndcg, 1.0);
}

TEST_F(NDCGComputation, ReversedRanking) {
    std::vector<std::string> retrieved = {
        "低相关文档",
        "中相关文档",
        "高相关文档"
    };
    std::vector<std::string> relevant = {
        "高相关文档",
        "中相关文档",
        "低相关文档"
    };

    double ndcg = evaluator_->compute_ndcg(retrieved, relevant, 3);
    EXPECT_LT(ndcg, 1.0);
    EXPECT_GT(ndcg, 0.0);
}

TEST_F(NDCGComputation, NoRelevant) {
    std::vector<std::string> retrieved = {
        "完全不相关"
    };
    std::vector<std::string> relevant = {
        "高相关文档"
    };

    double ndcg = evaluator_->compute_ndcg(retrieved, relevant, 3);
    EXPECT_GE(ndcg, 0.0);
}

TEST_F(NDCGComputation, EmptyRetrieved) {
    std::vector<std::string> retrieved = {};
    std::vector<std::string> relevant = {
        "高相关文档"
    };

    double ndcg = evaluator_->compute_ndcg(retrieved, relevant, 3);
    EXPECT_EQ(ndcg, 0.0);
}

// ========== 评估器集成测试（使用 Mock Pipeline）============

class EvaluatorWithMock : public ::testing::Test {
protected:
    void SetUp() override {
        evaluator_ = std::make_unique<RAGEvaluator>();
    }

    std::unique_ptr<RAGEvaluator> evaluator_;
};

// 测试评估器的基本功能
TEST_F(EvaluatorWithMock, AddTestCase) {
    TestCase tc;
    tc.query = "什么是 RAG？";
    tc.relevant_chunks = {"RAG 是检索增强生成"};
    tc.description = "RAG 定义查询";
    tc.category = "definition";

    evaluator_->add_test_case(tc);

    // 通过添加后的行为验证（不抛出异常即认为成功）
    SUCCEED();
}

TEST_F(EvaluatorWithMock, AddMultipleTestCases) {
    std::vector<TestCase> cases = {
        {"查询1", {"相关文档1"}, "描述1", "category1"},
        {"查询2", {"相关文档2"}, "描述2", "category2"}
    };

    evaluator_->add_test_cases(cases);

    SUCCEED();
}

TEST_F(EvaluatorWithMock, PrintSummary) {
    EvaluationResult result;
    result.total_queries = 10;
    result.successful_queries = 8;
    result.avg_latency_ms = 45.5;
    result.recall_at_1 = 0.5;
    result.recall_at_5 = 0.7;
    result.recall_at_10 = 0.85;
    result.recall_at_20 = 0.9;
    result.precision_at_1 = 0.6;
    result.precision_at_5 = 0.5;
    result.precision_at_10 = 0.4;
    result.mrr = 0.65;
    result.map = 0.62;
    result.ndcg_at_5 = 0.68;
    result.ndcg_at_10 = 0.7;
    result.ndcg_at_20 = 0.72;

    // 测试打印不抛出异常
    evaluator_->print_summary(result);

    SUCCEED();
}

// ========== TestSuiteGenerator 测试 ==========

class TestSuiteGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override {
        generator_ = std::make_unique<TestSuiteGenerator>();
    }

    std::unique_ptr<TestSuiteGenerator> generator_;
};

TEST_F(TestSuiteGeneratorTest, GenerateSynthetic) {
    auto cases = TestSuiteGenerator::generate_synthetic("RAG", 5);
    // 简化实现返回空结果
    EXPECT_TRUE(cases.empty());
}

TEST_F(TestSuiteGeneratorTest, GenerateFromDocs) {
    auto cases = TestSuiteGenerator::generate_from_docs("/path/to/docs", 5);
    // 简化实现返回空结果
    EXPECT_TRUE(cases.empty());
}

// ========== 工厂函数测试 ==========

TEST(FactoryFunctionTest, CreateEvaluator) {
    auto evaluator = create_evaluator();
    EXPECT_NE(evaluator, nullptr);
}

TEST(FactoryFunctionTest, CreateTestSuiteGenerator) {
    auto generator = create_test_suite_generator();
    EXPECT_NE(generator, nullptr);
}

// ========== DCG/IDCG 辅助函数测试 ==========

class DCGIDCGTest : public ::testing::Test {
protected:
    void SetUp() override {
        evaluator_ = std::make_unique<RAGEvaluator>();
    }

    std::unique_ptr<RAGEvaluator> evaluator_;
};

TEST_F(DCGIDCGTest, DCGCalculation) {
    std::vector<double> relevance = {3.0, 2.0, 1.0};
    double dcg = evaluator_->compute_dcg(relevance, 3);

    // DCG = 3/log2(2) + 2/log2(3) + 1/log2(4) = 3 + 1.26 + 0.5 = 4.76
    EXPECT_GT(dcg, 4.0);
    EXPECT_LT(dcg, 5.0);
}

TEST_F(DCGIDCGTest, IDCGCalculation) {
    std::vector<double> relevance = {1.0, 2.0, 3.0};  // 未排序
    double idcg = evaluator_->compute_idcg(relevance, 3);

    // IDCG 应该是排序后 (3, 2, 1) 的 DCG
    std::vector<double> sorted = {3.0, 2.0, 1.0};
    double expected = evaluator_->compute_dcg(sorted, 3);

    EXPECT_DOUBLE_EQ(idcg, expected);
}

TEST_F(DCGIDCGTest, DCGWithK) {
    std::vector<double> relevance = {3.0, 2.0, 1.0, 0.5, 0.1};
    double dcg_full = evaluator_->compute_dcg(relevance, 5);
    double dcg_partial = evaluator_->compute_dcg(relevance, 3);

    EXPECT_GT(dcg_full, dcg_partial);
}
