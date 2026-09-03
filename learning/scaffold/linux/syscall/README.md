# Linux 系统调用学习卡片

本学习卡片演示 Linux 系统调用的基本用法，包括进程管理、文件操作和内存映射。

## 学习目标

1. 理解什么是系统调用（System Call）
2. 掌握基本的文件操作 syscall：`open`、`read`、`write`、`close`
3. 理解进程 ID 相关 syscall：`getpid`、`getppid`
4. 掌握内存映射 syscall：`mmap`、`munmap`、`msync`
5. 学会使用 `perror` 进行错误处理

## 核心概念

### 系统调用接口

系统调用是用户程序与内核交互的接口。在 Linux 中：
- 每个 syscall 都有对应的 wrapper 函数（glibc 提供）
- 返回值：`-1` 表示错误，并设置 `errno`
- 成功时返回非负值（文件描述符、指针等）

```c
// 典型 syscall 用法
int fd = open(path, O_RDONLY);
if (fd < 0) {
    perror("open");
    return -1;
}
```

### 错误处理

Linux syscall 错误处理遵循一致的模式：

1. 检查返回值是否为 `-1`
2. 使用 `perror()` 或 `strerror(errno)` 获取错误信息
3. 常见的 `errno` 值：
   - `ENOENT`: 文件不存在
   - `EACCES`: 权限不足
   - `ENOSPC`: 磁盘空间不足
   - `EINVAL`: 无效参数

### 内存映射（mmap）

`mmap()` 将文件或设备映射到内存，实现高效的随机访问：

```c
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, offset);
```

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o test main.c
```

### 运行

```bash
./test
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
  Linux 系统调用学习演示
========================================

--- Demo 1: getpid() ---
[syscall] 调用 getpid() 获取当前进程 ID
[syscall] 当前进程 ID: 12345
[syscall] 调用 getppid() 获取父进程 ID
[syscall] 父进程 ID: 12340

--- Demo 2: open/read/write/close ---
[syscall] 调用 open() 创建/打开文件: /tmp/syscall_demo.txt
[syscall] open() 成功，文件描述符: 3
[syscall] 调用 write() 写入数据: Hello, Linux syscall!
[syscall] write() 写入字节数: 22
...

========================================
=== PASS ===
========================================
```

## 相关资源

- `man 2 open` — open 系统调用手册
- `man 2 mmap` — mmap 系统调用手册
- `man 3 perror` — perror 错误处理手册
