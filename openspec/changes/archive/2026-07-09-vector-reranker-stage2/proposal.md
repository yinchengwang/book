# Proposal: ReRanker 二阶段精排

## 1. Summary / 摘要

在自研向量索引中实现两阶段检索（粗排 + 精排）：
- Stage 1：HNSW/DiskANN 返回 Top-K 候选（使用 PQ 近似距离）
- Stage 2：精确距离重排 + 多度量融合 + MMR 去重

## 2. Motivation / 动机

当前向量检索存在精度损失：
- PQ 量化导致近似距离误差
- 单一度量无法覆盖所有场景
- 结果缺乏多样性（MMR）

## 3. What Changes / 变更内容

### 3.1 核心架构

```
┌─────────────────────────────────────────────────────────────┐
│                    两阶段检索                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Query ──→ Stage 1: HNSW(k=100) ─→ candidates[]            │
│               │                                              │
│               ▼                                              │
│  Stage 2: 精确距离重排 ─→ Top-K 结果                        │
│    ├─ 精确 L2/IP 距离（如果 Stage1 用 PQ）                   │
│    ├─ 多度量融合（L2 + Cosine + IP 加权）                   │
│    └─ MMR 去重（可选）                                       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 3.2 新增组件

| 组件 | 文件 | 说明 |
|------|------|------|
| ReRanker 接口 | `reranker.h` | 抽象重排能力 |
| 精确距离精排 | `precise_reranker.h/c` | 精确距离计算 |
| 多度量融合 | `multi_metric_reranker.h/c` | 多度量加权融合 |
| MMR 去重 | `mmr_reranker.h/c` | 最大边际相关性 |
| 两阶段入口 | `hybrid_search.h/c` | 集成到现有检索 |

### 3.3 API 变更

```c
// 新增两阶段检索 API
int32_t vector_index_search_2stage(vector_index_t *index,
                                    const float *query,
                                    int32_t k,
                                    int32_t stage1_k,
                                    float *distances,
                                    int32_t *ids);

// 新增 ReRanker API
typedef int32_t (*rerank_func_t)(const float *query,
                                  const float *candidates,
                                  int32_t n_candidates,
                                  int32_t k,
                                  float *scores,
                                  int32_t *reranked_ids);

int32_t reranker_register(const char *name, rerank_func_t func);
int32_t reranker_apply(const char *name,
                        const float *query,
                        const float *candidates,
                        int32_t n_candidates,
                        int32_t k,
                        float *scores,
                        int32_t *reranked_ids);

// MMR 去重 API
int32_t mmr_rerank(const float *vectors,
                   int32_t *ids,
                   int32_t n,
                   int32_t k,
                   float lambda,
                   int32_t *reranked_ids);
```

## 4. Backward Compatibility / 向后兼容性

- 现有 `vector_index_search()` API 保持不变
- 新增 `vector_index_search_2stage()` 作为可选高级 API
- 默认行为与现有搜索一致

## 5. Out of Scope / 范围外

- Cross-Encoder 模型集成（需要外部依赖）
- 学习式重排（长期规划）

## 6. Alternatives / 替代方案

| 方案 | 优点 | 缺点 |
|------|------|------|
| 方案A：仅精确距离 | 精度最高 | 慢，无法用于大数据集 |
| **方案B：两阶段（推荐）** | 平衡精度与性能 | 实现复杂度中等 |
| 方案C：全量精确搜索 | 简单 | 内存/时间开销大 |

## 7. Success Metrics / 成功指标

| 指标 | 目标 |
|------|------|
| Recall@10 | 提升至 ≥ 95%（vs 当前 PQ 80%） |
| Stage 2 开销 | < 5ms（100 候选） |
| MMR 多样性 | 覆盖 ≥ 3 个语义簇 |

## 8. Status / 状态

- [ ] proposal.md - 本文档
- [ ] design.md - 待创建
- [ ] tasks.md - 待创建
