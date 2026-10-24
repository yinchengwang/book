#include <gtest/gtest.h>
#include <cstdio>

extern "C" {
#include "kbase_index.h"
#include "infra/infra.h"
}

class KbaseIndexTest : public ::testing::Test {
protected:
    void SetUp() override { infra_init(); }
};

TEST_F(KbaseIndexTest, CreateAndScan) {
    const char* dir = "test-data/test_kbase_index/sample_notes";
    kbase_index_t* idx = kbase_index_create(dir);
    ASSERT_NE(idx, nullptr);
    int n = kbase_index_scan(idx);
    EXPECT_EQ(n, 10);
    EXPECT_EQ(idx->num_notes, 10);
    /* 验证标题非空 */
    EXPECT_NE(idx->notes[0].title, nullptr);
    kbase_index_destroy(idx);
}

TEST_F(KbaseIndexTest, EmptyDir) {
    kbase_index_t* idx = kbase_index_create("/tmp/nonexistent_dir");
    int n = kbase_index_scan(idx);
    EXPECT_EQ(n, 0);
    kbase_index_destroy(idx);
}

TEST_F(KbaseIndexTest, SaveLoad) {
    const char* dir = "test-data/test_kbase_index/sample_notes";
    kbase_index_t* idx = kbase_index_create(dir);
    kbase_index_scan(idx);
    infra_status_t s = kbase_index_save(idx, "/tmp/test_kbase");
    EXPECT_EQ(s, INFRA_OK);
    kbase_index_destroy(idx);
}