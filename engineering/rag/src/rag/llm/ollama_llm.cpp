/**
 * @file ollama_llm.cpp
 * @brief Ollama LLM 服务实现
 *
 * 通过 Ollama API 实现本地 LLM 推理
 */

#include "rag/ollama_llm.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <sstream>
#include <thread>

// HTTP 客户端
#ifdef RAG_USE_HTTPLIB
#include <httplib.h>
#else
#include <curl/curl.h>
#endif

// JSON 解析
#include <nlohmann/json.hpp>
using nlohmann::json;

namespace rag {

// ========== 工具函数 ==========

/**
 * @brief 简单的流式解析回调（用于 SSE）
 */
static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    auto* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<char*>(contents), realsize);
    return realsize;
}

// ========== OllamaLLMService 实现 ==========

OllamaLLMService::OllamaLLMService(const OllamaLLMConfig& config)
    : config_(config) {

    // 检查服务可用性
    if (check_service_available()) {
        loaded_ = true;
        detect_context_window();
        RAG_INFO("Ollama LLM service is ready (model: " + config_.model + ")");
    } else {
        RAG_WARN("Ollama LLM service not available");
    }
}

OllamaLLMService::OllamaLLMService(
    const std::string& base_url,
    const std::string& model)
    : OllamaLLMService(OllamaLLMConfig{base_url, model, 120, 3}) {}

OllamaLLMService::~OllamaLLMService() = default;

void OllamaLLMService::load(const std::string& model_path, const LLMConfig& config) {
    (void)model_path;  // Ollama 模型已通过构造函数指定
    (void)config;

    if (check_service_available()) {
        loaded_ = true;
        detect_context_window();
        RAG_INFO("Ollama LLM service loaded (model: " + config_.model + ")");
    } else {
        throw LLMException(errors::LLM_NOT_AVAILABLE,
            "Ollama service not available");
    }
}

void OllamaLLMService::unload() {
    loaded_ = false;
    RAG_INFO("Ollama LLM service unloaded");
}

GenerateResult OllamaLLMService::generate(const std::string& prompt,
                                           const GenerateOptions& options) {
    if (!loaded_) {
        throw LLMException(errors::LLM_NOT_AVAILABLE,
            "LLM model not loaded");
    }

    auto start = std::chrono::steady_clock::now();

    try {
        GenerateResult result = call_api(prompt, options, false);

        auto end = std::chrono::steady_clock::now();
        result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end - start).count();

        // 更新统计
        total_generations_++;
        total_tokens_ += result.tokens_generated;
        total_duration_ms_ += result.duration_ms;

        return result;
    } catch (const std::exception& e) {
        throw LLMException(errors::INTERNAL_ERROR,
            "Generation failed: " + std::string(e.what()));
    }
}

void OllamaLLMService::generate_stream(const std::string& prompt,
                                        const GenerateOptions& options,
                                        StreamCallback callback) {
    if (!loaded_) {
        throw LLMException(errors::LLM_NOT_AVAILABLE,
            "LLM model not loaded");
    }

    auto start = std::chrono::steady_clock::now();
    int tokens_generated = 0;

#ifdef RAG_USE_HTTPLIB
    // 使用 httplib
    httplib::Client cli(config_.base_url);
    cli.set_timeout(std::chrono::seconds(config_.timeout_seconds));

    json req_json;
    req_json["model"] = config_.model;
    req_json["prompt"] = prompt;
    req_json["stream"] = true;

    if (options.max_tokens > 0) {
        req_json["options"]["num_predict"] = options.max_tokens;
    }
    if (options.temperature > 0) {
        req_json["options"]["temperature"] = options.temperature;
    }
    if (options.top_p > 0) {
        req_json["options"]["top_p"] = options.top_p;
    }
    if (options.top_k > 0) {
        req_json["options"]["top_k"] = options.top_k;
    }
    if (!options.stop.empty()) {
        req_json["options"]["stop"] = options.stop;
    }

    std::string full_response;
    std::string buffer;

    auto res = cli.Post(
        "/api/generate",
        req_json.dump(),
        "application/json",
        [&](const char* data, size_t data_length, bool last_chunk) {
            buffer.append(data, data_length);

            // 解析 SSE 行
            size_t pos = 0;
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);

                if (line.rfind("data: ", 0) == 0) {
                    std::string json_str = line.substr(6);
                    if (json_str == "[DONE]") {
                        callback("", true);
                        return true;
                    }

                    try {
                        auto chunk = json::parse(json_str);
                        if (chunk.contains("response")) {
                            std::string token = chunk["response"].get<std::string>();
                            if (!token.empty()) {
                                callback(token, false);
                                full_response += token;
                                tokens_generated++;
                            }
                        }
                        if (chunk.value("done", false)) {
                            callback("", true);
                            return true;
                        }
                    } catch (const std::exception&) {
                        // 忽略解析错误
                    }
                }
            }

            (void)last_chunk;
            return true;
        });

    if (!res || res->status != 200) {
        throw LLMException(errors::INTERNAL_ERROR,
            "Stream request failed");
    }

#else
    // 使用 libcurl
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw LLMException(errors::INTERNAL_ERROR, "Failed to init CURL");
    }

    json req_json;
    req_json["model"] = config_.model;
    req_json["prompt"] = prompt;
    req_json["stream"] = true;

    if (options.max_tokens > 0) {
        req_json["options"]["num_predict"] = options.max_tokens;
    }
    if (options.temperature > 0) {
        req_json["options"]["temperature"] = options.temperature;
    }
    if (options.top_p > 0) {
        req_json["options"]["top_p"] = options.top_p;
    }
    if (options.top_k > 0) {
        req_json["options"]["top_k"] = options.top_k;
    }
    if (!options.stop.empty()) {
        req_json["options"]["stop"] = options.stop;
    }

    std::string req_body = req_json.dump();
    std::string response_buffer;
    std::string full_response;

    curl_easy_setopt(curl, CURLOPT_URL, (config_.base_url + "/api/generate").c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_.timeout_seconds);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // 流式响应回调
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        [](char* ptr, size_t size, size_t nmemb, void* userdata) {
            size_t realsize = size * nmemb;
            auto* buffer = static_cast<std::string*>(userdata);
            buffer->append(ptr, realsize);
            return realsize;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);

    CURLcode res_code = curl_easy_perform(curl);

    if (res_code != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        throw LLMException(errors::INTERNAL_ERROR,
            "CURL error: " + std::string(curl_easy_strerror(res_code)));
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        throw LLMException(errors::INTERNAL_ERROR,
            "HTTP error: " + std::to_string(http_code));
    }

    // 解析 SSE 响应
    std::istringstream stream(response_buffer);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("data: ", 0) == 0) {
            std::string json_str = line.substr(6);
            if (json_str == "[DONE]") {
                callback("", true);
                break;
            }

            try {
                auto chunk = json::parse(json_str);
                if (chunk.contains("response")) {
                    std::string token = chunk["response"].get<std::string>();
                    if (!token.empty()) {
                        callback(token, false);
                        full_response += token;
                        tokens_generated++;
                    }
                }
                if (chunk.value("done", false)) {
                    callback("", true);
                    break;
                }
            } catch (const std::exception&) {
                // 忽略解析错误
            }
        }
    }
#endif

    // 更新统计
    auto end = std::chrono::steady_clock::now();
    double duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start).count();
    total_generations_++;
    total_tokens_ += tokens_generated;
    total_duration_ms_ += duration_ms;
}

std::vector<GenerateResult> OllamaLLMService::generate_batch(
    const std::vector<std::string>& prompts,
    const GenerateOptions& options) {

    std::vector<GenerateResult> results;
    results.reserve(prompts.size());

    for (const auto& prompt : prompts) {
        results.push_back(generate(prompt, options));
    }

    return results;
}

std::vector<OllamaLLMService::ModelInfo> OllamaLLMService::list_models() {
    std::vector<ModelInfo> models;

#ifdef RAG_USE_HTTPLIB
    httplib::Client cli(config_.base_url);
    cli.set_timeout(std::chrono::seconds(5));

    auto res = cli.Get("/api/tags");
    if (!res || res->status != 200) {
        return models;
    }

    try {
        auto json = json::parse(res->body);
        if (json.contains("models")) {
            for (const auto& m : json["models"]) {
                ModelInfo info;
                info.name = m.value("name", "");
                info.model_file = m.value("model", "");
                info.size = m.value("size", 0);
                info.size_mb = info.size / (1024 * 1024);
                info.parameter_size = m.value("parameter_size", 0);
                info.quantization = m.value("quantization", "");
                models.push_back(info);
            }
        }
    } catch (const std::exception&) {
        // 忽略解析错误
    }

#else
    CURL* curl = curl_easy_init();
    if (!curl) {
        return models;
    }

    curl_easy_setopt(curl, CURLOPT_URL, (config_.base_url + "/api/tags").c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);

    std::string response_buffer;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    if (res == CURLE_OK && http_code == 200) {
        try {
            auto json_resp = json::parse(response_buffer);
            if (json_resp.contains("models")) {
                for (const auto& m : json_resp["models"]) {
                    ModelInfo info;
                    info.name = m.value("name", "");
                    info.model_file = m.value("model", "");
                    info.size = m.value("size", 0);
                    info.size_mb = info.size / (1024 * 1024);
                    info.parameter_size = m.value("parameter_size", 0);
                    info.quantization = m.value("quantization", "");
                    models.push_back(info);
                }
            }
        } catch (const std::exception&) {
            // 忽略解析错误
        }
    }
#endif

    return models;
}

bool OllamaLLMService::is_service_ready() const {
    return loaded_ && service_available_;
}

OllamaLLMService::Stats OllamaLLMService::get_stats() const {
    Stats stats;
    stats.total_generations = total_generations_;
    stats.total_tokens = total_tokens_;
    stats.avg_duration_ms = total_generations_ > 0
        ? total_duration_ms_ / total_generations_
        : 0.0;
    return stats;
}

void OllamaLLMService::reset_stats() {
    total_generations_ = 0;
    total_tokens_ = 0;
    total_duration_ms_ = 0.0;
}

// ========== 私有方法实现 ==========

GenerateResult OllamaLLMService::call_api(const std::string& prompt,
                                           const GenerateOptions& options,
                                           bool stream) {
    GenerateResult result;

#ifdef RAG_USE_HTTPLIB
    httplib::Client cli(config_.base_url);
    cli.set_timeout(std::chrono::seconds(config_.timeout_seconds));

    json req_json;
    req_json["model"] = config_.model;
    req_json["prompt"] = prompt;
    req_json["stream"] = stream;

    if (options.max_tokens > 0) {
        req_json["options"]["num_predict"] = options.max_tokens;
    }
    if (options.temperature > 0) {
        req_json["options"]["temperature"] = options.temperature;
    }
    if (options.top_p > 0) {
        req_json["options"]["top_p"] = options.top_p;
    }
    if (options.top_k > 0) {
        req_json["options"]["top_k"] = options.top_k;
    }
    if (options.repeat_penalty > 0) {
        req_json["options"]["repeat_penalty"] = options.repeat_penalty;
    }
    if (!options.stop.empty()) {
        req_json["options"]["stop"] = options.stop;
    }

    auto res = cli.Post("/api/generate", req_json.dump(), "application/json");

    if (!res || res->status != 200) {
        std::string err = res
            ? "HTTP " + std::to_string(res->status)
            : "Connection failed";
        throw LLMException(errors::INTERNAL_ERROR, "Ollama API error: " + err);
    }

    try {
        auto res_json = json::parse(res->body);

        result.text = res_json.value("response", "");
        result.tokens_generated = res_json.value("eval_count", 0);
        result.finished = res_json.value("done", false);
        result.finish_reason = result.finished ? "stop" : "incomplete";
    } catch (const std::exception& e) {
        throw LLMException(errors::PARSE_ERROR,
            "Failed to parse response: " + std::string(e.what()));
    }

#else
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw LLMException(errors::INTERNAL_ERROR, "Failed to init CURL");
    }

    json req_json;
    req_json["model"] = config_.model;
    req_json["prompt"] = prompt;
    req_json["stream"] = stream;

    if (options.max_tokens > 0) {
        req_json["options"]["num_predict"] = options.max_tokens;
    }
    if (options.temperature > 0) {
        req_json["options"]["temperature"] = options.temperature;
    }
    if (options.top_p > 0) {
        req_json["options"]["top_p"] = options.top_p;
    }
    if (options.top_k > 0) {
        req_json["options"]["top_k"] = options.top_k;
    }
    if (options.repeat_penalty > 0) {
        req_json["options"]["repeat_penalty"] = options.repeat_penalty;
    }
    if (!options.stop.empty()) {
        req_json["options"]["stop"] = options.stop;
    }

    std::string req_body = req_json.dump();
    std::string response_buffer;

    curl_easy_setopt(curl, CURLOPT_URL, (config_.base_url + "/api/generate").c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req_body.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, config_.timeout_seconds);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_buffer);

    CURLcode res_code = curl_easy_perform(curl);

    if (res_code != CURLE_OK) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        throw LLMException(errors::INTERNAL_ERROR,
            "CURL error: " + std::string(curl_easy_strerror(res_code)));
    }

    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (http_code != 200) {
        throw LLMException(errors::INTERNAL_ERROR,
            "HTTP error: " + std::to_string(http_code));
    }

    try {
        auto res_json = json::parse(response_buffer);
        result.text = res_json.value("response", "");
        result.tokens_generated = res_json.value("eval_count", 0);
        result.finished = res_json.value("done", false);
        result.finish_reason = result.finished ? "stop" : "incomplete";
    } catch (const std::exception& e) {
        throw LLMException(errors::PARSE_ERROR,
            "Failed to parse response: " + std::string(e.what()));
    }
#endif

    return result;
}

void OllamaLLMService::detect_context_window() {
    // 尝试通过模型列表获取上下文窗口
    auto models = list_models();
    for (const auto& m : models) {
        if (m.name.find(config_.model) != std::string::npos ||
            m.model_file.find(config_.model) != std::string::npos) {
            // 从模型名称推断上下文窗口
            // 常见模型上下文窗口
            if (config_.model.find("llama3.2") != std::string::npos) {
                context_window_ = 8192;
            } else if (config_.model.find("llama3") != std::string::npos) {
                context_window_ = 8192;
            } else if (config_.model.find("qwen2.5") != std::string::npos) {
                context_window_ = 8192;
            } else if (config_.model.find("deepseek") != std::string::npos) {
                context_window_ = 4096;
            } else {
                context_window_ = 4096;  // 默认
            }
            break;
        }
    }
}

bool OllamaLLMService::check_service_available() {
#ifdef RAG_USE_HTTPLIB
    httplib::Client cli(config_.base_url);
    cli.set_timeout(std::chrono::seconds(5));

    auto res = cli.Get("/api/tags");
    service_available_ = (res != nullptr && res->status == 200);

#else
    CURL* curl = curl_easy_init();
    if (!curl) {
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, (config_.base_url + "/api/tags").c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_easy_cleanup(curl);

    service_available_ = (res == CURLE_OK && http_code == 200);
#endif

    return service_available_;
}

// ========== 工厂函数 ==========

std::shared_ptr<LLMService> create_ollama_llm_service(
    const OllamaLLMConfig& config) {
    return std::make_shared<OllamaLLMService>(config);
}

std::shared_ptr<LLMService> create_ollama_llm_service(
    const std::string& base_url,
    const std::string& model) {
    return std::make_shared<OllamaLLMService>(base_url, model);
}

}  // namespace rag
