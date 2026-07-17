## ADDED Requirements

### Requirement: Linux 存储与文件系统深度文章

Linux 存储与文件系统的每篇深度文章 SHALL 覆盖以下知识点（预计 ~16 篇）：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `linux-ext4-xfs` | ext4/xfs 文件系统 | basic |
| `linux-disk-ssd` | HDD/SSD 特性与性能模型 | basic |
| `linux-block-layer` | 块设备层 | basic |
| `linux-lvm` | LVM 逻辑卷管理 | basic |
| `linux-fallocate` | 文件预分配 fallocate | intermediate |
| `linux-fdatasync` | fdatasync 与元数据优化 | intermediate |
| `linux-fs-journal` | 文件系统日志 journaling | intermediate |
| `linux-fio` | fio 存储基准测试 | intermediate |
| `linux-btrfs` | Btrfs 快照与压缩 | advanced |
| `linux-nfs` | NFS 分布式文件系统 | advanced |
| `linux-fuse` | FUSE 用户态文件系统 | advanced |
| `linux-raid` | RAID 与存储可靠性 | advanced |
| `linux-fs-cache` | 文件系统缓存层 | advanced |
| `linux-io-uring-storage` | io_uring 存储应用 | advanced |

每篇文章 SHALL 遵循 8 段式模版。

#### 特殊要求：存储文件系统文章 SHALL 额外包含

- 文件系统/存储层的 IO 路径完整链路（应用 → VFS → 具体文件系统 → 块层 → 驱动 → 磁盘）
- 核心性能指标及基准测试方法（fio 脚本示例）
- 数据库负载对文件系统的特殊需求（顺序写/WAL/数据文件扩展）
- 不同文件系统的选型对比表

#### Scenario: 存储文章 IO 路径覆盖

- **WHEN** 用户阅读块设备层或文件系统文章
- **THEN** 文章 SHALL 描述一次 IO 从 VFS 到磁盘的完整路径，标出各层的缓存/合并/调度行为
