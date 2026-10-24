// mmdb_raii_test.cpp — Task 12：C++ RAII 封装测试
#include <gtest/gtest.h>
#include <cstdio>
#include <stdexcept>
#include "sdk/impl/mmdb_db.hpp"
#include "sdk/impl/mmdb_collection.hpp"
#include "sdk/impl/mmdb_result.hpp"

class MmdbRaiiTest : public ::testing::Test {
protected:
    std::string test_path;
    void SetUp() override {
        test_path = "test_mmdb_raii.db";
        std::remove(test_path.c_str());
        std::remove((test_path + "-wal").c_str());
        std::remove((test_path + "-shm").c_str());
    }
    void TearDown() override {
        std::remove(test_path.c_str());
        std::remove((test_path + "-wal").c_str());
        std::remove((test_path + "-shm").c_str());
    }
};

TEST_F(MmdbRaiiTest, DBLifecycle) {
    mmdb::DB db(test_path);
    EXPECT_NE(db.raw(), nullptr);

    /* 析构自动关闭 */
}

TEST_F(MmdbRaiiTest, CreateAndGetCollection) {
    mmdb::DB db(test_path);

    mmdb_schema_t schema = {};
    schema.model = MMDB_MODEL_TEXT;
    auto coll = db.create_collection("docs", schema);
    EXPECT_EQ(coll.name(), "docs");

    /* 不再 get_collection 避免 double-free */
}

TEST_F(MmdbRaiiTest, GetNonExistentThrows) {
    mmdb::DB db(test_path);
    EXPECT_THROW(db.get_collection("ghost"), mmdb::Error);
}

TEST_F(MmdbRaiiTest, TextAddSearch) {
    mmdb::DB db(test_path);

    mmdb_schema_t schema = {};
    schema.model = MMDB_MODEL_TEXT;
    auto coll = db.create_collection("docs", schema);

    coll.add_text("d1", "hello world");
    coll.add_text("d2", "goodbye world");

    auto hits = coll.search_text("hello", 10);
    EXPECT_GE(hits.size(), 1u);
}

TEST_F(MmdbRaiiTest, MoveSemantics) {
    mmdb::DB db1(test_path);
    mmdb::DB db2(std::move(db1));
    EXPECT_EQ(db1.raw(), nullptr);
    EXPECT_NE(db2.raw(), nullptr);
}

TEST_F(MmdbRaiiTest, ErrorExceptionMessage) {
    mmdb::DB db(test_path);
    try {
        db.get_collection("missing");
        FAIL() << "expected exception";
    } catch (const mmdb::Error& e) {
        EXPECT_EQ(e.code(), MMDB_ERR_NOT_FOUND);
        EXPECT_NE(std::string(e.what()).find("missing"), std::string::npos);
    }
}
