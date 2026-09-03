# Redo Log 详解 - 工程对照

## 本学习卡演示的 Redo Log 在工程轨的实现对照

### 1. WAL（Write-Ahead Logging）原则

**核心原则**：数据修改必须先写入 Redo Log，日志落盘后才能修改数据页。

### 2. Redo Log 关键概念

| 概念 | 说明 | 工程实现 |
|------|------|---------|
| LSN | 日志序列号，唯一标识每条日志 | 递增整数 |
| REC_LSN | 脏页最近修改的 LSN | dirty_page.rec_lsn |
| 检查点 | 记录脏页状态，加速恢复 | checkpoint_lsn |

### 3. 崩溃恢复流程（ARIES 简化）

**分析阶段**：扫描日志，确定哪些页面是脏的
**重做阶段**：从检查点开始重放所有 Redo Log
**回滚阶段**：回滚未提交事务的修改

### 4. 工程实现要点

```c
// 脏页表条目（简化版）
typedef struct {
    int page_id;        // 页面 ID
    int rec_lsn;        // 最近修改的 LSN
    bool dirty;         // 是否脏
} DirtyPageEntry;

// 检查点信息
typedef struct {
    int checkpoint_lsn;     // 检查点 LSN
    int dirty_page_count;   // 脏页数
    int *dirty_pages;       // 脏页列表
} CheckpointInfo;
```

### 5. WAL 与 Redo Log 的关系

- WAL 是原则，Redo Log 是实现
- 每次修改数据前，先写入 Redo Log
- Redo Log 落盘后，才能修改数据页
- 数据页可以延迟刷盘（依赖 Redo Log 恢复）

### 6. 学习扩展

- ARIES 算法的完整三阶段恢复
- Redo Log 与 Undo Log 的区别
- 检查点频率与恢复时间的权衡

---

## 总结

本学习卡演示了 Redo Log 的核心机制：
1. LSN 唯一标识日志，支持顺序恢复
2. 脏页表记录修改历史，支持增量恢复
3. 检查点加速崩溃恢复
4. WAL 原则保证持久性