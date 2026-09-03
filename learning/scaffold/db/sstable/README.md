# sstable — SSTable 与分层存储

## 文件结构

- **Data Block**：数据区，存储实际的键值对数据
- **Index Block**：索引区，存储指向 Data Block 的索引条目
- **Filter Block**：布隆过滤器（Bloom Filter），快速判断键是否可能存在
- **Footer**：元数据，存储各区域的偏移和大小信息

## 编译运行

```bash
make && ./test
```
