# SSTable 块结构设计笔记

## 概述

SSTable（Sorted String Table）是 LevelDB/RocksDB 等 LSM 树存储引擎的核心数据结构。本笔记对照 `engineering/src/db/storage/page/page.c` 中的页面布局设计，分析 SSTable 的块结构。

## 与 page.c 的设计对照

### page.c 的页面结构

```c
typedef struct {
    page_header_t header;          /* 6字节：page_id + page_type + checksum */
    uint8_t data[PAGE_DATA_SIZE]; /* 4062字节：实际数据 */
} page_t;
```

关键特性：
- **固定大小**：每页 4KB（PAGE_SIZE）
- **页头固定**：前 6 字节存储元数据（page_id、page_type、checksum）
- **数据区灵活**：剩余空间用于存储实际数据
- **校验和保护**：每个页面都有 XOR 校验和

### SSTable 的块结构

| 块类型 | 大小 | 作用 |
|--------|------|------|
| Data Block | 可变（默认4KB） | 存储有序键值对 |
| Index Block | 可变 | 存储 Data Block 的索引（last_key + offset） |
| Filter Block | 可变 | 存储 Bloom Filter，快速过滤不存在的键 |
| Footer | 固定48字节 | 存储各区域的偏移指针 |

### 设计差异分析

1. **页面 vs 块**
   - `page.c`：物理存储单位，大小固定为 4KB
   - SSTable：逻辑存储单位，可配置大小（通常 4KB-64KB）

2. **元数据位置**
   - `page.c`：元数据在页头（header）
   - SSTable：元数据在 Footer（块头也可以存储元数据）

3. **索引结构**
   - `page.c`：通过 free_space_offset 管理空间
   - SSTable：Index Block 存储每个 Data Block 的最后一个键和偏移

4. **快速查找**
   - `page.c`：通过线性扫描或 B-Tree 索引
   - SSTable：通过 Bloom Filter 快速判断键不存在，减少 I/O

## SSTable 文件布局

```
+------------------+
|   Data Block 1   |  0
+------------------+
|   Data Block 2   |
+------------------+
|       ...        |
+------------------+
|   Index Block    |  <- 指向所有 Data Block
+------------------+
|  Filter Block    |  <- Bloom Filter
+------------------+
|     Footer       |  <- 指向 Index 和 Filter
+------------------+
```

## Bloom Filter 设计

SSTable 的 Filter Block 使用 Bloom Filter 快速判断键是否存在：

```c
typedef struct {
    uint32_t num_hashes;   /* 哈希函数数量 */
    uint64_t bitmask;      /* 位数组 */
} bloom_filter_t;
```

- **添加键**：使用多个哈希函数，将对应位设为 1
- **查询键**：所有对应位都为 1 才可能存在

优点：
- 查询 O(1)，无需读取 Data Block
- 有效过滤不存在的键

缺点：
- 存在假阳性（false positive）
- 不能删除键（会误删其他键的位）

## 空间利用率

假设 BLOCK_SIZE = 4096 字节：

| 块类型 | 理论最大条目数 | 实际使用 |
|--------|---------------|----------|
| Data Block | 16 个 KV 对 | 取决于键值大小 |
| Index Block | 16 个索引条目 | 每个条目含 last_key + offset |
| Filter Block | 4 个过滤器 | 每个过滤器 64 位 |

## 与 page.c 的协同

虽然 SSTable 和 `page.c` 使用不同的存储模型，但两者都遵循：

1. **固定块大小**：便于磁盘 I/O 优化
2. **元数据分离**：头部存储元数据，数据区存储实际内容
3. **校验和保护**：确保数据完整性
4. **空间管理**：通过 offset 追踪可用空间

SSTable 适合顺序写入的大数据场景，而 `page.c` 的页面模型适合随机读写的 OLTP 场景。
