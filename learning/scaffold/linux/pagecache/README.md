# Linux 页缓存学习卡片

本学习卡片演示 Linux 页缓存的工作原理，包括 posix_fadvise 提示、readahead 预读、以及直接 I/O 与缓存 I/O 的对比。

## 学习目标

1. 理解页缓存（Page Cache）的工作机制
2. 掌握 posix_fadvise 用法
3. 理解 readahead 预读原理
4. 理解直接 I/O 与缓存 I/O 的适用场景

## 核心概念

### 页缓存结构

```
用户进程                      内核                    磁盘
   |                           |                      |
   | read(fd, buf, 4096)       |                      |
   +-------------------------->|                      |
   |                           |                      |
   |                           | 检查页缓存           |
   |                           | 命中？               |
   |                           |                      |
   |    命中: 直接返回         |                      |
   |<--------------------------+                      |
   |                           |                      |
   |    未命中: 读磁盘+缓存    |                      |
   |                           | read() + 缓存        |
   |                           +--------------------->|
   |                           |<---------------------+
   |                           |                      |
   |<--------------------------+                      |
```

### posix_fadvise 选项

| 选项 | 说明 | 适用场景 |
|------|------|----------|
| POSIX_FADV_NORMAL | 无提示（默认） | 一般使用 |
| POSIX_FADV_SEQUENTIAL | 顺序访问 | 大文件顺序读 |
| POSIX_FADV_RANDOM | 随机访问 | 禁用预读 |
| POSIX_FADV_WILLNEED | 预加载到缓存 | 预读指定区域 |
| POSIX_FADV_DONTNEED | 从缓存移除 | 释放内存 |

### readahead

```c
#include <fcntl.h>

ssize_t readahead(int fd, off64_t offset, size_t count);
```

- 手动触发预读
- 用于预加载大文件
- 数据库批量导入时可用

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o pagecache main.c
```

### 运行

```bash
./pagecache
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
  Linux 页缓存学习演示
========================================

--- Demo 1: 页缓存基本概念 ---
[pagecache]   初始状态: free=8192 MB, cached=16384 MB

--- Demo 2: posix_fadvise 提示 ---
[pagecache] 创建测试文件: /tmp/pagecache_test.dat (10 MB)
[pagecache] 第一次读取（冷缓存）:
[pagecache]   顺序读取: 10485760 字节, 耗时 50.23 ms
[pagecache] 第二次读取（热缓存）:
[pagecache]   顺序读取: 10485760 字节, 耗时 0.12 ms

========================================
=== PASS ===
========================================
```

## 相关资源

- `man posix_fadvise` — 文件预读提示
- `man readahead` — 预读系统调用
- `man 2 open` — O_DIRECT 说明
- `/proc/meminfo` — 缓存统计信息
