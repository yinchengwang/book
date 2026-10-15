# P1-A 设计

> 详细实施步骤见 `docs/superpowers/plans/2026-10-16-vector-wal-complete.md`

## 数据契约

### WAL Record Header（已有，详见 `engineering/include/db/storage/wal/wal.h`）

```c
typedef struct wal_record_header_s {
    uint8_t  type;           // WAL_LOG_VECTOR_APPEND = 26
    uint8_t  size[3];        // record 总大小
    uint64_t lsn;
    uint32_t txn_id;
    uint32_t prev_lsn;
    uint32_t checksum;
} wal_record_header_t;  // 24 字节
```

### Vector Record Payload（本变更新增）

```
[uint64_t vector_id][int32_t dim][float[dim]·vector_data][uint32_t metadata_len][uint8_t[metadata_len]·metadata]
```

## 关键 API

### `vector_wal_replay()`（新增）

```c
/**
 * @brief 从 checkpoint_lsn 之后重放所有 WAL 记录
 * @param wal  vector_wal_t 句柄
 * @param db   vector_engine_db_t 目标引擎
 * @return 0 成功
 */
int vector_wal_replay(vector_wal_t *wal, vector_engine_db_t *db);
```

### `vector_api_save()`（修改）

```c
int vector_api_save(vector_api_t *api) {
    // 1. 强制 WAL 刷盘（新增）
    if (api->wal) {
        int rc = wal_flush((wal_t *)api->wal);
        if (rc != 0) return rc;
    }
    // 2. 原有 page flush
    ...
}
```

### `vector_api_load()`（修改）

```c
int vector_api_load(vector_api_t *api) {
    // 1. 原有 page reload
    ...
    // 2. 新增 WAL 重放
    if (api->wal && api->engine) {
        vector_wal_replay((vector_wal_t *)api->wal,
                          (vector_engine_db_t *)api->engine);
    }
}
```

## 测试设计

`vdb_wal_recovery_test.cpp` 4 用例：

| 用例 | 验证 |
|------|------|
| `SaveThenLoadPreservesData` | save + load 完整保留 100 条 |
| `WALReplayRecoversPostSaveInserts` | save 后再插 50，崩溃后能恢复到 150（或至少 100） |
| `MultipleSavesAccumulate` | 三次 save 累积 150 条 |
| `EmptyCollectionSaveLoad` | 空集合 save / load 不崩 |

## 失败模式

| 失败 | 退路 |
|------|------|
| `wal_read_record` API 不存在 | 自行实现 scan 逻辑（参考 `wal_recover.c`） |
| vector_id 重复 | idempotent insert（已存在则跳过） |
| WAL 格式变更 | 保留向后兼容，versioned header |