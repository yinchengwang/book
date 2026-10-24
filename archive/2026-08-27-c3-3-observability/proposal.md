# C3-3 可观测日志引擎（自研）Proposal

## Why

差距分析 §5.4 + 用户全自研决策：现有 Document/Timeseries/Vector 各自有部分可观测能力片段（BM25、Timeseries 指标、Vector 语义），但缺标签索引（高基数切片）、LogQL 子集、列式日志块、流式摄取——Loki/ClickHouse Observability 的核心能力组合。集成 ClickHouse 是更省力方案但用户选定全部自研。

## What Changes

- 新模块 `storage/observability/`
- **LogQL 子集**：`{service="x", level="error"}` 流选择器 + `|= "keyword"` 行过滤 + `| json` 字段提取 + rate/count 聚合
- **标签索引**：label → stream-id 集合（复用 doc_inverted 倒排）
- **高基数保护**：单标签基数超阈值自动降级全扫 + 告警（阈值 GUC）
- **按流分块列式存储**：每 (stream, 时间窗) 一个 chunk，列式布局（时间戳列 + 原始行 + 提取字段列），复用 ts_compress（时间戳列）与 RLE/zstd-like（原始行）
- **流式摄取 API**：`log_push(stream_labels, lines[])` 批量推送 + 内存写缓冲
- 复用 ts_retention 按时间窗 drop
- 接入共享 WAL（log append 记录）
- 写单流锁 + 读多流并行（mmdb_lock）

## Capabilities

| 能力 | 交付 |
|------|------|
| LogQL 子集 | 服务/级别标签过滤 + 关键字行过滤 + rate/count 聚合 |
| 高基数保护 | 超阈值标签降级 + 告警 |
| 流式摄取 | 批量推送 API，吞吐 ≥10K lines/s 单实例 |
| 列式压缩 | 行压缩率 ≥5x（与 Timeseries 同基准） |

## Impact

- 新增：storage/observability/ 全套（logql.c、tag_index.c、log_chunk.c、stream_engine.c）
- 预计 10-12 个 commit
- 依赖：C0-1、C0-2、C2-4
