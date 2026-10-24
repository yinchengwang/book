// mmdb_filter_test.cpp — Task 7：filter JSON 编译测试
// 通过 impl 头直接调用 mmdb_filter_compile（仅测试用）
#include <gtest/gtest.h>
#include <cstring>

#include "sdk/impl/filter_parser.h"

TEST(MmdbFilter, NullJsonReturnsEmptyOrNull) {
    char* sql = mmdb_filter_compile(nullptr, nullptr);
    ASSERT_NE(sql, nullptr);
    EXPECT_STREQ(sql, "");
    free(sql);
}

TEST(MmdbFilter, EmptyObjectReturnsEmptyWhere) {
    mmdb_filter_params_t p;
    char* sql = mmdb_filter_compile("{}", &p);
    ASSERT_NE(sql, nullptr);
    EXPECT_STREQ(sql, "");
    free(sql);
    mmdb_filter_params_free(&p);
}

TEST(MmdbFilter, SimpleEquality) {
    mmdb_filter_params_t p;
    char* sql = mmdb_filter_compile(R"({"category":"news"})", &p);
    ASSERT_NE(sql, nullptr);
    EXPECT_STREQ(sql, "category = ?");
    EXPECT_EQ(p.int_count + p.text_count, 1u);
    free(sql);
    mmdb_filter_params_free(&p);
}

TEST(MmdbFilter, NumericEquality) {
    mmdb_filter_params_t p;
    char* sql = mmdb_filter_compile(R"({"score":42})", &p);
    ASSERT_NE(sql, nullptr);
    EXPECT_STREQ(sql, "score = ?");
    EXPECT_EQ(p.int_count, 1u);
    EXPECT_EQ(p.int_values[0], 42);
    free(sql);
    mmdb_filter_params_free(&p);
}

TEST(MmdbFilter, GreaterThan) {
    mmdb_filter_params_t p;
    char* sql = mmdb_filter_compile(R"({"score":{"$gt":0.5}})", &p);
    ASSERT_NE(sql, nullptr);
    EXPECT_STREQ(sql, "score > ?");
    EXPECT_EQ(p.int_count, 1u);
    free(sql);
    mmdb_filter_params_free(&p);
}

TEST(MmdbFilter, LessThanOrEqual) {
    mmdb_filter_params_t p;
    char* sql = mmdb_filter_compile(R"({"price":{"$lte":100}})", &p);
    ASSERT_NE(sql, nullptr);
    EXPECT_STREQ(sql, "price <= ?");
    free(sql);
    mmdb_filter_params_free(&p);
}

TEST(MmdbFilter, InOperator) {
    mmdb_filter_params_t p;
    char* sql = mmdb_filter_compile(R"({"tag":{"$in":["a","b","c"]}})", &p);
    ASSERT_NE(sql, nullptr);
    EXPECT_STREQ(sql, "tag IN (?,?,?)");
    EXPECT_EQ(p.text_count, 3u);
    free(sql);
    mmdb_filter_params_free(&p);
}

TEST(MmdbFilter, NotInOperator) {
    mmdb_filter_params_t p;
    char* sql = mmdb_filter_compile(R"({"id":{"$nin":[1,2,3]}})", &p);
    ASSERT_NE(sql, nullptr);
    EXPECT_STREQ(sql, "id NOT IN (?,?,?)");
    free(sql);
    mmdb_filter_params_free(&p);
}

TEST(MmdbFilter, MultipleFieldsAreJoinedByAnd) {
    mmdb_filter_params_t p;
    char* sql = mmdb_filter_compile(
        R"({"a":1,"b":"x","c":{"$gt":0}})", &p);
    ASSERT_NE(sql, nullptr);
    EXPECT_STREQ(sql, "a = ? AND b = ? AND c > ?");
    EXPECT_EQ(p.int_count, 2u);
    EXPECT_EQ(p.text_count, 1u);
    free(sql);
    mmdb_filter_params_free(&p);
}

TEST(MmdbFilter, NotEqualOperator) {
    mmdb_filter_params_t p;
    char* sql = mmdb_filter_compile(R"({"status":{"$ne":"deleted"}})", &p);
    ASSERT_NE(sql, nullptr);
    EXPECT_STREQ(sql, "status != ?");
    free(sql);
    mmdb_filter_params_free(&p);
}

TEST(MmdbFilter, UnknownOperatorReturnsNull) {
    mmdb_filter_params_t p;
    char* sql = mmdb_filter_compile(R"({"x":{"$weird":1}})", &p);
    EXPECT_EQ(sql, nullptr);
}