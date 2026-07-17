# PostgreSQL 风格存储架构设计

## Context

当前数据库已实现基础锁系统（行锁、表锁、意向锁、乐观锁），但缺乏 PostgreSQL 风格的核心存储架构：
- Catalog 系统（元数据管理）
- Access Method (AM) 层（访问方法抽象）
- Buffer Pool（页面缓存）
- Relation/Heap 存储

本设计参考 PostgreSQL 架构，实现一个轻量级但完整的存储子系统。

## Goals / Non-Goals

**Goals:**
- 实现 Catalog 系统管理表、列、索引元数据
- 实现 Access Method 接口抽象（支持 Heap 和 BTree）
- 实现 Buffer Pool 缓存管理页面
- 实现 Heap 表的插入、查询、删除操作
- 与现有 KV 存储、锁系统、WAL 集成

**Non-Goals:**
- 不实现完整的 PostgreSQL 兼容性
- 不实现 MVCC/事务快照（未来扩展）
- 不实现复杂的查询优化器
- 不实现分区表、物化视图

## Decisions

### Decision 1: 使用现有 KV 存储作为底层

**选择**: 基于现有的 KV 存储引擎（`src/db/storage/kv.c`）实现页面存储

**理由**:
- 现有 KV 引擎已支持 WAL、事务、崩溃恢复
- 减少重复代码，复用已有基础设施
- KV 接口足够简单，适合作为 Page 存储后端

**替代方案考虑**:
- 直接操作文件：更底层但需自己实现 WAL 和崩溃恢复
- 第三方存储库：增加依赖，不符合项目约束

### Decision 2: Catalog 使用内存 + 持久化双模式

**选择**: Catalog 数据结构存储在内存，变更时持久化到系统表

**理由**:
- 内存访问速度快，符合高性能需求
- 系统表本身是 Catalog，减少数据复制
- 启动时从系统表加载 Catalog 到内存

**替代方案考虑**:
- 全内存 Catalog：快速但启动慢，持久化复杂
- 全持久化 Catalog：可靠性好但性能差

### Decision 3: Buffer Pool 使用 Clock-Sweep 置换算法

**选择**: Clock-Sweep (Second Chance) 算法

**理由**:
- 实现简单，效果接近 LRU
- 不需要维护精确的访问时间戳
- PostgreSQL 使用类似算法

**替代方案考虑**:
- LRU：需要记录每次访问时间，内存开销大
- LRU-K：复杂度高，不必要
- ARC：自适应但实现复杂

### Decision 4: AM 层采用接口函数指针模式

**选择**: 类似 PostgreSQL 的函数指针表接口

**理由**:
- 保持与 PostgreSQL 概念一致
- 便于扩展新的 Access Method
- 每个 Relation 可独立选择访问方法

**替代方案考虑**:
- 虚函数（C++）：需使用 RTTI，增加复杂性
- 策略模式：类似但更显式

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     SQL Executor                        │
├─────────────────────────────────────────────────────────┤
│                       Catalog                           │
│   ┌─────────┐  ┌─────────┐  ┌─────────┐               │
│   │ pg_class│  │pg_attrib│  │ pg_index│               │
│   └────┬────┘  └────┬────┘  └────┬────┘               │
├────────┼────────────┼────────────┼─────────────────────┤
│        │        Buffer Pool      │                      │
│        │  ┌──────────────────┐   │                      │
│        │  │ Page Hash Table  │   │                      │
│        │  │ Clock Replacer   │   │                      │
│        │  │ Buffer Descs     │   │                      │
│        │  └──────────────────┘   │                      │
├────────┼─────────────────────────┼─────────────────────┤
│        │         KV Store        │                      │
│        │  (WAL + Transactions)   │                      │
├────────┴─────────────────────────┴─────────────────────┤
│                    File System                          │
└─────────────────────────────────────────────────────────┘
```

## Module Design

### 1. Catalog 模块

**核心数据结构**:

```c
// pg_class 系统表结构
typedef struct {
    uint32_t oid;           // 表 OID
    char relname[NAMEDATALEN];  // 表名
    char relnamespace;      // 命名空间
    int reltype;            // 类型 (heap, index, etc.)
    int relkind;            // 种类 (TABLE, INDEX, VIEW)
    int relnatts;           // 列数
    int relfilenode;        // 物理文件节点
    // ... 其他字段
} FormData_pg_class;

// 内存中的 Catalog Cache
typedef struct {
    OidHashTable *class_cache;    // pg_class 缓存
    OidHashTable *attr_cache;     // pg_attribute 缓存
    RelationLock lock;            // Catalog 锁
} CatalogCache;
```

**关键接口**:
- `CatalogCacheInitialize()` - 启动时加载 Catalog
- `CatalogGetRelation(HeapTuple tuple)` - 获取表信息
- `CatalogAddRelation(Relation rel)` - 添加新表
- `CatalogRemoveRelation(Oid relid)` - 删除表
- `CatalogUpdateRelation(Relation rel)` - 更新表信息

### 2. Buffer Pool 模块

**核心数据结构**:

```c
// Buffer 描述符
typedef struct BufferDesc_s {
    uint32_t    buf_id;         // Buffer ID
    atomic_int  state;          // 状态标志
    uint32_t    relfilenode;    // 所属 Relation
    BlockNumber blocknum;       // 块号
    int         usage_count;    // Clock-Sweep 计数
    slock_t     buf_hdr_lock;   // 头部锁
    LWLock      content_lock;   // 内容锁
} BufferDesc;

// Buffer Pool
typedef struct BufferPool_s {
    BufferDesc  *descriptors;   // Buffer 描述符数组
    char        **block_data;   // 实际页面数据
    int         size;           // Buffer 数量
    int         next_victim;    // Clock 指针
    
    // Hash 表: (relfilenode, blocknum) → buf_id
    OidHashTable *buf_hash;
    
    // 统计
    uint64_t    hits;
    uint64_t    misses;
} BufferPool;
```

**关键接口**:
- `BufferPoolInit(int nbuffers)` - 初始化 Buffer Pool
- `ReadBuffer(Relation rel, BlockNumber blockNum)` - 读取页面
- `WriteBuffer(BufferDesc *buf)` - 写回页面
- `ReleaseBuffer(BufferDesc *buf)` - 释放 Buffer
- `InvalidateBuffer(BufferDesc *buf)` - 使 Buffer 无效

### 3. Access Method 模块

**核心数据结构**:

```c
// Access Method 函数表（类似 PostgreSQL 的 TableAmRoutine）
typedef struct AccessMethodFunctions_s {
    // 生命周期
    RelationOpenMethod     relation_open;
    RelationCloseMethod    relation_close;
    
    // 扫描
    TableScanBeginMethod   table_scan_begin;
    TableScanEndMethod     table_scan_end;
    TableScanNextMethod    table_scan_next;
    
    // 修改
    TableInsertMethod      table_insert;
    TableUpdateMethod      table_update;
    TableDeleteMethod      table_delete;
    
    // 索引
    IndexBuildMethod       index_build;
    IndexScanMethod        index_scan;
} AccessMethodFunctions;

// Relation 结构
typedef struct RelationData_s {
    Oid                 rd_id;          // Relation OID
    Oid                 rd_owner;       // 所有者
    RelFileNode         rd_node;        // 物理文件节点
    RD             *rd_rel;         // pg_class 行
    TupleDesc          rd_att;          // 列描述
    LockMode           rd_lockmode;     // 当前持有的锁
    AccessMethodFunctions *rd_am;       // AM 函数表
    void               *rd_amstate;     // AM 私有状态
    
    // Buffer 管理
    BufferPool         *rd_bufferpool;
} RelationData;
```

### 4. Heap AM 模块

**核心数据结构**:

```c
// Heap Page 头部
typedef struct PageHeaderData_s {
    LSN     pd_lsn;            // 最后修改的 WAL LSN
    uint16  pd_tli;            // TimeLine ID
    uint16  pd_flags;          // 页面标志
    LocationIndex pd_lower;    // 空闲空间起始位置
    LocationIndex pd_upper;    // 空闲空间结束位置
    uint16  pd_special;        // 特殊区域起始
    ItemIdData pd_linp[1];     // Line Pointer 数组
} PageHeaderData;

// 元组标识符
typedef struct ItemPointerData_s {
    uint32  ip_blkid;          // 页面号
    uint16  ip_posid;          // Line Pointer 位置 (1-based)
} ItemPointerData;

// 元组
typedef struct HeapTupleData_s {
    uint32      t_len;         // 元组长度
    Oid         t_tableOid;    // 所属表 OID
    ItemPointerData t_ctid;    // 当前版本指针
    // ... 元组头
    // ... 用户数据
} HeapTupleData;
```

**关键接口**:
- `heap_open(Relation rel)` - 打开堆表
- `heap_insert(Relation rel, HeapTuple tup)` - 插入元组
- `heap_update(Relation rel, HeapTuple old, HeapTuple new)` - 更新元组
- `heap_delete(Relation rel, HeapTuple tup)` - 删除元组
- `heap_getnext(HeapScanDesc scan)` - 获取下一元组

### 5. BTree AM 模块

**核心数据结构**:

```c
// BTree 页面头部
typedef struct BTPageOpaqueData_s {
    BlockNumber  btp_prev;     // 兄弟页面
    BlockNumber  btp_next;     // 兄弟页面
    uint32       btp_level;    // 树层级 (0 = 叶子)
    uint32       btp_flags;    // 页面类型标志
    TransactionId btp_xact_base; // 事务基础
} BTPageOpaqueData;

// BTree 元组
typedef struct BTTupleData_s {
    ItemPointerData itp_ctid;  // 指向子页面或 HEAP 元组
    uint16          itp_len;   // 键长度
    // ... 索引键数据
} BTTupleData;
```

**关键接口**:
- `btbuild(Relation index, Relation heap)` - 构建索引
- `btinsert(Relation index, Datum *values)` - 插入索引项
- `btscan(Relation index, ScanKey key)` - 索引扫描
- `btgetnext(IndexScanDesc scan)` - 获取下一项

## Data Flow

### 读取数据流程

```
1. SQL: SELECT * FROM users WHERE id = 1
         │
         ▼
2. Parser 分析查询，确定使用 users 表
         │
         ▼
3. Catalog.GetRelation("users") 获取表元数据
         │
         ▼
4. relation_open(oid) 打开 Relation
         │
         ▼
5. 扫描计划使用 IndexScan 或 SeqScan
         │
         ▼
6. BufferPool.ReadPage(rel, blocknum) 读取页面
         │
         ▼ 页面不在缓存
         ▼
7. KV Store 读取页面数据到 Buffer
         │
         ▼
8. AM 函数处理元组，返回给 Executor
```

### 写入数据流程

```
1. SQL: INSERT INTO users (id, name) VALUES (1, 'test')
         │
         ▼
2. Catalog.GetRelation("users") 获取元数据
         │
         ▼
3. relation_open 打开 Relation (IX 锁)
         │
         ▼
4. BufferPool.GetPage(rel, P_NEW) 获取新页面
         │
         ▼
5. HeapInsert(rel, tuple) 插入元组
         │
         ▼
6. Buffer.MarkDirty() 标记脏页
         │
         ▼
7. WAL 日志写入
         │
         ▼
8. Commit 时刷盘
```

## Risks / Trade-offs

| Risk | Impact | Mitigation |
|------|--------|------------|
| Buffer Pool 命中率低 | 性能下降 | 监控命中率，可配置 Buffer 大小 |
| Catalog 缓存一致性 | 数据不一致 | 使用锁保护，变更时刷新缓存 |
| 页面碎片化 | 存储浪费 | 定期 VACUUM 整理 |
| 锁竞争 | 并发性能 | 使用细粒度锁，减少锁持有时间 |

## Open Questions

1. **MVCC 支持**：当前实现不支持多版本并发控制，是否需要实现？
2. **WAL 集成**：Buffer 刷盘时机与 WAL 日志的协调策略？
3. **索引并发构建**：创建索引时是否阻塞表写入？
4. **Catalog 持久化格式**：使用现有的 KV 存储还是独立的表文件？
