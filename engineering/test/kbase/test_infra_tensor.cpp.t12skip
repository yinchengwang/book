#include <gtest/gtest.h>
#include <cmath>
#include <cstring>

extern "C" {
#include "infra/tensor.h"
#include "infra/memory.h"
}

TEST(InfraTensor, CreateAndFree) {
    int64_t dims[] = {2, 3};
    infra_tensor_t* t = infra_tensor_create(dims, 2, INFRA_DTYPE_F32);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->ndim, 2);
    EXPECT_EQ(t->dims[0], 2);
    EXPECT_EQ(t->dims[1], 3);
    EXPECT_EQ(t->nelems, 6u);
    EXPECT_EQ(t->nbytes, 24u);
    EXPECT_NE(t->data, nullptr);
    infra_tensor_free(t);
}

TEST(InfraTensor, Reshape) {
    int64_t dims[] = {2, 3};
    infra_tensor_t* t = infra_tensor_create(dims, 2, INFRA_DTYPE_F32);
    ASSERT_NE(t, nullptr);

    int64_t new_dims[] = {6};
    infra_status_t s = infra_tensor_reshape(t, new_dims, 1);
    EXPECT_EQ(s, INFRA_OK);
    EXPECT_EQ(t->ndim, 1);
    EXPECT_EQ(t->dims[0], 6);
    EXPECT_EQ(t->nelems, 6u);

    /* 错误的 reshape（元素数不匹配） */
    int64_t bad_dims[] = {7};
    s = infra_tensor_reshape(t, bad_dims, 1);
    EXPECT_EQ(s, INFRA_ERR_PARAM);

    infra_tensor_free(t);
}

TEST(InfraTensor, Copy) {
    int64_t dims[] = {2, 2};
    infra_tensor_t* src = infra_tensor_create(dims, 2, INFRA_DTYPE_F32);
    infra_tensor_t* dst = infra_tensor_create(dims, 2, INFRA_DTYPE_F32);
    ASSERT_NE(src, nullptr);
    ASSERT_NE(dst, nullptr);

    float* sd = (float*)src->data;
    for (int i = 0; i < 4; i++) sd[i] = (float)(i + 1);

    infra_status_t s = infra_tensor_copy(dst, src);
    EXPECT_EQ(s, INFRA_OK);

    float* dd = (float*)dst->data;
    for (int i = 0; i < 4; i++) EXPECT_FLOAT_EQ(dd[i], (float)(i + 1));

    infra_tensor_free(src);
    infra_tensor_free(dst);
}

TEST(InfraTensor, IsContiguous) {
    int64_t dims[] = {2, 3, 4};
    infra_tensor_t* t = infra_tensor_create(dims, 3, INFRA_DTYPE_F32);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(infra_tensor_is_contiguous(t), 1);
    infra_tensor_free(t);
}

TEST(InfraTensor, SizeofDtype) {
    EXPECT_EQ(infra_sizeof_dtype(INFRA_DTYPE_F32), 4u);
    EXPECT_EQ(infra_sizeof_dtype(INFRA_DTYPE_F16), 2u);
    EXPECT_EQ(infra_sizeof_dtype(INFRA_DTYPE_I32),  4u);
    EXPECT_EQ(infra_sizeof_dtype(INFRA_DTYPE_I64),  8u);
}

TEST(InfraMemory, AllocAndReset) {
    infra_memory_pool_t* pool = infra_memory_pool_create(1024, 1024 * 1024);
    ASSERT_NE(pool, nullptr);

    void* p = infra_memory_pool_alloc(pool, 256);
    ASSERT_NE(p, nullptr);

    void* q = infra_memory_pool_calloc(pool, 10, sizeof(int));
    ASSERT_NE(q, nullptr);
    int* qi = (int*)q;
    for (int i = 0; i < 10; i++) EXPECT_EQ(qi[i], 0);

    infra_memory_pool_reset(pool);
    infra_memory_pool_destroy(pool);
}