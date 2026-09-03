# Linux SAR (系统活动报告) 学习卡片

本学习卡片演示 SAR（System Activity Reporter）的工作原理，包括 sysinfo 系统调用、/proc/stat CPU 采样、内存统计、以及 SAR 风格输出格式化。

## 学习目标

1. 理解 sysinfo() 系统调用的用途
2. 掌握 /proc/stat 的 CPU 统计格式
3. 理解 CPU 使用率的计算方法
4. 实现简单的性能数据采样

## 核心概念

### sysinfo 系统调用

```c
#include <sys/sysinfo.h>

struct sysinfo {
    long uptime;      /* 系统运行时间（秒） */
    unsigned long loads[3];  /* 1/5/15 分钟负载 */
    unsigned long totalram;  /* 总内存 */
    unsigned long freeram;   /* 空闲内存 */
    unsigned long sharedram; /* 共享内存 */
    unsigned long bufferram; /* 缓冲区内存 */
    unsigned long totalhigh; /* 高端内存总量 */
    unsigned long freehigh;  /* 高端内存空闲 */
    unsigned long mem_unit;  /* 内存单位大小 */
    int procs;        /* 进程数量 */
};
```

### /proc/stat CPU 行格式

```
cpu  user nice system idle iowait irq softirq steal guest guest_nice
```

各字段含义：
- `user`: 用户态时间
- `nice`: 低优先级用户态时间
- `system`: 内核态时间
- `idle`: 空闲时间
- `iowait`: 等待 I/O 时间
- `irq`: 硬中断时间
- `softirq`: 软中断时间

### CPU 使用率计算

```c
// 两次采样间隔的 CPU 使用率
user_delta = user2 - user1;
total_delta = (user2 + nice2 + system2 + idle2 + iowait2 + irq2 + softirq2)
            - (user1 + nice1 + system1 + idle1 + iowait1 + irq1 + softirq1);

cpu_usage = 100.0 * (1.0 - idle_delta / total_delta);
```

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o sar main.c
```

### 运行

```bash
./sar
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
  Linux SAR (系统活动报告) 学习演示
========================================

--- Demo 1: sysinfo() 系统调用 ---
[sar]   系统运行时间: 86400 秒 (1 天)
[sar]   1/5/15 分钟负载: 0.50, 0.45, 0.40
[sar]   内存总量: 32768 MB

--- Demo 2: CPU 统计采样 ---
[sar]     user:    12.5%
[sar]     system:   3.2%
[sar]     idle:    84.3%

========================================
=== PASS ===
========================================
```

## 相关资源

- `man sysinfo` — sysinfo 系统调用手册
- `man sar` — SAR 工具手册
- `/proc/stat` — 内核文档
