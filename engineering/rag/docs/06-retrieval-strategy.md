# RAG 系统检索策略设计

## 概述

本文档定义 RAG 系统的检索策略，包括 HNSW 向量检索、BM25 全文检索、混合检索和 RRF 融合等。

---

## 1. 检索策略概览

### 1.1 检索流程

```
┌─────────────────────────────────────────────────────────────┐
│                    检索流程                                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  用户查询                                                    │
│     │                                                      │
│     ▼                                                      │
│  ┌─────────────────┐                                       │
│  │  查询向量化     │ ──→ EmbeddingService.encode()          │
│  └─────────────────┘                                       │
│     │                                                      │
│     ▼                                                      │
│  ┌─────────────────┐                                       │
│  │  混合检索       │                                       │
│  │  ┌───────────┐  │                                       │
│  │  │  HNSW    │  │                                       │
│  │  │  向量检索  │  │                                       │
│  │  └───────────┘  │                                       │
│  │  ┌───────────┐  │                                       │
│  │  │   BM25   │  │                                       │
│  │  │  全文检索 │  │                                       │
│  │  └───────────┘  │                                       │
│  └─────────────────┘                                       │
│     │                                                      │
│     ▼                                                      │
│  ┌─────────────────┐                                       │
│  │  RRF 融合      │ ──→ Reciprocal Rank Fusion             │
│  └─────────────────┘                                       │
│     │                                                      │
│     ▼                                                      │
│  ┌─────────────────┐                                       │
│  │  重排序 (可选)   │ ──→ Cross-Encoder 重排序             │
│  └─────────────────┘                                       │
│     │                                                      │
│     ▼                                                      │
│  ┌─────────────────┐                                       │
│  │   返回结果      │                                       │
│  └─────────────────┘                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 检索器接口

```cpp
// retriever.h
#pragma once

#include <string>
#include <vector>
#include <memory>
#include "chunk.h"

namespace rag {

struct RetrievalResult {
    Chunk chunk;
    float score;
    std::string source;                  // "hnsw", "bm25", "hybrid"
};

struct RetrievalConfig {
    int top_k = 5;                      // 返回的 Top-K 结果
    int hnsw_ef_search = 100;           // HNSW 搜索参数
    float min_score = 0.3f;            // 最低相关度阈值
    float hnsw_weight = 0.6f;           // HNSW 权重
    float bm25_weight = 0.4f;           // BM25 权重
    int rrf_k = 60;                     // RRF K 值
};

class Retriever {
public:
    virtual ~Retriever() = default;

    // 初始化
    virtual bool initialize(const Config& config) = 0;

    // 检索
    virtual std::vector<RetrievalResult> retrieve(
        const std::string& query,
        int top_k) = 0;

    // 异步检索
    virtual std::future<std::vector<RetrievalResult>>
    retrieve_async(const std::string& query, int top_k) = 0;

    // 单一检索器检索
    virtual std::vector<RetrievalResult>
    retrieve_hnsw(const Vector& query_vec, int top_k) = 0;

    virtual std::vector<RetrievalResult>
    retrieve_bm25(const std::string& query, int top_k) = 0;
};

}  // namespace rag
```

---

## 2. HNSW 向量检索

### 2.1 HNSW 算法原理

```
HNSW (Hierarchical Navigable Small World) 是一种基于图的向量索引算法：

层级结构：
┌─────────────────────────────────────────────────────────────┐
│  Level 3:    ○────────○                                    │
│                 │                                           │
│  Level 2:    ○───○───○───○                                │
│             /  │     │     │  \                            │
│  Level 1:  ○   ○     ○     ○   ○                          │
│           /|   |\    /|     |\   |\                         │
│  Level 0: ○───○───○───○───○───○───○                       │
└─────────────────────────────────────────────────────────────┘

搜索过程：
1. 从顶层最长边开始贪心搜索
2. 到达局部最优后下降到下一层
3. 重复直到最底层
```

### 2.2 HNSW 接口

```cpp
// hnsw_index.h
#pragma once

#include <vector>
#include <string>
#include <memory>

namespace rag {

struct HNSWParams {
    int dimension = 768;               // 向量维度
    int max_elements = 1000000;       // 最大元素数
    int m = 32;                        // 连接数
    int ef_construction = 200;         // 构建时搜索范围
    int ef_search = 100;               // 搜索时搜索范围
    int num_threads = 4;               // 线程数
    MetricType metric = MetricType::COSINE;  // 距离度量
};

enum class MetricType {
    L2,     // 欧氏距离
    COSINE, // 余弦距离
    IP      // 内积
};

struct SearchResult {
    int id;                            // 元素ID
    float distance;                    // 距离
    int label;                         // 标签 (Chunk ID)
};

class HNSWIndex {
public:
    HNSWIndex();
    ~HNSWIndex();

    // 初始化
    bool init(const HNSWParams& params);

    // 添加向量
    void add_vector(int id, const Vector& vector);
    void add_vectors(const std::vector<std::pair<int, Vector>>& vectors);

    // 搜索
    std::vector<SearchResult> search(
        const Vector& query,
        int k,
        int ef_search = -1);  // -1 使用默认

    // 保存/加载
    bool save(const std::string& path);
    bool load(const std::string& path);

    // 统计
    size_t size() const;
    size_t memory_usage() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rag
```

### 2.3 HNSW 集成

```cpp
// hnsw_retriever.h
#pragma once

#include "retriever.h"
#include "hnsw_index.h"

namespace rag {

class HNSWRetriever : public Retriever {
public:
    HNSWRetriever(std::shared_ptr<EmbeddingService> embed_service,
                   std::shared_ptr<HNSWIndex> index);

    std::vector<RetrievalResult> retrieve(
        const std::string& query,
        int top_k) override;

    std::vector<RetrievalResult> retrieve_hnsw(
        const Vector& query_vec,
        int top_k) override;

    std::vector<RetrievalResult> retrieve_bm25(
        const std::string& query,
        int top_k) override;

private:
    std::shared_ptr<EmbeddingService> embed_service_;
    std::shared_ptr<HNSWIndex> index_;
};

}  // namespace rag
```

### 2.4 HNSW 参数调优

| 参数 | 范围 | 默认值 | 说明 |
|------|------|--------|------|
| `m` | 16-64 | 32 | 邻居数。越大越精确，但内存占用越高 |
| `ef_construction` | 100-400 | 200 | 构建精度。越大构建越慢但精度越高 |
| `ef_search` | 50-1000 | 100 | 搜索精度。越大越精确但越慢 |
| `num_threads` | 1-16 | CPU核数 | 并行线程数 |

### 2.5 性能基准

| 配置 | 100K 向量 | 1M 向量 | 内存占用 |
|------|-----------|---------|----------|
| m=16, ef=100 | 5ms | 20ms | ~2GB |
| m=32, ef=200 | 8ms | 35ms | ~4GB |
| m=48, ef=400 | 12ms | 50ms | ~6GB |

---

## 3. BM25 全文检索

### 3.1 BM25 算法原理

```
BM25 (Best Matching 25) 是基于词袋模型的检索算法：

公式：
score(D, Q) = Σ IDF(qi) × (tf(ti,D) × (k1 + 1)) / (tf(ti,D) + k1 × (1 - b + b × |D|/avgdl))

其中：
- tf(ti,D): 词频
- IDF(qi): 逆文档频率
- |D|: 文档长度
- avgdl: 平均文档长度
- k1, b: 调优参数 (通常 k1=1.5, b=0.75)
```

### 3.2 BM25 接口

```cpp
// bm25_index.h
#pragma once

#include <string>
#include <vector>
#include <memory>

namespace rag {

struct BM25Params {
    float k1 = 1.5f;                   // 词频饱和参数
    float b = 0.75f;                  // 文档长度归一化参数
    std::string tokenizer_type = "chinese";
};

struct TokenizerConfig {
    std::string type = "chinese";      // standard, chinese, code
    bool lowercase = true;
    bool remove_stopwords = true;
};

class BM25Index {
public:
    BM25Index();
    ~BM25Index();

    // 初始化
    bool init(const BM25Params& params, const TokenizerConfig& tokenizer);

    // 添加文档
    void add_document(int id, const std::string& text);
    void add_documents(const std::vector<std::pair<int, std::string>>& docs);

    // 构建索引
    void build();

    // 搜索
    struct BM25Result {
        int doc_id;
        std::string doc_text;
        float score;
        std::vector<std::string> matched_terms;
    };

    std::vector<BM25Result> search(
        const std::string& query,
        int top_k);

    // 保存/加载
    bool save(const std::string& path);
    bool load(const std::string& path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rag
```

### 3.3 中文分词集成

```cpp
// chinese_tokenizer.h
#pragma once

#include <string>
#include <vector>

namespace rag {

// 基于 jieba 的中文分词器封装
class ChineseTokenizer {
public:
    ChineseTokenizer();
    ~ChineseTokenizer();

    void init(const std::string& dict_path = "", bool hmm = true);

    // 分词
    std::vector<std::string> tokenize(const std::string& text);

    // 添加词典
    void add_word(const std::string& word, float freq = 1.0f);

    // 加载用户词典
    void load_user_dict(const std::string& dict_path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace rag
```

### 3.4 BM25 参数调优

| 参数 | 范围 | 默认值 | 说明 |
|------|------|--------|------|
| `k1` | 1.2-2.0 | 1.5 | 控制词频增长速率 |
| `b` | 0.5-0.9 | 0.75 | 文档长度归一化程度 |

---

## 4. 混合检索

### 4.1 混合检索器实现

```cpp
// hybrid_retriever.h
#pragma once

#include "retriever.h"
#include "hnsw_retriever.h"
#include "bm25_retriever.h"

namespace rag {

class HybridRetriever : public Retriever {
public:
    HybridRetriever(std::shared_ptr<HNSWRetriever> hnsw,
                    std::shared_ptr<BM25Retriever> bm25,
                    const RetrievalConfig& config);

    std::vector<RetrievalResult> retrieve(
        const std::string& query,
        int top_k) override;

    std::vector<RetrievalResult> retrieve_hnsw(
        const Vector& query_vec,
        int top_k) override;

    std::vector<RetrievalResult> retrieve_bm25(
        const std::string& query,
        int top_k) override;

    // RRF 融合
    std::vector<RetrievalResult> rrf_fusion(
        const std::vector<std::vector<RetrievalResult>>& result_sets,
        int k = 60);

private:
    std::shared_ptr<HNSWRetriever> hnsw_;
    std::shared_ptr<BM25Retriever> bm25_;
    RetrievalConfig config_;
};

}  // namespace rag
```

### 4.2 RRF 融合算法

```cpp
// Reciprocal Rank Fusion (倒数排名融合)
std::vector<RetrievalResult> HybridRetriever::rrf_fusion(
    const std::vector<std::vector<RetrievalResult>>& result_sets,
    int k) {

    // 每个 result_set 的排名
    std::unordered_map<std::string, float> scores;

    for (const auto& results : result_sets) {
        for (int rank = 0; rank < results.size(); ++rank) {
            const auto& result = results[rank];
            std::string chunk_id = result.chunk.id;

            // RRF 公式: 1 / (k + rank)
            float rrf_score = 1.0f / (k + rank + 1);

            // 累加权重
            scores[chunk_id] += rrf_score;
        }
    }

    // 排序
    std::vector<std::pair<std::string, float>> sorted_scores(
        scores.begin(), scores.end());
    std::sort(sorted_scores.begin(), sorted_scores.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second;
              });

    // 构建结果
    std::vector<RetrievalResult> fused;
    for (const auto& [chunk_id, score] : sorted_scores) {
        // 找到对应的 Chunk
        RetrievalResult result;
        result.chunk = get_chunk(chunk_id);
        result.score = score;
        result.source = "hybrid";
        fused.push_back(result);
    }

    return fused;
}
```

### 4.3 带权重的混合检索

```cpp
std::vector<RetrievalResult> HybridRetriever::retrieve(
    const std::string& query,
    int top_k) {

    // 1. 获取查询向量
    Vector query_vec = embed_service_->encode(query);

    // 2. 并行执行两个检索
    auto hnsw_future = std::async(std::launch::async, [&] {
        return retrieve_hnsw(query_vec, top_k * 2);
    });

    auto bm25_future = std::async(std::launch::async, [&] {
        return retrieve_bm25(query, top_k * 2);
    });

    auto hnsw_results = hnsw_future.get();
    auto bm25_results = bm25_future.get();

    // 3. 归一化分数
    normalize_scores(hnsw_results, config_.hnsw_weight);
    normalize_scores(bm25_results, config_.bm25_weight);

    // 4. RRF 融合
    return rrf_fusion({hnsw_results, bm25_results}, config_.rrf_k);
}

// 分数归一化
void normalize_scores(std::vector<RetrievalResult>& results, float weight) {
    if (results.empty()) return;

    float max_score = results[0].score;
    if (max_score == 0) max_score = 1.0f;

    for (auto& result : results) {
        result.score = (result.score / max_score) * weight;
    }
}
```

---

## 5. 重排序 (Reranking)

### 5.1 Cross-Encoder 重排序

```cpp
// reranker.h
#pragma once

#include <string>
#include <vector>

namespace rag {

struct RerankResult {
    std::string chunk_id;
    float score;
    std::string reason;                  // 重排原因
};

class Reranker {
public:
    virtual ~Reranker() = default;

    // 初始化
    virtual bool init(const std::string& model_path) = 0;

    // 重排序
    virtual std::vector<RerankResult> rerank(
        const std::string& query,
        const std::vector<Chunk>& candidates,
        int top_n = 10) = 0;

    // 批量重排序
    virtual std::vector<std::vector<RerankResult>> rerank_batch(
        const std::vector<std::string>& queries,
        const std::vector<std::vector<Chunk>>& candidates,
        int top_n = 10) = 0;
};

}  // namespace rag
```

### 5.2 轻量级重排序

如果没有专用 Cross-Encoder 模型，可以使用轻量级方法：

```cpp
// lightweight_reranker.h
#pragma once

namespace rag {

// 基于关键词匹配的重排序
class LightweightReranker {
public:
    struct RerankResult {
        std::string chunk_id;
        float score;
    };

    std::vector<RerankResult> rerank(
        const std::string& query,
        const std::vector<Chunk>& candidates) {

        auto query_terms = tokenize(query);
        std::vector<RerankResult> results;

        for (const auto& chunk : candidates) {
            float score = 0.0f;
            auto chunk_terms = tokenize(chunk.content);

            // 精确匹配
            for (const auto& term : query_terms) {
                if (contains(chunk_terms, term)) {
                    score += 1.0f;
                    // 精确匹配加权
                    if (starts_with(chunk.content, term)) {
                        score += 0.5f;
                    }
                }
            }

            // BM25 分数
            score += bm25_score(chunk.content, query_terms);

            results.push_back({chunk.id, score});
        }

        // 排序
        std::sort(results.begin(), results.end(),
                  [](const auto& a, const auto& b) {
                      return a.score > b.score;
                  });

        return results;
    }

private:
    std::vector<std::string> tokenize(const std::string& text);
    float bm25_score(const std::string& doc,
                     const std::vector<std::string>& query_terms);
};

}  // namespace rag
```

---

## 6. 过滤与后处理

### 6.1 元数据过滤

```cpp
// retrieval_filter.h
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace rag {

struct Filter {
    // 文件类型过滤
    std::vector<std::string> file_types;  // 空 = 不过滤

    // 文件路径过滤
    std::string path_prefix;
    std::vector<std::string> path_excludes;

    // 时间范围
    int64_t created_after = 0;
    int64_t created_before = INT64_MAX;

    // 自定义元数据过滤
    std::unordered_map<std::string, std::string> metadata;
};

class RetrievalFilter {
public:
    // 应用过滤器
    std::vector<RetrievalResult> apply(
        const std::vector<RetrievalResult>& results,
        const Filter& filter);

    // 解析过滤器字符串
    static Filter parse(const std::string& filter_str);
};

}  // namespace rag
```

### 6.2 多样性增强

```cpp
// diversity_enhancer.h
#pragma once

namespace rag {

// MMR (Maximum Marginal Relevance) 多样性增强
class DiversityEnhancer {
public:
    struct MMRResult {
        RetrievalResult result;
        float mmr_score;
    };

    // MMR 选择
    std::vector<MMRResult> select_with_diversity(
        const std::vector<RetrievalResult>& candidates,
        const std::string& query,
        const Vector& query_vec,
        int top_k,
        float lambda = 0.5) {  // lambda: 0=只看相关性, 1=只看多样性

        std::vector<MMRResult> selected;
        std::vector<RetrievalResult> remaining = candidates;

        while (selected.size() < top_k && !remaining.empty()) {
            float best_score = -1.0f;
            int best_idx = -1;

            for (int i = 0; i < remaining.size(); ++i) {
                const auto& candidate = remaining[i];

                // 相关性分数
                float relevance = candidate.score;

                // 多样性分数 (与已选结果的最大相似度)
                float max_similarity = 0.0f;
                for (const auto& sel : selected) {
                    float sim = cosine_similarity(
                        sel.result.chunk.embedding,
                        candidate.chunk.embedding);
                    max_similarity = std::max(max_similarity, sim);
                }

                // MMR 分数
                float mmr = lambda * relevance +
                            (1 - lambda) * (1 - max_similarity);

                if (mmr > best_score) {
                    best_score = mmr;
                    best_idx = i;
                }
            }

            if (best_idx >= 0) {
                selected.push_back({remaining[best_idx], best_score});
                remaining.erase(remaining.begin() + best_idx);
            }
        }

        return selected;
    }

private:
    float cosine_similarity(const Vector& a, const Vector& b);
};

}  // namespace rag
```

---

## 7. 检索配置指南

### 7.1 推荐配置

```yaml
retrieval:
  # 基础参数
  top_k: 5
  min_score: 0.3

  # 混合检索权重
  hybrid:
    hnsw_weight: 0.6
    bm25_weight: 0.4

  # RRF 参数
  rrf:
    k: 60

  # HNSW 参数
  hnsw:
    ef_search: 100
    m: 32

  # BM25 参数
  bm25:
    k1: 1.5
    b: 0.75

  # 重排序
  rerank:
    enabled: false
    top_n: 10
    model: ""
```

### 7.2 场景化配置

| 场景 | hnsw_weight | bm25_weight | 说明 |
|------|--------------|-------------|------|
| 代码搜索 | 0.7 | 0.3 | 代码语义更重要 |
| 文档问答 | 0.5 | 0.5 | 平衡语义和关键词 |
| 精确匹配 | 0.3 | 0.7 | 关键词匹配优先 |
| 语义理解 | 0.8 | 0.2 | 语义相似度优先 |

### 7.3 性能优化

```cpp
// 检索性能优化

// 1. 查询向量缓存
class QueryCache {
    LRU<std::string, Vector> cache_{1000};
    Vector get_cached(const std::string& query);
};

// 2. 并行检索
auto [hnsw_results, bm25_results] = std::async(
    std::launch::async,
    [&] { return hnsw_retriever_.retrieve(query_vec, top_k * 2); },
    [&] { return bm25_retriever_.retrieve(query, top_k * 2); }
);

// 3. 提前终止
if (hnsw_results.size() >= top_k * 3 &&
    hnsw_results[0].score - hnsw_results[top_k * 3].score > 0.5) {
    // 显著优于其他结果，提前返回
}
```

---

*文档版本：1.0.0*
*最后更新：2026-07-06*
