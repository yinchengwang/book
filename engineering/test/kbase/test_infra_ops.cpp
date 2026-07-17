#include <gtest/gtest.h>
#include <cmath>
#include <vector>

extern "C" {
#include "infra/op.h"
#include "infra/tensor.h"
}

class InfraOpsTest : public ::testing::Test {
protected:
    void SetUp() override { infra_op_register_all(); }
};

/* 参考实现：朴素 MatMul */
static void ref_matmul(const float* A, const float* B, float* C, int M, int K, int N) {
    for (int i = 0; i < M; i++) {
        for (int j = 0; j < N; j++) {
            float s = 0.0f;
            for (int k = 0; k < K; k++) s += A[i*K+k] * B[k*N+j];
            C[i*N+j] = s;
        }
    }
}

TEST_F(InfraOpsTest, MatmulCorrectness) {
    int64_t ad[] = {3, 4}, bd[] = {4, 5}, cd[] = {3, 5};
    auto* A = infra_tensor_create(ad, 2, INFRA_DTYPE_F32);
    auto* B = infra_tensor_create(bd, 2, INFRA_DTYPE_F32);
    auto* C = infra_tensor_create(cd, 2, INFRA_DTYPE_F32);

    /* 填充 1.0 ... */
    float* a = (float*)A->data; for (int i = 0; i < 12; i++) a[i] = (float)i + 1.0f;
    float* b = (float*)B->data; for (int i = 0; i < 20; i++) b[i] = 1.0f;

    infra_tensor_t* in[2] = {A, B};
    infra_tensor_t* out[1] = {C};
    ASSERT_EQ(infra_op_execute("matmul", in, 2, out, 1, nullptr), INFRA_OK);

    /* 验证每行和 = sum(A[i]) */
    float ref[15] = {0};
    ref_matmul((float*)A->data, (float*)B->data, ref, 3, 4, 5);
    for (int i = 0; i < 15; i++) EXPECT_NEAR(((float*)C->data)[i], ref[i], 1e-3);

    infra_tensor_free(A); infra_tensor_free(B); infra_tensor_free(C);
}

TEST_F(InfraOpsTest, AddBroadcast) {
    int64_t ad[] = {2, 3}, bd[] = {3}, cd[] = {2, 3};
    auto* A = infra_tensor_create(ad, 2, INFRA_DTYPE_F32);
    auto* B = infra_tensor_create(bd, 1, INFRA_DTYPE_F32);
    auto* C = infra_tensor_create(cd, 2, INFRA_DTYPE_F32);
    float* a = (float*)A->data; for (int i = 0; i < 6; i++) a[i] = (float)i;
    float* b = (float*)B->data; b[0]=1; b[1]=2; b[2]=3;
    infra_tensor_t* in[2] = {A, B};
    infra_tensor_t* out[1] = {C};
    ASSERT_EQ(infra_op_execute("add", in, 2, out, 1, nullptr), INFRA_OK);
    /* row 0: 0+1,1+2,2+3 = 1,3,5; row 1: 3+1,4+2,5+3 = 4,6,8 */
    float* c = (float*)C->data;
    EXPECT_FLOAT_EQ(c[0], 1.0f); EXPECT_FLOAT_EQ(c[1], 3.0f); EXPECT_FLOAT_EQ(c[2], 5.0f);
    EXPECT_FLOAT_EQ(c[3], 4.0f); EXPECT_FLOAT_EQ(c[4], 6.0f); EXPECT_FLOAT_EQ(c[5], 8.0f);
    infra_tensor_free(A); infra_tensor_free(B); infra_tensor_free(C);
}

TEST_F(InfraOpsTest, ReLU) {
    int64_t d[] = {4};
    auto* A = infra_tensor_create(d, 1, INFRA_DTYPE_F32);
    auto* C = infra_tensor_create(d, 1, INFRA_DTYPE_F32);
    float* a = (float*)A->data; a[0]=-1; a[1]=0; a[2]=1; a[3]=2;
    infra_tensor_t* in[1] = {A};
    infra_tensor_t* out[1] = {C};
    ASSERT_EQ(infra_op_execute("relu", in, 1, out, 1, nullptr), INFRA_OK);
    float* c = (float*)C->data;
    EXPECT_FLOAT_EQ(c[0], 0.0f); EXPECT_FLOAT_EQ(c[1], 0.0f);
    EXPECT_FLOAT_EQ(c[2], 1.0f); EXPECT_FLOAT_EQ(c[3], 2.0f);
    infra_tensor_free(A); infra_tensor_free(C);
}

TEST_F(InfraOpsTest, GELU) {
    /* 与 PyTorch nn.GELU(approximate='tanh') 对比 */
    int64_t d[] = {3};
    auto* A = infra_tensor_create(d, 1, INFRA_DTYPE_F32);
    auto* C = infra_tensor_create(d, 1, INFRA_DTYPE_F32);
    float* a = (float*)A->data; a[0]=0.0f; a[1]=1.0f; a[2]=-1.0f;
    infra_tensor_t* in[1] = {A};
    infra_tensor_t* out[1] = {C};
    ASSERT_EQ(infra_op_execute("gelu", in, 1, out, 1, nullptr), INFRA_OK);
    /* PyTorch 参考值 */
    EXPECT_NEAR(((float*)C->data)[0], 0.0f, 1e-5);
    EXPECT_NEAR(((float*)C->data)[1], 0.8411920f, 1e-4);
    EXPECT_NEAR(((float*)C->data)[2], -0.1588080f, 1e-4);
    infra_tensor_free(A); infra_tensor_free(C);
}

TEST_F(InfraOpsTest, Softmax) {
    int64_t d[] = {3};
    auto* A = infra_tensor_create(d, 1, INFRA_DTYPE_F32);
    auto* C = infra_tensor_create(d, 1, INFRA_DTYPE_F32);
    float* a = (float*)A->data; a[0]=1.0f; a[1]=2.0f; a[2]=3.0f;
    infra_tensor_t* in[1] = {A};
    infra_tensor_t* out[1] = {C};
    ASSERT_EQ(infra_op_execute("softmax", in, 1, out, 1, nullptr), INFRA_OK);
    float* c = (float*)C->data;
    float sum = c[0]+c[1]+c[2];
    EXPECT_NEAR(sum, 1.0f, 1e-5);
    /* 单调递增 */
    EXPECT_LT(c[0], c[1]); EXPECT_LT(c[1], c[2]);
    infra_tensor_free(A); infra_tensor_free(C);
}

TEST_F(InfraOpsTest, LayerNorm) {
    int64_t d[] = {4};
    auto* A = infra_tensor_create(d, 1, INFRA_DTYPE_F32);
    auto* C = infra_tensor_create(d, 1, INFRA_DTYPE_F32);
    float* a = (float*)A->data; a[0]=1.0f; a[1]=2.0f; a[2]=3.0f; a[3]=4.0f;
    infra_tensor_t* in[1] = {A};
    infra_tensor_t* out[1] = {C};
    op_norm_params_t p = {1e-5f, 0, 0};
    ASSERT_EQ(infra_op_execute("layernorm", in, 1, out, 1, &p), INFRA_OK);
    /* 均值 ≈ 0, 方差 ≈ 1 */
    float* c = (float*)C->data;
    float mean = (c[0]+c[1]+c[2]+c[3])/4.0f;
    EXPECT_NEAR(mean, 0.0f, 1e-4);
    infra_tensor_free(A); infra_tensor_free(C);
}

TEST_F(InfraOpsTest, Transpose) {
    int64_t d[] = {2, 3};
    int64_t od[] = {3, 2};
    auto* A2 = infra_tensor_create(d, 2, INFRA_DTYPE_F32);
    auto* C2 = infra_tensor_create(od, 2, INFRA_DTYPE_F32);
    float* a = (float*)A2->data; int v=1;
    for (int i = 0; i < 2; i++) for (int j = 0; j < 3; j++) a[i*3+j] = (float)v++;
    infra_tensor_t* in[1] = {A2};
    infra_tensor_t* out[1] = {C2};
    op_transpose_params_t p = {0, 1};
    ASSERT_EQ(infra_op_execute("transpose", in, 1, out, 1, &p), INFRA_OK);
    /* C2[0,0]=A2[0,0]=1, C2[0,1]=A2[1,0]=4 */
    float* c = (float*)C2->data;
    EXPECT_FLOAT_EQ(c[0], 1.0f); EXPECT_FLOAT_EQ(c[1], 4.0f);
    EXPECT_FLOAT_EQ(c[2], 2.0f); EXPECT_FLOAT_EQ(c[3], 5.0f);
    EXPECT_FLOAT_EQ(c[4], 3.0f); EXPECT_FLOAT_EQ(c[5], 6.0f);
    infra_tensor_free(A2); infra_tensor_free(C2);
}

TEST_F(InfraOpsTest, Reshape) {
    int64_t d[] = {2, 3};
    auto* A = infra_tensor_create(d, 2, INFRA_DTYPE_F32);
    int64_t nd[] = {6};
    auto* C = infra_tensor_create(nd, 1, INFRA_DTYPE_F32);
    float* a = (float*)A->data; for (int i = 0; i < 6; i++) a[i] = (float)(i + 1);
    infra_tensor_t* in[1] = {A};
    infra_tensor_t* out[1] = {C};
    ASSERT_EQ(infra_op_execute("reshape", in, 1, out, 1, nullptr), INFRA_OK);
    float* c = (float*)C->data;
    for (int i = 0; i < 6; i++) EXPECT_FLOAT_EQ(c[i], (float)(i + 1));
    infra_tensor_free(A); infra_tensor_free(C);
}

TEST_F(InfraOpsTest, Gather) {
    int64_t weight_dims[] = {3, 4};
    int64_t indices_dims[] = {2};
    int64_t output_dims[] = {2, 4};
    auto* weight = infra_tensor_create(weight_dims, 2, INFRA_DTYPE_F32);
    auto* indices = infra_tensor_create(indices_dims, 1, INFRA_DTYPE_I32);
    auto* output = infra_tensor_create(output_dims, 2, INFRA_DTYPE_F32);
    ASSERT_NE(weight, nullptr);
    ASSERT_NE(indices, nullptr);
    ASSERT_NE(output, nullptr);

    float* weight_data = (float*)weight->data;
    for (int i = 0; i < 12; i++) weight_data[i] = (float)(i + 1);
    int32_t* indices_data = (int32_t*)indices->data;
    indices_data[0] = 0;
    indices_data[1] = 2;

    infra_tensor_t* inputs[2] = {weight, indices};
    infra_tensor_t* outputs[1] = {output};
    ASSERT_EQ(infra_op_execute("gather", inputs, 2, outputs, 1, nullptr), INFRA_OK);

    const float expected[] = {1.0f, 2.0f, 3.0f, 4.0f, 9.0f, 10.0f, 11.0f, 12.0f};
    const float* output_data = (const float*)output->data;
    for (int i = 0; i < 8; i++) EXPECT_FLOAT_EQ(output_data[i], expected[i]);

    infra_tensor_free(weight);
    infra_tensor_free(indices);
    infra_tensor_free(output);
}

TEST_F(InfraOpsTest, L2Norm) {
    int64_t dims[] = {4};
    auto* input = infra_tensor_create(dims, 1, INFRA_DTYPE_F32);
    auto* output = infra_tensor_create(dims, 1, INFRA_DTYPE_F32);
    ASSERT_NE(input, nullptr);
    ASSERT_NE(output, nullptr);

    float* input_data = (float*)input->data;
    input_data[0] = 3.0f;
    input_data[1] = 4.0f;
    input_data[2] = 6.0f;
    input_data[3] = 8.0f;

    infra_tensor_t* inputs[1] = {input};
    infra_tensor_t* outputs[1] = {output};
    ASSERT_EQ(infra_op_execute("l2norm", inputs, 1, outputs, 1, nullptr), INFRA_OK);

    const float expected[] = {0.26832816f, 0.35777088f, 0.53665632f, 0.71554175f};
    const float* output_data = (const float*)output->data;
    for (int i = 0; i < 4; i++) EXPECT_NEAR(output_data[i], expected[i], 1e-5f);

    infra_tensor_free(input);
    infra_tensor_free(output);
}

TEST_F(InfraOpsTest, Attention) {
    int64_t dims[] = {2, 2};
    auto* query = infra_tensor_create(dims, 2, INFRA_DTYPE_F32);
    auto* key = infra_tensor_create(dims, 2, INFRA_DTYPE_F32);
    auto* value = infra_tensor_create(dims, 2, INFRA_DTYPE_F32);
    auto* output = infra_tensor_create(dims, 2, INFRA_DTYPE_F32);
    ASSERT_NE(query, nullptr);
    ASSERT_NE(key, nullptr);
    ASSERT_NE(value, nullptr);
    ASSERT_NE(output, nullptr);

    const float identity[] = {1.0f, 0.0f, 0.0f, 1.0f};
    for (int i = 0; i < 4; i++) {
        ((float*)query->data)[i] = identity[i];
        ((float*)key->data)[i] = identity[i];
        ((float*)value->data)[i] = identity[i];
    }

    infra_tensor_t* inputs[3] = {query, key, value};
    infra_tensor_t* outputs[1] = {output};
    op_attention_params_t params = {1, 1.0f};
    ASSERT_EQ(infra_op_execute("attention", inputs, 3, outputs, 1, &params), INFRA_OK);

    const float expected[] = {0.7310586f, 0.2689414f, 0.2689414f, 0.7310586f};
    const float* output_data = (const float*)output->data;
    EXPECT_EQ(output->ndim, 2);
    EXPECT_EQ(output->dims[0], 2);
    EXPECT_EQ(output->dims[1], 2);
    for (int i = 0; i < 4; i++) EXPECT_NEAR(output_data[i], expected[i], 1e-5f);

    infra_tensor_free(query);
    infra_tensor_free(key);
    infra_tensor_free(value);
    infra_tensor_free(output);
}
