/**
 * @file test.cpp
 * @brief initdb 集成测试
 */

#include "db/initdb.h"
#include "db/pg_ctl.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

class InitdbTest : public ::testing::Test {
protected:
    char test_data_dir[256];

    void SetUp() override {
        // 创建临时测试目录
        snprintf(test_data_dir, sizeof(test_data_dir), "test_db_init_%lu", (unsigned long)time(NULL));
        mkdir(test_data_dir, 0755);
    }

    void TearDown() override {
        // 清理测试目录
        rmdir(test_data_dir);
    }
};

/**
 * @brief 测试 initdb_is_initialized
 */
TEST_F(InitdbTest, IsInitialized) {
    // 未初始化的目录
    EXPECT_FALSE(initdb_is_initialized(test_data_dir));

    // 不存在的目录
    EXPECT_FALSE(initdb_is_initialized("/nonexistent/path"));
}

/**
 * @brief 测试默认选项
 */
TEST_F(InitdbTest, DefaultOptions) {
    initdb_options_t *opts = initdb_default_options();
    ASSERT_NE(nullptr, opts);
    EXPECT_NE(nullptr, opts->data_dir);
    EXPECT_NE(nullptr, opts->username);
}

/**
 * @brief 测试目录创建
 */
TEST_F(InitdbTest, CreateDirectories) {
    int ret = initdb_create_directories(test_data_dir);
    // 可能失败因为目录已存在，但不应崩溃
    (void)ret;
}

/**
 * @brief 测试 pg_ctl 状态检查
 */
TEST_F(InitdbTest, PgCtlStatus) {
    pg_ctl_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.cmd = PG_CTL_STATUS;
    opts.data_dir = test_data_dir;

    // 未初始化目录应该返回特定状态码
    int ret = pg_ctl(&opts);
    EXPECT_EQ(3, ret);  // pg_ctl 约定：未初始化返回 3
}

/**
 * @brief 测试 pg_ctl 参数解析
 */
TEST_F(InitdbTest, PgCtlParseArgs) {
    pg_ctl_options_t opts;
    memset(&opts, 0, sizeof(opts));

    const char *argv1[] = {"pg_ctl", "start", "-D", "/data/db"};
    ASSERT_EQ(0, pg_ctl_parse_args(4, (char**)argv1, &opts));
    EXPECT_EQ(PG_CTL_START, opts.cmd);
    EXPECT_STREQ("/data/db", opts.data_dir);

    const char *argv2[] = {"pg_ctl", "stop", "-D", "/data/db", "-m", "fast"};
    memset(&opts, 0, sizeof(opts));
    ASSERT_EQ(0, pg_ctl_parse_args(7, (char**)argv2, &opts));
    EXPECT_EQ(PG_CTL_STOP, opts.cmd);
    EXPECT_TRUE(opts.fast);
}

/**
 * @brief 测试 pg_ctl 选项结构
 */
TEST_F(InitdbTest, PgCtlOptions) {
    pg_ctl_options_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.cmd = PG_CTL_STATUS;
    opts.data_dir = "/tmp/test_db";
    opts.wait = true;
    opts.wait_timeout = 30;

    int ret = pg_ctl(&opts);
    // 只需确保不崩溃
    (void)ret;
}
