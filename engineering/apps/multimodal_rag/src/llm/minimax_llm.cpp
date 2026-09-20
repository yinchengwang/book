/**
 * @file minimax_llm.cpp
 * @brief MiniMax LLM 服务实现 - OpenAI 兼容 API
 */

#include "mmrag/minimax_llm.h"
#include "mmrag/logger.h"

#ifdef RAG_USE_HTTPLIB
#include <httplib.h>
#endif

#include <nlohmann/json.hpp>
#include <sstream>
#include <chrono>

using json = nlohmann::json;

namespace mmrag {

MiniMaxLLMService::MiniMaxLLMService(const MiniMaxLLMConfig& config)
    : config_(config) {
    // API key: config > MINIMAX_API_KEY env
    if (config_.api_key.empty()) {
        const char* env = std::getenv("MINIMAX_API_KEY");
        if (env && *env) config_.api_key = env;
    }
}

MiniMaxLLMService::~MiniMaxLLMService() = default;

void MiniMaxLLMService::load(const std::string& model_path, const LLMConfig& /*config*/) {
    if (config_.api_key.empty()) {
        RAG_WARN("MiniMaxLLM: API key not set. Set MINIMAX_API_KEY env var.");
        loaded_ = false;
        return;
    }
    model_path_ = model_path;
    loaded_ = true;
    RAG_INFO("MiniMaxLLM ready: model=" + config_.model + ", url=" + config_.api_url);
}

void MiniMaxLLMService::unload() {
    loaded_ = false;
}

GenerateResult MiniMaxLLMService::generate(const std::string& prompt,
                                           const GenerateOptions& options) {
    GenerateResult result;
    if (!loaded_) {
        result.text = "[LLM not loaded]";
        result.finish_reason = "error";
        return result;
    }

    auto t0 = std::chrono::steady_clock::now();
    std::string content = call_api("", prompt, options);
    auto t1 = std::chrono::steady_clock::now();

    result.text = content;
    result.finished = true;
    result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (content.empty()) {
        result.finish_reason = "error";
        result.text = "[MiniMax API returned empty response]";
    } else {
        result.finish_reason = "stop";
    }

    return result;
}

void MiniMaxLLMService::generate_stream(const std::string& prompt,
                                       const GenerateOptions& options,
                                       StreamCallback callback) {
    // Streaming via SSE — simplified implementation
    if (!loaded_) {
        callback("[LLM not loaded]", true);
        return;
    }

#ifdef RAG_USE_HTTPLIB
    httplib::Client cli(config_.api_url);
    cli.set_max_timeout(std::chrono::seconds(config_.timeout_seconds));
    cli.set_read_timeout(std::chrono::seconds(config_.timeout_seconds));

    json req_json = {
        {"model", config_.model},
        {"messages", json::array({
            {{"role", "user"}, {"content", prompt}}
        })},
        {"temperature", options.temperature > 0 ? options.temperature : config_.temperature},
        {"max_tokens", options.max_tokens > 0 ? options.max_tokens : config_.max_tokens},
        {"stream", true}
    };

    std::string body = req_json.dump();

    auto res = cli.Post("/v1/chat/completions",
        {{"Authorization", "Bearer " + config_.api_key},
         {"Content-Type", "application/json"}},
        body, "application/json");

    if (res && res->status == 200) {
        // Parse SSE data
        std::istringstream stream(res->body);
        std::string line;
        std::string full_content;
        while (std::getline(stream, line)) {
            if (line.rfind("data: ", 0) == 0) {
                std::string data = line.substr(6);
                if (data == "[DONE]") break;
                try {
                    auto j = json::parse(data);
                    auto content = j["choices"][0]["delta"]["content"].get<std::string>();
                    if (!content.empty()) {
                        full_content += content;
                        callback(content, false);
                    }
                } catch (...) {}
            }
        }
        callback("", true);
    } else {
        std::string err = res ? "HTTP " + std::to_string(res->status) : "Connection failed";
        RAG_WARN("MiniMaxLLM stream error: " + err);
        callback("[API Error: " + err + "]", true);
    }
#else
    callback("[httplib not available]", true);
#endif
}

std::vector<GenerateResult> MiniMaxLLMService::generate_batch(
    const std::vector<std::string>& prompts,
    const GenerateOptions& options) {
    std::vector<GenerateResult> results;
    results.reserve(prompts.size());
    for (const auto& p : prompts) {
        results.push_back(generate(p, options));
    }
    return results;
}

std::string MiniMaxLLMService::call_api(const std::string& system_prompt,
                                        const std::string& user_content,
                                        const GenerateOptions& options) {
#ifdef RAG_USE_HTTPLIB
    httplib::Client cli(config_.api_url);
    cli.set_max_timeout(std::chrono::seconds(config_.timeout_seconds));

    // Build messages array
    json messages = json::array();
    if (!system_prompt.empty()) {
        messages.push_back({{"role", "system"}, {"content", system_prompt}});
    }
    messages.push_back({{"role", "user"}, {"content", user_content}});

    json req_json = {
        {"model", config_.model},
        {"messages", messages},
        {"temperature", options.temperature > 0 ? options.temperature : config_.temperature},
        {"max_tokens", options.max_tokens > 0 ? options.max_tokens : config_.max_tokens},
        {"top_p", options.top_p > 0 ? options.top_p : config_.top_p}
    };

    std::string body = req_json.dump();

    auto res = cli.Post("/v1/chat/completions",
        {{"Authorization", "Bearer " + config_.api_key},
         {"Content-Type", "application/json"}},
        body, "application/json");

    if (!res) {
        RAG_WARN("MiniMaxLLM: connection failed");
        return "";
    }

    if (res->status != 200) {
        RAG_WARN("MiniMaxLLM: HTTP " + std::to_string(res->status) + " - " + res->body);
        return "";
    }

    try {
        auto j = json::parse(res->body);
        auto choices = j["choices"];
        if (!choices.empty()) {
            return choices[0]["message"]["content"].get<std::string>();
        }
    } catch (const std::exception& e) {
        RAG_WARN("MiniMaxLLM: parse error - " + std::string(e.what()));
    }

    return "";
#else
    (void)system_prompt; (void)user_content; (void)options;
    return "[httplib not available]";
#endif
}

} // namespace mmrag