/**
 * @file llama_service.cpp
 * @brief LlamaService 实现 - 基于 llama.cpp 的本地 LLM 服务
 */

#include "rag/modular/llm/llama_service.h"
#include "rag/logger.h"
#include "rag/error.h"
#include <chrono>
#include <algorithm>

// llama.cpp 头文件
#include "llama.h"
#include "common.h"
#include "json.hpp"

using json = nlohmann::json;

namespace rag {

// ========== Pimpl 实现 ==========

class LlamaService::Impl {
public:
    ~Impl() {
        if (ctx_) {
            llama_free(ctx_);
            ctx_ = nullptr;
        }
        if (model_) {
            llama_free_model(model_);
            model_ = nullptr;
        }
    }

    bool load_model(const std::string& model_path, const LlamaConfig& config) {
        RAG_LOG_INFO("正在加载 Llama 模型: {}", model_path);

        // 模型参数
        llama_model_params model_params = llama_model_default_params();
        model_params.n_gpu_layers = config.n_gpu_layers;
        model_params.use_mmap = true;
        model_params.use_mlock = false;

        // 加载模型
        model_ = llama_load_model_from_file(model_path.c_str(), model_params);
        if (!model_) {
            RAG_LOG_ERROR("无法加载模型: {}", model_path);
            return false;
        }

        // 上下文参数
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = config.n_ctx;
        ctx_params.n_threads = config.n_threads;
        ctx_params.no_perf = false;

        // 创建上下文
        ctx_ = llama_new_context_with_model(model_, ctx_params);
        if (!ctx_) {
            RAG_LOG_ERROR("无法创建推理上下文");
            llama_free_model(model_);
            model_ = nullptr;
            return false;
        }

        RAG_LOG_INFO("模型加载成功");
        return true;
    }

    void unload_model() {
        if (ctx_) {
            llama_free(ctx_);
            ctx_ = nullptr;
        }
        if (model_) {
            llama_free_model(model_);
            model_ = nullptr;
        }
        RAG_LOG_INFO("模型已卸载");
    }

    bool is_loaded() const {
        return model_ != nullptr && ctx_ != nullptr;
    }

    GenerateResult generate(const std::string& prompt,
                           const GenerateOptions& options,
                           const LlamaConfig& llama_config) {
        GenerateResult result;
        auto start_time = std::chrono::steady_clock::now();

        if (!is_loaded()) {
            result.finish_reason = "error";
            result.text = "模型未加载";
            return result;
        }

        // 构建采样参数
        llama_sampler_params sampler_params;
        sampler_params.temp = options.temperature > 0 ? options.temperature : llama_config.temperature;
        sampler_params.top_p = options.top_p > 0 ? options.top_p : llama_config.top_p;
        sampler_params.top_k = options.top_k > 0 ? options.top_k : llama_config.top_k;
        sampler_params.repeat_penalty = options.repeat_penalty > 0 ? options.repeat_penalty : llama_config.repeat_penalty;
        sampler_params.repeat_last_n = 64;
        sampler_params.seed = options.seed >= 0 ? options.seed : llama_config.seed;

        // 对话模板 (简单处理)
        std::string formatted_prompt = prompt;
        if (!llama_model_add_bos_token(model_)) {
            // 模型需要 BOS token
            formatted_prompt = "<s> " + prompt;
        }

        // tokenize
        std::vector<llama_token> tokens = ::llama_tokenize(model_, formatted_prompt, true);
        if (tokens.empty()) {
            result.finish_reason = "error";
            result.text = "无法 tokenize 输入";
            return result;
        }

        // 检查上下文长度
        int n_ctx = llama_n_ctx(ctx_);
        if ((int)tokens.size() > n_ctx - 4) {
            result.finish_reason = "error";
            result.text = "输入过长，超出上下文窗口";
            return result;
        }

        // 设置生成上限
        int max_tokens = options.max_tokens > 0 ? options.max_tokens : llama_config.max_tokens;
        if (max_tokens > n_ctx - (int)tokens.size()) {
            max_tokens = n_ctx - (int)tokens.size();
        }

        // 评估输入
        if (!llama_eval(ctx_, tokens.data(), tokens.size(), 0, llama_config.n_threads)) {
            result.finish_reason = "error";
            result.text = "评估输入失败";
            return result;
        }

        // 生成
        std::string output;
        llama_token new_token;
        int tokens_generated = 0;

        while (tokens_generated < max_tokens) {
            new_token = llama_sample_token(ctx_, sampler_params);

            if (llama_token_is_eog(model_, new_token)) {
                result.finish_reason = "stop";
                break;
            }

            char buf[256];
            int n = llama_token_to_piece(model_, new_token, buf, sizeof(buf));
            if (n > 0) {
                output.append(buf, n);
                tokens_generated++;
            }

            // 检查停止序列
            if (!llama_config.stop_sequence.empty()) {
                if (output.find(llama_config.stop_sequence) != std::string::npos) {
                    size_t pos = output.find(llama_config.stop_sequence);
                    output = output.substr(0, pos);
                    result.finish_reason = "stop";
                    break;
                }
            }

            // 评估新 token
            if (!llama_eval(ctx_, &new_token, 1, tokens_generated, llama_config.n_threads)) {
                break;
            }
        }

        if (result.finish_reason.empty()) {
            result.finish_reason = tokens_generated >= max_tokens ? "length" : "stop";
        }

        result.text = output;
        result.tokens_generated = tokens_generated;
        result.finished = true;

        auto end_time = std::chrono::steady_clock::now();
        result.duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time).count();

        return result;
    }

    void generate_stream(const std::string& prompt,
                        const GenerateOptions& options,
                        const LlamaConfig& llama_config,
                        StreamCallback callback) {
        if (!is_loaded()) {
            callback("模型未加载", true);
            return;
        }

        // 构建采样参数
        llama_sampler_params sampler_params;
        sampler_params.temp = options.temperature > 0 ? options.temperature : llama_config.temperature;
        sampler_params.top_p = options.top_p > 0 ? options.top_p : llama_config.top_p;
        sampler_params.top_k = options.top_k > 0 ? options.top_k : llama_config.top_k;
        sampler_params.repeat_penalty = options.repeat_penalty > 0 ? options.repeat_penalty : llama_config.repeat_penalty;
        sampler_params.repeat_last_n = 64;
        sampler_params.seed = options.seed >= 0 ? options.seed : llama_config.seed;

        // 对话模板
        std::string formatted_prompt = prompt;
        if (!llama_model_add_bos_token(model_)) {
            formatted_prompt = "<s> " + prompt;
        }

        // tokenize
        std::vector<llama_token> tokens = ::llama_tokenize(model_, formatted_prompt, true);
        if (tokens.empty()) {
            callback("无法 tokenize 输入", true);
            return;
        }

        int n_ctx = llama_n_ctx(ctx_);
        if ((int)tokens.size() > n_ctx - 4) {
            callback("输入过长，超出上下文窗口", true);
            return;
        }

        int max_tokens = options.max_tokens > 0 ? options.max_tokens : llama_config.max_tokens;
        if (max_tokens > n_ctx - (int)tokens.size()) {
            max_tokens = n_ctx - (int)tokens.size();
        }

        // 评估输入
        if (!llama_eval(ctx_, tokens.data(), tokens.size(), 0, llama_config.n_threads)) {
            callback("评估输入失败", true);
            return;
        }

        // 流式生成
        std::string output;
        llama_token new_token;
        int tokens_generated = 0;
        bool complete = false;

        while (tokens_generated < max_tokens && !complete) {
            new_token = llama_sample_token(ctx_, sampler_params);

            if (llama_token_is_eog(model_, new_token)) {
                complete = true;
                break;
            }

            char buf[256];
            int n = llama_token_to_piece(model_, new_token, buf, sizeof(buf));
            if (n > 0) {
                output.append(buf, n);
                tokens_generated++;
                callback(buf, false);
            }

            // 检查停止序列
            if (!llama_config.stop_sequence.empty()) {
                if (output.find(llama_config.stop_sequence) != std::string::npos) {
                    complete = true;
                    break;
                }
            }

            if (!llama_eval(ctx_, &new_token, 1, tokens_generated, llama_config.n_threads)) {
                complete = true;
                break;
            }
        }

        callback("", true);  // 完成信号
    }

    int context_window() const {
        if (!ctx_) return 0;
        return llama_n_ctx(ctx_);
    }

    const std::string& model_type() const {
        return model_type_;
    }

    void set_model_type(const std::string& type) {
        model_type_ = type;
    }

    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    std::string model_path_;
    std::string model_type_ = "llama";
};

// ========== LlamaService 实现 ==========

LlamaService::LlamaService()
    : impl_(std::make_unique<Impl>()) {
}

LlamaService::~LlamaService() = default;

void LlamaService::load(const std::string& model_path, const LLMConfig& config) {
    LlamaConfig llama_config;
    llama_config.n_ctx = config.n_ctx > 0 ? config.n_ctx : 4096;
    llama_config.n_threads = config.n_threads > 0 ? config.n_threads : 4;
    llama_config.max_tokens = config.max_tokens > 0 ? config.max_tokens : 1024;
    llama_config.temperature = config.temperature;
    llama_config.top_p = config.top_p;
    llama_config.seed = -1;

    impl_->model_path_ = model_path;
    impl_->set_model_type(config.model_type);

    if (!impl_->load_model(model_path, llama_config)) {
        RAG_THROW(RAG_ERROR_INIT, "无法加载 Llama 模型");
    }
}

void LlamaService::unload() {
    impl_->unload_model();
}

bool LlamaService::is_loaded() const {
    return impl_->is_loaded();
}

GenerateResult LlamaService::generate(const std::string& prompt,
                                      const GenerateOptions& options) {
    return impl_->generate(prompt, options, llama_config_);
}

void LlamaService::generate_stream(const std::string& prompt,
                                   const GenerateOptions& options,
                                   StreamCallback callback) {
    impl_->generate_stream(prompt, options, llama_config_, callback);
}

std::vector<GenerateResult> LlamaService::generate_batch(
    const std::vector<std::string>& prompts,
    const GenerateOptions& options) {
    std::vector<GenerateResult> results;
    results.reserve(prompts.size());
    for (const auto& prompt : prompts) {
        results.push_back(generate(prompt, options));
    }
    return results;
}

int LlamaService::context_window() const {
    return impl_->context_window();
}

const std::string& LlamaService::model_type() const {
    return impl_->model_type();
}

const std::string& LlamaService::model_path() const {
    return impl_->model_path_;
}

}  // namespace rag
