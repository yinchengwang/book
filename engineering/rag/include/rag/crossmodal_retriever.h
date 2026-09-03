#pragma once

#include "rag/retriever.h"
#include "rag/types.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace rag {

// ========== 查询模态 ==========

enum class QueryModality {
    TEXT,
    IMAGE
};

// ========== 图片索引条目 ==========

struct ImageEntry {
    std::string image_id;
    std::string image_path;
    std::string caption;  // 图片描述
    std::vector<float> features;
    std::unordered_map<std::string, std::string> metadata;
};

// ========== 图片特征提取器接口 ==========

class ImageFeatureExtractor {
public:
    virtual ~ImageFeatureExtractor() = default;

    /**
     * @brief 从图片路径提取特征
     */
    virtual std::vector<float> extract(const std::string& image_path) = 0;

    /**
     * @brief 获取特征维度
     */
    virtual int dimension() const = 0;

    /**
     * @brief 是否就绪
     */
    virtual bool is_ready() const = 0;
};

// ========== 简单图片特征提取器 (随机向量，用于测试) ==========

class SimpleImageFeatureExtractor : public ImageFeatureExtractor {
public:
    explicit SimpleImageFeatureExtractor(int dimension = 512);

    std::vector<float> extract(const std::string& image_path) override;
    int dimension() const override { return dimension_; }
    bool is_ready() const override { return ready_; }

    void load() { ready_ = true; }

private:
    int dimension_;
    bool ready_ = false;
};

// ========== 跨模态检索器 ==========

class CrossModalRetriever : public Retriever {
public:
    CrossModalRetriever();

    // 检索 (根据默认模态)
    std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override;

    // 获取检索器名称
    std::string name() const override { return "crossmodal"; }

    // 获取配置
    const RetrievalConfig& config() const override { return config_; }

    // 文本查询图片
    std::vector<RetrievalResult> retrieve_by_text(const std::string& query, int top_k);

    // 图片查询图片
    std::vector<RetrievalResult> retrieve_by_image(const std::string& image_path, int top_k);

    // 添加图片到索引
    void add_image(const ImageEntry& image);

    // 批量添加
    void add_images(const std::vector<ImageEntry>& images);

    // 设置特征提取器
    void set_feature_extractor(std::shared_ptr<ImageFeatureExtractor> extractor);

    // 设置文本嵌入服务
    void set_embedding_service(std::shared_ptr<EmbeddingService> service);

    // 获取图片索引大小
    size_t index_size() const { return image_index_.size(); }

private:
    std::vector<ImageEntry> image_index_;
    std::shared_ptr<ImageFeatureExtractor> feature_extractor_;
    std::shared_ptr<EmbeddingService> embedding_service_;
    RetrievalConfig config_;

    // 计算相似度 (余弦相似度)
    float compute_similarity(const std::vector<float>& a, const std::vector<float>& b);

    // 搜索最相似的图片
    std::vector<std::pair<size_t, float>> search_similar(
        const std::vector<float>& query_features, int top_k);

    // 将 ImageEntry 转换为 RetrievalResult
    RetrievalResult to_retrieval_result(const ImageEntry& entry, float score, int rank);
};

// ========== 图片索引 ==========

class ImageIndex {
public:
    ImageIndex();

    // 添加
    void add(const ImageEntry& entry);

    // 批量添加
    void add_batch(const std::vector<ImageEntry>& entries);

    // 搜索
    std::vector<std::pair<std::string, float>> search(
        const std::vector<float>& query_features,
        int top_k);

    // 加载/保存 (JSON格式)
    bool load(const std::string& path);
    bool save(const std::string& path);

    // 大小
    size_t size() const { return index_.size(); }
    void clear() { index_.clear(); }

    // 获取所有图片ID
    std::vector<std::string> get_all_ids() const;

private:
    std::vector<ImageEntry> index_;
};

// ========== 工厂函数 ==========

std::unique_ptr<CrossModalRetriever> create_crossmodal_retriever();
std::unique_ptr<ImageIndex> create_image_index();
std::unique_ptr<ImageFeatureExtractor> create_simple_image_feature_extractor(int dimension = 512);

}  // namespace rag