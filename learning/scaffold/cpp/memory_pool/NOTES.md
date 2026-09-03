# 内存池笔记

## 核心概念

内存池（Memory Pool）是一种预分配内存的技术，通过减少系统调用（malloc/free）来提升性能。

## 三种实现对比

| 特性 | FixedSizePool | ObjectPool | Arena |
|------|---------------|------------|-------|
| 分配大小 | 固定 | 可变（模板类） | 可变 |
| 单独释放 | 支持 | 支持 | 不支持 |
| 批量释放 | - | - | 支持 |
| 复杂度 | 低 | 中 | 低 |

## 与工程代码的关联

### Buffer Pool (engineering/src/db/storage/bufmgr.c)

在 PostgreSQL 风格的存储引擎中，Buffer Pool 是页面级的内存管理：

```c
// bufmgr.c 中的 BufferDesc 结构
typedef struct BufferDesc {
    PageHeaderData data;        // 页面数据
    int buffer_id;              // buffer 编号
    int fork_num;               // 文件分叉号
    // ...
} BufferDesc;
```

关键设计思想：
1. **预分配固定大小的页面** - 每页 8KB，类似于 FixedSizePool
2. **哈希表快速查找** - 通过 buffer_id 快速定位页面
3. **Clock-Sweep 置换算法** - 类似内存池的淘汰策略
4. **脏页批量刷盘** - 类似 Arena 的批量释放

### 对比

| 本演示 | Buffer Pool |
|--------|-------------|
| FixedSizePool | BufferDesc 预分配 |
| 链表管理空闲块 | 哈希表管理 buffer_id |
| Arena Reset | 检查点（checkpoint）批量刷盘 |
