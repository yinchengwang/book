# Linux htop 原理学习卡片

本学习卡片演示 htop 的工作原理，包括遍历 /proc 进程信息、解析进程 stat 文件、构建进程树、以及 CPU/内存排序。

## 学习目标

1. 理解 /proc 文件系统在进程监控中的作用
2. 掌握 /proc/PID/stat 文件的解析方法
3. 理解进程树（父子关系）的构建原理
4. 实现简单的 CPU/内存使用率排序

## 核心概念

### /proc/PID/stat 格式

```
pid (comm) state ppid pgrp session tty_nr tpgid flags
minflt cminflt majflt cmajflt utime stime cutime cstime
priority nice num_threads itrealvalue starttime vsize rss ...
```

关键字段：
- `comm`: 进程名（可能被转义）
- `state`: 进程状态 (R/S/D/Z/T)
- `ppid`: 父进程 ID
- `utime/stime`: CPU 时间（时钟滴答）
- `rss`: 物理内存（页数）

### 进程状态说明

| 状态 | 含义 | 说明 |
|------|------|------|
| R | Running | 运行中或就绪 |
| S | Sleeping | 可中断睡眠 |
| D | Disk Sleep | 不可中断睡眠（等待 I/O） |
| Z | Zombie | 僵尸进程 |
| T | Stopped | 已停止 |

### htop vs top

| 特性 | htop | top |
|------|------|-----|
| 彩色显示 | 支持 | 不支持 |
| 进程树 | 支持 | 不支持 |
| 滚动 | 支持 | 不支持 |
| 依赖 | ncurses | 无 |

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o htop main.c
```

### 运行

```bash
./htop
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
  Linux htop 原理学习演示
========================================

--- Demo 1: 遍历进程列表 ---
[htop]   PID     PPID    STATE  NAME
[htop]   1       0       S       systemd
[htop]   2       0       S       kthreadd
...

--- Demo 3: CPU 使用率排序 ---
[htop]   PID     CPU(ms)  NAME
[htop]   1234    50000    chrome
[htop]   5678    30000    firefox
...

========================================
=== PASS ===
========================================
```

## 相关资源

- `man proc` — /proc 文件系统说明
- `man top` — top 手册
- `htop` 项目源码 — 了解完整实现
