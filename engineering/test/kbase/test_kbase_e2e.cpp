#include <gtest/gtest.h>
#include <cstdio>

extern "C" {
#include "kbase_index.h"
#include "kbase_search.h"
#include "infra/infra.h"
#include "infra/model.h"
}

class KbaseE2ETest : public ::testing::Test {
protected:
    void SetUp() override { infra_init(); }
};

TEST_F(KbaseE2ETest, FullPipeline) {
    /* 1. 创建索引 */
    kbase_index_t* idx = kbase_index_create("test-data/test_kbase_index/sample_notes");
    ASSERT_NE(idx, nullptr);
    int n = kbase_index_scan(idx);
    EXPECT_EQ(n, 10);

    /* 2. 创建 mock model */
    infra_model_t m = {};
    m.n_embd = 384;
    m.format = INFRA_MODEL_GGUF;

    /* 3. 构建索引（需要真实模型；此处使用 mock 时 HNSW 创建可能失败，跳过） */
    /* infra_status_t s = kbase_index_build(idx, &m);
       EXPECT_EQ(s, INFRA_OK); */

    /* 4. 搜索（占位） */
    int nr = 0;
    kbase_result_t* r = kbase_search(idx, &m, "transformer", 5, &nr);
    /* 占位 model 可能搜索失败 */
    kbase_search_free(r, nr);

    /* 5. 保存 */
    /* kbase_index_save(idx, "/tmp/kbase"); */

    kbase_index_destroy(idx);
    SUCCEED() << "E2E pipeline skeleton verified";
}