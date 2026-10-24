// mmdb_text_test.cpp — Task 11：文本模型（add/get/delete/search）测试
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>

#include "sdk/mmdb.h"
#include "sdk/mmdb_text.h"

namespace {
constexpr const char* kDbPath = "test_mmdb_text.db";
}

class MmdbTextTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;
    mmdb_collection_t* coll_ = nullptr;

    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);
        mmdb_schema_t s = {MMDB_MODEL_TEXT, 0, nullptr, 0};
        coll_ = mmdb_collection_create(db_, "docs", &s);
        ASSERT_NE(coll_, nullptr);
    }
    void TearDown() override {
        if (db_) mmdb_close(db_);
        std::remove(kDbPath);
    }
};

TEST_F(MmdbTextTest, AddSingle) {
    mmdb_text_entry_t e = {"doc1", "Hello world", nullptr};
    EXPECT_EQ(mmdb_text_add(coll_, &e), MMDB_OK);
}

TEST_F(MmdbTextTest, AddBatch) {
    mmdb_text_entry_t entries[3] = {
        {"d1", "first document", nullptr},
        {"d2", "second document", nullptr},
        {"d3", "third document", nullptr}
    };
    EXPECT_EQ(mmdb_text_add_batch(coll_, entries, 3), MMDB_OK);
}

TEST_F(MmdbTextTest, GetExisting) {
    mmdb_text_entry_t e = {"doc1", "Hello world", R"({"lang":"en"})"};
    ASSERT_EQ(mmdb_text_add(coll_, &e), MMDB_OK);

    mmdb_text_entry_t out = {};
    ASSERT_EQ(mmdb_text_get(coll_, "doc1", &out), MMDB_OK);
    EXPECT_STREQ(out.text, "Hello world");
    free((void*)out.text);
    free((void*)out.metadata_json);
}

TEST_F(MmdbTextTest, GetMissing) {
    mmdb_text_entry_t out = {};
    EXPECT_EQ(mmdb_text_get(coll_, "ghost", &out), MMDB_ERR_NOT_FOUND);
}

TEST_F(MmdbTextTest, DeleteExisting) {
    mmdb_text_entry_t e = {"d1", "test", nullptr};
    ASSERT_EQ(mmdb_text_add(coll_, &e), MMDB_OK);
    EXPECT_EQ(mmdb_text_delete(coll_, "d1"), MMDB_OK);

    mmdb_text_entry_t out = {};
    EXPECT_EQ(mmdb_text_get(coll_, "d1", &out), MMDB_ERR_NOT_FOUND);
}

TEST_F(MmdbTextTest, SearchBasic) {
    mmdb_text_entry_t entries[3] = {
        {"d1", "the quick brown fox", nullptr},
        {"d2", "lazy dog sleeps", nullptr},
        {"d3", "fox and dog are friends", nullptr}
    };
    ASSERT_EQ(mmdb_text_add_batch(coll_, entries, 3), MMDB_OK);

    mmdb_text_query_t q = {"fox", 10, nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_text_search(coll_, &q, &result), MMDB_OK);
    EXPECT_GE(result.count, 1u);
    mmdb_result_free(&result);
}

TEST_F(MmdbTextTest, SearchTopK) {
    mmdb_text_entry_t entries[5] = {
        {"d1", "alpha", nullptr},
        {"d2", "alpha alpha", nullptr},
        {"d3", "alpha alpha alpha", nullptr},
        {"d4", "beta", nullptr},
        {"d5", "gamma", nullptr}
    };
    ASSERT_EQ(mmdb_text_add_batch(coll_, entries, 5), MMDB_OK);

    mmdb_text_query_t q = {"alpha", 2, nullptr};
    mmdb_result_t result = {};
    ASSERT_EQ(mmdb_text_search(coll_, &q, &result), MMDB_OK);
    EXPECT_EQ(result.count, 2u);
    mmdb_result_free(&result);
}

TEST_F(MmdbTextTest, SearchEmpty) {
    mmdb_text_query_t q = {"nothing", 10, nullptr};
    mmdb_result_t result = {};
    EXPECT_EQ(mmdb_text_search(coll_, &q, &result), MMDB_OK);
    EXPECT_EQ(result.count, 0u);
}

TEST_F(MmdbTextTest, WrongCollectionModelFails) {
    mmdb_schema_t vs = {MMDB_MODEL_VECTOR, 0, nullptr, 4};
    mmdb_collection_t* v = mmdb_collection_create(db_, "vec", &vs);
    ASSERT_NE(v, nullptr);

    mmdb_text_entry_t e = {"d1", "test", nullptr};
    EXPECT_NE(mmdb_text_add(v, &e), MMDB_OK);
}
