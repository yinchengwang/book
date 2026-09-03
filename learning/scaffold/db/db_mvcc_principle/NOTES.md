# MVCC 工程对照笔记

## 概述

本文档对照 `learning/scaffold/db/db_mvcc_principle` 与 `engineering/src/db/concurrency/` 中的工程实现，梳理学习卡中的简化概念如何映射到真实代码。

## 版本链实现

### 学习卡简化版

学习卡中 `tuple_ver_t` 用链表模拟版本链：

```c
typedef struct {
    int xmin;
    int xmax;
    struct tuple_ver *chain;  // 内存链表
} tuple_ver_t;
```

### 工程实现（mvcc_version.c）

工程中采用更完整的结构 `mvcc_version_t`，字段命名直接对应 PostgreSQL：

```c
typedef struct mvcc_version {
    mvcc_txn_id_t xmin;           // 创建事务 ID
    mvcc_txn_id_t xmax;           // 删除事务 ID (0 = 未删除)
    mvcc_ctid_t ctid;             // 版本链指针 (block, offset)
    mvcc_ctid_t xvac;             // Undo 记录指针（用于 GC）
    void *data;
    size_t data_size;
    struct mvcc_version *next;    // 内存链表指针
} mvcc_version_t;
```

关键差异：
- **ctid**：磁盘存储时用 (block_num, offset) 指向页面内槽位
- **xvac**：指向 Undo 日志，用于回滚和垃圾判定
- **数据类型**：工程用 `int64_t`（`mvcc_txn_id_t`），学习卡简化为 `int`

工程核心操作：
- `mvcc_version_new()`：创建新版本，分配 data 内存
- `mvcc_version_insert()`：插入链表头部（O(1)）
- `mvcc_version_mark_deleted()`：设置 xmax
- `mvcc_undo_create()`：记录 UPDATE 前的旧值到 Undo 日志

## 快照管理

### 学习卡简化版

学习卡快照结构：

```c
typedef struct {
    int xmin;
    int xmax;
    int *xip_list;
    int xip_count;
    int self_txn_id;
} mvcc_snapshot_t;
```

### 工程实现（mvcc_snapshot.c）

工程快照结构（`mvcc_snapshot_t`）完全一致，但增加了序列化和容量管理：

```c
typedef struct mvcc_snapshot {
    mvcc_txn_id_t xmin;
    mvcc_txn_id_t xmax;
    mvcc_txn_id_t *xip_list;
    int xip_count;
    int xip_capacity;              // 动态数组容量
} mvcc_snapshot_t;
```

工程额外功能：
- `mvcc_snapshot_copy()`：快照复制（事务子查询需要）
- `mvcc_snapshot_serialize()`/`deserialize()`：快照网络传输和持久化
- 动态扩容：`xip_capacity` 避免频繁 realloc

## 可见性判断

### 学习卡简化版

```c
static bool version_visible(tuple_ver_t *ver, mvcc_snapshot_t *snap) {
    if (!txn_visible(ver->xmin, snap)) return false;
    if (ver->xmax > 0 && !txn_visible(ver->xmax, snap)) return false;
    return ver->xmax == 0;  // 删除事务已提交则不可见
}
```

### 工程实现（mvcc_visibility.c）

工程 `mvcc_version_visible()` 逻辑一致，条件更严格：

```c
bool mvcc_version_visible(const mvcc_snapshot_t *snapshot,
                          mvcc_txn_id_t xmin,
                          mvcc_txn_id_t xmax,
                          mvcc_txn_id_t cur_txn_id)
{
    // 自身事务创建的版本始终可见
    if (xmin == cur_txn_id) return true;

    // xmin 事务必须在快照创建前已提交
    if (xmin >= snapshot->xmax) return false;
    if (mvcc_snapshot_contains_txn(snapshot, xmin)) return false;

    // 删除事务判定
    if (xmax != MVCC_INVALID_TXN_ID) {
        if (mvcc_snapshot_contains_txn(snapshot, xmax)) return false;
        if (xmax < snapshot->xmax) return false;  // 删除已提交 -> 不可见
    }
    return true;
}
```

关键区别：
- 工程使用 `xmax < snapshot->xmax` 判断删除事务已提交（而非 `!txn_visible`）
- 工程额外提供 `mvcc_check_write_conflict()` 检测写-写冲突

## Undo 日志机制

工程中特有的 Undo 日志系统在学习卡中未演示：

```c
typedef struct mvcc_undo_record {
    mvcc_txn_id_t txn_id;
    mvcc_undo_type_t type;        // INSERT/UPDATE/DELETE
    void *old_data;               // UPDATE/DELETE 时的旧值
    mvcc_ctid_t row_ptr;          // 指向原行
    mvcc_ctid_t prev_undo;        // 前一个 Undo 记录
} mvcc_undo_record_t;
```

Undo 的作用：
1. **事务回滚**：UPDATE 时保存旧值，ROLLBACK 时覆盖新版本
2. **垃圾回收判定**：`xvac` 指针指向的 Undo 全部回收后，版本才可清理
3. **索引回溯**：索引需要沿版本链找到正确版本

## 垃圾回收（GC）

学习卡演示了简单计数，工程实现了完整的 VACUUM：

```c
// mvcc_gc_config_t：可配置参数
int vacuum_threshold;           // 死亡元组数量阈值
double vacuum_scale_factor;     // 相对于表大小的比例
int autovacuum_naptime;         // 自动 VACUUM 间隔
```

GC 触发条件（三个必须同时满足）：
1. `t_xmax > 0`：版本已标记删除
2. `t_xmin < oldest_active_xmin`：创建事务已结束
3. `t_xmax < oldest_active_xmin`：删除事务已结束

## 文件对照表

| 学习卡概念 | 工程实现文件 | 关键函数 |
|-----------|-------------|---------|
| 版本创建 | mvcc_version.c | `mvcc_version_new()` |
| 版本删除 | mvcc_version.c | `mvcc_version_mark_deleted()` |
| 快照创建 | mvcc_snapshot.c | `mvcc_snapshot_create()` |
| 快照查询 | mvcc_snapshot.c | `mvcc_snapshot_contains_txn()` |
| 可见性判断 | mvcc_visibility.c | `mvcc_version_visible()` |
| 遍历查找 | mvcc_visibility.c | `mvcc_version_find_visible()` |
| 冲突检测 | mvcc_visibility.c | `mvcc_check_write_conflict()` |
| Undo 创建 | mvcc_version.c | `mvcc_undo_create()` |
| GC 清理 | （gc 模块） | `mvcc_gc_vacuum()` |

## 延伸学习

1. **Read Committed vs Snapshot Isolation**：默认 RC 下每次语句重新计算快照，SI 下事务全程使用同一快照
2. **索引与 MVCC**：索引不存储 xmin/xmax，需要沿 ctid 回表验证可见性
3. **VACUUM 与 HOT**：Heap-Only Tuple 优化减少索引更新
4. **两阶段提交**：分布式 MVCC 需要额外处理全局事务 ID

## 参考文件路径

```
engineering/
├── include/db/concurrency/mvcc.h          # 公共接口定义
└── src/db/concurrency/
    ├── mvcc_version.c                     # 版本链 + Undo
    ├── mvcc_snapshot.c                    # 快照管理
    └── mvcc_visibility.c                  # 可见性判断
```
