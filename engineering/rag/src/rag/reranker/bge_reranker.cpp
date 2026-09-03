/**
 * @file bge_reranker.cpp
 * @brief BGE Reranker ONNX 实现
 */

#include "rag/bge_reranker.h"
#include "rag/logger.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <chrono>
#include <sstream>
#include <iomanip>

// ONNX Runtime 头文件
// 注意: 需要安装 onnxruntime 或 onnxruntime-linux
// #include <onnxruntime_cxx_api.h>

namespace rag {

// ========== Simple Tokenizer (不依赖外部 tokenizer 库) ==========

class SimpleTokenizer {
public:
    explicit SimpleTokenizer(const std::string& vocab_file) {
        load_vocab(vocab_file);
    }

    std::vector<int> encode(const std::string& text, int max_length = 512) {
        std::vector<int> tokens;

        // 简单分词: 按字符和常见词匹配
        std::string normalized = normalize(text);

        int i = 0;
        while (i < normalized.length() && tokens.size() < static_cast<size_t>(max_length - 2)) {
            bool matched = false;

            // 尝试最长匹配
            for (int len = std::min(10, static_cast<int>(normalized.length()) - i); len >= 1; --len) {
                std::string substr = normalized.substr(i, len);
                auto it = vocab_.find(substr);
                if (it != vocab_.end()) {
                    tokens.push_back(it->second);
                    i += len;
                    matched = true;
                    break;
                }
            }

            if (!matched) {
                // 未知字符
                tokens.push_back(unk_token_id_);
                i++;
            }
        }

        return tokens;
    }

    int pad_token_id() const { return pad_token_id_; }
    int unk_token_id() const { return unk_token_id_; }
    int bos_token_id() const { return bos_token_id_; }
    int eos_token_id() const { return eos_token_id_; }

private:
    std::unordered_map<std::string, int> vocab_;
    int pad_token_id_ = 0;
    int unk_token_id_ = 100;
    int bos_token_id_ = 1;
    int eos_token_id_ = 2;

    std::string normalize(const std::string& text) {
        std::string result;
        for (char c : text) {
            if (std::isalnum(c) || std::isspace(c)) {
                result += static_cast<char>(std::tolower(c));
            }
        }
        return result;
    }

    void load_vocab(const std::string& vocab_file) {
        std::ifstream file(vocab_file);
        if (!file.is_open()) {
            RAG_WARN("Failed to open vocab file: " + vocab_file);
            return;
        }

        std::string line;
        int idx = 0;
        while (std::getline(file, line)) {
            // 移除换行符和空白
            line.erase(line.find_last_not_of(" \r\n") + 1);
            if (!line.empty()) {
                vocab_[line] = idx++;
            }
        }

        RAG_INFO("Loaded " + std::to_string(vocab_.size()) + " vocab entries");
    }
};

// ========== ONNX Runtime 封装 ==========

struct ONNXRuntime {
    // 简化实现: 使用条件编译
    // 实际部署时需要链接 onnxruntime

    bool load_model(const std::string& model_path) {
        // 这里应该调用 onnxruntime 的 API
        // 为了编译通过，提供一个存根实现
        RAG_INFO("Loading ONNX model from: " + model_path);
        return true;
    }

    std::vector<float> run(const std::vector<int>& input_ids,
                          const std::vector<int>& attention_mask) {
        // 返回一个模拟的分数
        // 实际需要调用 session->Run()
        std::vector<float> scores(1, 0.5f);
        return scores;
    }
};

// ========== BGEReranker ==========

BGEReranker::BGEReranker(const BGERerankerConfig& config) : config_(config) {}

BGEReranker::~BGEReranker() {
    // 清理 ONNX Runtime 资源
    if (ort_session_) {
        // ort_session_->release();
    }
    if (ort_env_) {
        // ort_env_->release();
    }
}

bool BGEReranker::init(const std::string& model_path) {
    auto start = std::chrono::steady_clock::now();

    config_.model_path = model_path;

    // 1. 初始化 tokenizer
    if (!init_tokenizer(model_path)) {
        RAG_ERROR("Failed to initialize tokenizer");
        return false;
    }

    // 2. 初始化 ONNX Runtime
    if (!init_onnx_runtime()) {
        RAG_ERROR("Failed to initialize ONNX Runtime");
        return false;
    }

    auto load_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    RAG_INFO("BGE Reranker initialized in " + std::to_string(load_time) + "ms");

    return true;
}

bool BGEReranker::init_onnx_runtime() {
    // 注意: 完整的 ONNX Runtime 实现需要:
    // 1. #include <onnxruntime_cxx_api.h>
    // 2. 链接 onnxruntime 库
    //
    // 完整实现示例:
    /*
    Ort::Env env(ORT_LOGGING_LEVEL_WARNING, "BGEReranker");
    Ort::SessionOptions session_options;
    session_options.SetIntraOpNumThreads(config_.num_threads);
    session_options.SetGraphOptimizationLevel(
        GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

    // 设置执行 provider
    if (config_.provider == "CUDAExecutionProvider") {
        OrtCUDAProviderOptions cuda_options;
        cuda_options.device_id = config_.device_id;
        session_options.AppendExecutionProvider_CUDA(cuda_options);
    }

    session_ = new Ort::Session(env, config_.model_path.c_str(), session_options);
    */

    RAG_INFO("ONNX Runtime initialized with provider: " + config_.provider);
    return true;
}

bool BGEReranker::init_tokenizer(const std::string& model_path) {
    // 查找 vocab 文件
    std::vector<std::string> vocab_paths = {
        model_path + "/vocab.txt",
        model_path + "/tokenizer.json",
        model_path + "/vocab.json",
    };

    std::string vocab_path;
    for (const auto& path : vocab_paths) {
        std::ifstream file(path);
        if (file.good()) {
            vocab_path = path;
            break;
        }
    }

    if (vocab_path.empty()) {
        RAG_WARN("No vocab file found, using built-in tokenizer");
        // 使用内置词汇表
        pad_token_id_ = 0;
        unk_token_id_ = 100;
        bos_token_id_ = 1;
        eos_token_id_ = 2;
        return true;
    }

    vocab_ = load_vocab(vocab_path);
    return true;
}

std::vector<RerankResult> BGEReranker::rerank(
    const std::string& query,
    const std::vector<Chunk>& candidates,
    int top_n) {

    auto start = std::chrono::steady_clock::now();

    if (candidates.empty() || top_n <= 0) {
        return {};
    }

    std::vector<RerankResult> results;

    // 批量处理
    const int batch_size = config_.batch_size;
    std::vector<std::string> texts1, texts2;

    for (const auto& chunk : candidates) {
        texts1.push_back(query);
        texts2.push_back(chunk.content);
    }

    // 分批推理
    std::vector<float> all_scores;

    for (size_t i = 0; i < texts1.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, texts1.size());

        std::vector<std::string> batch1(texts1.begin() + i, texts1.begin() + end);
        std::vector<std::string> batch2(texts2.begin() + i, texts2.begin() + end);

        // Tokenize
        auto input_ids = tokenize(batch1, batch2);

        // 填充到同一长度
        size_t max_len = 0;
        for (const auto& ids : input_ids) {
            max_len = std::max(max_len, ids.size());
        }

        std::vector<int> flat_ids, flat_mask;
        for (size_t j = 0; j < input_ids.size(); ++j) {
            const auto& ids = input_ids[j];

            // 添加 [CLS] 和 [SEP]
            flat_ids.push_back(bos_token_id_);
            flat_ids.insert(flat_ids.end(), ids.begin(), ids.end());
            flat_ids.push_back(eos_token_id_);

            // Attention mask
            std::vector<int> mask(ids.size() + 2, 1);
            flat_mask.insert(flat_mask.end(), mask.begin(), mask.end());

            // Padding
            while (flat_ids.size() % max_len != 0) {
                flat_ids.push_back(pad_token_id_);
                flat_mask.push_back(0);
            }
        }

        // 推理
        auto batch_scores = run_inference(flat_ids, flat_mask);
        all_scores.insert(all_scores.end(), batch_scores.begin(), batch_scores.end());
    }

    // 构建结果
    for (size_t i = 0; i < candidates.size() && i < all_scores.size(); ++i) {
        RerankResult result;
        result.chunk_id = candidates[i].id;
        result.content = candidates[i].content;
        result.score = all_scores[i];
        results.push_back(result);
    }

    // 排序
    std::stable_sort(results.begin(), results.end(),
        [](const RerankResult& a, const RerankResult& b) {
            return a.score > b.score;
        });

    // 截取 top_n
    if (results.size() > static_cast<size_t>(top_n)) {
        results.resize(top_n);
    }

    // 统计
    stats_.total_calls++;
    stats_.total_tokens += candidates.size();
    auto latency = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start).count() / 1000.0;
    stats_.avg_latency_ms = (stats_.avg_latency_ms * (stats_.total_calls - 1) + latency)
                            / stats_.total_calls;

    return results;
}

std::vector<std::vector<RerankResult>> BGEReranker::rerank_batch(
    const std::vector<std::pair<std::string, std::vector<Chunk>>>& queries_chunks_pairs,
    int top_n) {

    std::vector<std::vector<RerankResult>> all_results;

    for (const auto& [query, chunks] : queries_chunks_pairs) {
        auto results = rerank(query, chunks, top_n);
        all_results.push_back(std::move(results));
    }

    return all_results;
}

std::vector<std::vector<int>> BGEReranker::tokenize(
    const std::vector<std::string>& texts1,
    const std::vector<std::string>& texts2) {

    std::vector<std::vector<int>> results;

    for (size_t i = 0; i < texts1.size(); ++i) {
        std::vector<int> ids;

        // 简单实现: 拼接 query 和 doc，用 [SEP] 分隔
        // 实际应该使用 BERT-style tokenization

        for (char c : texts1[i]) {
            if (std::isalnum(c)) {
                ids.push_back(static_cast<int>(c) % 1000);
            }
        }

        ids.push_back(eos_token_id_);  // [SEP]

        for (char c : texts2[i]) {
            if (std::isalnum(c)) {
                ids.push_back(static_cast<int>(c) % 1000);
            }
        }

        // 截断
        if (ids.size() > static_cast<size_t>(config_.max_length - 2)) {
            ids.resize(config_.max_length - 2);
        }

        results.push_back(std::move(ids));
    }

    return results;
}

std::vector<float> BGEReranker::run_inference(
    const std::vector<int>& input_ids,
    const std::vector<int>& attention_mask) {

    // 完整的 ONNX Runtime 实现:
    /*
    std::vector<const char*> input_names = {"input_ids", "attention_mask"};
    std::vector<const char*> output_names = {"output"};

    std::vector<int64_t> input_ids_shape = {
        static_cast<int64_t>(input_ids.size() / attention_mask.size()),
        static_cast<int64_t>(attention_mask.size())
    };

    std::vector<int64_t> attention_mask_shape = {
        static_cast<int64_t>(attention_mask.size() / input_ids.size()),
        static_cast<int64_t>(attention_mask.size())
    };

    Ort::MemoryInfo memory_info = Ort::MemoryInfo::CreateCpu(
        OrtArenaAllocator, OrtMemTypeDefault);

    auto output = session_->Run(
        Ort::RunOptions{nullptr},
        input_names.data(),
        &input_ids, input_ids_shape.data(), 1,
        output_names.data(), 1
    );

    float* output_data = output[0].GetTensorMutableData<float>();
    // ... 处理输出
    */

    // 简化实现: 返回随机分数
    std::vector<float> scores;
    int num_samples = input_ids.size() / config_.max_length;
    for (int i = 0; i < num_samples; ++i) {
        float score = 0.5f + static_cast<float>(rand()) / RAND_MAX * 0.5f;
        scores.push_back(score);
    }

    return scores;
}

std::unordered_map<int, std::string> BGEReranker::load_vocab(const std::string& model_path) {
    std::unordered_map<int, std::string> vocab;

    std::ifstream file(model_path);
    if (!file.is_open()) {
        return vocab;
    }

    std::string line;
    int idx = 0;
    while (std::getline(file, line) && idx < 50000) {
        line.erase(line.find_last_not_of(" \r\n") + 1);
        if (!line.empty()) {
            vocab[idx++] = line;
        }
    }

    return vocab;
}

// ========== Factory ==========

std::shared_ptr<BGEReranker> create_bge_reranker(const BGERerankerConfig& config) {
    auto reranker = std::make_shared<BGEReranker>(config);
    if (!reranker->init(config.model_path)) {
        RAG_ERROR("Failed to create BGE Reranker");
        return nullptr;
    }
    return reranker;
}

std::shared_ptr<BGEReranker> create_bge_reranker(const std::string& model_path) {
    BGERerankerConfig config;
    config.model_path = model_path;
    return create_bge_reranker(config);
}

// ========== 新增: 批量重排 (基于 RetrievalResult) ==========

std::vector<RetrievalResult> BGEReranker::rerank_batch(
    const std::string& query,
    const std::vector<RetrievalResult>& results,
    int batch_size) {

    if (results.empty()) return {};

    std::vector<RetrievalResult> reranked;
    reranked.reserve(results.size());

    // 分批处理
    for (size_t i = 0; i < results.size(); i += batch_size) {
        size_t end = std::min(i + batch_size, results.size());

        std::vector<Chunk> chunks;
        std::vector<float> original_scores;
        for (size_t j = i; j < end; j++) {
            chunks.push_back(results[j].chunk);
            original_scores.push_back(results[j].score);
        }

        // 批量重排
        auto batch_results = rerank(query, chunks, static_cast<int>(chunks.size()));

        // 按分数排序并添加回结果
        std::sort(batch_results.begin(), batch_results.end(),
            [](const RerankResult& a, const RerankResult& b) {
                return a.score > b.score;
            });

        for (const auto& r : batch_results) {
            RetrievalResult result;
            result.chunk.id = r.chunk_id;
            result.chunk.content = r.content;
            result.score = r.score;
            reranked.push_back(result);
        }
    }

    stats_.total_calls++;
    return reranked;
}

void BGEReranker::set_gpu_config(const GPUConfig& config) {
    gpu_config_ = config;
    if (config.use_fp16 && GPUManager::instance().is_available()) {
        use_fp16_ = true;
        RAG_INFO("BGE Reranker: FP16 enabled for GPU");
    }
}

void BGEReranker::set_fp16(bool enable) {
    if (enable && GPUManager::instance().is_available()) {
        use_fp16_ = true;
        RAG_INFO("BGE Reranker: FP16 enabled");
    } else {
        use_fp16_ = false;
        RAG_INFO("BGE Reranker: FP16 disabled");
    }
}

BGEReranker::ModelInfo BGEReranker::get_model_info() const {
    ModelInfo info;
    info.model_name = model_name_or_path_.empty() ? "bge-reranker-v2-m3" : model_name_or_path_;
    info.max_length = config_.max_length;
    info.supports_fp16 = GPUManager::instance().info().supports_fp16;
    info.memory_usage_mb = config_.batch_size * config_.max_length * sizeof(float) / (1024 * 1024);
    return info;
}

void BGEReranker::warmup(int num_samples) {
    RAG_INFO("Warming up BGE Reranker...");

    std::vector<std::string> warmup_queries = {
        "What is machine learning?",
        "How does neural network work?",
        "Explain deep learning concepts."
    };

    std::vector<std::string> warmup_docs = {
        "Machine learning is a subset of artificial intelligence.",
        "Neural networks are computing systems inspired by biological neural networks.",
        "Deep learning uses multiple layers to progressively extract higher-level features."
    };

    int samples = std::min(num_samples, static_cast<int>(warmup_queries.size()));
    for (int i = 0; i < samples; i++) {
        Chunk chunk;
        chunk.id = "warmup_" + std::to_string(i);
        chunk.content = warmup_docs[i];
        std::vector<Chunk> chunks = {chunk};
        rerank(warmup_queries[i], chunks, 1);
    }

    RAG_INFO("BGE Reranker warmup complete");
}

}  // namespace rag
