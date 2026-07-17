# R23 Linux 进阶性能 — 能力规格

## 概述

R23 Linux 进阶性能能力涵盖 Linux 性能分析的高级工具和方法，包括：
- CPU/内存/网络性能分析工具
- 内核追踪和 eBPF 技术
- 性能可视化（火焰图）
- NUMA/延迟/异步 I/O 等高级主题

## 能力卡片

### 1. perf_basic — perf 性能分析基础

**能力描述**：掌握 Linux perf 工具的基本使用方法

**核心技能**：
- `perf stat`：统计硬件/软件事件
- `perf record`：采样生成 perf.data
- `perf report`：分析采样数据
- 热点函数定位

**工程对照**：
- 数据库查询执行热点分析
- 缓存命中率与 perf 事件关联

### 2. blktrace — 块设备追踪

**能力描述**：使用 blktrace 追踪块设备 I/O 请求路径

**核心技能**：
- blktrace 架构（Q/G/I/M/D/C 阶段）
- I/O 延迟分解（队列+寻道+旋转+传输）
- blkparse 分析工具

**工程对照**：
- 数据库 Buffer Pool 与磁盘交互追踪
- WAL 写入延迟分析

### 3. pagecache_hit — 页缓存命中率

**能力描述**：理解 Linux 页缓存机制，分析缓存命中率

**核心技能**：
- /proc/vmstat 关键指标（pgfault/pgmajfault）
- 命中率计算方法
- 顺序访问 vs 随机访问的缓存行为

**工程对照**：
- 数据库 Buffer Pool 命中率监控
- 热点页面保持策略

### 4. net_diag — 网络诊断接口

**能力描述**：使用 /proc/net/* 接口诊断网络性能

**核心技能**：
- /proc/net/snmp 统计解析
- /proc/net/tcp 连接状态分析
- TCP 重传率、连接状态监控

**工程对照**：
- 数据库连接池诊断
- 网络 I/O 性能监控

### 5. ebpf_intro — eBPF 入门

**能力描述**：了解 Linux eBPF 技术基础和应用场景

**核心技能**：
- eBPF 架构（程序/Maps/验证器/JIT）
- BCC 工具集（biolatency/cachestat）
- bpftrace 脚本基础

**工程对照**：
- 数据库 I/O 路径追踪
- Buffer Pool 命中率实时监控

### 6. db_flamegraph — 数据库火焰图

**能力描述**：使用 perf + FlameGraph 生成 CPU 火焰图

**核心技能**：
- perf 采样生成折叠栈
- FlameGraph 工具使用
- 火焰图阅读方法（x=占比，y=栈深）

**工程对照**：
- 查询执行路径热点分析
- 优化前后差分火焰图对比

### 7. numa_observ — NUMA 观测

**能力描述**：理解 NUMA 架构，进行内存分布优化

**核心技能**：
- NUMA 拓扑检测（numactl --hardware）
- 内存分配策略（membind/interleave）
- numastat 监控

**工程对照**：
- Buffer Pool 的 NUMA 感知分配
- 连接线程的 NUMA 亲和性

### 8. latency_diag — 延迟诊断

**能力描述**：使用延迟分解方法定位性能瓶颈

**核心技能**：
- USE/RED 方法论
- 延迟分阶段测量
- P50/P95/P99 百分位分析

**工程对照**：
- 数据库查询延迟分解
- 慢查询根因定位

### 9. iouring — io_uring 异步 I/O

**能力描述**：理解 Linux io_uring 异步 I/O 接口

**核心技能**：
- io_uring 架构（SQE/CQ/SQ/CQ）
- 批量提交和完成队列
- 与 libaio/传统 I/O 对比

**工程对照**：
- WAL 批量刷盘优化
- Buffer Pool 页面预读

### 10. rcu — RCU 无锁同步

**能力描述**：理解 Linux RCU 锁机制和应用

**核心技能**：
- RCU 三步走（读/COPY/更新）
- 宽限期（Grace Period）概念
- 读侧无锁、零开销

**工程对照**：
- Catalog 缓存的 RCU 保护
- 查询计划缓存的无锁读取

### 11. cgroup2 — cgroup v2 资源控制

**能力描述**：理解 Linux cgroup v2 资源隔离机制

**核心技能**：
- cgroup v2 单层级模型
- 内存/CPU/IO 控制器
- 容器资源限制配置

**工程对照**：
- 容器化数据库的资源限制
- shared_buffers 与 cgroup 对齐

### 12. mmap_db — mmap 内存映射数据库

**能力描述**：理解 mmap 在存储引擎中的应用

**核心技能**：
- mmap 映射机制
- 页面错误和预读
- mmap vs 传统 I/O 对比

**工程对照**：
- SQLite 的 mmap 模式
- WiredTiger 的 mmap 优化

### 13. io_uring_storage — io_uring 高速存储

**能力描述**：使用 io_uring 实现高性能存储 I/O

**核心技能**：
- io_uring 批量提交
- SQ 轮询和 IOPOLL 模式
- 存储引擎 I/O 优化

**工程对照**：
- 检查点并行刷盘
- 大文件顺序 I/O 加速

## 能力规格

| 规格项 | 要求 |
|--------|------|
| scaffold 完整性 | 13 张卡 × 4 文件（main.c/Makefile/README.md/NOTES.md）|
| 编译通过 | gcc -std=c11 编译无错误 |
| NOTES.md 长度 | 每张卡 ≥100 字工程对照 |
| 双轨隔离 | 无 engineering 源码变更 |

## 扩展能力

- perf 进阶：perf sched、perf lock
- eBPF 进阶：BCC Python API、自定义探针
- 火焰图进阶：冷火焰图、内存火焰图
- cgroup 进阶：PSI 压力感知调度
