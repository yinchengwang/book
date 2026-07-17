# RAG 系统模型服务设计

## 概述

本文档定义 RAG 系统的模型服务设计，包括 Embedding 模型和 LLM 服务的集成方案。

---

## 1. Embedding 模型服务

### 1.1 模型选型

| 模型 | 维度 | 中文支持 | 速度 | 精度 | 推荐场景 |
|------|------|----------|------|------|----------|
| **bge-base-zh-v1.5** | 768 | ⭐⭐⭐⭐⭐ | 快 | 高 | 通用场景（推荐） |
| bge-large-zh-v1.5 | 1024 | ⭐⭐⭐⭐⭐ | 中 | 更高 | 高精度需求 |
| m3e-base | 768 | ⭐⭐⭐⭐ | 快 | 高 | 中文embedding |
| text2vec-large-chinese | 1024 | ⭐⭐⭐⭐⭐ | 慢 | 最高 | 最高精度需求 |

### 1.2 服务架构

```
┌─────────────────────────────────────────────────────────────┐
│                      EmbeddingService                       │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐   ┌─────────────┐   ┌─────────────┐       │
│  │  BatchQueue │──▶│ Tokenizer   │──▶│ Model推理   │       │
│  └─────────────┘   └─────────────┘   └─────────────┘       │
│                                              │              │
│                                              ▼              │
│                                        ┌─────────────┐       │
│                                        │ Normalizer  │       │
│                                        └─────────────┘       │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                        模型层                                │
│  ┌─────────────────┐      ┌─────────────────┐              │
│  │   CPU 推理      │      │   GPU 推理      │              │
│  │  (单线程/多线程) │      │  (CUDA/MPS)    │              │
│  └─────────────────┘      └─────────────────┘              │
└─────────────────────────────────────────────────────────────┘
```

### 1.3 服务接口

```cpp
// embed_service.h
#pragma once

#include <string>
#include <vector>
#include <memory>

namespace rag {

// 向量类型
using Vector = std::vector<float>;
using Embeddings = std::vector<Vector>;

struct EmbedRequest {
    std::string text;                    // 单文本
    // 或
    std::vector<std::string> texts;       // 批量文本
    bool normalize = true;                // 是否 L2 归一化
};

struct EmbedResponse {
    Vector embedding;                     // 单向量
    // 或
    Embeddings embeddings;                // 批量向量
    int tokens_used;                      // 使用的 token 数
};

class EmbeddingService {
public:
    // 初始化
    virtual ~EmbeddingService() = default;
    virtual bool initialize(const Config& config) = 0;

    // 编码单个文本
    virtual Vector encode(const std::string& text) = 0;

    // 编码批量文本
    virtual Embeddings encode_batch(const std::vector<std::string>& texts) = 0;

    // 异步编码
    virtual future<Vector> encode_async(const std::string& text) = 0;
    virtual future<Embeddings> encode_batch_async(
        const std::vector<std::string>& texts) = 0;

    // 模型信息
    virtual std::string model_name() const = 0;
    virtual int dimension() const = 0;
    virtual int max_sequence_length() const = 0;

    // 状态
    virtual bool is_loaded() const = 0;
    virtual void unload() = 0;
};

}  // namespace rag
```

### 1.4 批处理策略

```cpp
// 批处理配置
struct BatchConfig {
    size_t batch_size = 32;              // 每批数量
    size_t max_tokens = 4096;            // 每批最大 token 数
    size_t queue_size = 1000;            // 队列大小
    bool dynamic_batching = true;         // 动态 batching
    size_t dynamic_timeout_ms = 50;      // 动态批次超时
};

// 批处理逻辑
class BatchingScheduler {
public:
    // 提交编码请求
    void submit(std::vector<std::string> texts, Promise<Embeddings> promise);

    // 批量处理循环
    void process_loop();

private:
    // 按 token 数分组
    std::vector<std::vector<std::string>>
    group_by_tokens(const std::vector<std::string>& texts);

    // 等待凑批
    void wait_for_batch(size_t max_tokens);
};
```

### 1.5 模型加载

```cpp
// 模型加载策略
enum class LoadStrategy {
    EAGER,           // 启动时立即加载
    LAZY,            // 首次使用时加载
    BACKGROUND       // 后台异步加载
};

// 模型管理器
class ModelManager {
public:
    bool load_embedding_model(const std::string& path,
                             const LoadStrategy& strategy);

    bool load_llm_model(const std::string& path,
                        const LoadStrategy& strategy);

    // 卸载模型释放内存
    void unload(const std::string& model_name);

    // 获取模型大小
    size_t get_model_size(const std::string& model_name) const;

    // 内存监控
    struct MemoryInfo {
        size_t total_memory;
        size_t used_memory;
        size_t available_memory;
    };
    MemoryInfo get_memory_info() const;
};
```

---

## 2. LLM 服务设计

### 2.1 模型选型

| 模型 | 参数量 | 量化 | 内存占用 | 速度 | 推荐 |
|------|--------|------|----------|------|------|
| **Qwen2.5-7B** | 7B | Q4_K_M | ~5GB | 快 | ⭐推荐 |
| Qwen2.5-14B | 14B | Q4_K_M | ~10GB | 中 | 高精度 |
| Phi-3-mini | 3.8B | Q4 | ~2.5GB | 快 | 低显存 |
| DeepSeek-7B | 7B | Q4_K_M | ~5GB | 快 | 高性价比 |

### 2.2 llama.cpp 集成

```
┌─────────────────────────────────────────────────────────────┐
│                        LLMService                           │
├─────────────────────────────────────────────────────────────┤
│  ┌─────────────┐   ┌─────────────┐   ┌─────────────┐       │
│  │  Prompt     │──▶│  llama.cpp  │──▶│  Output     │       │
│  │  Template   │   │  Engine     │   │  Parser     │       │
│  └─────────────┘   └─────────────┘   └─────────────┘       │
│                                              │              │
│  ┌─────────────┐                             ▼              │
│  │  Cache      │◀──────────────────── Stream Output        │
│  └─────────────┘                                            │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                    llama.cpp 底层                            │
│  ┌─────────────────┐      ┌─────────────────┐              │
│  │   ggml_tensor   │      │   llama_context │              │
│  │   计算图        │      │   KV Cache      │              │
│  └─────────────────┘      └─────────────────┘              │
└─────────────────────────────────────────────────────────────┘
```

### 2.3 服务接口

```cpp
// llm_service.h
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <future>

namespace rag {

// 生成配置
struct GenerationConfig {
    float temperature = 0.7f;
    float top_p = 0.9f;
    int top_k = 50;
    float repeat_penalty = 1.1f;
    int max_tokens = 2048;
    std::vector<std::string> stop;

    // 流式配置
    size_t stream_chunk_size = 10;        // 流式输出块大小
};

// 生成结果
struct GenerationResult {
    std::string text;                    // 完整生成文本
    size_t tokens_generated;              // 生成的 token 数
    double inference_time_ms;             // 推理时间
    bool stopped_by_limit;                // 是否达到上限
    bool stopped_by_eos;                  // 是否遇到停止符
    std::string stop_reason;              // 停止原因
};

// 流式回调
using StreamCallback = std::function<bool(const std::string& chunk)>;

class LLMService {
public:
    virtual ~LLMService() = default;

    // 初始化
    virtual bool initialize(const Config& config) = 0;

    // 同步生成
    virtual GenerationResult generate(
        const std::string& prompt,
        const GenerationConfig& config = {}) = 0;

    // 流式生成
    virtual bool generate_stream(
        const std::string& prompt,
        StreamCallback callback,
        const GenerationConfig& config = {}) = 0;

    // 异步生成
    virtual std::future<GenerationResult> generate_async(
        const std::string& prompt,
        const GenerationConfig& config = {}) = 0;

    // 模型信息
    virtual std::string model_name() const = 0;
    virtual int context_size() const = 0;

    // 状态
    virtual bool is_loaded() const = 0;
    virtual void unload() = 0;
};

}  // namespace rag
```

### 2.4 llama.cpp 封装

```cpp
// llama_engine.h
#pragma once

extern "C" {
#include "llama.h"
}

#include <string>
#include <vector>
#include <memory>

namespace rag {

struct LlamaParams {
    // 模型路径
    std::string model_path;

    // 上下文配置
    int n_ctx = 8192;
    int n_gpu_layers = 35;               // GPU 层数
    int n_threads = 4;                   // CPU 线程数

    // 量化配置
    std::string quantize = "Q4_K_M";

    // 内存配置
    bool low_vram = false;
    bool use_mmap = true;
    bool use_mlock = false;

    // batch 配置
    int n_batch = 512;
};

class LlamaEngine {
public:
    LlamaEngine();
    ~LlamaEngine();

    // 加载模型
    bool load_model(const LlamaParams& params);

    // 推理
    struct Logits {
        std::vector<float> data;
        int n_tokens;
    };

    Logits forward(const std::vector<int>& tokens);

    // 生成单个 token
    int sample(
        const std::vector<int>& logits_ids,
        const std::vector<float>& logits_probs,
        float temperature,
        float top_p,
        float top_k,
        float repeat_penalty);

    // Tokenize / Detokenize
    std::vector<int> tokenize(const std::string& text);
    std::string detokenize(const std::vector<int>& tokens);

    // 属性
    int n_tokens() const;
    int n_ctx() const;
    bool has_vocab() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

}  // namespace rag
```

### 2.5 流式生成实现

```cpp
// 流式生成实现
bool LLMService::generate_stream(
    const std::string& prompt,
    StreamCallback callback,
    const GenerationConfig& config) {

    auto tokens = llama_.tokenize(prompt);
    std::vector<int> input_ids = tokens;

    int n_cur = 0;
    int n_decode = 0;

    while (n_cur < config.max_tokens) {
        // 前向传播
        auto logits = llama_.forward(input_ids);

        // 采样
        int new_token_id = llama_.sample(
            input_ids,
            logits.data,
            config.temperature,
            config.top_p,
            config.top_k,
            config.repeat_penalty
        );

        // 检查停止条件
        if (new_token_id == llama_eos_token()) {
            break;
        }

        for (const auto& stop_word : config.stop) {
            if (detokenize({new_token_id}) == stop_word) {
                return true;
            }
        }

        // 回调
        std::string chunk = llama_.detokenize({new_token_id});
        if (!callback(chunk)) {
            return false;  // 用户取消
        }

        // 更新状态
        input_ids.push_back(new_token_id);
        n_cur++;
        n_decode++;
    }

    return true;
}
```

---

## 3. 模型管理服务

### 3.1 模型注册表

```cpp
// model_registry.h
#pragma once

#include <string>
#include <map>
#include <vector>
#include <optional>

namespace rag {

struct ModelInfo {
    std::string name;
    std::string path;
    std::string type;                    // "embedding" | "llm"
    size_t size_bytes;
    std::string hash;
    bool is_loaded = false;
    bool is_downloading = false;
    double download_progress = 0.0;
};

class ModelRegistry {
public:
    // 注册模型
    void register_model(const ModelInfo& info);

    // 获取模型信息
    std::optional<ModelInfo> get_model(const std::string& name) const;
    std::vector<ModelInfo> list_models() const;

    // 模型验证
    bool verify_model(const std::string& name) const;

    // 下载进度
    void update_download_progress(const std::string& name, double progress);

    // 模型发现
    void discover_models(const std::string& models_dir);

private:
    std::map<std::string, ModelInfo> models_;
};

}  // namespace rag
```

### 3.2 模型下载器

```cpp
// model_downloader.h
#pragma once

#include <string>
#include <functional>
#include <future>

namespace rag {

struct DownloadProgress {
    std::string model_name;
    size_t downloaded_bytes;
    size_t total_bytes;
    double progress;                     // 0.0 - 1.0
    std::string speed;                   // "2.5 MB/s"
    std::string eta;                    // "2m 30s"
};

using ProgressCallback = std::function<void(const DownloadProgress&)>;

class ModelDownloader {
public:
    // 下载模型
    std::future<bool> download(
        const std::string& url,
        const std::string& save_path,
        ProgressCallback callback = nullptr);

    // 取消下载
    void cancel(const std::string& model_name);

    // 校验
    bool verify_checksum(const std::string& file_path,
                         const std::string& expected_hash);

private:
    // 支持的下载源
    static constexpr const char* HF_BASE = "https://huggingface.co";
    static constexpr const char* MISTRAL_BASE = "https://models.mistralai.com";

    std::string resolve_url(const std::string& model_name) const;
};

}  // rag
```

---

## 4. 模型部署方案

### 4.1 本地部署架构

```
┌─────────────────────────────────────────────────────────────┐
│                        内存布局                             │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌──────────────────┐     ┌──────────────────┐            │
│  │   Embedding      │     │      LLM          │            │
│  │   Model          │     │     Model         │            │
│  │   (~500MB)       │     │     (~5GB)       │            │
│  └──────────────────┘     └──────────────────┘            │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │                    系统内存 (~16GB)                    │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
│  ┌──────────────────────────────────────────────────────┐   │
│  │              GPU 显存 (~8GB, RTX 3060+)              │   │
│  │  ┌────────────┐     ┌──────────────────────────┐    │   │
│  │  │ KV Cache   │     │   部分 LLM 层 (~4GB)     │    │   │
│  │  └────────────┘     └──────────────────────────┘    │   │
│  └──────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 推荐硬件配置

| 场景 | CPU | 内存 | GPU | 存储 |
|------|-----|------|-----|------|
| 轻量级 | 4核 | 16GB | RTX 3060 (8GB) | 50GB |
| 标准 | 8核 | 32GB | RTX 4070 (12GB) | 100GB |
| 高性能 | 16核 | 64GB | RTX 4090 (24GB) | 200GB |

### 4.3 量化方案对比

| 量化级别 | 精度损失 | 内存占用 | 速度 | 推荐 |
|----------|----------|----------|------|------|
| FP16 | 无 | 100% | 基准 | 高精度 |
| Q8_0 | 极小 | 50% | 快 | 平衡 |
| Q6_K | 很小 | 40% | 快 | ⭐推荐 |
| Q5_K_M | 小 | 35% | 快 | 推荐 |
| Q4_K_M | 中等 | 30% | 很快 | ⭐最佳性价比 |
| Q4_0 | 中等 | 28% | 很快 | 低显存 |
| Q3_K | 较大 | 22% | 很快 | 极致压缩 |

---

## 5. 模型缓存管理

### 5.1 缓存策略

```cpp
// model_cache.h
#pragma once

#include <string>
#include <map>
#include <LRUCache.h>

namespace rag {

class ModelCache {
public:
    // 嵌入缓存
    void cache_embedding(const std::string& text_hash, const Vector& embedding);
    std::optional<Vector> get_embedding(const std::string& text_hash) const;

    // KV Cache (LLM)
    void cache_kv(const std::string& prompt_hash, const KVCache& cache);
    std::optional<KVCache> get_kv(const std::string& prompt_hash) const;

    // 清理
    void clear();
    void set_max_size(size_t max_size_bytes);

    // 统计
    size_t cache_size() const;
    size_t hit_count() const;
    double hit_rate() const;

private:
    LRUCache<std::string, Vector> embedding_cache_;
    LRUCache<std::string, KVCache> kv_cache_;
};

}  // namespace rag
```

### 5.2 缓存配置

```yaml
cache:
  embedding:
    enabled: true
    max_size: 10GB                    # 最大缓存大小
    ttl: 86400                        # TTL (秒)

  kv_cache:
    enabled: true
    max_size: 1GB                    # KV 缓存大小
    strategy: lru                    # lru, fifo

  disk_cache:
    enabled: true
    path: "./rag_data/cache"
    max_size: 50GB
    compression: lz4                  # lz4, zstd, none
```

---

## 6. 模型性能基准

### 6.1 Embedding 性能

| 模型 | 维度 | 吞吐量 (tokens/s) | 延迟 (ms/token) | 内存 |
|------|------|-------------------|-----------------|------|
| bge-base-zh-v1.5 (CPU) | 768 | 500 | 2 | 500MB |
| bge-base-zh-v1.5 (GPU) | 768 | 3000 | 0.3 | 600MB |
| m3e-base (CPU) | 768 | 600 | 1.7 | 400MB |

### 6.2 LLM 性能 (Qwen2.5-7B-Q4)

| 模式 | tokens/s | 首 token (ms) | 内存 |
|------|----------|---------------|------|
| 全 CPU | 15-20 | 200 | 5GB |
| 16GB GPU | 40-60 | 50 | 6GB |
| 24GB GPU | 80-100 | 30 | 7GB |

---

*文档版本：1.0.0*
*最后更新：2026-07-06*
