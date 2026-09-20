/**
 * @file vector_index.h
 * @brief 向量索引接口和 HNSW 实现
 */
#pragma once

#include "mmrag/types.h"
#include "mmrag/config.h"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

namespace mmrag {

// ========== 前向声明 ==========

class VectorIndex;

// ========== 向量索引接口 ==========

/**
 * @brief 向量索引接口
 */
class VectorIndex {
public:
    virtual ~VectorIndex() = default;

    // 初始化
    virtual void init(const HNSWConfig& config) = 0;

    // 添加向量
    virtual void add(const std::string& id,
                     const std::vector<float>& vector) = 0;

    // 批量添加
    virtual void add_batch(const std::vector<std::string>& ids,
                          const std::vector<std::vector<float>>& vectors) = 0;

    // 搜索
    virtual std::vector<std::pair<std::string, float>> search(
        const std::vector<float>& query,
        int top_k) = 0;

    // 获取向量
    virtual std::optional<std::vector<float>> get(const std::string& id) = 0;

    // 删除
    virtual void remove(const std::string& id) = 0;

    // 清空
    virtual void clear() = 0;

    // 保存到文件
    virtual void save(const std::filesystem::path& path) = 0;

    // 从文件加载
    virtual void load(const std::filesystem::path& path) = 0;

    // 获取维度
    virtual int dimension() const = 0;

    // 获取元素数
    virtual size_t size() const = 0;

    // 是否已加载
    virtual bool is_loaded() const = 0;
};

// ========== FlatArray 索引实现 (HNSW 双实现之一) ==========

/**
 * @brief 内存数组索引（暴力 L2 搜索）
 *
 * 适合小数据集（< 10k 向量）。纯内存 brute-force + partial_sort。
 * 无外部依赖，直接 std::vector 存储。
 */
class FlatArrayIndex : public VectorIndex {
public:
    FlatArrayIndex();
    ~FlatArrayIndex() override;

    // 禁用拷贝
    FlatArrayIndex(const FlatArrayIndex&) = delete;
    FlatArrayIndex& operator=(const FlatArrayIndex&) = delete;

    void init(const HNSWConfig& config) override;

    void add(const std::string& id, const std::vector<float>& vector) override;
    void add_batch(const std::vector<std::string>& ids,
                   const std::vector<std::vector<float>>& vectors) override;

    std::vector<std::pair<std::string, float>> search(
        const std::vector<float>& query,
        int top_k) override;

    std::optional<std::vector<float>> get(const std::string& id) override;
    void remove(const std::string& id) override;
    void clear() override;

    void save(const std::filesystem::path& path) override;
    void load(const std::filesystem::path& path) override;

    int dimension() const override { return config_.dim; }
    size_t size() const override;

    bool is_loaded() const override { return loaded_; }

    const HNSWConfig& config() const { return config_; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    HNSWConfig config_;
    bool loaded_ = false;
};

// ========== HNSW 索引实现 (HNSW 双实现之二) ==========

/**
 * @brief HNSW 向量索引（真实 faiss_hnsw）
 *
 * 使用 libfaiss_hnsw.a 的真实 HNSW 图索引。
 * 支持增量 add、tombstone 删除。
 */
class HNSWIndex : public VectorIndex {
public:
    HNSWIndex();
    ~HNSWIndex() override;

    // 禁用拷贝
    HNSWIndex(const HNSWIndex&) = delete;
    HNSWIndex& operator=(const HNSWIndex&) = delete;

    void init(const HNSWConfig& config) override;

    void add(const std::string& id, const std::vector<float>& vector) override;
    void add_batch(const std::vector<std::string>& ids,
                   const std::vector<std::vector<float>>& vectors) override;

    std::vector<std::pair<std::string, float>> search(
        const std::vector<float>& query,
        int top_k) override;

    std::optional<std::vector<float>> get(const std::string& id) override;
    void remove(const std::string& id) override;
    void clear() override;

    void save(const std::filesystem::path& path) override;
    void load(const std::filesystem::path& path) override;

    int dimension() const override { return config_.dim; }
    size_t size() const override;

    bool is_loaded() const override { return loaded_; }

    // 获取配置
    const HNSWConfig& config() const { return config_; }

private:
    // 内部状态
    struct Impl;
    std::unique_ptr<Impl> impl_;
    HNSWConfig config_;
    bool loaded_ = false;
};

// ========== 工厂函数 ==========

/**
 * @brief 创建向量索引
 *
 * 根据 config.hnsw.index_type 分发：
 *   "flat_array" → FlatArrayIndex（小数据集）
 *   "faiss_hnsw" → HNSWIndex（通用）
 *   其他 → HNSWIndex（默认）
 */
std::unique_ptr<VectorIndex> create_vector_index(const HNSWConfig& config);

}  // namespace mmrag
