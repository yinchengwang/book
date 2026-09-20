/**
 * @file db_adapter.h
 * @brief 数据库适配器接口
 *
 * 支持多数据库切换：kbase/Milvus/Weaviate/Qdrant
 */
#pragma once

#include <string>
#include <vector>
#include <memory>

namespace mmrag {

/**
 * @brief 数据库适配器接口
 */
class DBAdapter {
public:
    virtual ~DBAdapter() = default;

    /**
     * @brief 语义检索
     * @param query 查询文本
     * @param top_k 返回结果数量
     * @return 检索结果 (content, score)
     */
    virtual std::vector<std::pair<std::string, float>> search(
        const std::string& query, int top_k) = 0;

    /**
     * @brief 插入向量
     * @param id 向量 ID
     * @param embedding 向量数据
     * @param content 原始内容
     * @param metadata 元数据 (JSON)
     */
    virtual void insert(
        const std::string& id,
        const std::vector<float>& embedding,
        const std::string& content,
        const std::string& metadata) = 0;

    /**
     * @brief 删除向量
     * @param id 向量 ID
     */
    virtual void remove(const std::string& id) = 0;

    /**
     * @brief 批量插入
     * @param ids ID 列表
     * @param embeddings 向量列表
     * @param contents 内容列表
     * @param metadatas 元数据列表
     */
    virtual void batch_insert(
        const std::vector<std::string>& ids,
        const std::vector<std::vector<float>>& embeddings,
        const std::vector<std::string>& contents,
        const std::vector<std::string>& metadatas) = 0;

    /**
     * @brief 获取统计信息
     * @return 统计信息 JSON
     */
    virtual std::string get_stats() = 0;

    /**
     * @brief 获取所有文档 ID
     * @return 文档 ID 列表
     */
    virtual std::vector<std::string> get_all_ids() = 0;
};

/**
 * @brief 数据库适配器工厂
 */
class DBAdapterFactory {
public:
    /**
     * @brief 创建适配器实例
     * @param db_type 数据库类型 ("kbase", "milvus", "weaviate", "qdrant")
     * @param config_path 配置文件路径
     * @return 适配器实例
     */
    static std::unique_ptr<DBAdapter> create(
        const std::string& db_type,
        const std::string& config_path = "");

    /**
     * @brief 获取支持的数据库类型列表
     * @return 类型列表
     */
    static std::vector<std::string> supported_types();
};

}  // namespace mmrag
