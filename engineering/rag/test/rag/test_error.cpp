/**
 * @file test_error.cpp
 * @brief 错误处理测试
 */

#include <gtest/gtest.h>
#include "rag/error.h"

using namespace rag;

TEST(ErrorCodesTest, SuccessCode) {
    EXPECT_TRUE(ErrorCodes::SUCCESS.is_success());
    EXPECT_FALSE(ErrorCodes::SUCCESS.is_error());
}

TEST(ErrorCodesTest, ErrorCodes) {
    EXPECT_FALSE(ErrorCodes::INTERNAL_ERROR.is_success());
    EXPECT_TRUE(ErrorCodes::INTERNAL_ERROR.is_error());
    EXPECT_EQ(ErrorCodes::INTERNAL_ERROR.http_status, 500);
    EXPECT_EQ(ErrorCodes::INTERNAL_ERROR.category, "system");

    EXPECT_EQ(ErrorCodes::CONFIG_NOT_FOUND.http_status, 500);
    EXPECT_EQ(ErrorCodes::MODEL_LOAD_FAILED.http_status, 500);
    EXPECT_EQ(ErrorCodes::INDEX_NOT_FOUND.http_status, 404);
    EXPECT_EQ(ErrorCodes::QUERY_TOO_SHORT.http_status, 400);
}

TEST(ExceptionTest, RAGException) {
    RAGException e(ErrorCodes::INTERNAL_ERROR, "Test error", "Test details");

    EXPECT_EQ(e.code(), "RAG-100-001");
    EXPECT_EQ(e.http_status(), 500);
    EXPECT_EQ(e.category(), "system");
    EXPECT_EQ(e.message(), "Test error");
    EXPECT_EQ(e.details(), "Test details");

    // 测试 what()
    std::string what = e.what();
    EXPECT_FALSE(what.empty());
    EXPECT_NE(what.find("Test error"), std::string::npos);
}

TEST(ExceptionTest, ConfigException) {
    ConfigException e(ErrorCodes::CONFIG_NOT_FOUND, "Config file missing");

    EXPECT_EQ(e.code(), "RAG-200-001");
    EXPECT_TRUE(dynamic_cast<ConfigException*>(&e) != nullptr);
}

TEST(ExceptionTest, ModelException) {
    ModelException e(ErrorCodes::MODEL_LOAD_FAILED, "Cannot load model");

    EXPECT_EQ(e.code(), "RAG-300-002");
    EXPECT_TRUE(dynamic_cast<ModelException*>(&e) != nullptr);
}

TEST(ExceptionTest, IndexException) {
    IndexException e(ErrorCodes::INDEX_NOT_FOUND, "Index not found");

    EXPECT_EQ(e.code(), "RAG-400-001");
    EXPECT_TRUE(dynamic_cast<IndexException*>(&e) != nullptr);
}

TEST(ExceptionTest, RetrievalException) {
    RetrievalException e(ErrorCodes::RETRIEVAL_FAILED, "Search failed");

    EXPECT_EQ(e.code(), "RAG-500-001");
    EXPECT_TRUE(dynamic_cast<RetrievalException*>(&e) != nullptr);
}

TEST(ExceptionTest, LLMException) {
    LLMException e(ErrorCodes::LLM_GENERATION_FAILED, "Generation failed");

    EXPECT_EQ(e.code(), "RAG-600-002");
    EXPECT_TRUE(dynamic_cast<LLMException*>(&e) != nullptr);
}

TEST(ExceptionTest, Context) {
    RAGException e(ErrorCodes::INTERNAL_ERROR, "Error with context");
    e.set_context("key1", "value1");
    e.set_context("key2", "value2");

    auto ctx = e.context();
    EXPECT_EQ(ctx.size(), 2);
    EXPECT_EQ(ctx["key1"], "value1");
    EXPECT_EQ(ctx["key2"], "value2");
}

TEST(ExceptionTest, ToJSON) {
    RAGException e(ErrorCodes::INTERNAL_ERROR, "Test error", "Test details");
    e.set_context("request_id", "12345");

    std::string json = e.to_json();
    EXPECT_FALSE(json.empty());
    EXPECT_NE(json.find("RAG-100-001"), std::string::npos);
    EXPECT_NE(json.find("Test error"), std::string::npos);
    EXPECT_NE(json.find("Test details"), std::string::npos);
}

TEST(ExceptionTest, Clone) {
    RAGException e(ErrorCodes::INTERNAL_ERROR, "Original error");
    e.set_context("key", "value");

    auto cloned = e.clone();
    EXPECT_EQ(cloned->code(), e.code());
    EXPECT_EQ(cloned->message(), e.message());
    EXPECT_EQ(cloned->context()["key"], "value");
}
