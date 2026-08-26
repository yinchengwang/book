/**
 * @file bge_reranker.h
 * @brief BGE Reranker ONNX 实现
 *
 * 使用 ONNX Runtime 部署 BAAI/bge-reranker-v2-m3 模型
 */
#pragma once

#include "rag/reranker.h"
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>

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

// ========== BGE Reranker ==========

/**
 * @brief BGE Reranker ONNX 实现
 *
 * 使用 ONNX Runtime 加载 bge-reranker-v2-m3 模型
 * 支持 CPU/CUDA/CLANG 执行
 */
class BGEReranker : public Reranker {
public:
    /**
     * @brief 构造函数
     * @param config 配置
     */
    explicit BGEReranker(const BGERerankerConfig& config);

    /**
     * @brief 析构函数
     */
    ~BGEReranker() override;

    // ========== Reranker 接口 ==========

    /**
     * @brief 初始化 reranker
     * @param model_path 模型路径
     * @return 是否成功
     */
    bool init(const std::string& model_path) override;

    /**
     * @brief 检查是否就绪
     */
    bool is_ready() const override { return session_ != nullptr; }

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
        int top_n) override;

    /**
     * @brief 批量重排
     * @param queries_query_chunks_pairs 查询-候选对列表
     * @param top_n 返回前 N 个结果
     * @return 每对的重排结果
     */
    std::vector<std::vector<RerankResult>> rerank_batch(
        const std::vector<std::pair<std::string, std::vector<Chunk>>>& queries_chunks_pairs,
        int top_n) override;

    /**
     * @brief 获取名称
     */
    std::string name() const override { return "bge_reranker_onnx"; }

    /**
     * @brief 获取配置
     */
    const BGERerankerConfig& config() const { return config_; }

    /**
     * @brief 获取模型类型
     */
    std::string model_type() const override { return "bge-reranker-v2-m3"; }

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

    // 统计
    struct Stats {
        uint64_t total_calls = 0;
        uint64_t total_tokens = 0;
        double avg_latency_ms = 0.0;
    };
    Stats stats_;
};

// ========== Factory ==========

/**
 * @brief 创建 BGE Reranker
 * @param config 配置
 * @return Reranker 实例
 */
std::shared_ptr<Reranker> create_bge_reranker(const BGERerankerConfig& config);

/**
 * @brief 创建带默认配置的 BGE Reranker
 * @param model_path 模型路径
 * @return Reranker 实例
 */
std::shared_ptr<Reranker> create_bge_reranker(const std::string& model_path);

}  // namespace rag
