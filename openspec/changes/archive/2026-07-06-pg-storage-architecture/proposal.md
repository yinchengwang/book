# PostgreSQL 风格存储架构实现

## 1. 背景与目标

当前数据库实现了基础的锁系统（行锁、表锁、意向锁、死锁检测、乐观锁），但缺乏 PostgreSQL 风格的核心存储架构组件。

本变更实现：
- **Catalog 系统**：表、索引、元信息的集中管理
- **Access Method (AM) 层**：表和索引的访问方法抽象
- **Buffer Pool**：页面缓存管理
- **Relation/Heap 存储**：堆组织表结构

## 2. 问题分析

### 2.1 当前架构的不足

```
当前架构：
┌─────────────────────────────────┐
│         SQL 执行器              │
├─────────────────────────────────┤
│         KV 存储引擎             │
│    (简单的 key-value 接口)       │
├─────────────────────────────────┤
│         文件系统                │
└─────────────────────────────────┘

缺失的层次：
- Catalog（元数据管理）
- AM 层（访问方法抽象）
- Buffer Pool（缓存层）
- 事务/锁系统集成
```

### 2.2 PostgreSQL 架构参考

```
PostgreSQL 架构层次：
┌─────────────────────────────────────────┐
│         Parser / Rewriter / Planner     │
├─────────────────────────────────────────┤
│         Executor                        │
├─────────────────────────────────────────┤
│         存取方法层 (AM)                  │
│    ┌─────────┐  ┌─────────┐            │
│    │ Heap AM │  │ Index AM│  ...        │
│    └────┬────┘  └────┬────┘            │
├─────────┼────────────┼─────────────────┤
│         │   Buffer Pool │               │
├─────────┼────────────┼─────────────────┤
│         │   SMGR (存储管理)│             │
├─────────┼────────────┼─────────────────┤
│         │   文件系统 / WAL │             │
└─────────┴────────────┴─────────────────┘
```

## 3. 实现方案

### 3.1 Catalog 系统

```
Catalog 组成：
┌─────────────────────────────────────────┐
│              Catalog Manager            │
├──────────────┬──────────────┬───────────┤
│  pg_class   │   pg_attribute│  pg_index │
│  (表信息)    │   (列信息)     │  (索引)   │
├──────────────┴──────────────┴───────────┤
│           pg_proc (函数)                 │
│           pg_type (类型)                 │
└─────────────────────────────────────────┘
```

**核心表结构**：
- `pg_class`：存储所有 relations（表、索引、视图等）
- `pg_attribute`：存储每个 relation 的列定义
- `pg_index`：存储索引信息
- `pg_database`：数据库定义

### 3.2 Access Method 层

```
AM 接口抽象：
┌──────────────────────────────────────┐
│         RelationOpen/Close           │
├──────────────────────────────────────┤
│         RelationGetTuple             │
│         RelationInsertTuple          │
│         RelationUpdateTuple          │
│         RelationDeleteTuple          │
├──────────────────────────────────────┤
│         IndexScan / SeqScan          │
└──────────────────────────────────────┘

实现：
┌─────────────┐  ┌─────────────┐
│  Heap AM    │  │  BTree AM   │
│  (堆表)      │  │  (B树索引)   │
└─────────────┘  └─────────────┘
```

### 3.3 Buffer Pool

```
Buffer Pool 结构：
┌────────────────────────────────────┐
│         Buffer Pool Manager        │
├────────────────────────────────────┤
│  ┌────┬────┬────┬────┬────┬────┐  │
│  │ B0 │ B1 │ B2 │ B3 │ ... │Bn │  │
│  └────┴────┴────┴────┴────┴────┘  │
│     ┌───────────────────────┐      │
│     │    Hash Table         │      │
│     │  (relfilenode → buf)  │      │
│     └───────────────────────┘      │
├────────────────────────────────────┤
│  ┌─────────────────────────────┐   │
│  │    Clock-Sweep Replacer     │   │
│  └─────────────────────────────┘   │
└────────────────────────────────────┘
```

### 3.4 Relation/Heap 存储

```
Heap Page 结构：
┌────────────────────────────────────┐
│ PageHeaderData                     │
│   - lsn, tli, flags, free space   │
├────────────────────────────────────┤
│ LinePointer[] (指向实际元组)        │
│ ┌──────┬──────┬──────┬──────┐     │
│ │  →  │  →  │  →  │  ×  │ ...│     │
│ └──────┴──────┴──────┴──────┘     │
├────────────────────────────────────┤
│ Tuple Data (实际数据)              │
│ ┌────────────────────────────┐     │
│ │ T1 │ T2 │ T3 │ (deleted)  │     │
│ └────────────────────────────┘     │
└────────────────────────────────────┘
```

## 4. 核心 API 设计

### 4.1 Catalog API

```c
/* 表操作 */
catalog_result_t catalog_create_table(const char *name, column_def_t *cols, int ncols);
catalog_result_t catalog_drop_table(uint32_t table_oid);
catalog_result_t catalog_get_table(uint32_t table_oid, table_info_t **info);

/* 列操作 */
catalog_result_t catalog_add_column(uint32_t table_oid, column_def_t *col);
catalog_result_t catalog_get_columns(uint32_t table_oid, column_info_t **cols, int *ncols);

/* 索引操作 */
catalog_result_t catalog_create_index(const char *name, uint32_t table_oid, 
                                       const char **columns, int ncols);
catalog_result_t catalog_drop_index(uint32_t index_oid);
```

### 4.2 AM 层 API

```c
/* Relation 操作 */
Relation relation_open(uint32_t relid, LOCKMODE lockmode);
void relation_close(Relation rel, LOCKMODE lockmode);

/* 堆表操作 */
HeapTuple heap_insert(Relation rel, TupleDesc desc, Datum *values);
HeapTuple heap_getnext(Relation rel, ScanDirection dir);
HeapTuple heap_beginscan(Relation rel, ScanKey key);
void heap_endscan(HeapScanDesc scan);

/* 索引操作 */
IndexScanDesc index_beginscan(Relation index, Relation heap, 
                              ScanKey key, uint32_t nkeys);
HeapTuple index_getnext(IndexScanDesc scan);
```

### 4.3 Buffer Pool API

```c
/* Buffer 获取 */
BufferDesc *buffer_read_page(uint32_t relid, BlockNumber blocknum);
void buffer_unpin(BufferDesc *buf);
void buffer_mark_dirty(BufferDesc *buf);

/* Buffer 管理 */
int buffer_get_blocknum(BufferDesc *buf);
void *buffer_get_data(BufferDesc *buf);
```

## 5. 与现有系统的集成

### 5.1 与 KV 存储的集成

```
Buffer Pool → KV 存储：
┌──────────────┐     ┌──────────────┐
│ Buffer Pool  │────▶│   KV Store   │
│              │     │   (现有)     │
└──────────────┘     └──────────────┘
```

Buffer Pool 将页面数据存储到 KV 引擎，利用现有的 WAL 和事务支持。

### 5.2 与锁系统的集成

```
AM 层使用已有锁系统：
- relation_open/close 使用表锁
- heap_insert/update/delete 使用行锁
- 索引操作使用索引锁
```

### 5.3 与 SQL 层的集成

```
SQL 层调用新架构：
┌──────────────┐
│ SQL Executor │
├──────────────┤
│   Catalog    │  ← 获取表结构
│     AM       │  ← 数据访问
│  Buffer Pool │  ← 缓存
└──────────────┘
```

## 6. 文件结构

```
include/db/
├── catalog.h          # Catalog 公共接口
├── catalog_defs.h     # Catalog 内部定义
├── rel.h              # Relation 定义
├── buf.h              # Buffer Pool 接口
├── buf_internals.h    # Buffer 内部结构
├── am.h               # Access Method 接口
├── heapam.h           # Heap AM 接口
├── btreeam.h          # BTree AM 接口

src/db/storage/
├── catalog.c          # Catalog 实现
├── bufmgr.c           # Buffer Pool 实现
├── heapam.c           # Heap AM 实现
├── btreeam.c          # BTree AM 实现
├── page.c             # Page 工具函数
```

## 7. 里程碑

1. **M1：Catalog 基础** - 实现 pg_class, pg_attribute 表
2. **M2：Buffer Pool** - 实现页面缓存管理
3. **M3：Heap AM** - 实现堆表访问方法
4. **M4：BTree AM** - 实现 B 树索引访问
5. **M5：集成测试** - 与 SQL 层和锁系统集成
