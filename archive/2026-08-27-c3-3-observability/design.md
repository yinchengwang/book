# C3-3 可观测日志引擎（自研）设计文档

## 设计目标

自研日志存储与查询：标签索引 + 列式日志块 + LogQL 子集 + 流式摄取。对标 Loki/ClickHouse Observability。

## 方案

1. **流式摄取**：`log_push(labels, lines[])` 批量推送 + 内存写缓冲
2. **标签索引**：label → stream-id 集合（复用倒排索引），高基数保护（阈值降级全扫）
3. **列式日志块**：每 (stream, 时间窗) 一个 chunk，列式布局（时间戳列 + 原始行 + 提取字段列）
4. **LogQL 子集**：流选择器 `{service="x",level="error"}` + `|= "keyword"` 行过滤 + rate/count 聚合
5. **复用组件**：ts_compress（时间戳列）、ts_retention（按时间窗 drop）、mmdb_lock（读写锁）

## 实现文件

- `engineering/include/db/log_engine.h`（新增）
- `engineering/src/db/storage/log/log_engine.c`（新增）
