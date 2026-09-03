/**
 * @file llm_client.cpp
 * @brief LLM 客户端实现
 */

#include "rag/llm_config.h"
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>
#include <json/json.h>  // nlohmann/json

namespace rag {

// ========== 工具函数 ==========

static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total_size = size * nmemb;
    output->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// ========== LLMClient ==========

LLMClient::LLMClient(const LLMConfig& config) : config_(config) {}

std::string LLMClient::generate(const std::string& prompt) {
    Json::Value request;
    request["model"] = config_.model;
    request["prompt"] = prompt;
    request["stream"] = false;
    request["options"]["temperature"] = config_.temperature;
    request["options"]["num_predict"] = config_.max_tokens;
    request["options"]["top_p"] = config_.top_p;
    request["options"]["repeat_penalty"] = config_.repeat_penalty;

    if (!config_.stop.empty()) {
        request["options"]["stop"] = Json::arrayValue;
        for (const auto& s : config_.stop) {
            request["options"]["stop"].append(s);
        }
    }

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string body = Json::writeString(builder, request);

    std::string response = post_json("/api/generate", body);

    Json::CharReaderBuilder rb;
    std::istringstream stream(response);
    std::string errs;
    Json::Value json_resp;
    if (!Json::parseFromStream(rb, stream, &json_resp, &errs)) {
        throw std::runtime_error("Failed to parse Ollama response: " + errs);
    }

    return json_resp.get("response", "").asString();
}

void LLMClient::generate_stream(const std::string& prompt,
                                 std::function<void(std::string)> callback) {
    Json::Value request;
    request["model"] = config_.model;
    request["prompt"] = prompt;
    request["stream"] = true;
    request["options"]["temperature"] = config_.temperature;
    request["options"]["num_predict"] = config_.max_tokens;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string body = Json::writeString(builder, request);

    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to init CURL");

    std::string url = config_.base_url + "/api/generate";
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("CURL error: " + std::string(curl_easy_strerror(res)));
    }

    // 处理 SSE 流式响应（简化处理）
    std::istringstream stream(response);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.rfind("{", 0) == 0) {
            Json::CharReaderBuilder rb;
            std::istringstream line_stream(line);
            Json::Value json_resp;
            std::string errs;
            if (Json::parseFromStream(rb, line_stream, &json_resp, &errs)) {
                std::string token = json_resp.get("response", "").asString();
                if (!token.empty()) {
                    callback(token);
                }
            }
        }
    }
}

std::vector<float> LLMClient::embed(const std::string& text) {
    Json::Value request;
    request["model"] = config_.embedding_model.empty() ? config_.model : config_.embedding_model;
    request["prompt"] = text;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string body = Json::writeString(builder, request);

    std::string response = post_json("/api/embeddings", body);

    Json::CharReaderBuilder rb;
    std::istringstream stream(response);
    std::string errs;
    Json::Value json_resp;
    if (!Json::parseFromStream(rb, stream, &json_resp, &errs)) {
        throw std::runtime_error("Failed to parse embedding response: " + errs);
    }

    std::vector<float> embedding;
    const auto& emb = json_resp["embedding"];
    if (emb.isArray()) {
        for (const auto& v : emb) {
            embedding.push_back(static_cast<float>(v.asDouble()));
        }
    }
    return embedding;
}

bool LLMClient::is_available() const {
    try {
        get_json("/api/tags");
        return true;
    } catch (...) {
        return false;
    }
}

std::optional<LLMClient::ModelInfo> LLMClient::get_model_info() const {
    try {
        std::string response = get_json("/api/tags");
        Json::CharReaderBuilder rb;
        std::istringstream stream(response);
        Json::Value json_resp;
        std::string errs;
        if (!Json::parseFromStream(rb, stream, &json_resp, &errs)) {
            return std::nullopt;
        }

        ModelInfo info;
        info.name = config_.model;
        info.context_length = config_.context_window;
        info.supports_streaming = true;
        return info;
    } catch (...) {
        return std::nullopt;
    }
}

std::string LLMClient::post_json(const std::string& endpoint, const std::string& body) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to init CURL");

    std::string url = config_.base_url + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("CURL error: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

std::string LLMClient::get_json(const std::string& endpoint) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to init CURL");

    std::string url = config_.base_url + endpoint;
    std::string response;

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("CURL error: " + std::string(curl_easy_strerror(res)));
    }

    return response;
}

// ========== OllamaClient ==========

OllamaClient::OllamaClient(const std::string& base_url)
    : LLMClient(LLMConfig{}), base_url_(base_url) {}

std::vector<std::string> OllamaClient::list_models() {
    std::vector<std::string> models;
    std::string response = get_json("/api/tags");

    Json::CharReaderBuilder rb;
    std::istringstream stream(response);
    Json::Value json_resp;
    std::string errs;
    if (!Json::parseFromStream(rb, stream, &json_resp, &errs)) {
        return models;
    }

    const auto& models_arr = json_resp["models"];
    if (models_arr.isArray()) {
        for (const auto& m : models_arr) {
            models.push_back(m.get("name", "").asString());
        }
    }
    return models;
}

bool OllamaClient::pull_model(const std::string& model) {
    Json::Value request;
    request["name"] = model;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string body = Json::writeString(builder, request);

    try {
        post_json("/api/pull", body);
        return true;
    } catch (...) {
        return false;
    }
}

std::vector<float> OllamaClient::embed(const std::string& text) {
    Json::Value request;
    request["model"] = LLMClient::config_.embedding_model.empty()
        ? LLMClient::config_.model
        : LLMClient::config_.embedding_model;
    request["prompt"] = text;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string body = Json::writeString(builder, request);

    std::string response = post_json("/api/embeddings", body);

    Json::CharReaderBuilder rb;
    std::istringstream stream(response);
    Json::Value json_resp;
    std::string errs;
    if (!Json::parseFromStream(rb, stream, &json_resp, &errs)) {
        throw std::runtime_error("Failed to parse embedding response: " + errs);
    }

    std::vector<float> embedding;
    const auto& emb = json_resp["embedding"];
    if (emb.isArray()) {
        for (const auto& v : emb) {
            embedding.push_back(static_cast<float>(v.asDouble()));
        }
    }
    return embedding;
}

std::string OllamaClient::generate(const std::string& prompt,
                                    const std::string& model,
                                    float temperature,
                                    int max_tokens) {
    Json::Value request;
    request["model"] = model;
    request["prompt"] = prompt;
    request["stream"] = false;
    request["options"]["temperature"] = temperature;
    request["options"]["num_predict"] = max_tokens;

    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";
    std::string body = Json::writeString(builder, request);

    std::string response = post_json("/api/generate", body);

    Json::CharReaderBuilder rb;
    std::istringstream stream(response);
    Json::Value json_resp;
    std::string errs;
    if (!Json::parseFromStream(rb, stream, &json_resp, &errs)) {
        throw std::runtime_error("Failed to parse Ollama response: " + errs);
    }

    return json_resp.get("response", "").asString();
}

// ========== 工厂函数 ==========

std::unique_ptr<LLMClient> create_llm_client(const LLMConfig& config) {
    return std::make_unique<LLMClient>(config);
}

std::unique_ptr<OllamaClient> create_ollama_client(const std::string& base_url) {
    return std::make_unique<OllamaClient>(base_url.empty() ? "http://localhost:11434" : base_url);
}

}  // namespace rag