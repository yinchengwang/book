# Linux NFS 网络文件系统 学习卡片

本学习卡片演示 Linux NFS 挂载 + 远程文件操作 + 缓存一致性。

## 学习目标

1. 理解 NFS 网络文件系统 的核心概念
2. 掌握相关 API 和工具的用法
3. 理解与数据库存储引擎的工程对照

## 编译与运行

### 编译

```bash
make
# 或直接使用 gcc
gcc -std=c11 -Wall -o nfs main.c
```

### 运行

```bash
./nfs
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
