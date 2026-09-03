# cpu_diag — CPU 诊断

## 简介

演示 Linux CPU 性能监控：核心数、利用率采样、上下文切换统计、进程 CPU 占用。

## 编译

```bash
gcc -std=c11 -Wall -o cpu_diag main.c
```

## 运行

```bash
./cpu_diag
```

## 预期输出

```
=== CPU 核心数 ===
配置核心数: 16
在线核心数: 16

=== CPU 利用率 ===
总体 CPU 使用率: 5.2%

=== 上下文切换统计 ===
上下文切换次数: 123456789
```

## 核心概念

- `sysconf(_SC_NPROCESSORS_CONF)` — 配置的核心数
- `/proc/stat` — CPU 时间统计（user/nice/system/idle/iowait/irq/softirq）
- `/proc/[pid]/stat` — 进程 CPU 时间（utime/stime）
- 上下文切换：`ctxt` (context switches), `intr` (interrupts)

## 扩展

- 尝试用 `sched_setaffinity` 设置 CPU 亲和性
- 读取 `/proc/loadavg` 获取负载均值
