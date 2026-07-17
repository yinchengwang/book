# RAG Evaluation Spec

## Purpose

定义 RAG 系统评估框架的能力规格，支持检索质量和生成质量的量化评估。

## ADDED Requirements

### Requirement: 检索质量指标

RAG 评估器 SHALL 支持计算检索质量指标。

#### Scenario: Recall@K 计算
- **WHEN** 评估器接收查询、真实相关文档集和检索结果
- **WHEN** 计算 Recall@10
- **THEN** 返回 `|检索结果 ∩ 真实相关| / |真实相关|`

#### Scenario: MRR (Mean Reciprocal Rank) 计算
- **WHEN** 评估器接收多个查询及其检索结果
- **WHEN** 计算 MRR
- **THEN** 返回 `mean(1/rank_i)`，其中 rank_i 是第 i 个查询第一个相关文档的排名

#### Scenario: NDCG@K 计算
- **WHEN** 评估器接收带相关性等级的检索结果
- **WHEN** 计算 NDCG@10
- **THEN** 返回 `DCG@K / IDCG@K`，其中 DCG 考虑相关性等级衰减

### Requirement: Faithfulness (忠实度) 指标

Faithfulness SHALL 衡量答案对上下文的忠实程度。

#### Scenario: Faithfulness 计算
- **WHEN** 评估器接收问题、上下文和答案
- **WHEN** 计算 Faithfulness
- **THEN** 将答案拆分为独立陈述，逐个检查是否可从上下文推导
- **THEN** 返回 `(可推导陈述数) / (总陈述数)`

#### Scenario: Faithfulness = 1.0 (完全忠实)
- **WHEN** 答案所有陈述都能从上下文直接推导
- **THEN** Faithfulness = 1.0

#### Scenario: Faithfulness = 0.0 (完全不忠实)
- **WHEN** 答案所有陈述都无法从上下文推导
- **THEN** Faithfulness = 0.0

### Requirement: Answer Relevance (答案相关性) 指标

Answer Relevance SHALL 衡量答案与问题的相关性。

#### Scenario: Answer Relevance 计算
- **WHEN** 评估器接收问题和答案
- **WHEN** 计算 Answer Relevance
- **THEN** 使用 LLM 生成 N 个与答案相关的问题变体（默认 N=3）
- **THEN** 计算原始问题与生成问题的平均余弦相似度

#### Scenario: 相关答案得分高
- **WHEN** 答案直接回答问题
- **THEN** Answer Relevance > 0.8

#### Scenario: 不相关答案得分低
- **WHEN** 答案与问题无关
- **THEN** Answer Relevance < 0.3

### Requirement: Context Relevance (上下文相关性) 指标

Context Relevance SHALL 衡量检索上下文与问题的相关性。

#### Scenario: Context Relevance 计算
- **WHEN** 评估器接收问题和上下文
- **WHEN** 计算 Context Relevance
- **THEN** 使用 LLM 识别上下文中的关键句子
- **THEN** 返回 `(相关句子数) / (总句子数)`

#### Scenario: 高度相关上下文
- **WHEN** 上下文包含所有回答问题所需信息
- **THEN** Context Relevance > 0.9

#### Scenario: 低相关上下文
- **WHEN** 上下文大部分与问题无关
- **THEN** Context Relevance < 0.5

### Requirement: RAGAS 综合得分

RAGAS SHALL 提供综合评估得分。

#### Scenario: RAGAS 综合评分
- **WHEN** 评估器接收问题、上下文、答案
- **WHEN** 计算 RAGAS 得分
- **THEN** 返回包含 faithfulness、answer_relevance、context_relevance 的结构
- **THEN** 提供总体评分：`0.3 * faithfulness + 0.4 * answer_relevance + 0.3 * context_relevance`

### Requirement: 评估结果报告

评估器 SHALL 生成可读的评估报告。

#### Scenario: 单次查询评估报告
- **WHEN** 评估器对单次查询进行评估
- **THEN** 生成包含所有指标的 JSON 报告

#### Scenario: 批量评估报告
- **WHEN** 评估器对多个查询进行评估
- **THEN** 生成汇总统计：平均值、标准差、百分位数

### Requirement: 基准数据集支持

评估器 SHALL 支持导入和使用基准数据集。

#### Scenario: 加载 JSON 数据集
- **WHEN** 调用 `load_dataset("rag_benchmark.json")`
- **THEN** 解析文件中的 QA 对和数据

#### Scenario: 数据集格式
- **WHEN** 数据集包含 question、ground_truths、contexts 字段
- **THEN** 评估器正确解析并用于评估

## 数据结构

```cpp
// 检索指标
struct RetrievalMetrics {
    double recall_at_k;     // Recall@K
    double mrr;             // MRR
    double ndcg_at_k;      // NDCG@K
    double precision_at_k;  // Precision@K
};

// RAGAS 指标
struct RAGEvaluation {
    double faithfulness;      // 忠实度 [0, 1]
    double answer_relevance; // 答案相关性 [0, 1]
    double context_relevance;// 上下文相关性 [0, 1]
    double ragas_score;      // 综合得分 [0, 1]
};

// 评估结果
struct EvaluationResult {
    std::string question;
    std::string answer;
    std::vector<std::string> contexts;
    RetrievalMetrics retrieval;
    RAGEvaluation ragas;
    int64_t latency_ms;  // 端到端延迟
};

// 评估器接口
class RAGEvaluator {
public:
    // 评估单次查询
    EvaluationResult evaluate(
        const std::string& question,
        const std::string& answer,
        const std::vector<std::string>& contexts,
        const std::vector<std::string>& ground_truths);

    // 评估数据集
    std::vector<EvaluationResult> evaluate_dataset(
        const std::string& dataset_path,
        RAGEngine* engine);

    // 生成报告
    std::string generate_report(
        const std::vector<EvaluationResult>& results);
};
```

## 评估指标公式

### Recall@K
```
Recall@K = |Retrieved ∩ Relevant| / |Relevant|
```

### MRR
```
MRR = (1/N) * Σ(1/rank_i)
```
其中 rank_i 是第 i 个查询第一个相关文档的排名。

### NDCG@K
```
DCG@K = Σ(rel_i / log2(i+1)), i=1 to K
IDCG@K = DCG@K with ideal ordering
NDCG@K = DCG@K / IDCG@K
```

### Faithfulness
```
Faithfulness = (supported_statements) / (total_statements)
```

### Answer Relevance
```
Answer_Relevance = (1/N) * Σ cosine_similarity(Q, Q'_i)
```
其中 Q 是原始问题，Q'_i 是 LLM 生成的相关问题。

## 验收标准

- [ ] RetrievalMetrics 正确计算 Recall@K, MRR, NDCG@K
- [ ] Faithfulness 能区分忠实和不忠实的答案
- [ ] Answer Relevance 与人工判断相关性 > 0.7
- [ ] Context Relevance 正确识别相关上下文
- [ ] RAGAS 综合得分范围 [0, 1]
- [ ] 支持 JSON 格式基准数据集导入
- [ ] 生成包含平均值、百分位数的汇总报告
