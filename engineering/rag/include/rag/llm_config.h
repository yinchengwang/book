#pragma once

#include <string>
#include <memory>
#include <vector>
#include <optional>
#include <functional>

namespace rag {

// ========== LLM 提供商 ==========

enum class LLMProvider {
    OLLAMA,
    OPENAI,
    ANTHROPIC,
    LOCAL
};

// ========== LLM 配置 ==========

struct LLMConfig {
    // 提供商
    LLMProvider provider = LLMProvider::OLLAMA;

    // 连接
    std::string base_url = "http://localhost:11434";
    std::string api_key;

    // 模型
    std::string model = "qwen2.5:7b";
    std::string embedding_model;

    // 生成参数
    float temperature = 0.7f;
    int max_tokens = 1024;
    int context_window = 4096;
    float top_p = 0.9f;
    float repeat_penalty = 1.1f;

    // 高级
    bool stream = false;
    float frequency_penalty = 0.0f;
    float presence_penalty = 0.0f;
    std::vector<std::string> stop;
};

// ========== LLM 客户端 ==========

class LLMClient {
public:
    explicit LLMClient(const LLMConfig& config);

    // 生成
    std::string generate(const std::string& prompt);

    // 流式生成
    void generate_stream(
        const std::string& prompt,
        std::function<void(std::string)> callback);

    // Embedding
    std::vector<float> embed(const std::string& text);

    // 健康检查
    bool is_available() const;

    // 模型信息
    struct ModelInfo {
        std::string name;
        int context_length;
        bool supports_streaming;
    };
    std::optional<ModelInfo> get_model_info() const;

private:
    LLMConfig config_;

    // HTTP 请求
    std::string post_json(const std::string& endpoint, const std::string& body);
    std::string get_json(const std::string& endpoint);
};

// ========== Ollama 特定客户端 ==========

class OllamaClient : public LLMClient {
public:
    explicit OllamaClient(const std::string& base_url = "http://localhost:11434");

    // 模型列表
    std::vector<std::string> list_models();

    // 拉取模型
    bool pull_model(const std::string& model);

    // Embedding
    std::vector<float> embed(const std::string& text) override;

    // 生成
    std::string generate(const std::string& prompt,
                         const std::string& model,
                         float temperature = 0.7f,
                         int max_tokens = 1024) override;

private:
    std::string base_url_;
};

// ========== 工厂函数 ==========

std::unique_ptr<LLMClient> create_llm_client(const LLMConfig& config);
std::unique_ptr<OllamaClient> create_ollama_client(const std::string& base_url = "");

}  // namespace rag