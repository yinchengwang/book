/**
 * @file llama_service.h
 * @brief LlamaService 类 - 基于 llama.cpp 的本地 LLM 服务
 */
#pragma once

#include "rag/llm_service.h"
#include "rag/config.h"
#include <string>
#include <memory>
#include <vector>

// 前向声明 llama.cpp 结构
struct llama_context;
struct llama_model;

namespace rag {

// ========== Llama 配置 ==========

/**
 * @brief LlamaService 特定配置
 */
struct LlamaConfig {
    int n_ctx = 4096;              // 上下文窗口大小
    int n_threads = 4;              // CPU 线程数
    int n_gpu_layers = 0;           // GPU 层数 (0 表示仅 CPU)
    float rope_freq_base = 0.0f;   // RoPE 频率基数 (0 使用模型默认值)
    float rope_freq_scale = 1.0f;  // RoPE 频率缩放
    int max_tokens = 1024;          // 最大生成长度
    float temperature = 0.7f;       // 温度参数
    float top_p = 0.9f;            // Top-p 采样
    int top_k = 40;                 // Top-k 采样
    float repeat_penalty = 1.1f;   // 重复惩罚
    int seed = -1;                  // 随机种子 (-1 表示随机)
    std::string stop_sequence;      // 停止序列
};

// ========== LlamaService 类 ==========

/**
 * @brief LlamaService - 基于 llama.cpp 的本地 LLM 推理服务
 *
 * 使用 pimpl 模式封装 llama.cpp 的 GGUF 模型加载和推理
 */
class LlamaService : public LLMService {
public:
    LlamaService();
    ~LlamaService() override;

    // ========== 生命周期 ==========

    /**
     * @brief 加载 GGUF 模型
     * @param model_path 模型文件路径 (.gguf)
     * @param config LLM 配置
     */
    void load(const std::string& model_path, const LLMConfig& config) override;

    /**
     * @brief 卸载模型
     */
    void unload() override;

    /**
     * @brief 检查模型是否已加载
     */
    bool is_loaded() const override;

    // ========== 生成 ==========

    /**
     * @brief 同步生成
     * @param prompt 输入提示词
     * @param options 生成选项
     * @return 生成结果
     */
    GenerateResult generate(const std::string& prompt,
                           const GenerateOptions& options = {}) override;

    /**
     * @brief 流式生成
     * @param prompt 输入提示词
     * @param options 生成选项
     * @param callback 流式回调
     */
    void generate_stream(const std::string& prompt,
                        const GenerateOptions& options,
                        StreamCallback callback) override;

    /**
     * @brief 批量生成
     */
    std::vector<GenerateResult> generate_batch(
        const std::vector<std::string>& prompts,
        const GenerateOptions& options = {}) override;

    // ========== 信息 ==========

    /**
     * @brief 获取上下文窗口大小
     */
    int context_window() const override;

    /**
     * @brief 获取模型类型
     */
    const std::string& model_type() const override;

    /**
     * @brief 获取模型路径
     */
    const std::string& model_path() const override;

    // ========== Llama 特定方法 ==========

    /**
     * @brief 获取 llama 特定配置
     */
    const LlamaConfig& llama_config() const { return llama_config_; }

    /**
     * @brief 设置 llama 特定配置
     */
    void set_llama_config(const LlamaConfig& config) { llama_config_ = config; }

private:
    // Pimpl - 隐藏 llama.cpp 实现细节
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rag
