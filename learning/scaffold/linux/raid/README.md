# Linux RAID 阵列 学习卡片

本学习卡片演示 Linux RAID 0/1/5/10 + 条带化 + 冗余 + 重建。

## 学习目标

1. 理解 RAID 阵列 的核心概念
2. 掌握相关 API 和工具的用法
3. 理解与数据库存储引擎的工程对照

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o raid main.c
```

### 运行

```bash
./raid
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
