# P5 Spec —— Raft Persistence

## 1. 持久化字段

`raft-state.bin` 包含：
- magic: "RAFTv0.1" (8 bytes)
- current_term: uint64_le
- voted_for: uint64_le
- log_size: uint64_le
- 之后每条 entry：term + data_size + data

## 2. 触发时机

- `tick()` 末尾（如状态变更）
- `raft_submit()` 成功时
- 外部 `raft_server_save_state()`

## 3. 路径

`RaftStateConfig_t::state_path = NULL` → in-memory（向后兼容）
否则每次 dirty 后 sync 写盘

## 4. 不做

- ❌ Snapshot
- ❌ Async thread
- ❌ WAL integration
