# RAG 评估指南

> 本文档介绍如何使用 RAGAS 评估框架评估 RAG 系统的检索和生成质量。

## 概述

RAG 评估框架（RAGAS）提供了一套完整的指标来量化 RAG 系统的性能，包括：

- **检索质量指标**：Recall@K、MRR、NDCG@K、Precision@K
- **生成质量指标**：Faithfulness、Answer Relevance、Context Relevance
- **综合评分**：RAGAS Score

## 快速开始

### 1. 基本使用

```cpp
#include "rag/evaluator.h"

using namespace rag;

// 创建评估器
RAGEvaluator evaluator;

// 评估单次查询
EvaluationResult result = evaluator.evaluate(
    "什么是 RAG？",                          // 问题
    "RAG 是检索增强生成技术...",             // 答案
    {"上下文1...", "上下文2..."},           // 检索到的上下文
    {"RAG 是检索增强生成"}                   // 真实相关文档
);

// 检查结果
if (result.success) {
    std::cout << "Recall@K: " << result.retrieval.recall_at_k << std::endl;
    std::cout << "MRR: " << result.retrieval.mrr << std::endl;
    std::cout << "Faithfulness: " << result.ragas.faithfulness << std::endl;
    std::cout << "RAGAS Score: " << result.ragas.ragas_score << std::endl;
}
```

### 2. 批量评估

```cpp
// 评估多个查询
std::vector<EvaluationResult> results;
for (const auto& entry : dataset) {
    auto result = evaluator.evaluate(
        entry.question,
        generate_answer(entry.question),  // 通过 RAG 引擎生成
        retrieve_contexts(entry.question),
        entry.ground_truths
    );
    results.push_back(result);
}

// 生成汇总报告
auto summary = evaluator.compute_summary(results);
auto report = evaluator.generate_summary_report(results);

std::cout << report << std::endl;
```

## 评估指标详解

### 检索质量指标

#### Recall@K

衡量检索系统找到的相关文档比例：

```
Recall@K = |Retrieved ∩ Relevant| / |Relevant|
```

**解读**：
- 1.0 = 所有相关文档都被检索到
- 0.0 = 没有检索到任何相关文档

#### MRR (Mean Reciprocal Rank)

衡量第一个相关文档的排名质量：

```
MRR = (1/N) * Σ(1/rank_i)
```

**解读**：
- 1.0 = 所有查询的第一个结果都是相关的
- 0.5 = 平均第一个相关文档在第 2 位
- 0.0 = 没有查询检索到相关文档

#### NDCG@K (Normalized Discounted Cumulative Gain)

衡量排序质量，考虑相关性等级：

```
DCG@K = Σ(rel_i / log2(i+1)), i=1 to K
NDCG@K = DCG@K / IDCG@K
```

**解读**：
- 1.0 = 完美排序
- 0.0 = 最差排序

### 生成质量指标

#### Faithfulness (忠实度)

衡量答案对上下文的忠实程度：

```
Faithfulness = (能从上下文推导的陈述数) / (总陈述数)
```

**解读**：
- 1.0 = 答案完全忠实于上下文，无幻觉
- 0.5 = 一半陈述可从上下文推导
- 0.0 = 答案与上下文完全无关

#### Answer Relevance (答案相关性)

衡量答案与问题的相关程度：

```
Answer_Relevance = avg(cosine_similarity(Q, Q'_i))
```

其中 Q'_i 是 LLM 根据答案生成的相关问题。

**解读**：
- 1.0 = 答案完全回答了问题
- 0.5 = 答案部分相关
- 0.0 = 答案与问题无关

#### Context Relevance (上下文相关性)

衡量检索上下文与问题的相关程度：

```
Context_Relevance = (相关句子数) / (总句子数)
```

**解读**：
- 1.0 = 上下文完全相关
- 0.5 = 一半上下文相关
- 0.0 = 上下文与问题无关

### RAGAS 综合得分

```
RAGAS_Score = w1 * Faithfulness + w2 * Answer_Relevance + w3 * Context_Relevance
```

默认权重：w1=0.3, w2=0.4, w3=0.3

## 使用 LLM 评估

部分指标需要 LLM 来计算：

```cpp
#include "rag/ollama_llm.h"

// 创建 LLM 服务
OllamaLLMConfig llm_config;
auto llm_service = create_ollama_llm_service(llm_config);

// 创建带 LLM 的评估器
RAGEvaluator evaluator(llm_service);

// 使用 LLM 计算 Faithfulness
auto eval = evaluator.evaluate_generation(question, answer, contexts);
// evaluator 会调用 LLM 来评估忠实度和相关性
```

**注意**：没有 LLM 时，评估器会使用简化的基于关键词匹配的算法，结果可能不够准确。

## 基准数据集

项目包含一个基准测试数据集，位于 `test/data/rag_benchmark.json`：

```json
{
  "entries": [
    {
      "id": "q1",
      "question": "什么是 RAG？",
      "ground_truths": ["RAG 是检索增强生成"],
      "contexts": ["RAG 是检索增强生成（Retrieval-Augmented Generation）..."]
    }
  ]
}
```

### 加载数据集

```cpp
RAGEvaluator evaluator;

// 从文件加载
auto entries = evaluator.load_dataset("test/data/rag_benchmark.json");

// 评估数据集
for (const auto& entry : entries) {
    auto result = evaluator.evaluate(
        entry.question,
        "",  // 需要先通过 RAG 引擎生成答案
        entry.contexts,
        entry.ground_truths
    );
}
```

## 生成报告

### JSON 格式报告

```cpp
// 单次评估报告
auto json_report = evaluator.generate_json_report(result);
std::cout << json_report << std::endl;

// 批量评估汇总报告
auto summary_report = evaluator.generate_summary_report(results);
std::cout << summary_report << std::endl;
```

### 示例输出

```json
{
  "total_queries": 10,
  "successful_queries": 10,
  "success_rate": 1.0,
  "retrieval": {
    "avg_recall_at_k": 0.85,
    "avg_mrr": 0.72,
    "avg_ndcg_at_k": 0.78,
    "avg_precision_at_k": 0.68
  },
  "ragas": {
    "avg_faithfulness": 0.82,
    "avg_answer_relevance": 0.75,
    "avg_context_relevance": 0.88,
    "avg_ragas_score": 0.81
  },
  "latency": {
    "avg_ms": 125.5,
    "p50_ms": 110.0,
    "p95_ms": 200.0,
    "p99_ms": 280.0
  }
}
```

## 自定义配置

### 设置 RAGAS 权重

```cpp
RAGEvaluator evaluator;

// 自定义权重
evaluator.set_ragas_weights(
    0.5,   // faithfulness
    0.3,   // answer_relevance
    0.2    // context_relevance
);
```

### 设置检索 K 值

```cpp
// 设置 Recall@K 中的 K 值
evaluator.set_retrieval_k(10);
```

## 评估最佳实践

### 1. 使用真实数据集

```cpp
// 创建多样化的测试集
std::vector<DatasetEntry> test_set = {
    // 简单问题
    {"q1", "什么是 RAG？", {"RAG 是检索增强生成"}},
    // 复杂问题
    {"q2", "解释 HNSW 算法的层次结构如何提高搜索效率？", {...}},
    // 多跳问题
    {"q3", "RAG 系统如何使用知识图谱增强检索？", {...}}
};
```

### 2. 对比不同配置

```cpp
// 测试不同 Embedding 模型
std::vector<std::string> models = {"nomic-embed-text", "bge-m3"};
for (const auto& model : models) {
    config.embedding.model = model;
    auto engine = create_engine(config);
    auto results = evaluate(engine, test_set);
    print_results(model, results);
}
```

### 3. 持续监控

```cpp
// 定期评估生产环境
void monitor_system(RAGEngine& engine) {
    RAGEvaluator evaluator;
    auto results = evaluator.evaluate_dataset(load_latest_queries(), &engine);

    auto summary = evaluator.compute_summary(results);

    // 记录指标
    log_metric("ragas_score", summary.avg_ragas_score);
    log_metric("recall_at_k", summary.avg_recall_at_k);

    // 告警
    if (summary.avg_ragas_score < 0.6) {
        send_alert("RAG 系统性能下降");
    }
}
```

## 下一步

- 查看 [Ollama 集成指南](ollama-integration.md) 了解如何配置本地模型
- 查看 [RAG 系统设计](../README.md) 了解整体架构
