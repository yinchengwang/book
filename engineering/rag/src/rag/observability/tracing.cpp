/**
 * @file tracing.cpp
 * @brief 可观测性实现
 */

#include "rag/tracing.h"
#include "rag/logger.h"
#include <algorithm>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace rag {

// ========== Span ==========

void Span::add_attribute(const std::string& key, const std::string& value) {
    attributes.push_back({key, value});
}

void Span::add_attribute(const std::string& key, int value) {
    attributes.push_back({key, value});
}

void Span::add_attribute(const std::string& key, float value) {
    attributes.push_back({key, static_cast<double>(value)});
}

void Span::add_event(const std::string& name,
                     const std::unordered_map<std::string, std::string>& attrs) {
    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    int64_t ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    events.push_back({name, ns, attrs});
}

void Span::set_status(SpanStatus status, const std::string& message) {
    this->status = status;
    status_message = message;
}

// ========== RAGTracer ==========

std::shared_ptr<Span> RAGTracer::start_span(
    const std::string& name,
    const std::string& trace_id,
    const std::string& parent_span_id) {

    if (!should_sample()) {
        return nullptr;
    }

    auto span = std::make_shared<Span>();
    span->name = name;

    // 生成或使用传入的 trace_id
    if (trace_id.empty()) {
        span->trace_id = generate_id();
    } else {
        span->trace_id = trace_id;
    }

    span->parent_span_id = parent_span_id;
    span->span_id = generate_id();

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    span->start_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    // 存储 span
    trace_spans_[span->trace_id].push_back(span);

    if (config_.export_to_console) {
        RAG_DEBUG("Span started: " + name + " (trace: " + span->trace_id + ")");
    }

    return span;
}

void RAGTracer::end_span(std::shared_ptr<Span> span) {
    if (!span) return;

    auto now = std::chrono::system_clock::now();
    auto duration = now.time_since_epoch();
    span->end_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

    if (config_.export_to_console) {
        RAG_DEBUG("Span ended: " + span->name +
                  " (duration: " + std::to_string(span->duration_ms()) + "ms)");
    }
}

void RAGTracer::add_event(std::shared_ptr<Span> span,
                          const std::string& event_name,
                          const std::unordered_map<std::string, std::string>& attrs) {
    if (span) {
        span->add_event(event_name, attrs);
    }
}

std::shared_ptr<Span> RAGTracer::trace_query_classification(
    const std::string& query,
    QueryType type) {

    auto span = start_span("query_classification");
    if (span) {
        span->add_attribute("query_type", query_type_to_string(type));
        span->add_attribute("query_length", static_cast<int>(query.length()));
    }
    return span;
}

std::shared_ptr<Span> RAGTracer::trace_retrieval(
    const std::string& query,
    int recall_count,
    int retrieval_ms) {

    auto span = start_span("retrieval");
    if (span) {
        span->add_attribute("recall_count", recall_count);
        span->add_attribute("retrieval_ms", retrieval_ms);
    }
    return span;
}

std::shared_ptr<Span> RAGTracer::trace_rerank(
    const std::string& query,
    int candidate_count,
    int final_count,
    int rerank_ms) {

    auto span = start_span("rerank");
    if (span) {
        span->add_attribute("candidate_count", candidate_count);
        span->add_attribute("final_count", final_count);
        span->add_attribute("rerank_ms", rerank_ms);
    }
    return span;
}

std::shared_ptr<Span> RAGTracer::trace_generation(
    const std::string& prompt,
    const std::string& response,
    int generation_ms) {

    auto span = start_span("generation");
    if (span) {
        span->add_attribute("prompt_length", static_cast<int>(prompt.length()));
        span->add_attribute("response_length", static_cast<int>(response.length()));
        span->add_attribute("generation_ms", generation_ms);
    }
    return span;
}

void RAGTracer::configure(const Config& config) {
    config_ = config;
}

std::string RAGTracer::generate_id() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis(0, UINT64_MAX);

    uint64_t id = dis(gen);
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << id;
    return ss.str();
}

bool RAGTracer::should_sample() {
    if (!config_.enabled) return false;

    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_real_distribution<double> dis(0.0, 1.0);

    return dis(gen) < config_.sampling_rate;
}

// ========== RetrievalMetrics ==========

void RetrievalMetrics::reset() {
    query_latency_ms.reset();
    retrieval_latency_ms.reset();
    rerank_latency_ms.reset();
    generation_latency_ms.reset();

    queries_total.reset();
    active_queries.set(0);

    retrieval_empty.reset();
    retrieval_scores.reset();
    rerank_score_improvement.reset();

    cache_hits.reset();
    cache_misses.reset();

    modality_text.reset();
    modality_table.reset();
    modality_image.reset();
    modality_video.reset();
    modality_audio.reset();

    errors_total.reset();
    errors_by_type.clear();
}

// ========== MetricsCollector ==========

void MetricsCollector::record_query_latency(double ms) {
    metrics_.query_latency_ms.observe(ms);
}

void MetricsCollector::record_retrieval_latency(double ms) {
    metrics_.retrieval_latency_ms.observe(ms);
}

void MetricsCollector::record_rerank_latency(double ms) {
    metrics_.rerank_latency_ms.observe(ms);
}

void MetricsCollector::record_generation_latency(double ms) {
    metrics_.generation_latency_ms.observe(ms);
}

void MetricsCollector::increment_queries() {
    metrics_.queries_total.inc();
}

void MetricsCollector::set_active_queries(int count) {
    metrics_.active_queries.set(count);
}

void MetricsCollector::increment_cache_hit() {
    metrics_.cache_hits.inc();
}

void MetricsCollector::increment_cache_miss() {
    metrics_.cache_misses.inc();
}

void MetricsCollector::record_retrieval_score(double score) {
    metrics_.retrieval_scores.observe(score);
}

void MetricsCollector::record_rerank_improvement(double improvement) {
    metrics_.rerank_score_improvement.observe(improvement);
}

void MetricsCollector::increment_modality(const std::string& modality) {
    if (modality == "text") {
        metrics_.modality_text.inc();
    } else if (modality == "table") {
        metrics_.modality_table.inc();
    } else if (modality == "image") {
        metrics_.modality_image.inc();
    } else if (modality == "video") {
        metrics_.modality_video.inc();
    } else if (modality == "audio") {
        metrics_.modality_audio.inc();
    }
}

void MetricsCollector::record_error(const std::string& error_type) {
    metrics_.errors_total.inc();
    metrics_.errors_by_type[error_type].inc();
}

std::string MetricsCollector::export_prometheus() const {
    std::ostringstream ss;

    // 延迟指标
    ss << "# HELP rag_query_latency_ms Query latency in milliseconds\n";
    ss << "# TYPE rag_query_latency_ms histogram\n";
    ss << "rag_query_latency_ms{quantile=\"0.5\"} " << metrics_.query_latency_ms.p50() << "\n";
    ss << "rag_query_latency_ms{quantile=\"0.95\"} " << metrics_.query_latency_ms.p95() << "\n";
    ss << "rag_query_latency_ms{quantile=\"0.99\"} " << metrics_.query_latency_ms.p99() << "\n";

    ss << "# HELP rag_retrieval_latency_ms Retrieval latency in milliseconds\n";
    ss << "# TYPE rag_retrieval_latency_ms histogram\n";
    ss << "rag_retrieval_latency_ms{quantile=\"0.5\"} " << metrics_.retrieval_latency_ms.p50() << "\n";
    ss << "rag_retrieval_latency_ms{quantile=\"0.95\"} " << metrics_.retrieval_latency_ms.p95() << "\n";

    ss << "# HELP rag_rerank_latency_ms Rerank latency in milliseconds\n";
    ss << "# TYPE rag_rerank_latency_ms histogram\n";
    ss << "rag_rerank_latency_ms{quantile=\"0.5\"} " << metrics_.rerank_latency_ms.p50() << "\n";

    // 吞吐量
    ss << "# HELP rag_queries_total Total number of queries\n";
    ss << "# TYPE rag_queries_total counter\n";
    ss << "rag_queries_total " << metrics_.queries_total.value() << "\n";

    ss << "# HELP rag_active_queries Number of active queries\n";
    ss << "# TYPE rag_active_queries gauge\n";
    ss << "rag_active_queries " << metrics_.active_queries.value() << "\n";

    // 缓存
    ss << "# HELP rag_cache_hits_total Total cache hits\n";
    ss << "# TYPE rag_cache_hits_total counter\n";
    ss << "rag_cache_hits_total " << metrics_.cache_hits.value() << "\n";

    ss << "# HELP rag_cache_misses_total Total cache misses\n";
    ss << "# TYPE rag_cache_misses_total counter\n";
    ss << "rag_cache_misses_total " << metrics_.cache_misses.value() << "\n";

    // 多模态
    ss << "# HELP rag_modality_total Total retrievals by modality\n";
    ss << "# TYPE rag_modality_total counter\n";
    ss << "rag_modality_total{modality=\"text\"} " << metrics_.modality_text.value() << "\n";
    ss << "rag_modality_total{modality=\"table\"} " << metrics_.modality_table.value() << "\n";
    ss << "rag_modality_total{modality=\"image\"} " << metrics_.modality_image.value() << "\n";
    ss << "rag_modality_total{modality=\"video\"} " << metrics_.modality_video.value() << "\n";
    ss << "rag_modality_total{modality=\"audio\"} " << metrics_.modality_audio.value() << "\n";

    // 错误
    ss << "# HELP rag_errors_total Total errors\n";
    ss << "# TYPE rag_errors_total counter\n";
    ss << "rag_errors_total " << metrics_.errors_total.value() << "\n";

    for (const auto& [type, counter] : metrics_.errors_by_type) {
        ss << "rag_errors_total{type=\"" << type << "\"} " << counter.value() << "\n";
    }

    return ss.str();
}

std::string MetricsCollector::export_json() const {
    std::ostringstream ss;
    ss << "{\n";

    // Latency
    ss << "  \"latency\": {\n";
    ss << "    \"query\": {\"p50\": " << metrics_.query_latency_ms.p50()
       << ", \"p95\": " << metrics_.query_latency_ms.p95()
       << ", \"p99\": " << metrics_.query_latency_ms.p99() << "},\n";
    ss << "    \"retrieval\": {\"p50\": " << metrics_.retrieval_latency_ms.p50()
       << ", \"p95\": " << metrics_.retrieval_latency_ms.p95() << "},\n";
    ss << "    \"rerank\": {\"p50\": " << metrics_.rerank_latency_ms.p50() << "}\n";
    ss << "  },\n";

    // Throughput
    ss << "  \"throughput\": {\n";
    ss << "    \"queries_total\": " << metrics_.queries_total.value()
       << ", \"active_queries\": " << metrics_.active_queries.value() << "\n";
    ss << "  },\n";

    // Cache
    ss << "  \"cache\": {\n";
    ss << "    \"hits\": " << metrics_.cache_hits.value()
       << ", \"misses\": " << metrics_.cache_misses.value();
    double hit_rate = 0.0;
    double total = metrics_.cache_hits.value() + metrics_.cache_misses.value();
    if (total > 0) {
        hit_rate = metrics_.cache_hits.value() / total;
    }
    ss << ", \"hit_rate\": " << hit_rate << "\n";
    ss << "  },\n";

    // Modality
    ss << "  \"modality\": {\n";
    ss << "    \"text\": " << metrics_.modality_text.value()
       << ", \"table\": " << metrics_.modality_table.value()
       << ", \"image\": " << metrics_.modality_image.value()
       << ", \"video\": " << metrics_.modality_video.value()
       << ", \"audio\": " << metrics_.modality_audio.value() << "\n";
    ss << "  },\n";

    // Errors
    ss << "  \"errors\": {\n";
    ss << "    \"total\": " << metrics_.errors_total.value()
       << ", \"by_type\": {";
    bool first = true;
    for (const auto& [type, counter] : metrics_.errors_by_type) {
        if (!first) ss << ", ";
        ss << "\"" << type << "\": " << counter.value();
        first = false;
    }
    ss << "}}\n";

    ss << "}\n";
    return ss.str();
}

void MetricsCollector::configure(const Config& config) {
    config_ = config;
}

// ========== PipelineObserver ==========

PipelineObserver::PipelineObserver(
    std::shared_ptr<RetrievalPipeline> pipeline,
    std::shared_ptr<RAGTracer> tracer,
    std::shared_ptr<MetricsCollector> metrics)
    : pipeline_(pipeline), tracer_(tracer), metrics_(metrics) {}

PipelineResult PipelineObserver::execute_with_observation(
    const std::string& query,
    int top_k) {

    auto start = std::chrono::steady_clock::now();

    // 记录活动查询
    if (metrics_) {
        metrics_->increment_queries();
        metrics_->set_active_queries(
            static_cast<int>(metrics_->metrics().active_queries.value()) + 1);
    }

    PipelineResult result;

    try {
        // 执行查询
        result = pipeline_->execute(query, top_k);

        // 记录延迟
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();

        if (metrics_) {
            metrics_->record_query_latency(latency);

            if (result.from_cache) {
                metrics_->increment_cache_hit();
            } else {
                metrics_->increment_cache_miss();
            }

            if (result.results.empty()) {
                metrics_->record_error("empty_results");
            }
        }

    } catch (const std::exception& e) {
        if (metrics_) {
            metrics_->record_error("exception");
        }
        RAG_ERROR("Pipeline execution error: " + std::string(e.what()));
        result.success = false;
    }

    // 更新活动查询数
    if (metrics_) {
        metrics_->set_active_queries(
            static_cast<int>(metrics_->metrics().active_queries.value()) - 1);
    }

    return result;
}

std::future<PipelineResult> PipelineObserver::execute_async_with_observation(
    const std::string& query,
    int top_k) {

    return std::async(std::launch::async, [this, query, top_k]() {
        return execute_with_observation(query, top_k);
    });
}

void PipelineObserver::set_tracer(std::shared_ptr<RAGTracer> tracer) {
    tracer_ = tracer;
}

void PipelineObserver::set_metrics(std::shared_ptr<MetricsCollector> metrics) {
    metrics_ = metrics;
}

// ========== Factory ==========

std::shared_ptr<RAGTracer> create_tracer(const RAGTracer::Config& config) {
    auto tracer = std::make_shared<RAGTracer>();
    tracer->configure(config);
    return tracer;
}

std::shared_ptr<MetricsCollector> create_metrics_collector(
    const MetricsCollector::Config& config) {

    auto collector = std::make_shared<MetricsCollector>();
    collector->configure(config);
    return collector;
}

std::shared_ptr<PipelineObserver> create_pipeline_observer(
    std::shared_ptr<RetrievalPipeline> pipeline,
    std::shared_ptr<RAGTracer> tracer,
    std::shared_ptr<MetricsCollector> metrics) {

    return std::make_shared<PipelineObserver>(pipeline, tracer, metrics);
}

}  // namespace rag
