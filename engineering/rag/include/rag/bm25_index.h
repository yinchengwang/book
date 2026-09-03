/**
 * @file bm25_index.h
 * @brief BM25 全文检索索引
 */
#pragma once

#include "rag/types.h"
#include "rag/config.h"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>

namespace rag {

// ========== BM25 索引 ==========

/**
 * @brief BM25 全文检索索引
 *
 * 实现 Okapi BM25 算法用于关键词检索
 */
class BM25Index {
public:
    BM25Index();
    ~BM25Index();

    // 初始化
    void init(const BM25Config& config);

    // 添加文档
    void add(const std::string& id, const std::string& content);

    // 批量添加
    void add_batch(const std::vector<std::string>& ids,
                   const std::vector<std::string>& contents);

    // 搜索
    std::vector<std::pair<std::string, float>> search(
        const std::string& query,
        int top_k);

    // 搜索（多词）
    std::vector<std::pair<std::string, float>> search(
        const std::vector<std::string>& terms,
        int top_k);

    // 获取文档
    std::optional<std::string> get(const std::string& id);

    // 删除
    void remove(const std::string& id);

    // 清空
    void clear();

    // 保存到文件
    void save(const std::filesystem::path& path);

    // 从文件加载
    void load(const std::filesystem::path& path);

    // 获取文档数
    size_t size() const { return doc_count_; }

    // 是否已加载
    bool is_loaded() const { return loaded_; }

    // 获取配置
    const BM25Config& config() const { return config_; }

private:
    // 分词
    std::vector<std::string> tokenize(const std::string& text);

    // 计算 IDF
    float calculate_idf(const std::string& term);

    // 更新统计信息
    void update_stats();

    BM25Config config_;

    // 文档存储
    std::unordered_map<std::string, std::string> documents_;
    std::vector<std::string> doc_ids_;

    // 词频统计
    std::unordered_map<std::string, std::vector<int>> term_freq_;  // term -> {freq per doc}
    std::unordered_map<std::string, int> doc_freq_;  // term -> num docs containing term

    // 文档长度
    std::vector<int> doc_lengths_;
    int avg_doc_len_ = 0;

    int doc_count_ = 0;
    bool loaded_ = false;
};

// ========== 工厂函数 ==========

/**
 * @brief 创建 BM25 索引
 */
std::unique_ptr<BM25Index> create_bm25_index(const BM25Config& config);

}  // namespace rag
