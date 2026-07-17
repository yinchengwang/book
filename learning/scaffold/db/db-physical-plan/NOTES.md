# NOTES.md - 物理计划工程对照

## 物理操作符

| 逻辑算子 | 物理算子 | 说明 |
|---------|---------|------|
| Scan | SeqScan, IndexScan, IndexOnlyScan | 扫描方式 |
| Join | HashJoin, NestLoop, MergeJoin | 连接方式 |
| Aggregate | HashAgg, SortAgg | 聚合方式 |

## 代价模型

```
SeqScan: rows × page_cost
IndexScan: height + leaf_pages + match_rows
HashJoin: build_cost + probe_cost
```

## 优化策略

1. **Join 重排**: 选择最小代价的 Join 顺序
2. **索引选择**: 根据选择性选择最优索引
3. **聚合策略**: HashAgg vs SortAgg

## 面试高频题

1. HashJoin vs NestedLoop 何时使用
2. 索引覆盖扫描的原理
3. CBO 如何选择执行计划