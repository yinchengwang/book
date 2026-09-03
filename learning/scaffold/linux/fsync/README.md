# fsync 磁盘同步机制

## 功能概述

演示 Linux fsync/fdatasync 系统调用及其在数据库 WAL 中的应用。

## 核心内容

- fsync/fdatasync 的内核执行路径
- 性能对比（仅 write vs write+fsync vs write+fdatasync）
- 文件同步语义
- WAL 组提交与异步提交

## 编译运行

```bash
make
./fsync
make clean
```

## 输出示例

```
--- Demo 2: 性能对比 ---
[fsync]   仅 write(100 x 4KB): 123 us
[fsync]   write+fsync(100 x 4KB): 15234 us (123.9x)
[fsync]   fdatasync 节省: 15% vs fsync

=== PASS ===
```
