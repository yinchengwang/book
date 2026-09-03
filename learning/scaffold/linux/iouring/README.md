# iouring — io_uring 异步 I/O

## 简介

演示 Linux io_uring 异步 I/O 接口，对比传统同步 I/O，展示批量提交和完成队列机制。

## 编译

```bash
gcc -std=c11 -Wall -o iouring main.c
```

## 运行

```bash
./iouring
```

## 核心概念

### io_uring 架构

```
  用户空间                内核空间
  ┌────────┐          ┌──────────┐
  │ SQ (环)│ ──提交──→ │ I/O 引擎 │
  └────────┘          └──────────┘
  ┌────────┐  ←完成──
  │ CQ (环)│
  └────────┘
```

### 与传统 I/O 对比

| 方案 | 系统调用 | 延迟 | 吞吐 |
|------|---------|------|------|
| read/write | 每次 1 次 | 基准 | 基准 |
| io_uring 批量 | N 次合并 1 次 | 更低 | 5-10x |

## 操作类型

- `IORING_OP_READ/WRITE`：文件读写
- `IORING_OP_FSYNC`：文件同步
- `IORING_OP_SEND/RECV`：网络收发

## 内核要求

- Linux 5.1+：基本支持
- Linux 5.15+：推荐版本（稳定性/性能最佳）

## 扩展

- 安装 `liburing-dev` 使用用户层库
- 读取 `man io_uring_setup` 了解详细参数
