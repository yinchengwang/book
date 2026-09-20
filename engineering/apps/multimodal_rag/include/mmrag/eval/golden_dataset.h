/**
 * @file golden_dataset.h
 * @brief Golden Dataset 测试集管理
 *
 * 支持版本管理、CRUD 操作、持久化存储
 */

#pragma once

#include "mmrag/eval/metrics.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <cstdint>

namespace mmrag {
namespace eval {

/**
 * @brief Golden Dataset 结构
 */
struct GoldenDataset {
    std::string dataset_id;                       // 数据集唯一 ID
    std::string name;                             // 数据集名称
    std::string description;                      // 描述
    std::string version = "1.0.0";                // 语义化版本号
    int64_t created_at = 0;                       // 创建时间
    int64_t updated_at = 0;                       // 更新时间

    std::vector<TestCase> questions;              // 测试问题列表

    std::unordered_map<std::string, std::string> metadata;  // 元数据

    // 版本变更日志
    std::vector<std::string> changelog;
};

/**
 * @brief 数据集版本信息
 */
struct DatasetVersionInfo {
    std::string dataset_id;
    std::string version;
    int64_t created_at;
    std::string description;
    int question_count;
    std::string checksum;                         // SHA256 校验和
};

/**
 * @brief Golden Dataset 存储管理器
 *
 * 将数据集以 JSON 格式持久化到文件系统
 * 目录结构：
 *   eval_data/
 *     datasets/
 *       {dataset_id}/
 *         v1.0.0.json
 *         v1.1.0.json
 *         v2.0.0.json
 *         metadata.json     # 数据集元信息
 */
class GoldenDatasetStore {
public:
    /**
     * @brief 构造函数
     * @param base_path 存储根目录（默认 "eval_data/datasets"）
     */
    explicit GoldenDatasetStore(const std::string& base_path = "eval_data/datasets");

    /**
     * @brief 创建新数据集
     * @return 新数据集的唯一 ID
     */
    std::string create_dataset(
        const std::string& name,
        const std::string& description,
        const std::vector<TestCase>& questions);

    /**
     * @brief 保存数据集的新版本
     */
    bool save_dataset(const GoldenDataset& dataset);

    /**
     * @brief 加载指定版本的数据集
     */
    std::optional<GoldenDataset> load_dataset(
        const std::string& dataset_id,
        const std::string& version = "");

    /**
     * @brief 获取数据集的所有版本
     */
    std::vector<DatasetVersionInfo> list_versions(const std::string& dataset_id);

    /**
     * @brief 列出所有数据集
     */
    std::vector<DatasetVersionInfo> list_datasets();

    /**
     * @brief 删除数据集的指定版本
     */
    bool delete_version(
        const std::string& dataset_id,
        const std::string& version);

    /**
     * @brief 删除整个数据集
     */
    bool delete_dataset(const std::string& dataset_id);

    /**
     * @brief 向现有数据集添加问题（创建新版本）
     */
    bool add_questions(
        const std::string& dataset_id,
        const std::vector<TestCase>& new_questions,
        const std::string& new_version,
        const std::string& changelog_entry = "");

    /**
     * @brief 导出数据集为 JSON 字符串
     */
    std::string export_to_json(const GoldenDataset& dataset) const;

    /**
     * @brief 从 JSON 字符串导入数据集
     */
    std::optional<GoldenDataset> import_from_json(const std::string& json_content) const;

private:
    std::string base_path_;

    std::string get_dataset_dir(const std::string& dataset_id) const;
    std::string get_version_path(
        const std::string& dataset_id,
        const std::string& version) const;
    std::string get_metadata_path(const std::string& dataset_id) const;

    std::string compute_checksum(const GoldenDataset& dataset) const;
    std::string generate_id() const;
};

}  // namespace eval
}  // namespace mmrag