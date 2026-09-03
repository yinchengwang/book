#pragma once

#include "rag/pipeline.h"
#include "rag/types.h"
#include <vector>
#include <memory>

namespace rag {

// ========== 权重结构 ==========

struct RRFWeights {
    float hnsw_weight = 0.5f;
    float bm25_weight = 0.5f;
    float graph_weight = 0.0f;
};

// ========== Adaptive RRF 融合器 ==========

class AdaptiveRRF {
public:
    AdaptiveRRF(int rrf_k = 60);

    // 获取指定查询类型的权重
    RRFWeights get_weights(QueryType query_type);

    // 执行融合
    std::vector<RetrievalResult> fuse(
        const std::vector<RetrievalResult>& hnsw,
        const std::vector<RetrievalResult>& bm25,
        QueryType query_type,
        int top_k);

    // 三路融合（包含 graph）
    std::vector<RetrievalResult> fuse_with_graph(
        const std::vector<RetrievalResult>& hnsw,
        const std::vector<RetrievalResult>& bm25,
        const std::vector<RetrievalResult>& graph,
        QueryType query_type,
        int top_k);

    void set_rrf_k(int k) { rrf_k_ = k; }
    int rrf_k() const { return rrf_k_; }

private:
    // 标准 RRF 融合
    std::vector<RetrievalResult> rrf_fuse(
        const std::vector<std::pair<std::vector<RetrievalResult>, float>>& sources,
        int top_k);

    // 计算 RRF 得分
    float rrf_score(int rank, float weight);

    int rrf_k_;
};

// ========== 置信度加权融合 ==========

class ConfidenceWeightedFusion {
public:
    ConfidenceWeightedFusion(int rrf_k = 60);

    std::vector<RetrievalResult> fuse(
        const std::vector<RetrievalResult>& hnsw,
        const std::vector<RetrievalResult>& bm25,
        float hnsw_confidence,
        float bm25_confidence,
        int top_k);

    void set_rrf_k(int k) { rrf_k_ = k; }

private:
    int rrf_k_;
};

// ========== 工厂函数 ==========

std::unique_ptr<AdaptiveRRF> create_adaptive_rrf(int rrf_k = 60);
std::unique_ptr<ConfidenceWeightedFusion> create_confidence_fusion(int rrf_k = 60);

}  // namespace rag