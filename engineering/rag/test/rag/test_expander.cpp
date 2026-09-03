/**
 * @file test_expander.cpp
 * @brief Query Expander 测试用例
 */

#include "rag/query_expander.h"
#include "rag/pipeline.h"
#include <iostream>
#include <cassert>
#include <string>
#include <vector>
#include <memory>
#include <chrono>

using namespace rag;

// ========== 测试助手 ==========

void print_result(const ExpansionResult& result, const std::string& test_name) {
    std::cout << "\n=== " << test_name << " ===" << std::endl;
    std::cout << "Original Query: " << result.original_query << std::endl;
    std::cout << "Method: " << result.method << std::endl;
    std::cout << "Processing Time: " << result.processing_time_ms << "ms" << std::endl;
    std::cout << "Expanded Queries (" << result.expanded_queries.size() << "):" << std::endl;
    for (size_t i = 0; i < result.expanded_queries.size(); ++i) {
        std::cout << "  [" << i << "] " << result.expanded_queries[i]
                  << " (weight: " << result.weights[i] << ")" << std::endl;
    }
}

bool test_passed = true;

void check_condition(bool condition, const std::string& test_name) {
    if (!condition) {
        std::cout << "FAILED: " << test_name << std::endl;
        test_passed = false;
    } else {
        std::cout << "PASSED: " << test_name << std::endl;
    }
}

// ========== 测试用例 ==========

void test_hyde_expand() {
    std::cout << "\n--- Test: HyDEExpand ---" << std::endl;

    HyDEExpander expander(3);
    auto result = expander.expand("什么是检索增强生成？");

    print_result(result, "HyDEExpand");

    check_condition(result.original_query == "什么是检索增强生成？",
                    "Original query preserved");
    check_condition(result.method == "hyde", "Method is hyde");
    check_condition(result.expanded_queries.size() >= 2,
                    "At least 2 expanded queries (original + hypothesis)");
    check_condition(result.processing_time_ms >= 0,
                    "Processing time is valid");
    check_condition(result.weights[0] == 1.0f,
                    "Original query has weight 1.0");

    std::cout << "Expanded queries count: " << result.expanded_queries.size() << std::endl;
}

void test_synonym_expand() {
    std::cout << "\n--- Test: SynonymExpand ---" << std::endl;

    SynonymExpander expander(0.8f, 5);

    // 添加自定义同义词
    expander.add_synonyms("深度学习", {"深度神经网络", "CNN", "DNN"});
    expander.add_synonyms("NLP", {"自然语言处理", "文本分析"});

    auto result = expander.expand("深度学习在NLP中的应用");

    print_result(result, "SynonymExpand");

    check_condition(result.original_query == "深度学习在NLP中的应用",
                    "Original query preserved");
    check_condition(result.method == "synonym", "Method is synonym");
    check_condition(result.expanded_queries.size() >= 2,
                    "At least 2 expanded queries");
    check_condition(result.processing_time_ms >= 0,
                    "Processing time is valid");

    std::cout << "Expanded queries count: " << result.expanded_queries.size() << std::endl;
}

void test_composite_expand() {
    std::cout << "\n--- Test: CompositeExpand ---" << std::endl;

    CompositeExpander composite;
    composite.add_expander(std::make_shared<HyDEExpander>(2), 0.6f);
    composite.add_expander(std::make_shared<SynonymExpander>(0.7f, 3), 0.4f);

    auto result = composite.expand("什么是机器学习？");

    print_result(result, "CompositeExpand");

    check_condition(result.original_query == "什么是机器学习？",
                    "Original query preserved");
    check_condition(result.method == "composite", "Method is composite");
    check_condition(result.expanded_queries.size() >= 2,
                    "At least 2 expanded queries");
    check_condition(result.processing_time_ms >= 0,
                    "Processing time is valid");

    std::cout << "Expanded queries count: " << result.expanded_queries.size() << std::endl;
}

void test_query_expansion_config() {
    std::cout << "\n--- Test: QueryExpansionConfig ---" << std::endl;

    // 测试配置参数
    HyDEExpander hyde(5);  // max_hypotheses = 5
    auto hyde_result = hyde.expand("测试查询");

    check_condition(hyde_result.expanded_queries.size() >= 3,
                    "HyDE: at least 3 queries (1 original + 2 hypotheses)");

    SynonymExpander synonym(0.6f, 10);  // lower threshold, more expansions
    auto synonym_result = synonym.expand("测试查询");

    check_condition(synonym_result.method == "synonym",
                    "Synonym method set correctly");
    check_condition(synonym_result.expanded_queries.size() >= 1,
                    "Synonym: at least 1 query");

    // 测试配置更新
    hyde.set_max_hypotheses(1);
    hyde_result = hyde.expand("测试查询");
    check_condition(hyde_result.expanded_queries.size() >= 2,
                    "HyDE: updated max_hypotheses works");

    synonym.set_similarity_threshold(0.9f);
    synonym.set_max_expansions(3);
    check_condition(synonym_result.method == "synonym",
                    "Config changes accepted");

    std::cout << "HyDE expanded count: " << hyde_result.expanded_queries.size() << std::endl;
    std::cout << "Synonym expanded count: " << synonym_result.expanded_queries.size() << std::endl;
}

void test_tokenize() {
    std::cout << "\n--- Test: Tokenize ---" << std::endl;

    SynonymExpander expander;

    // 中文分词测试
    auto chinese_tokens = expander.tokenize("检索增强生成");
    check_condition(chinese_tokens.size() >= 4,
                    "Chinese tokenize: '检索增强生成' -> at least 4 tokens");
    std::cout << "Chinese tokens: ";
    for (const auto& t : chinese_tokens) {
        std::cout << "[" << t << "] ";
    }
    std::cout << std::endl;

    // 混合分词测试
    auto mixed_tokens = expander.tokenize("ML模型在NLP中的应用");
    check_condition(mixed_tokens.size() >= 4,
                    "Mixed tokenize: 'ML模型在NLP中的应用' -> at least 4 tokens");
    std::cout << "Mixed tokens: ";
    for (const auto& t : mixed_tokens) {
        std::cout << "[" << t << "] ";
    }
    std::cout << std::endl;

    // 英文分词测试
    auto english_tokens = expander.tokenize("machine learning models");
    check_condition(english_tokens.size() == 3,
                    "English tokenize: 'machine learning models' -> 3 tokens");
    check_condition(english_tokens[0] == "machine",
                    "First English token is 'machine'");
    check_condition(english_tokens[1] == "learning",
                    "Second English token is 'learning'");
    check_condition(english_tokens[2] == "models",
                    "Third English token is 'models'");

    // 标点分割测试
    auto punct_tokens = expander.tokenize("什么是AI？如何工作？");
    check_condition(punct_tokens.size() >= 3,
                    "Punctuation tokenize: '什么是AI？如何工作？' -> at least 3 tokens");
    std::cout << "Punct tokens: ";
    for (const auto& t : punct_tokens) {
        std::cout << "[" << t << "] ";
    }
    std::cout << std::endl;
}

// ========== 主函数 ==========

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "Query Expansion Module Tests" << std::endl;
    std::cout << "========================================" << std::endl;

    test_hyde_expand();
    test_synonym_expand();
    test_composite_expand();
    test_query_expansion_config();
    test_tokenize();

    std::cout << "\n========================================" << std::endl;
    if (test_passed) {
        std::cout << "ALL TESTS PASSED" << std::endl;
    } else {
        std::cout << "SOME TESTS FAILED" << std::endl;
    }
    std::cout << "========================================" << std::endl;

    return test_passed ? 0 : 1;
}