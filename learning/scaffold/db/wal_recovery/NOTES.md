# WAL 日志格式与崩溃恢复流程

## WAL 日志格式

参考 `engineering/src/db/storage/wal/wal.c` 中的实现：

### 文件头（56 字节）
```c
typedef struct wal_file_header_s {
    uint32_t magic;         // 魔数 0x57414C31
    uint32_t version;       // 版本号
    uint32_t page_size;     // 数据库页面大小
    uint32_t checksum;      // 头部校验和
    uint8_t  reserved[48];  // 保留字段
} wal_file_header_t;
```

### 记录头（24 字节）
```c
typedef struct wal_record_header_s {
    uint8_t  type;        // 操作类型 (BEGIN/INSERT/UPDATE/DELETE/COMMIT/ABORT/CHECKPOINT)
    uint8_t  size[3];     // 数据大小（小端序，3 字节）
    uint32_t lsn;         // Log Sequence Number，日志序列号
    uint32_t txn_id;      // 事务 ID
    uint32_t prev_lsn;    // 前一条日志的 LSN（用于构建日志链）
    uint32_t checksum;    // 校验和
} wal_record_header_t;
```

### 记录数据区
```
key_len (4B) + key (key_len B) + value_len (4B) + value (value_len B)
```

## 崩溃恢复流程

参考 `engineering/src/db/storage/wal/wal_buf.c` 中的实现：

### 恢复步骤

1. **分析阶段（Analysis）**
   - 打开 WAL 文件
   - 遍历所有日志记录
   - 识别已提交和未提交事务
   - 确定恢复起点（上次检查点）

2. **Redo 阶段**
   - 从检查点开始重演所有日志
   - 已提交事务：应用数据变更
   - 未提交事务：暂不提交，等待后续处理

3. **Undo 阶段**（可选）
   - 对崩溃前未提交的事务执行回滚
   - 使用 prev_lsn 构建日志反向链
   - 恢复旧值或删除新数据

### 检查点机制

检查点的核心作用是**缩短恢复时间**：

```
WAL 文件:
[检查点1] [日志...] [检查点2] [日志...] [崩溃点]
              ↑                            ↑
         恢复起点                       恢复终点
```

- 检查点记录包含**脏页列表**
- 恢复时只需重演检查点之后的日志
- 检查点生成：刷脏页 → 写检查点日志 → 更新 checkpoint_lsn

### WAL 与 Buffer Pool 协调

参考 `wal_buf_mark_dirty()` 函数：
1. 页面被修改时标记为脏
2. 同时写入 WAL 日志（记录完整页面内容）
3. 获取该页面的 LSN 并关联到 Buffer
4. 事务提交时确保 WAL 已刷盘（`wal_flush`）

### 关键设计原则

1. **Write-Ahead 原则**：数据页刷盘前，对应 WAL 必须先刷盘
2. **LSN 追踪**：每个 Buffer 记录最后修改的 LSN，用于判断是否需要 Redo
3. **组提交**：批量提交日志，减少 I/O 次数
4. **检查点频率**：通过 `wal_need_checkpoint()` 控制，平衡恢复时间与性能开销
