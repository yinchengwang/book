# C7 内核与索引特性 Proposal

## Why

C3-5 推迟项中核心是关系/KV/文档引擎的内核实装：Hash 索引、TOAST、FSM、kv_txn、宽表扫描、ltree 兼容。本变更完成批次 C。

## What Changes

- **Hash 索引**：kv_engine.h 旁路接口（hash on page）
- **TOAST**：heap_insert 大元组外存（对接 C3-1 blob）
- **FSM**：bitmap-based Free Space Map（每页 1 bit）
- **kv_txn**：基于 kv_cas 的乐观事务（begin/put/commit）
- **跨 CF WriteBatch**：多 CF 原子写（合并 CAS 检查）
- **ltree 兼容**：路径列类型 + 操作符骨架
- **wide_row 范围扫描**：prefix 扫描完整实装

## Capabilities

| 能力 | 交付 |
|------|------|
| Hash 索引 | O(1) 哈希定位，等值查询 |
| TOAST | heap_insert 自动判断大小，大值外存 |
| FSM | 每页 1 bit 记录空闲状态 |
| kv_txn | 乐观事务：begin/put/commit/rollback |
| WriteBatch | 跨 CF 原子写合并 |
| ltree | 路径列类型 + <@ / @> / ~ 操作符 |

## Impact

- 修改文件：db/hash_index.c（新）、db/toast.c（新）、db/fsm.c（新）、db/kv/txn.c（扩）、db/storage/blob/wide_row.c（完）
- 预计 8-10 个 commit
- 依赖：C3-1 blob_engine（已）、kv_cas（已）、kv_comparator（已）

## Commit 总览

| # | 阶段 | 简述 |
|---|------|------|
| 1 | C7.1 | Hash 索引 htable + bucket page layout |
| 2 | C7.2 | TOAST 大元组外存（heap_insert 自动判断 + blob_id 引用） |
| 3 | C7.3 | FSM bitmap（每页 1 bit，分配时扫描） |
| 4 | C7.4 | kv_txn 乐观事务（基于 kv_cas 的 read-then-write） |
| 5 | C7.5 | 跨 CF WriteBatch（多 CF 合并原子写） |
| 6 | C7.6 | ltree 路径列类型与操作符 |
| 7 | C7.7 | wide_row 范围扫描完整实装 |
| 8 | C7.8 | 集成测试 + Verify + Archive |
