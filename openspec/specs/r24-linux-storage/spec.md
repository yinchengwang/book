# R24 Linux 存储 — 规格沉淀

## 概述

R24 Linux 存储能力覆盖 13 张卡，涵盖文件系统日志、FIO 基准测试、Btrfs、FUSE、RAID、缓存管理、空间预分配、数据同步、块设备层、LVM、SSD 优化、ext4/xfs 对比等核心主题。

## 能力规格

### 1. fs_journal（文件系统日志）

| 属性 | 值 |
|------|-----|
| 主题 | ext4/xfs 日志机制 |
| 核心概念 | journal_commit、journal_revoke、日志恢复流程 |
| 工程对照 | 数据库 WAL（Write-Ahead Logging）机制 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 2. fio（FIO 基准测试）

| 属性 | 值 |
|------|-----|
| 主题 | 存储性能基准测试 |
| 核心概念 | fio 配置、读写测试、延迟分布、IOPS/QPS |
| 工程对照 | 数据库性能压测（pgbench、sysbench） |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 3. btrfs（Btrfs 文件系统）

| 属性 | 值 |
|------|-----|
| 主题 | Btrfs 高级特性 |
| 核心概念 | COW（写时复制）、快照、压缩、透明加密、RAID |
| 工程对照 | 数据库存储引擎的快照隔离级别 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 4. nfs（NFS 网络文件系统）

| 属性 | 值 |
|------|-----|
| 主题 | NFS 网络文件系统 |
| 核心概念 | 挂载选项、远程文件操作、缓存一致性、lock 管理 |
| 工程对照 | 分布式数据库的远程存储访问 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 5. fuse（FUSE 用户态文件系统）

| 属性 | 值 |
|------|-----|
| 主题 | FUSE 框架 |
| 核心概念 | 用户态文件系统实现、fuse_operations、/dev/fuse |
| 工程对照 | 数据库的虚拟文件系统抽象层 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 6. raid（RAID 阵列）

| 属性 | 值 |
|------|-----|
| 主题 | RAID 存储冗余 |
| 核心概念 | RAID 0/1/5/10、条带化、冗余、磁盘重建 |
| 工程对照 | 数据库的复制与高可用架构 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 7. fs_cache（文件系统缓存）

| 属性 | 值 |
|------|-----|
| 主题 | 文件系统缓存管理 |
| 核心概念 | drop_caches、缓存压力阈值、page cache、回收策略 |
| 工程对照 | 数据库 Buffer Pool 缓存管理 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 8. fallocate（空间预分配）

| 属性 | 值 |
|------|-----|
| 主题 | 文件空间预分配 |
| 核心概念 | fallocate()、稀疏文件、零拷贝初始化、预留空间 |
| 工程对照 | 数据库文件预分配策略 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 9. fdatasync（数据同步）

| 属性 | 值 |
|------|-----|
| 主题 | 数据同步与刷盘 |
| 核心概念 | fsync/fdatasync、O_SYNC、同步边界、批量刷盘 |
| 工程对照 | 数据库 WAL 同步策略（full/batch/async） |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 10. block_layer（块设备层）

| 属性 | 值 |
|------|-----|
| 主题 | Linux 块设备抽象 |
| 核心概念 | 多队列块设备（blk-mq）、请求合并、调度算法 |
| 工程对照 | 数据库存储引擎的 IO 调度优化 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 11. lvm（LVM 逻辑卷）

| 属性 | 值 |
|------|-----|
| 主题 | LVM 逻辑卷管理 |
| 核心概念 | PV/VG/LV、快照卷、thin-pool、动态扩容 |
| 工程对照 | 数据库存储卷的动态扩容能力 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 12. disk_ssd（SSD 优化）

| 属性 | 值 |
|------|-----|
| 主题 | SSD 存储优化 |
| 核心概念 | TRIM、写入均衡、队列深度、GC、wear leveling |
| 工程对照 | 数据库针对 SSD 的 IO 优化策略 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

### 13. ext4_xfs（ext4/xfs 对比）

| 属性 | 值 |
|------|-----|
| 主题 | 文件系统选型对比 |
| 核心概念 | ext4 vs xfs、日志模式、延迟分配、extent、配额 |
| 工程对照 | 数据库存储引擎的文件系统选型 |
| 文件 | main.c + Makefile + README.md + NOTES.md |

## 技术栈关联

- **前置知识**：R21 Linux 基础（proc_fs, syscall, pagecache）
- **延伸知识**：R23 Linux 性能（iostat, perf）
- **数据库关联**：R13 DB 索引系统、R14 DB 事务（存储层）

## 验收标准

- [x] 13 张卡 scaffold 完整（main.c + Makefile + README.md + NOTES.md）
- [x] 每张卡 NOTES.md 工程对照 ≥100 字
- [x] 13 张卡在 Linux 环境编译/运行通过
- [x] statuses.json 更新为 done
