/**
 * @file eval_store.cpp
 * @brief 评估运行存储实现
 */

#include "mmrag/eval/eval_store.h"
#include "mmrag/logger.h"
#include "mmrag/types.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <filesystem>

#include <nlohmann/json.hpp>
using nlohmann::json;

namespace fs = std::filesystem;

namespace mmrag {
namespace eval {

// ========== 辅助函数 ==========

namespace {

int64_t now_ms() {
    return mmrag::current_timestamp_ms();
}

std::string format_run_timestamp(int64_t timestamp_ms) {
    std::time_t t = static_cast<std::time_t>(timestamp_ms / 1000);
    std::tm* tm_info = std::localtime(&t);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", tm_info);
    return std::string(buf);
}

std::string iso8601(int64_t timestamp_ms) {
    std::time_t t = static_cast<std::time_t>(timestamp_ms / 1000);
    std::tm* tm_info = std::gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    return std::string(buf);
}

}  // anonymous namespace

// ========== EvalRunStore 实现 ==========

EvalRunStore::EvalRunStore(const std::string& base_path)
    : base_path_(base_path) {
    fs::create_directories(base_path_);
}

std::string EvalRunStore::get_run_path(const std::string& run_id) const {
    return base_path_ + "/" + run_id + ".json";
}

std::string EvalRunStore::generate_run_id() {
    return "run_" + format_run_timestamp(now_ms());
}

bool EvalRunStore::save_run(const EvalRunRecord& record) {
    try {
        std::string path = get_run_path(record.run_id);
        std::ofstream file(path);
        if (!file.is_open()) {
            RAG_ERROR("Failed to open run file: " + path);
            return false;
        }

        file << export_to_json(record);
        file.close();

        RAG_INFO("Saved eval run " + record.run_id);
        return true;
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to save eval run: ") + e.what());
        return false;
    }
}

std::optional<EvalRunRecord> EvalRunStore::load_run(const std::string& run_id) {
    try {
        std::string path = get_run_path(run_id);
        if (!fs::exists(path)) {
            RAG_ERROR("Run not found: " + run_id);
            return std::nullopt;
        }

        std::ifstream file(path);
        std::string content((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        file.close();

        return import_from_json(content);
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to load eval run: ") + e.what());
        return std::nullopt;
    }
}

std::vector<EvalRunRecord> EvalRunStore::list_runs(int limit) {
    std::vector<EvalRunRecord> result;

    try {
        if (!fs::exists(base_path_)) return result;

        std::vector<std::string> run_files;
        for (const auto& entry : fs::directory_iterator(base_path_)) {
            if (entry.path().extension() == ".json") {
                run_files.push_back(entry.path().filename().string());
            }
        }

        // 倒序排序（最新的在前）
        std::sort(run_files.begin(), run_files.end(), std::greater<std::string>());

        int count = 0;
        for (const auto& filename : run_files) {
            if (count >= limit) break;
            count++;

            // 提取 run_id
            std::string run_id = filename.substr(0, filename.size() - 5);
            auto record = load_run(run_id);
            if (record.has_value()) {
                result.push_back(record.value());
            }
        }
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to list runs: ") + e.what());
    }

    return result;
}

std::vector<EvalRunRecord> EvalRunStore::list_runs_by_pipeline(
    const std::string& pipeline_type,
    int limit) {
    std::vector<EvalRunRecord> all = list_runs(limit * 2);  // 多取一些用于过滤
    std::vector<EvalRunRecord> filtered;

    for (auto& r : all) {
        if (r.pipeline_type == pipeline_type) {
            filtered.push_back(r);
            if (static_cast<int>(filtered.size()) >= limit) break;
        }
    }

    return filtered;
}

ComparisonResult EvalRunStore::compare_runs(
    const std::string& run_id_a,
    const std::string& run_id_b) {
    ComparisonResult result;
    result.run_ids = {run_id_a, run_id_b};
    result.timestamp = now_ms();

    auto a = load_run(run_id_a);
    auto b = load_run(run_id_b);

    if (!a.has_value() || !b.has_value()) {
        RAG_ERROR("Failed to load runs for comparison");
        return result;
    }

    result.pipeline_types = {a->pipeline_type, b->pipeline_type};
    result.comparison_id = "comp_" + run_id_a + "_vs_" + run_id_b;

    auto compare_metric = [&](const std::string& name, double val_a, double val_b) {
        ComparisonResult::MetricDiff diff;
        diff.metric_name = name;
        diff.value_a = val_a;
        diff.value_b = val_b;
        diff.delta = val_b - val_a;

        if (val_a != 0.0) {
            diff.percent_change = (diff.delta / val_a) * 100.0;
        } else {
            diff.percent_change = 0.0;
        }

        if (std::abs(diff.delta) < 1e-9) {
            diff.direction = "same";
        } else if (diff.delta > 0) {
            diff.direction = "up";
        } else {
            diff.direction = "down";
        }

        result.metric_diffs.push_back(diff);
    };

    // 比较所有关键指标
    compare_metric("precision_at_5", a->summary.avg_precision_at_5, b->summary.avg_precision_at_5);
    compare_metric("precision_at_10", a->summary.avg_precision_at_10, b->summary.avg_precision_at_10);
    compare_metric("recall_at_5", a->summary.avg_recall_at_5, b->summary.avg_recall_at_5);
    compare_metric("recall_at_10", a->summary.avg_recall_at_10, b->summary.avg_recall_at_10);
    compare_metric("mrr", a->summary.mrr, b->summary.mrr);
    compare_metric("ndcg_at_10", a->summary.avg_ndcg_at_10, b->summary.avg_ndcg_at_10);
    compare_metric("hit_rate", a->summary.hit_rate, b->summary.hit_rate);
    compare_metric("map", a->summary.map_score, b->summary.map_score);
    compare_metric("faithfulness", a->summary.avg_faithfulness, b->summary.avg_faithfulness);
    compare_metric("answer_relevancy", a->summary.avg_answer_relevancy, b->summary.avg_answer_relevancy);
    compare_metric("correctness", a->summary.avg_answer_correctness, b->summary.avg_answer_correctness);
    compare_metric("hallucination_rate", a->summary.avg_hallucination_rate, b->summary.avg_hallucination_rate);
    compare_metric("completeness", a->summary.avg_completeness, b->summary.avg_completeness);
    compare_metric("avg_latency_ms", a->summary.avg_latency_ms, b->summary.avg_latency_ms);

    return result;
}

std::vector<std::pair<int64_t, double>> EvalRunStore::get_metric_trend(
    const std::string& pipeline_type,
    const std::string& metric_name,
    int limit) {
    std::vector<std::pair<int64_t, double>> trend;

    auto runs = list_runs_by_pipeline(pipeline_type, limit);

    // 按时间正序排列（早的在前）
    std::reverse(runs.begin(), runs.end());

    for (const auto& run : runs) {
        double value = 0.0;

        if (metric_name == "precision_at_5") value = run.summary.avg_precision_at_5;
        else if (metric_name == "precision_at_10") value = run.summary.avg_precision_at_10;
        else if (metric_name == "recall_at_5") value = run.summary.avg_recall_at_5;
        else if (metric_name == "recall_at_10") value = run.summary.avg_recall_at_10;
        else if (metric_name == "mrr") value = run.summary.mrr;
        else if (metric_name == "ndcg_at_10") value = run.summary.avg_ndcg_at_10;
        else if (metric_name == "hit_rate") value = run.summary.hit_rate;
        else if (metric_name == "map") value = run.summary.map_score;
        else if (metric_name == "faithfulness") value = run.summary.avg_faithfulness;
        else if (metric_name == "answer_relevancy") value = run.summary.avg_answer_relevancy;
        else if (metric_name == "correctness") value = run.summary.avg_answer_correctness;
        else if (metric_name == "hallucination_rate") value = run.summary.avg_hallucination_rate;
        else if (metric_name == "completeness") value = run.summary.avg_completeness;
        else if (metric_name == "avg_latency_ms") value = run.summary.avg_latency_ms;
        else continue;

        trend.emplace_back(run.timestamp, value);
    }

    return trend;
}

bool EvalRunStore::delete_run(const std::string& run_id) {
    try {
        std::string path = get_run_path(run_id);
        if (fs::exists(path)) {
            fs::remove(path);
            RAG_INFO("Deleted run " + run_id);
            return true;
        }
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to delete run: ") + e.what());
    }
    return false;
}

std::string EvalRunStore::export_to_json(const EvalRunRecord& record) const {
    json j;
    j["run_id"] = record.run_id;
    j["dataset_id"] = record.dataset_id;
    j["dataset_version"] = record.dataset_version;
    j["pipeline_type"] = record.pipeline_type;
    j["pipeline_config"] = record.pipeline_config;
    j["timestamp"] = record.timestamp;
    j["iso_time"] = iso8601(record.timestamp);
    j["author"] = record.author;
    j["description"] = record.description;
    j["tags"] = record.tags;

    // 摘要
    auto& summary = record.summary;
    json s;
    s["run_id"] = summary.run_id;
    s["dataset_id"] = summary.dataset_id;
    s["pipeline_type"] = summary.pipeline_type;
    s["timestamp"] = summary.timestamp;

    // 检索指标
    s["metrics"] = {
        {"precision_at_5", summary.avg_precision_at_5},
        {"precision_at_10", summary.avg_precision_at_10},
        {"recall_at_5", summary.avg_recall_at_5},
        {"recall_at_10", summary.avg_recall_at_10},
        {"mrr", summary.mrr},
        {"ndcg_at_10", summary.avg_ndcg_at_10},
        {"hit_rate", summary.hit_rate},
        {"map", summary.map_score},
        {"f1_at_10", summary.avg_f1_at_10},
        // 生成指标
        {"faithfulness", summary.avg_faithfulness},
        {"answer_relevancy", summary.avg_answer_relevancy},
        {"correctness", summary.avg_answer_correctness},
        {"hallucination_rate", summary.avg_hallucination_rate},
        {"completeness", summary.avg_completeness},
        // 性能
        {"avg_latency_ms", summary.avg_latency_ms},
        {"p50_latency_ms", summary.p50_latency_ms},
        {"p99_latency_ms", summary.p99_latency_ms},
        {"avg_token_usage", summary.avg_token_usage}
    };

    s["stats"] = {
        {"total_questions", summary.total_questions},
        {"successful_questions", summary.successful_questions},
        {"failed_questions", summary.failed_questions}
    };

    // 详细信息
    json details = json::array();
    for (const auto& d : summary.details) {
        json dj;
        dj["question_id"] = d.question_id;
        dj["query"] = d.query;
        dj["retrieved_doc_ids"] = d.retrieved_doc_ids;
        dj["retrieval_scores"] = d.retrieval_scores;
        dj["generated_answer"] = d.generated_answer;
        dj["ground_truth"] = d.ground_truth;
        dj["context"] = d.context;
        dj["metrics"] = {
            {"precision_at_5", d.precision_at_5},
            {"precision_at_10", d.precision_at_10},
            {"recall_at_5", d.recall_at_5},
            {"recall_at_10", d.recall_at_10},
            {"reciprocal_rank", d.reciprocal_rank},
            {"ndcg_at_10", d.ndcg_at_10},
            {"hit_rate", d.hit_rate},
            {"map", d.map_score},
            {"f1_at_10", d.f1_at_10},
            {"context_precision", d.context_precision},
            {"context_recall", d.context_recall},
            {"faithfulness", d.faithfulness},
            {"answer_relevancy", d.answer_relevancy},
            {"answer_correctness", d.answer_correctness},
            {"hallucination_rate", d.hallucination_rate},
            {"completeness", d.completeness}
        };
        dj["performance"] = {
            {"latency_ms", d.latency_ms},
            {"token_usage", d.token_usage}
        };
        dj["error_message"] = d.error_message;
        details.push_back(dj);
    }
    s["details"] = details;

    j["summary"] = s;

    return j.dump(2);
}

std::optional<EvalRunRecord> EvalRunStore::import_from_json(
    const std::string& json_content) const {
    try {
        json j = json::parse(json_content);
        EvalRunRecord record;
        record.run_id = j.value("run_id", "");
        record.dataset_id = j.value("dataset_id", "");
        record.dataset_version = j.value("dataset_version", "");
        record.pipeline_type = j.value("pipeline_type", "");
        record.pipeline_config = j.value("pipeline_config", "");
        record.timestamp = j.value("timestamp", static_cast<int64_t>(0));
        record.author = j.value("author", "");
        record.description = j.value("description", "");
        record.tags = j.value("tags", std::vector<std::string>{});

        if (j.contains("summary")) {
            const auto& s = j["summary"];
            auto& summary = record.summary;
            summary.run_id = s.value("run_id", "");
            summary.dataset_id = s.value("dataset_id", "");
            summary.pipeline_type = s.value("pipeline_type", "");
            summary.timestamp = s.value("timestamp", static_cast<int64_t>(0));
            summary.pipeline_config_snapshot = record.pipeline_config;

            // 检索指标
            const auto& metrics = s["metrics"];
            summary.avg_precision_at_5 = metrics.value("precision_at_5", 0.0);
            summary.avg_precision_at_10 = metrics.value("precision_at_10", 0.0);
            summary.avg_recall_at_5 = metrics.value("recall_at_5", 0.0);
            summary.avg_recall_at_10 = metrics.value("recall_at_10", 0.0);
            summary.mrr = metrics.value("mrr", 0.0);
            summary.avg_ndcg_at_10 = metrics.value("ndcg_at_10", 0.0);
            summary.hit_rate = metrics.value("hit_rate", 0.0);
            summary.map_score = metrics.value("map", 0.0);
            summary.avg_f1_at_10 = metrics.value("f1_at_10", 0.0);

            // 生成指标
            summary.avg_faithfulness = metrics.value("faithfulness", 0.0);
            summary.avg_answer_relevancy = metrics.value("answer_relevancy", 0.0);
            summary.avg_answer_correctness = metrics.value("correctness", 0.0);
            summary.avg_hallucination_rate = metrics.value("hallucination_rate", 0.0);
            summary.avg_completeness = metrics.value("completeness", 0.0);

            // 性能
            summary.avg_latency_ms = metrics.value("avg_latency_ms", 0.0);
            summary.p50_latency_ms = metrics.value("p50_latency_ms", 0.0);
            summary.p99_latency_ms = metrics.value("p99_latency_ms", 0.0);
            summary.avg_token_usage = metrics.value("avg_token_usage", 0.0);

            // 统计
            if (s.contains("stats")) {
                const auto& stats = s["stats"];
                summary.total_questions = stats.value("total_questions", 0);
                summary.successful_questions = stats.value("successful_questions", 0);
                summary.failed_questions = stats.value("failed_questions", 0);
            }

            // 详细信息
            if (s.contains("details")) {
                for (const auto& dj : s["details"]) {
                    QuestionResult d;
                    d.question_id = dj.value("question_id", "");
                    d.query = dj.value("query", "");
                    d.generated_answer = dj.value("generated_answer", "");
                    d.ground_truth = dj.value("ground_truth", "");
                    d.error_message = dj.value("error_message", "");

                    if (dj.contains("retrieved_doc_ids")) {
                        for (const auto& v : dj["retrieved_doc_ids"]) {
                            d.retrieved_doc_ids.push_back(v.get<std::string>());
                        }
                    }

                    if (dj.contains("retrieval_scores")) {
                        for (const auto& v : dj["retrieval_scores"]) {
                            d.retrieval_scores.push_back(v.get<double>());
                        }
                    }

                    if (dj.contains("context")) {
                        for (const auto& v : dj["context"]) {
                            d.context.push_back(v.get<std::string>());
                        }
                    }

                    if (dj.contains("metrics")) {
                        const auto& m = dj["metrics"];
                        d.precision_at_5 = m.value("precision_at_5", 0.0);
                        d.precision_at_10 = m.value("precision_at_10", 0.0);
                        d.recall_at_5 = m.value("recall_at_5", 0.0);
                        d.recall_at_10 = m.value("recall_at_10", 0.0);
                        d.reciprocal_rank = m.value("reciprocal_rank", 0.0);
                        d.ndcg_at_10 = m.value("ndcg_at_10", 0.0);
                        d.hit_rate = m.value("hit_rate", 0.0);
                        d.map_score = m.value("map", 0.0);
                        d.f1_at_10 = m.value("f1_at_10", 0.0);
                        d.context_precision = m.value("context_precision", 0.0);
                        d.context_recall = m.value("context_recall", 0.0);
                        d.faithfulness = m.value("faithfulness", 0.0);
                        d.answer_relevancy = m.value("answer_relevancy", 0.0);
                        d.answer_correctness = m.value("answer_correctness", 0.0);
                        d.hallucination_rate = m.value("hallucination_rate", 0.0);
                        d.completeness = m.value("completeness", 0.0);
                    }

                    if (dj.contains("performance")) {
                        const auto& p = dj["performance"];
                        d.latency_ms = p.value("latency_ms", static_cast<int64_t>(0));
                        d.token_usage = p.value("token_usage", 0);
                    }

                    summary.details.push_back(d);
                }
            }
        }

        return record;
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to parse run JSON: ") + e.what());
        return std::nullopt;
    }
}

}  // namespace eval
}  // namespace mmrag