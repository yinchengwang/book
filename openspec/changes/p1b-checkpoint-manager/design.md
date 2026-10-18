# P1-B 设计（详见 plan）

**关联 proposal：** `proposal.md`

## 核心思路

`vector_api` 现状是"无 WAL 直写 snapshot 模式"——简单但不能抗进程崩溃。改造为"snapshot + WAL 双层 + 后台 CheckpointManager"：

```
                        ┌─────────────────────────┐
   insert() ──append──> │  vector_wal_append      │ <── 后台 CheckpointManager 周期
                        │  (last_checkpoint_lsn)  │     触发 flush + checkpoint + truncate
                        └──────────┬──────────────┘
                                   │
                        ┌──────────▼──────────────┐
   save() ──flush──>    │  vector_wal_flush       │ ──> 写 collections.meta snapshot
                        └──────────┬──────────────┘
                                   │
                        ┌──────────▼──────────────┐
   load() ──replay──>   │  vector_wal_replay      │ ──> idempotent insert (按 vector_id 去重)
                        │  (从 last_checkpoint_lsn)│
                        └─────────────────────────┘
```

## 数据契约

### vector_wal record payload 格式

| 字段 | 大小 | 说明 |
|------|------|------|
| op | 1 byte | `VECTOR_WAL_OP_INSERT` / `VECTOR_WAL_OP_DELETE` / `VECTOR_WAL_OP_CHECKPOINT` |
| segment_id | 4 bytes | 段 ID（0 为默认段） |
| vector_id | 8 bytes | 向量 ID（idempotent key） |
| dim | 4 bytes | 向量维度 |
| data | dim*4 bytes | float 数组 |
| metadata_len | 4 bytes | 元数据长度 |
| metadata | metadata_len bytes | 元数据二进制 |

### checkpoint_lsn 起点正确性

- `vector_wal_create` 初始化 `last_checkpoint = sizeof(header)`
- `vector_wal_checkpoint` 更新 `last_checkpoint = current_pos`
- `vector_wal_replay` 从 `header.last_checkpoint` 开始读（不是从文件头！）
- `vector_wal_truncate` 删除 `last_checkpoint` 之前的所有数据

## 风险

- **idempotent insert 必须保证**：replay 时按 `vector_id` 去重，否则重复 insert 会导致 HNSW 索引状态错误
- **后台 CheckpointManager 线程安全**：与前台 insert / save 共享 `vector_wal_t`，需 mutex 保护
- **WAL 段文件膨胀**：truncate 失败会导致磁盘占满，需监控 + 告警
- **replay 性能**：HNSW 索引重建是 O(N log N)，100 万向量需分钟级——不优化只保证正确性

## 实施组织

1. **Task 1**：建 OPSX 变更骨架（本目录）
2. **Task 2**：探查 `vector_wal_t` 内部状态机 + 确认 idempotent insert 路径
3. **Task 3**：写失败的 `vdb_checkpoint_test.cpp`（红 5 用例）
4. **Task 4**：实现 `vector_api_insert` 接入 `vector_wal_append`
5. **Task 5**：实现 `vector_api_save` 加 `vector_wal_flush` + `vector_wal_checkpoint`
6. **Task 6**：实现 `vector_api_load` 加 `vector_wal_replay`（idempotent）
7. **Task 7**：实现 CheckpointManager 后台线程
8. **Task 8**：跑 vdb_chaos 3 用例 + vdb_checkpoint 5 用例 + 回归 vdb_wal_recovery + vdb_e2e
9. **Task 9**：收口 + 归档

## 关键架构决策

### 本地 replay helper 替代 `vector_wal_replay`

Task 6 实施时发现 `vector_wal_replay`（`vector_persist.h:690-693`）签名要求 `vector_segment_t*`（HNSW 段对象），而 `vector_api.c` 使用自己的 `VectorCollectionImpl`（扁平 `ids[] + vectors[]` 数组），两者数据模型不兼容。简报假设的回调签名 `vector_api_wal_replay_callback(void *ctx, const vector_wal_op_t *op)` 实际不存在。

**决策**：在 `vector_api.c` 中实现本地静态函数 `vector_api_wal_replay`，复用 `vector_wal.c:644-737` 的 record 解析格式（`VECTOR_WAL_HEADER_SIZE` 跳到 `last_checkpoint`、record header 17 字节、CRC32 校验），但目标改为 `VectorCollectionImpl`（线性查重 + `collection_expand` + `memcpy`）。

**影响**：
- 重复约 70 行 record 解析代码（已尽量复用 `vector_persist.h` 中的常量和枚举）
- 重复本地实现 `wal_crc32`（原 `vector_wal.c:38-52` 是 `static`）
- 顺手修复 `vector_wal.c` 原 replay 的 latent CRC 偏移 bug（Task 6 用 `data_size` 正确偏移读 stored_crc，原代码用 `data_size - 4` 读到 data 末尾 4 字节）
- 未来重构 `vector_wal_replay` 接受通用 record 回调，可消除重复（技术债）

### 测试 isolation 模式 fflush 修复

Task 6 审查发现 `vector_wal_create`（`vector_wal.c:103-109`）写 header 后未调 `fflush`，导致 isolation 模式（单跑某个 gtest 用例）下，后续 `fread` 读到空文件。已在 `vector_wal.c:110` 插入 `fflush(wal->fp);` 修复。
