# Linux Socket 缓冲区学习卡片

本学习卡片演示 Linux Socket 缓冲区的原理与调优，包括 getsockopt/setsockopt 用法、SO_SNDBUF/SO_RCVBUF 控制、以及 TCP 特定选项。

## 学习目标

1. 理解 Socket 发送/接收缓冲区的作用
2. 掌握 getsockopt/setsockopt 系统调用
3. 理解内核缓冲区翻倍的原因
4. 掌握 TCP_NODELAY 等 TCP 选项

## 核心概念

### SO_SNDBUF / SO_RCVBUF

```
应用层              内核缓冲区              网络
  |  write()  -->  |  TCP 发送队列  -->  |
  |               |                    |
  |  发送缓冲区    |   拥塞窗口控制      |
  |               |                    |
  v               v                    v
SO_SNDBUF        TCP 协议栈            网卡
```

### getsockopt / setsockopt 用法

```c
#include <sys/socket.h>

int getsockopt(int sock, int level, int optname,
               void *optval, socklen_t *optlen);

int setsockopt(int sock, int level, int optname,
               const void *optval, socklen_t optlen);
```

### 常用 Socket 选项

| 选项 | 级别 | 说明 |
|------|------|------|
| SO_SNDBUF | SOL_SOCKET | 发送缓冲区大小 |
| SO_RCVBUF | SOL_SOCKET | 接收缓冲区大小 |
| SO_REUSEADDR | SOL_SOCKET | 地址复用 |
| TCP_NODELAY | IPPROTO_TCP | 禁用 Nagle 算法 |
| TCP_QUICKACK | IPPROTO_TCP | 快速 ACK 模式 |

### 内核缓冲区翻倍

设置 SO_SNDBUF 时，内核会翻倍：
- 请求：512 KB
- 实际：1 MB（内核增加管理开销）

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o socket_buf main.c
```

### 运行

```bash
./socket_buf
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
  Linux Socket 缓冲区学习演示
========================================

--- Demo 1: 默认缓冲区大小 ---
[socket_buf]   默认 SO_SNDBUF: 262656 bytes (256.5 KB)
[socket_buf]   默认 SO_RCVBUF: 262656 bytes (256.5 KB)

--- Demo 2: 设置缓冲区大小 ---
[socket_buf]   设置后 SO_SNDBUF: 1046528 bytes (1022.0 KB)

========================================
=== PASS ===
========================================
```

## 相关资源

- `man 7 socket` — Socket 选项完整说明
- `man tcp` — TCP 选项说明
- `/proc/sys/net/core/rmem_*` — 接收缓冲区配置
- `/proc/sys/net/core/wmem_*` — 发送缓冲区配置
