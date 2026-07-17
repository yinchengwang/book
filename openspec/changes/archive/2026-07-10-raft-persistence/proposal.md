# P5 — Raft Persistence (term/voted_for/log)

## What Changes

Phase11 单节点 + P1 多节点 + 日志复制都在 in-memory。P5 加持久化：

1. **持久化字段**：`current_term`、`voted_for`、`log[]` 落盘到 `<path>/raft-state.bin`
2. **REST API**：`raft_state_path_t` config 字段（默认 `NULL`，向后兼容）
3. **RaftState_t struct**（公共 ABI）方便读取状态
4. **atomic snapshot**：每次 `tick()` / `submit()` 后写盘（async 或 sync）

## Why

**α 价值**：
- 真正的分布式共识必须能 crash-recover
- Persistence 让 Raft 真正"可工作"

**前置依赖**：
- phase11 + P1 已完成
- db_core 已提供 WAL framework

## Scope

**包含**：
- `raft_persist.c`：state serial/deserial (binary or text)
- `raft_state_load/save` API
- `raft_test_recovery`：模拟 crash + reload

**不包含**：
- ❌ Snapshot（独立 module）
- ❌ log compaction
- ❌ WAL integration（独立）
