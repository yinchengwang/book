/**
 * @file llm_service.h
 * @brief LLM 服务 - 本地模型推理
 */
#pragma once

#include "rag/config.h"
#include "rag/types.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <future>

namespace rag {

// ========== 前向声明 ==========

class LLMService;

// ========== 生成选项 ==========

/**
 * @brief LLM 生成选项
 */
struct GenerateOptions {
    int max_tokens = 1024;          // 最大生成长度
    float temperature = 0.7f;       // 温度参数
    float top_p = 0.9f;             // Top-p 采样
    int top_k = 40;                 // Top-k 采样
    float repeat_penalty = 1.1f;    // 重复惩罚
    int seed = -1;                  // 随机种子 (-1 表示随机)
    std::string stop;               // 停止词
};

// ========== 生成结果 ==========

/**
 * @brief LLM 生成结果
 */
struct GenerateResult {
    std::string text;               // 生成的文本
    bool finished = true;           // 是否完成
    int tokens_generated = 0;       // 生成的 token 数
    int64_t duration_ms = 0;        // 生成耗时
    std::string finish_reason;      // 完成原因 (stop/eos/error)
};

// ========== 流式回调 ==========

/**
 * @brief 流式生成回调
 * @param token 新生成的 token
 * @param complete 是否完成
 */
using StreamCallback = std::function<void(const std::string& token, bool complete)>;

// ========== LLM 服务接口 ==========

/**
 * @brief LLM 服务接口
 */
class LLMService {
public:
    virtual ~LLMService() = default;

    // ========== 生命周期 ==========

    // 加载模型
    virtual void load(const std::string& model_path, const LLMConfig& config) = 0;

    // 卸载模型
    virtual void unload() = 0;

    // 检查是否已加载
    virtual bool is_loaded() const = 0;

    // ========== 生成 ==========

    // 同步生成
    virtual GenerateResult generate(const std::string& prompt,
                                    const GenerateOptions& options = {}) = 0;

    // 流式生成
    virtual void generate_stream(const std::string& prompt,
                                 const GenerateOptions& options,
                                 StreamCallback callback) = 0;

    // 批量生成
    virtual std::vector<GenerateResult> generate_batch(
        const std::vector<std::string>& prompts,
        const GenerateOptions& options = {}) = 0;

    // ========== 信息 ==========

    // 获取上下文窗口大小
    virtual int context_window() const = 0;

    // 获取模型类型
    virtual const std::string& model_type() const = 0;

    // 获取模型路径
    virtual const std::string& model_path() const = 0;
};

// ========== 简单 LLM 服务 ==========

/**
 * @brief 简单的 LLM 服务（模拟版本）
 *
 * 生产环境应使用真正的 llama.cpp 集成
 */
class SimpleLLMService : public LLMService {
public:
    SimpleLLMService();
    ~SimpleLLMService() override;

    void load(const std::string& model_path, const LLMConfig& config) override;
    void unload() override;
    bool is_loaded() const override { return loaded_; }

    GenerateResult generate(const std::string& prompt,
                           const GenerateOptions& options = {}) override;

    void generate_stream(const std::string& prompt,
                        const GenerateOptions& options,
                        StreamCallback callback) override;

    std::vector<GenerateResult> generate_batch(
        const std::vector<std::string>& prompts,
        const GenerateOptions& options = {}) override;

    int context_window() const override { return config_.n_ctx; }
    const std::string& model_type() const override { return config_.model_type; }
    const std::string& model_path() const override { return model_path_; }

private:
    // 模拟生成（基于关键词匹配）
    std::string simulate_generate(const std::string& prompt);

    LLMConfig config_;
    std::string model_path_;
    bool loaded_ = false;
};

// ========== 工厂函数 ==========

/**
 * @brief 创建 LLM 服务
 */
std::unique_ptr<LLMService> create_llm_service();

/**
 * @brief 创建指定类型的 LLM 服务
 */
std::unique_ptr<LLMService> create_llm_service(const std::string& type);

}  // namespace rag
