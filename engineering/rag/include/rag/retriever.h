/**
 * @file retriever.h
 * @brief 检索器 - 混合检索和 RRF 融合
 */
#pragma once

#include "rag/types.h"
#include "rag/config.h"
#include "rag/vector_index.h"
#include "rag/bm25_index.h"
#include "rag/embedding.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace rag {

// ========== 前向声明 ==========

class Retriever;
class EmbeddingService;

// ========== 检索结果 ==========

/**
 * @brief 检索结果详情
 */
struct RetrievalDetails {
    std::string chunk_id;
    std::string content;
    std::string document_id;
    std::string file_path;
    float vector_score = 0.0f;
    float bm25_score = 0.0f;
    float combined_score = 0.0f;
    int rank = 0;
};

// ========== 检索器接口 ==========

/**
 * @brief 检索器接口
 */
class Retriever {
public:
    virtual ~Retriever() = default;

    // 检索
    virtual std::vector<RetrievalResult> retrieve(
        const std::string& query,
        int top_k) = 0;

    // 获取检索器名称
    virtual std::string name() const = 0;

    // 获取配置
    virtual const RetrievalConfig& config() const = 0;
};

// ========== HNSW 检索器 ==========

/**
 * @brief HNSW 向量检索器
 */
class HNSWRetriever : public Retriever {
public:
    HNSWRetriever(std::shared_ptr<VectorIndex> index,
                  std::shared_ptr<EmbeddingService> embed_service,
                  const RetrievalConfig& config);

    std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override;
    std::string name() const override { return "hnsw"; }
    const RetrievalConfig& config() const override { return config_; }

private:
    std::shared_ptr<VectorIndex> index_;
    std::shared_ptr<EmbeddingService> embed_service_;
    RetrievalConfig config_;
};

// ========== BM25 检索器 ==========

/**
 * @brief BM25 全文检索器
 */
class BM25Retriever : public Retriever {
public:
    BM25Retriever(std::shared_ptr<BM25Index> index,
                  const RetrievalConfig& config);

    std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override;
    std::string name() const override { return "bm25"; }
    const RetrievalConfig& config() const override { return config_; }

    // 添加文档供检索
    void add_document(const std::string& id, const std::string& content);

private:
    std::shared_ptr<BM25Index> index_;
    RetrievalConfig config_;
};

// ========== 混合检索器 ==========

/**
 * @brief 混合检索器 - 结合向量和全文检索
 */
class HybridRetriever : public Retriever {
public:
    HybridRetriever(std::shared_ptr<HNSWRetriever> hnsw,
                    std::shared_ptr<BM25Retriever> bm25,
                    const RetrievalConfig& config);

    std::vector<RetrievalResult> retrieve(const std::string& query, int top_k) override;
    std::string name() const override { return "hybrid"; }
    const RetrievalConfig& config() const override { return config_; }

    // 设置 RRF 参数
    void set_rrf_k(float k) { config_.rrf_k = k; }

    // 设置混合权重
    void set_hybrid_weight(float w) { config_.hybrid_weight = w; }

    // 获取子检索器
    std::shared_ptr<HNSWRetriever> hnsw_retriever() { return hnsw_; }
    std::shared_ptr<BM25Retriever> bm25_retriever() { return bm25_; }

private:
    // RRF 融合
    std::vector<RetrievalResult> rrf_fusion(
        const std::vector<RetrievalResult>& hnsw_results,
        const std::vector<RetrievalResult>& bm25_results,
        int top_k);

    // 加权融合
    std::vector<RetrievalResult> weighted_fusion(
        const std::vector<RetrievalResult>& hnsw_results,
        const std::vector<RetrievalResult>& bm25_results,
        int top_k);

    std::shared_ptr<HNSWRetriever> hnsw_;
    std::shared_ptr<BM25Retriever> bm25_;
    RetrievalConfig config_;
};

// ========== 检索器工厂 ==========

/**
 * @brief 创建检索器
 */
std::shared_ptr<Retriever> create_retriever(
    const RetrievalConfig& config,
    std::shared_ptr<VectorIndex> vector_index,
    std::shared_ptr<BM25Index> bm25_index,
    std::shared_ptr<EmbeddingService> embed_service);

}  // namespace rag
