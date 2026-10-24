# P6-Final 生产就绪 规格

## 能力: vdb-staircase-benchmark

#### Requirement: 阶梯基准测试
系统 SHALL 提供 1K/10K/100K/1M 四档阶梯基准测试，测量指标包括：
- insert speed（vec/s）
- search qps + p50/p99 延迟
- Recall@10（vs GT）

#### Requirement: Recall@10 阈值
- 100K 数据集 SHALL 达到 Recall@10 ≥ 0.95
- 1M 数据集 SHALL 达到 Recall@10 ≥ 0.85

#### Requirement: 测试输出
测试结果 SHALL 同时输出到控制台与 JSON 文件，便于后续分析。

## 能力: vdb-concurrent-stress

#### Requirement: 并发压力测试
系统 SHALL 提供多线程并发压力测试，覆盖：
- 并发插入
- 并发查询
- 读写混合

测试 SHALL 验证：无崩溃、无数据损坏、性能衰减可接受。

#### Scenario: 并发搜索稳定性
- **WHEN** 10 个线程各发起 50 次 HNSW 搜索
- **THEN** 系统 SHALL 全部返回正确结果，无死锁

> **Known Limitation**: 该场景历史上偶发挂起，归档前以 GTEST_SKIP 标记。
> 根因疑似 rwlock 递归/锁顺序，留待后续变更修复。

## 能力: vdb-performance-report

#### Requirement: 性能报告输出
系统 SHALL 输出 Markdown 格式性能报告，包含：
- 测试环境（CPU、内存、编译器）
- 阶梯基准结果
- 压力测试结果
- 结论与建议