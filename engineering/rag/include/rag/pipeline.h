/**
 * @file pipeline.h
 * @brief RAG 检索 Pipeline 抽象
 *
 * 提供可插拔的 Stage 编排框架，支持:
 * - Query Classification / Routing
 * - Query Expansion (HyDE, Synonym)
 * - Retrieval (Vector, BM25, Graph, Multimodal)
 * - Reranking
 * - Diversity (MMR)
 * - Self-RAG (Self-Evaluation)
 */
#pragma once

#include "rag/types.h"
#include "rag/config.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <variant>
#include <future>

namespace rag {

// ========== Forward Declarations ==========

class RetrievalStage;
class QueryClassifier;
class RetrievalCache;

// ========== Stage Types ==========

enum class StageType {
    QUERY_CLASSIFICATION,
    QUERY_EXPANSION,
    RETRIEVAL,
    RERANKING,
    DIVERSITY,
    SELF_RAG,
    GRAPH_RAG,
    MULTIMODAL,
    CUSTOM
};

// ========== Query Types ==========

enum class QueryType {
    FACTUAL,        // 事实型问题 - 需要精确检索
    ANALYTICAL,     // 分析型问题 - 需要综合多源
    COMPARATIVE,    // 比较型问题 - 需要对比分析
    SUMMARY,        // 总结型问题 - 需要摘要生成
    CHAT,           // 闲聊型 - 可能不需要检索
    MULTI_HOP       // 多跳问题 - 需要 Graph 检索
};

// ========== Stage Input/Output ==========

struct StageInput {
    std::string query;
    QueryType query_type = QueryType::FACTUAL;
    std::vector<RetrievalResult> candidates;
    int top_k = 5;
    std::unordered_map<std::string, std::string> metadata;

    // 缓存相关
    std::string cache_key;
    bool skip_cache = false;
};

struct StageOutput {
    enum class Status {
        SUCCESS,
        SKIPPED,
        FAILED,
        RETRY,
        STOP  // 用于终止 Pipeline
    };

    Status status = Status::SUCCESS;
    std::vector<RetrievalResult> results;
    std::string next_action;  // "continue", "retry", "stop"
    std::unordered_map<std::string, std::string> metadata;
};

struct PipelineResult {
    bool success = false;
    std::vector<RetrievalResult> results;
    std::string answer;
    float confidence = 0.0f;
    int64_t total_time_ms = 0;

    // 各阶段耗时
    std::unordered_map<std::string, int64_t> stage_times;

    // Pipeline 元数据
    QueryType query_type = QueryType::FACTUAL;
    bool from_cache = false;
    std::string trace_id;
};

// ========== Stage Configuration ==========

struct StageConfig {
    std::string name;
    StageType type = StageType::CUSTOM;
    int order = 0;  // 执行顺序，-1 表示禁用

    // 通用配置
    bool enabled = true;
    bool parallel_execution = false;

    // Stage 特定配置 (JSON)
    std::string config_json;
};

// ========== Retrieval Stage Interface ==========

/**
 * @brief 检索 Pipeline Stage 接口
 *
 * 所有 Pipeline 组件都实现此接口
 */
class RetrievalStage {
public:
    virtual ~RetrievalStage() = default;

    // 基础信息
    virtual std::string name() const = 0;
    virtual StageType type() const = 0;
    virtual std::string description() const { return ""; }

    // 初始化
    virtual bool init(const StageConfig& config) { return true; }
    virtual void shutdown() {}

    // 处理输入
    virtual StageOutput process(const StageInput& input) = 0;

    // 查询类型支持
    virtual bool supports(QueryType type) const { return true; }
    virtual std::vector<QueryType> supported_types() const {
        return {
            QueryType::FACTUAL,
            QueryType::ANALYTICAL,
            QueryType::COMPARATIVE,
            QueryType::SUMMARY,
            QueryType::CHAT,
            QueryType::MULTI_HOP
        };
    }

    // 健康检查
    virtual bool is_ready() const { return true; }

    // 统计信息
    struct Stats {
        uint64_t total_calls = 0;
        uint64_t total_errors = 0;
        uint64_t cache_hits = 0;
        uint64_t cache_misses = 0;
        double avg_time_ms = 0.0;
    };

    virtual Stats get_stats() const { return {}; }
    virtual void reset_stats() {}

protected:
    // 辅助方法
    StageOutput create_success(const std::vector<RetrievalResult>& results);
    StageOutput create_skipped();
    StageOutput create_failure(const std::string& reason);
};

// ========== Retrieval Pipeline ==========

/**
 * @brief 检索 Pipeline
 *
 * 编排多个 Stage，按顺序执行
 */
class RetrievalPipeline {
public:
    RetrievalPipeline();
    ~RetrievalPipeline();

    // 禁止拷贝
    RetrievalPipeline(const RetrievalPipeline&) = delete;
    RetrievalPipeline& operator=(const RetrievalPipeline&) = delete;

    // 配置
    void configure(const Config& config);

    // Stage 管理
    void add_stage(std::shared_ptr<RetrievalStage> stage, int order = -1);
    void remove_stage(const std::string& name);
    void clear_stages();

    // 设置组件
    void set_query_classifier(std::shared_ptr<QueryClassifier> classifier);
    void set_cache(std::shared_ptr<RetrievalCache> cache);

    // 执行 Pipeline
    PipelineResult execute(const std::string& query, int top_k = 5);

    // 异步执行
    std::future<PipelineResult> execute_async(const std::string& query, int top_k = 5);

    // 查询类型
    QueryType classify_query(const std::string& query);

    // 统计
    struct PipelineStats {
        uint64_t total_queries = 0;
        uint64_t cache_hits = 0;
        uint64_t cache_misses = 0;
        std::unordered_map<std::string, uint64_t> query_type_counts;
        std::unordered_map<std::string, double> avg_stage_times_ms;
    };

    PipelineStats get_stats() const;
    void reset_stats();

    // 健康检查
    bool is_healthy() const;
    std::vector<std::string> get_unhealthy_stages() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ========== Query Classifier ==========

/**
 * @brief 查询分类器
 */
class QueryClassifier {
public:
    virtual ~QueryClassifier() = default;

    virtual QueryType classify(const std::string& query) = 0;
    virtual std::pair<QueryType, float> classify_with_confidence(const std::string& query) = 0;

    // 提取关键词
    virtual std::vector<std::string> extract_keywords(const std::string& query) = 0;

    // 检测是否需要检索
    virtual bool needs_retrieval(QueryType type) const {
        return type != QueryType::CHAT;
    }
};

// ========== Retrieval Cache ==========

/**
 * @brief 检索缓存接口
 */
class RetrievalCache {
public:
    virtual ~RetrievalCache() = default;

    // 查询缓存
    virtual std::optional<StageOutput> get(const std::string& key) = 0;
    virtual void put(const std::string& key, const StageOutput& output) = 0;
    virtual void invalidate(const std::string& key) = 0;
    virtual void clear() = 0;

    // 统计
    virtual size_t size() const = 0;
    virtual size_t hits() const = 0;
    virtual size_t misses() const = 0;
};

// ========== Factory Functions ==========

/**
 * @brief 创建默认 Pipeline
 */
std::unique_ptr<RetrievalPipeline> create_default_pipeline(const Config& config);

/**
 * @brief 创建 Pipeline Builder
 */
class PipelineBuilder {
public:
    PipelineBuilder& with_classifier(std::shared_ptr<QueryClassifier> classifier);
    PipelineBuilder& with_cache(std::shared_ptr<RetrievalCache> cache);
    PipelineBuilder& add_stage(std::shared_ptr<RetrievalStage> stage, int order = -1);
    PipelineBuilder& configure(const Config& config);
    std::unique_ptr<RetrievalPipeline> build();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// ========== Utility Functions ==========

std::string query_type_to_string(QueryType type);
QueryType string_to_query_type(const std::string& str);
std::string stage_type_to_string(StageType type);

// ========== Inline Implementations ==========

inline StageOutput RetrievalStage::create_success(const std::vector<RetrievalResult>& results) {
    StageOutput output;
    output.status = StageOutput::Status::SUCCESS;
    output.results = results;
    output.next_action = "continue";
    return output;
}

inline StageOutput RetrievalStage::create_skipped() {
    StageOutput output;
    output.status = StageOutput::Status::SKIPPED;
    output.next_action = "continue";
    return output;
}

inline StageOutput RetrievalStage::create_failure(const std::string& reason) {
    StageOutput output;
    output.status = StageOutput::Status::FAILED;
    output.metadata["error"] = reason;
    output.next_action = "stop";
    return output;
}

}  // namespace rag
