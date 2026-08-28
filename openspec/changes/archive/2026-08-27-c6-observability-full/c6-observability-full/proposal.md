# C6 可观测日志引擎完整路径 Proposal

## Why

C3-3 仅落地 log_engine 骨架（log_push / log_query / log_aggregate 算子），但完整路径（标签索引、列式块、WAL 接入、TTL drop、集成测试）未完成。

本变更完成批次 B 的核心实装：
- 标签倒排索引（按标签 → stream-id 集合）
- 流式多线程写锁 + 读并行
- WAL 接入（log_push 后写 redo）
- TTL drop 后台调度

## What Changes

- **log_engine**：tags 倒排 + label 索引文件 + ts_retention 接入
- **高基数保护**：阈值（GUC） + 降级全扫
- **WAL 接入**：log_push 写 WAL record，db_startup_recover 重放
- **集成测试**：log_push → log_query → log_rate → log_aggregate 全链路
- **Ts_retention 接入**：定期清理过期 stream

## Capabilities

| 能力 | 交付 |
|------|------|
| 标签倒排 | label → stream 集合 O(1) 查询 |
| WAL 集成 | log append 写 redo，崩溃可恢复 |
| 高基数保护 | 超阈值标签降级全扫 |
| TTL drop | ts_retention 自动清理过期 stream 文件 |

## Impact

- 修改文件：log_engine.c、log_label_index.c（新）、log_wal.c（新）、log_integration_test.cpp（新）
- 预计 6-8 个 commit
- 依赖：C0-2 WAL（已）、C2-4 ts_retention（已骨架）、C0-1 mmdb_lock（已）

## Commit 总览

| # | 阶段 | 简述 |
|---|------|------|
| 1 | C6.1 | log_label_index 标签倒排 |
| 2 | C6.2 | log_engine 标签解析（接收 LogQL 流选择器 `{k="v"}`） |
| 3 | C6.3 | log_rate 实装（聚合 + 速率函数） |
| 4 | C6.4 | WAL 接入（log_push → wal_write） |
| 5 | C6.5 | TTL drop 后台调度接入 |
| 6 | C6.6 | 高基数保护阈值 + 降级全扫 |
| 7 | C6.7 | log_engine 集成测试（端到端 push→query→aggregate） |
| 8 | Archive | 归档 C6 |
