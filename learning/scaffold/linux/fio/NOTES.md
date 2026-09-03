# FIO 存储性能基准测试学习笔记

本文档记录 FIO 存储性能基准测试在项目工程代码中的实际应用对照。

## 工程对照

### 1. 存储性能基准 vs 数据库压测工具

FIO 是 Linux 系统级 I/O 压测工具，而数据库存储引擎同样需要关注 I/O 性能。工程中虽然未直接集成 FIO，但存储引擎的页面读写、Buffer Pool、WAL 等模块的 I/O 行为与 FIO 测试直接相关，相关代码位于 `c:\code\book\engineering\src\db\storage\` 目录下。

**对照维度：**

| 维度 | FIO 基准测试 | 数据库存储引擎 |
|------|-------------|---------------|
| 测试对象 | 块设备 / 文件系统 | 数据库文件（`disk_open` 创建的 .db 文件） |
| I/O 模式 | read/write/randread/randrw | 顺序扫描 / 随机读写（BTree 查找） |
| 块大小 | 可配置（4K / 1MB） | 固定 8KB（`BUF_PAGE_SIZE = 8192`） |
| 并发模型 | numjobs + iodepth | Buffer Pool 并发 + 事务并发 |
| 直接 I/O | `--direct=1` | `O_DIRECT` 绕过页缓存 |
| 指标 | IOPS / 带宽 / 延迟 | 事务吞吐 / 查询延迟 / 缓存命中率 |

### 2. 数据库 I/O 模式分析

工程存储引擎的几种典型 I/O 模式可以通过 FIO 模拟：

```c
// disk.h — 数据库底层 I/O 接口 (c:\code\book\engineering\include\db\disk.h)

// 顺序读（全表扫描）— 对应 FIO: --rw=read --bs=8k
ssize_t disk_pread(db_file_t *file, uint64_t offset, void *buf, size_t count);

// 顺序写（批量插入）— 对应 FIO: --rw=write --bs=8k
ssize_t disk_pwrite(db_file_t *file, uint64_t offset, const void *buf, size_t count);

// 随机读（BTree 索引查找）— 对应 FIO: --rw=randread --bs=8k
page_t *disk_read_page(db_file_t *file, page_id_t page_id);

// 随机写（更新操作）— 对应 FIO: --rw=randwrite --bs=8k
int disk_write_page(db_file_t *file, page_id_t page_id, const page_t *page);
```

### 3. 直接 I/O 策略

数据库为减少双缓存问题，使用 `O_DIRECT` 标志打开文件：

```c
// bufmgr.c — Buffer Pool 配置 (c:\code\book\engineering\src\db\storage\buffer\bufmgr.c)
// 直接 I/O 是数据库的标准做法，原因：
// 1. 数据库有 Buffer Pool，不需要内核页缓存
// 2. 避免数据在用户态和内核态之间双重复制
// 3. 精确控制缓存内容（Clock-Sweep 算法）

// 等效 FIO 参数：
// fio --direct=1 --bs=8k ...
```

### 4. 数据库压测场景与 FIO 参数映射

| 数据库场景 | FIO 模拟命令 | 关注指标 |
|-----------|-------------|---------|
| 全表扫描 | `fio --rw=read --bs=128k --iodepth=1` | 带宽 MB/s |
| 索引查找 | `fio --rw=randread --bs=8k --iodepth=32` | IOPS + 延迟 |
| 批量导入 | `fio --rw=write --bs=128k --iodepth=1` | 带宽 MB/s |
| OLTP 混合 | `fio --rw=randrw --bs=8k --iodepth=32` | IOPS + p99 延迟 |
| WAL 日志写 | `fio --rw=write --bs=4k --iodepth=1 --fsync=1` | fsync 延迟 |

### 5. IOPS / 带宽 / 延迟 在数据库中的意义

**IOPS（每秒操作数）：**
- 评估小事务吞吐能力（OLTP）
- Buffer Pool 命中率低时，IOPS 直接决定并发能力
- 工程中 BTree 的随机页读取依赖磁盘 IOPS

**带宽（MB/s）：**
- 评估大查询吞吐能力（OLAP）
- 全表扫描、批量导入的性能瓶颈
- 顺序 I/O 带宽远高于随机 I/O（SSD: ~500MB/s vs ~50MB/s 随机）

**延迟（微秒/毫秒）：**
- 评估单次查询响应时间
- SSD 随机读: ~100 us，HDD 随机读: ~5000 us
- WAL fsync 延迟直接影响事务提交延迟

### 6. 工程中可扩展的压测能力

工程代码中包含 `kv_t` 即 KV 数据库句柄，可通过以下方式扩展压测：

```c
// 基于 kv.h (c:\code\book\engineering\include\db\kv.h) 的压测思路
kv_t *db = kv_open("bench.db");

// 顺序写压测（模拟 FIO seq-write）
for (int i = 0; i < N; i++) {
    kv_put(db, key, key_len, value, value_len);
}

// 随机读压测（模拟 FIO rand-read）
for (int i = 0; i < N; i++) {
    kv_get(db, random_key, random_key_len, &val, &len);
}

kv_close(db);
```

## 参考源码

- `c:\code\book\engineering\include\db\disk.h` — 数据库底层 I/O 接口
- `c:\code\book\engineering\include\db\buf.h` — Buffer Pool 接口（页面缓存）
- `c:\code\book\engineering\src\db\storage\buffer\bufmgr.c` — Buffer Pool 实现
- `c:\code\book\engineering\src\db\storage\wal\wal.c` — WAL 日志 I/O
- `c:\code\book\engineering\include\db\kv.h` — KV 数据库接口
