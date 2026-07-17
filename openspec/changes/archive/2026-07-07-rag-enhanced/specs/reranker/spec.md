# 重排序器规格

> 状态: 草稿 | 创建日期: 2026-07-07

## 1. 概述

### 1.1 目的

定义重排序（Reranking）服务的接口和行为规范，用于在初步检索后对候选结果进行精细排序。

### 1.2 使用场景

```
┌─────────────────────────────────────────────────────────────────────┐
│                        两阶段检索流程                                │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  阶段 1: 粗召回 (Recall)                                            │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                                                             │   │
│  │   查询 ──▶ HNSW ──▶ Top-50                                 │   │
│  │            +                                                │   │
│  │           BM25 ──▶ Top-50 ──▶ RRF 融合 ──▶ 候选集         │   │
│  │                                                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                              │                                       │
│                              ▼                                       │
│  阶段 2: 精排序 (Rerank)                                            │
│  ┌─────────────────────────────────────────────────────────────┐   │
│  │                                                             │   │
│  │   候选集 ──▶ Cross-Encoder / Lightweight ──▶ Top-K        │   │
│  │                                                             │   │
│  └─────────────────────────────────────────────────────────────┘   │
│                                                                     │
└─────────────────────────────────────────────────────────────────────┘
```

## 2. 接口规范

### 2.1 Reranker 接口

```cpp
namespace rag {

/**
 * @brief 重排序结果
 */
struct RerankResult {
    std::string chunk_id;      // Chunk ID
    std::string content;       // Chunk 内容
    float score = 0.0f;        // 重排分数
    std::string reason;        // 重排原因 (可选)
};

/**
 * @brief 重排序器接口
 */
class Reranker {
public:
    virtual ~Reranker() = default;
    
    /**
     * @brief 初始化
     * @param model_path 模型路径或配置
     * @return 是否初始化成功
     */
    virtual bool init(const std::string& model_path) = 0;
    
    /**
     * @brief 重排序
     * @param query 查询文本
     * @param candidates 候选结果列表
     * @param top_n 返回结果数
     * @return 排序后的结果
     */
    virtual std::vector<RerankResult> rerank(
        const std::string& query,
        const std::vector<Chunk>& candidates,
        int top_n) = 0;
    
    /**
     * @brief 批量重排序
     */
    virtual std::vector<std::vector<RerankResult>> rerank_batch(
        const std::vector<std::string>& queries,
        const std::vector<std::vector<Chunk>>& candidates_list,
        int top_n) = 0;
    
    /**
     * @brief 获取模型名称
     */
    virtual std::string model_name() const = 0;
    
    /**
     * @brief 是否就绪
     */
    virtual bool is_ready() const = 0;
};

}  // namespace rag
```

### 2.2 LightweightReranker

```cpp
/**
 * @brief 轻量级重排序器
 * 
 * 基于关键词匹配和 BM25/向量分数融合，无外部依赖
 */
class LightweightReranker : public Reranker {
public:
    LightweightReranker();
    
    bool init(const std::string& model_path) override {
        return true;  // 无需初始化
    }
    
    std::vector<RerankResult> rerank(
        const std::string& query,
        const std::vector<Chunk>& candidates,
        int top_n) override;
    
    std::vector<std::vector<RerankResult>> rerank_batch(
        const std::vector<std::string>& queries,
        const std::vector<std::vector<Chunk>>& candidates_list,
        int top_n) override;
    
    std::string model_name() const override { return "lightweight"; }
    bool is_ready() const override { return true; }
    
    // ========== 配置方法 ==========
    
    /// 设置 BM25 分数
    void set_bm25_scores(const std::unordered_map<std::string, float>& scores);
    
    /// 设置向量相似度分数
    void set_vector_scores(const std::unordered_map<std::string, float>& scores);
    
    /// 设置权重
    void set_weights(float keyword, float bm25, float vector, float position);
    
    /// 清空分数缓存
    void clear_scores();

private:
    float compute_score(const std::string& query, const Chunk& chunk);
    float keyword_match_score(const std::string& query, const std::string& content);
    std::vector<std::string> tokenize(const std::string& text);
    
    std::unordered_map<std::string, float> bm25_scores_;
    std::unordered_map<std::string, float> vector_scores_;
    
    // 权重配置
    float keyword_weight_ = 0.4f;
    float bm25_weight_ = 0.3f;
    float vector_weight_ = 0.3f;
    float position_weight_ = 0.05f;
};
```

## 3. 算法规范

### 3.1 LightweightReranker 评分公式

```
综合分数 = 关键词匹配 * W_keyword + BM25分数 * W_bm25 + 向量分数 * W_vector + 位置奖励 * W_position

其中：
- W_keyword = 0.4
- W_bm25 = 0.3
- W_vector = 0.3
- W_position = 0.05
```

### 3.2 关键词匹配分数

```
关键词匹配分数 = 匹配度 + 精确匹配奖励

匹配度 = 匹配词数 / 查询总词数

精确匹配奖励 = 0.2 (如果文档内容以查询开头)
```

### 3.3 分数计算示例

```cpp
// 查询: "RAG 系统检索"
// 文档: "RAG 系统是一种检索增强生成技术"

// 分词结果
query_words = ["rag", "系统", "检索"]
doc_words = ["rag", "系统", "是", "一种", "检索", "增强", "生成", "技术"]

// 匹配词: rag, 系统, 检索 (3个)
// 匹配度 = 3/3 = 1.0

// 文档不以查询开头，精确匹配奖励 = 0
// 关键词分数 = 1.0 + 0 = 1.0
```

### 3.4 分数归一化

| 分数类型 | 归一化方法 |
|----------|------------|
| 关键词匹配 | 已在 [0, 1.2] 范围 |
| BM25 | Min-Max 归一化到 [0, 1] |
| 向量分数 | 已在 [0, 1] 范围 |
| 综合分数 | 在 [0, 1] 范围 |

## 4. 行为规范

### 4.1 输入输出

| 项目 | 规范 |
|------|------|
| 输入查询 | 非空字符串 |
| 输入候选 | 1-1000 个 Chunk |
| 输出结果 | 最多 top_n 个，按分数降序 |
| 空输入 | 返回空结果 |

### 4.2 分数说明

| 分数范围 | 含义 |
|----------|------|
| 0.8 - 1.0 | 高度相关 |
| 0.6 - 0.8 | 中度相关 |
| 0.4 - 0.6 | 低度相关 |
| < 0.4 | 不相关 |

### 4.3 异常处理

| 场景 | 行为 |
|------|------|
| 空候选列表 | 返回空结果 |
| 查询为空 | 返回空结果 |
| top_n <= 0 | 返回空结果 |
| top_n > 候选数 | 返回全部候选 |

## 5. 配置规范

### 5.1 配置文件

```yaml
reranker:
  # 类型: lightweight | bge
  type: "lightweight"
  
  # 模型路径 (用于 BGE)
  model_path: "./models/bge-reranker"
  
  # 权重配置 (Lightweight)
  weights:
    keyword: 0.4
    bm25: 0.3
    vector: 0.3
    position: 0.05
```

### 5.2 BGE-Reranker 配置

```cpp
struct BGERerankerConfig {
    std::string model_path;
    std::string device = "cpu";
    int batch_size = 32;
    float normalize = true;  // 是否归一化分数
};
```

## 6. 性能规范

### 6.1 延迟要求

| 重排序器 | 50 候选 | 100 候选 | 500 候选 |
|----------|---------|----------|----------|
| Lightweight | < 5ms | < 10ms | < 50ms |
| BGE-Reranker | < 500ms | < 1000ms | < 5000ms |

### 6.2 内存要求

| 重排序器 | 内存 |
|----------|------|
| Lightweight | < 1MB |
| BGE-Reranker | < 1GB |

## 7. 测试规范

### 7.1 单元测试

```cpp
TEST(LightweightReranker, ReturnsCorrectCount) {
    LightweightReranker reranker;
    
    std::vector<Chunk> candidates = {
        Chunk{"1", "test content 1"},
        Chunk{"2", "test content 2"},
        Chunk{"3", "test content 3"}
    };
    
    auto results = reranker.rerank("test", candidates, 2);
    EXPECT_EQ(results.size(), 2);
}

TEST(LightweightReranker, SortedByScore) {
    LightweightReranker reranker;
    
    std::vector<Chunk> candidates = {
        Chunk{"1", "unrelated content"},
        Chunk{"2", "test keyword"},
        Chunk{"3", "test keyword in detail"}
    };
    
    auto results = reranker.rerank("test", candidates, 3);
    
    // 验证排序
    for (size_t i = 1; i < results.size(); i++) {
        EXPECT_GE(results[i-1].score, results[i].score);
    }
}

TEST(LightweightReranker, KeywordMatching) {
    LightweightReranker reranker;
    
    // 设置向量分数
    std::unordered_map<std::string, float> vec_scores = {
        {"1", 0.5},
        {"2", 0.8}
    };
    reranker.set_vector_scores(vec_scores);
    
    std::vector<Chunk> candidates = {
        Chunk{"1", "hello world"},      // 0 关键词匹配
        Chunk{"2", "test hello world"}  // 1 关键词匹配
    };
    
    auto results = reranker.rerank("test", candidates, 2);
    
    // "test hello world" 应该排在前面
    EXPECT_GT(results[0].score, results[1].score);
}
```

### 7.2 集成测试

```cpp
TEST(LightweightReranker, IntegratesWithRetrieval) {
    // 模拟完整检索流程
    std::string query = "RAG system";
    
    // 阶段1: 粗召回
    std::vector<Chunk> candidates = {
        Chunk{"1", "RAG is retrieval augmented generation"},
        Chunk{"2", "Vector database similarity search"},
        Chunk{"3", "RAG combines retrieval and generation"},
        Chunk{"4", "Natural language processing basics"}
    };
    
    // 阶段2: 精排序
    LightweightReranker reranker;
    auto results = reranker.rerank(query, candidates, 2);
    
    // 验证 RAG 相关内容排在前面
    EXPECT_TRUE(results[0].content.find("RAG") != std::string::npos);
}
```

## 8. 扩展接口

### 8.1 异步重排序

```cpp
class AsyncReranker {
public:
    using RerankFuture = std::future<std::vector<RerankResult>>;
    
    RerankFuture rerank_async(
        const std::string& query,
        const std::vector<Chunk>& candidates,
        int top_n);
};
```

### 8.2 统计信息

```cpp
struct RerankerStats {
    uint64_t rerank_count;
    uint64_t total_candidates;
    double avg_latency_ms;
    size_t memory_usage_bytes;
};

virtual RerankerStats get_stats() const;
```

## 9. 与其他组件集成

### 9.1 EnhancedRetriever 集成

```cpp
class EnhancedRetriever {
    // 在 retrieve() 中调用
    auto candidates = base_retriever_->retrieve(query, config_.recall_top_k);
    
    if (config_.enable_rerank && reranker_->is_ready()) {
        auto reranked = reranker_->rerank(query, extract_chunks(candidates), top_k);
        // 合并分数
    }
};
```

### 9.2 分数注入

```cpp
// 在调用 reranker 前注入外部分数
reranker.set_bm25_scores(bm25_scores_map);
reranker.set_vector_scores(vector_scores_map);

auto results = reranker.rerank(query, candidates, top_k);
```
