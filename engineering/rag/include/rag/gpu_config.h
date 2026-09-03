// engineering/rag/include/rag/gpu_config.h
#pragma once

#include <string>
#include <cstdint>

namespace rag {

struct GPUInfo {
    int device_id = -1;
    std::string name;
    std::string compute_capability;
    uint64_t total_memory_mb = 0;
    uint64_t free_memory_mb = 0;
    bool supports_fp16 = false;
    bool supports_cuda_graph = false;
};

struct GPUConfig {
    bool enable = true;
    int device_id = 0;
    bool use_fp16 = true;
    int max_batch_size = 32;
    size_t max_memory_mb = 6144;  // 保留 2GB 给系统和 LLM
    bool allow_growth = true;     // 允许显存增长
};

class GPUManager {
public:
    static GPUManager& instance();

    // 检测 GPU
    bool detect();
    bool is_available() const { return available_; }

    // GPU 信息
    const GPUInfo& info() const { return info_; }

    // 配置
    void set_config(const GPUConfig& config);
    const GPUConfig& config() const { return config_; }

    // 显存管理
    bool allocate(size_t bytes);
    void release();
    size_t available_memory() const;

    // 预热
    void warmup();

    // 统计
    struct Stats {
        uint64_t total_allocations = 0;
        uint64_t failed_allocations = 0;
        uint64_t peak_memory_mb = 0;
    };
    Stats get_stats() const;

private:
    GPUManager() = default;
    ~GPUManager() = default;
    GPUManager(const GPUManager&) = delete;
    GPUManager& operator=(const GPUManager&) = delete;

    bool available_ = false;
    GPUInfo info_;
    GPUConfig config_;
    Stats stats_;
};

}  // namespace rag
