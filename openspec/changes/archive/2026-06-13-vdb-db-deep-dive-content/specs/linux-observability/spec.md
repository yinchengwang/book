## ADDED Requirements

### Requirement: Linux 可观测性深度文章

Linux 可观测性的每篇深度文章 SHALL 覆盖以下知识点（预计 ~13 篇）：

| 知识点 ID | 标题 | 难度 |
|-----------|------|------|
| `linux-proc-fs` | procfs 文件系统接口 | basic |
| `linux-cpu-diag` | CPU 性能诊断 | basic |
| `linux-mem-diag` | 内存性能诊断 | basic |
| `linux-disk-iostat` | 磁盘 I/O 性能观测 | basic |
| `linux-htop` | 系统监控与进程管理 | basic |
| `linux-sar` | 系统活动报告 sar | basic |
| `linux-perf-basic` | perf 性能分析基础 | intermediate |
| `linux-blktrace` | 块设备 I/O 追踪 | intermediate |
| `linux-pagecache-hit` | Page Cache 命中率分析 | intermediate |
| `linux-net-diag` | 网络性能诊断 | intermediate |
| `linux-ebpf-intro` | eBP 可观测性 | advanced |
| `linux-db-flamegraph` | 数据库火焰图分析 | advanced |
| `linux-numa-observ` | NUMA 观测与亲和性 | advanced |
| `linux-latency-diag` | 延迟诊断与分析 | advanced |

每篇文章 SHALL 遵循 8 段式模版。

#### 特殊要求：可观测性文章 SHALL 额外包含

- 工具命令的具体输出示例（带真实终端输出）
- 关键指标的"正常 vs 异常"阈值参考
- 排查流程的决策树（先看什么、后看什么）
- 数据库场景下的可观测性实践（如慢查询与 CPU/IO 关联分析）

#### Scenario: 可观测性文章完整性

- **WHEN** 用户阅读可观测性系列文章
- **THEN** 每篇文章 SHALL 包含至少一个命令行使用示例及其输出解析

#### Scenario: 性能诊断文章覆盖

- **WHEN** 用户阅读 `linux-cpu-diag` 或 `linux-mem-diag`
- **THEN** 文章 SHALL 给出各指标的"正常值参考范围"和"异常值排查方向"
