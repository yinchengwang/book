# Linux ext4 / xfs 文件系统对比学习笔记

本文档记录 ext4/xfs 文件系统对比在项目工程代码中的实际应用对照。

## 工程对照：文件系统选型 → 数据库部署时 ext4 vs xfs 选择指南

### 1. 数据库部署文件系统选型决策树

```
数据规模 > 50TB?
  ├── 是 → xfs（ext4 大卷性能下降明显）
  └── 否 → 并发连接 > 1000?
            ├── 是 → xfs（AG 并行分配，多核友好）
            └── 否 → 是否需要缩容?
                      ├── 是 → ext4（支持在线缩容）
                      └── 否 → 是否有海量小文件 (< 60B)?
                                ├── 是 → ext4（inline data 优势）
                                └── 否 → xfs（通用推荐）
```

### 2. 工程代码中的文件系统适配

```c
// engineering/src/db/storage/page/disk.c
// 根据文件系统类型调整 IO 行为

typedef enum {
    FS_UNKNOWN = 0,
    FS_EXT4,
    FS_XFS,
    FS_BTRFS,
    FS_ZFS,
} fs_type_t;

// 通过 statfs 探测文件系统类型
static fs_type_t detect_fs_type(const char *path) {
    struct statfs fs_info;
    if (statfs(path, &fs_info) < 0) return FS_UNKNOWN;

    switch (fs_info.f_type) {
        case 0xEF53:      return FS_EXT4;   // EXT4_SUPER_MAGIC
        case 0x58465342:  return FS_XFS;    // XFS_SUPER_MAGIC
        case 0x9123683E:  return FS_BTRFS;  // BTRFS_SUPER_MAGIC
        default:          return FS_UNKNOWN;
    }
}

// 根据文件系统选择最优的 IO 策略
static void configure_fs_io_params(disk_t *disk) {
    fs_type_t fstype = detect_fs_type(disk->path);

    switch (fstype) {
        case FS_XFS:
            // xfs: 大并发，利用 AG 并行性
            disk->io_workers = 8;       // 多线程并发 IO
            disk->alloc_batch = 1024;   // 批量分配,利用 extent
            disk->use_fallocate = 1;    // 使用 fallocate 预分配
            break;
        case FS_EXT4:
            // ext4: 适合中小规模
            disk->io_workers = 4;
            disk->alloc_batch = 256;
            disk->use_fallocate = 1;
            break;
        default:
            disk->io_workers = 2;
            disk->alloc_batch = 64;
            break;
    }
}
```

### 3. extent 与数据库空间分配

ext4/xfs 的 extent 分配与数据库的 extent 概念高度契合：

```c
// engineering/src/db/storage/page/disk.c
// 数据库的 extent 与文件系统的 extent 对齐

#define DB_EXTENT_SIZE 64  // 数据库一次分配 64 个页面 (512KB)

// 数据库 extent: 一次分配连续页面
// 文件系统 extent: 一次分配连续磁盘块
// 对齐后可减少文件碎片，提升顺序扫描性能
int disk_alloc_extent(disk_t *disk, uint32_t *start_page) {
    // fallocate: 在文件系统层面预分配连续空间
    // xfs: 通过 B+tree extent 高效管理
    // ext4: 通过 extent 树管理
    if (fallocate(disk->fd, 0,
                  disk->num_pages * PAGE_SIZE,
                  DB_EXTENT_SIZE * PAGE_SIZE) < 0) {
        return -1;
    }
    *start_page = disk->num_pages;
    disk->num_pages += DB_EXTENT_SIZE;
    return 0;
}
```

### 4. WAL 日志与文件系统日志的协同

```c
// engineering/src/db/storage/wal/wal.c
// 数据库 WAL + 文件系统日志: 双日志架构

// 设计考量:
//
// 1. ext4 data=ordered 模式:
//    - 元数据先日志, 数据直接写入
//    - 与数据库 WAL 互补: WAL 保护数据, ext4 日志保护元数据
//    - 推荐配置: mount -o data=ordered,noatime
//
// 2. xfs 仅元数据日志:
//    - xfs 不保护数据完整性
//    - 完全依赖数据库 WAL 保证数据一致性
//    - 推荐配置: mount -o noatime,nodiratime,logbsize=256k
//
// 3. O_DIRECT 绕过页缓存:
//    - 双日志情况下，页缓存是多余的
//    - O_DIRECT + WAL + fsync 保证持久性
//    - 文件系统日志保证元数据一致性

int wal_sync(wal_t *wal) {
    // 数据库 WAL 负责数据一致性
    if (write(wal->fd, wal->buf, wal->offset) < 0) return -1;

    // fsync 触发文件系统日志保证元数据一致性
    if (fsync(wal->fd) < 0) return -1;

    wal->offset = 0;
    return 0;
}
```

### 5. ext4 vs xfs 数据库部署建议

| 场景 | 推荐 | 原因 |
|------|------|------|
| 小型数据库 (< 100GB) | ext4 | 简单可靠, inline data 对小数据文件友好 |
| 中型数据库 (100GB-10TB) | xfs | AG 并行, 大 extent, 元数据 B+tree |
| 大型数据库 (> 10TB) | xfs | 唯一选择, ext4 在大卷下性能下降 |
| 高并发 OLTP | xfs | AG 并行分配, 多核无竞争 |
| OLAP / 数据仓库 | xfs | 大 extent 顺序扫描友好 |
| 频繁扩缩容 | ext4 | 支持在线缩容 (xfs 不支持) |
| WAL 独立设备 | xfs | 外置日志可分离到独立 SSD |
| 容器化部署 | ext4 | 兼容性最好, 驱动支持广泛 |

### 6. 挂载选项对照

```bash
# ext4 数据库推荐挂载选项
mount -o defaults,noatime,nodiratime,data=ordered,barrier=1 /dev/sdb1 /data

# xfs 数据库推荐挂载选项
mount -o defaults,noatime,nodiratime,nobarrier,logbsize=256k,logdev=/dev/nvme0n2, \
      allocsize=64k,inode64 /dev/sdb1 /data

# noatime:     不记录文件访问时间 (减少元数据写入)
# nodiratime:  不记录目录访问时间
# data=ordered:ext4 默认日志模式 (数据+元数据安全)
# nobarrier:   xfs 关闭写屏障 (有 UPS/RAID 电池时)
# logbsize:    xfs 日志块大小 (256KB 适合数据库 WAL)
# logdev:      xfs 外置日志设备 (推荐独立 SSD)
# allocsize:   分配大小 (与数据库页面大小对齐)
# inode64:     允许 64 位 inode (大文件系统)
```

## 参考源码

- `engineering/src/db/storage/page/disk.c` — 页面文件管理与文件系统适配
- `engineering/src/db/storage/wal/wal.c` — WAL 日志与双日志协同
- `engineering/include/db/guc.h` — 文件系统相关 GUC 参数
