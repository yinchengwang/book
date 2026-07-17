## ADDED Requirements

### Requirement: Linux 内核子系统深度文章

Linux 内核子系统的每篇深度文章 SHALL 覆盖以下知识点（预计 ~14 篇）：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `linux-syscall` | 系统调用机制 | basic |
| `linux-pagecache` | Page Cache 内核实现 | basic |
| `linux-cfs` | CFS 完全公平调度器 | basic |
| `linux-vm` | 虚拟内存管理 | basic |
| `linux-direct-io` | 直接 IO 原理 | intermediate |
| `linux-hugepages` | 大页机制 HugePages | intermediate |
| `linux-fsync` | fsync 与数据持久化 | intermediate |
| `linux-io-scheduler` | IO 调度器 | intermediate |
| `linux-strace` | 系统调用追踪 strace | intermediate |
| `linux-seccomp` | seccomp 安全计算 | intermediate |
| `linux-iouring` | io_uring 异步 IO | advanced |
| `linux-mmap-db` | mmap 与数据库应用 | advanced |
| `linux-cgroup2` | cgroup v2 资源隔离 | advanced |
| `linux-rcu` | RCU 机制 | advanced |

每篇文章 SHALL 遵循 8 段式模版。

#### 特殊要求：内核子系统文章 SHALL 额外包含

- 内核关键数据结构（源码级，如 `struct task_struct`、`struct mm_struct` 等）
- 系统调用的完整路径（用户态 → vDSO → 内核入口 → 具体实现）
- 与数据库/存储系统的关联（如 io_uring 在 RocksDB 中的应用）
- 关键内核参数（`/proc/sys/` 或 `sysctl` 可调参数）及其影响

#### Scenario: 内核文章与数据库关联

- **WHEN** 用户阅读内核子系统文章
- **THEN** 每篇文章 SHALL 至少提供一个"该内核机制如何影响数据库/存储系统性能"的实际案例
