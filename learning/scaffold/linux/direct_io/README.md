# Direct I/O 直接 I/O

## 功能概述

演示 Linux O_DIRECT 标志绕过 Page Cache 的直接 I/O 机制。

## 核心内容

- O_DIRECT 标志读写（绕过页缓存）
- 用户态缓冲区对齐要求（512B/4K）
- Direct I/O vs Buffered I/O 原理对比
- 数据库引擎 Direct I/O 配置

## 编译运行

```bash
make
./direct_io
make clean
```

## 依赖

- Linux 系统（O_DIRECT 需要内核支持）
- glibc（posix_memalign）

## 输出示例

```
========================================
  Linux Direct I/O 直接 I/O 演示
========================================

--- Demo 1: 检查 Direct I/O 支持 ---
[direct_io]   /tmp 支持 Direct I/O

--- Demo 2: Direct I/O 读写 ---
[direct_io]   Direct I/O 写入 65536 字节
[direct_io]   Direct I/O 读取 65536 字节

=== PASS ===
```
