# WAL 持久化原理

## 1. 概述

WAL（Write-Ahead Logging）是数据库保证数据持久化和原子性的核心技术。其核心原理是：**在修改数据页之前，先将修改操作写入日志**。

### 1.1 为什么需要 WAL？

```
直接修改数据页的问题：

┌─────────────────────────────────────────────────────────────┐
│  正常操作：                                                     │
│                                                             │
│  1. 读取页面 P 到内存                                          │
│  2. 修改页面 P 中的元组                                        │
│  3. 写回页面 P（标记为脏页）                                    │
│  4. 等待 fsync 完成（确保写入磁盘）                              │
│                                                             │
│  崩溃场景：                                                     │
│                                                             │
│  1. 读取页面 P 到内存                                          │
│  2. 修改页面 P 中的元组                                        │
│  3. 系统崩溃！💥                                                 │
│                                                             │
│  问题：                                                       │
│  - 直接修改了页面（破坏了一致性）                               │
│  - 无法恢复修改前的状态                                        │
│  - 部分修改的页面可能损坏                                      │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 WAL 的解决方案

```
WAL 操作流程：

┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  1. WAL 写入修改记录（顺序追加到日志文件）                     │
│                                │                            │
│                                ▼                            │
│  2. 确认 WAL 写入成功（fsync）                               │
│                                │                            │
│                                ▼                            │
│  3. 修改内存中的页面                                          │
│                                │                            │
│                                ▼                            │
│  4. 标记页面为脏（后台异步刷盘）                               │
│                                │                            │
│                              ✓ 即使步骤 4 前崩溃，WAL 中也有完整记录  │
└─────────────────────────────────────────────────────────────┘
```

## 2. WAL 记录格式

### 2.1 记录类型

| 类型 | 说明 | 典型内容 |
|------|------|---------|
| BEGIN | 事务开始 | xid, 开始 LSN |
| INSERT | 插入记录 | xid, 表 OID, 元组数据 |
| UPDATE | 更新记录 | xid, 表 OID, 旧值, 新值 |
| DELETE | 删除记录 | xid, 表 OID, 旧值 |
| COMMIT | 事务提交 | xid, 结束 LSN |
| ABORT | 事务回滚 | xid, 回滚 LSN |
| CHECKPOINT | 检查点 | 活跃事务列表, 脏页列表 |

### 2.2 记录结构

```c
typedef struct WALRecordHeader {
    uint32_t xl_tot_len;     // 记录总长度
    uint32_t xl_xid;         // 事务 ID
    uint32_t xl_prev;        // 前一个记录的偏移
    uint32_t xl_info;        // 标志位（记录类型等）
    uint64_t xl_checksum;    // 校验和（可选）
} WALRecordHeader;

// INSERT 记录
typedef struct WALInsertRecord {
    WALRecordHeader header;
    uint32_t xl_tbl_oid;     // 表的 OID
    uint16_t xl_key_len;     // 键长度
    uint16_t xl_val_len;     // 值长度
    uint8_t  xl_data[];      // 键 + 值数据
} WALInsertRecord;
```

### 2.3 WAL 文件结构

```
WAL 文件布局：

┌─────────────────────────────────────────────┐
│           WAL 文件 (默认 16MB)               │
├─────────────────────────────────────────────┤
│  Record 1: BEGIN (txn=1)                    │
│  Record 2: INSERT (txn=1, key, value)       │
│  Record 3: COMMIT (txn=1)                   │
│  Record 4: BEGIN (txn=2)                    │
│  Record 5: UPDATE (txn=2, old, new)         │
│  Record 6: ABORT (txn=2)                    │
│  Record 7: CHECKPOINT                       │
│  ...                                        │
│                                             │
│  空闲空间                                    │
│                                             │
├─────────────────────────────────────────────┤
│  EOF                                        │
└─────────────────────────────────────────────┘
```

## 3. LSN 管理

### 3.1 LSN 体系

LSN（Log Sequence Number）是 WAL 的核心标识：

```c
typedef uint64_t XLogRecPtr;  // LSN = 文件号 + 文件内偏移

// 关键 LSN 类型
typedef struct {
    XLogRecPtr last_flushed_lsn;    // 最后刷盘的 LSN
    XLogRecPtr last_parsed_lsn;     // 最后解析的 LSN
    XLogRecPtr last_checkpoint_lsn; // 最后一个检查点的 LSN
} LSNState;

/**
 * @brief 分配新的 LSN
 *
 * LSN 单调递增，全局唯一：
 * 1. 前 32 位：WAL 文件号
 * 2. 后 32 位：文件内偏移
 */
XLogRecPtr GetNextLSN(void) {
    // 使用原子操作保证唯一性
    return atomic_fetch_add(&GlobalLSN, record_size);
}
```

### 3.2 LSN 在页面中的角色

```
LSN 在恢复中的作用：

页面的 LSN < WAL 记录的 LSN：
  → 页面的修改还未应用，需要 Redo

页面的 LSN >= WAL 记录的 LSN：
  → 页面的修改已应用，跳过该记录

每个页面都有一个 LSN 字段（PageHeader.pd_lsn）：
┌──────────────────┐
│ PageHeader       │
│                  │
│ pd_lsn: 0x1234   │ ← 最后一次修改此页面时的 LSN
│                  │
├──────────────────┤
│ Page Data        │
│                  │
└──────────────────┘
```

## 4. 日志写入流程

### 4.1 写入过程

```
WAL 写入详细流程：

1. 事务准备写一个修改操作
       │
       ▼
2. 构造 WAL 记录
   ├──→ 填充记录头
   ├──→ 填充记录体
   └──→ 计算校验和
       │
       ▼
3. 写入 WAL Buffer（内存缓冲区）
   ├──→ payload_len <= WAL_BUF_SIZE：追加到当前缓冲区
   └──→ payload_len > WAL_BUF_SIZE：分配新缓冲区
       │
       ▼
4. 事务提交时刷盘
   ├──→ fsync(WAL 文件)
   └──→ last_flushed_lsn = 新 LSN
       │
       ▼
5. 修改数据页（此时数据页还可能在内存）
```

### 4.2 WAL Buffer

```c
typedef struct WALBuffer {
    char *buf;             // 缓冲区指针
    int buf_size;          // 缓冲区大小
    int write_pos;         // 当前写入位置
    bool needs_flush;      // 是否需要刷盘
} WALBuffer;

/**
 * @brief 写入 WAL Buffer
 *
 * WAL Buffer 的作用：
 * 1. 缓冲小记录，减少碎片 IO
 * 2. 批量写入，提高吞吐量
 */
int wal_buffer_write(WALBuffer *buf, const void *data, int len) {
    if (buf->write_pos + len > buf->buf_size) {
        wal_buffer_flush(buf);  // 缓冲区满，先刷盘
    }

    memcpy(buf->buf + buf->write_pos, data, len);
    buf->write_pos += len;
    buf->needs_flush = true;

    return buf->write_pos;
}
```

## 5. 检查点算法

### 5.1 为什么需要检查点？

```
WAL 日志的大小随时间增长：

┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  时间 →                                                      │
│                                                             │
│  ██████████████████████████████████████ (WAL 日志持续增长)    │
│  ↑                                    ↑                    │
│  最后一个检查点                       当前位置                │
│                                                             │
│  如果不做检查点：                                               │
│  - WAL 日志无限增长，磁盘空间耗尽                              │
│  - 崩溃恢复时需要重放所有日志，时间太长                        │
└─────────────────────────────────────────────────────────────┘
```

### 5.2 检查点流程

```
检查点执行过程：

1. 记录检查点开始的 LSN
       │
       ▼
2. 收集所有脏页列表
   ├──→ 遍历 Buffer Pool
   └──→ 记录每个脏页的 LSN
       │
       ▼
3. 写入检查点记录到 WAL
   ├──→ 记录活跃事务列表
   ├──→ 记录脏页列表
   └──→ 记录检查点 LSN
       │
       ▼
4. 刷脏页到磁盘
   ├──→ 逐页 fsync
   └──→ 写入双写缓冲区
       │
       ▼
5. 写入检查点完成标记
   ├──→ 更新 last_checkpoint_lsn
   └──→ 截断旧 WAL 文件
```

### 5.3 检查点代码实现

```c
/**
 * @brief 创建检查点
 *
 * 检查点是恢复的"起点"：
 * - 检查点之前的日志不需要重放
 * - 检查点记录了恢复所需的所有信息
 */
int wal_checkpoint(wal_t *wal, uint32_t *dirty_pages, int n_dirty) {
    // 1. 写入检查点记录
    uint64_t cp_lsn = wal_write_checkpoint(wal, dirty_pages, n_dirty);

    // 2. 刷盘确保检查点记录安全写入
    wal_flush(wal);

    // 3. 刷所有脏页
    for (int i = 0; i < n_dirty; i++) {
        const uint32_t *page_ids = dirty_pages;
        buf_flush_page(page_ids[i]);
    }

    // 4. 更新检查点 LSN
    wal->checkpoint_lsn = cp_lsn;

    // 5. 截断旧日志
    wal_truncate(wal, cp_lsn);

    return 0;
}
```

## 6. 崩溃恢复

### 6.1 ARIES 恢复算法

ARIES (Algorithm for Recovery and Isolation Exploiting Semantics) 是 WAL 恢复的标准算法：

```
ARIES 恢复的三个阶段：

┌─────────────────────────────────────────────────────────────┐
│                                                             │
│ 阶段 1: 分析 (Analysis)                                     │
│  ├──→ 从最后一个检查点开始扫描 WAL                           │
│  ├──→ 重建脏页表                                            │
│  └──→ 识别需要 Undo 的事务                                  │
│                                                             │
│ 阶段 2: 重做 (Redo)                                         │
│  ├──→ 从脏页表中最小的 recLSN 开始                          │
│  ├──→ 按 LSN 顺序重放所有操作                               │
│  └──→ 跳过页 LSN >= 记录 LSN 的记录                         │
│                                                             │
│ 阶段 3: 回滚 (Undo)                                         │
│  ├──→ 回滚分析阶段标记的未提交事务                          │
│  ├──→ 按 LSN 逆序回滚                                       │
│  └──→ 写入 CLR (Compensation Log Record)                   │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 恢复代码实现

```c
/**
 * @brief 崩溃恢复主流程
 *
 * 从 WAL 日志中恢复数据库：
 * 1. 找到最后一个检查点
 * 2. 从检查点开始重放日志
 * 3. 回滚未提交的事务
 */
int wal_recover(const char *wal_file) {
    // 1. 分析 WAL
    wal_recovery_info_t info;
    wal_analyze(wal_file, &info);

    // 2. 找到最后一个检查点
    XLogRecPtr checkpoint = info.checkpoint_lsn;
    if (checkpoint == 0) {
        // 没有检查点，从文件开始
        checkpoint = 0;
    }

    // 3. Redo 阶段：重放所有已完成事务
    wal_redo(wal_file, checkpoint);

    // 4. Undo 阶段：回滚未提交事务
    wal_undo(wal_file, &info);

    // 5. 清理
    wal_recovery_info_free(&info);

    return 0;
}
```

### 6.3 恢复示例

```
恢复场景示例：

WAL 文件：
┌──────────────────────────────────────────────┐
│ CHKPT (CP_LSN=100)                           │  ← 检查点
│ BEGIN (T1, LSN=150)                          │
│ INSERT (T1, key1, val1, LSN=170)             │
│ BEGIN (T2, LSN=200)                          │
│ UPDATE (T2, key1, old1, new1, LSN=220)       │
│ COMMIT (T2, LSN=250)                         │  ← T2 已提交
│ INSERT (T1, key2, val2, LSN=280)             │
│ BEGIN (T3, LSN=300)                          │
│ DELETE (T3, key1, LSN=320)                   │
│ → 系统崩溃 LSN=350                            │
└──────────────────────────────────────────────┘

恢复分析：
- T2 已提交 → 所有 T2 操作在 Redo 阶段完成
- T1 未提交 → Undo T1 的所有操作
- T3 未提交 → Undo T3 的所有操作

最终状态：
- key1 = new1 (T2 的 UPDATE 生效)
- key2 = 不存在 (T1 的 INSERT 被回滚)
```

## 7. WAL 配置

### 7.1 核心参数

| 参数 | 说明 | 影响 |
|------|------|------|
| `wal_level` | 日志级别 | replica 记录足够复制的信息 |
| `wal_buffers` | WAL 缓冲区大小 | 影响写入性能 |
| `wal_writer_delay` | 刷盘延迟 | 影响持久性保证 |
| `fsync` | 是否启用同步刷盘 | 影响性能和数据安全 |
| `full_page_writes` | 完整页面写入 | 防止断页 |
| `wal_compression` | 启用压缩 | 减少 IO 量 |

### 7.2 性能权衡

```
fsync = on vs off：

fsync=on（安全模式）：
- 每次提交都 fsync
- 数据安全，性能较低
- 适用场景：金融、交易系统

fsync=off（性能模式）：
- 不强制 fsync
- 性能高，可能丢失数据
- 适用场景：非关键数据、测试环境

两者的差异可达 10-100 倍
```

## 8. 日志截断

### 8.1 截断策略

```
WAL 截断时间点：

┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  WAL 文件 1    WAL 文件 2    WAL 文件 3    WAL 文件 4       │
│  ████████████ ██████████ ██████████ ██████████            │
│       ▲                                      ▲             │
│       │                                      │             │
│  最后检查点                              当前位置           │
│                                                             │
│  可以安全截断                                                │
│  ──────▶ 这些可以用 checkpoint_lsn 之前的日志来截断           │
│                                                             │
│  截断条件：                                                  │
│  1. 已创建检查点                                            │
│  2. 脏页已刷盘                                              │
│  3. 没有活跃的流复制连接                                     │
└─────────────────────────────────────────────────────────────┘
```

### 8.2 截断实现

```c
/**
 * @brief 截断旧 WAL 文件
 *
 * 安全截断条件：
 * 1. LSN < checkpoint_lsn
 * 2. 所有需要的页面已刷盘
 * 3. 没有备库需要这些日志
 */
void wal_truncate(wal_t *wal, XLogRecPtr target_lsn) {
    // 1. 找到 target_lsn 所在文件
    WalFile *file = wal_file_for_lsn(wal, target_lsn);
    if (!file) return;

    // 2. 删除更早的 WAL 文件
    WalFile *prev = file->prev;
    while (prev) {
        WalFile *to_delete = prev;
        prev = prev->prev;
        wal_file_remove(to_delete);
    }
}
```

## 9. 总结

WAL 的核心要点：

1. **先写日志后改数据**：保证数据页的修改可恢复
2. **LSN 体系**：全局唯一递增序号，精确定位每个修改
3. **检查点**：截断日志起点，优化恢复效率
4. **ARIES 恢复**：三阶段恢复算法（分析→重做→回滚）
5. **WAL Buffer**：聚合写入提高性能
6. **日志截断**：回收空间，控制文件数量

---

*文档版本：v1.0*
*最后更新：2026-07-12*
