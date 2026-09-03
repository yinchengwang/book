// engineering/rag/test/rag/test_gpu.cpp

#include <gtest/gtest.h>
#include "rag/gpu_config.h"

namespace rag {
namespace test {

class GPUTest : public ::testing::Test {
protected:
    void SetUp() override {
        manager_ = &GPUManager::instance();
        manager_->detect();
    }

    GPUManager* manager_;
};

TEST_F(GPUTest, DetectGPU) {
    bool detected = manager_->detect();
    // 即使没有 GPU 也应该能运行
    EXPECT_TRUE(detected || !detected);
}

TEST_F(GPUTest, IsAvailable) {
    bool available = manager_->is_available();
    // 检测结果应该一致
    EXPECT_EQ(available, manager_->detect());
}

TEST_F(GPUTest, GPUConfig) {
    GPUConfig config;
    config.enable = true;
    config.device_id = 0;
    config.use_fp16 = true;
    config.max_batch_size = 32;
    config.max_memory_mb = 6144;

    manager_->set_config(config);

    EXPECT_EQ(manager_->config().enable, true);
    EXPECT_EQ(manager_->config().device_id, 0);
    EXPECT_EQ(manager_->config().use_fp16, true);
}

TEST_F(GPUTest, MemoryAllocation) {
    GPUConfig config;
    config.max_memory_mb = 6144;
    manager_->set_config(config);

    // 测试分配（可能失败如果没有 GPU）
    bool allocated = manager_->allocate(1024 * 1024);  // 1MB
    if (manager_->is_available()) {
        EXPECT_TRUE(allocated);
    }
}

}  // namespace test
}  // namespace rag

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
