/**
 * @file namespace_test.cpp
 * @brief 命名空间隔离与资源配额测试（多租户 P6-M3.3）
 *
 * 覆盖场景：
 *   1. CreateGet       - 创建和获取命名空间
 *   2. GetNotFound     - 获取不存在的命名空间
 *   3. SetQuota        - 设置配额
 *   4. Usage           - 使用量查询
 *   5. Drop            - 删除命名空间
 *   6. InvalidParams   - NULL / 空参数
 *   7. DoubleDrop      - 重复删除
 */
#include <gtest/gtest.h>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_namespace.h"
}

#include <cstdio>
#include <cstring>

namespace {

constexpr const char* kDbPath = "test_namespace.db";

void cleanup_db() {
    std::remove(kDbPath);
}

}  // namespace

class NamespaceTest : public ::testing::Test {
   protected:
    mmdb_t* db_ = nullptr;

    void SetUp() override {
        cleanup_db();
        db_ = mmdb_open(kDbPath, nullptr);
        ASSERT_NE(db_, nullptr);
    }

    void TearDown() override {
        if (db_) mmdb_close(db_);
        cleanup_db();
    }
};

/* ======================================================================== */
/* CreateGet                                                                */
/* ======================================================================== */

TEST_F(NamespaceTest, CreateGet) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "tenant_a", nullptr, &ns), MMDB_OK);
    ASSERT_NE(ns, nullptr);

    /* 获取已创建的命名空间 */
    mmdb_namespace_t* ns2 = nullptr;
    ASSERT_EQ(mmdb_namespace_get(db_, "tenant_a", &ns2), MMDB_OK);
    EXPECT_EQ(ns, ns2);
}

/* ======================================================================== */
/* GetNotFound                                                              */
/* ======================================================================== */

TEST_F(NamespaceTest, GetNotFound) {
    mmdb_namespace_t* ns = nullptr;
    EXPECT_EQ(mmdb_namespace_get(db_, "nonexistent", &ns), MMDB_ERR_NOT_FOUND);
}

/* ======================================================================== */
/* SetQuota                                                                 */
/* ======================================================================== */

TEST_F(NamespaceTest, SetQuota) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "quota_set", nullptr, &ns), MMDB_OK);

    const char* quota = R"({"vectors_max":1000,"collections_max":10,"disk_max_bytes":1048576})";
    ASSERT_EQ(mmdb_namespace_set_quota(ns, quota), MMDB_OK);

    /* 验证配额已生效（通过 usage JSON 读取） */
    char buf[512] = {0};
    ASSERT_EQ(mmdb_namespace_usage(ns, buf, sizeof(buf)), MMDB_OK);
    EXPECT_NE(std::string(buf).find("\"vectors_max\":1000"), std::string::npos);
}

/* ======================================================================== */
/* Usage                                                                    */
/* ======================================================================== */

TEST_F(NamespaceTest, Usage) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "usage_test", nullptr, &ns), MMDB_OK);

    char buf[512] = {0};
    ASSERT_EQ(mmdb_namespace_usage(ns, buf, sizeof(buf)), MMDB_OK);

    /* 初始向量数为 0 */
    EXPECT_NE(std::string(buf).find("\"vectors_used\":0"), std::string::npos);
}

/* ======================================================================== */
/* Drop                                                                     */
/* ======================================================================== */

TEST_F(NamespaceTest, Drop) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "drop_test", nullptr, &ns), MMDB_OK);

    ASSERT_EQ(mmdb_namespace_drop(ns), MMDB_OK);

    /* 删除后不应再获取到 */
    mmdb_namespace_t* ns2 = nullptr;
    EXPECT_EQ(mmdb_namespace_get(db_, "drop_test", &ns2), MMDB_ERR_NOT_FOUND);
}

/* ======================================================================== */
/* InvalidParams                                                            */
/* ======================================================================== */

TEST_F(NamespaceTest, InvalidParams) {
    mmdb_namespace_t* ns = nullptr;

    /* NULL db */
    EXPECT_EQ(mmdb_namespace_create(nullptr, "x", nullptr, &ns), MMDB_ERR_INVALID);
    /* NULL name */
    EXPECT_EQ(mmdb_namespace_create(db_, nullptr, nullptr, &ns), MMDB_ERR_INVALID);
    /* NULL out_ns */
    EXPECT_EQ(mmdb_namespace_create(db_, "x", nullptr, nullptr), MMDB_ERR_INVALID);
    /* 空名称 */
    EXPECT_EQ(mmdb_namespace_create(db_, "", nullptr, &ns), MMDB_ERR_INVALID);

    /* get NULL */
    EXPECT_EQ(mmdb_namespace_get(nullptr, "x", &ns), MMDB_ERR_INVALID);
    EXPECT_EQ(mmdb_namespace_get(db_, nullptr, &ns), MMDB_ERR_INVALID);
    EXPECT_EQ(mmdb_namespace_get(db_, "x", nullptr), MMDB_ERR_INVALID);

    /* set_quota NULL */
    EXPECT_EQ(mmdb_namespace_set_quota(nullptr, "{}"), MMDB_ERR_INVALID);
    EXPECT_EQ(mmdb_namespace_set_quota(ns, nullptr), MMDB_ERR_INVALID);

    /* usage NULL */
    char buf[64];
    EXPECT_EQ(mmdb_namespace_usage(nullptr, buf, sizeof(buf)), MMDB_ERR_INVALID);

    /* drop NULL */
    EXPECT_EQ(mmdb_namespace_drop(nullptr), MMDB_ERR_INVALID);
}

/* ======================================================================== */
/* DoubleDrop                                                               */
/* ======================================================================== */

TEST_F(NamespaceTest, DoubleDrop) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "double_drop", nullptr, &ns), MMDB_OK);

    /* 第一次删除成功 */
    ASSERT_EQ(mmdb_namespace_drop(ns), MMDB_OK);

    /* 第二次删除应返回 INVALID（指针仍有效但已标记 dropped） */
    EXPECT_EQ(mmdb_namespace_drop(ns), MMDB_ERR_INVALID);
}
