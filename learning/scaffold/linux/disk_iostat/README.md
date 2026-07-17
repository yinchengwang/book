# Linux 磁盘 I/O 诊断学习卡片

本学习卡片演示 Linux 磁盘 I/O 诊断的基本方法，包括读取 /proc/diskstats、计算 iostat 指标、以及模拟 I/O 操作。

## 学习目标

1. 理解 /proc/diskstats 的数据结构
2. 掌握 iostat 关键指标的计算方法
3. 能够进行实时 I/O 监控
4. 理解 IOPS、吞吐量、延迟的含义

## 核心概念

### /proc/diskstats 字段说明

```
字段 1-2:  major minor     设备号
字段 3:    name            设备名
字段 4:    reads_completed 读取完成次数
字段 5:    reads_merged    合并的读取数
字段 6:    sectors_read    读取的扇区数
字段 7:    time_reading    读取时间(毫秒)
字段 8:    writes_completed写入完成次数
字段 9:    writes_merged   合并的写入数
字段 10:   sectors_written 写入的扇区数
字段 11:   time_writing    写入时间(毫秒)
字段 12:   io_in_progress  进行中的 I/O 数
字段 13:   time_io         I/O 服务总时间
字段 14:   weighted_time   加权 I/O 时间
```

### iostat 关键指标

| 指标 | 公式 | 单位 | 含义 |
|------|------|------|------|
| tps | reads + writes | 次/秒 | 每秒 I/O 请求数 |
| kB_read/s | sectors * 512 / 1024 / dt | kB/s | 读取吞吐量 |
| kB_writtn/s | sectors * 512 / 1024 / dt | kB/s | 写入吞吐量 |
| r_await | time_reading / reads | ms | 平均读取延迟 |
| w_await | time_writing / writes | ms | 平均写入延迟 |

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o disk_iostat main.c
```

### 运行

```bash
./disk_iostat
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
  Linux 磁盘 I/O 诊断学习演示
========================================

--- Demo 1: 读取 /proc/diskstats ---
[disk_iostat]   可用磁盘设备:
[disk_iostat]     sda (major=8, minor=0, reads=12345, writes=6789)

--- Demo 2: 实时 I/O 监控 ---
[disk_iostat]   监控设备: sda
[disk_iostat]   模拟顺序写入...
[disk_iostat] I/O 统计 (间隔 0.20 秒)
[disk_iostat]   ----- 读取 -----
[disk_iostat]     IOPS: 0.00 reads/s
[disk_iostat]     吞吐量: 0.00 MB/s
...

========================================
=== PASS ===
========================================
```

## 相关资源

- `man iostat` — I/O 统计工具手册
- `/proc/diskstats` — 内核文档
- `iostat -x 1` — 实时查看扩展统计
