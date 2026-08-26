// mmdb_core_test.cpp — Task 3/4：核心模块（错误码 + mmdb 生命周期）测试
#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#include "sdk/mmdb.h"
#include "sdk/mmdb_error.h"
#include "sdk/mmdb_version.h"

namespace {
constexpr const char* kDbPath = "test_mmdb_core.db";
}

class MmdbCoreLifecycle : public ::testing::Test {
   protected:
    void SetUp() override { std::remove(kDbPath); }
    void TearDown() override { std::remove(kDbPath); }
};

TEST(MmdbCore, ErrorStringsAreStable) {
    EXPECT_STREQ(mmdb_strerror(MMDB_OK), "OK");
    EXPECT_STREQ(mmdb_strerror(MMDB_ERR_NOT_FOUND), "not found");
    EXPECT_STREQ(mmdb_strerror(MMDB_ERR_ALREADY), "already exists");
    EXPECT_STREQ(mmdb_strerror(MMDB_ERR_INVALID), "invalid argument");
}

TEST(MmdbCore, UnknownErrorReturnsFallback) {
    EXPECT_STREQ(mmdb_strerror(99999), "unknown error");
    EXPECT_STREQ(mmdb_strerror(-99999), "unknown error");
}

TEST(MmdbCore, AllKnownCodesAreNonNull) {
    int codes[] = {MMDB_OK,         MMDB_ERR_INVALID,  MMDB_ERR_NOT_FOUND,
                   MMDB_ERR_ALREADY, MMDB_ERR_IO,       MMDB_ERR_CORRUPT,
                   MMDB_ERR_FULL,    MMDB_ERR_INTERNAL, MMDB_ERR_NOMEM,
                   MMDB_ERR_TIMEOUT, MMDB_ERR_BUSY};
    for (int c : codes) {
        const char* s = mmdb_strerror(c);
        ASSERT_NE(s, nullptr);
        EXPECT_GT(std::strlen(s), 0u);
    }
}

TEST(MmdbCore, DefaultOptionsAreReasonable) {
    mmdb_options_t opts = MMDB_OPTIONS_DEFAULT;
    EXPECT_GE(opts.cache_size_kb, 1024);
    EXPECT_GE(opts.busy_timeout_ms, 1000);
    EXPECT_EQ(opts.enable_wal, 1);
}

TEST_F(MmdbCoreLifecycle, OpenAndCloseInMemory) {
    mmdb_t* db = mmdb_open(":memory:", nullptr);
    ASSERT_NE(db, nullptr);
    EXPECT_EQ(mmdb_last_error_code(db), MMDB_OK);
    mmdb_close(db);
}

TEST_F(MmdbCoreLifecycle, OpenWithFilePathCreatesFile) {
    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);
    mmdb_close(db);

    FILE* f = std::fopen(kDbPath, "rb");
    ASSERT_NE(f, nullptr);
    std::fclose(f);
}

TEST_F(MmdbCoreLifecycle, ReopenExistingFile) {
    mmdb_t* db1 = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db1, nullptr);
    mmdb_close(db1);

    mmdb_t* db2 = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db2, nullptr);
    EXPECT_EQ(mmdb_last_error_code(db2), MMDB_OK);
    mmdb_close(db2);
}

TEST_F(MmdbCoreLifecycle, NullPathReturnsNull) {
    mmdb_t* db = mmdb_open(nullptr, nullptr);
    EXPECT_EQ(db, nullptr);
}

TEST_F(MmdbCoreLifecycle, LastErrorMessageIsReadable) {
    mmdb_t* db = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db, nullptr);
    const char* msg = mmdb_last_error_message(db);
    ASSERT_NE(msg, nullptr);
    EXPECT_GT(std::strlen(msg), 0u);
    mmdb_close(db);
}

TEST(MmdbCore, VersionMacroAndFunctionMatch) {
    int maj = 0, min = 0, pat = 0;
    mmdb_version(&maj, &min, &pat);
    EXPECT_EQ(maj, MMDB_VERSION_MAJOR);
    EXPECT_EQ(min, MMDB_VERSION_MINOR);
    EXPECT_EQ(pat, MMDB_VERSION_PATCH);
}

TEST(MmdbCore, VersionHandlesNullOutputs) {
    mmdb_version(nullptr, nullptr, nullptr);
    int v = -1;
    mmdb_version(&v, nullptr, nullptr);
    EXPECT_EQ(v, MMDB_VERSION_MAJOR);
}

/* ------------------------------------------------------------------ */
/* Collection CRUD（Task 6）                                            */
/* ------------------------------------------------------------------ */

class MmdbCollectionCRUD : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;
    void SetUp() override {
        std::remove(kDbPath);
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);
    }
    void TearDown() override {
        if (db_) mmdb_close(db_);
        std::remove(kDbPath);
    }
};

TEST_F(MmdbCollectionCRUD, CreateVectorCollection) {
    mmdb_field_def_t fields[] = {
        {const_cast<char*>("label"), MMDB_TYPE_TEXT, 0, nullptr},
    };
    mmdb_schema_t schema = {MMDB_MODEL_VECTOR, 1, fields, 128};

    mmdb_collection_t* c = mmdb_collection_create(db_, "vec", &schema);
    EXPECT_NE(c, nullptr);
    EXPECT_STREQ(mmdb_collection_name(c), "vec");
    EXPECT_EQ(mmdb_collection_db(c), db_);
}

TEST_F(MmdbCollectionCRUD, CreateGraphCollection) {
    mmdb_schema_t schema = {MMDB_MODEL_GRAPH, 0, nullptr, 0};
    mmdb_collection_t* c = mmdb_collection_create(db_, "graph", &schema);
    EXPECT_NE(c, nullptr);
}

TEST_F(MmdbCollectionCRUD, CreateTimeseriesCollection) {
    mmdb_schema_t schema = {MMDB_MODEL_TIMESERIES, 0, nullptr, 0};
    mmdb_collection_t* c = mmdb_collection_create(db_, "ts", &schema);
    EXPECT_NE(c, nullptr);
}

TEST_F(MmdbCollectionCRUD, CreateTextCollection) {
    mmdb_schema_t schema = {MMDB_MODEL_TEXT, 0, nullptr, 0};
    mmdb_collection_t* c = mmdb_collection_create(db_, "txt", &schema);
    EXPECT_NE(c, nullptr);
}

TEST_F(MmdbCollectionCRUD, VectorSchemaRequiresDim) {
    mmdb_schema_t schema = {MMDB_MODEL_VECTOR, 0, nullptr, 0};
    mmdb_collection_t* c = mmdb_collection_create(db_, "bad", &schema);
    EXPECT_EQ(c, nullptr);
    EXPECT_EQ(mmdb_last_error_code(db_), MMDB_ERR_INVALID);
}

TEST_F(MmdbCollectionCRUD, DuplicateCreateReturnsNull) {
    mmdb_schema_t schema = {MMDB_MODEL_TEXT, 0, nullptr, 0};
    mmdb_collection_t* a = mmdb_collection_create(db_, "t", &schema);
    ASSERT_NE(a, nullptr);
    mmdb_collection_t* b = mmdb_collection_create(db_, "t", &schema);
    EXPECT_EQ(b, nullptr);
    EXPECT_EQ(mmdb_last_error_code(db_), MMDB_ERR_ALREADY);
}

TEST_F(MmdbCollectionCRUD, GetReturnsExistingCollection) {
    mmdb_schema_t schema = {MMDB_MODEL_TEXT, 0, nullptr, 0};
    mmdb_collection_t* a = mmdb_collection_create(db_, "t", &schema);
    ASSERT_NE(a, nullptr);
    mmdb_collection_t* b = mmdb_collection_get(db_, "t");
    EXPECT_EQ(a, b);
    EXPECT_EQ(mmdb_collection_get(db_, "nonexistent"), nullptr);
}

TEST_F(MmdbCollectionCRUD, DropRemovesCollection) {
    mmdb_schema_t schema = {MMDB_MODEL_TEXT, 0, nullptr, 0};
    mmdb_collection_t* a = mmdb_collection_create(db_, "t", &schema);
    ASSERT_NE(a, nullptr);
    mmdb_collection_drop(a);
    EXPECT_EQ(mmdb_collection_get(db_, "t"), nullptr);
}

TEST_F(MmdbCollectionCRUD, PersistsAcrossReopen) {
    {
        mmdb_schema_t schema = {MMDB_MODEL_TEXT, 0, nullptr, 0};
        mmdb_collection_t* a = mmdb_collection_create(db_, "t", &schema);
        ASSERT_NE(a, nullptr);
    }
    mmdb_close(db_);
    db_ = nullptr;

    db_ = mmdb_open(kDbPath, nullptr);
    ASSERT_NE(db_, nullptr);
    EXPECT_NE(mmdb_collection_get(db_, "t"), nullptr);
}

TEST_F(MmdbCollectionCRUD, NullArgsReturnNull) {
    mmdb_schema_t schema = {MMDB_MODEL_TEXT, 0, nullptr, 0};
    EXPECT_EQ(mmdb_collection_create(nullptr, "x", &schema), nullptr);
    EXPECT_EQ(mmdb_collection_create(db_, nullptr, &schema), nullptr);
    EXPECT_EQ(mmdb_collection_create(db_, "x", nullptr), nullptr);
    EXPECT_EQ(mmdb_collection_get(nullptr, "x"), nullptr);
    EXPECT_EQ(mmdb_collection_get(db_, nullptr), nullptr);
    EXPECT_EQ(mmdb_collection_name(nullptr), nullptr);
    EXPECT_EQ(mmdb_collection_db(nullptr), nullptr);
}