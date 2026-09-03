# Linux CFS 调度器学习卡片

本学习卡片演示 Linux CFS（完全公平调度器）的原理，包括虚拟运行时间、红黑树管理、调度延迟、以及 CPU 亲和性设置。

## 学习目标

1. 理解 CFS 的核心概念：虚拟运行时间 (vruntime)
2. 理解 CFS 如何选择下一个运行进程
3. 掌握调度延迟和最小时间片的含义
4. 能够设置进程的调度策略和 CPU 亲和性

## 核心概念

### CFS 调度算法

```
红黑树（按 vruntime 排序）
      │
      ▼
  +-------+
  │ vrun  │ <-- 最小的 vruntime，获得 CPU
  +-------+
  │ vrun  │
  +-------+
  │ vrun  │
  +-------+
```

### vruntime 计算

```
vruntime += delta_exec * (1024 / weight)

其中：
- delta_exec: 实际运行时间
- weight: 基于 nice 值的权重
- nice=-20: weight=102476 (高优先级)
- nice=0:   weight=1024   (默认)
- nice=+19: weight=15     (低优先级)
```

### 调度参数

| 参数 | 默认值 | 说明 |
|------|--------|------|
| latency | 6ms/48ms | 调度周期 |
| min_granularity | 0.75ms | 最小时间片 |
| nice_0_latency | 6ms | nice=0 的时间片 |

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o cfs main.c
```

### 运行

```bash
./cfs
# 或使用 make
make check
```

### 清理

```bash
make clean
```

## 输出示例

```
========================================
  Linux CFS 调度器学习演示
========================================

--- Demo 3: CFS 算法原理 ---
[cfs] CFS (完全公平调度器) 原理:
[cfs] 1. 核心概念：虚拟运行时间 (vruntime)
[cfs]    - 每个进程维护一个 vruntime 值
[cfs]    - vruntime 越小，获得 CPU 的优先级越高

--- Demo 4: 设置调度策略 ---
[cfs]   当前调度策略: SCHED_OTHER (CFS)
[cfs]   调度优先级: 0

========================================
=== PASS ===
========================================
```

## 相关资源

- `man sched` — 调度相关系统调用
- `/proc/sched_debug` — 调度器统计
- `/proc/PID/sched` — 进程调度信息
- `chrt` — 实时调度工具
