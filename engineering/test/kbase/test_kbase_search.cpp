#include <gtest/gtest.h>

extern "C" {
#include "kbase_index.h"
#include "kbase_search.h"
#include "infra/infra.h"
#include "infra/model.h"
}

class KbaseSearchTest : public ::testing::Test {
protected:
    void SetUp() override {
        infra_init();
        /* 使用占位 model */
        mock_model.n_embd = 384;
        mock_model.format = INFRA_MODEL_GGUF;
    }
    infra_model_t mock_model = {};
};

TEST_F(KbaseSearchTest, SearchEmptyIndex) {
    kbase_index_t* idx = kbase_index_create("/tmp/empty");
    int n = 0;
    kbase_result_t* r = kbase_search(idx, &mock_model, "query", 10, &n);
    EXPECT_EQ(r, nullptr);
    EXPECT_EQ(n, 0);
    kbase_index_destroy(idx);
}

TEST_F(KbaseSearchTest, SearchWithEmptyQuery) {
    kbase_index_t* idx = kbase_index_create("test-data/test_kbase_index/sample_notes");
    kbase_index_scan(idx);
    int n = 0;
    /* 不构建索引（无 HNSW），应返回 0 */
    kbase_result_t* r = kbase_search(idx, &mock_model, "transformer", 10, &n);
    EXPECT_EQ(r, nullptr);
    EXPECT_EQ(n, 0);
    kbase_index_destroy(idx);
}

TEST_F(KbaseSearchTest, SearchFreeNullSafe) {
    kbase_search_free(nullptr, 0);
    SUCCEED();
}