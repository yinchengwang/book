# Linux 内存诊断学习卡片

本学习卡片演示 Linux 内存诊断的基本方法，包括读取系统内存信息、内存分配与释放、以及内存泄漏检测思路。

## 学习目标

1. 理解 /proc/meminfo 的数据结构
2. 掌握 malloc/free/calloc/realloc 的正确用法
3. 理解内存泄漏的概念和检测方法
4. 能够读取进程内存映射 (/proc/PID/maps)
5. 理解页缓存与实际内存占用的关系

## 核心概念

### /proc/meminfo 关键字段

```
MemTotal:      系统总物理内存
MemFree:       空闲内存（完全未使用）
MemAvailable:  可用内存（考虑页缓存）
Buffers:       块设备缓存
Cached:        文件页缓存
SwapTotal:     交换空间总量
SwapFree:      交换空间空闲量
```

### 内存分配函数对比

| 函数 | 功能 | 初始化 | 适用场景 |
|------|------|--------|----------|
| malloc() | 分配原始内存 | 未初始化 | 已知大小，需自行初始化 |
| calloc() | 分配并清零 | 全零 | 需要初始化为零的场景 |
| realloc() | 调整大小 | 保留原内容 | 动态扩展/收缩 |

### 内存泄漏检测思路

1. **引用计数法**：跟踪所有 malloc/free 调用
2. **工具法**：使用 valgrind、AddressSanitizer
3. **代码审查**：确保配对出现 malloc/free、new/delete

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o mem_diag main.c
```

### 运行

```bash
./mem_diag
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
  Linux 内存诊断学习演示
========================================

--- Demo 1: 读取 /proc/meminfo ---
[mem_diag] 读取 /proc/meminfo 获取系统内存信息
[mem_diag]   MemTotal:    32768 MB (32.0 GB)
[mem_diag]   MemFree:     8192 MB (8.0 GB)
[mem_diag]   MemAvailable: 24576 MB (24.0 GB)
[mem_diag]   Buffers:     256 MB
[mem_diag]   Cached:      16384 MB
[mem_diag]   内存使用率: 75.0%

--- Demo 2: 动态内存分配与释放 ---
[mem_diag] 调用 malloc(1024) 分配 1KB
...

========================================
=== PASS ===
========================================
```

## 相关资源

- `man proc` — /proc 文件系统说明
- `man malloc` — 内存分配函数手册
- `valgrind --leak-check=full ./program` — 内存泄漏检测
