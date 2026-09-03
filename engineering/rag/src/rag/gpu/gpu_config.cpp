// engineering/rag/src/rag/gpu/gpu_config.cpp

#include "rag/gpu_config.h"
#include "rag/logger.h"
#include <thread>
#include <mutex>

#if RAG_HAS_CUDA
#include <cuda_runtime.h>
#include <cuda.h>
#endif

namespace rag {

static std::unique_ptr<GPUManager> instance_;
static std::mutex instance_mutex_;

GPUManager& GPUManager::instance() {
    std::lock_guard<std::mutex> lock(instance_mutex_);
    if (!instance_) {
        instance_ = std::make_unique<GPUManager>();
    }
    return *instance_;
}

bool GPUManager::detect() {
#if RAG_HAS_CUDA
    int device_count = 0;
    cudaError_t err = cudaGetDeviceCount(&device_count);

    if (err != cudaSuccess || device_count == 0) {
        RAG_WARN("No CUDA devices found");
        available_ = false;
        return false;
    }

    // 获取第一个 GPU 信息
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);

    info_.device_id = 0;
    info_.name = prop.name;
    info_.compute_capability =
        std::to_string(prop.major) + "." + std::to_string(prop.minor);
    info_.total_memory_mb = prop.totalGlobalMem / (1024 * 1024);

    // 检查 FP16 支持
    info_.supports_fp16 = (prop.major >= 7);
    info_.supports_cuda_graph = (prop.major >= 7);

    size_t free_mem, total_mem;
    cudaMemGetInfo(&free_mem, &total_mem);
    info_.free_memory_mb = free_mem / (1024 * 1024);

    RAG_INFO("GPU detected: " + info_.name);
    RAG_INFO("  Compute: " + info_.compute_capability);
    RAG_INFO("  Memory: " + std::to_string(info_.total_memory_mb) + " MB");

    available_ = true;
    return true;
#else
    RAG_WARN("CUDA not compiled in, GPU acceleration disabled");
    available_ = false;
    return false;
#endif
}

void GPUManager::set_config(const GPUConfig& config) {
    config_ = config;
}

size_t GPUManager::available_memory() const {
#if RAG_HAS_CUDA
    size_t free_mem, total_mem;
    cudaMemGetInfo(&free_mem, &total_mem);
    return free_mem;
#else
    return 0;
#endif
}

bool GPUManager::allocate(size_t bytes) {
#if RAG_HAS_CUDA
    size_t available = available_memory();
    if (bytes > available) {
        stats_.failed_allocations++;
        return false;
    }
    stats_.total_allocations++;
    if (bytes / (1024 * 1024) > stats_.peak_memory_mb) {
        stats_.peak_memory_mb = bytes / (1024 * 1024);
    }
    return true;
#else
    return false;
#endif
}

GPUManager::Stats GPUManager::get_stats() const {
    return stats_;
}

}  // namespace rag
