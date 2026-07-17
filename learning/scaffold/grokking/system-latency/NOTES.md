# 延迟分析 学习笔记

## 核心概念

- **P50/P90/P99/P99.9**: 不同百分位的延迟指标，P99 是衡量长尾的关键
- **延迟分解**: 一次请求经过 DNS/TCP/TLS/排队/处理等阶段
- **SLA (Service Level Agreement)**: 服务级别协议，如"P99 ≤ 200ms"
- **抖动 (Jitter)**: P99 - P50 的差值，反映系统稳定性

## 典型延迟参考

| 操作 | 延迟 | 说明 |
|------|------|------|
| L1 缓存访问 | ~1 ns | CPU 一级缓存 |
| 内存访问 | ~100 ns | DDR 内存随机读 |
| SSD 随机读 | ~100 μs | NVMe 延迟 |
| 网络内同机房 | ~500 μs | 局域网 |
| 跨区域网络 | ~50 ms | 跨省/跨国 |
| 数据库查询 | ~10 ms | 索引命中 |

## 工程对照

`engineering/src/db/core/sql_exec.c` 中的执行器包含了查询执行的延迟模型——每个操作符（Seq Scan、Index Scan、NestLoop Join）都有不同的延迟特征。PostgreSQL 风格的优化器基于代价估算选择执行计划，代价估算本质上就是对 IO 和 CPU 延迟的量化。`engineering/include/db/rel.h` 中的 Relation 抽象提供了 `rd_tupdesc` 元组描述，执行器通过它计算元组大小和处理延迟。`engineering/src/db/index/btreeam.c` 中的 BTree 索引查找在页内二分查找的延迟是 O(log n)，而堆表的顺序扫描延迟是 O(n)——这直接对应系统设计中的"不同数据结构的延迟特征"分析。Buffer Pool 的 Clock-Sweep 置换算法（`bufmgr.c`）需在命中率和替换代价之间权衡，其 P50 命中率直接决定了大多数查询的响应延迟。

## 面试要点

1. P99 优化需关注长尾而非均值
2. 延迟分解是性能优化的起点，找出瓶颈阶段
3. 系统设计面试中一般假设 P99 = 2-5× P50
