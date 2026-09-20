/**
 * @file kbase_retriever.h
 * @brief KbaseRetriever 实现
 *
 * 封装自实现数据库 kbase 的检索接口
 */
#pragma once

#include "mmrag/retriever.h"
#include <string>
#include <vector>

namespace mmrag {

/**
 * @brief Kbase 检索结果
 */
struct KbaseResult {
    std::string content;
    float score;
    std::string source;
};

/**
 * @brief KbaseRetriever 实现
 */
class KbaseRetriever : public Retriever {
public:
    /**
     * @brief 构造函数
     * @param db_path 数据库路径
     */
    explicit KbaseRetriever(const std::string& db_path);

    ~KbaseRetriever() override;

    /**
     * @brief 语义检索
     * @param query 查询文本
     * @param top_k 返回结果数量
     * @return 检索结果列表
     */
    std::vector<Chunk> retrieve(const std::string& query, int top_k) override;

    /**
     * @brief 添加文档
     * @param file_path 文件路径
     * @return 是否成功
     */
    bool add_document(const std::string& file_path) override;

    /**
     * @brief 删除文档
     * @param file_path 文件路径
     * @return 是否成功
     */
    bool remove_document(const std::string& file_path) override;

    /**
     * @brief 重建索引
     * @param data_dir 数据目录
     */
    void rebuild_index(const std::string& data_dir) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace mmrag
