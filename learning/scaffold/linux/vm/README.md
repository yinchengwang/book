# Linux 虚拟内存学习卡片

本学习卡片演示 Linux 虚拟内存的核心机制，包括 mmap 内存映射、mprotect 内存保护、madvise 访问提示、以及缺页中断。

## 学习目标

1. 理解虚拟内存与物理内存的关系
2. 掌握 mmap/munmap 的用法
3. 理解 mprotect 的内存保护机制
4. 理解缺页中断的类型和处理流程

## 核心概念

### 虚拟地址空间

```
进程虚拟地址空间 (4GB on 32-bit)
0x00000000
    |
    +-- 代码段 (.text)
    |
    +-- 数据段 (.data, .bss)
    |
    +-- 堆 (向上增长)
    |     mmap 区域
    |
    +-- 共享库
    |
    +-- 栈 (向下增长)
    |
0xC0000000  (3GB)
```

### mmap 系统调用

```c
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);

int munmap(void *addr, size_t length);
```

### mprotect 内存保护

```c
#include <sys/mman.h>

int mprotect(void *addr, size_t len, int prot);

/* prot 参数 */
PROT_NONE   /* 不可访问 */
PROT_READ   /* 可读 */
PROT_WRITE  /* 可写 */
PROT_EXEC   /* 可执行 */
```

### madvise 访问提示

```c
#include <sys/mman.h>

int madvise(void *addr, size_t length, int advice);

/* advice 选项 */
MADV_NORMAL    /* 无提示 */
MADV_RANDOM    /* 随机访问 */
MADV_SEQUENTIAL/* 顺序访问 */
MADV_WILLNEED  /* 预加载 */
MADV_DONTNEED  /* 释放内存 */
```

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o vm main.c
```

### 运行

```bash
./vm
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
  Linux 虚拟内存学习演示
========================================

--- Demo 1: mmap 匿名映射 ---
[vm]   映射成功，地址: 0x7f...
[vm]   写入数据到映射内存...
[vm]   写入耗时: 0.05 ms

--- Demo 2: mmap 文件映射 ---
[vm]   文件映射成功，地址: 0x7f...
[vm]   写入内容: "Hello from mmap!"

========================================
=== PASS ===
========================================
```

## 相关资源

- `man mmap` — 内存映射手册
- `man mprotect` — 内存保护手册
- `man madvise` — 内存提示手册
- `/proc/vmstat` — 虚拟内存统计
