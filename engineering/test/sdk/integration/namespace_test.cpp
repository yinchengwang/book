/**
 * @file namespace_test.cpp
 * @brief 命名空间隔离与资源配额测试（多租户 P6-M3.3）
 *
 * 覆盖场景：
 *   1. CreateGet            - 创建和获取命名空间
 *   2. DuplicateName       - 重复创建同名命名空间
 *   3. GetNotFound          - 获取不存在的命名空间
 *   4. DataIsolation        - 不同命名空间的数据隔离
 *   5. NamespaceIdentity    - 命名空间标识（名称正确）
 *   6. SetQuota             - 设置配额
 *   7. QuotaCheckPass       - 未超出配额时允许操作
 *   8. QuotaCheckExceed     - 超出配额时拒绝操作
 *   9. QuotaNoLimit         - 配额为 0 表示无限制
 *  10. AddVectorsUsage      - 增加向量后使用量正确
 *  11. Usage                - 使用量查询
 *  12. Drop                 - 删除命名空间
 *  13. DropThenGet          - 删除后再获取返回 NOT_FOUND
 *  14. ReopenSameName       - 删除后可重新创建同名
 *  15. MultipleNamespaces   - 多命名空间并存
 *  16. InvalidParams        - NULL / 空参数
 *  17. DoubleDrop           - 重复删除
 */
#include <gtest/gtest.h>

extern "C" {
#include "sdk/mmdb.h"
#include "sdk/mmdb_namespace.h"
}

#include <cstdio>
#include <cstring>
#include <string>

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

    mmdb_namespace_t* ns2 = nullptr;
    ASSERT_EQ(mmdb_namespace_get(db_, "tenant_a", &ns2), MMDB_OK);
    EXPECT_EQ(ns, ns2);
}

/* ======================================================================== */
/* DuplicateName                                                            */
/* ======================================================================== */

TEST_F(NamespaceTest, DuplicateName) {
    mmdb_namespace_t* ns1 = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "tenant_dup", nullptr, &ns1), MMDB_OK);

    mmdb_namespace_t* ns2 = nullptr;
    EXPECT_EQ(mmdb_namespace_create(db_, "tenant_dup", nullptr, &ns2), MMDB_ERR_ALREADY);
}

/* ======================================================================== */
/* GetNotFound                                                              */
/* ======================================================================== */

TEST_F(NamespaceTest, GetNotFound) {
    mmdb_namespace_t* ns = nullptr;
    EXPECT_EQ(mmdb_namespace_get(db_, "nonexistent", &ns), MMDB_ERR_NOT_FOUND);
}

/* ======================================================================== */
/* DataIsolation                                                            */
/* ======================================================================== */

TEST_F(NamespaceTest, DataIsolation) {
    mmdb_namespace_t* ns_a = nullptr;
    mmdb_namespace_t* ns_b = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "iso_a", nullptr, &ns_a), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_create(db_, "iso_b", nullptr, &ns_b), MMDB_OK);

    /* 各自增加不同数量的向量 */
    ASSERT_EQ(mmdb_namespace_add_vectors(ns_a, 100), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_add_vectors(ns_b, 200), MMDB_OK);

    /* 验证使用量隔离 */
    char json_a[256] = {0};
    char json_b[256] = {0};
    ASSERT_EQ(mmdb_namespace_usage(ns_a, json_a, sizeof(json_a)), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_usage(ns_b, json_b, sizeof(json_b)), MMDB_OK);

    EXPECT_NE(strstr(json_a, "\"vectors_used\":100"), nullptr);
    EXPECT_NE(strstr(json_b, "\"vectors_used\":200"), nullptr);
}

/* ======================================================================== */
/* NamespaceIdentity                                                        */
/* ======================================================================== */

TEST_F(NamespaceTest, NamespaceIdentity) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "identity_test", nullptr, &ns), MMDB_OK);

    const char* name = mmdb_namespace_name(ns);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "identity_test");
}

TEST_F(NamespaceTest, NameNullHandle) {
    EXPECT_EQ(mmdb_namespace_name(nullptr), nullptr);
}

/* ======================================================================== */
/* SetQuota                                                                 */
/* ======================================================================== */

TEST_F(NamespaceTest, SetQuota) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "quota_set", nullptr, &ns), MMDB_OK);

    const char* quota = R"({"vectors_max":1000,"collections_max":10,"disk_max_bytes":1048576})";
    ASSERT_EQ(mmdb_namespace_set_quota(ns, quota), MMDB_OK);

    /* 验证配额已生效 */
    char json[512] = {0};
    ASSERT_EQ(mmdb_namespace_usage(ns, json, sizeof(json)), MMDB_OK);
    EXPECT_NE(std::string(json).find("\"vectors_limit\":1000"), std::string::npos);
    EXPECT_NE(std::string(json).find("\"collections_limit\":10"), std::string::npos);
    EXPECT_NE(std::string(json).find("\"disk_limit_bytes\":1048576"), std::string::npos);
}

/* ======================================================================== */
/* QuotaCheck                                                               */
/* ======================================================================== */

TEST_F(NamespaceTest, QuotaCheckPass) {
    mmdb_namespace_t* ns = nullptr;
    const char* quota = R"({"vectors_max":100})";
    ASSERT_EQ(mmdb_namespace_create(db_, "quota_pass", quota, &ns), MMDB_OK);

    EXPECT_EQ(mmdb_namespace_check_quota(ns, 50), MMDB_OK);
    EXPECT_EQ(mmdb_namespace_check_quota(ns, 0), MMDB_OK);
}

TEST_F(NamespaceTest, QuotaCheckExceed) {
    mmdb_namespace_t* ns = nullptr;
    const char* quota = R"({"vectors_max":10})";
    ASSERT_EQ(mmdb_namespace_create(db_, "quota_exceed", quota, &ns), MMDB_OK);

    /* 模拟已使用 8 个 */
    ASSERT_EQ(mmdb_namespace_add_vectors(ns, 8), MMDB_OK);

    /* 再加 3 个 = 11 > 10，应返回 MMDB_ERR_FULL */
    EXPECT_EQ(mmdb_namespace_check_quota(ns, 3), MMDB_ERR_FULL);

    /* 再加 2 个 = 10，刚好不超过 */
    EXPECT_EQ(mmdb_namespace_check_quota(ns, 2), MMDB_OK);
}

TEST_F(NamespaceTest, QuotaNoLimit) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "quota_nolimit", nullptr, &ns), MMDB_OK);

    /* 无限制配额下，任意数量都应通过 */
    EXPECT_EQ(mmdb_namespace_check_quota(ns, 1000000000ULL), MMDB_OK);
}

/* ======================================================================== */
/* AddVectorsUsage                                                          */
/* ======================================================================== */

TEST_F(NamespaceTest, AddVectorsUsage) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "usage_add", nullptr, &ns), MMDB_OK);

    char json[256] = {0};

    /* 初始使用量为 0 */
    ASSERT_EQ(mmdb_namespace_usage(ns, json, sizeof(json)), MMDB_OK);
    EXPECT_NE(strstr(json, "\"vectors_used\":0"), nullptr);

    /* 增加 50 个向量 */
    ASSERT_EQ(mmdb_namespace_add_vectors(ns, 50), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_usage(ns, json, sizeof(json)), MMDB_OK);
    EXPECT_NE(strstr(json, "\"vectors_used\":50"), nullptr);

    /* 再增加 30 个 */
    ASSERT_EQ(mmdb_namespace_add_vectors(ns, 30), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_usage(ns, json, sizeof(json)), MMDB_OK);
    EXPECT_NE(strstr(json, "\"vectors_used\":80"), nullptr);
}

/* ======================================================================== */
/* Usage                                                                    */
/* ======================================================================== */

TEST_F(NamespaceTest, Usage) {
    mmdb_namespace_t* ns = nullptr;
    const char* quota = R"({"vectors_max":500,"collections_max":5,"disk_max_bytes":2097152})";
    ASSERT_EQ(mmdb_namespace_create(db_, "usage_test", quota, &ns), MMDB_OK);

    char buf[512] = {0};
    ASSERT_EQ(mmdb_namespace_usage(ns, buf, sizeof(buf)), MMDB_OK);

    EXPECT_NE(std::string(buf).find("\"vectors_used\":0"), std::string::npos);
    EXPECT_NE(std::string(buf).find("\"vectors_limit\":500"), std::string::npos);
    EXPECT_NE(std::string(buf).find("\"collections_limit\":5"), std::string::npos);
    EXPECT_NE(std::string(buf).find("\"disk_limit_bytes\":2097152"), std::string::npos);
}

/* ======================================================================== */
/* Drop                                                                     */
/* ======================================================================== */

TEST_F(NamespaceTest, Drop) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "drop_test", nullptr, &ns), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_drop(ns), MMDB_OK);

    mmdb_namespace_t* ns2 = nullptr;
    EXPECT_EQ(mmdb_namespace_get(db_, "drop_test", &ns2), MMDB_ERR_NOT_FOUND);
}

TEST_F(NamespaceTest, DropThenGet) {
    mmdb_namespace_t* ns1 = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "drop_get", nullptr, &ns1), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_drop(ns1), MMDB_OK);

    mmdb_namespace_t* ns2 = nullptr;
    EXPECT_EQ(mmdb_namespace_get(db_, "drop_get", &ns2), MMDB_ERR_NOT_FOUND);
}

/* ======================================================================== */
/* ReopenSameName                                                           */
/* ======================================================================== */

TEST_F(NamespaceTest, ReopenSameName) {
    mmdb_namespace_t* ns1 = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "reopen", nullptr, &ns1), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_drop(ns1), MMDB_OK);

    /* 删除后可重新创建同名 */
    mmdb_namespace_t* ns2 = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "reopen", nullptr, &ns2), MMDB_OK);
    ASSERT_NE(ns2, nullptr);
    EXPECT_STREQ(mmdb_namespace_name(ns2), "reopen");
}

/* ======================================================================== */
/* MultipleNamespaces                                                       */
/* ======================================================================== */

TEST_F(NamespaceTest, MultipleNamespaces) {
    mmdb_namespace_t* ns1 = nullptr;
    mmdb_namespace_t* ns2 = nullptr;
    mmdb_namespace_t* ns3 = nullptr;

    ASSERT_EQ(mmdb_namespace_create(db_, "multi_1", nullptr, &ns1), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_create(db_, "multi_2", nullptr, &ns2), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_create(db_, "multi_3", nullptr, &ns3), MMDB_OK);

    /* 各自独立 */
    ASSERT_NE(ns1, ns2);
    ASSERT_NE(ns2, ns3);
    ASSERT_NE(ns1, ns3);

    EXPECT_STREQ(mmdb_namespace_name(ns1), "multi_1");
    EXPECT_STREQ(mmdb_namespace_name(ns2), "multi_2");
    EXPECT_STREQ(mmdb_namespace_name(ns3), "multi_3");

    /* 各自操作互不影响 */
    ASSERT_EQ(mmdb_namespace_add_vectors(ns1, 10), MMDB_OK);
    ASSERT_EQ(mmdb_namespace_add_vectors(ns2, 20), MMDB_OK);

    char json1[256] = {0}, json2[256] = {0};
    mmdb_namespace_usage(ns1, json1, sizeof(json1));
    mmdb_namespace_usage(ns2, json2, sizeof(json2));
    EXPECT_NE(strstr(json1, "\"vectors_used\":10"), nullptr);
    EXPECT_NE(strstr(json2, "\"vectors_used\":20"), nullptr);
}

/* ======================================================================== */
/* InvalidParams                                                            */
/* ======================================================================== */

TEST_F(NamespaceTest, InvalidParams) {
    mmdb_namespace_t* ns = nullptr;

    EXPECT_EQ(mmdb_namespace_create(nullptr, "x", nullptr, &ns), MMDB_ERR_INVALID);
    EXPECT_EQ(mmdb_namespace_create(db_, nullptr, nullptr, &ns), MMDB_ERR_INVALID);
    EXPECT_EQ(mmdb_namespace_create(db_, "x", nullptr, nullptr), MMDB_ERR_INVALID);
    EXPECT_EQ(mmdb_namespace_create(db_, "", nullptr, &ns), MMDB_ERR_INVALID);

    EXPECT_EQ(mmdb_namespace_get(nullptr, "x", &ns), MMDB_ERR_INVALID);
    EXPECT_EQ(mmdb_namespace_get(db_, nullptr, &ns), MMDB_ERR_INVALID);
    EXPECT_EQ(mmdb_namespace_get(db_, "x", nullptr), MMDB_ERR_INVALID);

    EXPECT_EQ(mmdb_namespace_set_quota(nullptr, "{}"), MMDB_ERR_INVALID);
    EXPECT_EQ(mmdb_namespace_set_quota(ns, nullptr), MMDB_ERR_INVALID);

    char buf[64];
    EXPECT_EQ(mmdb_namespace_usage(nullptr, buf, sizeof(buf)), MMDB_ERR_INVALID);

    EXPECT_EQ(mmdb_namespace_check_quota(nullptr, 1), MMDB_ERR_INVALID);

    EXPECT_EQ(mmdb_namespace_add_vectors(nullptr, 1), MMDB_ERR_INVALID);

    EXPECT_EQ(mmdb_namespace_drop(nullptr), MMDB_ERR_INVALID);
}

/* ======================================================================== */
/* DoubleDrop                                                               */
/* ======================================================================== */

TEST_F(NamespaceTest, DoubleDrop) {
    mmdb_namespace_t* ns = nullptr;
    ASSERT_EQ(mmdb_namespace_create(db_, "double_drop", nullptr, &ns), MMDB_OK);

    ASSERT_EQ(mmdb_namespace_drop(ns), MMDB_OK);
    EXPECT_EQ(mmdb_namespace_drop(ns), MMDB_ERR_INVALID);
}
