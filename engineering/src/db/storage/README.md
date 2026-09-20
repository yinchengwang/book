# 存储层 — 实现文档

> PostgreSQL 风格的存储引擎，覆盖 Buffer Pool、WAL、MVCC 事务、多模型存储等核心模块。

---

## 📐 存储层架构

```
┌─────────────────────────────────────────────────────────────────┐
│                    Access Method 层                               │
│    ┌────────┐  ┌────────┐  ┌────────┐  ┌────────┐            │
│    │ Heap   │  │ BTree  │  │ 向量 AM │  │ 其他 AM │            │
│    └────┬───┘  └────┬───┘  └────┬───┘  └────┬───┘            │
├─────────┼───────────┼───────────┼───────────┼───────────────────┤
│         │     Buffer Pool       │           │                   │
│    ┌────▼──────────────────────▼────┐                            │
│    │  Hash 表（O(1)查找）            │                            │
│    │  Clock-Sweep 置换算法           │                            │
│    │  脏页管理 + Pin/Unpin          │                            │
│    └────────────────────────────┬──┘                            │
├─────────────────────────────────┼───────────────────────────────┤
│              Page Manager       │                               │
│    ┌────────────────────────────▼──┐                            │
│    │  Page Header + Line Pointer   │                            │
│    │  Heap Tuples (行存储)          │                            │
│    └───────────────────────────────┘                            │
├─────────────────────────────────────────────────────────────────┤
│                      WAL 日志系统                                 │
│    写前日志 | Redo | 检查点 | Buffer 协调                       │
├─────────────────────────────────────────────────────────────────┤
│                      磁盘文件层                                   │
│    Page 管理 | 空闲页面分配 | 顺序/随机 I/O                     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 📁 模块目录

```
storage/
├── buffer/          # Buffer Pool 缓存管理
│   ├── bufmgr.c     # 缓冲区管理器（Clock-Sweep）
│   └── buffer.c     # 缓冲区操作
│
├── wal/             # WAL 日志系统
│   ├── wal.c        # WAL 核心
│   ├── wal_flush.c  # WAL 刷盘
│   ├── wal_buf.c    # WAL 缓冲区
│   └── wal_recover.c # 崩溃恢复
│
├── txn/             # 事务处理
│   ├── txn_xact.c   # 事务管理
│   ├── mvcc.c       # MVCC 实现
│   ├── mvcc_wal.c   # MVCC + WAL 集成
│   ├── lock_mgr.c   # 锁管理器
│   ├── heap_visibility.c # 可见性判断
│   ├── predicate.c  # 谓词锁
│   ├── vacuum.c     # 清理回收
│   └── vacuum_trigger.c # 清理触发
│
├── catalog/         # 系统目录
│   └── (元数据管理、OID 分配)
│
├── access/          # Access Method
│   ├── heap/        # Heap 堆表
│   └── btree/       # BTree AM
│
├── page/            # 页面管理
│   └── disk.c       # 磁盘 I/O
│
├── vector/          # 向量存储
│   ├── vec_page.c   # 向量页面
│   ├── vector_page.c # 向量页管理
│   ├── vector_buf.c # 向量缓冲
│   ├── vector_segment.c # 分段管理
│   ├── vector_wal.c # 向量 WAL
│   └── graph_dedup.c # 图去重
│
├── graph/           # 图存储
│   ├── graph_engine.c # 图引擎
│   ├── graph_store.c # 图存储
│   ├── graph_index.c # 图索引
│   └── graph_traverse.c # 图遍历
│
├── kv/              # 键值存储（LSM）
│   ├── kv.c         # KV 核心
│   ├── kv_engine.c  # KV 引擎
│   ├── kv_ttl.c     # TTL 过期
│   ├── kv_iter.c    # 迭代器
│   ├── kv_page_split.c # 页面分裂
│   ├── kv_txn.c     # KV 事务
│   ├── wide_row.c   # 宽行存储
│   └── lsm/         # LSM 树
│       └── sstable.c # SSTable
│
├── doc/             # 文档存储
│   ├── doc_fts.c    # 全文检索
│   ├── doc_vector.c # 文档向量
│   ├── doc_nested.c # 嵌套文档
│   ├── doc_agg.c    # 文档聚合
│   ├── doc_pipeline.c # 文档管道
│   ├── bm25.c       # BM25 算法
│   ├── jsonpath.c   # JSONPath
│   └── doc_field_boost.c # 字段权重
│
├── rel/             # 关系存储
│   ├── table.c      # 表管理
│   ├── rel_engine.c # 关系引擎
│   ├── ltree.c      # 树形结构
│   └── tid_resolver.c # TID 解析
│
├── mm/              # 内存池
│   └── mm_pool.c    # 内存池管理
│
├── lock/            # 锁管理
│
├── ts/              # 时序存储
│   ├── ts_engine.c  # 时序引擎
│   ├── ts_columnar.c # 列式存储
│   ├── ts_segment.c # 分段存储
│   ├── ts_retention.c # 数据保留
│   ├── ts_continuous_agg.c # 连续聚合
│   ├── ts_mview.c   # 物化视图
│   └── ts_sql_functions.c # SQL 函数
│
├── spatial/         # 空间存储
│   ├── spatial_geo.c # 地理处理
│   ├── spatial_quadtree.c # 四叉树
│   ├── rtree_file.c # R-Tree 文件
│   └── geography.c  # 地理计算
│
├── blob/            # 大对象存储
│   ├── blob_catalog.c # 大对象目录
│   ├── blob_upload.c # 上传管理
│   └── blob_manifest.c # 清单管理
│
├── columnar/        # 列式存储
│   └── columnar_engine_internal.h
│
├── rdf/             # RDF 存储
│   ├── rdf_index.c  # RDF 索引
│   └── sparql_parser.c # SPARQL 解析
│
├── stream/          # 流式存储
│   └── stream_engine.h
│
├── log/             # 日志存储
│   ├── log_engine_ext.c # 日志引擎
│   └── log_label_index.c # 标签索引
│
├── yang/            # Yang 存储引擎
│   ├── datastore.c  # 数据存储
│   └── yang_engine.c # 引擎
│
└── sparse/          # 稀疏向量
    └── sparse_vector.c
```

---

## 🔵 Buffer Pool

### 实现原理

```
请求页 → Hash 表查找 → 命中 → Pin + 返回
         │
         └── 未命中 → Clock-Sweep 淘汰 → 读盘 → Pin + 返回
```

**Clock-Sweep 置换算法**：
- 每个 buffer 有一个 `usage_count` 位
- 扫描时：若 `usage_count == 0`，淘汰该页
- 若 `usage_count > 0`，减一并继续扫描

### 核心 API

```c
// 读取页面（自动缓存）
Page bufmgr_read_page(OID rel, BlockNumber blk);

// 标记脏页
void bufmgr_mark_dirty(Buffer buf);

// 释放缓冲区
void bufmgr_unpin(Buffer buf);
```

---

## 🔵 WAL 日志

### 持久化流程

```
事务提交 → WAL 记录写入 WAL Buffer → WAL 刷盘 → 确认提交
```

### Redo 恢复

```
崩溃重启 → 扫描 WAL → 找到最后一个检查点 → 重放日志 → 恢复数据
```

### 核心 API

```c
// 写入 WAL 记录
void wal_insert(uint8 xlog_op, const void *data, Size len);

// 刷盘
void wal_flush(void);

// 恢复
void wal_recover(void);
```

---

## 🔵 MVCC 事务

### 可见性判断

```
查询元组 → 读取 xmin/xmax → 判断事务状态
    │
    ├── xmin 提交 + 在快照内 → 可见
    ├── xmax 提交 + 在快照内 → 不可见（已删除）
    └── 其他 → 不可见
```

### 锁管理

| 锁类型 | 粒度 | 说明 |
|--------|------|------|
| Row Lock | 行级 | 共享/排他 |
| Table Lock | 表级 | 意向锁 |
| Page Lock | 页级 | 谓词锁 |

---

## 🔵 多模型存储

### 存储类型

| 模型 | 适用场景 | 说明 |
|------|----------|------|
| **rel** | 结构化数据 | 行存储、SQL 查询 |
| **kv** | 键值对 | LSM 树、高写入吞吐 |
| **doc** | 半结构化文档 | JSON、全文检索 |
| **vector** | 向量数据 | ANN 索引、相似搜索 |
| **graph** | 图数据 | 邻接表、图遍历 |
| **ts** | 时序数据 | 时间线、聚合查询 |
| **spatial** | 空间数据 | R-Tree、地理计算 |
| **blob** | 大对象 | 文件、图片、视频 |
| **rdf** | 语义网数据 | SPARQL、三元组 |
| **columnar** | 分析型查询 | 列式压缩 |
| **stream** | 流数据 | 实时事件 |

---

## 📊 页面结构

```
┌────────────────────────┬──────────────────────┐
│      Page Header       │  Line Pointer Array  │
│   (PageHeaderData)     │  (指向 heap 数组)     │
├────────────────────────┴──────────────────────┤
│              Heap Tuples                       │
│   (实际数据，按写入顺序排列)                    │
└───────────────────────────────────────────────┘
```

---

## 🔗 相关模块

- [索引子系统](../index/README.md) — 20+ 索引实现
- [RAG 系统](../../../rag/README.md) — 向量检索应用
- [根目录](../README.md) — 数据库整体架构
