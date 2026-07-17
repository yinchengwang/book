# Tasks: ReRanker 二阶段精排

## 任务概述

在自研向量索引中实现两阶段检索（粗排 + 精排），提升检索精度。

## 任务清单

### B.1 ReRanker 接口设计

- [x] **B.1.1** 创建 `src/db/index/vector_index/reranker/` 目录
- [x] **B.1.2** 定义 `reranker_t` 接口结构
- [x] **B.1.3** 定义 `reranker_config_t` 配置结构
- [x] **B.1.4** 实现 `reranker_create()` / `reranker_destroy()`
- [x] **B.1.5** 实现 `reranker_register()` 注册工厂
- [x] **B.1.6** 添加默认 ReRanker 实现

### B.2 精确距离精排

- [x] **B.2.1** 设计 `precise_reranker_t` 结构
- [x] **B.2.2** 实现 `precise_rerank()` 精确距离计算
- [x] **B.2.3** 优化批量距离计算（SIMD）
- [x] **B.2.4** 添加距离类型支持（L2/Cosine/IP）
- [x] **B.2.5** 编写测试用例验证精度

### B.3 多度量融合

- [x] **B.3.1** 设计 `multi_metric_reranker_t` 结构
- [x] **B.3.2** 实现多度量距离计算
- [x] **B.3.3** 实现加权融合逻辑
- [x] **B.3.4** 添加度量权重配置
- [x] **B.3.5** 编写测试用例验证融合效果

### B.4 MMR 去重

- [x] **B.4.1** 设计 `mmr_reranker_t` 结构
- [x] **B.4.2** 实现 MMR 算法
- [x] **B.4.3** 添加 lambda 参数配置
- [x] **B.4.4** 优化 MMR 性能（预计算相似度）
- [x] **B.4.5** 编写测试用例验证多样性

### B.5 两阶段检索集成

- [x] **B.5.1** 设计 `hybrid_search_t` 结构
- [x] **B.5.2** 实现 `vector_index_search_2stage()` API
- [x] **B.5.3** 集成 Stage1（HNSW/DiskANN 粗排）
- [x] **B.5.4** 集成 Stage2（精排 + MMR）
- [x] **B.5.5** 添加 Stage 配置参数
- [x] **B.5.6** 添加性能监控

### B.6 测试与 Benchmark

- [x] **B.6.1** 编写精确距离精排单元测试
- [x] **B.6.2** 编写多度量融合测试
- [x] **B.6.3** 编写 MMR 去重测试
- [x] **B.6.4** 编写两阶段集成测试
- [x] **B.6.5** Benchmark：Recall@K 曲线
- [x] **B.6.6** Benchmark：Stage 2 延迟开销
- [x] **B.6.7** Benchmark：MMR 多样性指标

### B.7 文档与示例

- [x] **B.7.1** 编写设计文档 `DESIGN.md`
- [x] **B.7.2** 编写 API 使用示例
- [x] **B.7.3** 添加配置说明

---

## 任务依赖关系

```
B.1 ReRanker 接口
    │
    ├── B.2 精确距离精排
    │
    ├── B.3 多度量融合
    │
    ├── B.4 MMR 去重
    │
    └── B.5 两阶段检索集成
            │
            ├── B.6 测试与 Benchmark
            │
            └── B.7 文档与示例
```

## 状态

- [ ] 未开始
- [ ] 进行中
- [x] 已完成

完成时间: 2026-07-09

## 工时估算

| 阶段 | 任务 | 估算工时 |
|------|------|----------|
| ReRanker 接口 | B.1 | 4h |
| 精确距离精排 | B.2 | 6h |
| 多度量融合 | B.3 | 4h |
| MMR 去重 | B.4 | 4h |
| 两阶段集成 | B.5 | 6h |
| 测试 | B.6 | 6h |
| 文档 | B.7 | 2h |
| **总计** | | **32h** |
