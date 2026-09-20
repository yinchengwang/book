// stubs.cpp
// 工厂函数实现：通过 HTTP API 提供真实语义 Embedding 和 LLM 推理
//
// 当所有 Embedding 后端都不可用时，降级到 SimpleEmbeddingService（hash-based）。
// LLM 失败时返回 nullptr。

#include "mmrag/embedding.h"
#include "mmrag/llm_service.h"
#include "mmrag/minimax_llm.h"  // MiniMax API-based LLM
#include "mmrag/http_embedding.h"
#include "mmrag/graph_support.h"
#include "mmrag/database.h"
#include "mmrag/knowledge_graph.h"
#include "mmrag/modular/pipeline/corrective_pipeline.h"
#include "mmrag/modular/pipeline/react_pipeline.h"
#include "mmrag/modular/pipeline/naive_pipeline.h"
#include "mmrag/modular/pipeline/advanced_pipeline.h"
#include "mmrag/modular/pipeline/iterative_pipeline.h"
#include "mmrag/modular/pipeline/recursive_pipeline.h"
#include "mmrag/modular/pipeline/hybrid_pipeline.h"
#include "mmrag/modular/pipeline/hyde_pipeline.h"
#include "mmrag/modular/pipeline/graph_pipeline.h"
#include "mmrag/logger.h"
#include "mmrag/eval/eval_store.h"

#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include <cmath>
#include <cstring>  // for strlen (MINIMAX_API_KEY debug)
#include <filesystem>

namespace mmrag {

// =============================================================================
// SimpleEmbeddingService - 基于 hash 的确定性向量降级
// 当所有 Embedding 后端不可用时，使用文本 hash 生成确定性单位向量。
// 同一文本始终产生相同向量，不同文本大致正交，可满足基本检索需求。
// =============================================================================
SimpleEmbeddingService::SimpleEmbeddingService(int dimension)
    : dimension_(dimension), ready_(false) {}

std::vector<float> SimpleEmbeddingService::encode(const std::string& text) {
    std::vector<float> embedding(dimension_);

    // 使用文本内容生成确定性随机种子
    std::hash<std::string> hasher;
    size_t seed = hasher(text);
    std::mt19937 gen(static_cast<unsigned>(seed));
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    // 生成随机向量
    float norm = 0.0f;
    for (int i = 0; i < dimension_; i++) {
        embedding[i] = dist(gen);
        norm += embedding[i] * embedding[i];
    }

    // L2 归一化（单位向量，余弦相似度 = 点积）
    norm = std::sqrt(norm) + 1e-10f;
    for (int i = 0; i < dimension_; i++) {
        embedding[i] /= norm;
    }

    encode_count_++;
    return embedding;
}

std::vector<std::vector<float>>
SimpleEmbeddingService::encode_batch(const std::vector<std::string>& texts) {
    std::vector<std::vector<float>> result;
    result.reserve(texts.size());
    for (const auto& t : texts) {
        result.push_back(encode(t));
    }
    return result;
}

EmbeddingStats SimpleEmbeddingService::get_stats() const {
    EmbeddingStats stats;
    stats.encode_count = encode_count_;
    return stats;
}

// 默认降级构造：返回 SimpleEmbeddingService 实例（hash-based 确定性向量）
static std::shared_ptr<EmbeddingService> make_simple_embedding(int dim) {
    auto svc = std::make_shared<SimpleEmbeddingService>(dim);
    svc->load();
    return svc;
}

// =============================================================================
// EmbeddingService factory — 支持 kbase / http / 默认（降级到 Simple）
// =============================================================================
std::shared_ptr<EmbeddingService>
create_embedding_service(const std::string& type,
                         const std::string& model_path,
                         int dimension) {
    // kbase 类型：使用真实 GGUF MiniLM-L6 推理
    if (type == "kbase") {
        if (model_path.empty()) {
            RAG_WARN("create_embedding_service: type='kbase' but model_path is empty, "
                     "falling back to SimpleEmbeddingService");
            return make_simple_embedding(dimension);
        }
        // 检查模型文件是否存在，不存在则降级到 Simple
        if (!std::filesystem::exists(model_path)) {
            RAG_WARN("create_embedding_service: GGUF model not found at " + model_path +
                     ", falling back to SimpleEmbeddingService (hash-based vectors)");
            return make_simple_embedding(dimension);
        }
        return std::make_shared<KbaseEmbeddingService>(model_path, dimension);
    }

    // http 类型：通过 HTTP 调用 OpenAI-compatible /v1/embeddings 接口
    // 兼容智谱AI、OpenAI、通义千问、本地 vLLM 等
    // 注：此重载使用默认值；推荐使用 create_embedding_service(EmbeddingConfig)
    // 以便传入 api_url/api_key/api_model 等 HTTP 配置
    if (type == "http") {
        HttpEmbeddingConfig config;
        config.model = model_path.empty() ? "embedding-3" : model_path;
        config.dimension = dimension;
        config.max_cache_size = 10000;
        config.timeout_seconds = 30;

        auto svc = std::make_shared<HttpEmbeddingService>(config);
        if (svc->is_ready()) {
            RAG_INFO("HTTP Embedding service ready (model: " + config.model +
                     ", url: " + config.api_url + ")");
            return svc;
        } else {
            RAG_WARN("HTTP Embedding service not available, using fallback SimpleEmbeddingService");
            return make_simple_embedding(dimension);
        }
    }

    // 默认：降级到 SimpleEmbeddingService（hash-based 确定性向量）
    RAG_WARN("create_embedding_service: unknown type '" + type +
             "', falling back to SimpleEmbeddingService");
    return make_simple_embedding(dimension);
}

// =============================================================================
// EmbeddingService factory (完整配置版本) — 从 EmbeddingConfig 读取所有 HTTP 参数
// =============================================================================
std::shared_ptr<EmbeddingService>
create_embedding_service(const EmbeddingConfig& cfg) {
    const std::string& type = cfg.model_type;
    int dimension = cfg.api_dimension > 0 ? cfg.api_dimension : 768;

    // kbase 类型
    if (type == "kbase") {
        if (cfg.model_path.empty()) {
            RAG_WARN("create_embedding_service: type='kbase' but model_path is empty, "
                     "falling back to SimpleEmbeddingService");
            return make_simple_embedding(dimension);
        }
        if (!std::filesystem::exists(cfg.model_path)) {
            RAG_WARN("create_embedding_service: GGUF model not found at " + cfg.model_path +
                     ", falling back to SimpleEmbeddingService (hash-based vectors)");
            return make_simple_embedding(dimension);
        }
        return std::make_shared<KbaseEmbeddingService>(cfg.model_path, dimension);
    }

    // http 类型：从 EmbeddingConfig 读取 api_url/api_key/api_model
    if (type == "http") {
        HttpEmbeddingConfig hcfg;
        // 拆分 URL：api_url 是 base，path 部分从 api_url 提取后剩余
        // 例如 api_url = "https://open.bigmodel.cn/api/paas/v4/embeddings"
        //   → base = "https://open.bigmodel.cn"
        //   → path = "/api/paas/v4/embeddings"
        std::string full_url = cfg.api_url;
        if (full_url.empty()) {
            full_url = "https://open.bigmodel.cn/api/paas/v4/embeddings";
        }
        // 尝试提取 path：找到第三个 '/' 之后的位置
        size_t slash_count = 0;
        size_t path_start = std::string::npos;
        for (size_t i = 0; i < full_url.size(); ++i) {
            if (full_url[i] == '/') {
                slash_count++;
                if (slash_count == 3) {
                    path_start = i;
                    break;
                }
            }
        }
        if (path_start != std::string::npos) {
            hcfg.api_url = full_url.substr(0, path_start);
            hcfg.api_path = full_url.substr(path_start);
        } else {
            hcfg.api_url = full_url;
            hcfg.api_path = "/v1/embeddings";
        }

        hcfg.api_key = cfg.api_key;
        hcfg.model = cfg.api_model.empty() ? "embedding-3" : cfg.api_model;
        hcfg.dimension = dimension;
        hcfg.max_cache_size = 10000;
        hcfg.timeout_seconds = cfg.timeout_seconds > 0 ? cfg.timeout_seconds : 30;
        hcfg.max_retries = 3;

        // API Key 优先级：cfg.api_key > ARK_API_KEY env > ZHIPU_API_KEY env
        if (hcfg.api_key.empty()) {
            const char* env_key = std::getenv("ARK_API_KEY");
            if (env_key && *env_key) {
                hcfg.api_key = env_key;
            }
        }
        if (hcfg.api_key.empty()) {
            const char* env_key = std::getenv("ZHIPU_API_KEY");
            if (env_key && *env_key) {
                hcfg.api_key = env_key;
            }
        }
        if (hcfg.api_key.empty()) {
            const char* env_key = std::getenv("HTTP_API_KEY");
            if (env_key && *env_key) {
                hcfg.api_key = env_key;
            }
        }

        // API Key 缺失时给出明确警告
        if (hcfg.api_key.empty()) {
            RAG_WARN("HTTP Embedding: api_key is empty. Set one of these env vars: "
                     "ARK_API_KEY, ZHIPU_API_KEY, HTTP_API_KEY.");
        } else {
            RAG_INFO("HTTP Embedding: api_key loaded (length=" +
                     std::to_string(hcfg.api_key.size()) + ")");
        }

        auto svc = std::make_shared<HttpEmbeddingService>(hcfg);
        if (svc->is_ready()) {
            RAG_INFO("HTTP Embedding service ready (model: " + hcfg.model +
                     ", url: " + hcfg.api_url + hcfg.api_path + ")");
            return svc;
        }
        RAG_WARN("HTTP Embedding service not available, using fallback SimpleEmbeddingService");
        return make_simple_embedding(dimension);
    }

    // 默认降级到 SimpleEmbeddingService
    RAG_WARN("create_embedding_service: unknown type '" + type +
             "', falling back to SimpleEmbeddingService");
    return make_simple_embedding(dimension);
}

// =============================================================================
// LLMService factory — 优先 MiniMax API（需要 MINIMAX_API_KEY 环境变量）
// 如果 API key 未设置则返回 nullptr（pipeline fallback 到纯检索模式）
// =============================================================================
std::unique_ptr<LLMService> create_llm_service() {
    RAG_INFO("create_llm_service: called, checking MINIMAX_API_KEY env var...");
    // Try MiniMax API
    MiniMaxLLMConfig mm_config;
    const char* env_key = std::getenv("MINIMAX_API_KEY");
    RAG_INFO("create_llm_service: MINIMAX_API_KEY=" + std::string(env_key ? "SET (len=" + std::to_string(strlen(env_key)) + ")" : "NOT SET"));
    if (env_key && *env_key) {
        mm_config.api_key = env_key;
        auto svc = std::make_unique<MiniMaxLLMService>(mm_config);
        // load() checks api_key and sets loaded_
        LLMConfig dummy_config;
        svc->load("minimax://" + mm_config.model, dummy_config);
        if (svc->is_loaded()) {
            RAG_INFO("MiniMax LLM service ready (model: " + mm_config.model + ")");
            return svc;
        }
    }
    RAG_WARN("create_llm_service: no LLM backend available. "
             "Set MINIMAX_API_KEY env var to enable API-based LLM. "
             "Falling back to retrieval-only mode.");
    return nullptr;
}

std::unique_ptr<LLMService> create_llm_service(const std::string& type) {
    if (type == "minimax" || type.empty()) {
        return create_llm_service();
    }
    RAG_WARN("create_llm_service: unsupported type '" + type + "'");
    return nullptr;
}

// =============================================================================
// GraphExtension factory (原 graph_extension.cpp)
// =============================================================================
std::unique_ptr<GraphEngineExtension>
create_graph_extension(std::shared_ptr<Database> /*db*/,
                       const GraphConfig& /*cfg*/,
                       std::shared_ptr<KnowledgeGraph> /*kg*/) {
    return nullptr;
}

}  // namespace mmrag

// =============================================================================
// EvalRunStore stub implementation (Phase 8 eval_store.cpp excluded due to nlohmann/json dependency)
// =============================================================================
namespace mmrag::eval {

EvalRunStore::EvalRunStore(const std::string& /*path*/) {}

std::string EvalRunStore::generate_run_id() {
    return "run_stub_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::vector<EvalRunRecord> EvalRunStore::list_runs(int /*limit*/) { return {}; }
std::vector<EvalRunRecord> EvalRunStore::list_runs_by_pipeline(const std::string& /*pipeline*/, int /*limit*/) { return {}; }
bool EvalRunStore::save_run(const EvalRunRecord& /*record*/) { return true; }
std::optional<EvalRunRecord> EvalRunStore::load_run(const std::string& /*run_id*/) { return std::nullopt; }
ComparisonResult EvalRunStore::compare_runs(const std::string& /*run_id_1*/, const std::string& /*run_id_2*/) { return {}; }
std::vector<std::pair<int64_t, double>> EvalRunStore::get_metric_trend(const std::string& /*pipeline*/, const std::string& /*metric*/, int /*limit*/) { return {}; }
bool EvalRunStore::delete_run(const std::string& /*run_id*/) { return true; }
std::string EvalRunStore::export_to_json(const EvalRunRecord& /*record*/) const { return "{}"; }
std::optional<EvalRunRecord> EvalRunStore::import_from_json(const std::string& /*json_content*/) const { return std::nullopt; }

}  // namespace mmrag::eval

// =============================================================================
// modular 命名空间：被排除的 Pipeline 类的 stub 实现
// pipeline_factory.cpp 引用这些类，所以即使源文件被 EXCLUDE 也需要链接符号
// =============================================================================
namespace mmrag::modular {

CorrectivePipeline::CorrectivePipeline() = default;
CorrectivePipeline::~CorrectivePipeline() = default;
bool CorrectivePipeline::init(const ModularConfig&) { return false; }
ModularQueryResult CorrectivePipeline::query(const ModularQuery&) { return {}; }
bool CorrectivePipeline::is_ready() const { return false; }

ReActPipeline::ReActPipeline() = default;
ReActPipeline::~ReActPipeline() = default;
bool ReActPipeline::init(const ModularConfig&) { return false; }
ModularQueryResult ReActPipeline::query(const ModularQuery&) { return {}; }
bool ReActPipeline::is_ready() const { return false; }

// HybridPipeline / HyDEPipeline / GraphPipeline — pipeline_factory.cpp 引用它们
// 的默认构造函数。这些类需要非内联的链接符号。stub 让 is_ready()=false
// （除非源代码后续实现并启用对应 .cpp 文件）。
HybridPipeline::HybridPipeline() = default;
HybridPipeline::~HybridPipeline() = default;
bool HybridPipeline::init(const ModularConfig&) { return false; }
ModularQueryResult HybridPipeline::query(const ModularQuery&) { return {}; }
bool HybridPipeline::is_ready() const { return false; }

HyDEPipeline::HyDEPipeline() = default;
HyDEPipeline::~HyDEPipeline() = default;
bool HyDEPipeline::init(const ModularConfig&) { return false; }
ModularQueryResult HyDEPipeline::query(const ModularQuery&) { return {}; }
bool HyDEPipeline::is_ready() const { return false; }

GraphPipeline::GraphPipeline() = default;
GraphPipeline::~GraphPipeline() = default;
bool GraphPipeline::init(const ModularConfig&) { return false; }
ModularQueryResult GraphPipeline::query(const ModularQuery&) { return {}; }
bool GraphPipeline::is_ready() const { return false; }

// NaivePipeline / AdvancedPipeline / IterativePipeline / RecursivePipeline
// have real implementations in their respective .cpp files (now compiled into
// the build). The stubs that were previously needed for linking are removed
// to avoid duplicate symbol conflicts with the real implementations.
//
// Only pipeline classes EXCLUDED from the build (Corrective, ReAct, Hybrid,
// HyDE, Graph) still need stub symbols here.

}  // namespace mmrag::modular
