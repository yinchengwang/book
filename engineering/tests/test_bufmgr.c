#include <gtest/gtest.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "db/buf.h"
#include "db/page.h"

class BufMgrTest : public ::testing::Test {
protected:
    void SetUp() override {
        snprintf(test_path, sizeof(test_path), "./test_bufmgr_%d.dat", getpid());
        FILE *f = fopen(test_path, "w");
        if (f) fclose(f);
    }
    void TearDown() override {
        remove(test_path);
    }
    char test_path[256];
};

TEST_F(BufMgrTest, InitShutdown) {
    EXPECT_EQ(buf_init(test_path), 0);
    buf_shutdown();
}

TEST_F(BufMgrTest, InitNull) {
    EXPECT_NE(buf_init(NULL), 0);
}

TEST_F(BufMgrTest, ReadWriteRoundtrip) {
    buf_init(test_path);

    /* 读取页面 0 */
    BufferDesc *buf = buf_read(1, 0, 1);
    ASSERT_NE(buf, nullptr);

    /* 获取页面数据并写入 */
    void *data = buf_get_data(buf);
    ASSERT_NE(data, nullptr);
    memcpy(data, "hello world", 11);

    /* 标记为脏 */
    buf_dirty(buf);

    /* unpin 后再读取同一页面应该能读到之前写入的数据 */
    buf_unpin(buf);

    /* 再次读取同一页面 */
    BufferDesc *buf2 = buf_read(1, 0, 0);
    ASSERT_NE(buf2, nullptr);

    /* 验证数据一致 */
    void *data2 = buf_get_data(buf2);
    EXPECT_EQ(memcmp(data2, "hello world", 11), 0);

    buf_unpin(buf2);
    buf_shutdown();
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}