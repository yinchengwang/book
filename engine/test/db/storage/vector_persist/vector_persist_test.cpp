/**
 * @file vector_persist_test.cpp
 * @brief 向量持久化层单元测试
 *
 * 测试向量页面和向量段的持久化功能。
 */
#include <gtest/gtest.h>
#include <db/storage/vector/vector_persist.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path) _mkdir(path)
#define rmdir(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

// 测试夹具基类
class VectorPersistTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时目录
        test_dir_ = "test_vector_data";
        mkdir(test_dir_.c_str());
    }

    void TearDown() override {
        // 清理临时目录
        vector_segment_drop(test_dir_.c_str(), 0);
        rmdir(test_dir_.c_str());
    }

    std::string test_dir_;
};

// ============================================================
// 向量页测试
// ============================================================

class VectorPageTest : public VectorPersistTest {
protected:
    void SetUp() override {
        VectorPersistTest::SetUp();
        page_ = vector_page_create(0, 8192);
    }

    void TearDown() override {
        if (page_) {
            vector_page_free(page_);
            page_ = nullptr;
        }
        VectorPersistTest::TearDown();
    }

    vector_page_t *page_ = nullptr;
};

TEST_F(VectorPageTest, CreateAndFree) {
    ASSERT_NE(nullptr, page_);
    EXPECT_EQ(0u, vector_page_header(page_)->page_id);
    EXPECT_EQ(0u, vector_page_header(page_)->num_vectors);
    EXPECT_GT(vector_page_free_space(page_), 0u);
}

TEST_F(VectorPageTest, AppendVector) {
    float vec[128];
    for (int i = 0; i < 128; i++) vec[i] = (float)i;

    // 追加向量
    uint16_t offset = vector_page_append(page_, vec, 128);
    EXPECT_GT(offset, 0u);
    EXPECT_EQ(1u, vector_page_header(page_)->num_vectors);
}

TEST_F(VectorPageTest, AppendAndGetVector) {
    float vec_in[128];
    float vec_out[128];

    // 初始化输入向量
    for (int i = 0; i < 128; i++) {
        vec_in[i] = (float)(i * 2);
    }

    // 追加向量
    uint16_t offset = vector_page_append(page_, vec_in, 128);
    ASSERT_GT(offset, 0u);

    // 读取向量
    vector_page_get(page_, offset, 128, vec_out);

    // 验证数据
    for (int i = 0; i < 128; i++) {
        EXPECT_FLOAT_EQ(vec_in[i], vec_out[i]);
    }
}

TEST_F(VectorPageTest, AppendMultipleVectors) {
    float vec[128];

    // 追加多个向量
    for (int v = 0; v < 10; v++) {
        for (int i = 0; i < 128; i++) {
            vec[i] = (float)(v * 1000 + i);
        }
        uint16_t offset = vector_page_append(page_, vec, 128);
        ASSERT_GT(offset, 0u);
    }

    EXPECT_EQ(10u, vector_page_header(page_)->num_vectors);
}

TEST_F(VectorPageTest, Checksum) {
    float vec[128];
    for (int i = 0; i < 128; i++) vec[i] = (float)i;

    // 追加向量
    vector_page_append(page_, vec, 128);

    // 设置校验和
    vector_page_set_checksum(page_);

    // 验证校验和
    EXPECT_TRUE(vector_page_verify_checksum(page_));
}

TEST_F(VectorPageTest, FromData) {
    // 创建并填充页面
    float vec[128];
    for (int i = 0; i < 128; i++) vec[i] = (float)i;
    vector_page_append(page_, vec, 128);

    // 设置校验和
    vector_page_set_checksum(page_);

    // 获取数据
    void *data = vector_page_data(page_);
    ASSERT_NE(nullptr, data);

    // 从数据创建新页面
    vector_page_t *page2 = vector_page_from_data(data, 8192);
    ASSERT_NE(nullptr, page2);

    // 验证
    EXPECT_EQ(vector_page_header(page_)->num_vectors,
              vector_page_header(page2)->num_vectors);

    vector_page_free(page2);
}

// ============================================================
// 向量段测试
// ============================================================

class VectorSegmentTest : public VectorPersistTest {
protected:
    void TearDown() override {
        if (seg_) {
            vector_segment_close(seg_);
            seg_ = nullptr;
        }
        VectorPersistTest::TearDown();
    }

    vector_segment_t *seg_ = nullptr;
};

TEST_F(VectorSegmentTest, CreateAndClose) {
    seg_ = vector_segment_create(test_dir_.c_str(), 128, 0);
    ASSERT_NE(nullptr, seg_);
    EXPECT_EQ(128, vector_segment_dims(seg_));
    EXPECT_EQ(0u, vector_segment_id(seg_));
    EXPECT_EQ(VECTOR_SEGMENT_OPEN, vector_segment_state(seg_));
}

TEST_F(VectorSegmentTest, AppendVector) {
    seg_ = vector_segment_create(test_dir_.c_str(), 128, 0);
    ASSERT_NE(nullptr, seg_);

    float vec[128];
    for (int i = 0; i < 128; i++) vec[i] = (float)i;

    int32_t id = vector_segment_append(seg_, vec);
    EXPECT_GE(id, 0);
    EXPECT_EQ(1, vector_segment_size(seg_));
}

TEST_F(VectorSegmentTest, AppendAndGetVector) {
    seg_ = vector_segment_create(test_dir_.c_str(), 128, 0);
    ASSERT_NE(nullptr, seg_);

    float vec_in[128];
    float vec_out[128];

    // 初始化输入向量
    for (int i = 0; i < 128; i++) {
        vec_in[i] = (float)(i * 3);
    }

    // 追加向量
    int32_t id = vector_segment_append(seg_, vec_in);
    ASSERT_GE(id, 0);

    // 读取向量
    int ret = vector_segment_get(seg_, id, vec_out);
    EXPECT_EQ(0, ret);

    // 验证数据
    for (int i = 0; i < 128; i++) {
        EXPECT_FLOAT_EQ(vec_in[i], vec_out[i]);
    }
}

TEST_F(VectorSegmentTest, AppendBatch) {
    seg_ = vector_segment_create(test_dir_.c_str(), 64, 0);
    ASSERT_NE(nullptr, seg_);

    // 创建批量向量 (100 个 64 维向量)
    const int N = 100;
    const int DIMS = 64;
    float vectors[N * DIMS];
    for (int i = 0; i < N * DIMS; i++) {
        vectors[i] = (float)i;
    }

    // 批量追加
    int32_t start_id = vector_segment_append_batch(seg_, vectors, N);
    EXPECT_GE(start_id, 0);
    EXPECT_EQ(N, vector_segment_size(seg_));

    // 验证几个向量
    float vec_out[DIMS];
    for (int v = 0; v < 5; v++) {
        vector_segment_get(seg_, start_id + v, vec_out);
        for (int i = 0; i < DIMS; i++) {
            EXPECT_FLOAT_EQ(vectors[v * DIMS + i], vec_out[i]);
        }
    }
}

TEST_F(VectorSegmentTest, DeleteVector) {
    seg_ = vector_segment_create(test_dir_.c_str(), 128, 0);
    ASSERT_NE(nullptr, seg_);

    float vec[128];
    for (int i = 0; i < 128; i++) vec[i] = (float)i;

    int32_t id = vector_segment_append(seg_, vec);
    ASSERT_GE(id, 0);

    // 删除向量
    int ret = vector_segment_delete(seg_, id);
    EXPECT_EQ(0, ret);
}

TEST_F(VectorSegmentTest, Flush) {
    seg_ = vector_segment_create(test_dir_.c_str(), 128, 0);
    ASSERT_NE(nullptr, seg_);

    float vec[128];
    for (int i = 0; i < 128; i++) vec[i] = (float)i;

    // 追加多个向量
    for (int v = 0; v < 10; v++) {
        vector_segment_append(seg_, vec);
    }

    // 刷新
    int ret = vector_segment_flush(seg_);
    EXPECT_EQ(0, ret);
}

TEST_F(VectorSegmentTest, ReopenSegment) {
    // 创建并填充
    {
        seg_ = vector_segment_create(test_dir_.c_str(), 128, 0);
        ASSERT_NE(nullptr, seg_);

        float vec[128];
        for (int i = 0; i < 128; i++) vec[i] = (float)i;

        for (int v = 0; v < 5; v++) {
            vector_segment_append(seg_, vec);
        }

        vector_segment_flush(seg_);
        vector_segment_close(seg_);
        seg_ = nullptr;
    }

    // 重新打开
    {
        seg_ = vector_segment_open(test_dir_.c_str(), 0);
        ASSERT_NE(nullptr, seg_);

        EXPECT_EQ(5, vector_segment_size(seg_));
        EXPECT_EQ(128, vector_segment_dims(seg_));
    }
}

// ============================================================
// WAL 序列化测试
// ============================================================

TEST(VectorWalTest, SerializeAndDeserialize) {
    const int DIMS = 128;
    float vec[DIMS];
    for (int i = 0; i < DIMS; i++) vec[i] = (float)i;

    // 序列化
    uint8_t buf[1024];
    size_t size = vector_wal_serialize(VECTOR_WAL_APPEND, 0, 42, DIMS, vec, buf, sizeof(buf));
    EXPECT_GT(size, 0u);

    // 反序列化
    vector_wal_op_t op;
    uint32_t seg_id;
    int32_t vec_id;
    int32_t dims;
    float vec_out[DIMS];

    int ret = vector_wal_deserialize(buf, size, &op, &seg_id, &vec_id, &dims, vec_out);
    EXPECT_EQ(0, ret);
    EXPECT_EQ(VECTOR_WAL_APPEND, op);
    EXPECT_EQ(0u, seg_id);
    EXPECT_EQ(42, vec_id);
    EXPECT_EQ(DIMS, dims);

    // 验证数据
    for (int i = 0; i < DIMS; i++) {
        EXPECT_FLOAT_EQ(vec[i], vec_out[i]);
    }
}

// ============================================================
// 主函数
// ============================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
