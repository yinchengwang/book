# Access Method (AM) 层规格

## 1. 概述

Access Method 层提供 Relation（表、索引）的统一访问抽象，类似于 PostgreSQL 的 Table Access Method 和 Index Access Method。

## 2. 架构

```
┌─────────────────────────────────────────────────────┐
│                    SQL Executor                     │
├─────────────────────────────────────────────────────┤
│                    Access Method                    │
│  ┌─────────────────────────────────────────────┐   │
│  │           Relation Interface                │   │
│  │   relation_open() / relation_close()        │   │
│  └─────────────────────────────────────────────┘   │
│  ┌─────────────────┐    ┌─────────────────┐        │
│  │    Heap AM      │    │    BTree AM     │        │
│  │  (堆表访问)      │    │   (B树索引)     │        │
│  └────────┬────────┘    └────────┬────────┘        │
│           │                      │                  │
│  ┌────────┴──────────────────────┴────────┐        │
│  │           Buffer Pool                  │        │
│  └─────────────────────────────────────────┘        │
└─────────────────────────────────────────────────────┘
```

## 3. 核心接口

### 3.1 Relation 生命周期

```c
/**
 * @brief 打开 Relation
 * @param relid Relation OID
 * @param lockmode 锁模式
 * @return Relation 句柄，失败返回 NULL
 */
Relation relation_open(Oid relid, LOCKMODE lockmode);

/**
 * @brief 打开指定 RelFileNode 的 Relation
 * @param rnode 物理文件节点
 * @param lockmode 锁模式
 * @return Relation 句柄
 */
Relation relation_opennode(RelFileNode rnode, LOCKMODE lockmode);

/**
 * @brief 关闭 Relation
 * @param rel Relation 句柄
 * @param lockmode 锁模式
 */
void relation_close(Relation rel, LOCKMODE lockmode);

/**
 * @brief 创建新 Relation
 * @param relname 名称
 * @param relkind 种类 (RELKIND_TABLE, RELKIND_INDEX)
 * @param tupledesc 列描述
 * @return Relation 句柄
 */
Relation relation_create(const char *relname, char relkind, TupleDesc tupledesc);

/**
 * @brief 删除 Relation
 * @param relid Relation OID
 */
void relation_drop(Oid relid);
```

### 3.2 扫描接口

```c
// 扫描方向
typedef enum ScanDirection {
    ForwardScanDirection = 0,    // 前向扫描
    BackwardScanDirection = 1,   // 后向扫描
    NoMovementScanDirection = 2  // 不移动
} ScanDirection;

// 扫描键
typedef struct ScanKeyData_s {
    int         sk_attno;        // 列号
    int         sk_strategy;     // 比较策略
    Oid         sk_procedure;    // 比较函数
    Datum       sk_argument;     // 比较值
} ScanKeyData;

// 表扫描描述符
typedef struct TableScanDescData_s {
    Relation    rs_rd;           // 扫描的 Relation
    BlockNumber rs_startblock;   // 起始块
    BlockNumber rs_numblocks;    // 块数
    BlockNumber rs_cblock;       // 当前块
    OffsetNumber rs_coffset;     // 当前偏移
    ScanDirection rs_direction;  // 扫描方向
    ScanKey      rs_key;         // 扫描键
} TableScanDescData;

/**
 * @brief 开始表扫描
 * @param rel Relation
 * @param nkeys 扫描键数量
 * @param key 扫描键数组
 * @return 扫描描述符
 */
TableScanDesc table_beginscan(Relation rel, int nkeys, ScanKey key);

/**
 * @brief 结束扫描
 * @param scan 扫描描述符
 */
void table_endscan(TableScanDesc scan);

/**
 * @brief 获取下一条元组
 * @param scan 扫描描述符
 * @return 元组，扫描结束返回 NULL
 */
HeapTuple table_getnext(TableScanDesc scan);

/**
 * @brief 重新开始扫描
 * @param scan 扫描描述符
 */
void table_rescan(TableScanDesc scan, ScanKey key);
```

### 3.3 元组操作

```c
/**
 * @brief 插入元组
 * @param rel Relation
 * @param tup 元组
 * @return 插入的元组（带新 CTID）
 */
HeapTuple heap_insert(Relation rel, HeapTuple tup);

/**
 * @brief 更新元组
 * @param rel Relation
 * @param oldtup 旧元组
 * @param newtup 新元组
 * @return 新元组（带新 CTID）
 */
HeapTuple heap_update(Relation rel, HeapTuple oldtup, HeapTuple newtup);

/**
 * @brief 删除元组
 * @param rel Relation
 * @param tup 要删除的元组
 * @return 已删除元组
 */
HeapTuple heap_delete(Relation rel, HeapTuple tup);

/**
 * @brief 锁定元组
 * @param rel Relation
 * @param tup 元组
 * @param mode 锁模式
 * @param options 锁选项
 * @return 元组
 */
HeapTuple heap_lock_tuple(Relation rel, HeapTuple tup, LOCKMODE mode, int options);
```

## 4. 数据结构

### 4.1 Relation 结构

```c
typedef struct RelationData_s {
    Oid                 rd_id;          // Relation OID
    Oid                 rd_owner;       // 所有者
    RelFileNode         rd_node;        // 物理文件节点
    RelFileKind         rd_relkind;     // 种类
    
    // 元数据
    Form_pg_class       rd_rel;         // pg_class 行
    TupleDesc           rd_att;         // 列描述
    
    // 锁
    LOCKMODE            rd_lockmode;    // 当前持有的锁
    int                 rd_refcnt;      // 引用计数
    
    // AM 函数表
    const TableAmRoutine *rd_tableam;   // 表 AM 函数
    const IndexAmRoutine *rd_indexam;   // 索引 AM 函数
    
    // AM 私有状态
    void               *rd_amstate;     // AM 私有数据
    
    // Buffer 管理
    BufferPool         *rd_bufferpool;
    
    // 后端信息
    BackendId           rd_backend;     // 后端 ID
    bool                rd_isvalid;     // 是否有效
    bool                rd_isnailed;    // 是否固定
} RelationData;
```

### 4.2 TupleDesc 结构

```c
typedef struct TupleDescData_s {
    int         natts;          // 列数
    Oid         tdtypeid;       // 行类型 OID
    int         tdtypmod;       // 类型修饰符
    int         tdhasoid;       // 是否有 OID
    int         tdrefcount;     // 引用计数
    
    Form_pg_attribute *attrs;   // 列定义数组
} TupleDescData;
```

### 4.3 HeapTuple 结构

```c
typedef struct HeapTupleData_s {
    uint32          t_len;          // 元组长度
    Oid             t_tableOid;     // 所属表 OID
    ItemPointerData t_ctid;         // 当前版本指针
    ItemPointerData t_xmax;         // 删除/更新事务
    ItemPointerData t_xmin;         // 插入事务
    CommandId       t_cmin;         // 命令 ID
    CommandId       t_cmax;         // 命令 ID
    
    // 用户数据开始
    // ... (t_bits, NULL bitmap)
    // ... ( Datum 数据 )
} HeapTupleData;
```

### 4.4 Page 结构

```c
// 页面大小（默认 8KB）
#define BLCKSZ 8192

typedef struct PageHeaderData_s {
    LSN             pd_lsn;            // 最后修改 LSN
    uint16          pd_tli;            // TimeLine ID
    uint16          pd_flags;          // 页面标志
    LocationIndex   pd_lower;          // 空闲空间起始
    LocationIndex   pd_upper;          // 空闲空间结束
    LocationIndex   pd_special;        // 特殊区域起始
    uint16          pd_pagesize_version;
    TransactionId   pd_prune_xid;      // 可清理的最早事务
    ItemIdData      pd_linp[1];        // Line Pointer 数组 (变长)
} PageHeaderData;

// Line Pointer
typedef struct ItemIdData_s {
    unsigned    lp_off:15,     // 元组偏移 (低 15 位)
                lp_flags:2,    // 标志
                lp_len:15;     // 元组长度 (高 15 位)
} ItemIdData;
```

## 5. BTree AM 接口

### 5.1 索引操作

```c
/**
 * @brief 构建索引
 * @param index 索引 Relation
 * @param heap 堆表 Relation
 * @param index_info 索引信息
 */
void btbuild(Relation index, Relation heap, IndexInfo *index_info);

/**
 * @brief 插入索引项
 * @param index 索引 Relation
 * @param values 索引键值
 * @param heap_ctid 指向堆元组的指针
 */
void btinsert(Relation index, Datum *values, ItemPointer heap_ctid);

/**
 * @brief 开始索引扫描
 * @param index 索引 Relation
 * @param nkeys 扫描键数
 * @param key 扫描键
 * @return 索引扫描描述符
 */
IndexScanDesc bt_beginscan(Relation index, int nkeys, ScanKey key);

/**
 * @brief 获取下一条索引匹配
 * @param scan 扫描描述符
 * @return 匹配的 heap ctid
 */
ItemPointer bt_getnext(IndexScanDesc scan);

/**
 * @brief 结束扫描
 * @param scan 扫描描述符
 */
void bt_endscan(IndexScanDesc scan);
```

### 5.2 索引扫描描述符

```c
typedef struct IndexScanDescData_s {
    Relation        iss_RelationDesc;      // 索引 Relation
    Relation        iss_RangeTableRelation; // 堆表 Relation
    int             iss_nkeys;             // 扫描键数
    ScanKey         iss_key;               // 扫描键
    ItemPointer     iss_current;           // 当前匹配
    ScanDirection   iss_direction;         // 扫描方向
    
    // AM 私有状态
    void           *iss_amstate;           // AM 私有数据
} IndexScanDescData;
```

## 6. 锁模式

```c
typedef enum LOCKMODE {
    AccessShareLock = 0,      // SELECT
    RowShareLock = 1,         // SELECT FOR UPDATE
    RowExclusiveLock = 2,     // INSERT/UPDATE/DELETE
    ShareUpdateExclusiveLock = 3, // VACUUM
    ShareLock = 4,            // CREATE INDEX
    ShareRowExclusiveLock = 5, // LIKE
    ExclusiveLock = 6,        // 阻塞所有
    AccessExclusiveLock = 7   // ALTER TABLE, DROP TABLE
} LOCKMODE;
```

## 7. 页面类型

```c
#define BTP_LEAF         (1 << 0)   // 叶子页面
#define BTP_INTERNAL     (1 << 1)   // 内部页面
#define BTP_ROOT         (1 << 2)   // 根页面
#define BTP_DELETED      (1 << 3)   // 已删除
#define BTP_META         (1 << 4)   // 元数据页面
#define BTP_SAME_RElNODE (1 << 5)   // 与 Relation 同节点
```

## 8. 限制

- 不支持 MVCC（未来扩展）
- 不支持 Heap Only Tuple（HOT）优化（未来扩展）
- 索引只支持 BTree（未来可扩展其他类型）
