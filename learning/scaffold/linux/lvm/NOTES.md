# Linux LVM 逻辑卷管理学习笔记

本文档记录 LVM 逻辑卷管理在项目工程代码中的实际应用对照。

## 工程对照：存储卷管理 → 数据库存储扩容 / 表空间管理对照

### 1. LVM 与表空间管理的分层映射

LVM 的三层架构（PV → VG → LV）与数据库表空间管理存在一一对应关系：

```
LVM 体系                      数据库存储体系
──────────                    ──────────────
PV (物理卷)              ←→  裸盘/分区 (/dev/sdb1)
  /dev/sdb1, /dev/sdc1        磁盘设备文件
                                
VG (卷组)                ←→  表空间 (Tablespace)
  聚合多个 PV 的存储池         聚合多个数据文件的逻辑容器

LV (逻辑卷)              ←→  数据文件 / 表
  /dev/myvg/data_lv            /data/base/16384/12345
  可独立扩容缩容              可独立扩展（autoextend）
```

### 2. 工程代码中的存储引擎分层

```c
// engineering/include/db/mm_storage.h
// 多模态存储引擎 —— 类似 VG 聚合多种存储

// 类似 LVM 的 VG：一个数据库实例聚合多种存储引擎
typedef enum {
    STORAGE_KV,          // KV 存储 — 类似一个 LV
    STORAGE_VECTOR,      // 向量存储 — 类似另一个 LV
    STORAGE_TIMESERIES,  // 时序存储
    STORAGE_DOCUMENT,    // 文档存储
    STORAGE_SPATIAL,     // 空间存储
    STORAGE_GRAPH,       // 图存储
    STORAGE_YANG,        // 层次存储
} storage_type_t;

// 每个存储引擎管理自己的数据文件（类似独立的 LV）
typedef struct storage_engine_t {
    storage_type_t type;
    char     data_path[256];   // 数据文件路径（类似 LV 设备路径）
    uint64_t total_size;       // 总大小
    uint64_t used_size;        // 已用大小
} storage_engine_t;
```

### 3. 表空间扩容对应

工程代码中表空间的动态扩容，对应 LVM 的 lvextend：

```c
// engineering/src/db/storage/page/disk.c
// 页面文件扩展 —— 对应 LVM 的 LV 动态扩容

int disk_extend(disk_t *disk, uint32_t num_pages) {
    uint64_t new_size = (disk->num_pages + num_pages) * PAGE_SIZE;
    
    // 类似 lvextend: 增加存储空间
    if (ftruncate(disk->fd, new_size) < 0) {
        return -1;
    }
    
    disk->num_pages += num_pages;
    disk->total_size = new_size;
    return 0;
}
```

### 4. 快照与数据库备份

LVM 快照为数据库提供一致性备份能力：

```c
// 数据库备份流程（结合 LVM 快照）
// 
// 1. FLUSH TABLES WITH READ LOCK;   -- 锁定表
// 2. SHOW MASTER STATUS;            -- 记录 binlog 位置
// 3. lvcreate -L 1G -s -n snap /dev/myvg/data_lv  -- 创建快照
// 4. UNLOCK TABLES;                 -- 解锁（业务继续运行）
// 5. mount /dev/myvg/snap /mnt/snap
// 6. rsync -av /mnt/snap/ /backup/
// 7. umount /mnt/snap
// 8. lvremove /dev/myvg/snap       -- 删除快照

// 工程中 WAL + 检查点提供类似快照的恢复能力
// wal.c: WAL 日志记录所有变更，可从检查点 + WAL 恢复
```

### 5. Thin Pool 与稀疏文件

工程代码中的稀疏文件分配，类似 LVM thin-pool 的按需分配：

```c
// engineering/src/db/storage/page/disk.c
// 稀疏文件分配 —— 类似 thin provisioning

int disk_init_sparse(disk_t *disk, uint64_t max_pages) {
    // 创建稀疏文件：逻辑大小很大，但物理只占实际使用的空间
    disk->fd = open(disk->path, O_RDWR | O_CREAT, 0644);
    
    // fallocate 配合 PUNCH_HOLE 实现稀疏分配
    // 类似 thin-pool：提交空间 >> 物理空间
    fallocate(disk->fd, 0, 0, max_pages * PAGE_SIZE);
    
    disk->max_pages = max_pages;  // 逻辑最大页面数（超分配）
    disk->num_pages = 0;          // 实际使用页面数
    return 0;
}
```

### 6. 存储管理对比总结

| 特性 | LVM | 数据库存储引擎 |
|------|-----|---------------|
| 存储聚合 | VG 聚合 PV | 表空间聚合数据文件 |
| 动态扩容 | lvextend + resize2fs | ALTER TABLESPACE ... ADD DATAFILE |
| 快照备份 | lvcreate -s (COW) | WAL + 检查点 + PITR |
| 精简分配 | thin-pool | 稀疏文件 (fallocate) |
| 在线操作 | 在线扩容（ext4/xfs） | Online DDL |
| 空间回收 | fstrim / blkdiscard | VACUUM / 碎片整理 |
| 多租户隔离 | 不同 LV 挂载不同目录 | 不同表空间/数据库隔离 |

## 参考源码

- `engineering/include/db/mm_storage.h` — 多模态存储引擎类型定义
- `engineering/src/db/storage/page/disk.c` — 页面文件管理（扩容/稀疏分配）
- `engineering/src/db/storage/wal/wal.c` — WAL 日志（类似快照的恢复能力）
