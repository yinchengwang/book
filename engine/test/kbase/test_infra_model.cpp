#include <gtest/gtest.h>
#include <cstdio>

extern "C" {
#include "infra/model.h"
#include "infra/tensor.h"
}

class InfraModelTest : public ::testing::Test {
protected:
    const char* test_path = "test-data/test_infra_model/sample.gguf";
    void SetUp() override {
        FILE* f = fopen(test_path, "rb");
        if (!f) GTEST_SKIP() << "测试 GGUF 文件不存在: " << test_path;
        else fclose(f);
    }
};

TEST_F(InfraModelTest, LoadGGUF) {
    infra_model_t* m = infra_model_load(test_path, INFRA_MODEL_GGUF);
    ASSERT_NE(m, nullptr);
    EXPECT_EQ(m->format, INFRA_MODEL_GGUF);
    EXPECT_EQ(m->n_embd, 384);
    infra_model_free(m);
}

TEST_F(InfraModelTest, GetTensor) {
    infra_model_t* m = infra_model_load(test_path, INFRA_MODEL_GGUF);
    ASSERT_NE(m, nullptr);
    infra_tensor_t* t = infra_model_get_tensor(m, "tensor");
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->dtype, INFRA_DTYPE_F32);
    EXPECT_EQ(t->ndim, 1);
    EXPECT_EQ(t->dims[0], 4);
    float* d = (float*)t->data;
    EXPECT_FLOAT_EQ(d[0], 1.0f); EXPECT_FLOAT_EQ(d[3], 4.0f);
    infra_tensor_free(t);
    infra_model_free(m);
}

TEST_F(InfraModelTest, GetWeightsTensor) {
    infra_model_t* m = infra_model_load(test_path, INFRA_MODEL_GGUF);
    ASSERT_NE(m, nullptr);
    infra_tensor_t* t = infra_model_get_tensor(m, "weights");
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->ndim, 2);
    EXPECT_EQ(t->dims[0], 3); EXPECT_EQ(t->dims[1], 3);
    infra_tensor_free(t);
    infra_model_free(m);
}

TEST_F(InfraModelTest, TensorNotFound) {
    infra_model_t* m = infra_model_load(test_path, INFRA_MODEL_GGUF);
    ASSERT_NE(m, nullptr);
    infra_tensor_t* t = infra_model_get_tensor(m, "nonexistent");
    EXPECT_EQ(t, nullptr);
    infra_model_free(m);
}