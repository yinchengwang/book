/**
 * @file golden_dataset.cpp
 * @brief Golden Dataset 存储实现
 */

#include "mmrag/eval/golden_dataset.h"
#include "mmrag/types.h"
#include "mmrag/logger.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <random>
#include <filesystem>

// JSON
#include <nlohmann/json.hpp>
using nlohmann::json;

namespace fs = std::filesystem;

namespace mmrag {
namespace eval {

// ========== 辅助函数 ==========

namespace {

std::string sha256_hex(const std::string& input) {
    // 简化版 SHA-256（生产环境应使用 OpenSSL）
    // 这里使用简单的 hash 作为占位
    std::hash<std::string> hasher;
    size_t h = hasher(input);
    std::ostringstream oss;
    oss << std::hex << std::setw(16) << std::setfill('0') << h;
    return oss.str();
}

int64_t now_ms() {
    return mmrag::current_timestamp_ms();
}

int64_t now_s() {
    return mmrag::current_timestamp();
}

std::string format_iso8601(int64_t timestamp_ms) {
    std::time_t t = static_cast<std::time_t>(timestamp_ms / 1000);
    std::tm* tm_info = std::gmtime(&t);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", tm_info);
    return std::string(buf);
}

}  // anonymous namespace

// ========== GoldenDatasetStore 实现 ==========

GoldenDatasetStore::GoldenDatasetStore(const std::string& base_path)
    : base_path_(base_path) {
    // 确保基础目录存在
    fs::create_directories(base_path_);
}

std::string GoldenDatasetStore::get_dataset_dir(const std::string& dataset_id) const {
    return base_path_ + "/" + dataset_id;
}

std::string GoldenDatasetStore::get_version_path(
    const std::string& dataset_id,
    const std::string& version) const {
    return get_dataset_dir(dataset_id) + "/v" + version + ".json";
}

std::string GoldenDatasetStore::get_metadata_path(const std::string& dataset_id) const {
    return get_dataset_dir(dataset_id) + "/metadata.json";
}

std::string GoldenDatasetStore::compute_checksum(const GoldenDataset& dataset) const {
    // 使用问题的数量和 ID 列表生成校验和
    std::ostringstream oss;
    oss << dataset.dataset_id << "|" << dataset.version << "|";
    oss << dataset.questions.size() << "|";
    for (const auto& q : dataset.questions) {
        oss << q.id << ",";
    }
    return sha256_hex(oss.str());
}

std::string GoldenDatasetStore::generate_id() const {
    return mmrag::generate_uuid();
}

std::string GoldenDatasetStore::create_dataset(
    const std::string& name,
    const std::string& description,
    const std::vector<TestCase>& questions) {
    GoldenDataset dataset;
    dataset.dataset_id = generate_id();
    dataset.name = name;
    dataset.description = description;
    dataset.version = "1.0.0";
    dataset.questions = questions;
    dataset.created_at = now_ms();
    dataset.updated_at = now_ms();
    dataset.changelog.push_back(format_iso8601(now_ms()) + ": Initial version");

    if (save_dataset(dataset)) {
        return dataset.dataset_id;
    }
    return "";
}

bool GoldenDatasetStore::save_dataset(const GoldenDataset& dataset) {
    try {
        std::string dir = get_dataset_dir(dataset.dataset_id);
        fs::create_directories(dir);

        // 保存版本文件
        std::string version_path = get_version_path(dataset.dataset_id, dataset.version);
        std::ofstream file(version_path);
        if (!file.is_open()) {
            RAG_ERROR("Failed to open version file: " + version_path);
            return false;
        }
        file << export_to_json(dataset);
        file.close();

        // 更新 metadata.json
        json meta;
        meta["dataset_id"] = dataset.dataset_id;
        meta["name"] = dataset.name;
        meta["description"] = dataset.description;
        meta["latest_version"] = dataset.version;
        meta["created_at"] = dataset.created_at;
        meta["updated_at"] = now_ms();
        meta["question_count"] = dataset.questions.size();
        meta["checksum"] = compute_checksum(dataset);
        meta["changelog"] = dataset.changelog;
        meta["metadata"] = dataset.metadata;

        std::ofstream meta_file(get_metadata_path(dataset.dataset_id));
        if (!meta_file.is_open()) {
            RAG_ERROR("Failed to open metadata file");
            return false;
        }
        meta_file << meta.dump(2);
        meta_file.close();

        RAG_INFO("Saved dataset " + dataset.dataset_id + " v" + dataset.version);
        return true;
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to save dataset: ") + e.what());
        return false;
    }
}

std::optional<GoldenDataset> GoldenDatasetStore::load_dataset(
    const std::string& dataset_id,
    const std::string& version) {
    try {
        std::string meta_path = get_metadata_path(dataset_id);
        if (!fs::exists(meta_path)) {
            RAG_ERROR("Dataset not found: " + dataset_id);
            return std::nullopt;
        }

        // 读取 metadata 获取 latest_version（如果 version 为空）
        std::ifstream meta_file(meta_path);
        json meta = json::parse(meta_file);
        meta_file.close();

        std::string target_version = version;
        if (target_version.empty()) {
            target_version = meta.value("latest_version", "1.0.0");
        }

        // 加载指定版本
        std::string version_path = get_version_path(dataset_id, target_version);
        if (!fs::exists(version_path)) {
            RAG_ERROR("Version not found: " + target_version);
            return std::nullopt;
        }

        std::ifstream vf(version_path);
        std::string content((std::istreambuf_iterator<char>(vf)),
                             std::istreambuf_iterator<char>());
        vf.close();

        return import_from_json(content);
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to load dataset: ") + e.what());
        return std::nullopt;
    }
}

std::vector<DatasetVersionInfo> GoldenDatasetStore::list_versions(
    const std::string& dataset_id) {
    std::vector<DatasetVersionInfo> result;

    try {
        std::string dir = get_dataset_dir(dataset_id);
        if (!fs::exists(dir)) return result;

        for (const auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".json" &&
                entry.path().filename().string().substr(0, 2) == "v1") {
                // 跳过 metadata.json
                std::string filename = entry.path().filename().string();
                if (filename.substr(0, 2) == "v0" || filename == "metadata.json") {
                    continue;
                }

                // 解析版本号：v1.0.0.json -> 1.0.0
                std::string version = filename.substr(1, filename.size() - 6);

                std::ifstream vf(entry.path());
                json data = json::parse(vf);
                vf.close();

                DatasetVersionInfo info;
                info.dataset_id = dataset_id;
                info.version = version;
                info.created_at = data.value("created_at", static_cast<int64_t>(0));
                info.description = data.value("description", "");
                info.question_count = data.value("questions", json::array()).size();
                info.checksum = data.value("checksum", "");

                result.push_back(info);
            }
        }

        std::sort(result.begin(), result.end(),
                  [](const DatasetVersionInfo& a, const DatasetVersionInfo& b) {
                      return a.version > b.version;  // 倒序：最新版本在前
                  });
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to list versions: ") + e.what());
    }

    return result;
}

std::vector<DatasetVersionInfo> GoldenDatasetStore::list_datasets() {
    std::vector<DatasetVersionInfo> result;

    try {
        if (!fs::exists(base_path_)) return result;

        for (const auto& entry : fs::directory_iterator(base_path_)) {
            if (entry.is_directory()) {
                std::string dataset_id = entry.path().filename().string();
                std::string meta_path = get_metadata_path(dataset_id);

                if (fs::exists(meta_path)) {
                    std::ifstream mf(meta_path);
                    json meta = json::parse(mf);
                    mf.close();

                    DatasetVersionInfo info;
                    info.dataset_id = dataset_id;
                    info.version = meta.value("latest_version", "1.0.0");
                    info.created_at = meta.value("created_at", static_cast<int64_t>(0));
                    info.description = meta.value("description", "");
                    info.question_count = meta.value("question_count", 0);
                    info.checksum = meta.value("checksum", "");

                    result.push_back(info);
                }
            }
        }
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to list datasets: ") + e.what());
    }

    return result;
}

bool GoldenDatasetStore::delete_version(
    const std::string& dataset_id,
    const std::string& version) {
    try {
        std::string path = get_version_path(dataset_id, version);
        if (fs::exists(path)) {
            fs::remove(path);
            RAG_INFO("Deleted dataset " + dataset_id + " v" + version);
            return true;
        }
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to delete version: ") + e.what());
    }
    return false;
}

bool GoldenDatasetStore::delete_dataset(const std::string& dataset_id) {
    try {
        std::string dir = get_dataset_dir(dataset_id);
        if (fs::exists(dir)) {
            fs::remove_all(dir);
            RAG_INFO("Deleted dataset " + dataset_id);
            return true;
        }
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to delete dataset: ") + e.what());
    }
    return false;
}

bool GoldenDatasetStore::add_questions(
    const std::string& dataset_id,
    const std::vector<TestCase>& new_questions,
    const std::string& new_version,
    const std::string& changelog_entry) {
    // 加载最新版本
    auto current = load_dataset(dataset_id);
    if (!current.has_value()) {
        RAG_ERROR("Dataset not found: " + dataset_id);
        return false;
    }

    // 创建新版本（添加新问题）
    GoldenDataset updated = current.value();
    updated.questions.insert(updated.questions.end(),
                              new_questions.begin(), new_questions.end());
    updated.version = new_version;
    updated.updated_at = now_ms();

    std::string entry = changelog_entry.empty()
        ? ("Added " + std::to_string(new_questions.size()) + " questions")
        : changelog_entry;
    updated.changelog.push_back(format_iso8601(now_ms()) + ": " + entry);

    return save_dataset(updated);
}

std::string GoldenDatasetStore::export_to_json(const GoldenDataset& dataset) const {
    json j;
    j["dataset_id"] = dataset.dataset_id;
    j["name"] = dataset.name;
    j["description"] = dataset.description;
    j["version"] = dataset.version;
    j["created_at"] = dataset.created_at;
    j["updated_at"] = dataset.updated_at;
    j["checksum"] = compute_checksum(dataset);

    j["questions"] = json::array();
    for (const auto& q : dataset.questions) {
        json jq;
        jq["id"] = q.id;
        jq["query"] = q.query;
        jq["ground_truth"] = q.ground_truth;
        jq["category"] = q.category;
        jq["difficulty"] = q.difficulty;
        jq["relevant_doc_ids"] = q.relevant_doc_ids;
        jq["key_facts"] = q.key_facts;
        jq["metadata"] = q.metadata;
        j["questions"].push_back(jq);
    }

    j["metadata"] = dataset.metadata;
    j["changelog"] = dataset.changelog;

    return j.dump(2);
}

std::optional<GoldenDataset> GoldenDatasetStore::import_from_json(
    const std::string& json_content) const {
    try {
        json j = json::parse(json_content);
        GoldenDataset dataset;
        dataset.dataset_id = j.value("dataset_id", "");
        dataset.name = j.value("name", "");
        dataset.description = j.value("description", "");
        dataset.version = j.value("version", "1.0.0");
        dataset.created_at = j.value("created_at", static_cast<int64_t>(0));
        dataset.updated_at = j.value("updated_at", static_cast<int64_t>(0));

        if (j.contains("questions") && j["questions"].is_array()) {
            for (const auto& jq : j["questions"]) {
                TestCase q;
                q.id = jq.value("id", "");
                q.query = jq.value("query", "");
                q.ground_truth = jq.value("ground_truth", "");
                q.category = jq.value("category", "");
                q.difficulty = jq.value("difficulty", "medium");

                if (jq.contains("relevant_doc_ids")) {
                    for (const auto& d : jq["relevant_doc_ids"]) {
                        q.relevant_doc_ids.push_back(d.get<std::string>());
                    }
                }

                if (jq.contains("key_facts")) {
                    for (const auto& f : jq["key_facts"]) {
                        q.key_facts.push_back(f.get<std::string>());
                    }
                }

                if (jq.contains("metadata")) {
                    for (auto it = jq["metadata"].begin(); it != jq["metadata"].end(); ++it) {
                        q.metadata[it.key()] = it.value().get<std::string>();
                    }
                }

                dataset.questions.push_back(q);
            }
        }

        if (j.contains("metadata")) {
            for (auto it = j["metadata"].begin(); it != j["metadata"].end(); ++it) {
                dataset.metadata[it.key()] = it.value().get<std::string>();
            }
        }

        if (j.contains("changelog")) {
            for (const auto& c : j["changelog"]) {
                dataset.changelog.push_back(c.get<std::string>());
            }
        }

        return dataset;
    } catch (const std::exception& e) {
        RAG_ERROR(std::string("Failed to parse dataset JSON: ") + e.what());
        return std::nullopt;
    }
}

}  // namespace eval
}  // namespace mmrag