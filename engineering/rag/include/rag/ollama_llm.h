/**
 * @file ollama_llm.h
 * @brief Ollama LLM 服务
 *
 * 通过 Ollama API 实现本地 LLM 推理，支持同步和流式生成
 */
#pragma once

#include "rag/llm_service.h"
#include <string>
#include <memory>

namespace rag {

/**
 * @brief Ollama LLM 服务配置
 */
struct OllamaLLMConfig {
    std::string base_url = "http://localhost:11434";  // Ollama 服务地址
    std::string model = "llama3.2";                   // LLM 模型
    int timeout_seconds = 120;                        // 请求超时（生成可能较长）
    int max_retries = 3;                              // 最大重试次数
};

/**
 * @brief Ollama LLM 服务
 *
 * 通过 Ollama API 调用本地 LLM 模型（如 llama3.2/qwen2.5）
 * 支持同步生成、流式生成、批量生成。
 *
 * 特性：
 * - 同步/流式生成
 * - 支持 temperature/top_p/max_tokens 等选项
 * - 自动检测模型上下文窗口
 */
class OllamaLLMService : public LLMService {
public:
    /**
     * @brief 构造函数
     * @param config Ollama 配置
     */
    explicit OllamaLLMService(const OllamaLLMConfig& config);

    /**
     * @brief 简略构造函数
     * @param base_url Ollama 服务地址
     * @param model 模型名称
     */
    OllamaLLMService(
        const std::string& base_url = "http://localhost:11434",
        const std::string& model = "llama3.2");

    ~OllamaLLMService() override;

    // 禁用拷贝
    OllamaLLMService(const OllamaLLMService&) = delete;
    OllamaLLMService& operator=(const OllamaLLMService&) = delete;

    // ========== LLMService 接口 ==========

    /**
     * @brief 加载模型
     * @note Ollama 模型在启动时已加载，此处检查服务可用性
     */
    void load(const std::string& model_path, const LLMConfig& config) override;

    /**
     * @brief 卸载模型
     */
    void unload() override;

    /**
     * @brief 检查模型是否已加载
     */
    bool is_loaded() const override { return loaded_; }

    /**
     * @brief 同步生成
     * @param prompt 提示词
     * @param options 生成选项
     * @return 生成结果
     */
    GenerateResult generate(const std::string& prompt,
                            const GenerateOptions& options = {}) override;

    /**
     * @brief 流式生成
     * @param prompt 提示词
     * @param options 生成选项
     * @param callback 流式回调
     */
    void generate_stream(const std::string& prompt,
                         const GenerateOptions& options,
                         StreamCallback callback) override;

    /**
     * @brief 批量生成
     * @param prompts 提示词列表
     * @param options 生成选项
     * @return 生成结果列表
     */
    std::vector<GenerateResult> generate_batch(
        const std::vector<std::string>& prompts,
        const GenerateOptions& options = {}) override;

    /**
     * @brief 获取上下文窗口大小
     */
    int context_window() const override { return context_window_; }

    /**
     * @brief 获取模型类型
     */
    const std::string& model_type() const override { return model_type_; }

    /**
     * @brief 获取模型路径/名称
     */
    const std::string& model_path() const override { return config_.model; }

    // ========== 扩展接口 ==========

    /**
     * @brief 获取配置
     */
    const OllamaLLMConfig& config() const { return config_; }

    /**
     * @brief 获取模型信息
     */
    struct ModelInfo {
        std::string name;
        std::string model_file;
        size_t size;           // 模型大小（字节）
        int64_t size_mb;       // 模型大小（MB）
        int64_t parameter_size;// 参数量
        std::string quantization;  // 量化类型
    };
    std::vector<ModelInfo> list_models();

    /**
     * @brief 检查服务是否就绪
     */
    bool is_service_ready() const;

    /**
     * @brief 获取统计信息
     */
    struct Stats {
        int64_t total_generations;
        int64_t total_tokens;
        double avg_duration_ms;
    };
    Stats get_stats() const;

    /**
     * @brief 重置统计
     */
    void reset_stats();

private:
    /**
     * @brief 调用 Ollama API 生成
     */
    GenerateResult call_api(const std::string& prompt,
                            const GenerateOptions& options,
                            bool stream);

    /**
     * @brief 检测上下文窗口
     */
    void detect_context_window();

    /**
     * @brief 检查服务可用性
     */
    bool check_service_available();

    // 配置
    OllamaLLMConfig config_;

    // 状态
    bool loaded_ = false;
    bool service_available_ = false;
    int context_window_ = 4096;           // 默认 4K 上下文
    std::string model_type_ = "llama";    // 模型类型

    // 统计
    mutable int64_t total_generations_ = 0;
    mutable int64_t total_tokens_ = 0;
    mutable double total_duration_ms_ = 0.0;
};

// ========== 创建函数 ==========

/**
 * @brief 创建 Ollama LLM 服务
 * @param config 配置
 * @return 服务智能指针
 */
std::shared_ptr<LLMService> create_ollama_llm_service(
    const OllamaLLMConfig& config);

/**
 * @brief 创建 Ollama LLM 服务（简略版本）
 */
std::shared_ptr<LLMService> create_ollama_llm_service(
    const std::string& base_url = "http://localhost:11434",
    const std::string& model = "llama3.2");

}  // namespace rag
