# Linux FIO 基准测试 学习卡片

本学习卡片演示 Linux 存储性能基准测试 + 读写测试 + 延迟分布。

## 学习目标

1. 理解 FIO 基准测试 的核心概念
2. 掌握相关 API 和工具的用法
3. 理解与数据库存储引擎的工程对照

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o fio main.c
```

### 运行

```bash
./fio
# 或使用 make
make check
```

### 清理

```bash
make clean
```

## 工程对照

详见 [NOTES.md](NOTES.md) — 与数据库存储引擎的实际代码对照分析。

## 相关资源

- `man 2 <syscall>` — 系统调用手册
- Linux 内核文档 — 存储子系统
- 数据库存储引擎源码（`engineering/src/db/storage/`）
