# lsm — LSM-Tree 原理与实现

## 组件

- **MemTable**：内存跳表（SkipList），支持 O(log n) 的读写
- **WAL**：写前日志（Write-Ahead Log），确保宕机可恢复
- **SSTable**：排序字符串表，分层持久化到磁盘
- **Bloom Filter**：布隆过滤器，加速点查询，快速判断 key 是否存在

## 编译运行

```bash
make && ./test
```

## 核心流程

1. **写入**：先写 WAL（持久化保障） → 再写 MemTable
2. **MemTable 刷盘**：达到容量上限时，将跳表内容顺序写入 SSTable
3. **读取**：BloomFilter 预判 → MemTable 查询 → SSTable 查询
4. **分层**：LevelDB 风格分层（L0 → L1 → L2 ...），每层容量指数增长
