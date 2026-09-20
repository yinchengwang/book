/**
 * @file minimax_llm.h
 * @brief MiniMax LLM 服务 - OpenAI 兼容 API
 *
 * 通过 HTTP 调用 MiniMax 的 chat/completions 接口
 */
#pragma once

#include "mmrag/llm_service.h"
#include <string>
#include <vector>
#include <functional>
#include <memory>

namespace mmrag {

/**
 * @brief MiniMax LLM 服务配置
 */
struct MiniMaxLLMConfig {
    std::string api_url = "https://api.minimax.chat/v1/chat/completions";
    std::string api_key;
    std::string model = "abab6.5s-chat";
    int timeout_seconds = 120;
    int max_retries = 3;
    int max_tokens = 4096;
    float temperature = 0.7f;
    float top_p = 0.9f;
};

/**
 * @brief MiniMax LLM 服务
 */
class MiniMaxLLMService : public LLMService {
public:
    explicit MiniMaxLLMService(const MiniMaxLLMConfig& config = {});
    ~MiniMaxLLMService() override;

    // 生命周期
    void load(const std::string& model_path, const LLMConfig& config) override;
    void unload() override;
    bool is_loaded() const override { return loaded_; }

    // 生成
    GenerateResult generate(const std::string& prompt,
                           const GenerateOptions& options = {}) override;

    void generate_stream(const std::string& prompt,
                        const GenerateOptions& options,
                        StreamCallback callback) override;

    std::vector<GenerateResult> generate_batch(
        const std::vector<std::string>& prompts,
        const GenerateOptions& options = {}) override;

    // 信息
    int context_window() const override { return 8192; }
    const std::string& model_type() const override { return type_; }
    const std::string& model_path() const override { return model_path_; }

private:
    std::string call_api(const std::string& system_prompt,
                        const std::string& user_content,
                        const GenerateOptions& options);

    MiniMaxLLMConfig config_;
    std::string type_ = "minimax";
    std::string model_path_;
    bool loaded_ = false;
};

} // namespace mmrag