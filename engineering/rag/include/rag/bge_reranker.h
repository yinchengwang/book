/**
 * @file bge_reranker.h
 * @brief BGE Reranker ONNX 实现
 *
 * 使用 ONNX Runtime 部署 BAAI/bge-reranker-v2-m3 模型
 */
#pragma once

#include "rag/types.h"
#include "rag/gpu_config.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace rag {

// ========== BGE Reranker 配置 ==========

struct BGERerankerConfig {
    // 模型路径
    std::string model_path;

    // ONNX Runtime 配置
    std::string provider = "CPUExecutionProvider";  // CPU/CUDA/CLANG
    int num_threads = 4;

    // 推理配置
    int max_length = 512;
    int batch_size = 8;

    // 归一化
    bool normalize = true;
    bool apply_softmax = true;

    // 设备
    int device_id = 0;
};

// ========== 重排序结果 ==========

struct RerankResult {
    std::string chunk_id;      // Chunk ID
    std::string content;       // Chunk 内容
    float score = 0.0f;       // 重排分数
    std::string reason;        // 重排原因 (可选)
};

// ========== BGE Reranker ==========

/**
 * @brief BGE Reranker ONNX 实现
 *
 * 使用 ONNX Runtime 加载 bge-reranker-v2-m3 模型
 * 支持 CPU/CUDA/CLANG 执行
 */
class BGEReranker {
public:
    /**
     * @brief 构造函数
     * @param config 配置
     */
    explicit BGEReranker(const BGERerankerConfig& config);

    /**
     * @brief 析构函数
     */
    ~BGEReranker();

    // ========== 基础接口 ==========

    /**
     * @brief 初始化 reranker
     * @param model_path 模型路径
     * @return 是否成功
     */
    bool init(const std::string& model_path);

    /**
     * @brief 检查是否就绪
     */
    bool is_ready() const { return ort_session_ != nullptr; }

    /**
     * @brief 重排检索结果
     * @param query 查询文本
     * @param candidates 候选 chunks
     * @param top_n 返回前 N 个结果
     * @return 重排后的结果
     */
    std::vector<RerankResult> rerank(
        const std::string& query,
        const std::vector<Chunk>& candidates,
        int top_n);

    /**
     * @brief 批量重排
     * @param queries_chunks_pairs 查询-候选对列表
     * @param top_n 返回前 N 个结果
     * @return 每对的重排结果
     */
    std::vector<std::vector<RerankResult>> rerank_batch(
        const std::vector<std::pair<std::string, std::vector<Chunk>>>& queries_chunks_pairs,
        int top_n);

    /**
     * @brief 获取名称
     */
    std::string name() const { return "bge_reranker_onnx"; }

    /**
     * @brief 获取配置
     */
    const BGERerankerConfig& config() const { return config_; }

    /**
     * @brief 获取模型类型
     */
    std::string model_type() const { return "bge-reranker-v2-m3"; }

    // ========== 批量重排 (基于 RetrievalResult) ==========

    /**
     * @brief 批量重排 (使用 RetrievalResult)
     * @param query 查询文本
     * @param results 候选结果 (RetrievalResult 列表)
     * @param batch_size 批大小
     * @return 重排后的结果
     */
    std::vector<RetrievalResult> rerank_batch(
        const std::string& query,
        const std::vector<RetrievalResult>& results,
        int batch_size = 8);

    /**
     * @brief 设置 GPU 配置
     * @param config GPU 配置
     */
    void set_gpu_config(const GPUConfig& config);

    /**
     * @brief 设置 FP16 支持
     * @param enable 是否启用 FP16
     */
    void set_fp16(bool enable);

    /**
     * @brief 模型信息
     */
    struct ModelInfo {
        std::string model_name;
        int max_length;
        bool supports_fp16;
        size_t memory_usage_mb;
    };

    /**
     * @brief 获取模型信息
     */
    ModelInfo get_model_info() const;

    /**
     * @brief 预热模型
     * @param num_samples 预热样本数
     */
    void warmup(int num_samples = 10);

    // ========== 统计接口 ==========

    struct Stats {
        uint64_t total_calls = 0;
        uint64_t total_tokens = 0;
        double avg_latency_ms = 0.0;
    };

    /**
     * @brief 获取统计信息
     */
    const Stats& stats() const { return stats_; }

private:
    // 禁用拷贝
    BGEReranker(const BGEReranker&) = delete;
    BGEReranker& operator=(const BGEReranker&) = delete;

    // 初始化 ONNX Runtime
    bool init_onnx_runtime();

    // 初始化 tokenizer
    bool init_tokenizer(const std::string& model_path);

    // Tokenize
    std::vector<std::vector<int>> tokenize(
        const std::vector<std::string>& texts1,
        const std::vector<std::string>& texts2);

    // 运行推理
    std::vector<float> run_inference(
        const std::vector<int>& input_ids,
        const std::vector<int>& attention_mask);

    // 解析 tokenizer 词汇
    std::unordered_map<int, std::string> load_vocab(const std::string& model_path);

    BGERerankerConfig config_;

    // ONNX Runtime 相关
    void* ort_env_ = nullptr;
    void* ort_session_ = nullptr;
    void* ort_session_options_ = nullptr;

    // Tokenizer
    std::unordered_map<int, std::string> vocab_;
    std::string model_name_or_path_;
    int pad_token_id_ = 0;
    int unk_token_id_ = 1;
    int bos_token_id_ = 2;
    int eos_token_id_ = 3;

    GPUConfig gpu_config_;
    bool use_fp16_ = false;

    // 统计
    Stats stats_;
};

// ========== Factory ==========

/**
 * @brief 创建 BGE Reranker
 * @param config 配置
 * @return Reranker 实例
 */
std::shared_ptr<BGEReranker> create_bge_reranker(const BGERerankerConfig& config);

/**
 * @brief 创建带默认配置的 BGE Reranker
 * @param model_path 模型路径
 * @return Reranker 实例
 */
std::shared_ptr<BGEReranker> create_bge_reranker(const std::string& model_path);

}  // namespace rag