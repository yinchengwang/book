# proc_fs — /proc 文件系统

## 简介

演示 Linux `/proc` 虚拟文件系统的读取方法，包括 `/proc/meminfo`、`/proc/cpuinfo`、`/proc/self/status`。

## 编译

```bash
gcc -std=c11 -Wall -o proc_fs main.c
```

## 运行

```bash
./proc_fs
```

## 预期输出

```
===========================================
  procfs 文件系统接口演示
===========================================

[1] 直接读取 /proc/meminfo
...

[3] 解析 /proc/meminfo 关键指标
总内存:    32000 MB
空闲内存:  8000 MB
...

[4] 解析 /proc/cpuinfo
型号:      Intel(R) Core(TM) i7-10700K
核心数:    16
```

## 核心文件

- `/proc/meminfo` — 内存统计
- `/proc/cpuinfo` — CPU 信息
- `/proc/self/status` — 当前进程状态

## 扩展

- 尝试读取 `/proc/loadavg` 获取负载均值
- 读取 `/proc/uptime` 获取系统运行时间
