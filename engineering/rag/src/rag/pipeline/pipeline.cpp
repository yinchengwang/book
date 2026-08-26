/**
 * @file pipeline.cpp
 * @brief RAG 检索 Pipeline 实现
 */

#include "rag/pipeline.h"
#include "rag/retriever.h"
#include "rag/reranker.h"
#include "rag/query_expander.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <future>
#include <queue>

namespace rag {

// ========== Pipeline Statistics ==========

struct PipelineStats {
    uint64_t total_queries = 0;
    uint64_t cache_hits = 0;
    uint64_t cache_misses = 0;
    std::unordered_map<std::string, uint64_t> query_type_counts;
    std::unordered_map<std::string, double> stage_times_ms;
};

// ========== Pipeline::Impl ==========

struct RetrievalPipeline::Impl {
    // 配置
    Config config;

    // Stages
    std::vector<std::shared_ptr<RetrievalStage>> stages;
    std::map<int, std::shared_ptr<RetrievalStage>> ordered_stages;

    // 组件
    std::shared_ptr<QueryClassifier> classifier;
    std::shared_ptr<RetrievalCache> cache;

    // 统计
    PipelineStats stats;

    // 锁
    std::mutex stats_mutex;
    std::mutex stages_mutex;

    Impl() = default;

    // 排序 stages
    void rebuild_order() {
        std::lock_guard<std::mutex> lock(stages_mutex);
        ordered_stages.clear();
        int order = 0;
        for (auto& stage : stages) {
            ordered_stages[order++] = stage;
        }
    }

    // 添加 stage
    void add_stage(std::shared_ptr<RetrievalStage> stage, int order) {
        std::lock_guard<std::mutex> lock(stages_mutex);
        if (order < 0) {
            order = static_cast<int>(stages.size());
        }
        stages.push_back(stage);
        ordered_stages[order] = stage;
    }

    // 移除 stage
    void remove_stage(const std::string& name) {
        std::lock_guard<std::mutex> lock(stages_mutex);
        stages.erase(
            std::remove_if(stages.begin(), stages.end(),
                [&name](const auto& s) { return s->name() == name; }),
            stages.end()
        );
        rebuild_order();
    }
};

// ========== RetrievalPipeline ==========

RetrievalPipeline::RetrievalPipeline() : impl_(std::make_unique<Impl>()) {}

RetrievalPipeline::~RetrievalPipeline() = default;

void RetrievalPipeline::configure(const Config& config) {
    impl_->config = config;
}

void RetrievalPipeline::add_stage(std::shared_ptr<RetrievalStage> stage, int order) {
    impl_->add_stage(stage, order);
}

void RetrievalPipeline::remove_stage(const std::string& name) {
    impl_->remove_stage(name);
}

void RetrievalPipeline::clear_stages() {
    std::lock_guard<std::mutex> lock(impl_->stages_mutex);
    impl_->stages.clear();
    impl_->ordered_stages.clear();
}

void RetrievalPipeline::set_query_classifier(std::shared_ptr<QueryClassifier> classifier) {
    impl_->classifier = classifier;
}

void RetrievalPipeline::set_cache(std::shared_ptr<RetrievalCache> cache) {
    impl_->cache = cache;
}

QueryType RetrievalPipeline::classify_query(const std::string& query) {
    if (impl_->classifier) {
        return impl_->classifier->classify(query);
    }
    // 默认返回 FACTUAL
    return QueryType::FACTUAL;
}

PipelineResult RetrievalPipeline::execute(const std::string& query, int top_k) {
    auto start = std::chrono::steady_clock::now();

    PipelineResult result;
    result.trace_id = generate_uuid();

    // 1. 查询分类
    QueryType query_type = classify_query(query);
    result.query_type = query_type;

    // 更新统计
    {
        std::lock_guard<std::mutex> lock(impl_->stats_mutex);
        impl_->stats.total_queries++;
        impl_->stats.query_type_counts[query_type_to_string(query_type)]++;
    }

    // 2. 检查缓存
    std::string cache_key = query + ":" + std::to_string(top_k) + ":" + query_type_to_string(query_type);
    if (impl_->cache) {
        auto cached = impl_->cache->get(cache_key);
        if (cached) {
            result.results = cached->results;
            result.from_cache = true;
            {
                std::lock_guard<std::mutex> lock(impl_->stats_mutex);
                impl_->stats.cache_hits++;
            }
            result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            return result;
        }
    }

    // 3. 检查是否需要检索
    if (impl_->classifier && !impl_->classifier->needs_retrieval(query_type)) {
        // CHAT 类型，跳过检索
        result.success = true;
        result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        return result;
    }

    // 4. 构建 StageInput
    StageInput input;
    input.query = query;
    input.query_type = query_type;
    input.top_k = top_k;
    input.cache_key = cache_key;

    // 5. 按顺序执行 stages
    std::vector<RetrievalResult> current_results;

    {
        std::lock_guard<std::mutex> lock(impl_->stages_mutex);
        for (auto& [order, stage] : impl_->ordered_stages) {
            if (!stage->is_ready() || !stage->supports(query_type)) {
                continue;
            }

            auto stage_start = std::chrono::steady_clock::now();

            StageOutput output = stage->process(input);

            auto stage_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - stage_start).count();

            // 更新 stage 统计
            {
                std::lock_guard<std::mutex> lock(impl_->stats_mutex);
                impl_->stats.stage_times_ms[stage->name()] += stage_time;
            }

            // 检查输出状态
            if (output.status == StageOutput::Status::STOP) {
                break;
            }

            if (output.status == StageOutput::Status::SUCCESS) {
                current_results = output.results;
                input.candidates = current_results;
            }

            // 检查 next_action
            if (output.next_action == "stop") {
                break;
            } else if (output.next_action == "retry") {
                // 重试：重新执行当前 stage
                output = stage->process(input);
            }
        }
    }

    // 6. 结果后处理
    result.results = current_results;
    result.success = true;

    // 7. 写入缓存
    if (impl_->cache) {
        StageOutput cache_output;
        cache_output.status = StageOutput::Status::SUCCESS;
        cache_output.results = current_results;
        impl_->cache->put(cache_key, cache_output);
    }

    // 8. 计算总时间
    result.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    return result;
}

std::future<PipelineResult> RetrievalPipeline::execute_async(const std::string& query, int top_k) {
    return std::async(std::launch::async, [this, query, top_k]() {
        return execute(query, top_k);
    });
}

RetrievalPipeline::PipelineStats RetrievalPipeline::get_stats() const {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    PipelineStats stats = impl_->stats;
    return stats;
}

void RetrievalPipeline::reset_stats() {
    std::lock_guard<std::mutex> lock(impl_->stats_mutex);
    impl_->stats = PipelineStats();
}

bool RetrievalPipeline::is_healthy() const {
    std::lock_guard<std::mutex> lock(impl_->stages_mutex);
    for (auto& stage : impl_->stages) {
        if (!stage->is_ready()) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> RetrievalPipeline::get_unhealthy_stages() const {
    std::vector<std::string> unhealthy;
    std::lock_guard<std::mutex> lock(impl_->stages_mutex);
    for (auto& stage : impl_->stages) {
        if (!stage->is_ready()) {
            unhealthy.push_back(stage->name());
        }
    }
    return unhealthy;
}

// ========== Utility Functions ==========

std::string query_type_to_string(QueryType type) {
    switch (type) {
        case QueryType::FACTUAL: return "factual";
        case QueryType::ANALYTICAL: return "analytical";
        case QueryType::COMPARATIVE: return "comparative";
        case QueryType::SUMMARY: return "summary";
        case QueryType::CHAT: return "chat";
        case QueryType::MULTI_HOP: return "multi_hop";
        default: return "unknown";
    }
}

QueryType string_to_query_type(const std::string& str) {
    if (str == "factual") return QueryType::FACTUAL;
    if (str == "analytical") return QueryType::ANALYTICAL;
    if (str == "comparative") return QueryType::COMPARATIVE;
    if (str == "summary") return QueryType::SUMMARY;
    if (str == "chat") return QueryType::CHAT;
    if (str == "multi_hop") return QueryType::MULTI_HOP;
    return QueryType::FACTUAL;
}

std::string stage_type_to_string(StageType type) {
    switch (type) {
        case StageType::QUERY_CLASSIFICATION: return "query_classification";
        case StageType::QUERY_EXPANSION: return "query_expansion";
        case StageType::RETRIEVAL: return "retrieval";
        case StageType::RERANKING: return "reranking";
        case StageType::DIVERSITY: return "diversity";
        case StageType::SELF_RAG: return "self_rag";
        case StageType::GRAPH_RAG: return "graph_rag";
        case StageType::MULTIMODAL: return "multimodal";
        case StageType::CUSTOM: return "custom";
        default: return "unknown";
    }
}

// ========== PipelineBuilder ==========

struct PipelineBuilder::Impl {
    std::shared_ptr<QueryClassifier> classifier;
    std::shared_ptr<RetrievalCache> cache;
    std::vector<std::pair<std::shared_ptr<RetrievalStage>, int>> stages;
    Config config;
};

PipelineBuilder::PipelineBuilder() : impl_(std::make_unique<Impl>()) {}
PipelineBuilder::~PipelineBuilder() = default;

PipelineBuilder& PipelineBuilder::with_classifier(std::shared_ptr<QueryClassifier> classifier) {
    impl_->classifier = classifier;
    return *this;
}

PipelineBuilder& PipelineBuilder::with_cache(std::shared_ptr<RetrievalCache> cache) {
    impl_->cache = cache;
    return *this;
}

PipelineBuilder& PipelineBuilder::add_stage(std::shared_ptr<RetrievalStage> stage, int order) {
    impl_->stages.push_back({stage, order});
    return *this;
}

PipelineBuilder& PipelineBuilder::configure(const Config& config) {
    impl_->config = config;
    return *this;
}

std::unique_ptr<RetrievalPipeline> PipelineBuilder::build() {
    auto pipeline = std::make_unique<RetrievalPipeline>();
    pipeline->configure(impl_->config);

    if (impl_->classifier) {
        pipeline->set_query_classifier(impl_->classifier);
    }
    if (impl_->cache) {
        pipeline->set_cache(impl_->cache);
    }

    for (auto& [stage, order] : impl_->stages) {
        pipeline->add_stage(stage, order);
    }

    return pipeline;
}

// ========== Create Default Pipeline ==========

std::unique_ptr<RetrievalPipeline> create_default_pipeline(const Config& config) {
    PipelineBuilder builder;
    builder.configure(config);

    // TODO: 添加默认的 stages
    // - QueryExpansionStage
    // - HybridRetrievalStage
    // - RerankingStage
    // - DiversityStage

    return builder.build();
}

}  // namespace rag
